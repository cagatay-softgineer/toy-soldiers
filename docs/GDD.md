# GDD — Oyuncak Asker Masa Savaşı (v0.1)

Source of truth for creative intent: `docs/concept_board.html`.  
Product path to ship: `docs/ROADMAP_to_v1.md` (v0.5 MVP → v1.0, **200** tracked updates).

## Pitch

Childhood green plastic soldiers on a living-room table, reborn as a **4-player turn-based party strategy** game. Cards are orders. Towers are bases. The room can interrupt the battle.

## Core loop

1. Lobby / seats (hotseat offline; host lobby online)
2. **Map theme pick** (Living Room / Desert / Backyard)
3. Main tower type pick (Machine Gun / Sniper)
4. Deal cards
5. On your turn: pick target → play **one** card
6. **World event tick** (map-weighted, cooldown-limited)
7. Last standing tower wins

## Locked for M1–M5 (MVP complete)

| Decision | Choice |
|---|---|
| Player count | 4 seats (hotseat offline; host lobby online) |
| Win condition | Last tower with HP > 0 |
| Targeting | Free by default; **Sandstorm** forces adjacent |
| Card classes | Attack, Defense, Tactic (~15 defs, 22-card decks) |
| Tower types | MG **36 HP** · Sniper **34 HP** +2 attack damage |
| Network | Host-as-lobby-owner TCP, port `27123` |
| Maps | Living Room, Desert, Backyard (+ room props) |
| Events | ~20% spawn after turn 5, cooldown 4 (controlled) |
| Cosmetics | Plastic / skin / accessory (**no combat power**) |
| Voice | Out of scope |
| Empty seats | Host can fill with AI on match start |

## Systems

### Game state

- `Player`: tower HP, shield turns, hand/deck/discard, soldiers
- `Match`: active seat, turn number, phase, event log, RNG seed

### Rule engine

- Validate card + target
- Apply damage / shield / draw
- One card per turn → auto end turn
- Elimination when tower HP hits 0

### Physics (Box3D)

- Static felt table + wooden rim (color follows **map**)
- Static main towers at N/E/S/W seats (flood tint)
- Dynamic plastic soldiers knocked on tower hit / **cat pounce** / flood splash

### World events (M3 — implemented)

| Event | Trigger policy | Effect | Counter-play | Risk control |
|---|---|---|---|---|
| **Sandstorm** | Desert-weighted, after turn 4, ~28% when cooldown 0 | Adjacent-only attacks for 1 turn | Wait / use Defense-Tactic | Short duration |
| **Rain** | Backyard-weighted | Attack damage −1 for 2 turns | Still chip with multi-hit | Min damage floor |
| **Flood** | Desert/Backyard rare | Focus seat leaks 1 HP on their turn start (2 turns); **v1.2 #73:** also blocks line of sight — the flooded seat can only be targeted from an adjacent seat while the water stands | **Shield blocks** the leak; move adjacent to still reach them | Focused, not global wipe |
| **Cat** | Living Room-weighted | Warn → pounce physics knock (0 HP) | N/A (flavor) | No tower damage |
| **Titan** (v1.2 #69) | Rare, telegraphed 2 full turns ("distant stomping… something HUGE") | Table-wide physics stomp on every seat at once (no tower damage — flavor + knockback only) | None needed — no HP is ever at stake | Pure spectacle, can't swing a match |

- Cooldown: 3 turns after an event starts; 2-turn grace at match start
- Host authority; state is in snapshot protocol (now **v7** — see docs/PROTOCOL.md)

### Cosmetics & room atmosphere (M4 — implemented)

| Slot | Options | Effect |
|---|---|---|
| **Plastic color** | Classic Green, Olive, Blue, Red, Yellow, Purple, Orange, White, Pink, Camo | Soldier + base tint |
| **Tower skin** | Toy Block, Sandcastle, Bottle Rocket, Cardboard Fort | Shape + material tint |
| **Accessory** | None, Party Hat, Santa Hat, Bandana, Star Medal | Hat/medal prop on tower (and first soldier) |

- Lobby pickers for host/client; AI gets seeded random cosmetics
- **Room props** per map: sofa/lamp/chest · dunes/cactus · hedge/basket/tree
- Clear color follows map (room lighting mood)
- Synced via snapshot + `SetCosmetics` net message

### Network (M2 — implemented)

- **Transport:** non-blocking TCP, length-prefixed frames (`src/net/`)
- **Authority:** lobby owner (host) owns `Match`; validates every `PlayCard` / `EndTurn`
- **Sync:** full match snapshots (`serializeMatch`) after each mutation (`syncGeneration`)
- **Lobby:** seat assignment, ready flag, tower pick, **map pick**; host presses Start
- **Clients:** send intents only; apply host snapshots; physics impulses are local cues
- **Not yet:** host migration, NAT punch, mid-match join, hidden hands

## Card starter set (M5)

See `src/game/cards.cpp`. Highlights:

- Commons: Pellet Shot (2), Plastic Volley (3), Sandbag / Duck Tape
- Rare: Bayonet 4, Mortar 4, Rations heal 3, Scout Peek (+1 next attack, **persists**)
- Epic: March Fire 2+2, Glue Trap, Medkit heal 4
- Legendary: Ambush 3 (+2 if unshielded) — one copy per deck

Run `toy_balance_report` for Monte-Carlo length / tower win rates.

### Polish (M5)

- How-to-play modal, toast for world events / winner
- Damage preview on attack cards, auto-valid target, post-match stats
- Keys: `1–5` hand, `Enter` play, `H` help, `Space` auto, `R` reset
- Optional auto-orbit camera

## Open questions (post-MVP)

- 2–4 player support vs fixed 4
- Host migration when lobby owner leaves
- Event *cards* (player-cast) vs only ambient world events

## Visual principles

1. Green plastic silhouette default  
2. Toy-box nostalgia, not realistic war  
3. Cards = command orders  
4. Room/table is the battlefield skin  
