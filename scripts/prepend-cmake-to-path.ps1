# Run in an elevated PowerShell (Run as administrator) so Machine PATH can be updated.
# Moves C:\Program Files\CMake\bin before devkitPro MSYS cmake (and removes duplicate CMake entries).

$ErrorActionPreference = 'Stop'
$cmakeBin = 'C:\Program Files\CMake\bin'

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error 'Run this script as Administrator (right-click PowerShell -> Run as administrator).'
}

$current = [Environment]::GetEnvironmentVariable('Path', 'Machine')
if (-not $current) { $current = '' }

$parts = $current -split ';' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }
$norm = { param($p) [System.IO.Path]::GetFullPath($p).TrimEnd('\') }
$cmakeNorm = & $norm $cmakeBin

$filtered = @()
foreach ($p in $parts) {
    try {
        if ((& $norm $p) -eq $cmakeNorm) { continue }
    } catch { }
    $filtered += $p
}

$newPath = ($cmakeNorm + ';' + ($filtered -join ';')).TrimEnd(';')
[Environment]::SetEnvironmentVariable('Path', $newPath, 'Machine')

Write-Host 'Machine PATH updated. CMake should now resolve first after opening a new terminal.'
Write-Host 'Verify:  where.exe cmake'
Write-Host 'Expected: C:\Program Files\CMake\bin\cmake.exe'
