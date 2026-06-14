# MIDI track routing identity (channel + output port)

**Status:** Implemented in CG-Custom-FW (uncommitted at time of writing).

## Problem

Multi-USB MIDI output lets different tracks send to different physical ports. Track identity for clip↔instrument matching used **MIDI channel + suffix only**, so e.g. Nord on USB ch1 and Blofeld on DIN ch1 collided after song reload.

Same port + same channel + same suffix (e.g. two DIN ch8 synths) is **not** a firmware bug — one physical destination, one Deluge routing slot. Use different hardware channels or suffixes.

## Identity key

| Field | Role |
|-------|------|
| `channel` | MIDI channel 0–15 |
| `channelSuffix` | -1…25 for multiple tracks same channel **on same port** |
| `outputDevice` | 0=ALL, 1=DIN, 2+=USB cable index |
| `outputDeviceName` | Stable match when USB order changes |

**Match rule (`MIDIInstrument::matchesPreset`):**

- If clip has **no** `outputDevice` in XML (old songs): channel + suffix only (legacy).
- If clip specifies `outputDevice`: channel + suffix + **outputDevice** must match.

## Song XML (instrumentClip)

New attributes on MIDI clips (saved from firmware):

```xml
<instrumentClip midiChannel="7" outputDevice="2" outputDeviceName="H4MIDI-WC port 1" ...>
```

Instrument block unchanged (`<outputDevice device="…" deviceName="…"/>` inside `<midi>`).

## Code touchpoints

| File | Change |
|------|--------|
| `midi_device_helper.h` | `kMIDIOutputDeviceMatchUnspecified` (255) |
| `midi_instrument.h` | `matchesPreset` includes outputDevice |
| `song.cpp` | `getInstrumentFromPresetSlot`, `getMaxMIDIChannelSuffix(channel, device)` |
| `instrument_clip.cpp` | Read/write/claimOutput outputDevice on clips |
| `view.cpp` / `song.cpp` | Channel/suffix navigation scoped to port |

## Test matrix

| Scenario | Expected |
|----------|----------|
| Nord USB ch0 + synth DIN ch0 | Both clips load to correct instruments after save/reload |
| Two DIN ch8, suffix -1 and 0 | Still works (suffix scoped per port) |
| Two DIN ch8, both suffix -1 | First match wins (correct — same destination) |
| Old song, no clip `outputDevice` | Legacy channel+suffix match |
