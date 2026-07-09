$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$env:Path = "C:\Program Files\CMake\bin;" + $env:Path

Set-Location $Root
cmake --preset windows
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset windows-release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Built:"
Write-Host "  $Root\build\bin\Release\toy_soldiers.exe"
Write-Host "  $Root\build\bin\Release\toy_sim.exe"
