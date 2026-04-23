# === Personality knobs → MusicVAE latent offsets (512-D) ===
# Training-time only. Plugin runtime uses the 10 floats directly for ONNX conditioning.
from __future__ import annotations

import json
from pathlib import Path

import numpy as np

LATENT_DIM = 512
NUM_KNOBS = 10

# Order matches Bridge plugin APVTS / UI (0 = low, 1 = high on sliders).
KNOB_NAMES: tuple[str, ...] = (
    "Rhythmic Tightness",
    "Dynamic Range",
    "Timbre Texture",
    "Tension Arc",
    "Tempo Volatility",
    "Emotional Temperature",
    "Harmonic Adventurousness",
    "Structural Predictability",
    "Showmanship",
    "Genre Loyalty",
)


def _orthonormal_directions(rng: np.random.Generator, dim: int, n: int) -> np.ndarray:
    """n independent unit directions in R^dim (Gram-Schmidt on random Gaussian)."""
    raw = rng.standard_normal(size=(n, dim)).astype(np.float64)
    basis = np.zeros_like(raw)
    for i in range(n):
        v = raw[i].copy()
        for j in range(i):
            v -= np.dot(v, basis[j]) * basis[j]
        nrm = np.linalg.norm(v)
        if nrm < 1e-9:
            v = raw[i]
            nrm = np.linalg.norm(v)
        basis[i] = v / max(nrm, 1e-9)
    return basis.astype(np.float32)


# Per-knob strength in latent space (how far a full sweep moves along its axis).
DEFAULT_KNOB_SCALES = np.array(
    [0.35, 0.45, 0.40, 0.50, 0.30, 0.55, 0.45, 0.40, 0.50, 0.35], dtype=np.float32
)


class KnobToLatentMapper:
    """
    Maps knob vector k in [0,1]^10 to a delta z in R^512:
      delta = sum_i (k[i] - 0.5) * scale[i] * direction[i]

    Use with MusicVAE by adding delta to a base latent z0 before decode, or by
    biasing the encoder prior during training.
    """

    def __init__(
        self,
        seed: int = 42,
        scales: np.ndarray | None = None,
        directions: np.ndarray | None = None,
    ):
        self.scales = (
            np.asarray(scales, dtype=np.float32)
            if scales is not None
            else DEFAULT_KNOB_SCALES.copy()
        )
        if self.scales.shape != (NUM_KNOBS,):
            raise ValueError("scales must be shape (10,)")

        if directions is not None:
            d = np.asarray(directions, dtype=np.float32)
            if d.shape != (NUM_KNOBS, LATENT_DIM):
                raise ValueError("directions must be (10, 512)")
            self._dirs = d
        else:
            rng = np.random.default_rng(seed)
            self._dirs = _orthonormal_directions(rng, LATENT_DIM, NUM_KNOBS)

    @property
    def directions(self) -> np.ndarray:
        return self._dirs

    def knobs_to_latent_offset(self, knobs: np.ndarray) -> np.ndarray:
        """knobs: shape (10,) in [0,1]. Returns (512,) float32."""
        k = np.asarray(knobs, dtype=np.float32).reshape(-1)
        if k.size != NUM_KNOBS:
            raise ValueError(f"expected {NUM_KNOBS} knobs, got {k.size}")
        k = np.clip(k, 0.0, 1.0)
        centered = k - 0.5
        delta = np.sum(
            (centered[:, np.newaxis] * self.scales[:, np.newaxis] * self._dirs), axis=0
        )
        return delta.astype(np.float32)

    def batch_offsets(self, knobs_batch: np.ndarray) -> np.ndarray:
        """knobs_batch: (B, 10) → (B, 512)."""
        b = np.asarray(knobs_batch, dtype=np.float32)
        if b.ndim != 2 or b.shape[1] != NUM_KNOBS:
            raise ValueError("expected shape (B, 10)")
        return np.stack([self.knobs_to_latent_offset(b[i]) for i in range(b.shape[0])])

    def calibrate_from_report(
        self,
        report_path: str,
        sensitivity_key: str = "sensitivity_l2_mean",
        min_factor: float = 0.25,
        max_factor: float = 4.0,
    ) -> None:
        """
        Rescale per-knob latent axes using inverse sensitivity from test_inference.py output.
        Low-sensitivity knobs get larger effective scales; high-sensitivity knobs are damped.
        """
        path = Path(report_path)
        data = json.loads(path.read_text(encoding="utf-8"))
        inst = data.get("instruments", {})
        if not inst:
            raise ValueError(f"no instruments in {report_path!r}")

        sens = np.zeros(NUM_KNOBS, dtype=np.float64)
        n = 0
        for block in inst.values():
            row = np.asarray(block.get(sensitivity_key, []), dtype=np.float64).reshape(-1)
            if row.size == NUM_KNOBS:
                sens += row
                n += 1
        if n == 0:
            raise ValueError(f"no per-instrument {sensitivity_key!r} vectors in report")
        sens /= float(n)
        ref = float(np.mean(sens))
        eps = 1e-8
        factors = ref / (sens + eps)
        factors = np.clip(factors, min_factor, max_factor).astype(np.float32)
        self.scales = (self.scales * factors).astype(np.float32)

    def save_calibrated(self, path: str) -> None:
        """Persist scales + directions for distill_batch_* (NPZ: two arrays)."""
        p = Path(path)
        p.parent.mkdir(parents=True, exist_ok=True)
        np.savez(
            p,
            scales=self.scales.astype(np.float32),
            directions=self._dirs.astype(np.float32),
        )

    def load_calibrated(self, path: str) -> None:
        """Load scales + directions written by save_calibrated."""
        p = Path(path)
        z = np.load(p, allow_pickle=False)
        scales = np.asarray(z["scales"], dtype=np.float32).reshape(NUM_KNOBS)
        dirs = np.asarray(z["directions"], dtype=np.float32).reshape(NUM_KNOBS, LATENT_DIM)
        if scales.shape != (NUM_KNOBS,) or dirs.shape != (NUM_KNOBS, LATENT_DIM):
            raise ValueError("calibrated file has wrong shapes")
        self.scales = scales
        self._dirs = dirs


def knobs_to_latent_offset(knobs: np.ndarray, seed: int = 42) -> np.ndarray:
    """Convenience: stateless mapper with default scales/directions."""
    return KnobToLatentMapper(seed=seed).knobs_to_latent_offset(knobs)
