# === MIDI timing helpers for ML datasets (PrettyMIDI) ===
from __future__ import annotations

import numpy as np
import pretty_midi


def get_tempo(pm: pretty_midi.PrettyMIDI) -> float:
    """Return first tempo from the MIDI file, in BPM; default 120."""
    _, tempos = pm.get_tempo_changes()
    if len(tempos) > 0:
        return float(tempos[0])
    return 120.0


def get_step_duration(tempo: float, subdivisions: int = 4) -> float:
    """Duration of one grid step in seconds (one bar = `subdivisions` beats at `tempo` BPM)."""
    tempo = max(tempo, 1e-6)
    subdivisions = max(int(subdivisions), 1)
    beat_sec = 60.0 / tempo
    return beat_sec / float(subdivisions)


def snap_to_step(time: float, step_dur: float) -> int:
    """Nearest 16th-step index (0-based) for a time in seconds."""
    if step_dur <= 0:
        return 0
    return int(np.round(time / step_dur))


def offset_ms(actual_time: float, step: int, step_dur: float) -> float:
    """Humanization offset: (actual − quantized grid) in milliseconds."""
    quantized = float(step) * step_dur
    return (actual_time - quantized) * 1000.0
