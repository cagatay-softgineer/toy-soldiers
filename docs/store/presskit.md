# Press Kit — Oyuncak Asker Masa Savaşı (v1.0 #183)

## Factsheet
- **Developer:** preunec / cagatay-softgineer (solo + AI-assisted engineering)
- **Release:** v1.0.0 "Ship Toy Soldiers", 2026
- **Platforms:** Windows 10/11 x64 (portable zip + installer)
- **Price:** free / pay-what-you-want
- **Engine:** custom C++20 on Box3D physics + sokol + Dear ImGui; zero third-party assets
- **Languages:** English, Turkish
- **Press contact:** GitHub issues → https://github.com/cagatay-softgineer/toy-soldiers

## Description
A turn-based card battler about plastic toy soldiers holding the living-room table.
One order card per turn drives a real 3D physics sim — attacks knock actual little
soldiers over — while the room itself interferes with sandstorms, floods, blackouts,
a dog, a cat, and ants. Hotseat or 4-player LAN with 4-letter room codes, reconnect
protection, and a host-authoritative protocol hardened by an automated net-test suite.

## History
Born as a game-jam concept board ("childhood toy soldiers + cards + physics"), the
project shipped six public-facing milestones (v0.5 Tabletop MVP → v1.0) with a
200-item engineering backlog, each gated by a one-command regression suite.

## Notable facts for coverage
- Everything is procedural: the meow when the cat event fires is a synthesized sine slide.
- The whole 3D scene is one instanced draw call.
- The AI has personalities (Aggro / Turtle / Chaos) and a lethal-aware Hard mode.
- The multiplayer test suite includes a malformed-packet fuzzer and a 4-client stress match.
- Cosmetics are hard-ruled to never affect gameplay ("fashion ≠ stats" since v0.5).

## Assets
- 5 key screenshots — see `itch-page.md` shot list (capture at 1920×1080)
- 45s gameplay trailer — see `trailer-script.md`
- Logo: wordmark on navy, white on dark (export from title screen via photo mode)
