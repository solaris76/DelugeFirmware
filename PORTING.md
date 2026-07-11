# CG Custom Firmware — port to upstream `main`

This folder is a **separate integration project** for rebasing custom Deluge firmware work onto the current upstream default branch (`main` at [SynthstromAudible/DelugeFirmware](https://github.com/SynthstromAudible/DelugeFirmware)).

The original custom fork remains untouched at:

`/Volumes/Development/Development/Deluge-CGFW` → branch `CG-Custom-FW`

## Remotes

| Remote | URL | Purpose |
|--------|-----|---------|
| `origin` | SynthstromAudible/DelugeFirmware | Upstream (track `main`) |
| `cgfw` | local Deluge-CGFW clone | Cherry-pick source for custom commits |
| `solaris76` | solaris76/DelugeFirmware | Your GitHub fork (push target when ready) |

## Branch

**`CG-Custom-FW-on-main`** — integration branch based on upstream `main` (**CGFW v2.0.0**).

## Why a separate folder?

- Keeps the working v1.6.0 custom fork buildable while integration proceeds.
- Avoids risky merges on the branch you flash to hardware today.
- Makes it easy to compare behaviour side-by-side.

## Architecture note (important)

Upstream `main` and the old `community` branch **diverged before the MIDI root-complex refactor**. All custom CG work (USB multi-port, Launchpad, MIDI routing) assumes `root_complex` (`root_usb`, `root_din`, etc.).

**Phase 0 (prerequisite)** must land before any custom feature ports:

1. `72643e56` — Create MIDI root complex abstraction (#3157) ✅
2. `c600b073` — fix sysex on third USB MIDI interface (#3999) ✅
3. `619d7bf4` — upstream USB port 2 defaults — **skipped** (already on `main`)

## Porting phases

| Phase | Feature | Status | Notes |
|-------|---------|--------|-------|
| 0 | MIDI root_complex from `community` | ✅ Done | Required foundation |
| 1 | USB host multi-port MIDI | ✅ Done | MRCC 880 / multi-cable devices |
| 2 | MIDI output device selection + `MIDIRouting` | ✅ Done | Per-track/port routing |
| 2b | MIDI kit row automation | ✅ Done | CC, velocity, automation view |
| 3 | Automation generator lanes | ✅ Done | Shapes, clock rate, CC output modes |
| 4 | Expressive Chords keyboard layout | ✅ Done | |
| 5 | Sequencer modes (step / pulse / acid) | ✅ Done | |
| 6 | Launchpad MK3 (session grid + Note mode) | ✅ Done | Full commit series through `5db31ba8` |
| 7 | MIDI preset matching + kit song load fixes | ✅ Done | `7196dbd4`, `f0338ffd` |
| 8 | Re-apply upstream `main` UX fixes on ported code | ⬜ Ongoing | Test on hardware; upstream keeps moving |

## Build / test

```bash
cd /Volumes/Development/Development/Deluge-CGFW-on-main
./dbt build
```

After each phase, verify on hardware:

- USB multi-port: connect a multi-cable interface; each port appears separately in MIDI Devices.
- MIDI routing: route a track to a specific USB port; preset matching respects output device.
- Automation generators: mod buttons select lane shape; clock multiply/divide on gold knobs.
- Sequencer modes: step / pulse / acid clip types from clip menu.
- Expressive Chords: keyboard layout + chord sets.
- Launchpad MK3: enable **Launchpad Session Grid** in community features; connect on USB port 2; session mirror + Note mode.

## Pushing

When ready to publish:

```bash
git push -u solaris76 CG-Custom-FW-on-main
```

Do **not** replace `CG-Custom-FW` until integration is tested end-to-end.

## Merge strategy notes

- Cherry-pick CG commits in dependency order; resolve conflicts keeping upstream float interpolation, horizontal menus, and OLED improvements where they don't block CG behaviour.
- Bundle commits (e.g. `998cc60d`) may need manual merges — generator + acid sequencer landed together.
- Launchpad refresh policy: Deluge hardware uses `requestSync()`; UI navigation uses rate-limited `requestSyncAfterViewChange()`; Launchpad MIDI input coalesces via `flushDeferredInputSync()`.
