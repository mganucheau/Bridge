#!/usr/bin/env python3
"""Run a single ONNX inference and write a .mid for DAW audition (ORT + pretty_midi only)."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

try:
    import onnxruntime as ort
except ImportError as e:  # pragma: no cover
    raise SystemExit("onnxruntime is required") from e

from magenta_data_utils import bridge_student_output_to_pretty_midi
from onnx_runtime_inputs import DEFAULT_MODEL_FILES, build_fixed_input


def parse_knobs(s: str) -> np.ndarray:
    parts = [p.strip() for p in s.split(",") if p.strip()]
    if len(parts) != 10:
        raise SystemExit(f"expected 10 comma-separated knob values, got {len(parts)}")
    return np.array([float(x) for x in parts], dtype=np.float32)


def run_model(model_path: Path, x: np.ndarray) -> np.ndarray:
    sess = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
    inp = sess.get_inputs()[0]
    y = sess.run(None, {inp.name: np.asarray(x, dtype=np.float32).reshape(1, -1)})[0]
    return np.asarray(y, dtype=np.float32).reshape(-1)


def knobs_array_to_pretty_midi(
    model_path: Path | str,
    instrument: str,
    knobs: np.ndarray | list[float],
    *,
    bpm: float = 120.0,
    root: int = 0,
    octave: int = 3,
    chord_octave: int = 4,
    melody_octave: int = 5,
) -> tuple[object, np.ndarray]:
    """
    ORT inference + PrettyMIDI object (no file I/O). Used by quality_metrics / retrain_pipeline.
    Returns (pretty_midi.PrettyMIDI, raw_output_floats).
    """
    inst = instrument.lower().strip()
    if inst not in DEFAULT_MODEL_FILES:
        raise ValueError(f"instrument must be one of {list(DEFAULT_MODEL_FILES)}, got {instrument!r}")
    k = np.asarray(knobs, dtype=np.float32).reshape(-1)
    if k.size != 10:
        raise ValueError("knobs must have length 10")
    x = build_fixed_input(inst, k)
    y = run_model(Path(model_path), x)
    if inst == "drums":
        pm = bridge_student_output_to_pretty_midi("drums", y, bpm=bpm)
    elif inst == "bass":
        pm = bridge_student_output_to_pretty_midi(
            "bass", y, root_note=root, octave=octave, bpm=bpm
        )
    elif inst == "chords":
        pm = bridge_student_output_to_pretty_midi(
            "chords", y, root_note=root, octave=chord_octave, bpm=bpm
        )
    else:
        pm = bridge_student_output_to_pretty_midi(
            "melody", y, root_note=root, octave=melody_octave, bpm=bpm
        )
    return pm, y


def infer_instrument_for_path(model_path: Path) -> str | None:
    name = model_path.name.lower()
    for inst, fname in DEFAULT_MODEL_FILES.items():
        if fname.lower() == name:
            return inst
    return None


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", type=Path, required=True)
    ap.add_argument("--instrument", type=str, default=None, help="drums|bass|chords|melody (default: infer from filename)")
    ap.add_argument("--knobs", type=str, required=True, help="10 comma-separated floats in [0,1]")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--root", type=int, default=0, help="Pitch class 0–11 (chords/melody/bass root)")
    ap.add_argument("--octave", type=int, default=3, help="Plugin-style octave index for bass (default 3)")
    ap.add_argument("--chord-octave", type=int, default=4, help="Octave for chord voicings")
    ap.add_argument("--melody-octave", type=int, default=5, help="Octave for melody line")
    ap.add_argument("--bpm", type=float, default=120.0)
    args = ap.parse_args()

    inst = (args.instrument or "").lower().strip() if args.instrument else None
    if not inst:
        inst = infer_instrument_for_path(args.model)
    if inst not in DEFAULT_MODEL_FILES:
        raise SystemExit(
            "Set --instrument to one of: drums, bass, chords, melody "
            f"(could not infer from {args.model.name})"
        )

    knobs = parse_knobs(args.knobs)
    pm, _y = knobs_array_to_pretty_midi(
        args.model,
        inst,
        knobs,
        bpm=args.bpm,
        root=args.root,
        octave=args.octave,
        chord_octave=args.chord_octave,
        melody_octave=args.melody_octave,
    )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    pm.write(str(args.out))
    print(f"Wrote {args.out} ({inst})")


if __name__ == "__main__":
    main()
