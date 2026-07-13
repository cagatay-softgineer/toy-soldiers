# Changelog

All notable changes to **Oyuncak Asker Masa Savaşı (toy-soldiers)** are documented here.

## [Unreleased]

### Planned
- See the "v1.3 teaser" section of [docs/RELEASE.md](docs/RELEASE.md) — no promises,
  mostly gated on external prerequisites (a real relay backend, real Apple hardware,
  a Store/Partner Center identity) rather than more code.

## [1.2.0] — 2026-07-13 · "Reinforcements"

Cleared the entire v1.1 post-launch backlog: 25 deferred roadmap items implemented for
real (not stubbed), plus meaningful automated progress on the 4 remaining human-only
launch tasks. **Balance freeze reset at this tag** — two new rule-affecting mechanics
landed, revalidated with a fresh 200-trial report before shipping (see docs/RELEASE.md).

### Added — rules & content (#69/#73/#151/#157/#147/#135/#122/#123)
- **Titan boss event** (#69): rare, telegraphed 2 turns ahead, map-boss-scale world
  event for wave-1 tables
- **Flood line-of-sight** (#73): flood zones now block targeting in addition to their
  existing focus-seat HP leak, layering with Sandstorm's adjacent-only rule
- **Seasonal featured deck modifier** (#151): rotating weekly ban/add tweak surfaced in
  the lobby deck-tweaks panel
- **German/Italian locale stubs** (#157) for the Erasmus+ narrative track
- **Custom hex color import** (#147): local-only plastic override beyond the preset swatches
- **Spatial SFX** (#135): seat-position-aware stereo panning for hit/event audio
- **Screen-space AO lite** (#122) and **optional detailed procedural soldiers** (#123,
  render-only toggle, no gameplay effect)

### Added — protocol v7 (#111/#95/#96/#101/#102/#84)
- **Spectator mode** (#111): join read-only via a `spectate` Hello flag; host assigns
  `kSpectatorSeat` instead of a player seat and rejects any action message from one
- **Snapshot compression** (#95) and **delta snapshots** (#96): `CompressedState`/
  `DeltaState` message types (lz4, dirty-player-bitmask deltas with automatic resync
  fallback on a generation mismatch) — see docs/PROTOCOL.md for the full framing
- **Host migration**: lobby-only election (#101) and mid-match snapshot handoff with a
  name-reattach window (#102), reusing `Hello`/`Welcome` rather than new wire messages
- **Lobby banner color** (#84): host-picked, broadcast to everyone, shown as a strip in
  the lobby UI

### Added — capture & replay (#77/#97/#107/#179/#188/#184)
- **`.toyrec` replay export** (#107): records the action sequence (not AI decisions) for
  dispute review; verified deterministic via a from-scratch RNG-stream separation fix
  (`match.rng` for rule-affecting draws vs. `match.aiRng` for AI-decision-only noise) —
  a replay re-applies recorded actions directly and reproduces the original match
  bit-for-bit without ever running the AI
- **Lockstep determinism test** (#97): two independent `Match` instances compared after
  every single action across 4 seeds × 4 modes (16/16 passing)
- **GIF burst capture** (#77, `G` hotkey) and **9:16 vertical presets** for TikTok/
  Shorts/Reels (#184, `Shift+G`, plus a one-shot vertical PNG helper) — same rolling
  ~3s frame ring, center-cropped
- **Automated press-tour** (#179, `--press-tour` CLI flag): scripts a full lobby → match
  → event → cosmetics → victory flow and captures all 5 storefront screenshots via
  `PrintWindow(..., PW_RENDERFULLCONTENT)` (proactively used over `BitBlt`-from-`GetDC`,
  which is unreliable against D3D11 flip-model swapchains)
- **Shareable result-card PNG export** (#188) on the results screen

### Added — packaging & store assets (#90/#89/#161/#167/#172/#182/#181/#175)
- **QR code for the room code** (#90): rendered as a live sokol-gfx texture in the host
  lobby (vendored `qrcodegen`)
- **Transport seam for a future relay backend** (#89): `net::transportConnect()` is now
  the single call site `joinLobby` uses to establish its connection; a relay backend
  plugs in as one new branch with no other code changes. No relay service exists to
  build against yet, so selecting it fails loudly rather than pretending to work — see
  docs/WAN_PLAY.md for the direct-connect/port-forward path that works today
- **macOS CI job** (#161): builds the full Metal/AppKit GUI target and runs the headless
  gate suite on `macos-latest`. Required real fixes, not just a new workflow file: a
  separate Objective-C `sokol_impl.m` (sokol's Metal/AppKit backend needs Objective-C,
  which a plain `.c` file can't provide) and linking the `AudioToolbox` framework for
  `sokol_audio.h`'s macOS backend
- **MSIX packaging experiment** (#167): a real Desktop Bridge manifest + packing
  pipeline (`scripts/package-msix.ps1`) — verified by actually producing an installable
  `.msix` via `makeappx`, not just writing the manifest
- **Interactive rules comic** (#172): `docs/rules-comic.html`, 6 static SVG panels with
  click/arrow-key navigation
- **Steam capsule art set** (#182): all 4 store-page sizes (main/header/small/vertical),
  SVG source + exact-pixel PNG renders, verified pixel-dimension-exact
- **Automated equivalents for the playtest checklist** (#175): mapped each checklist
  item to existing automated coverage (net-test loopback stress, reclaim timing, beacon
  matching, settings round-trip) versus what's genuinely still human-only — see
  docs/RELEASE.md. The real 4-human LAN session itself did not happen this cycle.
- itch.io page copy (#181) and the trailer script (#180) were already complete from the
  v1.0 cycle — only needed closing, not new work.

### Fixed
- **Settings persistence bug**: `useCustomHex`/`customHex`/`detailedSoldiers` were
  parsed on load but never written on save — silently reset every restart
- Dead/unused `Gdiplus::Bitmap` construction and unused `grabClient` output params in
  the capture path

## [1.1.0] — 2026-07-12 · "Encore"

Issue-tracker sweep: every remaining roadmap item was audited — ~70 already-shipped
issues closed with implementation notes, 16 genuinely-future items moved to the v1.1
milestone with reasons, and the 9 finishable ones landed here. **No balance numbers
changed** (v1.0 freeze holds).

### Added
- **Trust lines (#104/#105):** match start logs `Trust: seed commit <fnv> — deck check
  0:<h> 1:<h>…` and game over reveals `Trust: seed was <seed>` — every client can verify
  the host never swapped seeds or stacked decks (order-independent FNV-1a fingerprints,
  covered by mode tests)
- **Large battle-log text (#112):** stream-friendly 1.35× log font toggle in settings
- **Felt dye (#141):** local-only table tint (Crimson / Royal Blue / Charcoal / Cream /
  Mint) in settings — display-only, never networked
- **Card-play arc (#126):** the last-played-card banner now arcs up from the hand area
  (ease-out, skipped under reduced motion)
- **Protocol reference (#116):** docs/PROTOCOL.md — framing, full message table, beacon
  format, trust-line verification, Wireshark recipes
- **Community templates:** card-proposal form mirroring the cards.json schema (#152) and
  a playtest-feedback form (#178) as GitHub issue templates
- **Linux verified in CI (#159):** new ubuntu job builds the full game (X11/GL/ALSA deps)
  against the pinned Box3D and runs all 8 headless gates on every push

## [1.0.0] — 2026-07-12 · "Ship Toy Soldiers"

**Balance freeze (#173):** card numbers, tower stats, and event weights are frozen.
Only bug fixes after this tag (policy: docs/RELEASE.md).

### Added — v1.0 release engineering (#162-#166)
- Single-source version (`src/app/version.h`) shown in title bar, menu, credits, and
  written to `version.json` inside every package
- `scripts/package.ps1`: portable win64 zip (exe + cards.json + licenses + README +
  version.json), optional symbols archive, and a `-Smoke` mode that extracts the zip to a
  clean temp dir and verifies the game boots (#200 — passing)
- Inno Setup installer script (`scripts/installer.iss`, EN/TR) as the one-click option
- `THIRD_PARTY_LICENSES.md` (Box3D MIT, sokol zlib, Dear ImGui MIT) ships in the package
- "Project page / check updates" button in credits (fail-open, opens the browser)

### Added — new-player path (#168-#169)
- **Guided tutorial**: 7-step overlay over a real Quick Duel vs a gentle Easy bot
  ("Sgt. Bumble") — auto-advances when you play your first card, completes win or lose
- Sandbox mode is locked until the tutorial is done (soft gate with hint)

### Added — meta systems (#170-#171, #185-#187)
- **Profile & Badges** screen: matches/wins/favorite map, six local badges
  (Graduate, First Blood, Veteran, Regular, Devoted, Party Host)
- Three challenge missions with live detection + completion toasts:
  Storm Winner, Field Medic (10+ HP healed), Untouchable (win at 75%+ HP)
- Match history: one line per finished match to `%APPDATA%/toy-soldiers/history.txt`,
  last 20 shown in the profile
- **Photo mode** (P): hides all UI; fully freezes the sim in offline matches
- Audio ducks to 15% when the window loses focus (#199), with a click-free glide

### QA & launch ops (#174-#177, #189-#197)
- Balance regression widened to 200 trials — 200/200 finished, avg 54.9 turns,
  sniper 56% win rate (mid-band), even seat spread
- Net fuzzer extended: 40 rounds of random-typed/garbage packets against a live host (#177)
- Launch docs: itch.io page copy EN/TR + screenshot shot list (#179/#181), 45s trailer
  script (#180), press kit (#183), playtest checklist (#175), known-issues list (#176),
  day-0 hotfix policy + v1.0.1 buffer + v1.1 teaser (#194/#196/#197),
  privacy/rating/EULA/accessibility statements (#189-#192) — see docs/store/, docs/RELEASE.md,
  docs/LEGAL.md

### Remaining human steps before store launch
- Capture the 5 screenshots + trailer per docs/store/
- Run the 4-human LAN playtest checklist (docs/RELEASE.md)
- Create the itch.io page and upload `dist/toy-soldiers-v1.0.0-win64.zip`

## [0.9.0-dev] — 2026-07-12

### Added — v0.9 Identity & Content
- **Rendering (#117-#121, #124-#125):** one instanced draw call for the whole table;
  directional + ambient "plastic" shading with a cheap sheen; blob shadows under towers and
  soldiers; pulsing ground ring on the selected target; soldier idle bob + victory cheer;
  tower damage flash and crack tiers (bruise <50%, char <25%); felt variation panels
  (checkered on the picnic blanket)
- **Audio (#130-#134):** fully procedural sokol_audio synth (no asset files) — card/damage/
  shield/draw/destroy SFX, UI click, illegal buzz, your-turn chime, victory fanfare; per-event
  stingers (meow, woof, rain patter, ants); menu lullaby + match march loops and a results
  sting from a tiny step sequencer; master/SFX/music sliders persisted in settings
  (sokol_audio.h vendored under `extern/sokol/`)
- **Maps (#136-#140):** Kids Bedroom (bunk bed, night light), Garage Workbench (pegboard
  tools, oil stain), Picnic Blanket (checker pattern, crumbs), Christmas Table (tree, gifts,
  candle) — 7 rooms total with per-map event weights, palettes, and toy-catalog flavor
  blurbs (#156); new **Ants** ambient event (jitters one seat's soldiers, zero damage)
- **Effects (#127-#129, #158):** capped 256-particle pool — rain droplets (snow on the
  Christmas table), sandstorm grit, flood splashes, ant trails, and a 120-bead victory
  confetti burst; intro camera fly-over on match start (respects reduced motion)
- **Cosmetics (#142-#146):** +10 plastics (jellies, fake chromes, glow mint…), +4 tower
  skins (Brick Stack, Tin Can, Book Stack, Dice Tower), +6 accessories (pirate/chef hats,
  antenna, cape, backpack, team flag) → 39 items; six named sets ("Birthday Platoon",
  "North Pole Guard"…); local unlock track driven by matches played / wins with the rule
  shown on locked entries
- **Cards (#148-#150, #153):** catalog grown to 30 defs (+Domino Push, Blanket Fort, Bubble
  Wrap, Tidy Up, Battle Plan, Glitter Bomb, Night Watch — sideboard material for deck
  tweaks); catalog is data-driven from `data/cards.json` with a safety floor (broken files
  can't nuke the game) and Debug hot-reload; complete Turkish card text with live-language
  lookup; balance report now exports `balance_report.csv` per trial
- **i18n (#153-#155):** remaining lobby/match/settings literals routed through the EN/TR
  dictionary; language still switches live in settings
- **QA:** new `toy_content_test` gate (catalog/TR text, JSON round-trip + safety, 7-map
  match sweep, ants, cosmetics counts, unlocks, snapshot) wired into `verify.ps1` —
  8 gates total, all green; graphical smoke-launch verified

### Notes
- Linux configure preset exists but remains unverified on this machine (#159)
- Deferred: real textures (#120), SSAO/GLTF (#122/#123), card-play arc UI anim (#126),
  seasonal deck modifier (#151), spatial SFX (#135), macOS smoke (#161)

## [0.8.0-dev] — 2026-07-11

### Added — v0.8 Reliable Party (protocol v6)
- **LAN discovery (#85/#86):** host broadcasts a UDP beacon (port 27124) with a 4-letter room code,
  host name, and seat counts; menu shows a live LAN game list, join-by-code field, and a
  recent-hosts list persisted in settings; manual IP moved behind an "advanced" collapse (#87)
- **Lobby product (#78–#82):** seat-card table with ready checkmarks, per-seat ping, and kick
  buttons; open/close lobby toggle (locked lobbies reject joins); ready-up required before start;
  room code display with copy-to-clipboard; rate-limited lobby chat + emote quick-buttons (#108)
- **Sync robustness (#91–#93):** clients ignore stale snapshots (matchId + syncGeneration);
  broken snapshots trigger an automatic full-resync request; host heartbeats every 1s and both
  sides drop silent peers after 8s with a human-readable reason (#98)
- **Reconnect (#99/#100-lite):** mid-match disconnect hands the seat to the AI and reserves it
  for 60s; rejoining with the same reconnect token (issued in Welcome) reclaims the seat live
- **Fairness & hygiene (#94, #103, #106, #88):** PlayCard carries an intent id (duplicate intents
  ignored — no double-spend); host rejections always carry the describeIllegalPlay reason;
  lightweight intent/chat rate limits; protocol version mismatch now hard-rejects with an
  "update the game" message on both sides
- **Rematch votes (#109):** rematch starts only when every connected human votes yes;
  host button shows the tally, clients get a vote button
- **Net QA (#113–#115):** toy_net_test expanded to 12 scenarios — beacon round-trip on loopback,
  version-mismatch reject, malformed-frame fuzz, lobby lock/kick/ready gate, mid-lobby disconnect
  frees the seat, mid-match drop → AI takeover → token reclaim, chat relay, ping measurement,
  unanimous rematch, and a 4-seat (host + 3 clients) loopback stress match

### Deferred (tracked for later versions)
- #95/#96 snapshot compression & deltas (needs a compression dep; snapshots are ~2.3 KB),
  #101/#102 host migration, #104/#105 deck hash & seed commit-reveal, #89/#90 relay/QR,
  #107 replay export, #111 spectator, #97 lockstep, #116 protocol docs

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
