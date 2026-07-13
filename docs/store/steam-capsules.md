# Steam store capsule art set (#182)

Roadmap #182 is explicitly "even if wishlists only" — this is the core asset set
needed to submit a Steam store page for wishlisting, not a full Store launch kit.

## What's here

`assets/steam/` has both the editable SVG source and the exact-pixel PNG Steam
actually accepts (Steam does not take SVG uploads):

| File | Size | Steam slot |
|---|---|---|
| `main_capsule_616x353` | 616 x 353 | Main Capsule (store page hero) |
| `header_capsule_460x215` | 460 x 215 | Header Capsule (search/tag lists) |
| `small_capsule_231x87` | 231 x 87 | Small Capsule (recommendation widgets) |
| `vertical_capsule_300x450` | 300 x 450 | Vertical/Library Capsule |

Same visual language as `docs/rules-comic.html` and the MSIX tile art
(`docs/store/msix-packaging.md`): navy background, gold toy-soldier silhouettes on
the game's actual felt-table green, since there's no separately commissioned key
art for this project — every in-game visual is a flat-colored low-poly cube, so
the capsules echo that rather than inventing an unrelated art style.

## Regenerating

Edit the `.svg` files directly (plain hand-authored SVG, no build tooling), then:

```powershell
powershell -File scripts/render-steam-capsules.ps1
```

This headlessly rasterizes each SVG to its exact required pixel size via a local
Chrome/Edge install and overwrites the matching `.png`. Verified this cycle: all
four outputs measured pixel-exact against their required dimensions
(`System.Drawing.Image` width/height check), and visually reviewed for text/art
collisions (an earlier draft's main-capsule tagline overlapped the table art —
fixed by shortening it).

## Not covered (out of scope for a wishlist-only page)

- **Library Hero** (3840x1240) and **Library Logo** — only needed after a page is
  approved and live, not for initial wishlist submission.
- Any raster key art beyond these four flat vector compositions — real
  photographed/rendered key art is a separate (art) task if this ever needs a
  genuine marketing push.
- Store page copy — already covered by `docs/store/itch-page.md` (EN/TR feature
  bullets adapt directly to a Steam "About this game" section).
