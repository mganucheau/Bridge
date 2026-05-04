#!/usr/bin/env bash
set -euo pipefail
# Builds the plugin (Release), packages ML bundle, and stages a .pkg-oriented payload layout.
# Code signing and notarization are manual — see installer/README.md.

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${VERSION:-v1}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-release}"
DIST="$ROOT/dist"
PAYLOAD="$DIST/pkgroot"

echo "==> Configure & build (Release)"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release --parallel

echo "==> ML bundle"
mkdir -p "$DIST"
python3 "$ROOT/ml/training/bundle.py" create \
  --models-dir "$ROOT/ml/models" \
  --version "$VERSION" \
  --out "$DIST/${VERSION}.bridgemodels"

echo "==> Stage payload under $PAYLOAD"
rm -rf "$PAYLOAD"
mkdir -p "$PAYLOAD"

cp "$DIST/${VERSION}.bridgemodels" "$PAYLOAD/"

# AU / VST3 product dirs after build (COPY_PLUGIN_AFTER_BUILD)
AU=$(find "$BUILD_DIR" -name "Bridge.component" -type d | head -1)
VST3=$(find "$BUILD_DIR" -name "Bridge.vst3" -type d | head -1)
if [[ -n "${AU:-}" ]]; then
  mkdir -p "$PAYLOAD/AudioUnit"
  cp -R "$AU" "$PAYLOAD/AudioUnit/"
fi
if [[ -n "${VST3:-}" ]]; then
  mkdir -p "$PAYLOAD/VST3"
  cp -R "$VST3" "$PAYLOAD/VST3/"
fi

echo "Staged:"
find "$PAYLOAD" -maxdepth 3 -type d | sed "s|^|  |"

echo
echo "Next: package with pkgbuild/productbuild, sign, and notarize (see installer/README.md)."
