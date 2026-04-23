# === Bass ML model — Lakh-style MIDI (kick + bass tracks) ===
# NOTE: The plugin’s runtime `bass_model.onnx` now expects 93 inputs (see bass_onnx_student.py).
# This script still exports 19→64 for legacy experiments only — retrain with the student pipeline for Bridge.
# Set LAKH_ROOT to a folder with MIDI files (or rely on synthetic data).

# === SETUP ===
import os
import random
from typing import List, Tuple

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import Dataset, DataLoader

from utils.dataset_utils import train_val_split, plot_loss

DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")
LAKH_ROOT = os.environ.get("LAKH_ROOT", "")
SEED = 42
random.seed(SEED)
np.random.seed(SEED)
torch.manual_seed(SEED)


# === DATASET ===
def _is_bass_program(p: int) -> bool:
    return 32 <= p <= 39


class BassDataset(Dataset):
    def __init__(self, root: str, max_files: int = 400):
        self.items: List[Tuple[np.ndarray, np.ndarray]] = []
        self._load(root, max_files)
        if not self.items:
            self._synthetic(3000)

    def _load(self, root: str, max_files: int) -> None:
        if not root or not os.path.isdir(root):
            return
        import pretty_midi

        paths = []
        for dp, _, fns in os.walk(root):
            for fn in fns:
                if fn.lower().endswith((".mid", ".midi")):
                    paths.append(os.path.join(dp, fn))
            if len(paths) >= max_files * 4:
                break
        random.shuffle(paths)

        for p in paths:
            if len(self.items) >= max_files:
                break
            try:
                pm = pretty_midi.PrettyMIDI(p)
            except Exception:
                continue
            drum_inst = None
            bass_inst = None
            for inst in pm.instruments:
                if inst.is_drum:
                    drum_inst = inst
                elif _is_bass_program(inst.program):
                    bass_inst = inst
            if drum_inst is None or bass_inst is None:
                continue

            tempo = 120.0
            _, tempos = pm.get_tempo_changes()
            if len(tempos):
                tempo = float(tempos[0])
            step_dur = (60.0 / max(tempo, 1e-6)) / 4.0

            kick = np.zeros(16, dtype=np.float32)
            for n in drum_inst.notes:
                if n.pitch in (35, 36):
                    step = int(np.clip(round(n.start / step_dur), 0, 15))
                    kick[step] = 1.0

            pitch_c = np.zeros(16, dtype=np.float32)
            off = np.zeros(16, dtype=np.float32)
            vel = np.zeros(16, dtype=np.float32)
            act = np.zeros(16, dtype=np.float32)
            for n in bass_inst.notes:
                step = int(np.clip(round(n.start / step_dur), 0, 15))
                act[step] = 1.0
                pitch_c[step] = float(n.pitch % 12)
                off[step] = float(np.random.uniform(-0.05, 0.05))  # placeholder micro-timing
                vel[step] = float(n.velocity) / 127.0

            root_note = random.randint(0, 11)
            scale_type = random.randint(0, 9)
            style = random.randint(0, 7)
            x = np.concatenate(
                [
                    kick,
                    [root_note / 11.0, scale_type / 9.0, style / 7.0],
                ]
            ).astype(np.float32)
            y = np.concatenate([pitch_c, off, vel, act]).astype(np.float32)
            self.items.append((x, y))

    def _synthetic(self, n: int) -> None:
        for _ in range(n):
            kick = (np.random.rand(16) > 0.75).astype(np.float32)
            root_note = random.randint(0, 11)
            scale_type = random.randint(0, 9)
            style = random.randint(0, 7)
            x = np.concatenate(
                [kick, [root_note / 11.0, scale_type / 9.0, style / 7.0]]
            ).astype(np.float32)
            pc = np.random.randint(0, 12, 16).astype(np.float32)
            off = np.random.uniform(-1, 1, 16).astype(np.float32) * 0.3
            vel = np.random.uniform(0.2, 1.0, 16).astype(np.float32)
            act = (np.random.rand(16) > 0.35).astype(np.float32)
            y = np.concatenate([pc, off, vel, act]).astype(np.float32)
            self.items.append((x, y))

    def __len__(self):
        return len(self.items)

    def __getitem__(self, i: int):
        x, y = self.items[i]
        return torch.from_numpy(x), torch.from_numpy(y)


# === MODEL ===
class BassModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.emb_style = nn.Embedding(8, 8)
        self.emb_root = nn.Embedding(12, 8)
        self.emb_scale = nn.Embedding(10, 8)
        self.rnn = nn.GRU(25, 128, num_layers=2, batch_first=True)
        self.head_pitch = nn.Linear(128, 12 * 16)
        self.head_off = nn.Linear(128, 16)
        self.head_vel = nn.Linear(128, 16)
        self.head_act = nn.Linear(128, 16)

    def predict_heads(self, xb: torch.Tensor):
        # xb: B,19 — kick(16) + root, scale, style as fractions
        B = xb.size(0)
        kick = xb[:, :16]
        root = (xb[:, 16] * 11).long().clamp(0, 11)
        sc = (xb[:, 17] * 9).long().clamp(0, 9)
        st = (xb[:, 18] * 7 + 0.5).long().clamp(0, 7)
        emb = torch.cat(
            [self.emb_root(root), self.emb_scale(sc), self.emb_style(st)], dim=-1
        )
        rnn_in = torch.cat([kick, emb], dim=-1).unsqueeze(1).expand(-1, 16, -1)
        h, _ = self.rnn(rnn_in)
        pitch_logits = self.head_pitch(h).view(B, 16, 12)
        off = self.head_off(h)
        vel = torch.sigmoid(self.head_vel(h))
        act = self.head_act(h)
        return pitch_logits, off, vel, act

    def forward(self, xb: torch.Tensor):
        pl, off, vel, act_log = self.predict_heads(xb)
        pc = torch.argmax(pl, dim=-1).float()
        act = torch.sigmoid(act_log)
        return torch.cat([pc, off, vel, act], dim=1)


# === TRAINING ===
def train_loop():
    ds = BassDataset(LAKH_ROOT)
    idxs = list(range(len(ds)))
    tr_i, va_i = train_val_split(idxs, val_fraction=0.2, seed=SEED)

    class Sub(Dataset):
        def __init__(self, d, ix):
            self.d, self.ix = d, ix

        def __len__(self):
            return len(self.ix)

        def __getitem__(self, i):
            return self.d[self.ix[i]]

    tl = DataLoader(Sub(ds, tr_i), batch_size=64, shuffle=True)
    vl = DataLoader(Sub(ds, va_i), batch_size=64, shuffle=False)

    model = BassModel().to(DEVICE)
    opt = torch.optim.Adam(model.parameters(), lr=0.001)
    ce = nn.CrossEntropyLoss()
    mse = nn.MSELoss()
    bce = nn.BCEWithLogitsLoss()

    tr_losses, va_losses = [], []
    best = float("inf")
    best_sd = None

    for epoch in range(200):
        model.train()
        run = 0.0
        for xb, yb in tl:
            xb, yb = xb.to(DEVICE), yb.to(DEVICE)
            pc_t = yb[:, :16].long().clamp(0, 11)
            off_t = yb[:, 16:32]
            vel_t = yb[:, 32:48]
            act_t = yb[:, 48:64]
            opt.zero_grad()
            pl, off, vel, act_log = model.predict_heads(xb)
            loss_p = ce(pl.reshape(-1, 12), pc_t.reshape(-1))
            loss_o = mse(off, off_t)
            loss_v = mse(vel, vel_t)
            loss_a = bce(act_log, act_t)
            loss = 2.0 * loss_p + 1.0 * loss_o + 0.5 * loss_v + 2.0 * loss_a
            loss.backward()
            opt.step()
            run += loss.item() * xb.size(0)
        run /= max(len(tr_i), 1)

        model.eval()
        vtot = 0.0
        with torch.no_grad():
            for xb, yb in vl:
                xb, yb = xb.to(DEVICE), yb.to(DEVICE)
                pc_t = yb[:, :16].long().clamp(0, 11)
                off_t = yb[:, 16:32]
                vel_t = yb[:, 32:48]
                act_t = yb[:, 48:64]
                pl, off, vel, act_log = model.predict_heads(xb)
                loss = (
                    2.0 * ce(pl.reshape(-1, 12), pc_t.reshape(-1))
                    + mse(off, off_t)
                    + 0.5 * mse(vel, vel_t)
                    + 2.0 * bce(act_log, act_t)
                )
                vtot += loss.item() * xb.size(0)
        vtot /= max(len(va_i), 1)
        tr_losses.append(run)
        va_losses.append(vtot)
        if vtot < best:
            best = vtot
            best_sd = {k: v.cpu().clone() for k, v in model.state_dict().items()}
        if (epoch + 1) % 20 == 0:
            print(f"epoch {epoch+1}/200 train {run:.4f} val {vtot:.4f}")

    if best_sd:
        model.load_state_dict(best_sd)
    torch.save(model.state_dict(), "bass_model_best.pt")


def export_onnx():
    m = BassModel()
    m.load_state_dict(torch.load("bass_model_best.pt", map_location="cpu"))
    m.eval()
    dummy = torch.randn(1, 19)
    torch.onnx.export(
        m,
        dummy,
        "bass_model.onnx",
        input_names=["input"],
        output_names=["output"],
        opset_version=17,
        dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}},
    )
    print("bass_model.onnx size", os.path.getsize("bass_model.onnx"))


def verify():
    import onnxruntime as ort

    sess = ort.InferenceSession("bass_model.onnx", providers=["CPUExecutionProvider"])
    kick = np.zeros((1, 19), dtype=np.float32)
    kick[0, :16:4] = 1.0
    kick[0, 16] = 0.0
    kick[0, 17] = 0.3
    kick[0, 18] = 0.5
    y = sess.run(None, {"input": kick})[0]
    print("bass onnx out", y.shape)


if __name__ == "__main__":
    train_loop()
    export_onnx()
    verify()
