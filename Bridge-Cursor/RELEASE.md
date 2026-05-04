# Bridge release checklist

1. **Retrain** (if needed): `python ml/training/retrain_pipeline.py --instrument all --version-tag vN`
2. **Quality**: review `preset_quality_report.md` and perceptual / snapshot outputs from the pipeline.
3. **Registry**: register and activate the ONNX set, e.g. `python ml/training/model_registry.py register vN --source ml/models --root ml/models` (and activate per your workflow).
4. **Bundle**: `make ml-bundle VERSION=vN` or `cmake --build <dir> --target ml-bundle -DML_BUNDLE_VERSION=vN`.
5. **Verify**: run **BridgeQA** (`ctest` / `BridgeTests`) and `cmake --build <dir> --target test-ml` (or `python ml/training/snapshot_tests.py run`).
6. **Installers**: `installer/build_installer_mac.sh` and `installer/build_installer_win.ps1` (set `VERSION=vN`); complete signing, macOS notarization, and Windows Authenticode per `installer/README.md`.
7. **CDN / updates**: upload `vN.bridgemodels` to your CDN; publish `manifest.json` at the update URL (shape: `{ "latest": "vN", "url": "https://…/vN.bridgemodels", "min_bridge_version": 9 }`). Keep **no user identifiers** in that endpoint.
8. **Git**: tag the release (e.g. `vN`) and publish release notes.
