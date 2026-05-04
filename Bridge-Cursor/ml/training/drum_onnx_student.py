# === Drum student ONNX: drum_humanizer.onnx (46 → 32) ===
# Plugin wiring comes in a later step; this defines the training contract.
#
# Input layout (46 floats):
#   [0:10]   shared personality knobs [0,1]
#   [10:42]  previous hit pattern — 8 drum voices × 4 time slots, values in [0,1]
#            (activity / energy per voice-slot; flatten voice-major: v*4 + t)
#   [42:46]  timing / groove context: swing, step_density, backbeat_strength, syncopation [0,1]
#
# Output (32 floats): hit probabilities in [0,1], same 8×4 layout (voice-major flatten).
#   Runtime (Bridge): BridgeMLManager::generateDrums enforces 32 outputs; DrumEngine runs
#   four inferences per full bar (one per quarter of 16 steps) with captureMLContextForQuarter.
from __future__ import annotations

import os

import numpy as np
import torch
import torch.nn as nn

DRUM_ONNX_IN = 46
DRUM_ONNX_OUT = 32
DRUM_VOICES = 8
DRUM_HISTORY_SLOTS = 4


class DrumStudentMLP(nn.Module):
    def __init__(self, hidden: int = 128):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(DRUM_ONNX_IN, hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.ReLU(),
            nn.Linear(hidden, DRUM_ONNX_OUT),
            nn.Sigmoid(),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def pack_drum_input(
    knobs10: np.ndarray,
    prev_pattern32: np.ndarray,
    groove4: np.ndarray,
) -> np.ndarray:
    k = np.clip(np.asarray(knobs10, dtype=np.float32).reshape(10), 0, 1)
    prev = np.zeros(32, dtype=np.float32)
    p = np.asarray(prev_pattern32, dtype=np.float32).reshape(-1)
    prev[: min(32, p.size)] = np.clip(p[:32], 0, 1)
    g = np.clip(np.asarray(groove4, dtype=np.float32).reshape(-1), 0, 1)
    groove = np.zeros(4, dtype=np.float32)
    groove[: min(4, g.size)] = g[:4]
    return np.concatenate([k, prev, groove])


def synthetic_drum_target(x: np.ndarray) -> np.ndarray:
    rng = np.random.default_rng(int(np.sum(x * 1e4) % (2**31)))
    prev = x[10:42].reshape(DRUM_VOICES, DRUM_HISTORY_SLOTS)
    groove = x[42:46]
    out = np.zeros((DRUM_VOICES, DRUM_HISTORY_SLOTS), dtype=np.float32)
    for v in range(DRUM_VOICES):
        for t in range(DRUM_HISTORY_SLOTS):
            base = prev[v, t] * 0.55 + groove[0] * 0.15 + groove[1] * 0.15
            out[v, t] = float(np.clip(base + rng.normal(0, 0.08), 0, 1))
    return out.reshape(-1)


def train_drum_student_quick(
    steps: int = 1500,
    batch: int = 64,
    lr: float = 1e-3,
    device: str | None = None,
) -> DrumStudentMLP:
    dev = torch.device(device or ("cuda" if torch.cuda.is_available() else "cpu"))
    model = DrumStudentMLP().to(dev)
    opt = torch.optim.Adam(model.parameters(), lr=lr)
    loss_fn = nn.MSELoss()
    model.train()
    for _ in range(steps):
        xb = np.stack(
            [
                pack_drum_input(
                    np.random.rand(10),
                    np.random.rand(32),
                    np.random.rand(4),
                )
                for _ in range(batch)
            ]
        )
        yb = np.stack([synthetic_drum_target(xb[i]) for i in range(batch)])
        xt = torch.from_numpy(xb).to(dev)
        yt = torch.from_numpy(yb).to(dev)
        opt.zero_grad()
        pred = model(xt)
        loss = loss_fn(pred, yt)
        loss.backward()
        opt.step()
    return model


def train_student_from_xy(
    X: np.ndarray,
    Y: np.ndarray,
    steps: int = 1500,
    batch: int = 64,
    lr: float = 1e-3,
    device: str | None = None,
    hidden: int = 128,
) -> DrumStudentMLP:
    """Train DrumStudentMLP from distilled (N, 46) / (N, 32) arrays."""
    dev = torch.device(device or ("cuda" if torch.cuda.is_available() else "cpu"))
    X = np.asarray(X, dtype=np.float32)
    Y = np.asarray(Y, dtype=np.float32)
    if X.shape[0] != Y.shape[0]:
        raise ValueError("X and Y must have the same number of rows")
    n = X.shape[0]
    model = DrumStudentMLP(hidden=hidden).to(dev)
    opt = torch.optim.Adam(model.parameters(), lr=lr)
    loss_fn = nn.MSELoss()
    model.train()
    rng = np.random.default_rng(42)
    for _ in range(steps):
        idx = rng.integers(0, n, size=min(batch, n))
        xt = torch.from_numpy(X[idx]).to(dev)
        yt = torch.from_numpy(Y[idx]).to(dev)
        opt.zero_grad()
        loss = loss_fn(model(xt), yt)
        loss.backward()
        opt.step()
    return model


def export_drum_onnx(model: DrumStudentMLP, path: str) -> None:
    model.eval()
    dummy = torch.randn(1, DRUM_ONNX_IN)
    torch.onnx.export(
        model,
        dummy,
        path,
        input_names=["input"],
        output_names=["output"],
        opset_version=17,
        dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}},
    )


if __name__ == "__main__":
    m = train_drum_student_quick()
    out_path = os.path.join(
        os.path.dirname(__file__), "..", "models", "drum_humanizer.onnx"
    )
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    export_drum_onnx(m, out_path)
    print("wrote", out_path, os.path.getsize(out_path))
