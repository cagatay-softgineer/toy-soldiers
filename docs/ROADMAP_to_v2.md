# Roadmap — Oyuncak Asker Masa Savaşı → **v2.0**

**Product:** childhood plastic toy soldiers on a living-room table — turn-based party strategy with cards, towers, maps, events, host-lobby multiplayer, Box3D physics.
**Today:** **v1.2 "Reinforcements"** shipped (all 200 v1 roadmap items + protocol v7: spectator, `.toyrec` replay, snapshot compression/deltas, host migration, capture/press tooling).
**Target:** **v2.0 "Living Toybox"** — the game grows past *one LAN, one room* into a persistent, online, living product with ranked play, user content, real 3D toys, a solo campaign, and a second platform.

This document defines **each +0.1 version from v1.3 → v2.0**: general purpose, supporting goals, and a concrete feature backlog (**250 numbered updates, IDs 201–450**), continuing the stable ID scheme from `ROADMAP_to_v1.md` (IDs 1–200). Items are implementable engineering/design tasks, not slogans.

> **Why a major version.** v1.x proved and shipped the fantasy on **LAN + hotseat**. Everything below crosses a boundary the v1 line deliberately did not: **backend services, real accounts, persistent progression, UGC, authored 3D art, PvE, and mobile**. Each of those is a "you can't just add it in a patch" system — collectively they are v2.0. The original roadmap parked them verbatim as the "Optional v1.1+ backlog"; v2.0 is where they stop being parked.

---

## Version ladder (overview)

| Version | Codename | General purpose | Theme in one line |
|--------:|----------|-----------------|-------------------|
| **v1.3** | *Open Signal* | Play beyond the LAN | Real relay + matchmaking behind the #89 transport seam; join from anywhere |
| **v1.4** | *Persistent Toybox* | The toybox remembers you | Accounts, cloud saves, server-authoritative profiles & unlocks |
| **v1.5** | *Proving Ground* | Give competition a home | Ranked ladder, MMR, leaderboards, seasons v1 |
| **v1.6** | *Workshop* | Let players make toys | In-game UGC for cards / cosmetics / maps, sharing & moderation |
| **v1.7** | *Solid Cast* | Look like a real toybox, and broadcast it | Authored 3D soldiers/meshes, art upgrade, observer & caster tools |
| **v1.8** | *Solo Campaign* | A reason to play alone | PvE campaign, roguelike run mode, missions & bosses |
| **v1.9** | *Pocket Platoon* | Two platforms, one game | Mobile companion + cross-play spectator; Steam/Store launch follow-through |
| **v2.0** | *Living Toybox* | A product that stays alive | Live-ops seasons, cosmetic pass, launch, support & telemetry — the whole thing shipped |

**Progress model per version** (unchanged from v1)

1. **General purpose** — why this release exists
2. **Primary goals** — 3–6 outcomes that define "done"
3. **Workstreams** — other purposes (design / eng / net / backend / content / QA / live-ops)
4. **Feature list** — concrete updates (IDs stable for tracking, continuing from #200)

**Priority tags:** `P0` must ship in that version · `P1` should · `P2` stretch if time.

---

## Standing constraints carried from v1 (do not break in v2.0)

These graduate from the v1 "cross-cutting purposes" into **hard v2.0 acceptance gates**:

| Constraint | v2.0 meaning |
|---|---|
| **Host authority** | Even with a relay/matchmaker, the *match* stays host-authoritative. Backend brokers connections and stores metadata; it never simulates or arbitrates game state. |
| **No power creep** (Fashion ≠ stats) | Battle pass, ranked rewards, UGC cosmetics, and campaign unlocks are **cosmetic / flavor only**. No purchasable or grindable stat advantage — this is a launch blocker if violated. |
| **Deterministic rules** | Ranked integrity and replays depend on `match.rng` (rule draws) staying separated from `match.aiRng` (AI noise), the split shipped in v1.2. UGC and campaign must not reintroduce non-determinism into ranked. |
| **Toy tone** | Seasons, campaign story, and marketing keep the plastic/nostalgia voice. No realistic-military framing. |
| **Test gate** | `scripts/verify.ps1` (8 headless gates today) stays green before any tag. New systems add new gates, they don't skip the old ones. |
| **Privacy-first** | Telemetry opt-in only (v1.0 policy). Accounts collect the minimum; no chat/data resale; GDPR-style delete-my-account path. |
| **Offline still works** | A player with no network must still get hotseat + vs-AI + campaign. Online is additive, never a gate on the core loop. |

---

## v1.3 — Open Signal

### General purpose
Turn the **already-built** transport seam (#89, `net::transportConnect()`) into **real play from anywhere**: a relay/matchmaking backend so two people who are *not* on the same LAN can find each other and play, without port-forwarding or reading an IP over voice chat.

> Prerequisite honesty: v1.2 built the *seam* but deliberately shipped no relay service. **v1.3's headline work is standing that service up** — this is the first version that owns a backend deployment, not just a client.

### Primary goals
1. Two players on different networks complete a full match connected via relay — zero port-forwarding.
2. A **matchmaker** pairs "Quick Match" players by mode without exchanging IPs.
3. Relay path degrades gracefully: NAT-punch first, relay fallback, clear failure UI.
4. The backend is a deployable, versioned service with health checks and a staging env.
5. `net::ITransport` has ≥2 real implementations (direct LAN, relay) chosen at runtime.

### Workstreams
| Workstream | Purpose |
|------------|---------|
| **Backend service** | Relay + matchmaker + room registry (stateless brokering, no game sim) |
| **Transport** | Second `ITransport` impl behind the #89 seam; NAT traversal |
| **Client netcode** | Quick Match flow, region select, connection-state UX |
| **Observability** | Relay metrics, connection success rate, room lifecycle logs |
| **Security** | Rate limits, room-code entropy, no PII in transit beyond display name |

### Features (v1.3)

#### A. Backend service
201. `P0` Stand up a **relay service** (WebSocket/UDP relay) as a versioned, deployable process (Docker image + health endpoint)
202. `P0` **Room registry**: host registers a room → gets a short code / token; clients resolve code → relay endpoint
203. `P0` Backend **protocol-version handshake** (rejects mismatched clients with an upgrade message, mirroring the LAN path #88)
204. `P0` Staging + prod environments with a config-selectable backend URL (never hard-coded)
205. `P1` **Matchmaker** endpoint: queue by mode, pair 2–4 players, hand back a relayed room
206. `P1` Room **TTL + cleanup** (abandoned rooms expire; no leaks)
207. `P1` Backend **structured logs + metrics** (connections, relay bytes, room lifetime) — no chat content, no PII
208. `P2` Region hint (latency-based endpoint pick) — single region acceptable at first
209. `P2` Backend **admin kill-switch** to drain/close rooms for maintenance

#### B. Transport (client, behind the #89 seam)
210. `P0` Second `net::ITransport` implementation: **relay transport**, selected by `transportConnect()` — no changes to `joinLobby` call sites
211. `P0` **NAT-punch attempt first**, relay fallback second; both transparent to `session.cpp`
212. `P0` Connection-state machine surfaced to UI: *resolving → punching → relayed → connected → failed(reason)*
213. `P1` Relay **keepalive/heartbeat** reusing the existing 1s host heartbeat / 8s timeout contract (#93)
214. `P1` Relayed **snapshot compression on by default** (reuses #95 `CompressedState`) to keep relay bytes down
215. `P2` Transport **auto-reselect**: if relay path dies, fall back to a fresh matchmaking attempt, not a hard drop

#### C. Client UX
216. `P0` Main menu **Quick Match** (matchmade) and **Play with Friends** (room code) entries alongside LAN
217. `P0` Room-code **share sheet**: copy code + copy `toysoldiers://join/<code>` deep link
218. `P1` **Recent online friends** list (display names + last room), separate from LAN recent-hosts (#86)
219. `P1` Connection-quality badge in-lobby (relay ping, relayed-vs-direct indicator)
220. `P1` Clear, non-technical **failure copy** for every failure state ("Couldn't reach the relay — check your connection")
221. `P2` **Deep-link handler** (`toysoldiers://join/<code>`) registered on Windows so a shared link launches straight into a lobby

#### D. Protocol & compatibility
222. `P0` Bump to **protocol v8**: no wire-message changes required for LAN, but relay framing + backend handshake are versioned together
223. `P0` LAN direct-connect path (v1.x) remains fully functional and default-preferred when both peers are local
224. `P1` docs/PROTOCOL.md updated with the relay framing, matchmaker handshake, and the LAN-vs-relay decision tree
225. `P1` docs/WAN_PLAY.md rewritten: "the port-forward path still works, **but you probably don't need it anymore**"

#### E. QA & ops
226. `P0` New `toy_relay_test` gate: room register → resolve → relayed loopback match, added to `verify.ps1`
227. `P0` Backend integration test (spins the relay locally, connects 2 headless clients through it)
228. `P1` Chaos test: relay drops 5% packets, injects 150ms jitter — match still completes (extends the #114 chaos harness)
229. `P1` Backend **CI**: build/test/publish the relay image on push; deploy to staging on tag
230. `P2` Load test: N simultaneous relayed rooms; publish a "rooms per instance" number
231. `P2` Synthetic uptime monitor + status note in credits ("Online services: OK / degraded")

#### F. Trust & safety (online-specific)
232. `P0` Room codes have enough entropy to resist guessing; brute-force attempts rate-limited at the registry
233. `P1` Extend the existing trust lines (#104/#105) over relay: seed-commit + deck-hash reveal work identically online
234. `P1` **Report player** hook (client → backend log) for later moderation, display-name based
235. `P2` Basic word-filter on display names surfaced through matchmaking

**v1.3 feature count: 35** (IDs 201–235)

---

## v1.4 — Persistent Toybox

### General purpose
Give players an **identity that follows them across devices and matches**: optional accounts, server-authoritative profiles, cloud saves, and unlock sync — while keeping the offline profile (`%APPDATA%/toy-soldiers/`) working for anyone who never signs in.

### Primary goals
1. Sign in with a lightweight account; play signed-out still works unchanged.
2. Cosmetics unlocks, settings, and stats **sync** across two machines for a signed-in player.
3. The server is authoritative for **owned cosmetics** (anti-tamper for future ranked rewards).
4. Local → cloud **migration** on first sign-in (no lost progress).
5. Full **delete-my-account** and export-my-data paths (privacy-first).

### Workstreams
| Workstream | Purpose |
|------------|---------|
| **Accounts** | Auth (email-link or OAuth), sessions, minimal PII |
| **Profile service** | Server-owned profile: display name, stats, unlocks, badges |
| **Sync** | Local ↔ cloud reconciliation with conflict rules |
| **Migration** | One-way local→cloud import, idempotent |
| **Privacy/legal** | Consent, export, delete; update LEGAL.md |

### Features (v1.4)

#### A. Accounts
236. `P0` **Optional sign-in** (email magic-link or a single OAuth provider) — never required to play offline
237. `P0` Session token stored securely on-device; auto-refresh; explicit sign-out clears it
238. `P0` Signed-out remains a first-class mode: local profile untouched, all offline features intact
239. `P1` **Link/unlink** device; "you're signed in on N devices" view
240. `P2` Guest → account **upgrade** (claim a local profile into a new account)

#### B. Profile service
241. `P0` Server-authoritative **profile record**: display name, avatar cosmetic, aggregate stats, owned cosmetics, badges
242. `P0` Client reads/writes profile through one service module (mirrors the DI/repository discipline used elsewhere)
243. `P1` **Owned-cosmetics ledger** on the server (source of truth for unlock gating & future rewards)
244. `P1` Badge sync: the six local badges (#171) + new online badges become server-side
245. `P2` Public **profile card** (display name, top badges, favorite map) fetchable by room-mates

#### C. Cloud saves & sync
246. `P0` **Settings sync** (the settings.ini schema) with a "this device overrides" toggle
247. `P0` **Unlock/cosmetic sync**: unlocking on one machine appears on another
248. `P0` **Stats/history sync**: match history (#186) and profile stats merge, not clobber
249. `P1` Conflict resolution: last-write-wins for settings, **union** for unlocks/badges, **sum/merge** for stats
250. `P1` Offline queue: changes made offline sync on next sign-in (idempotent, no double-count)
251. `P2` Cloud save **versioning** with a rollback window (guard against a bad merge)

#### D. Migration
252. `P0` First sign-in **imports** the existing local `%APPDATA%` profile (idempotent; safe to re-run)
253. `P1` Clear UI: "we found a local profile — import it?" with a preview of what merges
254. `P2` Post-migration, local files become a cache, not a second source of truth

#### E. Privacy & legal
255. `P0` **Delete-my-account** flow: server purges profile + backend logs tied to the account
256. `P0` **Export-my-data** (JSON download) — profile, stats, history
257. `P0` Consent screen on first sign-in; telemetry stays opt-in (v1.0 policy) and separable from accounts
258. `P1` docs/LEGAL.md + in-app privacy statement updated for accounts/cloud data
259. `P2` Data-retention policy (e.g., prune backend connection logs after N days) documented and enforced

#### F. QA
260. `P0` `toy_profile_test`: local→cloud migration idempotency, unlock union, stats-merge correctness, added to `verify.ps1`
261. `P0` Round-trip test: sign in on A, unlock, sign in on B, see it (headless mock of the profile service)
262. `P1` Tamper test: forged owned-cosmetics claim rejected by server authority
263. `P1` Offline-queue replay test (make changes offline, reconcile, no double-apply)
264. `P2` Delete-account leaves no orphaned rows/logs (verified in the integration harness)
265. `P2` Load: profile read/write latency budget documented

**v1.4 feature count: 30** (IDs 236–265)

---

## v1.5 — Proving Ground

### General purpose
Give the competitive audience a reason to keep playing: a **ranked ladder** with matchmaking rating, **leaderboards**, and **seasons** — built strictly on the deterministic-rules + host-authority + no-power-creep foundation so the ladder means something.

### Primary goals
1. **Ranked Quick Duel** (1v1) with a visible, monotonic-feeling rating.
2. Matchmaking pairs by MMR and connection quality, not just "any open room".
3. **Leaderboards** (global + friends) that can't be trivially faked.
4. **Seasons** with a reset cadence and cosmetic-only rewards.
5. Ranked integrity: seed commit-reveal + deck-hash enforced; abandon penalties.

### Workstreams
| Workstream | Purpose |
|------------|---------|
| **Ranked service** | MMR store, matchmaking-by-rating, result submission |
| **Integrity** | Anti-cheat lite, result validation, abandon handling |
| **Seasons/live-ops** | Season config, resets, reward tracks (cosmetic) |
| **UX** | Rank display, placement, post-match rating delta |
| **Leaderboards** | Query + anti-fake + friends scope |

### Features (v1.5)

#### A. Ranked core
266. `P0` **Ranked Quick Duel** queue (1v1), gated behind sign-in (v1.4)
267. `P0` **MMR** model (Elo/Glicko-style) stored server-side; hidden rating + a friendly displayed rank (tiers/toys, e.g. Tin → Plastic → Chrome)
268. `P0` **Placement matches** (best-of-N) before a rank is shown
269. `P0` Matchmaker pairs by MMR band, widening over queue time; respects connection quality (v1.3)
270. `P1` **Ranked 2v2** as a second queue reusing the Teams mode
271. `P1` Post-match **rating delta** screen (+/− with a short reason)
272. `P2` Soft **decay** for inactivity at high ranks only

#### B. Integrity & fairness
273. `P0` **Server result submission**: host reports outcome; the deterministic action log (`.toyrec` #107) is the tiebreak/audit artifact
274. `P0` Ranked **enforces** seed commit-reveal (#105) + deck-hash (#104) — no opt-out in ranked
275. `P0` **Abandon penalty**: rage-quit forfeits + small rating hit; AI-takeover (#100) does **not** count as abandon for the disconnected party if they reclaim in time
276. `P1` Result **plausibility check** on the server (turn count / damage sanity vs the submitted log)
277. `P1` **Duplicate-result / replay-attack** guard (nonce per match)
278. `P2` Optional **server-side replay verification** of contested matches (re-apply the `.toyrec` without physics)
279. `P2` Shadow **cheat metrics** (impossible action rates) feeding the #234 report pipeline

#### C. Seasons & rewards (cosmetic-only)
280. `P0` **Season** construct: start/end, config-driven, with a visible countdown
281. `P0` End-of-season **soft reset** of MMR toward the mean
282. `P0` Season **reward track** — **cosmetic-only** (a season plastic color, a badge, an accessory); *no stat rewards* (hard rule)
283. `P1` Ranked **badge tier** on the profile card (#245), resets/records per season
284. `P1` "Best rank this season" + lifetime peak stored in the profile
285. `P2` Season **flavor theme** (toy-catalog voice: "Spring Cleaning Season")

#### D. Leaderboards
286. `P0` **Global leaderboard** (top N by rating, per queue, per season)
287. `P0` **Friends leaderboard** (scoped to linked/known display names)
288. `P1` Leaderboard **anti-fake**: entries derive from server-validated results only, never client-reported totals
289. `P1` "Your position" jump + nearby-players slice
290. `P2` Regional leaderboards if regions exist (#208)

#### E. UX
291. `P0` **Rank widget** on the main menu (tier, division, progress bar)
292. `P0` Ranked **queue screen** with estimated wait + cancel
293. `P1` Placement-progress UI ("3 of 5 placements")
294. `P1` End-of-match **honor/report** prompt (feeds #234)
295. `P2` "Climbing" recap card sharable via the result-card export (#188)

#### F. QA
296. `P0` `toy_ranked_test`: MMR update math, placement flow, abandon accounting, reward-is-cosmetic assertion — in `verify.ps1`
297. `P1` Matchmaker fairness sim: synthetic population, verify band-widening + reasonable wait times
298. `P2` Integrity fuzz: forged results / duplicate nonces / impossible logs all rejected

**v1.5 feature count: 33** (IDs 266–298)

---

## v1.6 — Workshop

### General purpose
Let players **make toys, not just play with them**: an in-game creator for **cards, cosmetics, and maps**, plus sharing and moderation. Builds on the fact that `data/cards.json` has been the modding surface since v1.0 and the card-proposal template (#152) already exists — v1.6 makes it a *product*, not a GitHub form.

### Primary goals
1. Create + save a custom card / cosmetic / map **in-game**, no text editor.
2. **Share** creations via code; **browse & subscribe** to others'.
3. Play **custom matches** with subscribed content (offline + private lobbies).
4. **Moderation**: reporting, curation, and a hard wall between UGC and ranked.
5. UGC **cannot** create power creep in ranked (custom content is unranked-only).

### Workstreams
| Workstream | Purpose |
|------------|---------|
| **Creator tools** | In-game editors that emit the existing JSON schemas |
| **Content service** | Upload/host/browse UGC with versioning |
| **Curation/moderation** | Report, review, featured, takedown |
| **Sandboxing** | UGC can't crash the game or break determinism |
| **Balance firewall** | UGC is custom/unranked only; ranked uses the frozen set |

### Features (v1.6)

#### A. Card creator
299. `P0` **In-game card editor** emitting the `data/cards.json` schema (name, cost, keywords, effect, rarity, i18n text)
300. `P0` Editor **validation** against the schema + a safety floor (bad definitions can't nuke the game — reuses the v0.9 #149 loader guard)
301. `P0` **Local playtest**: drop a custom card into a Sandbox match instantly
302. `P1` Effect builder from the **existing keyword vocabulary** only (`Shield/Pierce/Draw/Heal/AdjacentOnly/AOE/Event`) — no arbitrary code, keeps determinism
303. `P1` Card **art**: pick from a palette/sticker set (no external image upload in v1.6 → moderation-safe)
304. `P2` Custom **card back / rarity frame**

#### B. Cosmetic & map creators
305. `P0` **Cosmetic creator**: recolor/name a plastic set, tower skin variant, accessory tint (within the no-power-creep rule)
306. `P1` **Map creator (lite)**: pick base room, felt dye, prop layout from a fixed prop kit, event-weight sliders
307. `P1` Map **validation**: event weights sum sane, no unreachable/softlock layouts
308. `P2` Custom **map flavor blurb** (toy-catalog voice, length-capped)

#### C. Sharing & discovery
309. `P0` **Publish** a creation → get a share code (content service, versioned)
310. `P0` **Subscribe by code**; subscribed content appears in Sandbox / private lobbies
311. `P1` **Browse** UI: recent / featured / most-subscribed, with a preview
312. `P1` **Update** a published creation (versioned; subscribers see "update available")
313. `P2` **Collections** ("my platoon pack") bundling several creations

#### D. Moderation & safety
314. `P0` **Report content** button on every UGC item → backend queue (extends #234)
315. `P0` **Takedown** path: a reported/removed item stops downloading and is hidden
316. `P1` **Featured / curated** shelf (manually promoted good content)
317. `P1` Name/text **word-filter** on publish (reuses #235)
318. `P2` Creator **trust level** (verified creators get lighter review)

#### E. Balance firewall (hard rule)
319. `P0` **UGC is unranked-only**: custom cards/maps are blocked from Ranked queues; ranked always uses the frozen v1.2 set
320. `P0` Private/custom lobbies **flag** themselves as "Custom rules" so no one is surprised
321. `P1` A curated card, once balance-reviewed, *may* be promoted into an official rotation — but only via the normal balance-report gate (200-trial band), never automatically
322. `P2` Community **voting** on custom cards feeds the curation queue (not the balance decision)

#### F. Sandboxing & determinism
323. `P0` UGC runs through the **same validated loader** as official content — no eval, no native code, data-only
324. `P0` A malformed/hostile UGC file **fails safe** (skipped + toast), never crashes host or clients
325. `P1` Custom content **hash** is part of the deck-hash trust line (#104) so a private lobby verifies everyone has the same version
326. `P2` Determinism check: a custom card in the golden-seed/lockstep tests must reproduce bit-for-bit

#### G. QA
327. `P0` `toy_workshop_test`: schema validation, safety-floor rejection, unranked-only enforcement, hash-in-trust-line — in `verify.ps1`
328. `P1` Round-trip: create → publish → subscribe → play, all headless-mockable
329. `P1` Hostile-content corpus (garbage/oversized/malformed) all fail safe
330. `P2` Moderation queue smoke (report → takedown → download blocked)

**v1.6 feature count: 32** (IDs 299–330)

---

## v1.7 — Solid Cast

### General purpose
Make the toybox **look authored, not proceduralized**, and make it **broadcastable**: real 3D soldier/tower meshes replacing the cube-era look (opt-in, perf-budgeted), plus observer/caster tooling built on the spectator (#111) + replay (#107) + capture (#77) systems already shipped.

### Primary goals
1. Optional **authored 3D soldiers** (GLTF, ≥1 LOD) that beat the cube look, staying inside the 60 FPS budget.
2. A cohesive **art upgrade** (materials, lighting, table dressing) with a low-spec fallback.
3. **Observer mode**: a spectator UI built for casting, not just watching.
4. **Caster tools**: free camera, scoreboard overlay, replay scrubbing, name/logo plates.
5. No gameplay change — this is presentation, and it must not desync or tax low-end machines.

### Workstreams
| Workstream | Purpose |
|------------|---------|
| **3D content** | Authored meshes, LODs, materials, animation |
| **Rendering** | Material/lighting upgrade on the sokol path, perf budget |
| **Observer/caster** | Broadcast-grade spectator + overlays |
| **Replay tooling** | Timeline scrub, jump, bookmark on `.toyrec` |
| **Perf** | Low-spec fallback so cubes-era hardware still runs |

### Features (v1.7)

#### A. Authored 3D content
331. `P0` **GLTF soldier mesh** with ≥1 LOD, replacing the instanced cube soldier (toggle; cube path stays as low-spec)
332. `P0` **Tower meshes** per skin family (the v0.9 skins become real geometry)
333. `P1` Skeletal or morph **animations**: march, aim, get-knocked, victory cheer (extends the v0.9 procedural bob #124)
334. `P1` Accessory meshes **attach** to the soldier rig (hats/capes/flags sit correctly)
335. `P2` Authored **map props** replacing box props on the flagship maps

#### B. Rendering upgrade
336. `P0` PBR-lite **material pass** (plastic sheen, felt, wood, metal) on the sokol path
337. `P0` **Low-spec fallback** auto-detect (drop to cubes + flat shading) with a manual override — protects the 60 FPS budget (v0.9 #158)
338. `P1` Real **shadow map** (upgrade from blob shadows #119) with a quality slider
339. `P1` Better **table dressing** + real felt textures (finally lands the deferred #120)
340. `P2` Post FX: subtle bloom/vignette, toggleable, off on low-spec

#### C. Observer mode
341. `P0` **Observer UI** distinct from a playing spectator seat: no hand, full-board info, all-seat HP
342. `P0` **Free camera** (WASD/orbit) unlocked for observers, independent of the player camera
343. `P1` **Scoreboard overlay** (seats, HP, rank if ranked, current event) toggleable for capture
344. `P1` Name/team **plates** and a lower-third strip (feeds the capture/press tooling #179)
345. `P2` **Multi-view**: quick-cut between seat-focused cameras

#### D. Caster & replay tools
346. `P0` **Replay player** for `.toyrec`: play/pause, step, **timeline scrub** (upgrades the read-only #25 scrubber to the online replay format)
347. `P1` **Bookmarks/markers** on a replay timeline (mark the clutch turn)
348. `P1` Replay **export to GIF/vertical** reusing the #77/#184 capture ring from any scrub point
349. `P2` **Caster mode** preset: observer + overlays + hidden UI chrome in one toggle for streaming

#### E. Perf & QA
350. `P0` Frame-time budget re-profiled with meshes on flagship maps; documented "recommended vs minimum" spec
351. `P0` `toy_render_smoke` extended: mesh path loads, LOD switch, low-spec fallback path all boot headless-safe where possible
352. `P1` Determinism unaffected: observer/replay never mutates match state (asserted)
353. `P1` Memory budget for meshes/textures capped; asset load failures fall back to cubes, never crash
354. `P2` Automated **perf regression** number in CI (frame-time sample on a fixed scene)

**v1.7 feature count: 24** (IDs 331–354)

---

## v1.8 — Solo Campaign

### General purpose
Give the game **something to do alone** beyond Sandbox and skirmish vs AI: a light **PvE campaign** with a toy-catalog story, plus a **roguelike run** mode for replayable solo depth — reusing the whole existing card/tower/event/AI stack.

### Primary goals
1. A **campaign** of hand-authored missions with objectives beyond "destroy tower".
2. A **roguelike run**: escalating AI, draftable rewards (cosmetic + run-scoped modifiers), permadeath-per-run.
3. **Boss encounters** reusing the Titan (#69) and event system as set-pieces.
4. Toy-catalog **narrative** framing (nostalgia, not war).
5. Fully **offline**; campaign progress syncs if signed in (v1.4) but never requires it.

### Workstreams
| Workstream | Purpose |
|------------|---------|
| **Mission system** | Scripted objectives, win/lose conditions, scenario config |
| **Run mode** | Roguelike loop, reward drafting, run-scoped modifiers |
| **PvE AI** | Encounter-tuned bots + scripted bosses (builds on the AI tiers #58) |
| **Narrative** | Between-mission flavor, toy-catalog voice, TR/EN |
| **Progression** | Campaign unlocks (cosmetic) + run meta-progress |

### Features (v1.8)

#### A. Campaign structure
355. `P0` **Campaign map/menu**: an ordered set of missions with lock/unlock progression
356. `P0` **Scenario config** format (data-driven, like cards.json): starting state, enemy setup, objective, event script
357. `P0` **Objective types** beyond kill-tower: survive N turns, protect a prop, win under a forced event, reach a damage total (reuses the challenge-mission detectors #170)
358. `P1` **Between-mission** flavor screens (toy-catalog narrative, TR/EN #153)
359. `P1` **Difficulty** selector per mission (maps onto AI tiers #58 + modifiers)
360. `P2` **Star rating** per mission (e.g., win / win fast / win untouched) for completionists

#### B. Roguelike run mode
361. `P0` **Run loop**: sequence of escalating AI fights; lose a fight → run ends
362. `P0` **Reward draft** between fights: pick 1 of 3 (cosmetic unlocks + **run-scoped** card/tweak modifiers that reset when the run ends — never permanent stats)
363. `P0` **Run seed** (deterministic, shareable — reuses the seed-trust plumbing) so runs can be compared/challenged
364. `P1` **Run-scoped modifiers** (e.g., "start with +1 shield", "sandstorm every 3rd turn") drawn from a curated pool
365. `P1` **Meta-progress**: completing runs unlocks new starting toys/cosmetics (cosmetic + variety, not power in PvP)
366. `P2` **Daily run**: fixed shared seed, offline leaderboard (feeds #286 if online)

#### C. Bosses & encounters
367. `P0` **Boss encounter** framework: scripted multi-phase enemy reusing the Titan telegraph (#69)
368. `P1` 2–3 **authored bosses** with distinct gimmicks (all built from existing keywords/events — no new engine primitives)
369. `P2` **Boss rush** mode (back-to-back bosses) as a run variant

#### D. PvE AI
370. `P0` Encounter-specific **AI scripts** (an enemy can be forced to play a set opener) layered over the existing personalities (#59)
371. `P1` PvE **difficulty modifiers** (enemy HP/damage tuning **for PvE only** — never touches the frozen PvP balance)
372. `P2` "Puzzle" encounters: fixed hand, one solvable line (like a chess puzzle)

#### E. Progression & sync
373. `P0` Campaign/run progress saved locally; **syncs via the profile service** (v1.4) if signed in
374. `P1` Campaign **completion badges** (cosmetic) added to the badge set (#244)
375. `P2` Run **history** (best run, longest run) in the profile

#### F. QA & content gate
376. `P0` `toy_campaign_test`: scenario loader + objective detection + run-loop determinism (fixed seed → fixed outcome), in `verify.ps1`
377. `P0` **PvP balance untouched**: assert campaign/run modifiers never alter the frozen ranked card/tower numbers
378. `P1` Every authored mission is **completable** (an automated "can a scripted optimal line win?" check where feasible)
379. `P2` Narrative **string parity** TR/EN checked at load (reuses the i18n key check)

**v1.8 feature count: 25** (IDs 355–379)

---

## v1.9 — Pocket Platoon

### General purpose
Reach a **second platform** and finish the **store follow-through** that v1.x scaffolded: a **mobile companion** (at minimum a cross-play **spectator/second-screen**, stretch: touch play of the lighter modes) plus real submission of the MSIX/Steam work (#167/#182) and the long-deferred real-hardware macOS smoke (#161).

> Prerequisite honesty: like v1.3's relay, several items here are **gated on external identities/hardware** (Apple hardware, a Steam/MS Partner identity, a mobile toolchain). v1.9 owns *closing* those, or explicitly re-parking them with a reason — no silent skips.

### Primary goals
1. A **mobile companion app** that can at least **spectate** a live match cross-play (via the relay #201 + spectator #111).
2. **Touch-playable** at least one light mode (Quick Duel) on mobile — stretch, gated on effort.
3. **Steam** and/or **MS Store** page actually submitted (the capsules #182 / MSIX #167 exist).
4. macOS/Metal build **smoke-tested on real Apple hardware** (finally closes #161's caveat).
5. Cross-play **compatibility contract**: a mobile and a desktop client interoperate on protocol.

### Workstreams
| Workstream | Purpose |
|------------|---------|
| **Mobile client** | Companion app (spectate → light play), shared protocol |
| **Cross-play** | One protocol, two clients; feature-flag capability negotiation |
| **Store ops** | Steam/MS submission, ratings, build pipelines |
| **Platform QA** | Real macOS + mobile-device smoke |
| **Input/UX** | Touch-first UI for the mobile surface |

### Features (v1.9)

#### A. Mobile companion (spectate-first)
380. `P0` **Mobile spectator**: join a relayed room as a spectator (#111) and watch live, cross-play with desktop hosts
381. `P0` **Touch UI** for the spectator surface (board view, HP, event log, seat cards) — companion-scale layout
382. `P1` **Second-screen** helper for a desktop player at the same table (view your hand privately on phone in hotseat)
383. `P2` **Touch play** of Quick Duel (1v1) end-to-end on mobile — stretch, only if the input/UX budget allows

#### B. Cross-play contract
384. `P0` **Capability negotiation** in the Hello/Welcome handshake: clients advertise platform + supported features; host adapts (mobile spectator vs desktop player)
385. `P0` Protocol **stays single-source** (one `net/protocol.*`); mobile builds the same wire code, no fork
386. `P1` Cross-play **compatibility test**: desktop host + mobile spectator through the relay, in CI where a mobile headless target is feasible
387. `P2` Graceful **capability downgrade** (a mobile client that can't render meshes gets the cube path #337)

#### C. Store follow-through
388. `P0` **Steam** page submission using the existing capsule set (#182): store copy TR/EN (#181), build depot, wishlist-ready
389. `P0` **MS Store** submission of the MSIX (#167) *or* a documented decision not to, with the blocker named
390. `P1` **Store build pipeline**: tag → produce Steam depot / MSIX artifact automatically (extends `package.ps1`)
391. `P1` **Age rating** questionnaires (IARC/ESRB self-serve) using the toy-violence content notes (#189)
392. `P2` Steam **rich presence / achievements** mapping the local badges (#171) to platform achievements

#### D. Platform QA (close the real caveats)
393. `P0` macOS/Metal build **smoke-tested on real Apple hardware** — closes the #161 / known-issue-#2 caveat (or re-parks it with the specific blocker if no Mac is obtainable)
394. `P0` Mobile **device smoke** matrix (≥1 real Android + ≥1 real iOS device): boot, spectate, exit clean
395. `P1` Cross-play **latency profile** desktop↔mobile over relay documented
396. `P2` Mobile **battery/thermal** sanity note for a 15-minute session

#### E. Mobile UX & lifecycle
397. `P0` Mobile **app lifecycle** handled (background/foreground, reconnect on resume via the v0.8 reconnect token #99)
398. `P1` Mobile **settings** subset (volume, language, reduced motion) synced via the profile service (v1.4)
399. `P2` Push-style **"your match is starting"** local notification for a queued player

**v1.9 feature count: 20** (IDs 380–399)

---

## v2.0 — Living Toybox

### General purpose
Ship the **major version**: pull v1.3–v1.9's systems into a coherent, **live-operated product** — seasons and a cosmetic pass on a cadence, a real launch on the platforms stood up, telemetry/support to run it, and a **balance re-freeze** for the online era. This is the "1.0 of the online game."

### Primary goals
1. **Live-ops loop**: a repeatable season/content cadence (config-driven, not a code deploy per drop).
2. **Cosmetic pass / reward track** — free + optional paid, **cosmetic-only** (hard rule).
3. **Public launch** on ≥1 store with the full online stack live and monitored.
4. **Balance re-freeze v2.0** with a fresh 200-trial report covering every rule-affecting addition since v1.2.
5. **Operate it**: telemetry (opt-in), a support/patch pipeline, and a live-ops runbook.

### Workstreams
| Workstream | Purpose |
|------------|---------|
| **Live-ops** | Season/content scheduling, remote config, feature flags |
| **Monetization (ethical)** | Cosmetic-only pass; no P2W, no lootbox-gambling loops |
| **Launch** | Store go-live, day-0 readiness across all platforms |
| **Telemetry/support** | Opt-in analytics, crash pipeline, support flow |
| **Balance** | v2.0 re-freeze + regression across all new mechanics |

### Features (v2.0)

#### A. Live-ops platform
400. `P0` **Remote config / feature-flag** service: enable/disable modes, tune season parameters, roll out gradually — no client rebuild
401. `P0` **Season scheduler**: seasons (#280) start/end on a server calendar; clients read it live
402. `P1` **Content drop** pipeline: push a new cosmetic set / featured deck modifier (#151) as data, not a build
403. `P1` **Emergency toggle**: disable a broken mode/queue remotely (extends the relay kill-switch #209)
404. `P2` **A/B** hook for a season parameter (cadence experiment), telemetry-gated

#### B. Cosmetic pass / reward track (cosmetic-only)
405. `P0` **Season pass** track: free tier + optional premium tier, **100% cosmetic** (plastics, skins, accessories, badges, table dyes) — *zero* stat content (launch blocker if violated)
406. `P0` **Progression currency** earned by playing (any mode), spendable **only** on cosmetics
407. `P0` **No gambling loops**: everything is directly purchasable/earnable; no randomized lootboxes (ethical + rating-safe)
408. `P1` **Store UI** (cosmetics shop) reading the owned-cosmetics ledger (#243) as source of truth
409. `P1` **Gifting / showcase** of cosmetics (no trading economy)
410. `P2` **Founder/legacy** cosmetic for pre-v2.0 players (recognizes early adopters)

#### C. Launch
411. `P0` **Public launch** on the primary store stood up in v1.9, with the online stack (relay #201, profiles #241, ranked #266) live in prod
412. `P0` **Day-0 readiness**: hotfix branch policy (extends #194) covers backend + client; rollback plan for the relay/profile services
413. `P0` **Launch trailer + screenshots** refreshed for the 3D/online era (reuses capture tooling #179/#348)
414. `P1` **Storefront parity** TR/EN across every platform page
415. `P1` **Server capacity** plan for launch spike (rooms-per-instance #230 → provisioning target)
416. `P2` **Press/creator** kit refresh + a few seeded content creators using the caster tools (#349)

#### D. Telemetry, support & trust
417. `P0` **Opt-in telemetry** (v1.0 privacy policy holds): funnel + retention + crash rate, **no PII**, clear consent, easy off
418. `P0` **Crash pipeline** at scale: field minidumps (#32) + backend errors aggregate to one dashboard
419. `P0` **Support flow**: in-app report attaches version.json + session log + minidump (extends #195) to a real intake
420. `P1` **Live status page** for online services (upgrades the #231 synthetic monitor) linked from credits
421. `P1` **Moderation SLA**: the report queues (#234/#314) have an actual review cadence documented
422. `P2` **Player-facing changelog / news** panel in the menu, fed by remote config (#400)

#### E. Balance re-freeze (v2.0)
423. `P0` **Balance re-freeze v2.0**: re-validate every rule-affecting addition since v1.2 (campaign is PvE-isolated #377, UGC is unranked #319, run modifiers are run-scoped #362) with a fresh **200-trial** `toy_balance_report` inside the 25–70 turn band
424. `P0` **Ranked uses only the frozen official set** — confirmed by an automated assertion, not a promise
425. `P1` Ranked **season-1 (v2.0) health report**: real MMR distribution, win rates by tier, published in docs/RELEASE.md
426. `P2` A documented process for **rotating** a curated UGC card (#321) into the official set via the same 200-trial gate

#### F. Final v2.0 gates
427. `P0` **Full regression**: all headless gates green (`verify.ps1`) **+** backend integration suite **+** cross-play smoke **+** 200-trial balance
428. `P0` **Online launch smoke**: two real machines on different networks, matchmake → ranked match → season reward → sign-out/in on a third machine, all clean
429. `P0` **Offline still whole**: with the network unplugged, hotseat + vs-AI + campaign + Sandbox all fully playable (the additive-online rule, verified)
430. `P0` **CHANGELOG + docs**: `ROADMAP_to_v2.md` closed out, `PROTOCOL.md` at the shipped protocol version, `RELEASE.md` carries the v2.0 freeze + live-ops runbook

**v2.0 feature count: 31** (IDs 400–430)

---

## Optional post-v2.0 backlog (parked, not counted)

Things intentionally *not* in v2.0 scope:

- Full mid-match host migration with zero-hitch handoff (v1.2 shipped the lite/handoff path)
- Dedicated authoritative game servers (v2.0 keeps host-authority + relay, not server-sim)
- Trading economy / player marketplace for cosmetics (kept out on purpose — gifting only)
- Full mobile feature parity (touch play of *all* modes)
- Voice chat (still out of scope, per the original GDD)
- Tournament/bracket automation as a first-class in-client system (caster tools exist; brackets are manual)

---

## Totals

| Version | Codename | Feature IDs | Count |
|---------|----------|-------------|------:|
| v1.3 | Open Signal | 201–235 | 35 |
| v1.4 | Persistent Toybox | 236–265 | 30 |
| v1.5 | Proving Ground | 266–298 | 33 |
| v1.6 | Workshop | 299–330 | 32 |
| v1.7 | Solid Cast | 331–354 | 24 |
| v1.8 | Solo Campaign | 355–379 | 25 |
| v1.9 | Pocket Platoon | 380–399 | 20 |
| v2.0 | Living Toybox | 400–430 | 31 |
| **Total planned updates (v2 line)** | | **201–430** | **230** |

*(v1 line: IDs 1–200, all shipped through v1.2. The v2 line continues the ID scheme without reuse.)*

---

## New standing purposes for the v2.0 era

Additions to the v1 cross-cutting duties (which all still apply):

| Standing purpose | What "good" looks like |
|------------------|------------------------|
| **Backend is a broker, never a referee** | Online services connect, store, and match — they never simulate or judge a game. Host authority is untouched. |
| **Additive online** | Every online system has an offline-equivalent or degrades cleanly. Pulling the plug never bricks the core loop. |
| **Cosmetic-only economy** | No pass, reward, UGC item, or purchase ever grants stats. This is checked in `verify.ps1`, not just intended. |
| **Ranked integrity** | Determinism + trust lines + server validation make a rank mean something. Any feature that threatens that is unranked-only. |
| **Ethical monetization** | No lootbox gambling, no dark patterns, no P2W. Direct, earnable, honest. |
| **Operate what you ship** | Every online system lands with telemetry (opt-in), a kill-switch, and a line in the live-ops runbook. |

---

## One-page "what is v2.0?" definition

**v2.0 is done when** a player can install on Windows (or a supported store/platform), optionally sign in, **matchmake against a stranger on another network**, climb a **ranked ladder** across **seasons** whose rewards are purely cosmetic, **make and share** their own cards/maps in a workshop that can never touch ranked balance, watch a friend's match from their **phone**, play a **solo campaign** with the network unplugged, and see the game get **new seasonal content without a patch** — all while the original promises hold: **host-authoritative, deterministic, no power creep, plastic-toy tone, and green automated tests before every tag.**

Until then, ship intermediate `v1.x` tags with honest notes — and be explicit about the handful of items gated on **external prerequisites** (a live relay/matchmaker deployment, a store/publisher identity, real Apple + mobile hardware), exactly as `docs/RELEASE.md` has done since v1.0.

---

## Sequencing note (dependency order)

The ladder is intentionally ordered by dependency, not just ambition:

```
v1.3 relay/matchmaking ──┐
                         ├─► v1.5 ranked (needs online + accounts)
v1.4 accounts/profiles ──┘
v1.4 profiles ──────────────► v1.6 workshop ownership, v1.8 progression sync
v1.7 art/caster ────────────► v2.0 launch presentation
v1.9 mobile/store ──────────► v2.0 public launch
v1.3–v1.9 systems ──────────► v2.0 live-ops + re-freeze + ship
```

A team short on time can **cut from the back**: v2.0 can launch on desktop-only (defer v1.9 mobile), or with seasons-but-no-pass (defer §B), and still be a coherent major release. Prefer a cut list over a slipped date — the v1 "ship small" rule carries forward.

---

*Document version: 2026-07-13 · Product: toy-soldiers · Engine: Box3D · Continues `docs/ROADMAP_to_v1.md` (IDs 1–200) · Live with `docs/GDD.md`, `docs/PROTOCOL.md`, `docs/RELEASE.md`.*
