# v1.2 #167: generates the placeholder MSIX tile art (assets/msix/*.png).
#
# There is no hand-authored logo for this project — every visual in the game
# itself is a flat-colored low-poly cube (no textures), so these placeholders
# draw the same silhouette (a blocky toy-soldier body+head) in the game's own
# palette instead of inventing unrelated brand art. Swap these for real art
# before an actual Store submission; nothing here claims to be final key art.
#
# Usage: powershell -File scripts/generate-msix-assets.ps1

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$Root = Split-Path -Parent $PSScriptRoot
$OutDir = Join-Path $Root "assets\msix"
New-Item -ItemType Directory -Force $OutDir | Out-Null

# Game palette: dark navy table-felt background, gold seat-0 color soldier.
$bg = [System.Drawing.Color]::FromArgb(255, 11, 15, 42)      # #0B0F2A
$fg = [System.Drawing.Color]::FromArgb(255, 245, 196, 92)    # gold (kBanner[0])

function New-TileIcon {
    param([int]$Width, [int]$Height, [string]$Path)

    $bmp = New-Object System.Drawing.Bitmap($Width, $Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear($bg)

    # Blocky toy-soldier silhouette: rectangular body + square head, centered,
    # scaled to ~60% of the shorter dimension (mirrors the in-game low-poly look).
    $unit = [Math]::Min($Width, $Height) / 10.0
    $brush = New-Object System.Drawing.SolidBrush($fg)

    $bodyW = $unit * 2.2
    $bodyH = $unit * 4.0
    $bodyX = ($Width / 2.0) - ($bodyW / 2.0)
    $bodyY = ($Height / 2.0) - ($unit * 0.5)
    $g.FillRectangle($brush, $bodyX, $bodyY, $bodyW, $bodyH)

    $headS = $unit * 1.6
    $headX = ($Width / 2.0) - ($headS / 2.0)
    $headY = $bodyY - $headS + ($unit * 0.3)
    $g.FillRectangle($brush, $headX, $headY, $headS, $headS)

    $brush.Dispose()
    $g.Dispose()
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "Wrote $Path ($Width x $Height)"
}

New-TileIcon -Width 44  -Height 44  -Path (Join-Path $OutDir "Square44x44Logo.png")
New-TileIcon -Width 150 -Height 150 -Path (Join-Path $OutDir "Square150x150Logo.png")
New-TileIcon -Width 50  -Height 50  -Path (Join-Path $OutDir "StoreLogo.png")
New-TileIcon -Width 310 -Height 150 -Path (Join-Path $OutDir "Wide310x150Logo.png")
New-TileIcon -Width 620 -Height 300 -Path (Join-Path $OutDir "SplashScreen.png")

Write-Host "MSIX placeholder assets generated in $OutDir"
