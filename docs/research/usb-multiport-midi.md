# USB multi-port hosted MIDI — design notes

**Status:** Parked (2026-06-02). Branch reset to `solaris76/CG-Custom-FW` @ v1.5.3.  
**Prior WIP:** stashed as `stash@{0}` — *"multiport midi wip + matriceal research"*.  
**Prior commit (not on branch):** `8366b1339` — *USB: multi-port hosted device support…*

---

## Problem

Many USB MIDI devices expose **multiple virtual cables** (embedded jacks) — e.g. OXI One (4 ports), multi-timbral synths, aggregate controllers. On current Deluge firmware:

1. **One logical device per physical USB attachment** — all virtual cables map to the same `MIDICableUSBHosted` instance.
2. **Receive path collapses to cable 0** — incoming cable number is parsed but falls back when `cable > maxPortConnected`.
3. **MIDI Devices menu** lists one entry per hosted device name, not per port.
4. **Routing / deviceFilter** indexes by hosted-device slot, not by `(device, port)`.

User-visible symptom: connect a 4-port device → only one port works; cannot route track A to port 1 and track B to port 2.

---

## USB MIDI cable model (reminder)

USB MIDI packets are 4 bytes: `[CableNumber:4 | CIN:4] [Status] [Data1] [Data2]`.

- **Cable number** (0–15) selects the virtual jack within one USB device.
- Deluge already encodes `portNumber` on **send** via `MIDICableUSB::sendMessage()` (`usb_common.cpp`).
- **Receive** in `usb_hosted.cpp` extracts `cable` from the packet but currently falls back to 0.

---

## Current architecture (v1.5.3 baseline)

```
InstrumentClip / routing
        ↓
   MidiEngine::sendUsbMidi(deviceFilter)
        ↓
   root_usb->getCable(index)  →  MIDICableUSBHosted
        ↓
   MIDICableUSB::sendMessage()  →  portNumber in USB cable nibble
        ↓
   ConnectedUSBMIDIDevice::bufferMessage()  →  USB host driver
```

| Component | Role |
|-----------|------|
| `MIDIRootComplexUSBHosted` | Owns `hostedMIDIDevices_` vector; `getNumCables()` = element count |
| `hostedDeviceConfigured()` | Creates **one** `MIDICableUSBHosted`, assigns **same pointer** to `cable[0..maxPortConnected]` |
| `ConnectedUSBMIDIDevice` | `cable[4]`, ring buffer, USB I/O |
| `devices.cpp` | Index 0 = DIN; index 1+ = `root_usb->getCable(n-1)` |
| `getOrCreateHostedMIDIDeviceFromDetails()` | Merges by VID:PID — blocks multiple entries for same hardware |

Key files:

- `src/deluge/io/midi/midi_device_manager.cpp` — attach/detach, device registry
- `src/deluge/io/midi/root_complex/usb_hosted.cpp` — poll/receive, flush/send
- `src/deluge/io/midi/cable_types/usb_common.cpp` — `portNumber` on send
- `src/deluge/io/midi/midi_engine.cpp` — `deviceFilter` routing
- `src/deluge/gui/menu_item/midi/devices.cpp` — device picker UI

---

## Target behaviour

1. **Detect port count** at attach time (descriptor parse + driver `maxPortConnected` fallback).
2. **Expose each port as a separate selectable cable** in MIDI Devices — e.g. `"OXI One port 1"` … `"port 4"`.
3. **Each port** is its own `MIDICableUSBHosted` with `portNumber` set; all share the same USB `ConnectedUSBMIDIDevice` and `connectionFlags` bit.
4. **Receive** routes to `connectedDevice.cable[cable]` without fallback (within `maxPortConnected`).
5. **Persistence** — song/settings reference device by name (+ VID/PID); per-port names must be stable.
6. **Cap** — practical limit of **6 visible hosted ports** total (RAM / UI); document if truncated.

Non-goals for v1:

- Per-port MIDI channel in device settings (still clip-level / cable-level channel).
- Changing upstream (Deluge-as-peripheral) port model — already 3 upstream cables.

---

## Recommended approach (incremental, stay on root_complex)

Avoid the large architectural rollback in commit `8366b1339` (global `hostedMIDIDevices`, USB code duplicated into `midi_engine.cpp`). Instead, extend the **existing** `root_usb` design.

### Phase 1 — Attach & registry

**File:** `midi_device_manager.cpp` → `hostedDeviceConfigured()`

1. Parse configuration descriptor for `bNumEmbMIDIJack` (class-specific endpoint MS_GENERAL, `bType == 0x25`, `subType == 0x01`).
2. `ports = max(detectedCables - 1, maxPortConnected)`, clamp 0–5.
3. For each port `i` in `0..ports`:
   - Build name: `"{baseName} port {i+1}"`.
   - Call `getOrCreateHostedMIDIDeviceFromDetails(&perPortName, vid, pid)`.
   - Set `perPortDevice->portNumber = i`.
   - `connectedDevice->cable[i] = perPortDevice`.
   - `perPortDevice->connectedNow(midiDeviceNum)`.

**File:** `getOrCreateHostedMIDIDeviceFromDetails()`

- If `gotAName`: search by name only; **do not** merge by VID:PID (allows `"OXI port 1"` and `"OXI port 2"` with same IDs).
- If no name: keep VID:PID merge for anonymous attach.

**File:** `midi_device_manager.h`

- Consider `cable[4]` → `cable[6]` if descriptor reports >4 jacks (match cap).

**File:** `hostedDeviceDetached()`

- Clear all `cable[i]` up to previous `maxPortConnected`; remove or disconnect per-port entries from vector (careful: shared settings — prefer clearing `connectionFlags` bit only, keep vector entries for song recall).

### Phase 2 — Receive routing

**File:** `usb_hosted.cpp` → `poll()`

- Remove “fallback to cable 0” when `cable > maxPortConnected` (drop invalid packets instead).
- Sysex: route via `connectedDevice.cable[cable]` when valid, not always `cable[0]` (see existing XXX comment ~line 273).

### Phase 3 — UI

**File:** `devices.cpp`

- Current model (index 0 = DIN, 1+ = `getCable(n-1)`) should work **once** `hostedMIDIDevices_` contains per-port entries.
- Verify OLED scroll when >4 devices (prior WIP fixed `currentScroll` / wrap — cherry-pick UI hunks only).
- 7-segment: scrolling name must fit (`"MATX"`-style truncation via `getDisplayName()`).

### Phase 4 — Output routing audit

**File:** `midi_engine.cpp` → `sendUsbMidi(..., deviceFilter)`

- `deviceFilter 2+` maps to USB cable index — confirm index aligns with per-port vector order after attach.
- **Broadcast** (`deviceFilter == 0`): should still fan out to all cables; no change expected.
- **MPE / clock / sysex**: regression-test on multi-port device.

### Phase 5 — Persistence & song compat

- `readDeviceReferenceFromFile()` matches by name/VID/PID — per-port names must round-trip.
- Old songs referencing `"OXI One"` (single name): on load, match base device or first port; document migration behaviour.
- Flash storage for learn targets: `writeToFlash()` on `MIDICableUSBHosted` — ensure port identity survives.

---

## Prior WIP commit (`8366b1339`) — lessons

| What it did | Concern |
|-------------|---------|
| Descriptor-based port count | Good — reuse |
| Per-port `MIDICableUSBHosted` + `portNumber` | Good — reuse |
| No VID:PID merge when name present | Good — reuse |
| Global `hostedMIDIDevices` outside `root_usb` | **Avoid** — fights current refactor |
| Moved USB flush/send into `midi_engine.cpp` | **Avoid** — duplicates `usb_hosted.cpp` |
| Replaced `root_din` / `root_usb` with static upstream cables | **Avoid** — large blast radius |

**Stash state** was a partial revert of that commit plus extra `usb_hosted.cpp` tweaks — treat stash as reference, not apply wholesale.

### Recover prior work

```bash
# List stash
git stash list

# Inspect WIP diff (do not apply blindly)
git stash show -p stash@{0}

# Or view the old commit
git show 8366b1339
```

---

## Descriptor parsing snippet (from WIP)

Walk config descriptor; on CS endpoint MS_GENERAL:

```cpp
if (bType == 0x25 && bLen >= 5) {
    uint8_t subType = cfg[pos + 2];
    if (subType == 0x01) {  // MS_GENERAL
        uint8_t numJacks = cfg[pos + 3];
        detectedCables = max(detectedCables, numJacks);
    }
}
ports = (detectedCables > 0) ? (detectedCables - 1) : connectedDevice->maxPortConnected;
```

Requires: `extern` access to `g_p_usb_hmidi_config_table[ip]` (see `r_usb_hmidi.h`).

---

## Test plan

| Test | Pass criteria |
|------|---------------|
| Single-port device (WIDI, etc.) | Unchanged behaviour; one menu entry |
| 4-port device (OXI One) | Four entries in MIDI Devices; each sends on correct cable |
| Receive on port 2 | Learn / MIDI thru / clip input sees port 2 only |
| Song save/reload | Per-port routing restored |
| Hub + two MIDI devices | Both attach; ports enumerated independently |
| MPE synth on port 1 | Note + pitch bend on same port |
| Clock to all ports | Broadcast still works |
| OLED device list scroll | Can reach all entries |
| RAM / attach failure | Graceful cap at 6 ports; no crash |

---

## Implementation checklist

- [ ] Phase 1: `hostedDeviceConfigured` + `getOrCreateHostedMIDIDeviceFromDetails`
- [ ] Phase 1: `cable[6]` if needed
- [ ] Phase 2: `usb_hosted.cpp` receive + sysex routing
- [ ] Phase 3: `devices.cpp` scroll/wrap polish
- [ ] Phase 4: `deviceFilter` index audit
- [ ] Phase 5: song/flash persistence
- [ ] Hardware test matrix above
- [ ] Single focused commit; push to `CG-Custom-FW`

---

## Related docs

- `docs/research/oxi-split-protocol.md` — OXI One USB port layout (if using OXI as primary test device)
- `docs/research/launchpad-mk3-usb.md` — Novation Launchpad MK3 **dual MIDI interface** support (different from virtual cables)
- Matriceal WIP is in the same stash under `docs/research/matriceal-wip/` (unrelated)
