# Network Protocol — toy-soldiers (protocol v7)

Reference for debugging with Wireshark or building tools (#116). Host-authoritative:
clients only ever send *intents*; the host owns all game state and broadcasts snapshots.

## Transports

| Channel | Transport | Port | Purpose |
|---|---|---|---|
| Game session | TCP | 27123 (default, host-configurable) | lobby + match, length-framed messages |
| LAN discovery | UDP | 27124 (fixed) | host beacon broadcast, 1/s |

## TCP framing

Every message is a length-prefixed frame:

```
[u32 payload_size LE][payload]
```

`payload_size` covers the payload only; max 1 MiB (larger = protocol error, peer dropped).

## Message header (start of every payload)

```
struct MsgHeader {            // #pragma pack(1)
    uint32_t magic;           // 0x4D325954 — "TY2M" in LE byte order
    uint16_t type;            // MsgType below
    uint16_t version;         // kProtocolVersion (7); mismatch -> readable reject
};
```

## Message types

| # | Name | Dir | Payload |
|---|------|-----|---------|
| 1 | Hello | C→H | char name[24], u16 version, u32 reconnectToken (0 = fresh), u8 spectate (v1.2 #111) |
| 2 | Welcome | H→C | i32 seat (`kSpectatorSeat = -3` for spectators), u32 reconnectToken |
| 3 | LobbyState | H→C | full match snapshot (phase == Lobby) |
| 4 | MatchState | H→C | full match snapshot |
| 5 | Ready | C→H | u8 ready |
| 6 | SetTower | C→H | u8 TowerType (rejected if >= Count) |
| 8 | PlayCard | C→H | u32 intentId, i32 handIndex, i32 target (duplicate intentId ignored) |
| 9 | EndTurn | C→H | — |
| 10 | Reject | H→C | u16 len, char reason[len] |
| 11 | Chat | C↔H | u16 len, char text[len] (rate-limited both sides) |
| 13 | SetCosmetics | C→H | u8 plastic, u8 towerSkin, u8 accessory (range-checked) |
| 14 | SetDeckMods | C→H | i32 ban0, i32 ban1, i32 extra0, i32 extra1 |
| 15 | Heartbeat | H→C | u32 hostMillis (echo back in Pong) — every ~1s |
| 16 | Pong | C→H | u32 echoedMillis (host derives RTT; freshens timeout) |
| 17 | Kick | H→C | u16 len, char reason[len]; socket closes after |
| 18 | ResyncRequest | C→H | — (host answers with a full snapshot) |
| 19 | RematchVote | C→H | u8 accept |
| 20 | CompressedState | H→C | u8 innerType, u32 rawSize, lz4 block (v1.2 #95) |
| 21 | DeltaState | H→C | u32 baseSyncGeneration, player-level delta payload (v1.2 #96) |

Timeouts: either side drops a peer after 8s of silence. A mid-match drop hands the seat
to the AI and reserves it for the reconnect token for 60s.

## Snapshots

`LobbyState`/`MatchState` payloads (after the header) are the full serialized match:
magic `0x32594F54` ("TOY2"), u16 protocol version, then config, world event, per-player
state (including hands/decks), the last 24 log lines, `bannerColor` (u8, 0-7, v1.2 #84),
per-seat `peerIp[40]` (host-local, not meaningful to clients), and the pending physics
impulse. Clients ignore snapshots older than the last seen (matchId, syncGeneration)
pair. ~2.3 KB typical; see `src/game/snapshot.cpp` for the exact field order.

## Compression and deltas (v1.2 #95/#96)

`packState()` (`src/net/session.cpp`) decides per-send how to wrap a snapshot:

1. **First send to a peer, or lobby/new-match snapshots** — always a full
   `LobbyState`/`MatchState`, uncompressed. A client needs a complete baseline before
   deltas mean anything.
2. **Full snapshot > 2 KB** — wrapped in `CompressedState` (lz4-compressed; `innerType`
   records whether the inner payload was `LobbyState` or `MatchState` so the client
   re-dispatches correctly after decompressing).
3. **Mid-match broadcast where the client already has a same-or-newer baseline** — sent
   as `DeltaState` instead: a dirty-player bitmask (which of the 4 seats changed) plus
   just those players' serialized fields and the log tail, referenced against
   `baseSyncGeneration`. `applyMatchDelta()` on the client returns
   `DeltaResult::NeedResync` if the base generation doesn't match what it has, which
   triggers a `ResyncRequest` — deltas degrade to a full resync rather than silently
   desyncing.

Delta payloads are typically well under half the size of a full snapshot for a
single-player-changed update (e.g. a card play or end-turn); a no-op/idle delta
(nothing changed) is smaller still. See `toy_net_test`'s "compression + delta unit"
case for the exact size assertions.

## Spectators (v1.2 #111)

`Hello` carries a `spectate` flag; the host assigns `kSpectatorSeat` (-3) instead of
consuming one of the 4 player seats. Spectators receive the same snapshot stream as
everyone else (full/compressed/delta, per the rules above) but the host rejects any
`PlayCard`/`EndTurn`/`SetTower`/etc. from a spectator-seated connection — read-only by
construction, not just by client-side UI hiding.

## Host migration (v1.2 #101/#102)

Lobby-only migration (v1-lite, #101): if the host disconnects while `phase == Lobby`,
remaining clients elect a new host locally (lowest seat index still connected) and
that client re-hosts a fresh lobby; other clients reconnect via the normal `Hello`
path — no new wire messages needed, since nothing mid-match needs preserving yet.

Mid-match migration (#102) additionally opens a `migrationWindow_` on the new host: an
incoming `Hello` whose display name matches a seat from the snapshot the new host
inherited is reattached to that seat (rather than treated as a fresh joiner) until the
window's deadline passes. This reuses `Hello`/`Welcome` rather than adding new message
types — the "is this a migration reattach or a fresh join" decision is host-side name
matching against `MigrationPlan`, not a protocol-level flag.

## UDP beacon (discovery)

One datagram, not framed:

```
u32 magic;            // 0x43425954 — "TYBC" LE
char code[5];         // 4-letter room code + NUL
u16 version;          // protocol version (mismatched lobbies grey out in the browser)
u16 tcpPort;          // host lobby port
char hostName[24];
u8 seatsTaken, seatsTotal;
```

Broadcast to 255.255.255.255:27124 and 127.0.0.1:27124 every second while a lobby is open.

## Trust lines (#104/#105)

At match start the host logs (and therefore broadcasts inside snapshots):

```
Trust: seed commit <fnv32> — deck check 0:<h> 1:<h> ...
```

and at game over reveals `Trust: seed was <seed> (commit <fnv32>)`. Any client can
recompute both fingerprints (FNV-1a; `seedCommitHash` / `deckCheckHash` in
`src/game/cards.cpp`) to confirm the host did not swap seeds or stack decks mid-match.

## Wireshark quick recipes

- Session traffic: `tcp.port == 27123` → right-click → *Follow TCP Stream* (frames are
  visible as 4-byte LE lengths followed by `TY2M` magic).
- Discovery: `udp.port == 27124` — payload starts with `TYBC`.
- Version-mismatch repro: hand-craft a Hello with a wrong header version; the host
  answers with a readable Reject and closes.
