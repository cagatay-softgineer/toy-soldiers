# v1.2 #167: Microsoft Store packaging experiment (Desktop Bridge - packages
# the existing Win32 exe as-is, no UWP rewrite). This is explicitly labelled
# an "experiment" in the roadmap: it produces a real, installable .msix for
# local sideload testing. Actual Store submission additionally needs a
# Partner Center identity (real Publisher CN + reserved app Name) and a
# certificate from a trusted CA or the Store's own signing - neither of which
# exists for this project yet. See docs/store/msix-packaging.md.
#
# Usage (after a Release build):
#   powershell -File scripts/package-msix.ps1              # stage + pack -> dist/*.msix (unsigned)
#   powershell -File scripts/package-msix.ps1 -SelfSign     # + sign with a local dev cert, for `Add-AppxPackage` sideload testing
#   powershell -File scripts/package-msix.ps1 -Install      # + install the self-signed package on this machine (implies -SelfSign)

param(
  [switch]$SelfSign,
  [switch]$Install,
  [string]$Configuration = "Release",
  [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
if ($Install) { $SelfSign = $true }

$verLine = Select-String -Path "src/app/version.h" -Pattern 'kVersionString = "([^"]+)"'
$Version = $verLine.Matches[0].Groups[1].Value

$dist = Join-Path $Root "dist"
$stage = Join-Path $dist "toy-soldiers-v$Version"
if (-not (Test-Path (Join-Path $stage "toy_soldiers.exe"))) {
  Write-Host "No staged build at $stage - running package.ps1 first..."
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "package.ps1") -Configuration $Configuration -BuildDir $BuildDir
}

# --- Assemble the MSIX layout ---
$msixStage = Join-Path $dist "msix-stage"
if (Test-Path $msixStage) { Remove-Item -Recurse -Force $msixStage }
New-Item -ItemType Directory -Force (Join-Path $msixStage "Assets") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $msixStage "data") | Out-Null

Copy-Item (Join-Path $stage "toy_soldiers.exe") $msixStage
Copy-Item (Join-Path $stage "data\cards.json") (Join-Path $msixStage "data")

$assetsSrc = Join-Path $Root "assets\msix"
if (-not (Test-Path (Join-Path $assetsSrc "Square150x150Logo.png"))) {
  Write-Host "No MSIX tile art found - generating placeholders..."
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "generate-msix-assets.ps1")
}
Copy-Item (Join-Path $assetsSrc "*.png") (Join-Path $msixStage "Assets")

$manifestTemplate = Join-Path $Root "packaging\msix\AppxManifest.xml.template"
$manifest = Get-Content $manifestTemplate -Raw
$manifest = $manifest.Replace("__VERSION__", $Version)
Set-Content -Path (Join-Path $msixStage "AppxManifest.xml") -Value $manifest -Encoding utf8

# --- Locate the Windows SDK tools ---
function Find-SdkTool([string]$Name) {
  $hit = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\$Name" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
  if (-not $hit) { Write-Error "$Name not found under Windows Kits 10\bin - install the Windows SDK." }
  return $hit.FullName
}
$makeappx = Find-SdkTool "makeappx.exe"

# --- Pack ---
$msixPath = Join-Path $dist "toy-soldiers-v$Version.msix"
if (Test-Path $msixPath) { Remove-Item -Force $msixPath }
& $makeappx pack /d $msixStage /p $msixPath /o
if ($LASTEXITCODE -ne 0) { Write-Error "makeappx pack failed (exit $LASTEXITCODE)" }
Write-Host "Packed: $msixPath ($([math]::Round((Get-Item $msixPath).Length / 1MB, 2)) MB)"

if ($SelfSign) {
  $signtool = Find-SdkTool "signtool.exe"
  $certSubject = "CN=preunec-dev"
  $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $certSubject } | Select-Object -First 1
  if (-not $cert) {
    Write-Host "No local dev cert found for $certSubject - creating one (sideload testing only)."
    $cert = New-SelfSignedCertificate -Type Custom -Subject $certSubject `
      -KeyUsage DigitalSignature -FriendlyName "Toy Soldiers MSIX dev cert" `
      -CertStoreLocation "Cert:\CurrentUser\My" `
      -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
  }
  $thumbprint = $cert.Thumbprint
  & $signtool sign /fd SHA256 /sha1 $thumbprint $msixPath
  if ($LASTEXITCODE -ne 0) { Write-Error "signtool sign failed (exit $LASTEXITCODE)" }
  Write-Host "Signed with local dev cert $certSubject (thumbprint $thumbprint) - NOT a Store-ready signature."
  Write-Host "To trust it on this machine for sideloading: import the cert into 'Trusted People' (Cert:\LocalMachine\TrustedPeople)."
}

if ($Install) {
  $trustedPeople = Get-ChildItem Cert:\LocalMachine\TrustedPeople | Where-Object { $_.Thumbprint -eq $thumbprint }
  if (-not $trustedPeople) {
    Write-Host "Trusting the dev cert for this machine (requires admin)..."
    $certBytes = (Get-Item Cert:\CurrentUser\My\$thumbprint).Export("Cert")
    $tmpCer = Join-Path $env:TEMP "toy-soldiers-msix-dev.cer"
    [IO.File]::WriteAllBytes($tmpCer, $certBytes)
    Import-Certificate -FilePath $tmpCer -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null
    Remove-Item $tmpCer -Force
  }
  Add-AppxPackage -Path $msixPath
  Write-Host "Installed. Launch from the Start menu ('Toy Soldiers Tabletop') to smoke-test."
}

exit 0
