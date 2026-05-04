# ONNX Runtime (optional)

Bridge can link against **ONNX Runtime** for ML inference. If this folder is not set up, the build still succeeds and the plugin uses **legacy** humanization only.

## Download pre-built binaries

Get official releases from:

**https://github.com/microsoft/onnxruntime/releases**

Suggested version layout (adjust version as needed):

| Platform    | Archive (example)              |
|------------|---------------------------------|
| Mac arm64  | `onnxruntime-osx-arm64-1.16.3` |
| Mac x86_64 | `onnxruntime-osx-x86_64-1.16.3` |
| Windows x64| `onnxruntime-win-x64-1.16.3`   |

## Install layout

Unzip the archive so that this repository contains:

```text
libs/onnxruntime/
  include/          ← headers (e.g. onnxruntime_cxx_api.h)
  lib/              ← libraries
```

On **macOS**, link against `lib/libonnxruntime.dylib`.  
On **Windows**, link against `lib/onnxruntime.lib` (and ensure the DLL is found at runtime).

The CMake project checks for `libs/onnxruntime/include/onnxruntime_cxx_api.h` to enable ML features.
