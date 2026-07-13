# Microsoft Store packaging experiment (#167)

Roadmap item #167 explicitly calls this an "experiment" — this is a real, working
Desktop Bridge (MSIX) packaging pipeline for the existing Win32 build, verified by
actually producing an installable `.msix` locally. It is not a Store submission.

## What's automated

- `scripts/generate-msix-assets.ps1` — draws the required tile art (44x44, 150x150,
  50x50 store logo, 310x150 wide tile, 620x300 splash) as flat-colored placeholders
  in the game's own palette (navy background, gold toy-soldier silhouette). There is
  no hand-authored logo anywhere in this project — every in-game visual is a
  procedurally-colored low-poly cube — so these placeholders match the game's actual
  look rather than inventing unrelated brand art. **Swap before a real Store listing.**
- `packaging/msix/AppxManifest.xml.template` — a Desktop Bridge manifest
  (`EntryPoint="Windows.FullTrustApplication"`, `rescap:Capability Name="runFullTrust"`)
  that packages `toy_soldiers.exe` unmodified. Full-trust apps keep the normal Win32
  process model, so the direct TCP/UDP sockets LAN play depends on and the
  `%APPDATA%\toy-soldiers\` settings/log/crash-dump paths work exactly as in the
  portable build — no extra network capability needed, no code changes.
- `scripts/package-msix.ps1` — stages `dist/toy-soldiers-v<ver>/` (building it via
  `package.ps1` first if missing), copies the tile art in, substitutes the real
  version from `src/app/version.h` into the manifest, and runs `makeappx pack`.
  Verified this cycle: `Package creation succeeded` against a real Release build,
  producing an installable `dist/toy-soldiers-v<ver>.msix`.
- `-SelfSign` signs the package with a local dev certificate (`CN=preunec-dev`,
  created on first use) via `signtool` — enough for `Add-AppxPackage` sideload
  testing on a machine that trusts that cert. `-Install` (implies `-SelfSign`) also
  imports the cert into `Cert:\LocalMachine\TrustedPeople` (needs admin) and installs
  the package with `Add-AppxPackage`. Neither was run as part of building this
  pipeline — packing alone already validates the manifest schema, asset references,
  and payload; installing modifies machine-wide certificate trust and is a
  deliberate, separate step for whoever runs it.

## What's still a human/business step

A real Microsoft Store listing needs, at minimum:

1. A Partner Center developer account and a reserved app name (the `Identity/Name`
   and `Publisher` in the manifest must match what Partner Center issues — the
   `preunec.ToySoldiersTabletop` / `CN=preunec-dev` values here are placeholders).
2. Either Store-side signing (Partner Center signs on submission — no local cert
   needed) or a certificate from a CA the target machines already trust, if
   distributing the `.msix` outside the Store.
3. Real tile/splash art replacing `assets/msix/*.png`.
4. Store listing metadata (age ratings, screenshots, privacy policy URL) — see
   `docs/store/itch-page.md` and `docs/LEGAL.md` for content that's already written
   and can be adapted.

## Usage

```powershell
cmake --build --preset windows-release --parallel   # build toy_soldiers.exe first
powershell -File scripts/package-msix.ps1            # -> dist/toy-soldiers-v<ver>.msix (unsigned)
powershell -File scripts/package-msix.ps1 -Install    # + local dev-signed sideload install
```
