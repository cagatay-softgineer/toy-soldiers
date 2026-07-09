# Create milestones + roadmap issues on GitHub.
# Usage (from repo root, after push):
#   powershell -File scripts/bootstrap-github.ps1
#   powershell -File scripts/bootstrap-github.ps1 -Repo cagatay-softgineer/toy-soldiers

param(
  [string]$Repo = "",
  [string]$RoadmapPath = "docs/ROADMAP_to_v1.md",
  [switch]$SkipIssues,
  [switch]$DryRun
)

$ErrorActionPreference = "Stop"

if (-not $Repo) {
  $Repo = (gh repo view --json nameWithOwner -q .nameWithOwner)
}

Write-Host "Repo: $Repo"
Write-Host "Roadmap: $RoadmapPath"

function Ensure-Label {
  param([string]$Name, [string]$Color, [string]$Desc)
  gh label create $Name --repo $Repo --color $Color --description $Desc 2>$null | Out-Null
  if ($LASTEXITCODE -ne 0) {
    gh label edit $Name --repo $Repo --color $Color --description $Desc 2>$null | Out-Null
  }
}

Write-Host "Ensuring labels..."
Ensure-Label "priority:P0" "d73a4a" "Must ship in milestone"
Ensure-Label "priority:P1" "fbca04" "Should ship"
Ensure-Label "priority:P2" "0e8a16" "Stretch"
Ensure-Label "type:feature" "1d76db" "Roadmap feature"
Ensure-Label "roadmap" "5319e7" "From ROADMAP_to_v1.md"

$milestones = @(
  @{ Title = "v0.5 - Tabletop MVP"; Desc = "Shipped jam MVP (M0-M5). Baseline."; State = "closed"; Due = $null },
  @{ Title = "v0.6 - Solid Core"; Desc = "App shell, settings, UX clarity, QA gates. Features #1-36."; State = "open"; Due = "2026-08-15" },
  @{ Title = "v0.7 - Deep Toybox"; Desc = "Cards, towers, modes, AI, event cards. Features #37-77."; State = "open"; Due = "2026-09-30" },
  @{ Title = "v0.8 - Reliable Party"; Desc = "Lobby UX, room codes, reconnect, sync. Features #78-116."; State = "open"; Due = "2026-11-15" },
  @{ Title = "v0.9 - Identity and Content"; Desc = "Art, audio, maps, cosmetics, TR/EN. Features #117-161."; State = "open"; Due = "2027-01-15" },
  @{ Title = "v1.0 - Ship Toy Soldiers"; Desc = "Installer, store, tutorial, balance freeze. Features #162-200."; State = "open"; Due = "2027-03-01" }
)

$milestoneNumber = @{}

foreach ($m in $milestones) {
  Write-Host "Milestone: $($m.Title)"
  if ($DryRun) { continue }

  $createArgs = @(
    "api", "-X", "POST", "repos/$Repo/milestones",
    "-f", "title=$($m.Title)",
    "-f", "description=$($m.Desc)",
    "-f", "state=$($m.State)"
  )
  if ($m.Due) {
    $createArgs += @("-f", "due_on=$($m.Due)T23:59:59Z")
  }
  $created = & gh @createArgs 2>$null
  if ($LASTEXITCODE -eq 0 -and $created) {
    $obj = $created | ConvertFrom-Json
    $milestoneNumber[$m.Title] = $obj.number
    Write-Host "  created #$($obj.number)"
  } else {
    $list = gh api "repos/$Repo/milestones?state=all&per_page=100" | ConvertFrom-Json
    $found = $list | Where-Object { $_.title -eq $m.Title } | Select-Object -First 1
    if (-not $found) { throw "Could not create or find milestone: $($m.Title)" }
    $milestoneNumber[$m.Title] = $found.number
    gh api -X PATCH "repos/$Repo/milestones/$($found.number)" -f "description=$($m.Desc)" -f "state=$($m.State)" | Out-Null
    Write-Host "  exists #$($found.number)"
  }
}

function Milestone-ForId([int]$id) {
  if ($id -le 36) { return "v0.6 - Solid Core" }
  if ($id -le 77) { return "v0.7 - Deep Toybox" }
  if ($id -le 116) { return "v0.8 - Reliable Party" }
  if ($id -le 161) { return "v0.9 - Identity and Content" }
  return "v1.0 - Ship Toy Soldiers"
}

if ($SkipIssues) {
  Write-Host "SkipIssues set - done."
  exit 0
}

$lines = Get-Content $RoadmapPath -Encoding UTF8
$features = @()
foreach ($line in $lines) {
  if ($line -match '^\s*(\d+)\.\s+`(P[012])`\s+(.+?)\s*$') {
    $features += [pscustomobject]@{
      Id       = [int]$Matches[1]
      Priority = $Matches[2]
      Title    = $Matches[3].Trim()
    }
  }
}

Write-Host "Parsed $($features.Count) features from roadmap."

$existing = gh issue list --repo $Repo --state all --limit 500 --json number,title | ConvertFrom-Json
$existingKeys = @{}
foreach ($e in $existing) {
  if ($e.title -match '^#(\d+)\b') {
    $existingKeys[[int]$Matches[1]] = $e.number
  }
}

$createdCount = 0
$skippedCount = 0
$failCount = 0

foreach ($f in ($features | Sort-Object Id)) {
  $msTitle = Milestone-ForId $f.Id
  $issueTitle = "#{0} [{1}] {2}" -f $f.Id, $f.Priority, $f.Title
  if ($issueTitle.Length -gt 250) {
    $issueTitle = $issueTitle.Substring(0, 247) + "..."
  }

  if ($existingKeys.ContainsKey($f.Id)) {
    Write-Host "Skip existing #$($f.Id) -> issue $($existingKeys[$f.Id])"
    $skippedCount++
    continue
  }

  $body = @"
## Roadmap feature

| Field | Value |
|-------|-------|
| **Roadmap ID** | ``#$($f.Id)`` |
| **Priority** | ``$($f.Priority)`` |
| **Milestone** | $msTitle |
| **Source** | [docs/ROADMAP_to_v1.md](docs/ROADMAP_to_v1.md) |

### Description
$($f.Title)

### Acceptance (draft)
- [ ] Implemented and wired into game/lobby as needed
- [ ] Covered by automated test when feasible
- [ ] Documented in CHANGELOG when shipped

---
*Auto-filed from roadmap bootstrap script.*
"@

  if ($DryRun) {
    Write-Host "[dry-run] $issueTitle"
    continue
  }

  $labelPri = "priority:$($f.Priority)"
  $out = gh issue create --repo $Repo `
    --title $issueTitle `
    --body $body `
    --label "type:feature" `
    --label "roadmap" `
    --label $labelPri `
    --milestone $msTitle 2>&1

  if ($LASTEXITCODE -ne 0) {
    Start-Sleep -Milliseconds 800
    $out = gh issue create --repo $Repo `
      --title $issueTitle `
      --body $body `
      --label "type:feature" `
      --label "roadmap" `
      --label $labelPri `
      --milestone $msTitle 2>&1
    if ($LASTEXITCODE -ne 0) {
      Write-Host "FAIL #$($f.Id): $out"
      $failCount++
      continue
    }
  }

  Write-Host "Created: $out"
  $createdCount++
  Start-Sleep -Milliseconds 300
}

Write-Host ""
Write-Host "Done. created=$createdCount skipped=$skippedCount failed=$failCount totalParsed=$($features.Count)"
if ($failCount -gt 0) { exit 1 }
