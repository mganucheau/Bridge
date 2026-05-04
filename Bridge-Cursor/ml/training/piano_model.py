# === Piano comping model ===
# pip install torch numpy onnx onnxruntime music21 pretty_midi matplotlib

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
def _is_piano_program(p: int) -> bool:
    return 0 <= p <= 7


class PianoDataset(Dataset):
    """Input: chord_root, chord_type, density, style (as floats). Target: 48 floats."""

    def __init__(self, root: str, max_files: int = 300):
        self.rows: List[Tuple[np.ndarray, np.ndarray]] = []
        self._from_midi(root, max_files)
        if not self.rows:
            self._synthetic(2500)

    def _from_midi(self, root: str, max_files: int) -> None:
        if not root or not os.path.isdir(root):
            return
        import pretty_midi

        paths = []
        for dp, _, fns in os.walk(root):
            for fn in fns:
                if fn.lower().endswith((".mid", ".midi")):
                    paths.append(os.path.join(dp, fn))
            if len(paths) > max_files * 6:
                break
        random.shuffle(paths)

        for p in paths:
            if len(self.rows) >= max_files:
                break
            try:
                pm = pretty_midi.PrettyMIDI(p)
            except Exception:
                continue
            piano = next((i for i in pm.instruments if not i.is_drum and _is_piano_program(i.program)), None)
            if piano is None:
                continue
            try:
                from music21 import converter

                s = converter.parse(p)
                m = s.chordify()
                chord_root = 0
                chord_type = 0
                for el in m.flatten().getElementsByClass("Chord"):
                    chord_root = el.root().pitch.midi % 12
                    # simple type bucket
                    iv = len(el.pitches)
                    chord_type = min(11, max(0, iv - 1))
                    break
            except Exception:
                chord_root = random.randint(0, 11)
                chord_type = random.randint(0, 11)

            tempo = 120.0
            _, tempos = pm.get_tempo_changes()
            if len(tempos):
                tempo = float(tempos[0])
            step_dur = (60.0 / max(tempo, 1e-6)) / 4.0

            active = np.zeros(16, dtype=np.float32)
            poff = np.zeros(16, dtype=np.float32)
            vel = np.zeros(16, dtype=np.float32)
            for n in piano.notes:
                step = int(np.clip(round(n.start / step_dur), 0, 15))
                active[step] = 1.0
                poff[step] = float((n.pitch % 12) - chord_root) % 12
                vel[step] = n.velocity / 127.0

            density = float(active.mean())
            style = random.randint(0, 7)
            x = np.array(
                [chord_root / 11.0, chord_type / 11.0, density, style / 7.0], dtype=np.float32
            )
            y = np.concatenate([active, poff, vel]).astype(np.float32)
            self.rows.append((x, y))

    def _synthetic(self, n: int) -> None:
        for _ in range(n):
            cr = random.randint(0, 11)
            ct = random.randint(0, 11)
            density = random.random()
            style = random.randint(0, 7)
            x = np.array([cr / 11.0, ct / 11.0, density, style / 7.0], dtype=np.float32)
            active = (np.random.rand(16) > (1.0 - density)).astype(np.float32)
            poff = np.random.randint(0, 24, 16).astype(np.float32)
            vel = np.random.uniform(0.2, 1.0, 16).astype(np.float32)
            y = np.concatenate([active, poff, vel]).astype(np.float32)
            self.rows.append((x, y))

    def __len__(self):
        return len(self.rows)

    def __getitem__(self, i: int):
        x, y = self.rows[i]
        return torch.from_numpy(x), torch.from_numpy(y)


# === MODEL ===
class PianoComper(nn.Module):
    def __init__(self):
        super().__init__()
        self.emb_root = nn.Embedding(12, 16)
        self.emb_type = nn.Embedding(12, 16)
        self.fc = nn.Linear(34, 128)
        self.rnn = nn.GRU(128, 128, num_layers=2, batch_first=True)
        self.h_act = nn.Linear(128, 16)
        self.h_pitch = nn.Linear(128, 16 * 24)
        self.h_vel = nn.Linear(128, 16)

    def heads(self, xb: torch.Tensor):
        cr = (xb[:, 0] * 11 + 0.5).long().clamp(0, 11)
        ct = (xb[:, 1] * 11 + 0.5).long().clamp(0, 11)
        dens = xb[:, 2:3]
        st = (xb[:, 3] * 7 + 0.5).long().clamp(0, 7)
        emb = torch.cat([self.emb_root(cr), self.emb_type(ct)], dim=-1)
        stf = st.float().unsqueeze(1) / 7.0
        fused = torch.cat([emb, dens, stf], dim=-1)
        h0 = F.relu(self.fc(fused)).unsqueeze(1).expand(-1, 16, -1)
        h, _ = self.rnn(h0)
        act_log = self.h_act(h)
        pitch_l = self.h_pitch(h).view(-1, 16, 24)
        vel = torch.sigmoid(self.h_vel(h))
        return act_log, pitch_l, vel

    def forward(self, xb: torch.Tensor):
        act_log, pitch_l, vel = self.heads(xb)
        active = torch.sigmoid(act_log)
        pc = torch.argmax(pitch_l, dim=-1).float()
        return torch.cat([active, pc, vel], dim=1)


# === TRAINING ===
def train_loop():
    ds = PianoDataset(LAKH_ROOT)
    ix = list(range(len(ds)))
    tr, va = train_val_split(ix, val_fraction=0.2, seed=SEED)

    class Sub(Dataset):
        def __init__(self, d, idx):
            self.d, self.idx = d, idx

        def __len__(self):
            return len(self.idx)

        def __getitem__(self, i):
            return self.d[self.idx[i]]

    tl = DataLoader(Sub(ds, tr), batch_size=64, shuffle=True)
    vl = DataLoader(Sub(ds, va), batch_size=64, shuffle=False)

    model = PianoComper().to(DEVICE)
    opt = torch.optim.Adam(model.parameters(), lr=0.001)
    bce = nn.BCEWithLogitsLoss()
    ce = nn.CrossEntropyLoss()
    mse = nn.MSELoss()

    tr_l, va_l = [], []
    best = float("inf")
    best_sd = None

    for epoch in range(150):
        model.train()
        run = 0.0
        for xb, yb in tl:
            xb, yb = xb.to(DEVICE), yb.to(DEVICE)
            act_t = yb[:, :16]
            pc_t = yb[:, 16:32].long().clamp(0, 23)
            vel_t = yb[:, 32:48]
            opt.zero_grad()
            a_log, p_l, vel = model.heads(xb)
            loss = (
                bce(a_log, act_t)
                + 2.0 * ce(p_l.reshape(-1, 24), pc_t.reshape(-1))
                + mse(vel, vel_t)
            )
            loss.backward()
            opt.step()
            run += loss.item() * xb.size(0)
        run /= max(len(tr), 1)

        model.eval()
        vtot = 0.0
        with torch.no_grad():
            for xb, yb in vl:
                xb, yb = xb.to(DEVICE), yb.to(DEVICE)
                act_t = yb[:, :16]
                pc_t = yb[:, 16:32].long().clamp(0, 23)
                vel_t = yb[:, 32:48]
                a_log, p_l, vel = model.heads(xb)
                loss = (
                    bce(a_log, act_t)
                    + 2.0 * ce(p_l.reshape(-1, 24), pc_t.reshape(-1))
                    + mse(vel, vel_t)
                )
                vtot += loss.item() * xb.size(0)
        vtot /= max(len(va), 1)
        tr_l.append(run)
        va_l.append(vtot)
        if vtot < best:
            best = vtot
            best_sd = {k: v.cpu().clone() for k, v in model.state_dict().items()}
        if (epoch + 1) % 10 == 0:
            print(f"epoch {epoch+1}/150 train {run:.4f} val {vtot:.4f}")

    if best_sd:
        model.load_state_dict(best_sd)
    torch.save(model.state_dict(), "piano_model_best.pt")
    plot_loss(tr_l, va_l, "Piano")


def export_onnx():
    m = PianoComper()
    m.load_state_dict(torch.load("piano_model_best.pt", map_location="cpu"))
    m.eval()
    dummy = torch.randn(1, 4)
    torch.onnx.export(
        m,
        dummy,
        "piano_model.onnx",
        input_names=["input"],
        output_names=["output"],
        opset_version=17,
        dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}},
    )
    print("piano_model.onnx", os.path.getsize("piano_model.onnx"))


def verify():
    import onnxruntime as ort

    sess = ort.InferenceSession("piano_model.onnx", providers=["CPUExecutionProvider"])
    x = np.array([[0.5, 0.3, 0.7, 0.25]], dtype=np.float32)
    y = sess.run(None, {"input": x})[0]
    print("piano out", y.shape)
    assert y.shape[1] == 48


if __name__ == "__main__":
    train_loop()
    export_onnx()
    verify()
