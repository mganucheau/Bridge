#!/usr/bin/env python3
"""Signed .bridgemodels bundle (zip + manifest + sha256). create_* may use pathlib; verify_* is stdlib-only."""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
import zipfile
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass
class BundleManifest:
    version: str
    bridge_version_min: int
    instruments: list[str]
    preset_names: list[str]
    files: dict[str, str]  # relative name -> sha256 hex


MODEL_NAMES = (
    "drum_humanizer.onnx",
    "bass_model.onnx",
    "chords_model.onnx",
    "melody_model.onnx",
)
PRESETS_JSON = "personality_presets.json"
MANIFEST_NAME = "manifest.json"


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def create_bundle(models_dir: str | Path, version_tag: str, output_path: str | Path) -> Path:
    models_dir = Path(models_dir)
    out = Path(output_path)
    presets_src = Path(__file__).resolve().parent / PRESETS_JSON
    if not presets_src.is_file():
        raise FileNotFoundError(f"Missing {presets_src} (sync with personality_presets.py)")

    files_hash: dict[str, str] = {}
    to_zip: list[tuple[Path, str]] = []

    for name in MODEL_NAMES:
        p = models_dir / name
        if not p.is_file():
            raise FileNotFoundError(f"Required model missing: {p}")
        files_hash[name] = _sha256_file(p)
        to_zip.append((p, name))

    files_hash[PRESETS_JSON] = _sha256_file(presets_src)
    to_zip.append((presets_src, PRESETS_JSON))

    with open(presets_src, encoding="utf-8") as f:
        presets_obj = json.load(f)
    preset_names = list(presets_obj.keys())

    manifest = BundleManifest(
        version=version_tag.strip(),
        bridge_version_min=9,
        instruments=["drums", "bass", "chords", "melody"],
        preset_names=preset_names,
        files=files_hash,
    )

    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(MANIFEST_NAME, json.dumps(asdict(manifest), indent=2))
        for src, arc in to_zip:
            zf.write(src, arcname=arc)
    return out


def verify_bundle(bundle_path: str | Path) -> tuple[bool, dict[str, bool]]:
    """Stdlib only: zipfile + hashlib + json."""
    status: dict[str, bool] = {}
    bp = str(bundle_path)
    try:
        with zipfile.ZipFile(bp, "r") as zf:
            if MANIFEST_NAME not in zf.namelist():
                return False, {"manifest": False}
            manifest = json.loads(zf.read(MANIFEST_NAME).decode("utf-8"))
            files: dict[str, str] = manifest.get("files", {})
            ok_all = True
            for name, expect_hex in files.items():
                if name not in zf.namelist():
                    status[name] = False
                    ok_all = False
                    continue
                data = zf.read(name)
                got = hashlib.sha256(data).hexdigest()
                match = got.lower() == str(expect_hex).lower()
                status[name] = match
                ok_all = ok_all and match
            status["manifest"] = True
            return ok_all, status
    except (OSError, zipfile.BadZipFile, json.JSONDecodeError, KeyError):
        return False, {"error": False}


def extract_bundle(bundle_path: str | Path, dest_dir: str | Path) -> bool:
    ok, _ = verify_bundle(bundle_path)
    if not ok:
        return False
    dest = Path(dest_dir)
    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(str(bundle_path), "r") as zf:
        manifest_raw = zf.read(MANIFEST_NAME)
        manifest = json.loads(manifest_raw.decode("utf-8"))
        files: dict[str, str] = manifest["files"]
        for name, expect_hex in files.items():
            data = zf.read(name)
            if hashlib.sha256(data).hexdigest().lower() != str(expect_hex).lower():
                return False
            (dest / name).write_bytes(data)
        (dest / MANIFEST_NAME).write_bytes(manifest_raw)
    return True


def main() -> None:
    p = argparse.ArgumentParser(description="Bridge .bridgemodels bundle tooling")
    sub = p.add_subparsers(dest="cmd", required=True)

    c = sub.add_parser("create", help="Zip models + presets into a .bridgemodels bundle")
    c.add_argument("--models-dir", required=True, help="Directory containing *_model.onnx / drum_humanizer.onnx")
    c.add_argument("--version", required=True, dest="version_tag", help='e.g. "v1"')
    c.add_argument("--out", required=True, help="Output path, e.g. dist/v1.bridgemodels")

    v = sub.add_parser("verify", help="Verify sha256 entries in bundle")
    v.add_argument("bundle", help="Path to .bridgemodels")

    x = sub.add_parser("extract", help="Verify and extract bundle to directory")
    x.add_argument("bundle", help="Path to .bridgemodels")
    x.add_argument("dest_dir", help="Destination directory")

    args = p.parse_args()
    if args.cmd == "create":
        out = create_bundle(args.models_dir, args.version_tag, args.out)
        print(f"Created {out}")
    elif args.cmd == "verify":
        ok, st = verify_bundle(args.bundle)
        for k, val in sorted(st.items()):
            print(f"  {k}: {'ok' if val else 'FAIL'}")
        if not ok:
            raise SystemExit(1)
        print("verify: OK")
    elif args.cmd == "extract":
        if not extract_bundle(args.bundle, args.dest_dir):
            print("extract: verification failed", file=sys.stderr)
            raise SystemExit(1)
        print(f"extracted to {args.dest_dir}")


if __name__ == "__main__":
    main()

