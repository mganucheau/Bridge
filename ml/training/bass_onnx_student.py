# === Lightweight student network: ONNX bass_model.onnx (93 → 64) ===
# Input layout must match Bridge C++ BridgeMLManager::generateBass packing:
#   [0:10]   personality knobs [0,1]
#   [10:26]  kick grid (16)
#   [26:29]  root/11, scale/9, style/7
#   [29:93]  prior bass context (16 steps × 4: pc/11, active, vel/127, timeShiftNorm)
# Output: 64 floats = pitchClass×16, offset×16, vel×16, active×16 (same as legacy bass merge).
from __future__ import annotations

import os
from typing import Tuple

import numpy as np
import torch
import torch.nn as nn


BASS_ONNX_IN = 93
BASS_ONNX_OUT = 64


class BassStudentMLP(nn.Module):
    def __init__(self, hidden: int = 256):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(BASS_ONNX_IN, hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.ReLU(),
            nn.Linear(hidden, BASS_ONNX_OUT),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def pack_bridge_input(
    knobs10: np.ndarray,
    kick16: np.ndarray,
    root_note: int,
    scale_type: int,
    style_index: int,
    context64: np.ndarray,
) -> np.ndarray:
    """Build one training input vector (93,)."""
    k = np.clip(np.asarray(knobs10, dtype=np.float32).reshape(10), 0, 1)
    kick = np.clip(np.asarray(kick16, dtype=np.float32).reshape(16), 0, 1)
    ctx = np.zeros(64, dtype=np.float32)
    c = np.asarray(context64, dtype=np.float32).reshape(-1)
    ctx[: min(64, c.size)] = c[:64]
    return np.concatenate(
        [
            k,
            kick,
            np.array(
                [
                    float(np.clip(root_note, 0, 11)) / 11.0,
                    float(np.clip(scale_type, 0, 9)) / 9.0,
                    float(np.clip(style_index, 0, 7)) / 7.0,
                ],
                dtype=np.float32,
            ),
            ctx,
        ]
    )


def synthetic_target_from_input(x: np.ndarray) -> np.ndarray:
    """Placeholder teacher: deterministic pseudo-target for smoke training."""
    rng = np.random.default_rng(int(np.sum(x * 1e4) % (2**31)))
    out = np.zeros(64, dtype=np.float32)
    kick = x[10:26]
    root = int(np.round(x[26] * 11))
    for s in range(16):
        if kick[s] > 0.3:
            out[s] = float(rng.integers(0, 12))
            out[16 + s] = rng.uniform(-0.1, 0.1)
            out[32 + s] = rng.uniform(0.35, 0.95)
            out[48 + s] = 1.0
        else:
            out[48 + s] = rng.uniform(0, 0.4)
    out[:16] = (out[:16] + (root % 12) / 12.0) % 1.0 * 11.0
    return out


def train_student_quick(
    steps: int = 2000,
    batch: int = 64,
    lr: float = 1e-3,
    device: str | None = None,
) -> BassStudentMLP:
    dev = torch.device(device or ("cuda" if torch.cuda.is_available() else "cpu"))
    model = BassStudentMLP().to(dev)
    opt = torch.optim.Adam(model.parameters(), lr=lr)
    loss_fn = nn.MSELoss()
    model.train()
    for _ in range(steps):
        xb = np.stack(
            [
                pack_bridge_input(
                    np.random.rand(10),
                    np.random.rand(16) > 0.65,
                    int(np.random.randint(0, 12)),
                    int(np.random.randint(0, 10)),
                    int(np.random.randint(0, 8)),
                    np.random.randn(64).astype(np.float32) * 0.1,
                )
                for _ in range(batch)
            ]
        )
        yb = np.stack([synthetic_target_from_input(xb[i]) for i in range(batch)])
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
    hidden: int = 256,
) -> BassStudentMLP:
    """Train BassStudentMLP from distilled (N, 93) / (N, 64) arrays."""
    dev = torch.device(device or ("cuda" if torch.cuda.is_available() else "cpu"))
    X = np.asarray(X, dtype=np.float32)
    Y = np.asarray(Y, dtype=np.float32)
    if X.shape[0] != Y.shape[0]:
        raise ValueError("X and Y must have the same number of rows")
    n = X.shape[0]
    model = BassStudentMLP(hidden=hidden).to(dev)
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


def export_bass_onnx(model: BassStudentMLP, path: str) -> None:
    model.eval()
    dummy = torch.randn(1, BASS_ONNX_IN)
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
    m = train_student_quick()
    out_path = os.path.join(
        os.path.dirname(__file__), "..", "models", "bass_model.onnx"
    )
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    export_bass_onnx(m, out_path)
    print("wrote", out_path, "size", os.path.getsize(out_path))
