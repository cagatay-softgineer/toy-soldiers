# v1.2 #182: rasterizes assets/steam/*.svg to the exact PNG pixel sizes Steam
# requires for store-page/wishlist submission (Steam does not accept SVG
# uploads - the .svg files are the editable source, these .png files are what
# actually gets uploaded to Partner Center).
#
# Requires a Chrome or Edge install for headless rendering (no other deps).
# Usage: powershell -File scripts/render-steam-capsules.ps1

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$SrcDir = Join-Path $Root "assets\steam"

$browser = @(
  "C:\Program Files\Google\Chrome\Application\chrome.exe",
  "C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
  "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
  "C:\Program Files\Microsoft\Edge\Application\msedge.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $browser) { Write-Error "No Chrome/Edge install found for headless SVG rendering." }

# name -> (width, height), must match the .svg filename's own dimensions.
$capsules = @{
  "main_capsule_616x353"     = @(616, 353)
  "header_capsule_460x215"   = @(460, 215)
  "small_capsule_231x87"     = @(231, 87)
  "vertical_capsule_300x450" = @(300, 450)
}

foreach ($name in $capsules.Keys) {
  $w, $h = $capsules[$name]
  $svg = Join-Path $SrcDir "$name.svg"
  $png = Join-Path $SrcDir "$name.png"
  if (-not (Test-Path $svg)) { Write-Error "$svg missing" }
  $uri = "file:///" + ($svg -replace '\\', '/')
  & $browser --headless --disable-gpu "--window-size=$w,$h" "--screenshot=$png" $uri | Out-Null
  if (-not (Test-Path $png)) { Write-Error "Failed to render $name" }
  Write-Host "Rendered $name.png ($w x $h)"
}

Write-Host "Done. Upload the .png files under assets/steam/ to Partner Center."
