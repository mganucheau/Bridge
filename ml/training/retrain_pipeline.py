#!/usr/bin/env python3
"""
Orchestrate distillation → train → export → validate → register (lazy Magenta/TF for distill only).
Requires local venv with tensorflow/magenta for distill; onnxruntime/numpy/pandas for scoring.
"""
from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path
from typing import Any

_SCRIPT_DIR = Path(__file__).resolve().parent
_MODELS = _SCRIPT_DIR.parent / "models"


def _run(cmd: list[str], **kw: Any) -> None:
    subprocess.run(cmd, check=True, **kw)


def _maybe_calibrate(models_dir: Path) -> Path | None:
    rep = models_dir / "inference_report.json"
    out_npz = models_dir / "knob_mapper_calibrated.npz"
    if not rep.is_file():
        print("Calibration skipped: no inference_report.json yet (run after first test_inference).")
        return None
    _run(
        [
            sys.executable,
            str(_SCRIPT_DIR / "knob_calibration.py"),
            "--report",
            str(rep),
            "--out",
            str(out_npz),
        ]
    )
    return out_npz if out_npz.is_file() else None


def _run_test_inference(models_dir: Path) -> None:
    _run(
        [
            sys.executable,
            str(_SCRIPT_DIR / "test_inference.py"),
            "--models-dir",
            str(models_dir),
            "--out",
            str(models_dir / "inference_report.json"),
        ]
    )


def _run_snapshots(models_dir: Path, skip: bool) -> None:
    if skip:
        print("Skipping snapshot regression (--skip-snapshot-test).")
        return
    r = subprocess.run(
        [sys.executable, str(_SCRIPT_DIR / "snapshot_tests.py"), "run", "--models-dir", str(models_dir)],
    )
    if r.returncode != 0:
        raise SystemExit(
            "Snapshot regression failed (cosine < threshold or drift). "
            "Fix models or run: python ml/training/snapshot_tests.py init-baselines --models-dir ml/models"
        )


def _write_validation_midis(models_dir: Path, bpm: float) -> None:
    from personality_presets import PERSONALITY_PRESETS
    from validate_midi_output import knobs_array_to_pretty_midi
    from onnx_runtime_inputs import DEFAULT_MODEL_FILES

    out_dir = models_dir / "validation_midis"
    out_dir.mkdir(parents=True, exist_ok=True)
    for inst, fname in DEFAULT_MODEL_FILES.items():
        mp = models_dir / fname
        if not mp.is_file():
            continue
        for preset_name, vec in PERSONALITY_PRESETS.items():
            pm, _ = knobs_array_to_pretty_midi(mp, inst, vec, bpm=bpm)
            safe = preset_name.replace(" ", "_")
            path = out_dir / f"{inst}_{safe}.mid"
            pm.write(str(path))
    print("Wrote validation MIDIs →", out_dir)


def _quality_scores_json(models_dir: Path, bpm: float) -> None:
    from datetime import datetime, timezone

    from personality_presets import PERSONALITY_PRESETS
    from quality_metrics import score_preset_sweep
    from onnx_runtime_inputs import DEFAULT_MODEL_FILES

    payload: dict[str, Any] = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "bpm": bpm,
        "instruments": {},
    }
    for inst in DEFAULT_MODEL_FILES:
        onnx = models_dir / DEFAULT_MODEL_FILES[inst]
        if not onnx.is_file():
            continue
        df = score_preset_sweep(str(models_dir), PERSONALITY_PRESETS, inst, bpm=bpm)
        payload["instruments"][inst] = df.to_dict(orient="records")
    out = models_dir / "quality_scores.json"
    out.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print("Wrote", out)


def _train_export_drums(
    models_dir: Path,
    n_samples: int,
    midi_path: str | None,
    mapper_path: str | None,
    train_steps: int,
    cache_dir: str | None,
) -> None:
    from magenta_pipeline import MagentaDrumsTeacher, distill_batch_drums
    from drum_onnx_student import export_drum_onnx, train_student_from_xy

    teacher = MagentaDrumsTeacher(cache_dir=cache_dir)
    X, Y = distill_batch_drums(
        teacher,
        n_samples=n_samples,
        seed=42,
        midi_path=midi_path,
        mapper_path=mapper_path,
    )
    model = train_student_from_xy(X, Y, steps=train_steps, batch=64, lr=1e-3)
    out = models_dir / "drum_humanizer.onnx"
    export_drum_onnx(model, str(out))
    print("Exported", out)


def _train_export_bass(
    models_dir: Path,
    n_samples: int,
    midi_path: str | None,
    mapper_path: str | None,
    train_steps: int,
    cache_dir: str | None,
) -> None:
    from magenta_pipeline import MagentaBassTeacher, distill_batch_bass
    from bass_onnx_student import export_bass_onnx, train_student_from_xy

    teacher = MagentaBassTeacher(cache_dir=cache_dir)
    X, Y = distill_batch_bass(
        teacher,
        n_samples=n_samples,
        seed=42,
        midi_path=midi_path,
        mapper_path=mapper_path,
    )
    model = train_student_from_xy(X, Y, steps=train_steps, batch=64, lr=1e-3)
    out = models_dir / "bass_model.onnx"
    export_bass_onnx(model, str(out))
    print("Exported", out)


def _train_export_chords(
    models_dir: Path,
    n_samples: int,
    midi_path: str | None,
    mapper_path: str | None,
    train_steps: int,
    cache_dir: str | None,
) -> None:
    from magenta_pipeline import MagentaChordsTeacher, distill_batch_chords
    from chords_onnx_student import export_chords_onnx, train_student_from_xy

    teacher = MagentaChordsTeacher(cache_dir=cache_dir)
    X, Y = distill_batch_chords(
        teacher,
        n_samples=n_samples,
        seed=42,
        midi_path=midi_path,
        mapper_path=mapper_path,
    )
    model = train_student_from_xy(X, Y, steps=train_steps, batch=64, lr=1e-3)
    out = models_dir / "chords_model.onnx"
    export_chords_onnx(model, str(out))
    print("Exported", out)


def _train_export_melody(
    models_dir: Path,
    n_samples: int,
    midi_path: str | None,
    mapper_path: str | None,
    train_steps: int,
    cache_dir: str | None,
) -> None:
    from magenta_pipeline import MagentaMelodyTeacher, distill_batch_melody
    from melody_onnx_student import export_melody_onnx, train_student_from_xy

    teacher = MagentaMelodyTeacher(cache_dir=cache_dir)
    X, Y = distill_batch_melody(
        teacher,
        n_samples=n_samples,
        seed=42,
        midi_path=midi_path,
        mapper_path=mapper_path,
    )
    model = train_student_from_xy(X, Y, steps=train_steps, batch=64, lr=1e-3)
    out = models_dir / "melody_model.onnx"
    export_melody_onnx(model, str(out))
    print("Exported", out)


_TRAINERS = {
    "drums": _train_export_drums,
    "bass": _train_export_bass,
    "chords": _train_export_chords,
    "melody": _train_export_melody,
}


def _load_json(p: Path) -> Any:
    if not p.is_file():
        return None
    return json.loads(p.read_text(encoding="utf-8"))


def _mean_metric(q: dict | None, key: str) -> float | None:
    if not q or "instruments" not in q:
        return None
    vals = []
    for inst, rows in q["instruments"].items():
        if not isinstance(rows, list):
            continue
        for row in rows:
            if isinstance(row, dict) and key in row and row[key] is not None:
                try:
                    v = float(row[key])
                    if not math.isnan(v):
                        vals.append(v)
                except (TypeError, ValueError):
                    pass
    if not vals:
        return None
    return sum(vals) / len(vals)


def _print_metric_delta(prev: dict | None, cur: dict | None) -> None:
    if not prev or not cur:
        print("No previous quality_scores.json for comparison.")
        return
    keys = (
        "pitch_entropy",
        "density",
        "velocity_range",
        "groove_score",
        "rhythmic_consistency",
    )
    print("\n=== Metric means (all presets × instruments) ===")
    for k in keys:
        a, b = _mean_metric(prev, k), _mean_metric(cur, k)
        if a is None or b is None:
            continue
        print(f"  {k}: {a:.4f} → {b:.4f} (Δ {b - a:+.4f})")


def retrain_pipeline(args: argparse.Namespace) -> None:
    models_dir = Path(args.models_dir)
    models_dir.mkdir(parents=True, exist_ok=True)
    cache_dir = args.cache_dir or str(Path.home() / ".cache" / "bridge_magenta")

    mapper_path: str | None = None
    if args.calibrate:
        npz = _maybe_calibrate(models_dir)
        if npz:
            mapper_path = str(npz)
    elif args.mapper_path:
        p = Path(args.mapper_path)
        if p.is_file():
            mapper_path = str(p)

    prev_scores = _load_json(models_dir / "quality_scores.json")
    reg_path = models_dir / "registry.json"
    prev_tag = None
    if reg_path.is_file():
        try:
            prev_tag = json.loads(reg_path.read_text(encoding="utf-8")).get("active")
        except json.JSONDecodeError:
            pass
    prev_version_scores = None
    if prev_tag:
        prev_version_scores = _load_json(models_dir / prev_tag / "quality_scores.json")

    instruments = list(_TRAINERS.keys()) if args.instrument == "all" else [args.instrument]

    for inst in instruments:
        if inst not in _TRAINERS:
            raise SystemExit(f"Unknown instrument {inst!r}")
        print(f"\n########## {inst} ##########")
        _TRAINERS[inst](
            models_dir,
            n_samples=args.n_samples,
            midi_path=args.midi_seed,
            mapper_path=mapper_path,
            train_steps=args.train_steps,
            cache_dir=cache_dir,
        )

    _run_test_inference(models_dir)
    _run_snapshots(models_dir, args.skip_snapshot_test)
    _write_validation_midis(models_dir, args.bpm)
    _quality_scores_json(models_dir, args.bpm)
    cur_scores = _load_json(models_dir / "quality_scores.json")
    _print_metric_delta(prev_version_scores or prev_scores, cur_scores)

    if args.version_tag:
        _run(
            [
                sys.executable,
                str(_SCRIPT_DIR / "model_registry.py"),
                "register",
                args.version_tag,
                "--source",
                str(models_dir),
                "--root",
                str(models_dir),
            ]
        )
        print("Registered version", args.version_tag)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--instrument", type=str, required=True, help="drums|bass|chords|melody|all")
    ap.add_argument("--n-samples", type=int, default=500)
    ap.add_argument("--train-steps", type=int, default=1200)
    ap.add_argument("--calibrate", action="store_true", help="Run knob_calibration from inference_report")
    ap.add_argument("--mapper-path", type=str, default=None, help="NPZ mapper for distill (overrides --calibrate output)")
    ap.add_argument("--version-tag", type=str, default=None, help="Pass to model_registry.register after success")
    ap.add_argument("--midi-seed", type=str, default=None, dest="midi_seed")
    ap.add_argument("--models-dir", type=str, default=str(_MODELS))
    ap.add_argument("--cache-dir", type=str, default=None)
    ap.add_argument("--bpm", type=float, default=120.0)
    ap.add_argument(
        "--skip-snapshot-test",
        action="store_true",
        help="Do not run snapshot_tests.py (not recommended)",
    )
    args = ap.parse_args()
    retrain_pipeline(args)


if __name__ == "__main__":
    main()
