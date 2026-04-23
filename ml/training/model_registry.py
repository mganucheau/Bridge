# === Versioned ONNX artifacts under ml/models (no TF) ===
from __future__ import annotations

import argparse
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
MODELS_ROOT = SCRIPT_DIR.parent / "models"
REGISTRY_NAME = "registry.json"

ONNX_NAMES = (
    "drum_humanizer.onnx",
    "bass_model.onnx",
    "chords_model.onnx",
    "melody_model.onnx",
)

OPTIONAL_ARTIFACTS = (
    "inference_report.json",
    "quality_scores.json",
    "knob_mapper_calibrated.npz",
)


def _registry_path(root: Path | None = None) -> Path:
    return (root or MODELS_ROOT) / REGISTRY_NAME


def _load_registry(root: Path) -> dict[str, Any]:
    p = _registry_path(root)
    if not p.is_file():
        return {"versions": {}, "active": None}
    return json.loads(p.read_text(encoding="utf-8"))


def _save_registry(root: Path, data: dict[str, Any]) -> None:
    p = _registry_path(root)
    p.write_text(json.dumps(data, indent=2), encoding="utf-8")


def register_model_set(
    version_tag: str,
    source_dir: str | Path | None = None,
    *,
    root: Path | None = None,
) -> Path:
    """
    Copy ml/models/*.onnx (+ optional reports) into ml/models/{version_tag}/ and update registry.json.
    """
    models_root = root or MODELS_ROOT
    src = Path(source_dir) if source_dir else models_root
    version_tag = version_tag.strip().replace("/", "_")
    if not version_tag:
        raise ValueError("version_tag required")

    dest = models_root / version_tag
    dest.mkdir(parents=True, exist_ok=True)

    copied = []
    for name in ONNX_NAMES:
        sf = src / name
        if sf.is_file():
            shutil.copy2(sf, dest / name)
            copied.append(name)

    for name in OPTIONAL_ARTIFACTS:
        sf = src / name
        if sf.is_file():
            shutil.copy2(sf, dest / name)
            copied.append(name)

    reg = _load_registry(models_root)
    reg.setdefault("versions", {})
    reg["versions"][version_tag] = {
        "path": version_tag,
        "created": datetime.now(timezone.utc).isoformat(),
        "artifacts": copied,
    }
    _save_registry(models_root, reg)

    return dest


def activate_version(version_tag: str, root: Path | None = None) -> None:
    """Copy versioned folder into ml/models/current/ and root-level .onnx (plugin expects flat ml/models)."""
    base = Path(root) if root is not None else MODELS_ROOT
    version_tag = version_tag.strip()
    vdir = base / version_tag
    if not vdir.is_dir():
        raise FileNotFoundError(vdir)

    cur = base / "current"
    if cur.is_dir():
        shutil.rmtree(cur)
    shutil.copytree(vdir, cur, dirs_exist_ok=True)

    for name in ONNX_NAMES + list(OPTIONAL_ARTIFACTS):
        sf = vdir / name
        if sf.is_file():
            shutil.copy2(sf, base / name)

    reg = _load_registry(base)
    reg["active"] = version_tag
    _save_registry(base, reg)


def list_versions(root: Path | None = None) -> None:
    base = root or MODELS_ROOT
    reg = _load_registry(base)
    versions = reg.get("versions", {})
    active = reg.get("active")
    print(f"{'TAG':<12} {'CREATED':<28} {'ACTIVE':<8} ARTIFACTS")
    for tag, meta in sorted(versions.items()):
        cr = str(meta.get("created", ""))[:26]
        is_act = "*" if tag == active else ""
        arts = ",".join(meta.get("artifacts", [])[:4])
        if len(meta.get("artifacts", [])) > 4:
            arts += ",..."
        print(f"{tag:<12} {cr:<28} {is_act:<8} {arts}")


def _read_json(p: Path) -> Any:
    if not p.is_file():
        return None
    return json.loads(p.read_text(encoding="utf-8"))


def compare_versions(tag_a: str, tag_b: str, root: Path | None = None) -> dict[str, Any]:
    base = root or MODELS_ROOT
    out: dict[str, Any] = {"a": tag_a, "b": tag_b}

    for label, tag in (("scores_a", tag_a), ("scores_b", tag_b)):
        d = base / tag
        out[label] = _read_json(d / "quality_scores.json")

    for label, tag in (("infer_a", tag_a), ("infer_b", tag_b)):
        d = base / tag
        out[label] = _read_json(d / "inference_report.json")

    # Shallow diff: mean sensitivity per instrument if present
    def _mean_sens(rep: Any) -> dict[str, float] | None:
        if not rep or "instruments" not in rep:
            return None
        m: dict[str, float] = {}
        for inst, block in rep["instruments"].items():
            row = block.get("sensitivity_l2_mean")
            if isinstance(row, list) and row:
                m[inst] = float(sum(float(x) for x in row) / len(row))
        return m or None

    ia, ib = out.get("infer_a"), out.get("infer_b")
    ma, mb = _mean_sens(ia), _mean_sens(ib)
    if ma and mb:
        out["mean_sensitivity_delta"] = {
            k: mb.get(k, 0) - ma.get(k, 0) for k in sorted(set(ma) | set(mb))
        }

    return out


def main() -> None:
    ap = argparse.ArgumentParser(description="Bridge ML model registry")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_reg = sub.add_parser("register", help="Snapshot models into ml/models/<tag>/")
    p_reg.add_argument("version_tag")
    p_reg.add_argument("--source", type=Path, default=None)
    p_reg.add_argument("--root", type=Path, default=MODELS_ROOT)

    p_act = sub.add_parser("activate", help="Promote a version to ml/models/current + flat copies")
    p_act.add_argument("version_tag")
    p_act.add_argument("--root", type=Path, default=MODELS_ROOT)

    p_list = sub.add_parser("list", help="Print registry table")
    p_list.add_argument("--root", type=Path, default=MODELS_ROOT)

    p_cmp = sub.add_parser("compare", help="Diff quality / inference JSON between versions")
    p_cmp.add_argument("tag_a")
    p_cmp.add_argument("tag_b")
    p_cmp.add_argument("--root", type=Path, default=MODELS_ROOT)

    args = ap.parse_args()

    if args.cmd == "register":
        src = args.source if args.source is not None else args.root
        dest = register_model_set(args.version_tag, src, root=args.root)
        print("Registered", args.version_tag, "→", dest)
    elif args.cmd == "activate":
        activate_version(args.version_tag, root)
        print("Active:", args.version_tag)
    elif args.cmd == "list":
        list_versions(args.root)
    elif args.cmd == "compare":
        d = compare_versions(args.tag_a, args.tag_b, args.root)
        print(json.dumps(d, indent=2, default=str))


if __name__ == "__main__":
    main()
