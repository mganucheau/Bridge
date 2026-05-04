# === Small dataset helpers for Colab training ===
from __future__ import annotations

import random
from typing import Any, List, Tuple

import numpy as np


def normalize_offset(offset_ms: float, max_ms: float = 30.0) -> float:
    """Clamp humanization offset to [-1, 1] using `max_ms` as full scale."""
    max_ms = max(max_ms, 1e-6)
    x = float(offset_ms) / max_ms
    return float(np.clip(x, -1.0, 1.0))


def denormalize_offset(value: float, max_ms: float = 30.0) -> float:
    """Map [-1, 1] back to milliseconds."""
    return float(value) * max_ms


def train_val_split(
    dataset: List[Any], val_fraction: float = 0.2, seed: int = 42
) -> Tuple[List[Any], List[Any]]:
    """Shuffle-copy split for list-backed datasets."""
    rng = random.Random(seed)
    items = list(dataset)
    rng.shuffle(items)
    n_val = int(round(len(items) * val_fraction))
    n_val = max(0, min(len(items), n_val))
    val = items[:n_val]
    train = items[n_val:]
    if not train and items:
        train = val
        val = []
    return train, val


def plot_loss(train_losses: List[float], val_losses: List[float], title: str = "Training") -> None:
    """Plot training / validation curves (requires matplotlib)."""
    import matplotlib.pyplot as plt

    plt.figure(figsize=(8, 4))
    plt.plot(train_losses, label="train")
    if val_losses:
        plt.plot(val_losses, label="val")
    plt.title(title)
    plt.xlabel("epoch")
    plt.ylabel("loss")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.show()
