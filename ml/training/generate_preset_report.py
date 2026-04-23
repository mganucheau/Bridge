#!/usr/bin/env python3
"""Write ml/models/preset_quality_report.md from preset sweep + inference report."""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import numpy as np
import pandas as pd

_SCRIPT_DIR = Path(__file__).resolve().parent
_MODELS = _SCRIPT_DIR.parent / "models"
_REPORT_OUT = _MODELS / "preset_quality_report.md"

from knob_mapping import KNOB_NAMES  # noqa: E402
from personality_presets import PERSONALITY_PRESETS  # noqa: E402
from quality_metrics import METRIC_KEYS, score_preset_sweep  # noqa: E402


def _load_inference_report(path: Path) -> dict | None:
    if not path.is_file():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def _dead_knob_indices(report: dict | None, threshold: float = 0.01) -> dict[str, list[int]]:
    out: dict[str, list[int]] = {}
    if not report:
        return out
    for inst, block in report.get("instruments", {}).items():
        sens = block.get("sensitivity_l2_mean")
        if not isinstance(sens, list):
            continue
        dead = [i for i, v in enumerate(sens) if float(v) < threshold]
        if dead:
            out[inst] = dead
    return out


def _convergence_pairs(df: pd.DataFrame, rel_tol: float = 0.05) -> list[tuple[str, str, str]]:
    warnings: list[tuple[str, str, str]] = []
    if df.empty or "preset" not in df.columns:
        return warnings
    presets = df["preset"].tolist()
    metric_cols = [c for c in METRIC_KEYS if c in df.columns]
    if not metric_cols:
        return warnings
    for i, a in enumerate(presets):
        for b in presets[i + 1 :]:
            row_a = df[df["preset"] == a].iloc[0]
            row_b = df[df["preset"] == b].iloc[0]
            ok = True
            for c in metric_cols:
                va, vb = row_a[c], row_b[c]
                if isinstance(va, float) and math.isnan(va):
                    continue
                if isinstance(vb, float) and math.isnan(vb):
                    continue
                mx = max(abs(float(va)), abs(float(vb)), 1e-9)
                if abs(float(va) - float(vb)) / mx > rel_tol:
                    ok = False
                    break
            if ok:
                warnings.append((a, b, "metrics within 5%"))
    return warnings


def _ascii_heatmap(df: pd.DataFrame, title: str) -> str:
    metric_cols = [c for c in METRIC_KEYS if c in df.columns]
    if df.empty or not metric_cols:
        return f"### {title}\n\n(no data)\n"
    mat = df[metric_cols].to_numpy(dtype=np.float64)
    mat = np.nan_to_num(mat, nan=0.0)
    col_max = np.maximum(mat.max(axis=0), 1e-12)
    norm = mat / col_max
    chars = " .:-=+*#%@"
    lines = [f"### {title}", "", "preset | " + " | ".join(f"{c[:4]}" for c in metric_cols), ""]
    for ri, (_, row) in enumerate(df.iterrows()):
        pr = str(row.get("preset", ri))
        cells = []
        for j in range(len(metric_cols)):
            idx = int(np.clip(norm[ri, j] * (len(chars) - 1), 0, len(chars) - 1))
            cells.append(chars[idx] * 2)
        lines.append(f"{pr[:12]:<12} | " + " | ".join(cells))
    lines.append("")
    lines.append("*Brightness: value / column max for this instrument.*\n")
    return "\n".join(lines)


def _suggest_knob_nudges(
    conv_by_inst: dict[str, list[tuple[str, str, str]]],
    dead: dict[str, list[int]],
    report: dict | None,
) -> list[str]:
    tips: list[str] = []
    if report:
        for inst, pairs in conv_by_inst.items():
            if not pairs:
                continue
            block = report.get("instruments", {}).get(inst)
            if not block:
                continue
            sens = block.get("sensitivity_l2_mean")
            if not isinstance(sens, list) or len(sens) != 10:
                continue
            top = sorted(range(10), key=lambda i: float(sens[i]), reverse=True)[:3]
            kn = ", ".join(KNOB_NAMES[i] for i in top)
            for a, b, _ in pairs:
                tips.append(
                    f"- **{inst}**: presets `{a}` / `{b}` look similar — try separating with {kn}."
                )
    for inst, idxs in dead.items():
        tips.append(
            f"- **{inst}**: sensitivity < 0.01 on "
            f"{', '.join(KNOB_NAMES[i] for i in idxs)} — recalibrate or retrain."
        )
    return list(dict.fromkeys(tips))


def generate_report(
    models_dir: Path | None = None,
    out_path: Path | None = None,
    bpm: float = 120.0,
) -> Path:
    models_dir = models_dir or _MODELS
    out_path = out_path or _REPORT_OUT
    infer_path = models_dir / "inference_report.json"
    report = _load_inference_report(infer_path)
    dead = _dead_knob_indices(report, 0.01)

    sections: list[str] = [
        "# Bridge — preset quality report",
        "",
        f"Models: `{models_dir}`",
        "",
    ]

    all_sweeps: dict[str, pd.DataFrame] = {}
    conv_by_inst: dict[str, list[tuple[str, str, str]]] = {}

    for inst in ("drums", "bass", "chords", "melody"):
        try:
            df = score_preset_sweep(
                str(models_dir),
                PERSONALITY_PRESETS,
                inst,
                bpm=bpm,
            )
            all_sweeps[inst] = df
        except FileNotFoundError:
            sections.append(f"## {inst}\n\n*Skipped — ONNX missing.*\n")
            continue

        pairs = _convergence_pairs(df, 0.05)
        conv_by_inst[inst] = pairs
        if pairs:
            sections.append(f"## {inst} — convergence warnings\n")
            for a, b, msg in pairs:
                sections.append(f"- `{a}` vs `{b}`: {msg}")
            sections.append("")

        sections.append(_ascii_heatmap(df, f"{inst} heatmap"))

    if dead:
        sections.append("## Dead knobs (inference sensitivity < 0.01)\n")
        for inst, idxs in dead.items():
            sections.append(
                f"- **{inst}**: {', '.join(KNOB_NAMES[i] for i in idxs)} (indices {idxs})"
            )
        sections.append("")

    tips = _suggest_knob_nudges(conv_by_inst, dead, report)
    if tips:
        sections.append("## Suggested adjustments\n")
        sections.extend(tips)
        sections.append("")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(sections), encoding="utf-8")
    return out_path


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--models-dir", type=Path, default=_MODELS)
    ap.add_argument("--out", type=Path, default=_REPORT_OUT)
    ap.add_argument("--bpm", type=float, default=120.0)
    args = ap.parse_args()
    p = generate_report(args.models_dir, args.out, args.bpm)
    print("Wrote", p)


if __name__ == "__main__":
    main()
