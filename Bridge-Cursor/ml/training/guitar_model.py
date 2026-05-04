# === Guitar: strum pattern + inertia delay models ===
# pip install torch numpy onnx onnxruntime pretty_midi matplotlib

# === SETUP ===
import os
import random
from typing import List, Tuple

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader

from utils.dataset_utils import train_val_split, plot_loss

DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")
LAKH_ROOT = os.environ.get("LAKH_ROOT", "")
SEED = 42
random.seed(SEED)
np.random.seed(SEED)
torch.manual_seed(SEED)


def _is_guitar_program(p: int) -> bool:
    return 24 <= p <= 31


# ---------------------------------------------------------------------------
# STRUM MODEL
# ---------------------------------------------------------------------------

# === STRUM DATASET ===
class StrumDataset(Dataset):
    def __init__(self, root: str, max_files: int = 350):
        self.data: List[Tuple[np.ndarray, np.ndarray]] = []
        self._load(root, max_files)
        if not self.data:
            self._synthetic(2000)

    def _load(self, root: str, max_files: int) -> None:
        if not root or not os.path.isdir(root):
            return
        import pretty_midi

        paths = []
        for dp, _, fns in os.walk(root):
            for fn in fns:
                if fn.lower().endswith((".mid", ".midi")):
                    paths.append(os.path.join(dp, fn))
            if len(paths) > max_files * 5:
                break
        random.shuffle(paths)

        for p in paths:
            if len(self.data) >= max_files:
                break
            try:
                pm = pretty_midi.PrettyMIDI(p)
            except Exception:
                continue
            g = next((i for i in pm.instruments if not i.is_drum and _is_guitar_program(i.program)), None)
            if g is None:
                continue
            tempo = 120.0
            _, tempos = pm.get_tempo_changes()
            if len(tempos):
                tempo = float(tempos[0])
            step_dur = (60.0 / max(tempo, 1e-6)) / 4.0
            vel = np.zeros(16, dtype=np.float32)
            for n in g.notes:
                s = int(np.clip(round(n.start / step_dur), 0, 15))
                vel[s] = max(vel[s], n.velocity / 127.0)
            style = np.zeros(8, dtype=np.float32)
            style[random.randint(0, 7)] = 1.0
            ctype = np.zeros(12, dtype=np.float32)
            ctype[random.randint(0, 11)] = 1.0
            dens = np.array([random.random()], dtype=np.float32)
            x = np.concatenate([style, ctype, dens]).astype(np.float32)
            self.data.append((x, vel))

    def _synthetic(self, n: int) -> None:
        for _ in range(n):
            style = np.zeros(8, dtype=np.float32)
            style[random.randint(0, 7)] = 1.0
            ctype = np.zeros(12, dtype=np.float32)
            ctype[random.randint(0, 11)] = 1.0
            dens = np.array([random.random()], dtype=np.float32)
            x = np.concatenate([style, ctype, dens]).astype(np.float32)
            vel = np.random.uniform(0.0, 1.0, 16).astype(np.float32)
            self.data.append((x, vel))

    def __len__(self):
        return len(self.data)

    def __getitem__(self, i: int):
        a, b = self.data[i]
        return torch.from_numpy(a), torch.from_numpy(b)


# === STRUM MODEL ===
class StrumModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(21, 64),
            nn.ReLU(),
            nn.Linear(64, 64),
            nn.ReLU(),
            nn.Linear(64, 16),
            nn.Sigmoid(),
        )

    def forward(self, x):
        return self.net(x)


def train_strum():
    ds = StrumDataset(LAKH_ROOT)
    ix = list(range(len(ds)))
    tr, va = train_val_split(ix, 0.2, SEED)

    class Sub(Dataset):
        def __init__(self, d, idx):
            self.d, self.idx = d, idx

        def __len__(self):
            return len(self.idx)

        def __getitem__(self, i):
            return self.d[self.idx[i]]

    tl = DataLoader(Sub(ds, tr), batch_size=64, shuffle=True)
    vl = DataLoader(Sub(ds, va), batch_size=64, shuffle=False)
    model = StrumModel().to(DEVICE)
    opt = torch.optim.Adam(model.parameters(), lr=0.001)
    loss_fn = nn.MSELoss()
    best, best_sd = float("inf"), None
    tr_l, va_l = [], []
    for epoch in range(100):
        model.train()
        run = 0.0
        for xb, yb in tl:
            xb, yb = xb.to(DEVICE), yb.to(DEVICE)
            opt.zero_grad()
            loss = loss_fn(model(xb), yb)
            loss.backward()
            opt.step()
            run += loss.item() * xb.size(0)
        run /= max(len(tr), 1)
        model.eval()
        vtot = 0.0
        with torch.no_grad():
            for xb, yb in vl:
                xb, yb = xb.to(DEVICE), yb.to(DEVICE)
                vtot += loss_fn(model(xb), yb).item() * xb.size(0)
        vtot /= max(len(va), 1)
        tr_l.append(run)
        va_l.append(vtot)
        if vtot < best:
            best = vtot
            best_sd = {k: v.cpu().clone() for k, v in model.state_dict().items()}
        if (epoch + 1) % 20 == 0:
            print(f"strum epoch {epoch+1}/100 train {run:.5f} val {vtot:.5f}")
    if best_sd:
        model.load_state_dict(best_sd)
    torch.save(model.state_dict(), "guitar_strum_best.pt")
    plot_loss(tr_l, va_l, "Guitar strum")


def export_strum_onnx():
    m = StrumModel()
    m.load_state_dict(torch.load("guitar_strum_best.pt", map_location="cpu"))
    m.eval()
    torch.onnx.export(
        m,
        torch.randn(1, 21),
        "guitar_strum.onnx",
        input_names=["input"],
        output_names=["output"],
        opset_version=17,
    )
    print("guitar_strum.onnx", os.path.getsize("guitar_strum.onnx"))


# ---------------------------------------------------------------------------
# INERTIA MODEL
# ---------------------------------------------------------------------------

# === SYNTHETIC DATA ===
def inertia_dataset(n: int = 10000):
    xs, ys = [], []
    for _ in range(n):
        iv = random.randint(0, 24)
        delay = iv * 1.2 + random.gauss(0, 2.0)
        delay = max(0.0, delay)
        xs.append([iv / 24.0])
        ys.append([delay])
    return torch.tensor(xs, dtype=torch.float32), torch.tensor(ys, dtype=torch.float32)


# === INERTIA MODEL ===
class InertiaModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(1, 16),
            nn.ReLU(),
            nn.Linear(16, 16),
            nn.ReLU(),
            nn.Linear(16, 1),
        )

    def forward(self, x):
        return self.net(x)


def train_inertia():
    x, y = inertia_dataset(10000)
    n = x.size(0)
    perm = torch.randperm(n)
    tr, va = perm[: int(n * 0.8)], perm[int(n * 0.8) :]
    model = InertiaModel().to(DEVICE)
    opt = torch.optim.Adam(model.parameters(), lr=0.001)
    loss_fn = nn.MSELoss()
    for epoch in range(50):
        model.train()
        opt.zero_grad()
        pred = model(x[tr].to(DEVICE))
        loss = loss_fn(pred, y[tr].to(DEVICE))
        loss.backward()
        opt.step()
        if (epoch + 1) % 10 == 0:
            model.eval()
            with torch.no_grad():
                v = loss_fn(model(x[va].to(DEVICE)), y[va].to(DEVICE)).item()
            print(f"inertia epoch {epoch+1}/50 val {v:.4f}")
    torch.save(model.state_dict(), "guitar_inertia_best.pt")


def export_inertia_onnx():
    m = InertiaModel()
    m.load_state_dict(torch.load("guitar_inertia_best.pt", map_location="cpu"))
    m.eval()
    torch.onnx.export(
        m,
        torch.randn(1, 1),
        "guitar_inertia.onnx",
        input_names=["input"],
        output_names=["output"],
        opset_version=17,
    )
    print("guitar_inertia.onnx", os.path.getsize("guitar_inertia.onnx"))


# === VERIFY ===
def verify():
    import onnxruntime as ort

    s = ort.InferenceSession("guitar_strum.onnx", providers=["CPUExecutionProvider"])
    x = np.random.randn(1, 21).astype(np.float32)
    y = s.run(None, {"input": x})[0]
    print("strum", y.shape)

    inc = ort.InferenceSession("guitar_inertia.onnx", providers=["CPUExecutionProvider"])
    for iv in (0, 3, 6, 12, 24):
        t = np.array([[iv / 24.0]], dtype=np.float32)
        d = inc.run(None, {"input": t})[0][0, 0]
        print(f"inertia interval {iv} -> delay_ms ~ {d:.2f}")


if __name__ == "__main__":
    train_strum()
    export_strum_onnx()
    train_inertia()
    export_inertia_onnx()
    verify()
