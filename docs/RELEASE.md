# Release Operations — v1.2 "Reinforcements"

## Balance freeze (#173)

**Card numbers, tower stats, and event weights are FROZEN as of v1.2.0.**
v1.2 added two new rule-affecting mechanics (Titan boss event #69, flood line-of-sight
#73), which per this policy required re-validating balance before shipping — done: a
fresh 200-trial `toy_balance_report` run (part of every `scripts/verify.ps1` pass) came
back inside the 25–70 turn band with Sniper win rate at 56% (target band ~40–60%,
consistent with the pre-existing v1.0.0 baseline). After the `v1.2.0` tag, only bug
fixes land on `main` again. Any further balance change requires a new minor-version
branch and a fresh 200-trial balance report inside the same band.

## Day-0 hotfix policy (#194)

1. Blocker found after tag → branch `hotfix/v1.2.x` from the tag, never from main.
2. Fix + regression test reproducing the bug; `scripts/verify.ps1` must be green.
3. Bump `kVersionPatch` (+ CMake VERSION), re-run `scripts/package.ps1 -Smoke`.
4. Tag `v1.2.x`, upload zip to itch, note in CHANGELOG under a new heading.
5. Cherry-pick to main afterwards.

## Playtest checklist (#175) — sign off before public release

- [ ] 4 humans, LAN, full Classic FFA match to completion (no voice-chat debugging needed)
- [ ] One player force-killed mid-match → AI takes over → reclaim within 60s works
- [ ] Room-code join works from a second physical machine (not loopback)
- [ ] Tutorial completed by someone who has never seen the game (target: < 5 min)
- [ ] Turkish UI pass: switch language, play a full match, no clipped/missing strings
- [ ] Fresh Windows account: extract zip, run, host, exit — no crashes, settings persist

### Automated equivalents (v1.2, #175)

Four humans in one room on the same LAN couldn't be scheduled for this pass, so each
checklist item below was covered as far as automation reasonably can — genuinely
human-only items are called out, not silently skipped:

| Checklist item | Automated coverage | Still needs a human |
|---|---|---|
| 4-player full match to completion | `toy_net_test`'s "4-seat loopback stress" runs 4 independent client connections through a full match over real TCP sockets (loopback, not physical NICs) | Real NIC/router/Wi-Fi behavior, physical-machine timing |
| Force-kill → AI takeover → reclaim ≤60s | `toy_net_test`'s "mid-match drop + reclaim" case kills a client socket mid-match and asserts AI takes the seat, then reclaims it before the token deadline | — (this one is fully exercised programmatically) |
| Room-code join from a second physical machine | `toy_net_test`'s "LAN beacon + room code" covers the UDP beacon + code-matching logic over loopback | Actually two separate PCs on Wi-Fi/Ethernet — beacon broadcast behavior varies by router/switch and can't be simulated on one host |
| Tutorial completed by a first-timer, < 5 min | — | Genuinely human: "is this confusing" isn't something a script can judge |
| Turkish UI pass, no clipped strings | `i18n` message-key parity is checked at load (missing keys fall back loudly, not silently), and the DE/IT stub work (#157) reused the same check | Visual clipping at real window sizes/fonts still needs eyes on screen |
| Fresh account, extract/run/host/exit, settings persist | `settings.cpp`'s load/save round-trip is exercised by every headless test run (each spawns with a clean settings file); the useCustomHex/detailedSoldiers persistence bug found and fixed this cycle was caught this way | A literal fresh Windows user account — can't fake first-run OS state from CI |

Net effect: the mechanical/determinism half of this checklist (drop handling, reclaim
timing, beacon matching, 4-seat correctness) has automated regression coverage that
runs on every `verify.ps1` pass. The human-judgment half (does it feel confusing, does
text clip on a real screen, does it work on an actual fresh OS install) is unchanged —
still open, still needs a real playtest session before a marketing push.

## Known issues at v1.2.0 (#176) — ≤5 non-blockers

1. Sniper win rate sits at the top of the target band (56%, down from ~59–60% at
   v1.0.0 but still the high side) — watch, don't touch (freeze).
2. macOS builds and runs the full headless gate suite in CI (#161) but has never been
   smoke-tested on real Apple hardware — no physical Mac available this cycle.
3. Window resolution changes need an app restart (sokol swapchain limitation).
4. Photo mode in a networked match hides UI but cannot freeze the host's simulation.
5. Beacon discovery may need a firewall allow-rule on first launch (UDP 27124).

## v1.2.1 buffer list — pre-prioritized, fix-only

1. Any crash from the field (minidumps land in `%APPDATA%/toy-soldiers/crashes/`)
2. Firewall UX: detect blocked UDP beacon and show a hint instead of an empty LAN list
3. History file growth cap (currently unbounded append; trim to last 200 lines)
4. i18n stragglers reported by TR players (now also DE/IT stub-quality strings, #157)

## v1.2 delivered (was the v1.1 teaser, #197)

Everything the v1.1 release notes teased as "no promises" shipped this cycle:
spectator seats (#111), replay export (`.toyrec`, #107), snapshot compression + delta
snapshots (#95/#96), host migration lobby-lite and mid-match (#101/#102), and a
transport seam for a future relay backend (#89 — the seam is real and wired in, the
relay backend behind it is not, since no relay service exists to build against; see
`docs/WAN_PLAY.md`). The community card-proposal pipeline (`data/cards.json` as the
modding surface) predates this and is unchanged.

## v1.3 teaser — no promises

- A real relay backend behind the `net::ITransport`-style seam from #89, if a relay
  service (Steam Networking Sockets or self-hosted) ever gets stood up.
- Real Apple hardware to actually smoke-test the macOS/Metal build instead of just
  CI-verifying it compiles and passes headless gates.
- Store submission follow-through for the MSIX/Steam-capsule scaffolding built this
  cycle (#167/#182) — both need a real developer/publisher identity, not more code.
- Community card-proposal pipeline actually receiving submissions via the GitHub issue
  template (#152) added in v1.0.

## Support (#195)

- Bugs: https://github.com/cagatay-softgineer/toy-soldiers/issues (template: version.json
  contents + session log + minidump if any)
- Community: Discord invite TBD (placeholder in credits)
