#!/usr/bin/env python3
"""Lightweight ONNX output regression: save snapshots and compare cosine similarity."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from onnx_runtime_inputs import DEFAULT_MODEL_FILES, build_fixed_input

SNAP_DIR_NAME = "snapshots"


def cosine_sim(a: np.ndarray, b: np.ndarray) -> float:
    a = np.asarray(a, dtype=np.float64).reshape(-1)
    b = np.asarray(b, dtype=np.float64).reshape(-1)
    if a.size != b.size:
        return 0.0
    na = np.linalg.norm(a)
    nb = np.linalg.norm(b)
    if na < 1e-12 or nb < 1e-12:
        return 1.0 if na < 1e-12 and nb < 1e-12 else 0.0
    return float(np.dot(a, b) / (na * nb))


def run_onnx(model_path: Path, x: np.ndarray) -> np.ndarray:
    import onnxruntime as ort

    sess = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
    inp = sess.get_inputs()[0]
    y = sess.run(None, {inp.name: np.asarray(x, dtype=np.float32).reshape(1, -1)})[0]
    return np.asarray(y, dtype=np.float32).reshape(-1)


def save_snapshot(
    model_path: Path,
    instrument: str,
    knob_vector: list[float],
    label: str,
    models_dir: Path | None = None,
) -> Path:
    """Run inference, write ml/models/snapshots/{label}.json"""
    models_dir = models_dir or (_SCRIPT_DIR.parent / "models")
    inst = instrument.lower().strip()
    if inst not in DEFAULT_MODEL_FILES:
        raise ValueError(f"instrument must be one of {list(DEFAULT_MODEL_FILES)}")
    k = np.asarray(knob_vector, dtype=np.float32).reshape(-1)
    if k.size != 10:
        raise ValueError("knob_vector must have length 10")
    x = build_fixed_input(inst, k)
    y = run_onnx(model_path, x)
    snap_dir = models_dir / SNAP_DIR_NAME
    snap_dir.mkdir(parents=True, exist_ok=True)
    out_path = snap_dir / f"{label}.json"
    try:
        rel_model = str(model_path.resolve().relative_to(models_dir.resolve()))
    except ValueError:
        rel_model = model_path.name
    payload = {
        "version": 1,
        "label": label,
        "instrument": inst,
        "model": rel_model,
        "knobs": [float(v) for v in k],
        "output": [float(v) for v in y.reshape(-1)],
    }
    out_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return out_path


def run_snapshot_tests(
    models_dir: Path,
    min_cosine: float = 0.95,
) -> int:
    """
    Returns 0 if all snapshots pass or none found; 1 on drift failure.
    Missing model files skip with warning (exit 0).
    """
    snap_dir = models_dir / SNAP_DIR_NAME
    files = sorted(snap_dir.glob("*.json"))
    if not files:
        print("snapshot_tests: no JSON files in", snap_dir, "— skipping (OK).")
        return 0

    failed = 0
    for fp in files:
        data = json.loads(fp.read_text(encoding="utf-8"))
        inst = data.get("instrument", "")
        rel = data.get("model", "")
        mp = models_dir / rel if rel and not Path(rel).is_absolute() else Path(rel)
        if not mp.is_file():
            print(f"[skip] {fp.name}: model not found {mp}")
            continue
        exp = np.asarray(data.get("output", []), dtype=np.float32).reshape(-1)
        if exp.size == 0:
            print(f"[skip] {fp.name}: empty output in snapshot")
            continue
        knobs = np.asarray(data.get("knobs", [0.5] * 10), dtype=np.float32).reshape(-1)
        x = build_fixed_input(str(inst), knobs)
        got = run_onnx(mp, x)
        if got.size != exp.size:
            print(f"[fail] {fp.name}: output size {got.size} != snapshot {exp.size}")
            failed += 1
            continue
        c = cosine_sim(got, exp)
        if c < min_cosine:
            print(f"[fail] {fp.name}: cosine={c:.4f} < {min_cosine}")
            failed += 1
        else:
            print(f"[ok]   {fp.name}: cosine={c:.4f}")
    return 1 if failed else 0


def cmd_init_baselines(models_dir: Path) -> None:
    for inst, fname in DEFAULT_MODEL_FILES.items():
        mp = models_dir / fname
        if not mp.is_file():
            print(f"[skip] baseline {inst}: missing {mp}")
            continue
        p = save_snapshot(mp, inst, [0.5] * 10, f"baseline_{inst}", models_dir)
        print("wrote", p)


def main() -> None:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_run = sub.add_parser("run", help="Run regression against ml/models/snapshots/*.json")
    p_run.add_argument("--models-dir", type=Path, default=_SCRIPT_DIR.parent / "models")
    p_run.add_argument("--min-cosine", type=float, default=0.95)

    p_save = sub.add_parser("save", help="Save one snapshot JSON")
    p_save.add_argument("--model", type=Path, required=True)
    p_save.add_argument("--instrument", type=str, required=True)
    p_save.add_argument("--knobs", type=str, default=",".join(["0.5"] * 10))
    p_save.add_argument("--label", type=str, required=True)
    p_save.add_argument("--models-dir", type=Path, default=_SCRIPT_DIR.parent / "models")

    p_init = sub.add_parser("init-baselines", help="Save [0.5]^10 snapshot per instrument if models exist")
    p_init.add_argument("--models-dir", type=Path, default=_SCRIPT_DIR.parent / "models")

    args = ap.parse_args()
    if args.cmd == "run":
        try:
            import onnxruntime  # noqa: F401
        except ImportError:
            print(
                "snapshot_tests: onnxruntime not installed — skipping ML regression (OK).",
                file=sys.stderr,
            )
            raise SystemExit(0)
        code = run_snapshot_tests(args.models_dir, min_cosine=args.min_cosine)
        raise SystemExit(code)
    if args.cmd == "save":
        knobs = [float(x.strip()) for x in args.knobs.split(",") if x.strip()]
        path = save_snapshot(args.model, args.instrument, knobs, args.label, args.models_dir)
        print(path)
    elif args.cmd == "init-baselines":
        cmd_init_baselines(args.models_dir)


if __name__ == "__main__":
    main()
