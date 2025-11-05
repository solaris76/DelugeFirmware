# Improved MIDI Drum UI - Final Behavior

## ✅ Problem Solved: Clean Separation of Functions

### Old Behavior (Confusing):
```
HOLDING audition pad:
  Gold Knob 0 → Change MIDI channel
  Gold Knob 1 → Change MIDI note
  Gold Knobs also sending MIDI CC (conflict!)
```

### New Behavior (Clean):
```
HOLDING audition pad:
  OLED Popup → Shows "CH: 1 N#: 36 VEL: 64" (display only)
  Gold Knobs → Send MIDI CC (if assigned) ✅
  Gold Knobs do NOT change channel/note anymore ✅

MENU (Press SELECT button):
  MIDI Settings → Note, Channel, Velocity
  - Cleaner UI
  - No conflicts
  - Can set precise values
```

---

## Gold Knob Behavior Summary

### Always (Regardless of Pad State):
- **Gold knobs control MIDI CC automation**
- If CC assigned to knob → turning knob sends MIDI CC
- If no CC assigned → knob does nothing

### Never:
- ❌ Gold knobs no longer adjust note
- ❌ Gold knobs no longer adjust channel
- ❌ No mode switching confusion

---

## User Workflow

### 1. **Set Note, Channel, Velocity via Menu**
```
1. Select MIDI drum kit row
2. Press SELECT button (enter menu)
3. Navigate to "MIDI" (horizontal menu)
   → Shows 3 sub-items: NOTE | CHANNEL | VELOCITY
4. Turn SELECT encoder left/right to switch between items
5. Turn ◀▶ encoders to adjust values
6. Done!
```

### 2. **Audition Drum (Hold Pad)**
```
1. Hold audition pad for kit row
2. OLED popup shows: "CH: 1 N#: 36 VEL: 64" (info only)
3. Turn gold knobs → Sends MIDI CC to device (if assigned)
4. Release pad → popup disappears
```

### 3. **Assign MIDI CCs to Gold Knobs**
```
1. Select kit row (don't hold pad)
2. Press gold knob encoder button
3. Turn SELECT encoder → Change CC number
4. Release → CC assigned
5. Now turning that gold knob (anytime) sends that MIDI CC
```

---

## Benefits of New Approach

### 1. **No Conflicts**
- Gold knobs dedicated to CC automation
- Note/channel/velocity in menu (separate, clean)

### 2. **Consistent with MIDI Instruments**
- MIDI instrument tracks: Channel is in menu
- MIDI drum kit rows: Channel is in menu (same!)

### 3. **Better for Automation**
- Gold knobs always send MIDI CC (even when auditioning)
- No accidental note/channel changes during performance
- Cleaner workflow

### 4. **Velocity Control Added**
- MIDI drums now have default velocity setting
- Was missing before!
- Shows in audition popup

---

## Menu Structure

### MIDI Drum Kit Row Menu:
```
SOUND MENU:
  ├─ MIDI (horizontal menu) ← NEW!
  │   ├─ Note (0-127)
  │   ├─ Channel (1-16)
  │   └─ Velocity (1-127)
  ├─ Output Device (DIN/USB)
  ├─ Device Definition (load Digitakt.XML, etc.)
  ├─ Arpeggiator
  └─ Randomizer
```

---

## Comparison: Old vs New

| Action | Old Behavior | New Behavior |
|--------|-------------|--------------|
| **Hold pad + gold knob 0** | Changes MIDI channel | Sends MIDI CC (if assigned) |
| **Hold pad + gold knob 1** | Changes MIDI note | Sends MIDI CC (if assigned) |
| **Hold pad + gold knobs 2-4** | Nothing | Sends MIDI CC (if assigned) |
| **Change channel** | Hold pad + turn knob 0 | Menu → MIDI → Channel |
| **Change note** | Hold pad + turn knob 1 | Menu → MIDI → Note |
| **Change velocity** | ❌ Not possible | Menu → MIDI → Velocity |
| **Audition popup** | Shows CH + Note | Shows CH + Note + Velocity |

---

## Technical Implementation

### What Was Changed:

**1. MIDIDrum::modEncoderAction()**
```cpp
// OLD: Called base class, which changed channel with knob 0
NonAudioDrum::modEncoderAction(modelStack, offset, whichModEncoder);
if (auditioning && whichModEncoder == 1) {
    modChange(modelStack, offset, &noteEncoderCurrentOffset, &note, 128);
}

// NEW: Does nothing - gold knobs dedicated to CC automation
// (CC automation handled by View layer automatically)
return -64;
```

**2. Added Menu Items:**
- `midi::sound::Note` - Edit note (0-127)
- `midi::sound::Velocity` - Edit velocity (1-127)
- `midi::sound::OutputMidiChannel` - Edit channel (already existed)

**3. HorizontalMenu Created:**
```cpp
HorizontalMenu midiDrumSettingsMenu{
    STRING_FOR_MIDI,
    {&midiDrumNoteMenu, &outputMidiChannelMenu, &midiDrumVelocityMenu}
};
```

**4. Audition Popup Updated:**
```cpp
// Now shows velocity too
drumName.append("CH: ");
drumName.appendInt(midiDrum->channel + 1);
drumName.append(" N#: ");
drumName.appendInt(midiDrum->note);
drumName.append(" VEL: ");  // NEW!
drumName.appendInt(midiDrum->defaultVelocity);
```

---

## Summary

### Gold Knobs Now:
- ✅ **Always control MIDI CC** (if assigned)
- ✅ **Never change note/channel** (moved to menu)
- ✅ **Work consistently** (no mode switching)
- ✅ **Display-only popup** when auditioning

### Menu Now Has:
- ✅ **Note setting** (horizontal menu item 1)
- ✅ **Channel setting** (horizontal menu item 2)
- ✅ **Velocity setting** (horizontal menu item 3) - NEW!

**Much cleaner UX!** 🎯

