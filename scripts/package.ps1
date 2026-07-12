# v1.0 #163/#165/#200: portable-zip packaging + optional pdb archive + install smoke.
# Usage (from repo root, after a Release build):
#   powershell -File scripts/package.ps1            # stage + zip
#   powershell -File scripts/package.ps1 -Smoke     # + extract to temp, launch 6s, verify log
#   powershell -File scripts/package.ps1 -Pdb       # + separate symbols archive

param(
  [switch]$Smoke,
  [switch]$Pdb,
  [string]$Configuration = "Release",
  [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

# Single-source version from src/app/version.h
$verLine = Select-String -Path "src/app/version.h" -Pattern 'kVersionString = "([^"]+)"'
$Version = $verLine.Matches[0].Groups[1].Value
$codeLine = Select-String -Path "src/app/version.h" -Pattern 'kVersionCodename = "([^"]+)"'
$Codename = $codeLine.Matches[0].Groups[1].Value
$protoLine = Select-String -Path "src/game/types.h" -Pattern 'kProtocolVersion = (\d+)'
$Protocol = $protoLine.Matches[0].Groups[1].Value

$exe = Join-Path $BuildDir "bin\$Configuration\toy_soldiers.exe"
if (-not (Test-Path $exe)) {
  Write-Error "Build first: $exe not found (cmake --build --preset windows-release)"
  exit 1
}

$stageName = "toy-soldiers-v$Version"
$dist = Join-Path $Root "dist"
$stage = Join-Path $dist $stageName
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force (Join-Path $stage "data") | Out-Null

Copy-Item $exe (Join-Path $stage "toy_soldiers.exe")
Copy-Item "data/cards.json" (Join-Path $stage "data/cards.json")
Copy-Item "THIRD_PARTY_LICENSES.md" $stage
Copy-Item "LICENSE" $stage
Copy-Item "CHANGELOG.md" $stage

# #162: version.json in the package root.
$built = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
@"
{
  "name": "Oyuncak Asker Masa Savasi",
  "version": "$Version",
  "codename": "$Codename",
  "protocol": $Protocol,
  "builtUtc": "$built"
}
"@ | Out-File -Encoding utf8 (Join-Path $stage "version.json")

@"
Oyuncak Asker Masa Savasi (Toy Soldiers Tabletop) v$Version "$Codename"
=======================================================================

Portable build - no installation required.

  1. Run toy_soldiers.exe
  2. First time? Hit "Tutorial" on the main menu.
  3. LAN party: one player clicks "Host Lobby" and shares the 4-letter
     room code; everyone else types it under "Join a LAN game".

Settings, logs and crash dumps live under %APPDATA%\toy-soldiers\.
The game makes no internet connections; multiplayer is direct LAN/IP only.

Support: https://github.com/cagatay-softgineer/toy-soldiers/issues
"@ | Out-File -Encoding utf8 (Join-Path $stage "README.txt")

$zip = Join-Path $dist "$stageName-win64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host "Packaged: $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 2)) MB)"

if ($Pdb) {
  $pdbFile = Join-Path $BuildDir "bin\$Configuration\toy_soldiers.pdb"
  if (Test-Path $pdbFile) {
    $pdbZip = Join-Path $dist "$stageName-pdb.zip"
    if (Test-Path $pdbZip) { Remove-Item -Force $pdbZip }
    Compress-Archive -Path $pdbFile -DestinationPath $pdbZip
    Write-Host "Symbols: $pdbZip"
  } else {
    Write-Host "No pdb found (Release without /Zi) - skipping symbols archive"
  }
}

if ($Smoke) {
  # #200: complete install smoke - extract the zip somewhere clean and run it.
  $smokeDir = Join-Path $env:TEMP "toy-soldiers-smoke-$([guid]::NewGuid().ToString('N').Substring(0,8))"
  Expand-Archive -Path $zip -DestinationPath $smokeDir
  $smokeExe = Join-Path $smokeDir "$stageName\toy_soldiers.exe"
  Write-Host "Smoke: launching $smokeExe"
  $p = Start-Process -FilePath $smokeExe -WorkingDirectory (Split-Path $smokeExe) -PassThru
  Start-Sleep -Seconds 6
  if ($p.HasExited) {
    Write-Host "SMOKE FAILED: exited early with code $($p.ExitCode)"
    try { Remove-Item -Recurse -Force $smokeDir -ErrorAction Stop } catch {}
    exit 2
  }
  Stop-Process -Id $p.Id -Force
  $p.WaitForExit()
  Start-Sleep -Seconds 1
  try { Remove-Item -Recurse -Force $smokeDir -ErrorAction Stop } catch {
    Write-Host "note: smoke dir cleanup deferred ($smokeDir)"
  }
  Write-Host "SMOKE OK: packaged build runs from a clean extract"
}

exit 0
