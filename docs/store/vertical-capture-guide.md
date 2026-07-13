# Vertical capture guide — TikTok/Shorts/Reels (#184)

The existing "last 3 seconds of chaos" GIF capture (`G` hotkey, #77) now has a
9:16 portrait preset alongside the original landscape one — TikTok, YouTube
Shorts, and Instagram Reels all standardize on the same 9:16 ratio, so one
preset covers all three.

## Usage

| Hotkey | Output | Ratio |
|---|---|---|
| `G` (in a match) | `chaos-<timestamp>.gif` | native landscape |
| `Shift+G` (in a match) | `chaos-vertical-<timestamp>.gif` | 9:16 portrait |

Both read from the same rolling ~3-second frame ring (`captureRingTick`, quarter
resolution, ~8fps) — `Shift+G` just center-crops each buffered frame to 9:16
before encoding instead of using the full landscape frame. Files land in
`%APPDATA%\toy-soldiers\screenshots\`, same as every other capture output.

There's also a one-shot vertical PNG (`toy::captureWindowPngVertical()` in
`src/app/capture.h`) using the same crop helper, for a still frame instead of a
clip — not currently bound to a hotkey (there was no existing plain-screenshot
hotkey to extend; it's available for future UI wiring, e.g. a vertical variant
of the result-card export, #188).

## How the crop works

`cropCenterVertical()` (`src/app/capture.cpp`) trims the captured frame's width
down to `height * 9 / 16`, centered — the capture window is landscape, so height
is already the limiting dimension and a horizontal-only crop produces a true 9:16
portrait frame without letterboxing or stretching. This deliberately keeps the
UI hand panel (which sits at the screen edges) mostly out of frame, since the
3D table is the part worth sharing.

## Capturing a good clip

1. Get action on screen first — a card resolving, a world event triggering, a
   tower falling. The ring buffer always holds the *last* ~3 seconds, so press
   the hotkey right after something happens, not before.
2. `Shift+G` right after a knockback hit or an event banner appears (sandstorm,
   flood, the cat) tends to produce the most shareable clips — matches the shot
   list already used for the trailer script (`docs/store/trailer-script.md`).
3. For a longer edit, capture several short clips and stitch them externally —
   the in-engine buffer is intentionally short (~3s at low res) to keep it
   cheap to hold in memory every frame, not meant to replace OBS for a full
   recording session.

Verified this cycle via `--press-tour`: both `captureWindowPngVertical` and
`captureRingSaveGifVertical` were exercised against a real running match window
and produced a correctly-proportioned 810x1440 PNG and a multi-frame GIF (the
temporary verification hook has since been removed from `main.cpp`).
