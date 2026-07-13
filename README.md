# Oyuncak Asker Masa Savaşı — Toy Soldiers

Game-jam prototype of **childhood plastic toy soldiers on a tabletop**, powered by **[Box3D](https://github.com/erincatto/box3d)**.

Concept board: [`docs/concept_board.html`](docs/concept_board.html)  
Design notes: [`docs/GDD.md`](docs/GDD.md)  
**Roadmap to v1.0 (200 features):** [`docs/ROADMAP_to_v1.md`](docs/ROADMAP_to_v1.md) *(shipped through v1.2)*
**Roadmap to v2.0 (230 features):** [`docs/ROADMAP_to_v2.md`](docs/ROADMAP_to_v2.md) — online, ranked, workshop, 3D, campaign, mobile

## Fantasy

Players command green plastic army men with **cards**, protect a **main tower**, and react to **room/map events** on a nostalgic toy-box table. MVP is a **4-player hotseat** match; multiplayer host-lobby is M2+.

## Stack

| Layer | Tech |
|---|---|
| Physics | Box3D (`../box3d`, local sibling) |
| Window / GPU | sokol (D3D11 on Windows) |
| HUD | Dear ImGui |
| Build | CMake 3.22+, C17 / C++20 |

## Layout

```
toy-soldiers/
  src/game/       # cards, rules, match state (headless-safe)
  src/physics/    # Box3D table / towers / soldiers
  src/render/     # camera, cube draw, ImGui HUD
  docs/           # GDD + concept board
```

Architecture mirrors the concept board state layers:

`Input → Rule Engine → Game State → Event System → (later) Network Sync`

## Build (Windows)

Requires CMake on `PATH` (or `C:\Program Files\CMake\bin`) and Visual Studio 2022 with C++ tools. Box3D must live at `../box3d`.

```powershell
$env:Path = "C:\Program Files\CMake\bin;" + $env:Path
cd D:\Projects\Github\preunec\toy-soldiers
cmake --preset windows
cmake --build --preset windows-release --parallel
```

## Run

```powershell
# Graphical prototype (from repo root so relative paths stay sane)
.\build\bin\Release\toy_soldiers.exe

# Headless smoke (rules + physics)
.\build\bin\Release\toy_sim.exe
```

### Controls

| Input | Action |
|---|---|
| LMB drag | Orbit camera |
| Scroll | Zoom |
| UI hand buttons | Select / play card |
| `Space` | Auto-play current seat |
| `R` | Reset match |
| `N` | New match (new seed) |

## Multiplayer (M2)

Host PC is the lobby authority (no dedicated server).

1. **Host:** run `toy_soldiers.exe` → **Host Lobby** (default port `27123`)
2. **Clients:** same LAN/localhost → enter host IP → **Join Host**
3. Host sees seats fill → optional **Fill empty with AI** → **Start Match**
4. On your turn, play a card; host validates and broadcasts state

```powershell
# Headless multiplayer smoke (localhost host+client)
.\build\bin\Release\toy_net_test.exe
```

Firewall: allow inbound TCP `27123` on the host machine for LAN play.

## Maps & events (M3)

| Map | Feel | Biased events |
|---|---|---|
| Living Room Table | Green felt | Cat, light rain |
| Desert Playmat | Sand | Sandstorm, flood |
| Backyard Picnic | Grass | Rain, flood |

| Event | Effect |
|---|---|
| Sandstorm | Adjacent-only attacks (1 turn) |
| Rain | Attack damage −1 (2 turns) |
| Flood | Focus seat leaks 1 HP on their turn; **shield blocks** |
| Cat | Warning → physics pounce (no tower damage) |

Events roll on the host after turn advances (cooldown + map weights). Offline: **Debug force event** under the match panel.

```powershell
.\build\bin\Release\toy_event_test.exe
```

## Cosmetics & rooms (M4)

Lobby loadout (no combat power):

- **Plastic color** — 10 toy colors (classic green default)
- **Tower skin** — Block / Sandcastle / Rocket / Fort (different silhouettes)
- **Accessory** — Party hat, Santa hat, bandana, star medal

Each map also places simple **room props** (sofa, dunes, picnic gear) and a matching sky/clear color.

```powershell
.\build\bin\Release\toy_cosmetic_test.exe
```

## Balance & polish (M5)

- **Towers:** Machine Gun 36 HP · Sniper 34 HP +2 damage  
- **Scout Peek** buff persists until your next attack (fixed)  
- **Events:** rarer (~20%, cooldown 4, start after turn 5)  
- **UI:** how-to-play, event toasts, damage preview, post-match stats  
- **Keys:** `1–5` select card · `Enter` play · `H` help · `Space` auto  

```powershell
.\build\bin\Release\toy_balance_report.exe   # Monte-Carlo length / win rates
```

## Milestone map (from concept board)

| ID | Scope | Status |
|---|---|---|
| **M0** | Lock rules | Done |
| **M1** | Local hotseat | Done |
| **M2** | Host lobby multiplayer | Done |
| **M3** | Map + events | Done |
| **M4** | Rooms, cosmetics | Done |
| **M5** | Balance & polish | **Done** |

**MVP complete** for the game-jam concept board.

## License

Prototype code is MIT-style for jam use. Box3D is MIT (Erin Catto). Sokol / ImGui keep their upstream licenses.
