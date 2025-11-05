# Kit Row MIDI CC Automation - Testing Guide

## ✅ Build Status: SUCCESS

Firmware is ready for hardware testing!

---

## Quick Start Testing Sequence

### 1. **Create Test Kit**
```
1. Create NEW KIT
2. Add MIDI drum on Row 1:
   - Press pad + Turn note knob → Note 36
   - Press pad + Turn channel knob → Channel 0
3. Set output device:
   - SOUND MENU → OUTPUT DEVICE → Select USB device (e.g., "Digitakt")
4. Add second MIDI drum on Row 2:
   - Note 38, Channel 1
   - Different output device (e.g., "TR-8S")
```

### 2. **Test CC Assignment (CRITICAL)**
```
1. Select Row 1 (don't hold pad - just tap)
2. Press GOLD KNOB 1 encoder button
   → Display shows: "CC -" or "CC 0" (current assignment)
3. Turn SELECT encoder
   → Display cycles: CC 1, CC 2, CC 3... CC 74...
4. Select CC 74
5. Release encoder button
   → CC 74 is now assigned to gold knob 1
6. Verify LED 1 is ON (shows CC is assigned)
```

### 3. **Test Multiple CC Assignments**
```
1. Assign CCs to all 4 gold knobs:
   - Knob 1 → CC 74
   - Knob 2 → CC 71
   - Knob 3 → CC 70
   - Knob 4 → CC 10
2. Verify all 4 LEDs are ON
3. Press MOD BUTTON (lower mode)
4. Verify LEDs turn OFF (no CCs in lower mode yet)
5. Assign CCs in lower mode:
   - Knob 1 → CC 7
   - Knob 2 → CC 11
6. Press MOD BUTTON (upper mode)
7. Verify original 4 LEDs come back ON
```

### 4. **Test Row Switching**
```
1. While Row 1 is selected, note which LEDs are ON
2. Press Row 2 pad
3. Verify LEDs change (Row 2 has different assignments)
4. Press back to Row 1
5. Verify LEDs return to original state
   → Each row remembers its own CC assignments!
```

### 5. **Test Automation Recording**
```
1. Enter AUTOMATION VIEW
2. Turn AFFECT ENTIRE OFF
3. Select Row 1 pad
4. Turn GOLD KNOB 1 (assigned to CC 74)
   → Should send MIDI CC to Digitakt device
   → Check external device receives CC 74
5. Record some automation moves
6. Exit automation view
7. Playback → verify automation plays back
```

### 6. **Test Device Selection Per Row**
```
1. Row 1 → Set to USB device (Digitakt)
2. Row 2 → Set to different USB device (TR-8S)
3. Row 3 → Set to DIN MIDI
4. In automation view:
   - Turn gold knobs on Row 1 → MIDI goes to Digitakt only
   - Turn gold knobs on Row 2 → MIDI goes to TR-8S only
   - Turn gold knobs on Row 3 → MIDI goes to DIN only
```

### 7. **Test Save/Load**
```
1. Save kit preset with CC assignments
2. Clear kit
3. Load kit preset
4. Verify each row has correct:
   - CC assignments
   - Device selection
   - Mod knob mode
5. Save song with automation
6. Load song
7. Verify automation plays back correctly
```

---

## Expected Behavior Checklist

### Context Switching (CRITICAL):
- [ ] Holding audition pad + turning gold knobs → changes NOTE/CHANNEL (existing behavior)
- [ ] NOT holding pad + pressing encoder → enters CC assignment mode (new behavior)
- [ ] NO CONFLICT between note/channel adjustment and CC assignment

### CC Assignment:
- [ ] Press encoder button → display shows current CC
- [ ] Turn SELECT encoder → CC number changes
- [ ] Release button → assignment saved
- [ ] LED lights up to show CC is assigned

### Mod Button Modes:
- [ ] Press MOD BUTTON UPPER → access CC slots 0-3
- [ ] Press MOD BUTTON LOWER → access CC slots 4-7
- [ ] Total of 8 independent CC assignments per drum
- [ ] MOD button LED shows which mode is active

### Row Switching:
- [ ] Press different kit row → LEDs update immediately
- [ ] Each row has independent CC assignments
- [ ] Each row remembers its own mod knob mode
- [ ] Switching back restores previous assignments

### MIDI CC Sending:
- [ ] Turning gold knob sends MIDI CC in real-time
- [ ] Correct CC number is sent
- [ ] Correct MIDI channel is used (from drum settings)
- [ ] Correct device receives MIDI (respects device selection)

### Automation:
- [ ] CC automation records to timeline
- [ ] Each row has independent automation
- [ ] Automation playback sends MIDI CC correctly
- [ ] Automation respects device selection

### Custom CC Labels:
- [ ] Create device definition file with CC names
- [ ] Load definition for MIDI drum
- [ ] Display shows custom names instead of numbers
- [ ] Save/load preserves custom labels

---

## Testing with External MIDI Monitor

### Recommended Setup:
1. Connect USB MIDI device (e.g., Digitakt)
2. Use MIDI monitor software (e.g., MIDI Monitor on Mac, MIDI-OX on Windows)
3. Verify Deluge sends correct MIDI messages

### What to Check:
```
Action: Turn gold knob 1 (assigned to CC 74)
Expected MIDI Output:
  - Channel: 0 (from drum settings)
  - CC Number: 74
  - Value: 0-127 (based on knob position)
  - Device: Only to selected device (not ALL devices)
```

---

## Known Issues to Watch For

### If CCs Don't Send:
- Check device selection is set correctly
- Verify MIDI channel is correct
- Check CC is actually assigned (LED should be ON)
- Try DIN MIDI first (simpler routing)

### If LEDs Don't Update:
- Verify `setKnobIndicatorLevels()` is being called
- Check `getModKnobMode()` returns correct mode
- Ensure `activeModControllableModelStack` points to correct drum

### If Row Switching Doesn't Work:
- Check `InstrumentClip::getActiveModControllable()` supports MIDI drums
- Verify `setActiveModControllableTimelineCounter()` is called on row change
- Check Affect Entire is OFF

### If Automation Doesn't Record:
- Verify ParamManager has MIDIParamCollection
- Check `getParamToControlFromInputMIDIChannel()` returns valid param
- Ensure automation view is in correct mode

---

## Advanced Testing

### Multi-Device Setup:
```
Kit Configuration:
  Row 1: Kick → Elektron Digitakt (USB, Channel 0, Note 36)
  Row 2: Snare → Roland TR-8S (USB, Channel 1, Note 38)
  Row 3: HiHat → Moog DFAM (DIN, Channel 0, Note 42)
  Row 4: Tom → Native Instruments Maschine (USB, Channel 0, Note 45)

Test:
  - Automate CC 74 on Row 1 → only Digitakt receives
  - Automate CC 1 on Row 2 → only TR-8S receives
  - Automate CC 16 on Row 3 → only DIN MIDI receives
  - Automate CC 20 on Row 4 → only Maschine receives
```

### Device Reconnection Test:
```
1. Create kit with USB device assignments
2. Save kit
3. Disconnect USB devices
4. Reconnect in DIFFERENT order
5. Load kit
6. Verify device name matching finds correct devices
```

---

## Performance Testing

### CPU Usage:
- Monitor CPU usage during automation playback
- Should be similar to MIDI instrument automation
- Check for audio dropouts or glitches

### Memory Usage:
- Check RAM usage with complex kits (many MIDI drums)
- Each MIDI drum adds ~50 bytes
- Compare to SOUND drums (~1000 bytes each)

---

## Success Criteria

### Minimum Viable:
- ✅ Can assign CCs to gold knobs
- ✅ Gold knobs send MIDI CC
- ✅ Automation records and plays back
- ✅ Device selection works per row
- ✅ Row switching updates CC assignments

### Full Feature Complete:
- ✅ All above +
- ✅ 8 CC assignments per drum (upper/lower modes)
- ✅ Custom CC label names
- ✅ Save/load works perfectly
- ✅ Device name matching after reconnection
- ✅ No conflicts with note/channel adjustment

---

## What to Report

### If It Works:
- ✅ Mark Phase 9 complete
- Document any UX improvements needed
- Consider Phase 10 (clip view mode)

### If Issues Found:
- Note which step fails
- Check MIDI monitor output
- Report error codes if any appear
- Provide reproduction steps

---

## Flash Instructions

### Copy firmware to SD card:
```
1. Copy build/Release/deluge.elf to SD card root
2. Rename to: FIRMWARE.UPG
3. Insert SD card into Deluge
4. Power on while holding SHIFT
5. Wait for firmware update
6. Test!
```

---

Happy testing! 🚀

**Report any issues and I'll help debug immediately.**

