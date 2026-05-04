# === Melody student ONNX: melody_model.onnx (45 → 32) ===
# Input layout (45 floats):
#   [0:10]   shared personality knobs [0,1]
#   [10:13]  tonality: root/11, scale_type/9, style_index/7
#   [13:29]  previous note context — 4 events × 4 features = 16 floats:
#              per event: pitch_class/11, active [0,1], velocity/127, timeShiftNorm (clipped)
#   [29:45]  rhythmic grid — 16 steps, onset emphasis / grid strength [0,1]
#
# Output (32 floats): e.g. 16 steps × (onset probability, pitch-class bias); values in [0,1] via Sigmoid.
from __future__ import annotations

import os

import numpy as np
import torch
import torch.nn as nn

MELODY_ONNX_IN = 45
MELODY_ONNX_OUT = 32


class MelodyStudentMLP(nn.Module):
    def __init__(self, hidden: int = 128):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(MELODY_ONNX_IN, hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.ReLU(),
            nn.Linear(hidden, MELODY_ONNX_OUT),
            nn.Sigmoid(),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def pack_melody_input(
    knobs10: np.ndarray,
    root_note: int,
    scale_type: int,
    style_index: int,
    prev_note_context16: np.ndarray,
    rhythmic_grid16: np.ndarray,
) -> np.ndarray:
    k = np.clip(np.asarray(knobs10, dtype=np.float32).reshape(10), 0, 1)
    tonal = np.array(
        [
            float(np.clip(root_note, 0, 11)) / 11.0,
            float(np.clip(scale_type, 0, 9)) / 9.0,
            float(np.clip(style_index, 0, 7)) / 7.0,
        ],
        dtype=np.float32,
    )
    pn = np.zeros(16, dtype=np.float32)
    p = np.asarray(prev_note_context16, dtype=np.float32).reshape(-1)
    pn[: min(16, p.size)] = p[:16]
    rg = np.zeros(16, dtype=np.float32)
    r = np.asarray(rhythmic_grid16, dtype=np.float32).reshape(-1)
    rg[: min(16, r.size)] = np.clip(r[:16], 0, 1)
    return np.concatenate([k, tonal, pn, rg])


def synthetic_melody_target(x: np.ndarray) -> np.ndarray:
    rng = np.random.default_rng(int(np.sum(x * 1e4) % (2**31)))
    grid = x[29:45]
    out = np.zeros(32, dtype=np.float32)
    for s in range(16):
        g = grid[s] if s < 16 else 0.5
        out[s] = float(np.clip(g * 0.7 + rng.uniform(0, 0.25), 0, 1))
        out[16 + s] = float(np.clip(rng.uniform(0, 1) * (0.3 + x[10] * 0.5), 0, 1))
    return out


def train_melody_student_quick(
    steps: int = 1500,
    batch: int = 64,
    lr: float = 1e-3,
    device: str | None = None,
) -> MelodyStudentMLP:
    dev = torch.device(device or ("cuda" if torch.cuda.is_available() else "cpu"))
    model = MelodyStudentMLP().to(dev)
    opt = torch.optim.Adam(model.parameters(), lr=lr)
    loss_fn = nn.MSELoss()
    model.train()
    for _ in range(steps):
        xb = np.stack(
            [
                pack_melody_input(
                    np.random.rand(10),
                    int(np.random.randint(0, 12)),
                    int(np.random.randint(0, 10)),
                    int(np.random.randint(0, 8)),
                    np.random.rand(16),
                    np.random.rand(16),
                )
                for _ in range(batch)
            ]
        )
        yb = np.stack([synthetic_melody_target(xb[i]) for i in range(batch)])
        xt = torch.from_numpy(xb).to(dev)
        yt = torch.from_numpy(yb).to(dev)
        opt.zero_grad()
        loss = loss_fn(model(xt), yt)
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
) -> MelodyStudentMLP:
    """Train MelodyStudentMLP from distilled (N, 45) / (N, 32) arrays."""
    dev = torch.device(device or ("cuda" if torch.cuda.is_available() else "cpu"))
    X = np.asarray(X, dtype=np.float32)
    Y = np.asarray(Y, dtype=np.float32)
    if X.shape[0] != Y.shape[0]:
        raise ValueError("X and Y must have the same number of rows")
    n = X.shape[0]
    model = MelodyStudentMLP(hidden=hidden).to(dev)
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


def export_melody_onnx(model: MelodyStudentMLP, path: str) -> None:
    model.eval()
    dummy = torch.randn(1, MELODY_ONNX_IN)
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
    m = train_melody_student_quick()
    out_path = os.path.join(os.path.dirname(__file__), "..", "models", "melody_model.onnx")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    export_melody_onnx(m, out_path)
    print("wrote", out_path, os.path.getsize(out_path))
