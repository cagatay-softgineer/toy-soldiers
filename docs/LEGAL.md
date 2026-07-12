# Legal & Player-Facing Statements — v1.0 (#189–#192)

## Content rating notes (#189)

- Cartoon "toy violence" only: plastic soldiers fall over; towers crack and pop back up.
- No blood, gore, realistic weaponry, or human characters. No war realism by design
  (standing rule since v0.5: "toy tone — plastic and nostalgia, never military marketing").
- No chat filter is applied to LAN lobby chat; multiplayer is with people you invite on
  your own network. Suggested self-rating: PEGI 7 / ESRB E10+ equivalent.

## Privacy (#190)

- **No telemetry. No analytics. No accounts. No internet connections initiated by the game.**
- Networking is limited to the LAN session you explicitly host or join
  (TCP on your chosen port, UDP beacon on 27124) plus the browser page that opens only
  if you click "Project page / check updates".
- Crash minidumps are written **locally** to `%APPDATA%/toy-soldiers/crashes/` and are
  never transmitted. Attaching them to a bug report is entirely opt-in and manual.
- Settings, logs, and match history stay in `%APPDATA%/toy-soldiers/`.

## EULA / Terms stub (#191)

The game is provided "as is", without warranty of any kind. You may play, copy, and
redistribute the unmodified package for free. Third-party components are licensed per
`THIRD_PARTY_LICENSES.md`. This stub is a placeholder for the store's standard terms;
the itch.io default terms apply on that storefront.

## Accessibility statement (#192) — partial, honest

Available today:
- UI scale 100/125/150%, high-contrast theme, colorblind-safe seat colors (not red/green)
- Reduced-motion mode (disables auto-orbit, camera punch, slow-mo, fly-over; shortens toasts)
- Full keyboard play for the core loop (1–8 select card, Enter play, Esc pause, H help)
- Per-category audio volumes; the game is fully playable muted

Known gaps (roadmap, not promised): screen-reader support, remappable keys,
subtitle-style event callouts, one-handed layout.
