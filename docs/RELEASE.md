# Release Operations — v1.0 "Ship Toy Soldiers"

## Balance freeze (#173)

**Card numbers, tower stats, and event weights are FROZEN as of v1.0.0.**
After the `v1.0.0` tag, only bug fixes land on `main`. Any balance change requires a
`v1.1` branch and a fresh 200-trial balance report inside the 25–70 turn band.

## Day-0 hotfix policy (#194)

1. Blocker found after tag → branch `hotfix/v1.0.x` from the tag, never from main.
2. Fix + regression test reproducing the bug; `scripts/verify.ps1` must be green.
3. Bump `kVersionPatch` (+ CMake VERSION), re-run `scripts/package.ps1 -Smoke`.
4. Tag `v1.0.x`, upload zip to itch, note in CHANGELOG under a new heading.
5. Cherry-pick to main afterwards.

## Playtest checklist (#175) — sign off before public release

- [ ] 4 humans, LAN, full Classic FFA match to completion (no voice-chat debugging needed)
- [ ] One player force-killed mid-match → AI takes over → reclaim within 60s works
- [ ] Room-code join works from a second physical machine (not loopback)
- [ ] Tutorial completed by someone who has never seen the game (target: < 5 min)
- [ ] Turkish UI pass: switch language, play a full match, no clipped/missing strings
- [ ] Fresh Windows account: extract zip, run, host, exit — no crashes, settings persist

## Known issues at v1.0.0 (#176) — ≤5 non-blockers

1. Sniper win rate sits at the top of the target band (~59–60%) — watch, don't touch (freeze).
2. Linux configure preset exists but is unverified on real hardware.
3. Window resolution changes need an app restart (sokol swapchain limitation).
4. Photo mode in a networked match hides UI but cannot freeze the host's simulation.
5. Beacon discovery may need a firewall allow-rule on first launch (UDP 27124).

## v1.0.1 buffer list (#196) — pre-prioritized, fix-only

1. Any crash from the field (minidumps land in `%APPDATA%/toy-soldiers/crashes/`)
2. Firewall UX: detect blocked UDP beacon and show a hint instead of an empty LAN list
3. History file growth cap (currently unbounded append; trim to last 200 lines)
4. i18n stragglers reported by TR players

## v1.1 teaser (#197) — no promises

Spectator seats, replay export (`.toyrec`), snapshot compression, host migration,
Steam Networking relay, and the community card-proposal pipeline (`data/cards.json`
is already the modding surface).

## Support (#195)

- Bugs: https://github.com/cagatay-softgineer/toy-soldiers/issues (template: version.json
  contents + session log + minidump if any)
- Community: Discord invite TBD (placeholder in credits)
