# Changelog

All notable changes to **Oyuncak Asker Masa Savaşı (toy-soldiers)** are documented here.

## [Unreleased]

### Planned
- v0.7 Deep Toybox — see [docs/ROADMAP_to_v1.md](docs/ROADMAP_to_v1.md)

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
