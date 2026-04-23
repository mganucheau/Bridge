# === Magenta teachers + distillation batches (training-time only) ===
# Never imported by the C++ plugin. Intended for Colab / local venv with Magenta installed.
from __future__ import annotations

import os
import tarfile
import urllib.request
import warnings
from typing import Any

import numpy as np

from knob_mapping import KnobToLatentMapper, NUM_KNOBS

try:
    import tensorflow as tf  # noqa: F401
except Exception:  # pragma: no cover
    tf = None

try:
    from note_seq.protobuf import music_pb2  # noqa: F401
except Exception:  # pragma: no cover
    music_pb2 = None


# Pre-trained MusicVAE / GrooVAE checkpoints (Google Cloud Storage, public).
CHECKPOINT_URLS: dict[str, str] = {
    "groovae_4bar": "https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/groovae_4bar.tar",
    "cat-mel_2bar_big": "https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/cat-mel_2bar_big.tar",
    "cat-mel_2bar_med_chords": "https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/cat-mel_2bar_med_chords.tar",
    "cat-mel_2bar_small": "https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/cat-mel_2bar_small.tar",
}

# Config keys in magenta.models.music_vae.configs.CONFIG_MAP
CONFIG_KEYS = {
    "groovae_4bar": "groovae_4bar",
    "cat-mel_2bar_big": "cat-mel_2bar_big",
    "cat-mel_2bar_med_chords": "cat-mel_2bar_med_chords",
    "cat-mel_2bar_small": "cat-mel_2bar_small",
}


def knobs_delta_to_z(delta_512: np.ndarray, z_size: int) -> np.ndarray:
    """Map 512-D personality offset to teacher latent dim (blend halves, tanh-bound)."""
    d = np.asarray(delta_512, dtype=np.float32).reshape(-1)
    if d.size < z_size:
        z = np.zeros(z_size, dtype=np.float32)
        z[: d.size] = d
        return np.tanh(z)
    a = d[:z_size]
    b = d[z_size : z_size * 2] if d.size >= z_size * 2 else d[-z_size:]
    return np.tanh(0.5 * (a + b)).astype(np.float32)


def download_and_extract_checkpoint(key: str, cache_dir: str) -> str | None:
    """Download .tar if needed, extract, return directory containing model checkpoint."""
    if key not in CHECKPOINT_URLS:
        warnings.warn(f"Unknown checkpoint key {key!r}")
        return None
    url = CHECKPOINT_URLS[key]
    os.makedirs(cache_dir, exist_ok=True)
    tar_name = url.rsplit("/", 1)[-1]
    tar_path = os.path.join(cache_dir, tar_name)
    if not os.path.isfile(tar_path):
        try:
            urllib.request.urlretrieve(url, tar_path)
        except Exception as e:  # pragma: no cover
            warnings.warn(f"Failed to download {url}: {e}")
            return None
    extract_root = os.path.join(cache_dir, tar_name.replace(".tar", "_extracted"))
    if not os.path.isdir(extract_root):
        os.makedirs(extract_root, exist_ok=True)
        try:
            with tarfile.open(tar_path, "r:*") as tar:
                tar.extractall(extract_root)
        except Exception as e:  # pragma: no cover
            warnings.warn(f"Failed to extract {tar_path}: {e}")
            return None
    # Deepest directory that looks like a TensorFlow checkpoint
    best = None
    best_depth = -1
    for root, _dirs, files in os.walk(extract_root):
        has_ckpt = any(
            f.startswith("model.ckpt") or f.startswith("ckpt") or f == "checkpoint"
            for f in files
        )
        if has_ckpt:
            depth = root.count(os.sep)
            if depth > best_depth:
                best_depth = depth
                best = root
    return best or extract_root


def _try_create_trained_model(config_name: str, checkpoint_dir: str) -> Any:
    from magenta.models.music_vae import configs
    from magenta.models.music_vae.trained_model import TrainedModel

    cfg = configs.CONFIG_MAP[config_name]
    return TrainedModel(cfg, batch_size=4, checkpoint_dir_or_path=checkpoint_dir)


def _decode_ns_list(model: Any, z: np.ndarray, length: int, temperature: float) -> list:
    z = np.asarray(z, dtype=np.float32)
    if z.ndim == 1:
        z = z.reshape(1, -1)
    return model.decode(z, length=length, temperature=temperature)


def _shift_pitched_notes_down(ns: Any, semitones: int = 24, low: int = 28, high: int = 76) -> Any:
    from copy import deepcopy

    out = deepcopy(ns)
    for n in out.notes:
        if not n.is_drum:
            n.pitch = int(np.clip(n.pitch - semitones, low, high))
    return out


class _MagentaTeacherBase:
    """Shared KnobToLatentMapper; subclass per instrument for decode hooks."""

    def __init__(
        self,
        mapper_seed: int = 42,
        mapper: KnobToLatentMapper | None = None,
    ):
        self.mapper = mapper if mapper is not None else KnobToLatentMapper(seed=mapper_seed)

    def knobs_to_latent(self, knobs: np.ndarray) -> np.ndarray:
        return self.mapper.knobs_to_latent_offset(knobs)


class MagentaDrumsTeacher(_MagentaTeacherBase):
    """GrooVAE 4-bar → drum hits (32,) via note_sequence_to_drum_hits."""

    def __init__(
        self,
        checkpoint_path: str | None = None,
        cache_dir: str | None = None,
        mapper_seed: int = 42,
        mapper: KnobToLatentMapper | None = None,
    ):
        super().__init__(mapper_seed, mapper)
        self._model = None
        self._z_size = 256
        self._decode_length = 64
        self._config_name = CONFIG_KEYS["groovae_4bar"]
        cache_dir = cache_dir or os.path.join(os.path.expanduser("~"), ".cache", "bridge_magenta")
        ckpt_dir = checkpoint_path
        if ckpt_dir is None:
            ckpt_dir = download_and_extract_checkpoint("groovae_4bar", cache_dir)
        if ckpt_dir and tf is not None:
            try:
                self._model = _try_create_trained_model(self._config_name, ckpt_dir)
                cfg = getattr(self._model, "_config", None)
                if cfg is not None:
                    self._z_size = int(cfg.hparams.z_size)
                    self._decode_length = int(cfg.hparams.max_seq_len)
            except Exception as e:  # pragma: no cover
                warnings.warn(f"MagentaDrumsTeacher: could not load GrooVAE ({e}); using fallback.")
                self._model = None
        else:
            warnings.warn(
                "MagentaDrumsTeacher: no checkpoint or TensorFlow unavailable; "
                "using randomized groove fallback (install magenta + download checkpoint for real distillation)."
            )

    def generate_hits(
        self,
        personality_knobs: np.ndarray,
        prior_hits: np.ndarray,
        groove_scalars: np.ndarray,
    ) -> np.ndarray:
        from magenta_data_utils import note_sequence_to_drum_hits

        knobs = np.clip(np.asarray(personality_knobs, dtype=np.float32).reshape(NUM_KNOBS), 0, 1)
        if self._model is not None:
            try:
                delta = self.knobs_to_latent(knobs)
                z = knobs_delta_to_z(delta, self._z_size).reshape(1, -1)
                z = z + 0.12 * np.random.randn(*z.shape).astype(np.float32)
                seqs = _decode_ns_list(
                    self._model,
                    z,
                    length=self._decode_length,
                    temperature=float(0.85 + 0.25 * knobs[4]),
                )
                if seqs:
                    return note_sequence_to_drum_hits(seqs[0])
            except Exception as e:  # pragma: no cover
                warnings.warn(f"GrooVAE decode failed ({e}); using latent fallback.")
        return self._fallback_hits(knobs, prior_hits, groove_scalars)

    def _fallback_hits(
        self, knobs: np.ndarray, prior: np.ndarray, groove: np.ndarray
    ) -> np.ndarray:
        rng = np.random.default_rng(int(np.sum(knobs * 1e5) % (2**31)))
        p = prior.reshape(8, 4)
        g = groove.reshape(1, 4)
        noise = rng.uniform(0.0, 1.0, (8, 4)).astype(np.float32)
        y = p * (0.45 + 0.35 * knobs[0]) + noise * (0.25 + 0.45 * knobs[1])
        y = y + g * 0.12
        return np.clip(y.reshape(-1), 0.0, 1.0)


class MagentaBassTeacher(_MagentaTeacherBase):
    """MusicVAE mel_2bar_big: decode melody-shaped sequence, shift to bass register → 64 target."""

    def __init__(
        self,
        checkpoint_path: str | None = None,
        cache_dir: str | None = None,
        mapper_seed: int = 42,
        mapper: KnobToLatentMapper | None = None,
    ):
        super().__init__(mapper_seed, mapper)
        self._model = None
        self._z_size = 512
        self._decode_length = 32
        self._config_name = CONFIG_KEYS["cat-mel_2bar_big"]
        cache_dir = cache_dir or os.path.join(os.path.expanduser("~"), ".cache", "bridge_magenta")
        ckpt_dir = checkpoint_path
        if ckpt_dir is None:
            ckpt_dir = download_and_extract_checkpoint("cat-mel_2bar_big", cache_dir)
        if ckpt_dir and tf is not None:
            try:
                self._model = _try_create_trained_model(self._config_name, ckpt_dir)
                cfg = getattr(self._model, "_config", None)
                if cfg is not None:
                    self._z_size = int(cfg.hparams.z_size)
                    self._decode_length = int(cfg.hparams.max_seq_len)
            except Exception as e:  # pragma: no cover
                warnings.warn(f"MagentaBassTeacher: could not load MusicVAE ({e}); using fallback.")
                self._model = None
        else:
            warnings.warn(
                "MagentaBassTeacher: no checkpoint or TensorFlow unavailable; using fallback targets."
            )

    def generate_events(
        self,
        personality_knobs: np.ndarray,
        kick_pattern: np.ndarray,
        tonality: np.ndarray,
        bass_context: np.ndarray,
    ) -> np.ndarray:
        from bass_onnx_student import synthetic_target_from_input, pack_bridge_input
        from magenta_data_utils import bass_ns_to_bridge_target

        knobs = np.clip(np.asarray(personality_knobs, dtype=np.float32).reshape(NUM_KNOBS), 0, 1)
        kick = np.clip(np.asarray(kick_pattern, dtype=np.float32).reshape(16), 0, 1)
        tonal = np.asarray(tonality, dtype=np.float32).reshape(3)
        root = int(np.clip(np.round(tonal[0] * 11), 0, 11))
        sc = int(np.clip(np.round(tonal[1] * 9), 0, 9))
        st = int(np.clip(np.round(tonal[2] * 7), 0, 7))
        ctx = np.asarray(bass_context, dtype=np.float32).reshape(-1)
        if self._model is not None:
            try:
                delta = self.knobs_to_latent(knobs)
                z = knobs_delta_to_z(delta, self._z_size).reshape(1, -1)
                z = z + 0.1 * np.random.randn(*z.shape).astype(np.float32)
                seqs = _decode_ns_list(
                    self._model, z, length=self._decode_length, temperature=float(0.9 + 0.1 * knobs[2])
                )
                if seqs:
                    ns_bass = _shift_pitched_notes_down(seqs[0], semitones=24)
                    return bass_ns_to_bridge_target(ns_bass)
            except Exception as e:  # pragma: no cover
                warnings.warn(f"MusicVAE bass decode failed ({e}); using synthetic_target_from_input.")
        x = pack_bridge_input(knobs, kick, root, sc, st, ctx)
        return synthetic_target_from_input(x)


class MagentaChordsTeacher(_MagentaTeacherBase):
    """Chord-conditioned MusicVAE → 24 voicing weights."""

    def __init__(
        self,
        checkpoint_path: str | None = None,
        cache_dir: str | None = None,
        mapper_seed: int = 42,
        mapper: KnobToLatentMapper | None = None,
    ):
        super().__init__(mapper_seed, mapper)
        self._model = None
        self._z_size = 256
        self._decode_length = 32
        self._config_name = CONFIG_KEYS["cat-mel_2bar_med_chords"]
        cache_dir = cache_dir or os.path.join(os.path.expanduser("~"), ".cache", "bridge_magenta")
        ckpt_dir = checkpoint_path
        if ckpt_dir is None:
            ckpt_dir = download_and_extract_checkpoint("cat-mel_2bar_med_chords", cache_dir)
        if ckpt_dir and tf is not None:
            try:
                self._model = _try_create_trained_model(self._config_name, ckpt_dir)
                cfg = getattr(self._model, "_config", None)
                if cfg is not None:
                    self._z_size = int(cfg.hparams.z_size)
                    self._decode_length = int(cfg.hparams.max_seq_len)
            except Exception as e:  # pragma: no cover
                warnings.warn(f"MagentaChordsTeacher: could not load model ({e}); using fallback.")
                self._model = None
        else:
            warnings.warn(
                "MagentaChordsTeacher: no checkpoint or TensorFlow unavailable; using fallback targets."
            )

    def generate_voicing(
        self,
        personality_knobs: np.ndarray,
        bass_pattern16: np.ndarray | None = None,
    ) -> np.ndarray:
        from chords_onnx_student import synthetic_chords_target, pack_chords_input
        from magenta_data_utils import chords_ns_to_bridge_target

        knobs = np.clip(np.asarray(personality_knobs, dtype=np.float32).reshape(NUM_KNOBS), 0, 1)
        bass16 = (
            np.clip(np.asarray(bass_pattern16, dtype=np.float32).reshape(16), 0.0, 1.0)
            if bass_pattern16 is not None
            else np.random.rand(16).astype(np.float32)
        )
        if self._model is not None:
            try:
                delta = self.knobs_to_latent(knobs)
                z = knobs_delta_to_z(delta, self._z_size).reshape(1, -1)
                z = z + 0.12 * np.random.randn(*z.shape).astype(np.float32)
                z = z + 0.08 * float(np.mean(bass16)) * np.random.randn(*z.shape).astype(np.float32)
                seqs = _decode_ns_list(self._model, z, length=self._decode_length, temperature=0.92)
                if seqs:
                    w = chords_ns_to_bridge_target(seqs[0])
                    w = np.clip(w * (0.72 + 0.28 * float(np.mean(bass16))), 0.0, 1.0)
                    return np.clip(w, 0.0, 1.0)
            except Exception as e:  # pragma: no cover
                warnings.warn(f"Chord MusicVAE decode failed ({e}); using synthetic target.")
        x = pack_chords_input(
            knobs,
            int(np.random.randint(0, 12)),
            int(np.random.randint(0, 10)),
            int(np.random.randint(0, 8)),
            np.random.rand(8).astype(np.float32),
            bass16,
        )
        return synthetic_chords_target(x)


class MagentaMelodyTeacher(_MagentaTeacherBase):
    """
    MusicVAE cat-mel_2bar_small (Python counterpart to TFJS mel_4bar_small_q2 class).
    Produces 32-float onset + pitch-class targets.
    """

    def __init__(
        self,
        checkpoint_path: str | None = None,
        cache_dir: str | None = None,
        mapper_seed: int = 42,
        mapper: KnobToLatentMapper | None = None,
    ):
        super().__init__(mapper_seed, mapper)
        self._model = None
        self._z_size = 256
        self._decode_length = 32
        self._config_name = CONFIG_KEYS["cat-mel_2bar_small"]
        cache_dir = cache_dir or os.path.join(os.path.expanduser("~"), ".cache", "bridge_magenta")
        ckpt_dir = checkpoint_path
        if ckpt_dir is None:
            ckpt_dir = download_and_extract_checkpoint("cat-mel_2bar_small", cache_dir)
        if ckpt_dir and tf is not None:
            try:
                self._model = _try_create_trained_model(self._config_name, ckpt_dir)
                cfg = getattr(self._model, "_config", None)
                if cfg is not None:
                    self._z_size = int(cfg.hparams.z_size)
                    self._decode_length = int(cfg.hparams.max_seq_len)
            except Exception as e:  # pragma: no cover
                warnings.warn(f"MagentaMelodyTeacher: could not load model ({e}); using fallback.")
                self._model = None
        else:
            warnings.warn(
                "MagentaMelodyTeacher: no checkpoint or TensorFlow unavailable; using fallback targets."
            )

    def generate_melody_probs(
        self,
        personality_knobs: np.ndarray,
        prev_note_context16: np.ndarray | None = None,
        rhythmic_grid16: np.ndarray | None = None,
    ) -> np.ndarray:
        from melody_onnx_student import synthetic_melody_target, pack_melody_input
        from magenta_data_utils import melody_ns_to_bridge_target

        knobs = np.clip(np.asarray(personality_knobs, dtype=np.float32).reshape(NUM_KNOBS), 0, 1)
        prev = (
            np.asarray(prev_note_context16, dtype=np.float32).reshape(16)
            if prev_note_context16 is not None
            else np.random.rand(16).astype(np.float32)
        )
        grid = (
            np.clip(np.asarray(rhythmic_grid16, dtype=np.float32).reshape(16), 0.0, 1.0)
            if rhythmic_grid16 is not None
            else np.random.rand(16).astype(np.float32)
        )
        if self._model is not None:
            try:
                delta = self.knobs_to_latent(knobs)
                z = knobs_delta_to_z(delta, self._z_size).reshape(1, -1)
                z = z + 0.1 * np.random.randn(*z.shape).astype(np.float32)
                z = z + 0.05 * float(np.mean(prev)) * np.random.randn(*z.shape).astype(np.float32)
                temp = float(0.88 + 0.12 * knobs[5] + 0.08 * float(np.mean(grid)))
                seqs = _decode_ns_list(self._model, z, length=self._decode_length, temperature=temp)
                if seqs:
                    y = melody_ns_to_bridge_target(seqs[0])
                    y = np.clip(y * (0.75 + 0.25 * float(np.mean(grid))), 0.0, 1.0)
                    return np.clip(y, 0.0, 1.0)
            except Exception as e:  # pragma: no cover
                warnings.warn(f"Melody MusicVAE decode failed ({e}); using synthetic target.")
        x = pack_melody_input(
            knobs,
            int(np.random.randint(0, 12)),
            int(np.random.randint(0, 10)),
            int(np.random.randint(0, 8)),
            prev,
            grid,
        )
        return synthetic_melody_target(x)


# --- Distillation batches (input tensors for Bridge students) ---


def distill_batch_drums(
    teacher: MagentaDrumsTeacher,
    n_samples: int = 500,
    seed: int = 0,
    midi_path: str | None = None,
    mapper_path: str | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    from drum_onnx_student import pack_drum_input
    from magenta_data_utils import (
        build_groove_scalars,
        ensure_simple_groove_fixture,
        load_midi_to_note_sequence,
        note_sequence_to_drum_hits,
    )

    if mapper_path:
        m = KnobToLatentMapper(seed=42)
        m.load_calibrated(mapper_path)
        teacher.mapper = m

    rng = np.random.default_rng(seed)
    path = midi_path or ensure_simple_groove_fixture()
    ns = load_midi_to_note_sequence(path)
    prior = note_sequence_to_drum_hits(ns)
    groove = build_groove_scalars(ns)
    xs, ys = [], []
    for _ in range(n_samples):
        knobs = rng.random(NUM_KNOBS, dtype=np.float32)
        y = teacher.generate_hits(knobs, prior, groove)
        x = pack_drum_input(knobs, prior, groove)
        xs.append(x)
        ys.append(np.clip(np.asarray(y, dtype=np.float32).reshape(32), 0.0, 1.0))
    return np.stack(xs), np.stack(ys)


def distill_batch_bass(
    teacher: MagentaBassTeacher,
    n_samples: int = 500,
    seed: int = 0,
    midi_path: str | None = None,
    mapper_path: str | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    from bass_onnx_student import pack_bridge_input
    from magenta_data_utils import (
        ensure_simple_groove_fixture,
        load_midi_to_note_sequence,
        note_sequence_to_bass_events,
    )

    if mapper_path:
        m = KnobToLatentMapper(seed=42)
        m.load_calibrated(mapper_path)
        teacher.mapper = m

    rng = np.random.default_rng(seed)
    path = midi_path or ensure_simple_groove_fixture()
    ns = load_midi_to_note_sequence(path)
    ctx_seed = note_sequence_to_bass_events(ns)
    xs, ys = [], []
    for i in range(n_samples):
        knobs = rng.random(NUM_KNOBS, dtype=np.float32)
        kick = rng.random(16, dtype=np.float32)
        if i % 3 == 0:
            kick[:4] = rng.random(4) * 0.5
        root = int(rng.integers(0, 12))
        sc = int(rng.integers(0, 10))
        st = int(rng.integers(0, 8))
        tonal = np.array([root / 11.0, sc / 9.0, st / 7.0], dtype=np.float32)
        ctx = ctx_seed + 0.08 * rng.standard_normal(64).astype(np.float32)
        ctx = np.clip(ctx, 0.0, 1.0)
        y = teacher.generate_events(knobs, kick, tonal, ctx)
        x = pack_bridge_input(knobs, kick, root, sc, st, ctx)
        xs.append(x)
        ys.append(np.asarray(y, dtype=np.float32).reshape(64))
    return np.stack(xs), np.stack(ys)


def distill_batch_chords(
    teacher: MagentaChordsTeacher,
    n_samples: int = 500,
    seed: int = 0,
    midi_path: str | None = None,
    mapper_path: str | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    from chords_onnx_student import pack_chords_input
    from magenta_data_utils import (
        ensure_simple_groove_fixture,
        load_midi_to_note_sequence,
        note_sequence_to_chord_context,
    )

    if mapper_path:
        m = KnobToLatentMapper(seed=42)
        m.load_calibrated(mapper_path)
        teacher.mapper = m

    rng = np.random.default_rng(seed)
    path = midi_path or ensure_simple_groove_fixture()
    ns = load_midi_to_note_sequence(path)
    cc_seed = note_sequence_to_chord_context(ns)
    xs, ys = [], []
    for _ in range(n_samples):
        knobs = rng.random(NUM_KNOBS, dtype=np.float32)
        root = int(rng.integers(0, 12))
        sc = int(rng.integers(0, 10))
        st = int(rng.integers(0, 8))
        cc = np.clip(cc_seed + 0.05 * rng.standard_normal(8).astype(np.float32), 0.0, 1.0)
        bass16 = rng.random(16, dtype=np.float32)
        x = pack_chords_input(knobs, root, sc, st, cc, bass16)
        y = teacher.generate_voicing(knobs, bass16)
        xs.append(x)
        ys.append(np.clip(np.asarray(y, dtype=np.float32).reshape(24), 0.0, 1.0))
    return np.stack(xs), np.stack(ys)


def distill_batch_melody(
    teacher: MagentaMelodyTeacher,
    n_samples: int = 500,
    seed: int = 0,
    midi_path: str | None = None,
    mapper_path: str | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    from melody_onnx_student import pack_melody_input
    from magenta_data_utils import (
        ensure_simple_groove_fixture,
        load_midi_to_note_sequence,
        note_sequence_to_melody_context,
    )

    if mapper_path:
        m = KnobToLatentMapper(seed=42)
        m.load_calibrated(mapper_path)
        teacher.mapper = m

    rng = np.random.default_rng(seed)
    path = midi_path or ensure_simple_groove_fixture()
    ns = load_midi_to_note_sequence(path)
    mel_ctx = note_sequence_to_melody_context(ns)
    xs, ys = [], []
    for _ in range(n_samples):
        knobs = rng.random(NUM_KNOBS, dtype=np.float32)
        root = int(rng.integers(0, 12))
        sc = int(rng.integers(0, 10))
        st = int(rng.integers(0, 8))
        grid = rng.random(16, dtype=np.float32) * (0.5 + 0.5 * knobs[3])
        prev = mel_ctx + 0.06 * rng.standard_normal(16).astype(np.float32)
        prev = np.clip(prev, 0.0, 1.0)
        x = pack_melody_input(knobs, root, sc, st, prev, grid)
        y = teacher.generate_melody_probs(knobs, prev, grid)
        xs.append(x)
        ys.append(np.clip(np.asarray(y, dtype=np.float32).reshape(32), 0.0, 1.0))
    return np.stack(xs), np.stack(ys)


def distill_batch_stub(
    knobs_batch: np.ndarray,
    kick_batch: np.ndarray,
    context_batch: np.ndarray,
    root_scale_style: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    """Legacy bass stub (synthetic targets). Prefer distill_batch_bass + MagentaBassTeacher."""
    from bass_onnx_student import pack_bridge_input, synthetic_target_from_input

    b = knobs_batch.shape[0]
    xs, ys = [], []
    for i in range(b):
        r, sc, st = root_scale_style[i]
        x = pack_bridge_input(
            knobs_batch[i],
            kick_batch[i],
            int(r),
            int(sc),
            int(st),
            context_batch[i],
        )
        xs.append(x)
        ys.append(synthetic_target_from_input(x))
    return np.stack(xs), np.stack(ys)
