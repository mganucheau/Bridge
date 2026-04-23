#Requires -Version 5.1
<#
  Builds Bridge (Release), creates the .bridgemodels bundle, and stages files for an NSIS/WiX installer.
  Adjust $CMakeGenerator and paths for your toolchain (VS 2022 x64).
#>
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Version = if ($env:VERSION) { $env:VERSION } else { "v1" }
$BuildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path $Root "build-release" }
$Dist = Join-Path $Root "dist"
$Payload = Join-Path $Dist "nsis-payload"

Write-Host "==> Configure & build (Release)"
cmake -S $Root -B $BuildDir -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --config Release --parallel

Write-Host "==> ML bundle"
New-Item -ItemType Directory -Force -Path $Dist | Out-Null
python (Join-Path $Root "ml/training/bundle.py") create `
  --models-dir (Join-Path $Root "ml/models") `
  --version $Version `
  --out (Join-Path $Dist "$Version.bridgemodels")

Write-Host "==> Stage payload: $Payload"
if (Test-Path $Payload) { Remove-Item -Recurse -Force $Payload }
New-Item -ItemType Directory -Force -Path $Payload | Out-Null
Copy-Item (Join-Path $Dist "$Version.bridgemodels") $Payload

$vst3 = Get-ChildItem -Path $BuildDir -Recurse -Directory -Filter "Bridge.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($vst3) {
  $dest = Join-Path $Payload "Bridge.vst3"
  Copy-Item $vst3.FullName $dest -Recurse
}

Write-Host "Staged under $Payload"
Get-ChildItem $Payload -Recurse | Select-Object -First 20 FullName

Write-Host "`nNext: run NSIS or WiX against this payload; see installer/README.md."
