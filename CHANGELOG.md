# Changelog

All notable changes to **Oyuncak Asker Masa Savaşı (toy-soldiers)** are documented here.

## [Unreleased]

### Planned
- v0.8 Reliable Party — see [docs/ROADMAP_to_v1.md](docs/ROADMAP_to_v1.md)

## [0.7.0-dev] — 2026-07-11

### Added — v0.7 Deep Toybox (protocol v5)
- **Towers (#37–#41):** Shield Bearer (starts with 1 shield, attacks −1) and Sapper (first attack +1);
  tower pick UI with stats comparison table + passive tooltips; per-tower signature starter card
- **Cards (#42–#51):** keyword system (`Shield`, `Pierce`, `Draw`, `Heal`, `AdjacentOnly`, `AOE`, `Event`);
  catalog grown to 23 defs — Tin Rain (AOE both neighbors), Marble Strike (Pierce), Slingshot,
  Cardboard Bunker, Magnet Hand (steal shield), Line Cutter (capped turn skip);
  deck builder lite (ban 2 / add 2 sideboard, new `SetDeckMods` message); hand-full draw feedback;
  public last-played-card banner; rarity-tinted card buttons; offline **Card Codex** browser
- **Modes (#52–#57):** Classic FFA, **Quick Duel** (1v1, 24 HP, 15-card decks), **2v2 Teams**
  (shared win, no friendly fire), **Hot Potato King** (crown +1 dmg, steals on hit),
  **Sandbox** (towers reset, free event spawns); optional turn timer 30/45/60s with host auto-skip
- **AI (#58–#61):** Easy (random legal) / Normal (heuristic) / Hard (lethal-aware) difficulty;
  Aggro / Turtle / Chaos personalities per AI seat; flavor log lines; softlock-proof fallback
- **Events 2.0 (#63–#68):** player-cast **Event cards** — "Call the Cat", "Pocket Sand"
  (max 2 per deck); ambient **Dog** (table-wide physics shove) and **Blackout** (enemy HP fog);
  event history panel with icons
- **Targeting (#70–#72):** lobby toggle free vs adjacent-only; illegal targets greyed out in HUD;
  teammates untargetable in 2v2
- **Physics readability (#74–#76):** slow-mo on tower destroy, camera punch on ≥5 dmg hits
  (both respect reduced motion), optional parade-rest ruleset (soldiers stand back up each round)
- **QA:** new `toy_mode_test` gate (towers, keywords, modes, AI tiers, snapshot v5) wired into
  `scripts/verify.ps1`; hand hotkeys extended to 1–8; CMake project version 0.7.0

## [0.6.0-dev] — 2026-07-09

### Added — v0.6 Solid Core (P0 + P1 + P2)
- App screens: **Menu → Lobby → Match → Results** (+ Settings / How to Play / **Credits**)
- Settings persistence (`%APPDATA%/toy-soldiers/settings.ini`)
- Illegal-play reasons as toasts ("Not your turn", "Sandstorm: adjacent only", …)
- Turn-change banner (~0.85s)
- Auto-select first legal target each turn
- Protocol version shown in lobby/match UI
- `scripts/verify.ps1` one-command test gate
- Pause overlay (Esc) with rules reminder + **confirm leave**
- **P1:** fullscreen/VSync/resolution prefs, FPS & sync debug, high-contrast theme, UI scale
- **P1:** EN/TR UI strings + glossary; card tooltips with keywords
- **P1:** Continue last mode; rematch keeps cosmetics/map + new seed
- **P1:** Post-match Play again / Change loadout / Menu
- **P1:** Session log file (no secrets); golden-seed determinism test
- **P1:** Client reject soft-fail toasts
- **P2:** Credits screen (Box3D, sokol, ImGui, team)
- **P2:** Coach tips for first 3 completed matches (toggle in Settings)
- **P2:** Settings export/import JSON (next to settings.ini)
- **P2:** Match timeline scrubber (battle log + results)
- **P2:** AddressSanitizer Debug preset (`windows-asan`) + [docs/ASAN.md](docs/ASAN.md)
- **P2:** Windows crash minidumps under `%APPDATA%/toy-soldiers/crashes/`
- **P2:** Reduced motion (no auto-orbit, shorter toasts/banners)

## [0.5.0] — 2026-07-09

### Added — MVP (M0–M5)
- Hotseat turn-based match (cards, towers, win condition)
- Host-authoritative TCP lobby multiplayer
- Maps: Living Room, Desert, Backyard + room props
- World events: Sandstorm, Rain, Flood, Cat
- Cosmetics: plastic color, tower skin, accessory
- Balance pass + headless tests + balance report
- ImGui HUD, how-to-play, toasts, damage preview
- Continuous CI release workflow (GitHub Actions)

### Notes
- Physics via sibling [Box3D](https://github.com/erincatto/box3d)
- Product baseline labeled **v0.5 Tabletop MVP**
