# Create the v2.0-line milestones + roadmap issues on GitHub.
# Twin of bootstrap-github.ps1 for docs/ROADMAP_to_v2.md (feature IDs 201-430).
# Usage (from repo root, after push):
#   powershell -File scripts/bootstrap-github-v2.ps1 -DryRun     # preview parse + milestones
#   powershell -File scripts/bootstrap-github-v2.ps1             # file for real
#   powershell -File scripts/bootstrap-github-v2.ps1 -SkipIssues # milestones only

param(
  [string]$Repo = "",
  [string]$RoadmapPath = "docs/ROADMAP_to_v2.md",
  [switch]$SkipIssues,
  [switch]$DryRun
)

# Manual exit-code handling throughout (gh writes to stderr on benign "already exists"
# cases, which PowerShell 5.1 turns into terminating errors under 'Stop').
$ErrorActionPreference = "Continue"

if (-not $Repo) {
  $Repo = (gh repo view --json nameWithOwner -q .nameWithOwner)
}

Write-Host "Repo: $Repo"
Write-Host "Roadmap: $RoadmapPath"

function Ensure-Label {
  param([string]$Name, [string]$Color, [string]$Desc)
  # --force upserts (create or update); try/catch swallows the native-stderr noise.
  try { gh label create $Name --repo $Repo --color $Color --description $Desc --force 2>$null | Out-Null } catch {}
}

Write-Host "Ensuring labels..."
Ensure-Label "priority:P0" "d73a4a" "Must ship in milestone"
Ensure-Label "priority:P1" "fbca04" "Should ship"
Ensure-Label "priority:P2" "0e8a16" "Stretch"
Ensure-Label "type:feature" "1d76db" "Roadmap feature"
Ensure-Label "roadmap:v2" "5319e7" "From ROADMAP_to_v2.md"

$milestones = @(
  @{ Title = "v1.3 - Open Signal"; Desc = "Relay + matchmaking behind the #89 transport seam. Features #201-235."; State = "open"; Due = "2027-04-15" },
  @{ Title = "v1.4 - Persistent Toybox"; Desc = "Accounts, cloud saves, server-authoritative profiles. Features #236-265."; State = "open"; Due = "2027-06-01" },
  @{ Title = "v1.5 - Proving Ground"; Desc = "Ranked ladder, MMR, leaderboards, seasons v1. Features #266-298."; State = "open"; Due = "2027-07-15" },
  @{ Title = "v1.6 - Workshop"; Desc = "In-game UGC for cards/cosmetics/maps, sharing, moderation. Features #299-330."; State = "open"; Due = "2027-09-01" },
  @{ Title = "v1.7 - Solid Cast"; Desc = "Authored 3D models, art upgrade, observer + caster tools. Features #331-354."; State = "open"; Due = "2027-10-15" },
  @{ Title = "v1.8 - Solo Campaign"; Desc = "PvE campaign, roguelike run, missions + bosses. Features #355-379."; State = "open"; Due = "2027-12-01" },
  @{ Title = "v1.9 - Pocket Platoon"; Desc = "Mobile companion + cross-play spectator; store follow-through. Features #380-399."; State = "open"; Due = "2028-01-15" },
  @{ Title = "v2.0 - Living Toybox"; Desc = "Live-ops seasons, cosmetic pass, launch, telemetry, re-freeze. Features #400-430."; State = "open"; Due = "2028-03-01" }
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
  if ($id -le 235) { return "v1.3 - Open Signal" }
  if ($id -le 265) { return "v1.4 - Persistent Toybox" }
  if ($id -le 298) { return "v1.5 - Proving Ground" }
  if ($id -le 330) { return "v1.6 - Workshop" }
  if ($id -le 354) { return "v1.7 - Solid Cast" }
  if ($id -le 379) { return "v1.8 - Solo Campaign" }
  if ($id -le 399) { return "v1.9 - Pocket Platoon" }
  return "v2.0 - Living Toybox"
}

if ($SkipIssues) {
  Write-Host "SkipIssues set - done."
  exit 0
}

$lines = Get-Content $RoadmapPath -Encoding UTF8
$features = @()
foreach ($line in $lines) {
  if ($line -match '^\s*(\d+)\.\s+`(P[012])`\s+(.+?)\s*$') {
    $id = [int]$Matches[1]
    if ($id -lt 201 -or $id -gt 430) { continue } # v2 line only
    $features += [pscustomobject]@{
      Id       = $id
      Priority = $Matches[2]
      Title    = $Matches[3].Trim()
    }
  }
}

Write-Host "Parsed $($features.Count) features from roadmap."

$existing = gh issue list --repo $Repo --state all --limit 800 --json number,title | ConvertFrom-Json
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
| **Source** | [docs/ROADMAP_to_v2.md](docs/ROADMAP_to_v2.md) |

### Description
$($f.Title)

### Acceptance (draft)
- [ ] Implemented and wired into game/lobby/backend as needed
- [ ] Covered by automated test when feasible
- [ ] Documented in CHANGELOG when shipped

---
*Auto-filed from the v2 roadmap bootstrap script.*
"@

  if ($DryRun) {
    Write-Host "[dry-run] $issueTitle -> $msTitle"
    continue
  }

  $labelPri = "priority:$($f.Priority)"
  $out = gh issue create --repo $Repo `
    --title $issueTitle `
    --body $body `
    --label "type:feature" `
    --label "roadmap:v2" `
    --label $labelPri `
    --milestone $msTitle 2>&1

  if ($LASTEXITCODE -ne 0) {
    Start-Sleep -Milliseconds 800
    $out = gh issue create --repo $Repo `
      --title $issueTitle `
      --body $body `
      --label "type:feature" `
      --label "roadmap:v2" `
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
