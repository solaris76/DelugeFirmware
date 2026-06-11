# Nord Drum 3P — Song Load Bug (Resolved)

**Resolved:** 2026-06-09 — SONG8 and SONG9 load **6 rows** on hardware after fix v6.

---

## Symptom

Nord Drum 3P MIDI kit: **6 rows**, CC names, gold knob mappings, and per-row CC automation survived **kit preset** load but not **save song → reload song** (typically **1 row** only).

---

## Root cause

**Not malformed XML.** Song saves embed more data per `midiOutput` than standalone kit presets:

| Element | Song (SONG8) | Kit preset |
|---------|--------------|------------|
| `midiOutput` | 6 | 6 |
| `arpeggiator` per drum | **6** | **0** |
| `modKnobs` | 6 | 6 |
| `noteRow` / `midiParams` | 6 / 6 | — |

1. **Primary (1-row bug):** `NonAudioDrum::readDrumTagFromFile()` read `<arpeggiator>` on each drum (written on song save via `writeArpeggiatorToFile()`) but never called `exitTag("arpeggiator")`. XML parser mis-aligned after drum 0 → `Kit::readFromFile()` only loaded one drum → `claimOutput()` left one note row.

2. **Secondary:** `MIDIDrum` modKnob / `midiDevice` read paths used wrong `exitTag` pattern vs working `MIDIInstrument` parser.

3. **Secondary:** Kit-row `midiParams` read omitted `param = &midiParam->param` and proper `exitTag("param")` (AI-added path from Nov 2025).

4. **Save:** `writeModKnobAssignmentsToFile()` skipped writing when sibling drums still in linked list during song save walk.

---

## Fixes (committed)

| File | Change |
|------|--------|
| `non_audio_drum.cpp` | `exitTag("arpeggiator")` after arpeggiator read |
| `midi_drum.cpp/h` | ModKnob/midiDevice read `exitTag` aligned with MIDIInstrument; modKnobs save dedup removed; deferred definition file load; propagate shared settings |
| `note_row.cpp` | Kit-row `midiParams` read/write restored with correct parser |
| `kit.cpp` | `newDrum->kit = this` before read; propagate before save |
| `storage_manager.cpp/h` | `loadPendingMidiDeviceDefinitionFilesForKit/Song()` after file close |
| `load_song_ui.cpp` / `load_instrument_preset_ui.cpp` | Call deferred definition load |
| `instrument_clip.cpp` | `claimOutput`: `getDrumFromIndexAllowNull()` |

---

## What is stored where (kit MIDI rows)

| Data | Location |
|------|----------|
| Channel, note, device | `<midiOutput>` in kit `soundSources` |
| Gold knob CC mapping | `<modKnobs>` on each drum |
| CC names | `<midiDevice><definitionFile>` |
| Row → drum link | `drumIndex` on clip `noteRow` |
| CC automation | `<midiParams>` on clip `noteRow` |
| Per-drum arp settings | `<arpeggiator>` on each `midiOutput` (song saves only) |

---

## Test assets

`docs/research/test-kits/` — seven ND3P debug kits + `generate_test_kits.py`  
SD: `SONGS/SONG6.XML` … `SONG12.XML`, `KITS/_CG/External/ND3P Test *.XML`

| Song | Expected after fix |
|------|-------------------|
| SONG8, SONG9 | **6 rows** ✅ verified |
| SONG10 | 6 rows (same class as 8/9) |
| SONG11 | Inline `ccLabels` — not re-tested; may need separate check |
| SONG6, 7, 12 | Regression (no meta) |

---

## Investigation notes

- Skipping kit-row `midiParams` (fix v4) did **not** fix 1-row bug — confirmed arpeggiator was the song-vs-preset difference.
- Kit preset load never hits per-drum `arpeggiator` in test XML (generator omits them); song load always does.
