# Deluge DAW mode — abandoned WIP archive

**Date:** 2026-06-14  
**Decision:** Stop DAW-mode development on this path; return working tree to `CG-Custom-FW`.  
**Preserved on branch:** `deluge-daw-mode` @ `c39a3f3cf` (committed skeleton only). Uncommitted session WIP was discarded on checkout.

---

## Why this was abandoned

Attempted to make Deluge an Ableton control surface (16×8 clip grid + companion `Deluge_DAW` Remote Script). The approach went wrong in practice:

| Issue | What happened |
|-------|----------------|
| **No grid from Live** | Clip LEDs did not appear on Deluge even with clips in Ableton session view |
| **OLED ghosting** | Status debug lines drew on top of each other without clearing |
| **Protocol confusion** | Mixed stock Launchpad note-MIDI LED feedback with custom SysEx; unreliable and hard to debug |
| **Wrong mental model** | Treated Ableton like a Launchpad surface sending notes back, instead of replicating the **Launchpad mirror** pattern (own `padImage` → paint hardware via SysEx) with Live as source |

User testing showed the integration was not working end-to-end. Architecture discussion concluded the correct model is:

```
Ableton (source of truth) → Deluge SysEx IN → padImage[] → PadLEDs
```

Same stack as `launchpad_extension` (song → padImage → SysEx OUT → physical LP), but **inbound** from the computer and **local** pad hardware — not Novation Programmer SysEx outbound.

---

## What existed before revert

### Git branches

| Branch | Tip | Contents |
|--------|-----|----------|
| `CG-Custom-FW` | `5db31ba8c` | Launchpad work + **MIDI output-port routing** (see below). No DAW mode. |
| `deluge-daw-mode` | `c39a3f3cf` | Above + committed DAW skeleton (`deluge_daw_mode.cpp` ~724 lines, hooks, feature flag, design doc). |

### Committed on `deluge-daw-mode` (kept on branch, not on `CG-Custom-FW`)

- `docs/research/deluge-daw-mode.md` — full design (885 lines)
- `src/deluge/io/midi/device_specific/deluge_daw_mode.{h,cpp}`
- `src/deluge/io/midi/device_specific/deluge_daw_cc_map.h`
- Hooks: `buttons.cpp`, `encoders.cpp`, `matrix_driver.cpp`, `midi_engine.cpp`, `ui_timer_manager.cpp`
- Runtime feature: `EnableDelugeDawMode`
- Enter: Shift + MIDI + Learn

### Uncommitted session WIP (discarded)

Further work on top of `c39a3f3cf` in:

- `deluge_daw_mode.cpp` — SysEx grid `0xDB`, OLED clear/redraw, scroll chrome, col-17 fixes, mixer/session modes
- `deluge_daw_cc_map.h` — `kSysexCmdDawGrid`, grid sync CC 57
- `launchpad_extension.cpp` — mutual exclusion with DAW mode
- `ui_timer_manager.cpp` — DAW graphics routine

### Ableton script (local only, gitignored)

Path: `scripts/ableton/Deluge_DAW/` (not in git — `.gitignore`)

- `DelugeDAW.py` — SessionComponent 16×8, transport, mixer, browse, `_paint_session_grid()`, SysEx `0xDB` bulk grid
- `midi_map.py`, `browse.py`, `README.md`
- Install: `~/Music/Ableton/User Library/Remote Scripts/Deluge_DAW/`

Copy this folder elsewhere if you want to keep the script after cleanup.

---

## SysEx protocol sketched (for future resurrection)

| Command | ID | Direction | Purpose |
|---------|-----|-----------|---------|
| Display lines | `0xDA` | Live → Deluge | OLED status (track/preset/status lines) |
| Grid bulk | `0xDB` | Live → Deluge | 128 velocity bytes, index `x + y*16` (y=0 bottom) |
| Grid sync request | CC 57 ch16 | Deluge → Live | Ask script to repaint grid |

Header: `F0 00 21 7B 01` (existing Deluge SysEx family).

**Lesson:** Session grid rendering should be **SysEx-only** for `Deluge_DAW`. MIDI notes are for pad **input** (Deluge → Live), not LED output.

---

## MIDI output-port routing on `CG-Custom-FW` (confirmed present)

The earlier MIDI routing work **is** on `CG-Custom-FW`, committed as:

```
7196dbd4f Include MIDI output port in track preset matching.
```

Doc: `docs/research/midi-track-routing-identity.md`

### What it does

Clips/instruments match on **channel + suffix + outputDevice** (not channel alone), so e.g. Nord on USB ch1 and Blofeld on DIN ch1 no longer collide after song reload.

### Files in that commit

- `midi_device_helper.h`, `midi_routing.h`
- `midi_instrument.h` — `matchesPreset` includes `outputDevice`
- `instrument_clip.cpp/h` — read/write/claim `outputDevice` on clips
- `song.cpp/h` — `getMaxMIDIChannelSuffix` scoped to port
- `view.cpp` — channel navigation scoped to port
- `instrument.h`, `output.h`, `cv_instrument.h`, `audio_output.h`

If routing “doesn't appear to work”, check:

1. You are on **`CG-Custom-FW`** (not `deluge-daw-mode`).
2. Song was **saved after** the change (clip XML needs `outputDevice="…"`).
3. Old songs without `outputDevice` on clips still use legacy channel+suffix match only.

---

## Recommended next attempt (if resumed)

1. Branch from `CG-Custom-FW` (not from DAW WIP).
2. Copy **launchpad_extension** structure: `padImage[]`, isolated `graphicsRoutine`, block song grid.
3. **Inbound** Deluge-native SysEx only for rendering (RGB delta, like `launchpad_sysex.cpp` `DeltaBatch` but reversed).
4. Ableton script reads `clip_slots` directly; no SessionComponent LED cache, no note-MIDI LED fallback.
5. Flash/pulse for triggered clips in firmware blink service.
6. Scene column (col 16) in same SysEx batch as main grid.

---

## How to recover DAW work later

```bash
git checkout deluge-daw-mode          # committed skeleton + design doc
git show c39a3f3cf                    # initial DAW commit
# scripts/ableton/Deluge_DAW/         # local copy if still on disk
```

Do **not** merge `deluge-daw-mode` into `CG-Custom-FW` without a full rethink of the rendering path.
