# Deluge SysEx Developer Guide

This guide consolidates everything currently available through the Deluge JSON‑over‑SysEx interface so hardware & software developers can build their own controllers, desktop tools, or browser UIs (e.g. JUCE apps, Launchpad integrations, or our own `tools/sysex-test-ui.html`). Use it alongside `docs/dev/SysExProtocolNotes.md` for low‑level framing details.

---

## 1. Transport Layer Basics

| Item | Details |
| --- | --- |
| Manufacturer ID / family | `F0 00 21 7B 01` (Synthstrom) |
| JSON payload | Always a single top-level key (command) whose value is an object of parameters. |
| Responses | Same structure but caret-prefixed key, e.g. `{ "^transportState": { ... } }`. |
| Sequencing | Host increments a 7-bit sequence number (1‑127). Deluge replies with the same seq. Async notifications use seq `0`. |
| Binary payloads | Optional (after a `0x00` separator) for file read/write; still 7-bit packed. |

**Common Helpers**

- `SysexCommon::startResponse` + `writeStatus` keep all firmware replies consistent.
- On the host side, `JsonReplyHandler` (web UI) or your own sequencer should map replies back to requests via the sequence byte.

---

## 2. Capability Overview

| Area | Key Commands (request → response) | Notes |
| --- | --- | --- |
| Connectivity & misc | `ping`, `getFirmwareVersion` | Handshake, versioning. |
| File I/O | `dir`, `open`, `read`, `write`, `close`, `delete`, `mkdir`, `rename` | Uses optional binary payload for block transfers. |
| System settings | `getSettings*`, `setSetting`, `getSetting` | Includes CV/Gate/Clock/Pads/Recording/UI groups. |
| Transport | `transport`, `getTransportState`, `setTempo`, `setSwing`, `setMetronome`, `subscribeTransport`, `unsubscribeTransport` | Control playback + subscribe for play/record/tempo changes. |
| Song scale / root | `getSongScale`, `setSongScale` | New in this branch; exposes `Song::setRootNote` / `setScale`. |
| Parameter editing | `getParameters`, `setParameter`, `subscribeParameters`, `^parameterChanged` | Works for whichever synth/kit clip is focused. |
| Kits / drums | `getKitInfo`, `getKitDrums`, `getDrumInfo`, `addDrum`, `setDrumProperty`, `setDrumSample`, `getDrumParameters`, `setDrumParameter`, `subscribeKit`, `subscribeDrumParams` | `getDrumParameters` now returns full filter drive/mode etc. |
| Clips & tracks | `getClips`, `getTracks`, `createClip`, `duplicateClip`, `moveClip`, `deleteClip`, `launchSection`, `enterClip`, `exitClip`, `setClipColour`, `setTrackColour` | All clip IDs now include `clipId`, `trackId`, `section`, `length`, names, colours. |
| Notes | `getNotes`, `setNotes` | Batch add/update/delete with `noteId`, `rowId`, `start`, `length`, `velocity`. |
| Transport notifications | `^transportState`, `^tempoChanged`, `^transportChanged`, `^metronomeSet` etc. | Emitted when subscribed. |

---

## 3. Connecting from Your App

### Web MIDI (browser)
1. Request MIDI access with SysEx enabled: `navigator.requestMIDIAccess({ sysex: true })`.
2. Pick the Deluge port (port 3 is the dedicated SysEx pipe).
3. For each JSON command, wrap the payload between the manufacturer header and `0xF7`, optionally including a sequence byte and binary section.
4. Convert JSON to UTF-8 bytes; log everything (especially while debugging).
5. Parse replies by slicing bytes 7..n-1 and passing the result to `JSON.parse`.

### JUCE / Native apps
1. Use `MidiOutput::sendMessageNow()` with the same header + payload.
2. Maintain your own sequence counter and timeout logic (the firmware does not queue multiple outstanding replies per cable).
3. For binary transfers (e.g. uploading `deluge.bin` to SD), reuse the 7->8 encoding logic from `FileRoutines.js` or port it directly.

### Hardware integrations (Launchpad/MCU)
1. Use a small MCU that can present itself as a USB MIDI interface with SysEx capability.
2. Implement the same 7-bit packet wrapper.
3. For real-time clip feedback, subscribe to `^transportChanged` and poll `getClips` / `getTracks` as needed.

---

## 4. Transport & Song-Level Control

### Commands
- `{"transport":{"command":"play|stop|record|restart|loop|loopLayering|fill|undo|redo|tap"}}`
- `{"setTempo":{"bpm":120}}`
- `{"setSwing":{"swing":25}}`
- `{"setMetronome":{"enabled":1}}`
- `{"getTransportState":{}}`
- `{"subscribeTransport":{}}` → Deluge pushes `^transportChanged` + `^tempoChanged`.
- `{"setSongScale":{"rootNote":60,"scale":0}}`
- `{"getSongScale":{}}`

### Responses
```json
{"^transportState":{"playing":0,"recording":0,"bpm":120,"swing":25,"metronomeOn":0}}
{"^songScale":{"rootNote":60,"scale":0,"scaleName":"MAJOR"}}
{"^songScaleSet":{"status":"success","rootNote":60,"scale":9,"scaleName":"HUNGARIAN MINOR"}}
```

### Tips
- `setSongScale` prefers `scale` indexes (0‑15 = preset, 16 = USER). You can also send `scaleName`.
- Always include both root and scale when building UI forms so they stay in sync with song-level state.

---

## 5. Parameter Editing

| Command | Usage |
| --- | --- |
| `getParameters` | Dumps the currently focused synth/kit clip. |
| `setParameter` | `{ "setParameter": { "name": "lpfFrequency", "value": 905969664 } }` |
| `subscribeParameters` | Push-style updates (`^parameterChanged`). |

**Notes:**
- JSON serializer now escapes everything, so large dumps parse reliably.
- You’ll still see full `^parameters` dumps if your UI auto-refreshes after each change; consider debouncing.
- Parameter names are human-friendly thanks to `paramNameForFile`.

---

## 6. Clip / Track Management

### Discover
```json
{"getClips":{}}
{"^clips":{"status":"success","count":4,"clips":[
  {"index":0,"clipId":539002024,"trackId":539007440,"type":"synth","section":0,"length":384,
   "colour":54,"trackColour":68,"name":"","trackName":"Bass"}
]}}

{"getTracks":{}}
{"^tracks":{"status":"success","tracks":[
  {"trackId":539007440,"name":"Bass","type":"synth","colour":68,"clipIds":[539002024,539075776]}
]}}
```

### Mutate
- `createClip`: accepts `type`, `index`, optional `presetPath`, `drumIndex`. Newly created clips default to section 0 so they appear on the first row.
- `duplicateClip`: `{ "duplicateClip": { "clipId": 539002024, "targetTrackId": 539007440, "insertPosition": 1 } }` (section auto-advances if omitted).
- `moveClip`, `deleteClip`, `launchSection`, `enterClip`, `exitClip`.
- Colour controls:
  - `setClipColour` → `Clip::colourOffset` (row/automation hue).
  - `setTrackColour` → `Output::colour` (session pad hue).

### UI Guidance
- Always display both `Track ID` and `Section` so remote launchpads know where to place pads.
- When replicating Deluge behaviour, follow these conventions:
  - Section = column (0‑15).
  - `clipId` is unique per clip; `trackId` groups clips that share an instrument.
  - `createClip` on an empty slot implicitly creates a new track/instrument, just like pressing `NEW` on hardware.

---

## 7. Note Editing

### getNotes
```json
{"getNotes":{"clipId":539002024}}
{"^notes":{
  "status":"success",
  "clipId":539002024,
  "length":384,
  "scaleMode":1,
  "scaleType":0,
  "rootNote":60,
  "notes":[
    {"noteId":"12:0","rowId":12,"y":60,"start":0,"length":96,"velocity":100}
  ]
}}
```

### setNotes
Payload is an array of `{ op, noteId?, rowId?, y, start, length, velocity }`.
```json
{"setNotes":{
  "clipId":539002024,
  "notes":[
    {"op":"add","rowId":12,"y":60,"start":192,"length":96,"velocity":110},
    {"op":"update","noteId":"12:0","length":120,"velocity":90},
    {"op":"delete","noteId":"12:0","start":0}
  ]
}}
{"^notesSet":{"status":"success","successCount":3,"errorCount":0}}
```

**Implementation Notes**
- `noteId` encodes `rowId:pos` to stay stable even if row pointers change.
- For adds, `rowId` defaults to `y` if omitted.
- `setNotes` batches operations and reports `successCount` / `errorCount` for easy auditing.

---

## 8. Kit & Drum Controls

| Command | Description |
| --- | --- |
| `getKitInfo`, `getKitDrums` | Retrieve high-level kit metadata and per-drum stats. |
| `addDrum`, `removeDrum`, `setDrumProperty` | Create/adjust kit rows (MIDI note/channel, arp mode, etc.). |
| `setDrumSample` | Load a sample into a row. |
| `getDrumParameters`, `setDrumParameter` | Full parameter snapshot including filter drive/mode. |
| `subscribeKit`, `subscribeDrumParams` | Receive `^kitChanged`, `^drumChanged`, parameter pushes. |

**Preset loading**
- When creating a new clip, you can pass `presetPath` and optional `drumIndex`; the firmware calls `StorageManager::loadInstrumentFromFile` / `loadSynthToDrum` and returns status flags in `^clipCreated`.

---

## 9. File & Firmware Utilities

These mirror the legacy “web dev kit” functionality:
- `dir`, `open`, `read`, `write`, `close`, `delete`, `rename`, `mkdir`.
- Use `write` with binary payloads to upload firmware (`deluge.bin`) or presets to SD quickly.
- `tools/sysex-test-ui.html` includes helper routines for chunked upload/download; use that as sample code if you’re porting to another platform.

---

## 10. Notifications & Subscriptions

| Subscription | Command | Notifications |
| --- | --- | --- |
| Transport | `subscribeTransport` | `^transportChanged`, `^tempoChanged`, `^metronomeSet` |
| Parameters | `subscribeParameters` | `^parameterChanged` |
| Kits / drums | `subscribeKit`, `subscribeDrumParams` | `^kitChanged`, `^drumChanged`, `^drumParameters` |
| (Planned) Clips / Notes | TODO | `^clipChanged`, `^notesChanged` (future roadmap) |

Always unsubscribe before disconnecting to avoid stale cable references on the firmware side.

---

## 11. UI Design Tips

- **State bootstrapping**: upon connect, request `getTransportState`, `getSongScale`, `getClips`, `getTracks`, and any subscriptions you need. The test UI’s `runInitialTests()` is a good template.
- **Debounce heavy calls**: `getParameters`, `getClips`, and `getDrumParameters` can be large; avoid polling faster than necessary.
- **Sections vs rows**: the Deluge Session grid is [track,row] = [column, row]. Always include `section` (column index) in your UI tables so Launchpad‑style controllers place pads correctly.
- **Colour handling**: clip colour offset (0‑71) is separate from track hue (0‑191). Expose both if you want full parity with hardware.
- **Preset loading**: for `createClip`, allow users to type/paste SD paths. Firmware replies include `presetLoaded` + `presetError` so you can display meaningful toast messages.

---

## 12. Future Roadmap Hooks

See `docs/dev/sysex_roadmap.md` for planned features, including:
- Clip metadata subscriptions, follow actions, armed state.
- Row-level iterance/probability, per-note probability.
- Automation streaming via `SysexParamStream`.
- Kit create/load/save flows.

Keeping your host app modular will make it easier to adopt these APIs as they land.

---

## 13. References

- `docs/dev/SysExProtocolNotes.md` – byte-level framing.
- `tools/sysex-test-ui.html` – comprehensive sample UI.
- Firmware sources:
  - `src/deluge/io/midi/sysex/transport_sysex.cpp`
  - `src/deluge/io/midi/sysex/clip_sysex.cpp`
  - `src/deluge/io/midi/sysex/kit_sysex.cpp`
  - `src/deluge/storage/smsysex.cpp`
- Community discussions & roadmap: `docs/dev/sysex_roadmap.md`.

Happy hacking!

