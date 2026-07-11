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

**`CG-Custom-FW-on-main`** — integration branch based on upstream `main`.

## Why a separate folder?

- Keeps the working v1.6.0 custom fork buildable while integration proceeds.
- Avoids risky merges on the branch you flash to hardware today.
- Makes it easy to compare behaviour side-by-side.

## Architecture note (important)

Upstream `main` and the old `community` branch **diverged before the MIDI root-complex refactor**. All custom CG work (USB multi-port, Launchpad, MIDI routing) assumes `root_complex` (`root_usb`, `root_din`, etc.).

**Phase 0 (prerequisite)** must land before any custom feature ports:

1. `72643e56` — Create MIDI root complex abstraction (#3157) ✅
2. `c600b073` — fix sysex on third USB MIDI interface (#3999) ✅
3. `619d7bf4` — upstream USB port 2 defaults — **skipped** (already on `main` as receiveClock work)

## Porting phases

| Phase | Feature | Status | Notes |
|-------|---------|--------|-------|
| 0 | MIDI root_complex from `community` | ✅ Done | Required foundation |
| 1 | USB host multi-port MIDI | ✅ Done | MRCC 880 / multi-cable devices |
| 2 | MIDI output device selection + `MIDIRouting` | ⬜ Next | Per-track/port routing; needed before Launchpad |
| 3 | Automation generator lanes | ⬜ | |
| 4 | Expressive Chords keyboard layout | ⬜ | |
| 5 | Sequencer modes (step / pulse / acid) | ⬜ | |
| 6 | Launchpad MK3 (session grid + Note mode) | ⬜ | Highest conflict risk; depends on phases 1–2 |
| 7 | Re-apply upstream `main` UX fixes on ported code | ⬜ | e.g. MIDI Devices OLED scroll, horizontal menus |

## Phase 1 commits (applied)

- `c1eeb014` — Add USB host multi-port MIDI support for interfaces like MRCC 880
- `bcccb092` — Add USB multi-port MIDI design notes

## Phase 2 source commits (from `cgfw/CG-Custom-FW`)

Cherry-pick or manual port, in order:

- `0fb4b74b1` — MIDI output device selection for instrument tracks / kit rows
- `b64744756` — Replace bitmask with `MIDIRouting`
- `b23d92372` — Clean up MIDI device selection
- `ad2c5715b` — `midi_device_helper` deduplication
- `aff459fba` — extern declaration fix

New files expected: `midi_routing.h`, `midi_device_helper.h`, `output_device_selection.cpp/h`, engine/instrument changes.

## Build / test

```bash
cd /Volumes/Development/Development/Deluge-CGFW-on-main
./dbt build
```

After each phase, verify on hardware:

- USB multi-port: connect a multi-cable interface; each port appears separately in MIDI Devices.
- MIDI routing (phase 2): route a track to a specific USB port.
- Launchpad (phase 6): session grid mirror on USB port 2.

## Pushing

When ready to publish:

```bash
git push -u solaris76 CG-Custom-FW-on-main
```

Do **not** replace `CG-Custom-FW` until integration is tested end-to-end.
