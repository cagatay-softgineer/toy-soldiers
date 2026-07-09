# Roadmap — Oyuncak Asker Masa Savaşı → **v1.0**

**Product:** childhood plastic toy soldiers on a living-room table — turn-based party strategy with cards, towers, maps, events, host-lobby multiplayer, Box3D physics.  
**Today:** concept-board **MVP complete** (internal M0–M5) → versioned here as **v0.5**.  
**Target:** **v1.0** shippable jam→product release (Windows first; itch/Steam-ready).

This document defines **each +0.1 version**: general purpose, supporting goals, and a large concrete feature backlog (**180 numbered updates**). Items are intentionally implementable engineering/design tasks, not slogans.

---

## Version ladder (overview)

| Version | Codename | General purpose | Theme in one line |
|--------:|----------|-----------------|-------------------|
| **v0.5** | *Tabletop MVP* | Prove the fantasy is fun | Hotseat + host lobby + maps/events/cosmetics |
| **v0.6** | *Solid Core* | Make every session feel intentional | UX shell, rules clarity, save/settings, QA baseline |
| **v0.7** | *Deep Toybox* | Expand strategic depth without bloat | Cards, towers, modes, AI, event cards |
| **v0.8** | *Reliable Party* | Multiplayer you can trust on LAN/online | Sync, reconnect, lobby UX, anti-desync |
| **v0.9** | *Identity & Content* | Own look, sound, and content volume | Art pass, audio, maps, cosmetics, localization TR/EN |
| **v1.0** | *Ship Toy Soldiers* | Release-ready product | Installers, store page, tutorials, balance freeze, ship checklist |

**Progress model per version**

1. **General purpose** — why this release exists  
2. **Primary goals** — 3–6 outcomes that define “done”  
3. **Workstreams** — other purposes (design / eng / net / content / QA / live-ops)  
4. **Feature list** — concrete updates (IDs stable for tracking)

**Priority tags:** `P0` must ship in that version · `P1` should · `P2` stretch if time.

---

## v0.5 — Tabletop MVP *(shipped)*

### General purpose
Validate the core fantasy: **card orders + towers + table physics + host lobby** in a playable prototype.

### Primary goals (done)
- Local hotseat match loop  
- Host-authoritative multiplayer  
- Maps + controlled world events  
- Cosmetics without power creep  
- Balance pass + basic polish  

### Inventory (reference only — not re-counted as backlog)

| Area | Status |
|------|--------|
| Rules engine (attack/defense/tactic) | Done |
| Box3D table / soldiers / impulses | Done |
| TCP host lobby + snapshots | Done |
| 3 maps, 4 events, cosmetics | Done |
| Headless tests + balance report | Done |

---

## v0.6 — Solid Core

### General purpose
Turn the jam prototype into a **stable daily-driver app**: clear UI, settings, persistence, consistent rules messaging, and automated regression so later content does not break the floor.

### Primary goals
1. Players understand the game in under 2 minutes without reading source.  
2. Settings/preferences persist across launches.  
3. CI or local one-command verify for all smoke tests.  
4. No silent illegal plays; all host rejections are human-readable.  
5. Match setup flow feels like a product lobby, not debug panels.

### Other purposes / workstreams
| Workstream | Purpose |
|------------|---------|
| **UX shell** | Replace “debug ImGui sprawl” with guided screens |
| **Rules clarity** | Tooltips, history, illegal-move reasons |
| **Persistence** | Player profile name, cosmetics, last host IP |
| **Engineering hygiene** | Build, tests, logging, assert discipline |
| **Accessibility (light)** | Readable UI scale, colorblind-friendly seat colors |

### Features (v0.6)

#### A. App shell & navigation
1. `P0` Main menu: Play Offline / Host / Join / Settings / How to Play / Quit  
2. `P0` Screen state machine: Menu → Lobby → Match → Results → Menu  
3. `P1` Pause overlay in-match (rules reminder, resign, leave lobby)  
4. `P1` Consistent window title + app icon  
5. `P1` “Continue last mode” shortcut on menu  
6. `P2` Credits screen (Box3D, sokol, imgui, team)  

#### B. Onboarding & copy
7. `P0` First-run tutorial rewritten as 5 short steps with illustrations  
8. `P0` In-match help always one click away  
9. `P0` Illegal play messages: “Not your turn”, “Sandstorm: adjacent only”, “Target eliminated”  
10. `P1` Card tooltips always show full effect + keywords  
11. `P1` Glossary: Shield, Adjacent, World Event, Host  
12. `P1` TR + EN strings for all player-facing UI (data-driven)  
13. `P2` Optional coach tips first 3 games only  

#### C. Settings & persistence
14. `P0` Settings: display name, default port, auto-orbit, master volume placeholder  
15. `P0` Save last join IP / last map / last cosmetics  
16. `P1` Settings: fullscreen, VSync, resolution list  
17. `P1` Settings: show FPS, show sync generation (debug toggle)  
18. `P1` Reset settings to defaults  
19. `P2` Export/import settings JSON  

#### D. Match flow polish
20. `P0` Explicit “End of turn” banner for 0.8s  
21. `P0` Auto-select first legal target every turn  
22. `P1` Confirm resign / leave match  
23. `P1` Rematch keeps cosmetics + map, new seed  
24. `P1` Post-match “Play again” / “Change loadout” / “Menu”  
25. `P2` Match timeline scrubber (read-only log)  

#### E. Engineering & QA
26. `P0` `scripts/verify.ps1` runs sim + event + cosmetic + net + balance gates  
27. `P0` Fix protocol version display in lobby (v4+)  
28. `P1` Structured log file `logs/session-*.txt` (no secrets)  
29. `P1` Assert-free Release; soft fail + toast on client  
30. `P1` Golden seed replay for determinism of rules (no physics)  
31. `P2` AddressSanitizer Debug preset docs  
32. `P2` Crash handler with minidump path (Windows)  

#### F. Accessibility & comfort
33. `P1` UI scale 100/125/150%  
34. `P1` High-contrast ImGui theme option  
35. `P1` Seat colors not only red/green dependency  
36. `P2` Reduced motion (disable auto-orbit, toasts shorter)  

**v0.6 feature count: 36**

---

## v0.7 — Deep Toybox

### General purpose
Increase **strategic depth and replay** so sessions stay interesting after the novelty of physics and cosmetics fades—without exploding rules complexity.

### Primary goals
1. At least **3 tower types** and **25+ unique cards** with clear niches.  
2. Optional **modes** (FFA, 2v2 teams, short match).  
3. AI that is fun at Easy/Normal/Hard.  
4. Player-facing **event cards** (controlled) + ambient events.  
5. Targeting rules selectable: free / adjacent-only / range icons.

### Other purposes / workstreams
| Workstream | Purpose |
|------------|---------|
| **Card design** | Expand kit identity, rarities, counters |
| **Modes** | Different session lengths & social shapes |
| **AI** | Replace “greedy autoplay” with personalities |
| **Events 2.0** | Player agency + ambient drama |
| **Physics toys** | More readable table chaos without softlocks |

### Features (v0.7)

#### A. Tower & identity
37. `P0` Third tower type: **Shield Bearer** (starts with 1 shield, −1 atk)  
38. `P0` Tower pick UI with stats comparison  
39. `P1` Fourth tower: **Sapper** (first attack each game +1)  
40. `P1` Tower passive tooltips  
41. `P2` Tower unique starter card (1 card injected into deck)  

#### B. Card system expansion
42. `P0` Keyword system: `Shield`, `Pierce`, `Draw`, `Heal`, `AdjacentOnly`, `AOE`  
43. `P0` +8 new cards across rarities (total catalog ≥ 22 defs)  
44. `P0` Card that attacks **all adjacent** enemies for 1  
45. `P1` Card that steals 1 shield or cleanses own negative  
46. `P1` Card that swaps initiative (next player skip — carefully capped)  
47. `P1` Deck builder lite: ban 2 cards / add 2 from sideboard  
48. `P1` Hand limit enforcement UX (“discard down to 8”)  
49. `P1` Reveal “public last played card” banner  
50. `P2` Foil/legendary VFX on card buttons  
51. `P2` Card codex browser offline  

#### C. Modes
52. `P0` Mode: **Classic FFA** (current)  
53. `P0` Mode: **Quick Duel** 1v1, 24 HP towers, 15-card decks  
54. `P1` Mode: **2v2 Teams** shared win condition  
55. `P1` Mode: **Hot Potato King** — one “crown” player takes +1 dmg  
56. `P1` Turn timer optional (30/45/60s → auto-skip)  
57. `P2` Mode: **Sandbox** free spawn events + infinite HP practice  

#### D. AI
58. `P0` AI difficulty: Easy (random legal) / Normal (current+) / Hard (lookahead 1)  
59. `P1` AI personalities: Aggro, Turtle, Chaos  
60. `P1` AI emotes/log lines (“AI-Green digs in.”)  
61. `P1` AI never softlocks (always end turn if no plays)  
62. `P2` AI training via self-play metrics export  

#### E. Events 2.0
63. `P0` Event **cards** class (1–2 in deck max): player-cast ambient effects  
64. `P0` Event card: “Call the Cat” (physics knock, 0 tower dmg)  
65. `P1` Event card: “Pocket Sand” (force adjacent for everyone 1 turn)  
66. `P1` Ambient event: **Dog** (pushes all soldiers inward)  
67. `P1` Ambient event: **Blackout** (hide enemy HP 1 turn — UI fog)  
68. `P1` Event history panel with icons  
69. `P2` Map boss event (rare, telegraphed 2 turns)  

#### F. Targeting & board rules
70. `P0` Lobby toggle: Free targeting vs Adjacent-only  
71. `P1` Range indicator on table (highlight legal towers)  
72. `P1` “Cannot target self” already — add “Cannot target teammate” in 2v2  
73. `P2` Line-of-sight blocked by flood zone (advanced)  

#### G. Physics readability
74. `P1` Slow-mo 0.3s on tower destroy  
75. `P1` Camera punch on big hits (≥5 dmg)  
76. `P1` Reset fallen soldiers to “parade rest” each round start (optional ruleset)  
77. `P2` Record short GIF of last 3 seconds of chaos  

**v0.7 feature count: 41** (IDs 37–77)

---

## v0.8 — Reliable Party

### General purpose
Make multiplayer the **default social experience**: easy lobby, stable sync, reconnect, and clear authority—so “4 friends on LAN / VPN” just works.

### Primary goals
1. Join via **room code** (not raw IP) on LAN; optional internet relay later.  
2. Host disconnect → graceful end or migration path (even if v1 is “end match cleanly”).  
3. Desync detection + automatic resync snapshot.  
4. Spectator / late join lobby before start.  
5. Network metrics visible when things fail.

### Other purposes / workstreams
| Workstream | Purpose |
|------------|---------|
| **Lobby product** | Party UX, readiness, kick, chat |
| **Transport** | Robust sockets, timeouts, NAT guidance |
| **Authority** | Hard validation, anti-cheat lite |
| **Resilience** | Reconnect, host loss, version mismatch |
| **Observability** | Net graph, sequence numbers, logs |

### Features (v0.8)

#### A. Lobby UX
78. `P0` Lobby screen redesign: seat cards with ready checkmarks  
79. `P0` Host can kick seat / open-close lobby  
80. `P0` Ready-up required for humans (AI always ready)  
81. `P1` Lobby chat (text, rate-limited)  
82. `P1` Copy “join command” / room code to clipboard  
83. `P1` Show ping / last packet age per client  
84. `P2` Custom lobby banner color  

#### B. Discovery & addressing
85. `P0` **Room code** mapping: host broadcasts LAN beacon (UDP)  
86. `P0` Join UI: code field + recent hosts list  
87. `P1` Manual IP advanced collapse  
88. `P1` Protocol version mismatch hard reject with upgrade message  
89. `P2` Internet: optional Steam Networking / free relay plugin interface  
90. `P2` QR code for room code (local network)  

#### C. Sync robustness
91. `P0` Snapshot sequence numbers; client ignores older  
92. `P0` Client requests full resync if gap detected  
93. `P0` Host heartbeat every 1s; client timeout 8s → disconnect UI  
94. `P1` Intent queue ack (PlayCard id) to avoid double-spend  
95. `P1` Compress snapshots (lz4/zlib) if >2KB  
96. `P1` Delta snapshots (only dirty players)  
97. `P2` Lockstep optional mode for pure determinism experiments  

#### D. Reconnect & host loss
98. `P0` Soft disconnect: “Host lost — return to menu” with reason  
99. `P1` Client reconnect token for same match if host still up (lobby only)  
100. `P1` Mid-match reconnect: host keeps seat 60s, then AI takes over  
101. `P1` Host migration **v1-lite**: elect new host only in lobby (not mid-match)  
102. `P2` Full mid-match host migration with snapshot handoff  

#### E. Fairness & validation
103. `P0` Host rejects out-of-turn / bad indices (already) — surface toast always  
104. `P1` Server-side deck hash verify at match start  
105. `P1` Seed commit-reveal optional for trust (host publishes seed hash pre-match)  
106. `P2` Simple rate limit on intents  
107. `P2` Replay file export `.toyrec` for dispute review  

#### F. Social quality
108. `P1` Emote wheel: salute / laugh / angry (log + optional sound)  
109. `P1` “Rematch votes” 3/4 must accept  
110. `P1` Friend list local (saved names + last codes)  
111. `P2` Spectator mode (read-only snapshots)  
112. `P2` Twitch-friendly larger log font  

#### G. Net QA
113. `P0` Expand `toy_net_test` for disconnect mid-lobby  
114. `P0` Chaos test: drop 5% packets simulation  
115. `P1` 4-client loopback stress (4 processes) script  
116. `P2` Wireshark docs for protocol  

**v0.8 feature count: 39** (IDs 78–116)

---

## v0.9 — Identity & Content

### General purpose
Give the game a **memorable toybox identity**: art direction, audio, more maps/cosmetics, TR/EN polish—so it feels like a product, not a tech demo.

### Primary goals
1. Signature visual style (plastic, felt, wood, warm light).  
2. Full **SFX + music** loops (or procedural-lite).  
3. ≥ **6 maps**, ≥ **30 cosmetics**, ≥ **30 cards**.  
4. TR/EN complete for all UI + cards.  
5. Performance budget: 60 FPS on mid-range Windows laptop.

### Other purposes / workstreams
| Workstream | Purpose |
|------------|---------|
| **Art direction** | Cohesive toy aesthetic |
| **Audio** | Feedback for every important action |
| **Content volume** | Maps, events, cosmetics, cards |
| **Narrative flavor** | Toy catalog lore, not war realism |
| **Perf & rendering** | Better draw path without losing simplicity |

### Features (v0.9)

#### A. Rendering upgrade (still sokol-friendly)
117. `P0` Instanced cube rendering batch  
118. `P0` Simple directional light + ambient (plastic shading)  
119. `P1` Soft shadows (projection or blob)  
120. `P1` Table felt texture (tiling)  
121. `P1` Outline on selected tower/target  
122. `P2` Screen-space AO lite  
123. `P2` Optional GLTF soldier mesh (1 LOD)  

#### B. Art & animation
124. `P0` Soldier idle bob / win cheer pose  
125. `P0` Tower damage flash + crack decal stages (0/50/25% HP)  
126. `P1` Card play arc animation (UI)  
127. `P1` Event VFX: rain particles, sand mist, flood plane, cat silhouette  
128. `P1` Victory confetti (plastic beads)  
129. `P2` Intro camera fly-over table  

#### C. Audio
130. `P0` SFX: card play, damage, shield, draw, tower destroy  
131. `P0` SFX: UI click, illegal buzz, your-turn chime  
132. `P1` Event stingers (sandstorm/rain/flood/cat)  
133. `P1` Music: menu loop + match loop + results sting  
134. `P1` Per-category volume sliders  
135. `P2` Spatial SFX from seat positions  

#### D. Content: maps & rooms
136. `P0` Map: **Kids Bedroom** (bunk bed props, night light)  
137. `P0` Map: **Garage Workbench** (tools, oil stains events)  
138. `P1` Map: **Picnic Blanket** (ants event)  
139. `P1` Map: **Christmas Table** (seasonal cosmetics unlock)  
140. `P1` Per-map loading blurb + recommended events list  
141. `P2` User map color themes (felt dye)  

#### E. Content: cosmetics
142. `P0` +10 plastic colors (transparent jelly, gold chrome fake)  
143. `P0` +4 tower skins (Lego-ish, tin can, book stack, dice tower)  
144. `P0` +6 accessories (pirate hat, chef hat, antenna, cape, backpack, flag)  
145. `P1` Cosmetic sets with names (“Birthday Platoon”)  
146. `P1` Unlock track: wins / matches played (offline local)  
147. `P2` Import custom color hex  

#### F. Content: cards & balance ops
148. `P0` Card data driven from `data/cards.json`  
149. `P0` Hot-reload cards in Debug  
150. `P1` Balance dashboard CSV export from `toy_balance_report`  
151. `P1` Seasonal rotating “featured deck modifier”  
152. `P2` Community card proposals template  

#### G. Localization & narrative
153. `P0` Complete Turkish UI + card text  
154. `P0` Complete English UI + card text  
155. `P1` Locale switch live in settings  
156. `P1` Flavor text for maps/events (toy catalog voice)  
157. `P2` German/IT stubs if Erasmus narrative needed  

#### H. Performance & platforms
158. `P0` Profile frame time; cap particle/prop counts  
159. `P1` Linux build preset verified  
160. `P1` High-DPI font scaling fixes  
161. `P2` macOS Metal path smoke (if machine available)  

**v0.9 feature count: 45** (IDs 117–161)

---

## v1.0 — Ship Toy Soldiers

### General purpose
Ship a **release-quality v1.0**: installable build, store presence, frozen balance, complete tutorial, known-bug budget zero for blockers, and a launch checklist the team can execute.

### Primary goals
1. One-click installer / zip portable with README.  
2. Tutorial + practice AI match mandatory path optional skip.  
3. Balance freeze + patch notes for v1.0.0.  
4. itch.io (and optional Steam page) assets ready.  
5. Support path: logs, known issues, version info.

### Other purposes / workstreams
| Workstream | Purpose |
|------------|---------|
| **Release engineering** | Packaging, versioning, symbols |
| **Store & marketing** | Trailer, screenshots, page copy TR/EN |
| **Player support** | Bug template, FAQ |
| **Legal** | Licenses, third-party notices |
| **Launch ops** | Day-0 patch readiness |

### Features (v1.0)

#### A. Packaging & versioning
162. `P0` Semantic version in UI + `version.json`  
163. `P0` Windows portable zip + installer (Inno/NSIS)  
164. `P0` Ship third-party licenses file  
165. `P0` Strip debug asserts; ship pdb optional archive  
166. `P1` Auto-update check (optional URL; fail open)  
167. `P2` Microsoft Store packaging experiment  

#### B. Tutorial & new player
168. `P0` Guided first match vs 1 AI (scripted turns)  
169. `P0` Practice sandbox unlock after tutorial  
170. `P1` Challenge missions: “Win under sandstorm”, “Heal 10 total”  
171. `P1` Achievement-like local badges (10 wins, host 5 lobbies)  
172. `P2` Interactive rules comic (static panels)  

#### C. Balance freeze & QA
173. `P0` Freeze card numbers; only bugfixes after tag  
174. `P0` Full regression: unit + net + 200-seed balance report  
175. `P0` Playtest checklist (4 humans LAN) signed off  
176. `P1` Known issues list ≤5 non-blockers  
177. `P1` Fuzz illegal network packets (host must not crash)  
178. `P2` External playtest feedback form  

#### D. Store & presentation
179. `P0` 5 key screenshots (lobby, match, event, cosmetics, win)  
180. `P0` 30–60s gameplay trailer script + capture  
181. `P0` itch.io page TR/EN  
182. `P1` Steam store capsule art set (even if wishlists only)  
183. `P1` Press kit zip  
184. `P2` TikTok/Shorts vertical capture presets  

#### E. Meta systems (light, shippable)
185. `P0` Local profile stats: matches, wins, favorite map  
186. `P1` Match history last 20 (offline file)  
187. `P1` Photo mode freeze + hide UI  
188. `P2` Shareable result card image export  

#### F. Safety & legal
189. `P0` Content rating notes (toy violence, no gore)  
190. `P0` Privacy: no telemetry by default; optional anonymous crash  
191. `P1` EULA/Terms stub for store  
192. `P1` Accessibility statement (partial)  

#### G. Launch ops
193. `P0` `CHANGELOG.md` from v0.5→v1.0  
194. `P0` Day-0 hotfix branch policy  
195. `P0` Support email / Discord invite placeholder  
196. `P1` Post-launch v1.0.1 buffer list (pre-prioritized)  
197. `P2` Roadmap teaser for v1.1 (no promises binding)  

#### H. Final content gates
198. `P0` Minimum content checklist: 6 maps OR 3 maps fully polished + 3 cosmetics sets  
199. `P0` Audio ducks correctly when app loses focus  
200. `P0` “Complete install smoke”: clean PC run, host+client, full match, exit 0  

**v1.0 feature count: 39** (IDs 162–200)

---

## Optional v1.1+ backlog (out of v1.0 scope, for later)

Not counted in the 200, but parked:

- Dedicated matchmaking server  
- Cross-play mobile spectator  
- UGC card workshop  
- Full 3D authored characters  
- Ranked ladder  
- Cloud saves  

---

## Totals

| Version | Feature IDs | Count |
|---------|-------------|------:|
| v0.6 Solid Core | 1–36 | 36 |
| v0.7 Deep Toybox | 37–77 | 41 |
| v0.8 Reliable Party | 78–116 | 39 |
| v0.9 Identity & Content | 117–161 | 45 |
| v1.0 Ship | 162–200 | 39 |
| **Total planned updates** | **1–200** | **200** |

*(v0.5 MVP work is already shipped and listed separately; not double-counted.)*

---

## Cross-cutting purposes (apply every version)

These are not extra version numbers; they are **standing duties** for every +0.1:

| Standing purpose | What “good” looks like |
|------------------|------------------------|
| **Fun first** | A 15-minute session always ends with a story (cat, comeback, sniper clutch) |
| **Host authority** | Clients never invent game state; intents only |
| **No power creep cosmetics** | Fashion ≠ stats (hard rule through v1.0) |
| **Deterministic rules** | Same seed + intents → same winner (physics may diverge) |
| **Toy tone** | Never realistic military marketing; keep plastic/nostalgia |
| **Test gate** | `verify.ps1` green before tagging a version |
| **Ship small** | Prefer cut list over delayed release |

---

## Suggested sequencing inside a version (sprint recipe)

For each `v0.x`:

1. **Spec freeze (1–2 days)** — pick P0 subset (~12 items)  
2. **Vertical slice** — one player-visible path complete  
3. **Content + systems** — fill P0/P1  
4. **Playtest** — 4-player LAN or 4 AI stress  
5. **Bug/stability** — no open P0 bugs  
6. **Tag & notes** — `CHANGELOG` + balance snapshot  

---

## Tracking template (copy per feature)

```md
### #NNN Title
- Version: v0.x
- Priority: P0|P1|P2
- Workstream: UX | Rules | Net | Content | Audio | Art | QA | Release
- Depends on: #…
- Acceptance: …
- Status: todo | doing | done
```

---

## Milestone → version mapping (history)

| Concept board / internal | Version |
|--------------------------|---------|
| M0 Rules lock | v0.1 era |
| M1 Hotseat | v0.2–0.3 era |
| M2 Host lobby | v0.3–0.4 era |
| M3 Maps & events | v0.4 |
| M4 Cosmetics & rooms | v0.5 |
| M5 Balance & polish | **v0.5 (MVP tag)** |
| Next engineering | **v0.6 Solid Core** → … → **v1.0** |

---

## One-page “what is v1.0?” definition

**v1.0 is done when** a new player can install on Windows, learn in-app, complete a match vs AI, host a 4-seat lobby for friends on LAN, enjoy maps/events/cosmetics/audio, and leave without needing a developer on voice chat—and the team has green automated tests, a changelog, and a store page.

Until then, ship intermediate `v0.x` tags with honest notes.

---

*Document version: 2026-07-09 · Product: toy-soldiers · Engine: Box3D · Live with `docs/GDD.md` + concept board.*
