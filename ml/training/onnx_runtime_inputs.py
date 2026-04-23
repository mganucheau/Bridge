# === Pure-numpy ONNX input packing (no PyTorch) for validation scripts ===
from __future__ import annotations

import numpy as np

from knob_mapping import NUM_KNOBS

DEFAULT_MODEL_FILES = {
    "drums": "drum_humanizer.onnx",
    "bass": "bass_model.onnx",
    "chords": "chords_model.onnx",
    "melody": "melody_model.onnx",
}


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


def pack_bridge_input(
    knobs10: np.ndarray,
    kick16: np.ndarray,
    root_note: int,
    scale_type: int,
    style_index: int,
    context64: np.ndarray,
) -> np.ndarray:
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


def _fixed_rng() -> np.random.Generator:
    return np.random.default_rng(20250420)


def build_fixed_input(instrument: str, knobs10: np.ndarray) -> np.ndarray:
    rng = _fixed_rng()
    k = np.clip(np.asarray(knobs10, dtype=np.float32).reshape(NUM_KNOBS), 0.0, 1.0)
    if instrument == "drums":
        prev = rng.random(32, dtype=np.float32)
        groove = np.array([0.52, 0.48, 0.61, 0.37], dtype=np.float32)
        return pack_drum_input(k, prev, groove)
    if instrument == "bass":
        kick = rng.random(16, dtype=np.float32)
        ctx = rng.random(64, dtype=np.float32) * 0.4
        return pack_bridge_input(k, kick, 5, 3, 2, ctx)
    if instrument == "chords":
        cc = rng.random(8, dtype=np.float32) * 0.5
        bass16 = rng.random(16, dtype=np.float32) * 0.6
        return pack_chords_input(k, 5, 3, 2, cc, bass16)
    if instrument == "melody":
        prev = rng.random(16, dtype=np.float32) * 0.45
        grid = rng.random(16, dtype=np.float32) * 0.55
        return pack_melody_input(k, 5, 3, 2, prev, grid)
    raise ValueError(f"unknown instrument {instrument!r}")
