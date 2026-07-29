# Bootstrap vcpkg for Markup's Skia preset (manifest installs skia + sdl3 on cmake configure).
# Usage (PowerShell, from repo root):
#   .\scripts\setup-vcpkg.ps1
#   cmake --preset msvc-x64-skia
#   cmake --build build-skia --config Release

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$VcpkgDir = Join-Path $Root "vcpkg"

# devkitPro ships a broken git.exe that breaks vcpkg Skia fetches; prefer Git for Windows.
$GitCmd = "C:\Program Files\Git\cmd"
$GitBin = "C:\Program Files\Git\bin"
if (Test-Path (Join-Path $GitCmd "git.exe")) {
    $env:PATH = "$GitCmd;$GitBin;" + $env:PATH
} else {
    Write-Warning "Git for Windows not found at $GitCmd; vcpkg may fail if another git is broken."
}

if (-not (Test-Path $VcpkgDir)) {
    & (Join-Path $GitCmd "git.exe") clone https://github.com/microsoft/vcpkg.git $VcpkgDir
}

$VcpkgExe = Join-Path $VcpkgDir "vcpkg.exe"
if (-not (Test-Path $VcpkgExe)) {
    & (Join-Path $VcpkgDir "bootstrap-vcpkg.bat")
}

Write-Host "vcpkg ready at $VcpkgDir"
Write-Host "Next (manifest mode installs Skia + SDL3 during configure; first run can take 30+ min):"
Write-Host "  cmake --preset msvc-x64-skia"
Write-Host "  cmake --build build-skia --config Release"
