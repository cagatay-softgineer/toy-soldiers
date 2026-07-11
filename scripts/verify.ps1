# v0.6: one-command regression gate for toy-soldiers.
# Usage (from repo root, after build):
#   powershell -File scripts/verify.ps1
#   powershell -File scripts/verify.ps1 -Configuration Debug

param(
  [string]$Configuration = "Release",
  [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$candidates = @(
  (Join-Path $BuildDir "bin\$Configuration"),
  (Join-Path $BuildDir "bin")
)

$bin = $null
foreach ($c in $candidates) {
  if (Test-Path (Join-Path $c "toy_sim.exe")) {
    $bin = $c
    break
  }
}

if (-not $bin) {
  Write-Host "Binaries not found. Building $Configuration..."
  $env:Path = "C:\Program Files\CMake\bin;" + $env:Path
  if (-not (Test-Path $BuildDir)) {
    cmake --preset windows
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  }
  cmake --build --preset ("windows-" + $Configuration.ToLower()) --parallel
  if ($LASTEXITCODE -ne 0) {
    cmake --build $BuildDir --config $Configuration --parallel
  }
  foreach ($c in $candidates) {
    if (Test-Path (Join-Path $c "toy_sim.exe")) {
      $bin = $c
      break
    }
  }
}

if (-not $bin) {
  Write-Error "Could not locate toy_sim.exe under $BuildDir"
  exit 1
}

Write-Host "Using binaries in: $bin"
$tests = @(
  "toy_sim.exe",
  "toy_event_test.exe",
  "toy_cosmetic_test.exe",
  "toy_mode_test.exe",
  "toy_balance_report.exe",
  "toy_net_test.exe",
  "toy_golden_seed_test.exe"
)

$failed = 0
foreach ($t in $tests) {
  $path = Join-Path $bin $t
  if (-not (Test-Path $path)) {
    Write-Host "MISSING $t"
    $failed++
    continue
  }
  Write-Host "=== $t ==="
  & $path
  if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL $t exit=$LASTEXITCODE"
    $failed++
  } else {
    Write-Host "OK $t"
  }
}

if ($failed -gt 0) {
  Write-Host "verify FAILED ($failed)"
  exit 2
}
Write-Host "verify OK - all gates passed"
exit 0
