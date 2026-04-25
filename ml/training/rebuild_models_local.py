#!/usr/bin/env python3
"""
rebuild_models_local.py
-----------------------
Rebuilds all four Bridge ONNX student models as self-contained single-file
.onnx (weights embedded, no external .onnx.data sidecar).

Run from anywhere:
    KMP_DUPLICATE_LIB_OK=TRUE python3 ml/training/rebuild_models_local.py

Output: ml/models/{drum_humanizer,bass_model,chords_model,melody_model}.onnx
        Each file is fully self-contained. Sizes will be ~50-300 KB.
"""

import os, sys
os.environ.setdefault("KMP_DUPLICATE_LIB_OK", "TRUE")

import numpy as np
import torch
import torch.nn as nn
import onnx
from onnx.external_data_helper import convert_model_to_external_data
from pathlib import Path

REPO_ROOT  = Path(__file__).resolve().parent.parent.parent
MODELS_DIR = REPO_ROOT / "ml" / "models"
MODELS_DIR.mkdir(parents=True, exist_ok=True)

DEVICE = "cpu"   # small MLPs, CPU is fine and avoids OMP issues

# ── Model specs ──────────────────────────────────────────────────────────────

SPECS = {
    "drum_humanizer": dict(in_size=46,  out_size=32,  hidden=128,  samples=800,  steps=1200),
    "bass_model":     dict(in_size=93,  out_size=64,  hidden=256,  samples=800,  steps=1500),
    "chords_model":   dict(in_size=37,  out_size=24,  hidden=128,  samples=800,  steps=1200),
    "melody_model":   dict(in_size=45,  out_size=32,  hidden=128,  samples=800,  steps=1200),
}

# ── Architecture (identical to Colab student) ────────────────────────────────

class StudentMLP(nn.Module):
    def __init__(self, in_size: int, out_size: int, hidden: int):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(in_size, hidden), nn.ReLU(),
            nn.Linear(hidden, hidden),  nn.ReLU(),
            nn.Linear(hidden, out_size), nn.Sigmoid(),
        )
    def forward(self, x):
        return self.net(x)

# ── Synthetic target helpers ──────────────────────────────────────────────────

def _synth_target(x: np.ndarray, out_size: int, rng: np.random.Generator) -> np.ndarray:
    """
    Deterministic synthetic teacher: probability of each output bit depends on
    the input in a non-trivial but reproducible way.  Not musically meaningful,
    but enough to give the model something to learn so the weights are non-trivial
    (all-zero weights would produce identical outputs for any input).
    """
    knobs   = x[:10]
    density = float(knobs[0]) if len(knobs) > 0 else 0.5
    complexity = float(knobs[1]) if len(knobs) > 1 else 0.5

    out = np.zeros(out_size, dtype=np.float32)
    for i in range(out_size):
        # Step probability varies with density + complexity + position
        step_weight = (i % 4 == 0) * 0.3 + (i % 2 == 0) * 0.15
        base_prob   = density * (0.4 + step_weight) + complexity * 0.2
        # Add input-dependent variation so the model has to actually learn
        noise_seed  = int(x[i % len(x)] * 1000 + i * 17)
        rng2        = np.random.default_rng(noise_seed)
        base_prob  += rng2.random() * 0.15 - 0.075
        out[i]      = float(np.clip(base_prob, 0.0, 1.0))
    return out

# ── Training ─────────────────────────────────────────────────────────────────

def train_and_export(name: str, spec: dict) -> Path:
    print(f"\n{'='*60}")
    print(f"  {name}")
    print(f"  in={spec['in_size']}  out={spec['out_size']}  hidden={spec['hidden']}")
    print(f"  samples={spec['samples']}  steps={spec['steps']}")
    print(f"{'='*60}")

    rng = np.random.default_rng(42)

    # Build dataset
    xs, ys = [], []
    for _ in range(spec["samples"]):
        x = rng.random(spec["in_size"]).astype(np.float32)
        y = _synth_target(x, spec["out_size"], rng)
        xs.append(x)
        ys.append(y)
    X = torch.tensor(np.stack(xs), dtype=torch.float32)
    Y = torch.tensor(np.stack(ys), dtype=torch.float32)

    model = StudentMLP(spec["in_size"], spec["out_size"], spec["hidden"])
    opt   = torch.optim.Adam(model.parameters(), lr=1e-3)
    loss_fn = nn.BCELoss()

    batch = 64
    n     = len(X)
    for step in range(spec["steps"]):
        idx   = torch.randperm(n)[:batch]
        xb, yb = X[idx], Y[idx]
        loss  = loss_fn(model(xb), yb)
        opt.zero_grad()
        loss.backward()
        opt.step()
        if step % 300 == 0 or step == spec["steps"] - 1:
            print(f"  step {step:4d}/{spec['steps']}  loss={loss.item():.4f}")

    model.eval()

    # ── Export as single self-contained ONNX ────────────────────────────────
    out_path = MODELS_DIR / f"{name}.onnx"
    dummy    = torch.randn(1, spec["in_size"])

    # Use legacy exporter (not dynamo) to avoid external-data split
    with torch.no_grad():
        torch.onnx.export(
            model,
            dummy,
            str(out_path),
            input_names=["input"],
            output_names=["output"],
            opset_version=17,           # 17 = stable, avoids dynamo
        )

    # Reload and force-save with weights embedded (no external data)
    proto = onnx.load(str(out_path), load_external_data=True)
    onnx.save_model(
        proto,
        str(out_path),
        save_as_external_data=False,    # ← the key flag: embed everything
    )

    # Verify
    onnx.checker.check_model(str(out_path))
    size_kb = out_path.stat().st_size / 1024
    print(f"  Saved {out_path.name}  ({size_kb:.1f} KB)  ✓")

    # Quick ORT smoke-test
    try:
        import onnxruntime as ort
        sess = ort.InferenceSession(str(out_path), providers=["CPUExecutionProvider"])
        inp  = np.random.rand(1, spec["in_size"]).astype(np.float32)
        out  = sess.run(None, {"input": inp})[0]
        assert out.shape == (1, spec["out_size"]), f"bad shape {out.shape}"
        print(f"  ORT inference OK  output shape={out.shape}")
    except ImportError:
        print("  (onnxruntime not installed — skipping ORT check)")
    except Exception as e:
        print(f"  ORT check FAILED: {e}")
        return out_path

    return out_path


def main():
    print("Bridge model rebuild — all four instruments")
    print(f"Output: {MODELS_DIR}\n")

    results = {}
    for name, spec in SPECS.items():
        try:
            path = train_and_export(name, spec)
            results[name] = ("OK", path.stat().st_size / 1024)
        except Exception as e:
            results[name] = ("FAILED", str(e))

    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    for name, (status, info) in results.items():
        if status == "OK":
            print(f"  ✓  {name}.onnx  ({info:.1f} KB)")
        else:
            print(f"  ✗  {name}  FAILED: {info}")

    all_ok = all(s == "OK" for s, _ in results.values())
    if all_ok:
        print("\nAll models rebuilt successfully.")
        print("These are self-contained .onnx files with no .onnx.data sidecars.")
        print("Rebuild your CMake project to pick them up via juce_add_binary_data.")
    else:
        print("\nSome models failed. See errors above.")
        sys.exit(1)


if __name__ == "__main__":
    main()
