# === Chords / comping student ONNX: chords_model.onnx (37 → 24) ===
# Input layout (37 floats):
#   [0:10]   shared personality knobs [0,1]
#   [10:13]  tonality: root/11, scale_type/9, style_index/7  (same normalization as bass)
#   [13:21]  previous chord context (8 floats):
#              [0:2]   last two chord roots as pc/11
#              [2:4]   chord type buckets /11
#              [4:6]   voicing density (spread) per chord [0,1]
#              [6:8]   rhythm slot emphasis [0,1]
#   [21:37]  bass pattern context — 16 steps, activity/energy [0,1] (e.g. |Δpitch| or onset)
#
# Output (24 floats): voicing weights in [0,1] (e.g. 12 semitone weights × 2 registers, or
#   8×3 chord-tone weights). Sigmoid final.
from __future__ import annotations

import os

import numpy as np
import torch
import torch.nn as nn

CHORDS_ONNX_IN = 37
CHORDS_ONNX_OUT = 24


class ChordsStudentMLP(nn.Module):
    def __init__(self, hidden: int = 128):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(CHORDS_ONNX_IN, hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.ReLU(),
            nn.Linear(hidden, CHORDS_ONNX_OUT),
            nn.Sigmoid(),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def pack_chords_input(
    knobs10: np.ndarray,
    root_note: int,
    scale_type: int,
    style_index: int,
    chord_context8: np.ndarray,
    bass_pattern16: np.ndarray,
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
    cc = np.zeros(8, dtype=np.float32)
    c = np.asarray(chord_context8, dtype=np.float32).reshape(-1)
    cc[: min(8, c.size)] = np.clip(c[:8], 0, 1)
    bp = np.zeros(16, dtype=np.float32)
    b = np.asarray(bass_pattern16, dtype=np.float32).reshape(-1)
    bp[: min(16, b.size)] = np.clip(b[:16], 0, 1)
    return np.concatenate([k, tonal, cc, bp])


def synthetic_chords_target(x: np.ndarray) -> np.ndarray:
    rng = np.random.default_rng(int(np.sum(x * 1e4) % (2**31)))
    root = x[10] * 11.0
    bass = x[21:37]
    out = np.zeros(CHORDS_ONNX_OUT, dtype=np.float32)
    energy = float(np.mean(bass)) + float(x[0]) * 0.2
    for i in range(CHORDS_ONNX_OUT):
        out[i] = float(
            np.clip(
                rng.uniform(0.2, 0.9) * (0.5 + energy * 0.5) + (root / 132.0) * (i % 3),
                0,
                1,
            )
        )
    return out


def train_chords_student_quick(
    steps: int = 1500,
    batch: int = 64,
    lr: float = 1e-3,
    device: str | None = None,
) -> ChordsStudentMLP:
    dev = torch.device(device or ("cuda" if torch.cuda.is_available() else "cpu"))
    model = ChordsStudentMLP().to(dev)
    opt = torch.optim.Adam(model.parameters(), lr=lr)
    loss_fn = nn.MSELoss()
    model.train()
    for _ in range(steps):
        xb = np.stack(
            [
                pack_chords_input(
                    np.random.rand(10),
                    int(np.random.randint(0, 12)),
                    int(np.random.randint(0, 10)),
                    int(np.random.randint(0, 8)),
                    np.random.rand(8),
                    np.random.rand(16),
                )
                for _ in range(batch)
            ]
        )
        yb = np.stack([synthetic_chords_target(xb[i]) for i in range(batch)])
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
) -> ChordsStudentMLP:
    """Train ChordsStudentMLP from distilled (N, 37) / (N, 24) arrays."""
    dev = torch.device(device or ("cuda" if torch.cuda.is_available() else "cpu"))
    X = np.asarray(X, dtype=np.float32)
    Y = np.asarray(Y, dtype=np.float32)
    if X.shape[0] != Y.shape[0]:
        raise ValueError("X and Y must have the same number of rows")
    n = X.shape[0]
    model = ChordsStudentMLP(hidden=hidden).to(dev)
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


def export_chords_onnx(model: ChordsStudentMLP, path: str) -> None:
    model.eval()
    dummy = torch.randn(1, CHORDS_ONNX_IN)
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
    m = train_chords_student_quick()
    out_path = os.path.join(os.path.dirname(__file__), "..", "models", "chords_model.onnx")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    export_chords_onnx(m, out_path)
    print("wrote", out_path, os.path.getsize(out_path))
