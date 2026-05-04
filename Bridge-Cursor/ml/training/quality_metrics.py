# === Perceptual proxies for generated MIDI (NumPy + pretty_midi only) ===
from __future__ import annotations

import math
import os
import tempfile
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd

_SCRIPT_DIR = Path(__file__).resolve().parent
try:
    import pretty_midi
except ImportError as e:  # pragma: no cover
    raise ImportError("pretty_midi required: pip install pretty_midi") from e


METRIC_KEYS = (
    "rhythmic_consistency",  # lower = tighter timing (IOI var normalized)
    "pitch_entropy",
    "velocity_range",
    "density",
    "groove_score",
    "melodic_contour_smoothness",  # lower = smoother; NaN for drums
)


def _load_pm(path: str) -> pretty_midi.PrettyMIDI:
    return pretty_midi.PrettyMIDI(path)


def _note_events_for_instrument(
    pm: pretty_midi.PrettyMIDI, instrument: str
) -> list[tuple[float, float, int, int, bool]]:
    """(start, end, pitch, velocity, is_drum) for relevant tracks."""
    inst = instrument.lower().strip()
    out: list[tuple[float, float, int, int, bool]] = []
    for ins in pm.instruments:
        if inst == "drums":
            if not ins.is_drum:
                continue
        else:
            if ins.is_drum:
                continue
        for n in ins.notes:
            out.append(
                (float(n.start), float(n.end), int(n.pitch), int(n.velocity), bool(ins.is_drum))
            )
    return out


def _shannon_pc_entropy(pitches: np.ndarray) -> float:
    if pitches.size == 0:
        return 0.0
    pc = pitches % 12
    hist = np.bincount(pc, minlength=12).astype(np.float64)
    total = hist.sum()
    if total <= 0:
        return 0.0
    p = hist / total
    p = p[p > 1e-12]
    return float(-np.sum(p * np.log2(p)))


def _mono_pitch_sequence(events: list[tuple[float, float, int, int, bool]]) -> list[int]:
    """One pitch per onset time (highest simultaneous note)."""
    by_onset: dict[float, int] = {}
    for start, _end, pitch, _vel, _ in events:
        t = round(start, 5)
        by_onset[t] = max(by_onset.get(t, -999), pitch)
    if not by_onset:
        return []
    return [by_onset[k] for k in sorted(by_onset.keys())]


def _ioi_variance_normalized(onsets_sec: np.ndarray, bpm: float) -> float:
    if onsets_sec.size < 3:
        return 0.0
    ioi = np.diff(np.sort(onsets_sec))
    ioi = ioi[ioi > 1e-6]
    if ioi.size < 2:
        return 0.0
    beat = 60.0 / float(bpm)
    return float(np.var(ioi) / (beat * beat))


def _groove_strong_weak_ratio(onsets_sec: np.ndarray, bpm: float) -> float:
    """Ratio of onsets on even sixteenth indices vs odd (within bar)."""
    if onsets_sec.size == 0:
        return 0.5
    strong = weak = 0
    for t in onsets_sec:
        six_total = t * (float(bpm) / 60.0) * 4.0
        six_in_bar = int(round(six_total)) % 16
        if six_in_bar % 2 == 0:
            strong += 1
        else:
            weak += 1
    if strong + weak == 0:
        return 0.5
    return float(strong / (strong + weak))


def score_pretty_midi(pm: pretty_midi.PrettyMIDI, instrument: str, bpm: float = 120.0) -> dict[str, Any]:
    inst = instrument.lower().strip()
    events = _note_events_for_instrument(pm, inst)
    if not events:
        return {
            "rhythmic_consistency": 0.0,
            "pitch_entropy": 0.0,
            "velocity_range": 0.0,
            "density": 0.0,
            "groove_score": 0.5,
            "melodic_contour_smoothness": float("nan"),
            "duration_beats": 0.0,
            "n_notes": 0,
        }

    onsets = np.array([e[0] for e in events], dtype=np.float64)
    ends = np.array([e[1] for e in events], dtype=np.float64)
    pitches = np.array([e[2] for e in events], dtype=np.int32)
    vels_np = np.array([e[3] for e in events], dtype=np.float64) if events else np.zeros(0)

    t_end = float(max(ends.max(), onsets.max()) + 1e-6) if ends.size else 0.0
    duration_beats = t_end * float(bpm) / 60.0
    n_notes = len(events)
    density = float(n_notes / duration_beats) if duration_beats > 1e-6 else 0.0

    ioi_norm = _ioi_variance_normalized(onsets, bpm)
    pent = _shannon_pc_entropy(pitches)
    vrange = float(vels_np.max() - vels_np.min()) if vels_np.size else 0.0
    groove = _groove_strong_weak_ratio(onsets, bpm)

    if inst == "drums":
        contour = float("nan")
    else:
        mono = _mono_pitch_sequence(events)
        if len(mono) < 2:
            contour = 0.0
        else:
            intervals = np.abs(np.diff(np.array(mono, dtype=np.float64)))
            contour = float(np.mean(intervals))

    return {
        "rhythmic_consistency": ioi_norm,
        "pitch_entropy": pent,
        "velocity_range": vrange,
        "density": density,
        "groove_score": groove,
        "melodic_contour_smoothness": contour,
        "duration_beats": float(duration_beats),
        "n_notes": int(n_notes),
    }


def score_midi_file(path: str, instrument: str, bpm: float = 120.0) -> dict[str, Any]:
    pm = _load_pm(path)
    out = score_pretty_midi(pm, instrument, bpm)
    out["path"] = path
    return out


def compare_midi_files(path_a: str, path_b: str, instrument: str, bpm: float = 120.0) -> dict[str, Any]:
    sa = score_midi_file(path_a, instrument, bpm)
    sb = score_midi_file(path_b, instrument, bpm)
    delta: dict[str, Any] = {}
    for k in METRIC_KEYS:
        va, vb = sa.get(k), sb.get(k)
        if va is None or vb is None:
            continue
        if isinstance(va, (int, float)) and isinstance(vb, (int, float)) and not (
            isinstance(va, float) and math.isnan(va) or isinstance(vb, float) and math.isnan(vb)
        ):
            if isinstance(va, float) and math.isnan(va):
                continue
            if isinstance(vb, float) and math.isnan(vb):
                continue
            delta[f"delta_{k}"] = float(vb) - float(va)
    return {"a": path_a, "b": path_b, "instrument": instrument, **delta}


def score_preset_sweep(
    models_dir: str,
    presets: dict[str, list[float]],
    instrument: str,
    bpm: float = 120.0,
    root: int = 0,
    octave: int = 3,
    chord_octave: int = 4,
    melody_octave: int = 5,
) -> pd.DataFrame:
    """For each preset knob vector: ORT → MIDI (temp file) → metrics."""
    from onnx_runtime_inputs import DEFAULT_MODEL_FILES
    from validate_midi_output import knobs_array_to_pretty_midi

    inst = instrument.lower().strip()
    if inst not in DEFAULT_MODEL_FILES:
        raise ValueError(f"instrument must be one of {list(DEFAULT_MODEL_FILES)}")
    model_path = Path(models_dir) / DEFAULT_MODEL_FILES[inst]
    if not model_path.is_file():
        raise FileNotFoundError(model_path)

    rows: list[dict[str, Any]] = []
    for preset_name, vec in presets.items():
        pm, _y = knobs_array_to_pretty_midi(
            model_path,
            inst,
            vec,
            bpm=bpm,
            root=root,
            octave=octave,
            chord_octave=chord_octave,
            melody_octave=melody_octave,
        )
        fd, tmp = tempfile.mkstemp(suffix=".mid")
        os.close(fd)
        try:
            pm.write(tmp)
            sc = score_midi_file(tmp, inst, bpm)
            sc.pop("path", None)
            sc["preset"] = preset_name
            rows.append(sc)
        finally:
            try:
                os.unlink(tmp)
            except OSError:
                pass

    df = pd.DataFrame(rows)
    if not df.empty and "preset" in df.columns:
        cols = ["preset"] + [c for c in df.columns if c != "preset"]
        df = df[[c for c in cols if c in df.columns]]
    return df


def perceptual_verdict(metrics: dict[str, Any], instrument: str) -> str:
    """
    Colab one-liner: flag anemic / undifferentiated output heuristically.
    """
    inst = instrument.lower().strip()
    dens = float(metrics.get("density", 0.0) or 0.0)
    pent = float(metrics.get("pitch_entropy", 0.0) or 0.0)
    n_notes = int(metrics.get("n_notes", 0) or 0)
    groove = float(metrics.get("groove_score", 0.5) or 0.5)

    if n_notes < 2 or dens < 0.05:
        return "⚠ Check knob calibration"
    if inst == "drums":
        if pent < 0.4 and dens < 0.35:
            return "⚠ Check knob calibration"
        if abs(groove - 0.5) < 0.03 and dens < 0.25:
            return "⚠ Check knob calibration"
        return "✓ Musically differentiated"
    contour = metrics.get("melodic_contour_smoothness")
    if contour is not None and isinstance(contour, (int, float)) and not (
        isinstance(contour, float) and math.isnan(contour)
    ):
        if pent < 0.35 and float(contour) < 0.01 and dens < 0.4:
            return "⚠ Check knob calibration"
    if pent < 0.3 and dens < 0.3:
        return "⚠ Check knob calibration"
    return "✓ Musically differentiated"
