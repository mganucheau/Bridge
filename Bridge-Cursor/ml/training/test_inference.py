#!/usr/bin/env python3
"""ORT-only inference analysis: entropy, per-knob sensitivity, dead outputs."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

try:
    import onnxruntime as ort
except ImportError as e:  # pragma: no cover
    raise SystemExit("onnxruntime is required: pip install onnxruntime") from e

from knob_mapping import KNOB_NAMES, NUM_KNOBS
from onnx_runtime_inputs import DEFAULT_MODEL_FILES, build_fixed_input

DEFAULT_MODELS_DIR = _SCRIPT_DIR.parent / "models"
DEFAULT_REPORT = DEFAULT_MODELS_DIR / "inference_report.json"


def latin_hypercube_unit(n_samples: int, dim: int, rng: np.random.Generator) -> np.ndarray:
    """Classic LHS on [0, 1]^dim."""
    x = np.zeros((n_samples, dim), dtype=np.float64)
    for j in range(dim):
        perm = rng.permutation(n_samples)
        u = rng.random(n_samples)
        x[:, j] = (perm + u) / n_samples
    return x.astype(np.float32)


def mean_binary_entropy(y: np.ndarray) -> float:
    p = np.clip(np.asarray(y, dtype=np.float64).reshape(-1), 1e-12, 1.0 - 1e-12)
    h = -(p * np.log2(p) + (1.0 - p) * np.log2(1.0 - p))
    return float(np.mean(h))


def run_onnx( sess: ort.InferenceSession, x: np.ndarray) -> np.ndarray:
    inp = sess.get_inputs()[0]
    name = inp.name
    x2 = np.asarray(x, dtype=np.float32).reshape(1, -1)
    out = sess.run(None, {name: x2})[0]
    return np.asarray(out, dtype=np.float32).reshape(-1)


def analyze_instrument(
    model_path: Path,
    instrument: str,
    n_samples: int,
    fd_eps: float,
    dead_thresh: float,
    rng: np.random.Generator,
) -> dict:
    sess = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
    lhs = latin_hypercube_unit(n_samples, NUM_KNOBS, rng)
    entropies: list[float] = []
    sens_accum = np.zeros(NUM_KNOBS, dtype=np.float64)
    outputs_stack: list[np.ndarray] = []

    for row in lhs:
        x0 = build_fixed_input(instrument, row)
        y0 = run_onnx(sess, x0)
        entropies.append(mean_binary_entropy(y0))
        outputs_stack.append(y0.copy())
        for i in range(NUM_KNOBS):
            up = row.copy()
            up[i] = float(np.clip(row[i] + fd_eps, 0.0, 1.0))
            dn = row.copy()
            dn[i] = float(np.clip(row[i] - fd_eps, 0.0, 1.0))
            y_up = run_onnx(sess, build_fixed_input(instrument, up))
            y_dn = run_onnx(sess, build_fixed_input(instrument, dn))
            sens_accum[i] += float(np.linalg.norm(y_up - y_dn) / (2.0 * fd_eps))

    sens_mean = (sens_accum / max(1, n_samples)).astype(np.float64)
    stacked = np.stack(outputs_stack, axis=0)
    mx = np.max(stacked, axis=0)
    dead_idx = [int(i) for i in np.where(mx < dead_thresh)[0]]

    return {
        "model_path": str(model_path.resolve()),
        "output_dim": int(stacked.shape[1]),
        "mean_binary_entropy": float(np.mean(entropies)),
        "per_sample_entropy": [float(e) for e in entropies],
        "sensitivity_l2_mean": [float(sens_mean[i]) for i in range(NUM_KNOBS)],
        "dead_output_indices": dead_idx,
        "n_dead_outputs": len(dead_idx),
        "finite_difference_eps": fd_eps,
        "dead_activation_threshold": dead_thresh,
    }


def main() -> None:
    ap = argparse.ArgumentParser(description="Analyze Bridge student ONNX models (ORT only).")
    ap.add_argument("--models-dir", type=Path, default=DEFAULT_MODELS_DIR)
    ap.add_argument("--out", type=Path, default=DEFAULT_REPORT)
    ap.add_argument("--samples", type=int, default=50)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--fd-eps", type=float, default=0.05)
    ap.add_argument("--dead-thresh", type=float, default=0.1)
    args = ap.parse_args()

    rng = np.random.default_rng(args.seed)
    models_dir: Path = args.models_dir
    report: dict = {
        "version": 1,
        "knob_names": list(KNOB_NAMES),
        "latin_hypercube_samples": args.samples,
        "finite_difference_eps": args.fd_eps,
        "instruments": {},
    }

    for inst, fname in DEFAULT_MODEL_FILES.items():
        mp = models_dir / fname
        if not mp.is_file():
            print(f"[skip] {inst}: missing {mp}", file=sys.stderr)
            continue
        print(f"=== {inst} ({mp.name}) ===")
        block = analyze_instrument(mp, inst, args.samples, args.fd_eps, args.dead_thresh, rng)
        report["instruments"][inst] = block
        print(f"  mean binary entropy: {block['mean_binary_entropy']:.4f}")
        print(f"  dead outputs (<{args.dead_thresh} max): {block['n_dead_outputs']} {block['dead_output_indices'][:12]}{'...' if block['n_dead_outputs'] > 12 else ''}")
        print("  sensitivity L2 (mean over samples, per knob):")
        for i, name in enumerate(KNOB_NAMES):
            print(f"    {i:2d} {name:28s} {block['sensitivity_l2_mean'][i]:.6f}")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print(f"\nWrote {args.out}")


if __name__ == "__main__":
    main()
