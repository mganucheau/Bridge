# === Shared MIDI / NoteSequence helpers for Magenta distillation (training only) ===
from __future__ import annotations

import os
import warnings
from typing import Any

import numpy as np

try:
    import note_seq
    from note_seq import midi_io
except ImportError:  # pragma: no cover
    note_seq = None
    midi_io = None

# 8 Bridge drum lanes → GM pitches (approximate Roland-style mapping).
VOICE_PRIMARY_PITCHES: tuple[int, ...] = (36, 38, 42, 46, 45, 48, 49, 51)

_GM_PITCH_TO_VOICE: dict[int, int] = {}
for _i, _p in enumerate(VOICE_PRIMARY_PITCHES):
    _GM_PITCH_TO_VOICE[_p] = _i
for _p, _v in [
    (35, 0),
    (37, 1),
    (40, 1),
    (44, 2),
    (47, 4),
    (50, 5),
    (57, 6),
    (53, 7),
]:
    _GM_PITCH_TO_VOICE.setdefault(_p, _v)


def _require_note_seq() -> Any:
    if note_seq is None:
        raise ImportError("note_seq is required — pip install note-seq")
    return note_seq


def fixture_midi_path() -> str:
    return os.path.join(os.path.dirname(__file__), "test_fixtures", "simple_groove.mid")


def ensure_simple_groove_fixture() -> str:
    """Return path to simple_groove.mid, generating it via build script if missing."""
    p = fixture_midi_path()
    if os.path.isfile(p) and os.path.getsize(p) > 32:
        return p
    import subprocess
    import sys

    builder = os.path.join(os.path.dirname(p), "build_simple_groove.py")
    if os.path.isfile(builder):
        subprocess.check_call([sys.executable, builder])
    if not os.path.isfile(p):
        raise FileNotFoundError(f"Could not create MIDI fixture at {p}")
    return p


def load_midi_to_note_sequence(midi_path: str) -> Any:
    """Load a MIDI file into a NoteSequence (note_seq)."""
    ns_mod = _require_note_seq()
    if not os.path.isfile(midi_path):
        raise FileNotFoundError(midi_path)
    return ns_mod.midi_file_to_note_sequence(midi_path)


def quantize_ns(ns: Any, steps_per_quarter: int = 4) -> Any:
    ns_mod = _require_note_seq()
    try:
        return ns_mod.quantize_note_sequence(ns, steps_per_quarter=steps_per_quarter)
    except Exception as e:  # pragma: no cover
        warnings.warn(f"quantize_note_sequence failed ({e}); using unquantized ns")
        return ns


def note_sequence_to_drum_hits(
    ns: Any,
    n_steps: int = 32,
    voices: int = 8,
    slots: int = 4,
) -> np.ndarray:
    """
    Map drum NoteSequence to (32,) float prior grid: voice-major v*4+t, t = last 4 sixteenths.
    Values in [0,1] = onset energy (velocity / 127).
    """
    if n_steps != voices * slots:
        raise ValueError("expected n_steps == voices * slots (32)")
    q = quantize_ns(ns)
    spq = 4
    spb = spq * 4
    bar_len = spb
    # Last 4 sixteenth indices within first bar [12..15]
    out = np.zeros((voices, slots), dtype=np.float32)
    step_ix = [bar_len - slots + t for t in range(slots)]

    for note in q.notes:
        if not note.is_drum:
            continue
        p = int(note.pitch)
        v = _GM_PITCH_TO_VOICE.get(p, -1)
        if v < 0 or v >= voices:
            continue
        if note.quantized_start_step is None:
            continue
        s0 = int(note.quantized_start_step)
        s1 = int(note.quantized_end_step) if note.quantized_end_step else s0 + 1
        vel = np.clip(note.velocity / 127.0, 0.0, 1.0)
        for si, target_step in enumerate(step_ix):
            if s0 <= target_step < max(s1, s0 + 1):
                out[v, si] = max(out[v, si], vel)
    return out.reshape(-1)


def note_sequence_to_bass_events(ns: Any, n_steps: int = 16) -> np.ndarray:
    """
    Build 64-float bass *context* (16×4): per step pc/11, active, vel/127, timeShiftNorm.
    Monophonic: lowest non-drum pitch per quantized step; time shift norm ~0 without micro-timing.
    """
    q = quantize_ns(ns)
    spq = 4
    steps = n_steps
    pc = np.zeros(steps, dtype=np.float32)
    active = np.zeros(steps, dtype=np.float32)
    vel = np.zeros(steps, dtype=np.float32)
    tsn = np.zeros(steps, dtype=np.float32)

    buckets: list[list[tuple[int, int]]] = [[] for _ in range(steps)]

    for note in q.notes:
        if note.is_drum:
            continue
        if note.quantized_start_step is None:
            continue
        s0 = int(note.quantized_start_step)
        s1 = int(note.quantized_end_step) if note.quantized_end_step else s0 + 1
        for s in range(max(0, s0), min(steps, s1)):
            buckets[s].append((int(note.pitch), int(note.velocity)))

    for s in range(steps):
        if not buckets[s]:
            continue
        pitches = [p for p, _ in buckets[s]]
        pmin = min(pitches)
        vels = [vv for pp, vv in buckets[s] if pp == pmin]
        vm = float(np.mean(vels)) if vels else 80.0
        pc[s] = (pmin % 12) / 11.0
        active[s] = 1.0
        vel[s] = np.clip(vm / 127.0, 0.0, 1.0)
        tsn[s] = 0.0

    return np.concatenate([pc, active, vel, tsn], axis=0).astype(np.float32)


def note_sequence_to_chord_context(ns: Any, n_steps: int = 8) -> np.ndarray:
    """
    8-float chord context (matches chords_onnx_student commentary):
      [0:2] last two chord roots pc/11, [2:4] type proxies /11,
      [4:6] spread + density, [6:8] rhythm emphasis halves.
    `n_steps` is the total quantized window (two halves of n_steps//2).
    """
    q = quantize_ns(ns)
    spb = 16
    half = max(1, n_steps // 2)
    hist: list[np.ndarray] = []
    for b in range(2):
        lo = b * half
        hi = min(lo + half, spb)
        chroma = np.zeros(12, dtype=np.float32)
        for note in q.notes:
            if note.is_drum:
                continue
            if note.quantized_start_step is None:
                continue
            s0 = int(note.quantized_start_step)
            s1 = int(note.quantized_end_step) if note.quantized_end_step else s0 + 1
            if max(s0, lo) < min(s1, hi):
                chroma[int(note.pitch) % 12] += note.velocity / 127.0
        hist.append(chroma)

    roots = [int(np.argmax(h)) for h in hist]
    types = [float(np.argmax(h) % 8) for h in hist]

    lo_midi, hi_midi, n_notes = 127, 0, 0
    for note in q.notes:
        if note.is_drum:
            continue
        lo_midi = min(lo_midi, int(note.pitch))
        hi_midi = max(hi_midi, int(note.pitch))
        n_notes += 1
    spread = np.clip((hi_midi - lo_midi) / 36.0, 0.0, 1.0) if n_notes else 0.0

    e0 = float(np.sum(hist[0])) * 0.25 if len(hist) > 0 else 0.0
    e1 = float(np.sum(hist[1])) * 0.25 if len(hist) > 1 else 0.0

    return np.array(
        [
            roots[0] / 11.0,
            roots[1] / 11.0 if len(roots) > 1 else roots[0] / 11.0,
            types[0] / 11.0,
            types[1] / 11.0 if len(types) > 1 else types[0] / 11.0,
            spread,
            np.clip(n_notes / 32.0, 0.0, 1.0),
            np.clip(e0, 0.0, 1.0),
            np.clip(e1, 0.0, 1.0),
        ],
        dtype=np.float32,
    )


def note_sequence_to_melody_context(ns: Any, n_steps: int = 4) -> np.ndarray:
    """
    16 floats: last `n_steps` melodic events × (pc/11, active, vel/127, timeShiftNorm), most recent first.
    (`n_steps` here means number of prior events, matching Bridge 4×4 layout.)
    """
    q = quantize_ns(ns)
    melodic = [
        n
        for n in q.notes
        if not n.is_drum and n.quantized_start_step is not None
    ]
    melodic.sort(key=lambda n: int(n.quantized_start_step), reverse=True)
    out = np.zeros(16, dtype=np.float32)
    count = 0
    for note in melodic:
        if count >= n_steps:
            break
        base = count * 4
        out[base + 0] = (int(note.pitch) % 12) / 11.0
        out[base + 1] = 1.0
        out[base + 2] = np.clip(note.velocity / 127.0, 0.0, 1.0)
        out[base + 3] = 0.0
        count += 1
    return out


def build_groove_scalars(ns: Any) -> np.ndarray:
    """
    4 floats: swing, step_density, backbeat_strength, syncopation — aligned with drum student.
    Heuristic from quantized drum hits.
    """
    q = quantize_ns(ns)
    spb = 16
    grid = np.zeros(spb, dtype=np.float32)
    snare_hit = np.zeros(spb, dtype=np.float32)
    for note in q.notes:
        if not note.is_drum or note.quantized_start_step is None:
            continue
        s = int(note.quantized_start_step) % spb
        e = np.clip(note.velocity / 127.0, 0.0, 1.0)
        grid[s] = max(grid[s], e)
        if int(note.pitch) in (38, 40, 37):
            snare_hit[s] = max(snare_hit[s], e)

    density = float(np.mean(grid > 0.05))
    backbeat = float((snare_hit[4] + snare_hit[12]) / 2.0) if spb > 12 else 0.0
    off = sum(grid[i] for i in range(spb) if i % 2 == 1)
    onb = sum(grid[i] for i in range(spb) if i % 2 == 0) + 1e-6
    syncop = float(np.clip(off / onb, 0.0, 1.0))
    # swing proxy: asymmetry of 8th pairs
    pairs = []
    for b in range(4):
        i = b * 4 + 1
        j = b * 4 + 2
        if i < spb and j < spb:
            pairs.append(abs(grid[i] - grid[j]))
    swing = float(np.mean(pairs)) if pairs else 0.15
    swing = np.clip(swing + 0.1, 0.0, 1.0)
    return np.array([swing, density, backbeat, syncop], dtype=np.float32)


def melody_ns_to_bridge_target(ns: Any, n_steps: int = 16) -> np.ndarray:
    """Convert monophonic / top-line melody NS to 32-float student target (onset + pc bias)."""
    q = quantize_ns(ns)
    onset = np.zeros(n_steps, dtype=np.float32)
    pc_bias = np.zeros(n_steps, dtype=np.float32)
    for note in q.notes:
        if note.is_drum or note.quantized_start_step is None:
            continue
        s0 = int(note.quantized_start_step)
        if s0 < 0 or s0 >= n_steps:
            continue
        onset[s0] = max(onset[s0], np.clip(note.velocity / 127.0, 0.0, 1.0))
        pc_bias[s0] = (int(note.pitch) % 12) / 11.0
    # smooth onset slightly
    for s in range(1, n_steps - 1):
        if onset[s] < 0.05:
            onset[s] = 0.25 * (onset[s - 1] + onset[s + 1])
    return np.concatenate([onset, pc_bias], axis=0).astype(np.float32)


def bass_ns_to_bridge_target(ns: Any, n_steps: int = 16) -> np.ndarray:
    """64-float bass teacher target: pc×16, offset×16, vel×16, active×16."""
    q = quantize_ns(ns)
    out = np.zeros(64, dtype=np.float32)
    for note in q.notes:
        if note.is_drum or note.quantized_start_step is None:
            continue
        s0 = int(note.quantized_start_step)
        if s0 < 0 or s0 >= n_steps:
            continue
        out[s0] = (int(note.pitch) % 12) / 11.0
        out[16 + s0] = 0.0
        out[32 + s0] = np.clip(note.velocity / 127.0, 0.0, 1.0)
        out[48 + s0] = 1.0
    return out


def chords_ns_to_bridge_target(ns: Any) -> np.ndarray:
    """24-float chord voicing weights from chroma energy per half-bar × registers."""
    q = quantize_ns(ns)
    spb = 16
    weights = []
    for b in range(2):
        lo = b * 8
        hi = lo + 8
        c = np.zeros(12, dtype=np.float32)
        for note in q.notes:
            if note.is_drum:
                continue
            if note.quantized_start_step is None:
                continue
            s0 = int(note.quantized_start_step)
            if lo <= s0 < hi:
                c[int(note.pitch) % 12] += note.velocity / 127.0
        if c.max() > 1e-6:
            c = c / c.max()
        weights.append(c)
    w0, w1 = weights[0], weights[1] if len(weights) > 1 else weights[0]
    return np.concatenate([w0, w1], axis=0).astype(np.float32)


# --- Validation / audition (ORT + pretty_midi only; no TensorFlow) -----------------


def _require_pretty_midi():  # pragma: no cover
    try:
        import pretty_midi
    except ImportError as e:
        raise ImportError("pretty_midi is required for MIDI export: pip install pretty_midi") from e
    return pretty_midi


def bridge_drums_output_to_pretty_midi(
    y32: np.ndarray,
    bpm: float = 120.0,
    hit_threshold: float = 0.45,
) -> Any:
    """
    Map 32-float drum student output (8 voices × 4 slots, last bar sixteenths) to a one-bar GM drum clip.
    """
    pm_mod = _require_pretty_midi()
    pm = pm_mod.PrettyMIDI(initial_tempo=float(bpm))
    inst = pm_mod.Instrument(program=0, is_drum=True)
    y = np.asarray(y32, dtype=np.float32).reshape(8, 4)
    sec_per_step = (60.0 / float(bpm)) / 4.0
    for v in range(8):
        for t in range(4):
            p = float(y[v, t])
            if p < hit_threshold:
                continue
            step = 12 + t
            start = step * sec_per_step
            end = start + sec_per_step * 0.92
            pitch = int(VOICE_PRIMARY_PITCHES[v])
            vel = int(np.clip(round(p * 127.0), 1, 127))
            inst.notes.append(pm_mod.Note(velocity=vel, pitch=pitch, start=start, end=end))
    pm.instruments.append(inst)
    return pm


def bridge_bass_output_to_pretty_midi(
    y64: np.ndarray,
    root_note: int = 0,
    octave: int = 3,
    bpm: float = 120.0,
) -> Any:
    """Aligns with BassEngine::mergePatternFromML interpretation (pc, offset×30ms, vel, active)."""
    pm_mod = _require_pretty_midi()
    pm = pm_mod.PrettyMIDI(initial_tempo=float(bpm))
    inst = pm_mod.Instrument(program=33, is_drum=False)
    y = np.asarray(y64, dtype=np.float32).reshape(-1)
    if y.size < 64:
        y = np.pad(y, (0, 64 - y.size))
    root_midi = int(np.clip(root_note + (octave + 1) * 12, 12, 115))
    sec_per_step = (60.0 / float(bpm)) / 4.0
    for s in range(16):
        if float(y[48 + s]) < 0.5:
            continue
        pc = int(np.clip(np.round(float(y[s])), 0, 11))
        midi = int(np.clip(root_midi + pc, 12, 127))
        vel = int(np.clip(np.round(float(y[32 + s]) * 127.0), 1, 127))
        off_s = float(y[16 + s]) * 30.0 / 1000.0
        start = s * sec_per_step + off_s
        end = start + sec_per_step * 0.9
        inst.notes.append(pm_mod.Note(velocity=vel, pitch=midi, start=start, end=end))
    pm.instruments.append(inst)
    return pm


def bridge_chords_output_to_pretty_midi(
    y24: np.ndarray,
    root_note: int = 0,
    octave: int = 4,
    bpm: float = 120.0,
    step_threshold: float = 0.35,
) -> Any:
    """Simplified comp: triad at each active step (matches PianoEngine weight gating, not full voicing tensor)."""
    pm_mod = _require_pretty_midi()
    pm = pm_mod.PrettyMIDI(initial_tempo=float(bpm))
    inst = pm_mod.Instrument(program=0, is_drum=False)
    y = np.asarray(y24, dtype=np.float32).reshape(-1)
    if y.size < 24:
        y = np.pad(y, (0, 24 - y.size))
    root_midi = int(np.clip(root_note + (octave + 1) * 12, 12, 100))
    sec_per_step = (60.0 / float(bpm)) / 4.0
    intervals = (0, 4, 7)
    for s in range(16):
        w = float(y[s])
        if w < step_threshold:
            continue
        voicing = float(y[16 + (s % 8)])
        delta = int(np.clip(np.round((voicing - 0.5) * 4.0), -2, 2))
        base = root_midi + delta
        vel = int(np.clip(round(50 + w * 70), 1, 127))
        for iv in intervals:
            pitch = int(np.clip(base + iv, 12, 127))
            start = s * sec_per_step
            end = start + sec_per_step * 0.95
            inst.notes.append(pm_mod.Note(velocity=vel, pitch=pitch, start=start, end=end))
    pm.instruments.append(inst)
    return pm


def bridge_melody_output_to_pretty_midi(
    y32: np.ndarray,
    root_note: int = 0,
    octave: int = 5,
    bpm: float = 120.0,
    onset_threshold: float = 0.38,
) -> Any:
    """Melody student: onset [0:16], pitch-class bias [16:32] — mirrors GuitarEngine::mergePatternFromML."""
    pm_mod = _require_pretty_midi()
    pm = pm_mod.PrettyMIDI(initial_tempo=float(bpm))
    inst = pm_mod.Instrument(program=0, is_drum=False)
    y = np.asarray(y32, dtype=np.float32).reshape(-1)
    if y.size < 32:
        y = np.pad(y, (0, 32 - y.size))
    base = int(np.clip(root_note + (octave + 1) * 12, 12, 108))
    sec_per_step = (60.0 / float(bpm)) / 4.0
    for s in range(16):
        ons = float(y[s])
        if ons < onset_threshold:
            continue
        pc_bias = float(y[16 + s])
        delta = int(np.clip(np.round((pc_bias - 0.5) * 4.0), -5, 5))
        midi = int(np.clip(base + delta, 12, 127))
        vel = int(np.clip(round(40 + ons * 80), 1, 127))
        start = s * sec_per_step
        end = start + sec_per_step * 0.92
        inst.notes.append(pm_mod.Note(velocity=vel, pitch=midi, start=start, end=end))
    pm.instruments.append(inst)
    return pm


def bridge_student_output_to_pretty_midi(
    instrument: str,
    y: np.ndarray,
    *,
    root_note: int = 0,
    octave: int = 4,
    bpm: float = 120.0,
) -> Any:
    instrument = instrument.lower().strip()
    if instrument == "drums":
        return bridge_drums_output_to_pretty_midi(y, bpm=bpm)
    if instrument == "bass":
        return bridge_bass_output_to_pretty_midi(y, root_note=root_note, octave=octave, bpm=bpm)
    if instrument in ("chords", "piano", "keys"):
        return bridge_chords_output_to_pretty_midi(y, root_note=root_note, octave=octave, bpm=bpm)
    if instrument in ("melody", "guitar"):
        return bridge_melody_output_to_pretty_midi(y, root_note=root_note, octave=octave, bpm=bpm)
    raise ValueError(f"unknown instrument {instrument!r}")
