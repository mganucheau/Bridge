#!/usr/bin/env python3
"""Apply inference_report.json sensitivities to KnobToLatentMapper and save NPZ."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from knob_mapping import KnobToLatentMapper


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--report",
        type=Path,
        default=_SCRIPT_DIR.parent / "models" / "inference_report.json",
    )
    ap.add_argument(
        "--out",
        type=Path,
        default=_SCRIPT_DIR.parent / "models" / "knob_mapper_calibrated.npz",
    )
    ap.add_argument("--seed", type=int, default=42, help="Fallback mapper seed before calibrate.")
    args = ap.parse_args()

    m = KnobToLatentMapper(seed=args.seed)
    m.calibrate_from_report(str(args.report))
    m.save_calibrated(str(args.out))
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
