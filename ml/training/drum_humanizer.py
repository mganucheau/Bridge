# === Drum humanizer — train in Colab, export drum_humanizer.onnx ===
# Groove MIDI: https://magenta.tensorflow.org/datasets/groove (download & set GROOVE_ROOT)

# === SETUP ===
# pip install torch pretty_midi numpy onnx onnxruntime matplotlib
import os
import random
from typing import List, Tuple

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader

from utils.midi_utils import get_tempo, get_step_duration, offset_ms, snap_to_step
from utils.dataset_utils import train_val_split, plot_loss

DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")
GROOVE_ROOT = os.environ.get("GROOVE_ROOT", "")  # folder with .mid / .midi from Groove
STYLE_LABELS = list(range(8))  # map subset of genres → 0..7
SEED = 42
random.seed(SEED)
np.random.seed(SEED)
torch.manual_seed(SEED)


# === DATASET ===
class GrooveDataset(Dataset):
    """
    Reads Groove-style monophonic/percussion MIDI: builds (input [129], target [256]).
    Input: 16×8 binary drum grid + style / 8. Target: 128 normalized offsets + 128 velocity scales.
    When no files are found, synthesizes random paired examples so the notebook always runs.
    """

    def __init__(self, root: str, style_labels: List[int]):
        self.samples: List[Tuple[np.ndarray, np.ndarray]] = []
        self._scan(root, style_labels)
        if not self.samples:
            self._synthetic(2000)

    def _scan(self, root: str, style_labels: List[int]) -> None:
        if not root or not os.path.isdir(root):
            return
        import pretty_midi

        paths = []
        for dp, _, fns in os.walk(root):
            for fn in fns:
                if fn.lower().endswith((".mid", ".midi")):
                    paths.append(os.path.join(dp, fn))
        random.shuffle(paths)
        paths = paths[:800]

        for p in paths:
            try:
                pm = pretty_midi.PrettyMIDI(p)
            except Exception:
                continue
            tempo = get_tempo(pm)
            step_dur = get_step_duration(tempo, subdivisions=16)
            grid = np.zeros((16, 8), dtype=np.float32)
            offs = np.zeros((16, 8), dtype=np.float32)
            vels = np.zeros((16, 8), dtype=np.float32)

            for inst in pm.instruments:
                if inst.is_drum:
                    for n in inst.notes:
                        step = snap_to_step(n.start, step_dur) % 16
                        # crude GM mapping
                        vel_n = float(n.velocity) / 127.0
                        if 35 <= n.pitch <= 36:
                            d = 0
                        elif n.pitch in (38, 40):
                            d = 1
                        elif n.pitch in (42, 44):
                            d = 2
                        elif n.pitch in (46, 26):
                            d = 3
                        elif n.pitch == 41:
                            d = 4
                        elif n.pitch == 45:
                            d = 5
                        elif n.pitch == 48:
                            d = 6
                        elif n.pitch in (49, 51):
                            d = 7
                        else:
                            continue
                        grid[step, d] = 1.0
                        offs[step, d] = offset_ms(n.start, step, step_dur)
                        vels[step, d] = vel_n

            style = random.choice(style_labels) if style_labels else 0
            inp = np.zeros(129, dtype=np.float32)
            inp[:128] = grid.reshape(-1)
            inp[128] = float(style) / 8.0
            tgt = np.zeros(256, dtype=np.float32)
            tgt[:128] = np.clip(offs.reshape(-1) / 30.0, -1.0, 1.0)
            tgt[128:] = np.clip(vels.reshape(-1), 0.0, 1.0)
            self.samples.append((inp, tgt))

    def _synthetic(self, n: int) -> None:
        for _ in range(n):
            grid = (np.random.rand(16, 8) > 0.82).astype(np.float32)
            style = random.randint(0, 7)
            inp = np.zeros(129, dtype=np.float32)
            inp[:128] = grid.reshape(-1)
            inp[128] = style / 8.0
            tgt = np.zeros(256, dtype=np.float32)
            tgt[:128] = np.random.uniform(-1, 1, 128).astype(np.float32)
            tgt[128:] = np.random.uniform(0.2, 1.0, 128).astype(np.float32)
            self.samples.append((inp, tgt))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, i: int):
        x, y = self.samples[i]
        return torch.from_numpy(x), torch.from_numpy(y)


# === MODEL ===
class DrumHumanizer(nn.Module):
    def __init__(self):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(129, 256),
            nn.ReLU(),
            nn.Linear(256, 256),
            nn.ReLU(),
            nn.Linear(256, 256),
        )

    def forward(self, x):
        return self.net(x)


# === TRAINING ===
def main_train():
    ds = GrooveDataset(GROOVE_ROOT, STYLE_LABELS)
    train_s, val_s = train_val_split(list(range(len(ds))), val_fraction=0.2, seed=SEED)

    class SubsetDS(Dataset):
        def __init__(self, base, idxs):
            self.base = base
            self.idxs = idxs

        def __len__(self):
            return len(self.idxs)

        def __getitem__(self, i):
            return self.base[self.idxs[i]]

    train_loader = DataLoader(SubsetDS(ds, train_s), batch_size=64, shuffle=True)
    val_loader = DataLoader(SubsetDS(ds, val_s), batch_size=64, shuffle=False)

    model = DrumHumanizer().to(DEVICE)
    opt = torch.optim.Adam(model.parameters(), lr=0.001)
    loss_fn = nn.MSELoss()

    train_losses, val_losses = [], []
    best_val = float("inf")
    best_state = None

    for epoch in range(150):
        model.train()
        tl = 0.0
        for xb, yb in train_loader:
            xb, yb = xb.to(DEVICE), yb.to(DEVICE)
            opt.zero_grad()
            pred = model(xb)
            loss = loss_fn(pred, yb)
            loss.backward()
            opt.step()
            tl += loss.item() * xb.size(0)
        tl /= max(len(train_s), 1)

        model.eval()
        vl = 0.0
        with torch.no_grad():
            for xb, yb in val_loader:
                xb, yb = xb.to(DEVICE), yb.to(DEVICE)
                vl += loss_fn(model(xb), yb).item() * xb.size(0)
        vl /= max(len(val_s), 1)

        train_losses.append(tl)
        val_losses.append(vl)
        if vl < best_val:
            best_val = vl
            best_state = {k: v.cpu().clone() for k, v in model.state_dict().items()}
        if (epoch + 1) % 10 == 0:
            print(f"epoch {epoch+1}/150  train {tl:.5f}  val {vl:.5f}")

    if best_state:
        model.load_state_dict(best_state)
    torch.save(model.state_dict(), "drum_humanizer_best.pt")
    print("saved drum_humanizer_best.pt  best val", best_val)
    plot_loss(train_losses, val_losses, "Drum humanizer")


# === EXPORT ===
def export_onnx():
    model = DrumHumanizer()
    model.load_state_dict(torch.load("drum_humanizer_best.pt", map_location="cpu"))
    model.eval()
    dummy = torch.randn(1, 129)
    torch.onnx.export(
        model,
        dummy,
        "drum_humanizer.onnx",
        input_names=["input"],
        output_names=["output"],
        opset_version=17,
        dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}},
    )
    sz = os.path.getsize("drum_humanizer.onnx")
    print("drum_humanizer.onnx bytes:", sz)


# === VERIFY ===
def verify():
    import onnxruntime as ort

    sess = ort.InferenceSession("drum_humanizer.onnx", providers=["CPUExecutionProvider"])
    x = np.random.randn(1, 129).astype(np.float32)
    y = sess.run(None, {"input": x})[0]
    print("output shape", y.shape, "min", y.min(), "max", y.max())
    assert y.shape == (1, 256)


if __name__ == "__main__":
    main_train()
    export_onnx()
    verify()
