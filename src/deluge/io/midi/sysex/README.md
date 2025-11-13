# Deluge SysEx Commands

This directory contains all SysEx command implementations for the Deluge.

## Structure

Each command module handles a specific domain of functionality:

### Current Modules

- **transport_sysex** - Playback control (play, stop, record, tempo, swing)
- **settings_sysex** - Flash storage settings (MIDI, UI, defaults, etc.)

### Planned Modules

- **param_sysex** - Parameter control (all 200+ synth/FX parameters)
- **clip_sysex** - Clip management (create, delete, move, properties)
- **note_sysex** - Sequencer control (add, edit, delete notes)
- **automation_sysex** - Automation curve editing
- **sample_sysex** - Sample management and metadata

## Integration

All commands are dispatched through `storage/smsysex.cpp` which:
1. Receives raw SysEx data
2. Parses JSON payload
3. Routes to appropriate command module
4. Returns JSON response

## Command Format

All commands follow this SysEx format:
```
F0 00 21 7B 01 04 <MSGID> <JSON_PAYLOAD> F7
```

Where:
- `F0` = SysEx start
- `00 21 7B` = Synthstrom manufacturer ID
- `01` = Deluge device ID
- `04` = JSON command type
- `<MSGID>` = Message ID for request/response matching
- `<JSON_PAYLOAD>` = UTF-8 encoded JSON command
- `F7` = SysEx end

## Adding New Commands

To add a new command:

1. Create `your_command_sysex.h` and `your_command_sysex.cpp`
2. Add namespace `YourCommandSysex` with command functions
3. Add dispatcher entry in `storage/smsysex.cpp`:
   ```cpp
   else if (!strcmp(tagName, "yourCommand")) {
       YourCommandSysex::yourCommand(de.cable, parser);
       goto done;
   }
   ```
4. Update this README
5. Document in main SysEx API docs

## Testing

Use the included test UI (`sysex-test-ui.html`) to test all commands.

## Documentation

See `/SYSEX_API.md` for complete API reference.

