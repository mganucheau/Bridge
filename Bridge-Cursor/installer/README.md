# Bridge installers

## Payload layout

Scripts stage a directory you can consume from **pkgbuild** (macOS) or **NSIS / WiX** (Windows):

- `Bridge.vst3` — VST3 bundle (Windows: folder; macOS: bundle)
- `Bridge.component` — Audio Unit (macOS only, when built)
- `*.bridgemodels` — signed model zip + `manifest.json` (sha256 per file) + ONNX + `personality_presets.json`

First-run users get working models from the bundled `.bridgemodels` (installer copies files next to plug-ins and/or documents the optional app-support path). The plugin also loads from Application Support — see `BridgeModelBundle::getModelsDirectory()`.

## macOS: signing and notarization (summary)

1. **Developer ID Application** certificate installed in Keychain.
2. Sign all binaries inside `.component` / `.vst3` and the bundle roots:

   ```bash
   codesign --force --options runtime --sign "Developer ID Application: …" -v Path/To/Bridge.vst3
   codesign --force --options runtime --sign "Developer ID Application: …" -v Path/To/Bridge.component
   ```

3. Build the installer (`.pkg`) with `pkgbuild` / `productbuild`, sign the package.
4. Submit for **notarization** (`xcrun notarytool` or `altool`), then staple:

   ```bash
   xcrun stapler staple YourInstaller.pkg
   ```

5. Gatekeeper: distribute the stapled pkg or notarized zip.

Details change with Apple tooling; use Apple’s current “Notarizing macOS software” documentation.

## Windows: signing

Sign the installer executable and optionally the VST3 payload with your Authenticode certificate (`signtool`). WiX / NSIS tutorials cover timestamp servers and dual-signing when required.

## Model bundle

Produce the bundle with:

```bash
make ml-bundle VERSION=v1
# or
cmake --build build --target ml-bundle
```

Configure the tag with `-DML_BUNDLE_VERSION=v2` when using CMake.
