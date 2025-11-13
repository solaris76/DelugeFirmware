# Deluge SysEx API Reference

Complete bidirectional SysEx API for the Synthstrom Audible Deluge, enabling external control and monitoring via USB MIDI.

## Overview

This API provides comprehensive access to:
- **68 settings** across 9 categories (CV, Gate, Clock, MIDI, Defaults, Pads, Recording, Community Features, UI)
- **10 transport commands** (playback control, looping, undo/redo)
- **File operations** (read/write songs, samples, firmware upload)
- **Real-time sync** (tempo encoder changes feed back to connected apps)

## Connection

**Port:** Use Deluge **USB Port 3** (dedicated SysEx port)  
**Format:** All commands use JSON over SysEx  
**Header:** `F0 00 21 7B 01 04 <MSGID> <JSON> F7`

Where:
- `F0` = SysEx start
- `00 21 7B 01` = Synthstrom Deluge manufacturer ID
- `04` = JSON command
- `<MSGID>` = Message ID (1-127, for request/response matching)
- `<JSON>` = UTF-8 JSON payload
- `F7` = SysEx end

## Transport Commands

### Basic Playback
```javascript
{transport: {command: "play"}}         // Start playback
{transport: {command: "stop"}}         // Stop playback
{transport: {command: "record"}}       // Start recording
{transport: {command: "restart"}}      // Restart from beginning
```

### Tempo & Timing
```javascript
{setTempo: {bpm: 120}}                 // Set tempo (30-300 BPM)
{setSwing: {swing: 25}}                // Set swing (-50 to +50)
{setMetronome: {enabled: 1}}           // Toggle metronome (0/1)
```

### Loop & Performance
```javascript
{transport: {command: "loop"}}         // Standard loop
{transport: {command: "loopLayering"}} // Continuous layering loop
{transport: {command: "fill"}}         // Trigger fill mode
```

### Edit
```javascript
{transport: {command: "undo"}}         // Undo last action
{transport: {command: "redo"}}         // Redo action
```

### Query & Subscribe
```javascript
{getTransportState: {}}                // Get current state
// Response: {"^transportState": {"playing": 0, "recording": 0, "bpm": 120, "swing": 0, "metronomeOn": 0}}

{subscribeTransport: {}}               // Subscribe to transport changes
{unsubscribeTransport: {}}             // Unsubscribe
// Notifications: {"^tempoChanged": {"bpm": 125}}, {"^transportChanged": {...}}
```

## Settings Commands

### Grouped Settings (9 categories)

**CV Settings (6 settings)**
```javascript
{getSettingsCV: {}}
// Response: {"^settingsCV": {"cv1v": 100, "cv2v": 100, "cv1t": 0, "cv2t": 0, "cv1c": 0, "cv2c": 0}}
```
- `cv1v`, `cv2v` - Volts per octave (0-200, 100 = 1V/Oct, 0 = Hz/V mode)
- `cv1t`, `cv2t` - Transpose (-24 to +24 semitones)
- `cv1c`, `cv2c` - Cents (-50 to +50)

**Gate Settings (5 settings)**
```javascript
{getSettingsGate: {}}
// Response: {"^settingsGate": {"g0": 0, "g1": 0, "g2": 0, "g3": 0, "goff": 10}}
```
- `g0`, `g1`, `g2`, `g3` - Gate type (0=V-Trig, 1=S-Trig, 2=Trigger)
- `goff` - Min gate off time (0-50 ms)

**Trigger Clock Settings (5 settings)**
```javascript
{getSettingsClock: {}}
// Response: {"^settingsClock": {"cas": 1, "cip": 24, "cop": 24, "mco": 1, "tmm": 0}}
```
- `cas` - Clock auto-start (0/1)
- `cip` - Analog clock in PPQN (1-96)
- `cop` - Analog clock out PPQN (1-96)
- `mco` - MIDI clock out enabled (0/1)
- `tmm` - Tempo magnitude matching (0/1)

**MIDI Settings (26 settings)**
```javascript
{getSettingsMidi: {}}
// Response: {"^settingsMidi": {"mt": 0, "mc": 1, "mto": 0, "mskr": 0, "gmc0c": 255, "gmc0n": 255, ...}}
```
- `mt` - MIDI thru (0/1)
- `mc` - MIDI clock in (0/1)
- `mto` - MIDI takeover mode (0=Jump, 1=Pickup, 2=Scale, 3=Relative)
- `mskr` - MIDI select kit row on note (0/1)
- `gmc0c`-`gmc10c` - Global command channels (255 = not learned)
- `gmc0n`-`gmc10n` - Global command notes (255 = not learned)

**Defaults (11 settings)**
```javascript
{getSettingsDefaults: {}}
```
- `ds` - Default scale (0-11)
- `dv` - Default velocity (0-127)
- `dm` - Default magnitude/resolution (-3 to 3)
- `br0`, `br1` - Bend range up/down (0-96 semitones)
- `mv` - Metronome volume (0-127)
- `si` - Swing interval (0-255)
- `ssm` - Startup song mode (enum)
- `nct` - New clip type (enum: Synth/Kit/MIDI/CV)
- `ulct` - Use last clip type (0/1)
- `pcp` - Patch cable polarity (0=Bipolar, 1=Unipolar)

**Pads (7 settings)**
```javascript
{getSettingsPads: {}}
```
- `cact`, `cstp`, `cmut`, `csol`, `cfil`, `conc` - Pad colors (0-5: Red, Orange, Yellow, Green, Blue, Magenta)
- `curs` - Cursor flash (0=Off, 1=Slow, 2=Fast)

**Recording (6 settings)**
```javascript
{getSettingsRecording: {}}
```
- `rq` - Record quantize level (0-8)
- `acm` - Audio clip record margins (0/1)
- `cib` - Count-in bars (0-4)
- `mon` - Monitor mode (0=Smart, 1=On, 2=Off)
- `trm` - Threshold recording mode (enum)
- `lrc` - Loop recording command (enum)

**Community Features (22 settings)**
```javascript
{getSettingsCommunity: {}}
// Response: {"^settingsCommunity": {"cf0": 1, "cf1": 1, "cf2": 1, ...}}
```
All community runtime features (cf0-cf21):
- cf0: Drum Randomizer
- cf1: Quantize
- cf2: Fine Tempo Knob
- cf3: Catch Notes
- cf4: Delete Unused Kit Rows
- cf5: Alt Golden Knob Delay Params
- cf6: Dev SysEx Allowed
- cf7: Sync Scaling Action
- cf8: Highlight Incoming Notes
- cf9: Display Norns Layout
- cf10: Shift Is Sticky
- cf11: Light Shift LED
- cf12: DX7 Engine Enabled
- cf13: Emulated Display
- cf14: Keyboard View Sidebar Exit
- cf15: Launch Event Playhead
- cf16: Display Chord Keyboard
- cf17: Alternative Playback Start
- cf18: Grid View Loop Pads
- cf19: Alternative Tap Tempo
- cf20: Horizontal Menus
- cf21: Trim From Start of Audio Clip

**UI Settings (17 settings)**
```javascript
{getSettingsUI: {}}
```
- `pb` - Pad brightness (1-25)
- `kl` - Keyboard layout (0-3)
- `sl` - Session layout (0-2)
- `ht` - Hold time (1-20)
- `sbp` - Sample browser preview (0=Off, 1=Only when not playing, 2=On)
- `sm` - Default slice mode (enum)
- `gam` - Grid active mode (enum)
- `fav` - Favorites layout (enum)
- `geu`, `gecr`, `gags` - Grid options (0/1)
- `kvg`, `kmg` - Keyboard glide options (0/1)
- `ac` - Accessibility shortcuts (0/1)
- `amh` - Menu highlighting (enum)
- `cpu` - CPU usage indicator (0/1)
- `sharp` - Use sharps vs flats (0/1)

### Get All Settings
```javascript
{getSettings: {}}
// Returns all 68 settings in one response
```

### Individual Setting Operations
```javascript
{setSetting: {name: "pb", value: 15}}  // Set pad brightness to 15
{getSetting: {name: "pb"}}             // Get current pad brightness
```

## File Operations

```javascript
{dir: {path: "/SONGS"}}                // List directory
{open: {path: "/SONGS/SONG001.XML", write: 0}}  // Open for reading
{read: {fid: 1, addr: 0, size: 1024}}  // Read 1024 bytes
{write: {fid: 1, addr: 0, size: 128}}  // Write 128 bytes (+ binary payload)
{close: {fid: 1}}                      // Close file
{delete: {path: "/SONGS/OLD.XML"}}     // Delete file
```

## Firmware Info

```javascript
{getFirmwareVersion: {}}
// Response: {"^firmware": {"major": 1, "minor": 3, "patch": 0, "type": "community"}}
```

## Real-Time Notifications

When subscribed to transport changes, the Deluge sends automatic notifications:

```javascript
{"^tempoChanged": {"bpm": 125}}        // Tempo encoder was turned
{"^transportChanged": {                 // Transport state changed
  "playing": 1, 
  "recording": 0, 
  "bpm": 125, 
  "swing": 0, 
  "metronomeOn": 1
}}
```

## Implementation Details

### Files
- `src/deluge/io/midi/sysex/transport_sysex.h/cpp` - Transport control
- `src/deluge/io/midi/sysex/settings_sysex.h/cpp` - Settings control
- `src/deluge/storage/smsysex.cpp` - Command dispatcher
- `src/deluge/model/song/song.cpp` - Tempo change hook

### Key Features
- **Reentrancy safe**: Separate `notifyWriter` for async notifications
- **LED sync**: Metronome button lights match state
- **Auto-loop**: Community features automatically expose new settings
- **Grouped access**: Match Deluge menu structure for organized access
- **Bidirectional**: Hardware encoder changes feed back to connected apps

### Testing
Use `sysex-test-ui.html` (in project root) for comprehensive testing of all commands.

## Browser Compatibility

- **Chrome/Edge**: Full support (recommended)
- **Opera**: Full support
- **Firefox**: Requires Web MIDI enable in about:config
- **Safari**: Limited Web MIDI support

## Notes

- Use **Port 3** (rightmost USB) for best SysEx performance
- File operations require firmware 1.3+ (Community) or 4.0+ (Official)
- Some operations (undo/redo) are "pended" to avoid crashes during audio processing
- Song loading via SysEx is not supported (use file read/write instead)

## Future Expansion

Planned modules:
- **Parameter Control** - Full synth/FX parameter access (200+ params)
- **Clip Management** - Create, delete, move clips
- **Note Editing** - Sequencer note manipulation
- **Automation** - Automation curve editing

---

**Repository:** https://github.com/solaris76/DelugeFirmware  
**Branch:** `sysex`  
**Test UI:** `../sysex-test-ui.html`

