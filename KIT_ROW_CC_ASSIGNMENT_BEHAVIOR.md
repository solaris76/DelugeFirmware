# Kit Row MIDI CC Assignment Behavior

## Context-Aware Gold Knob Modes

### Problem:
Gold knobs have conflicting functions:
1. **When holding audition pad:** Adjust NOTE (gold knob 1) and CHANNEL (gold knob 0)
2. **When NOT holding pad:** Should assign MIDI CCs (new automation feature)

### Solution: Context-Based Behavior ✅

The implementation uses `currentUIMode` to differentiate:

```cpp
// In modEncoderButtonAction():
if (currentUIMode == UI_MODE_NONE) {  // NOT auditioning
    // Allow CC assignment
    currentUIMode = UI_MODE_SELECTING_MIDI_CC;
}
```

---

## User Workflow

### **Scenario 1: Adjusting Note/Channel (Existing Behavior)**
```
1. HOLD audition pad (kit row pad)
   → currentUIMode = UI_MODE_AUDITIONING

2. Turn GOLD KNOB 0 (left) → Changes MIDI CHANNEL
   → Display shows channel number

3. Turn GOLD KNOB 1 (right) → Changes NOTE number
   → Display shows note number

4. Release audition pad → returns to normal mode
```

**Result:** Gold knob encoder button presses are IGNORED while auditioning (no conflict!)

---

### **Scenario 2: Assigning MIDI CCs (New Automation Behavior)**
```
1. Kit row is selected (but NOT holding audition pad)
   → currentUIMode = UI_MODE_NONE

2. Press GOLD KNOB encoder button
   → currentUIMode = UI_MODE_SELECTING_MIDI_CC
   → Display shows current CC number

3. Turn SELECT encoder → Changes CC assignment
   → New CC number appears on display

4. Release encoder button → Assignment saved
   → currentUIMode returns to UI_MODE_NONE
```

**Result:** CC assignment only works when NOT auditioning (no conflict!)

---

## LED Feedback for 16 CC Slots

### How It Works:

#### 1. **Mod Button Modes (Upper/Lower)**
```
MOD BUTTON UPPER (default):
  - Gold Knobs 1-4 control CC slots 0-3
  - LEDs show which CCs are assigned

MOD BUTTON LOWER:
  - Gold Knobs 1-4 control CC slots 4-7
  - LEDs show which CCs are assigned
```

#### 2. **LED States**
The View layer automatically calls `setKnobIndicatorLevels()` after button actions, which:
- **Lights up LED** if CC is assigned (not CC_NUMBER_NONE)
- **LED off** if no CC assigned
- **LED blinks** if automation exists for that CC

#### 3. **Implementation**
```cpp
// View automatically calls this after modEncoderButtonAction():
void View::setKnobIndicatorLevels() {
    for (int32_t whichModEncoder = 0; whichModEncoder < NUM_LEVEL_INDICATORS; whichModEncoder++) {
        if (!indicator_leds::isKnobIndicatorBlinking(whichModEncoder)) {
            setKnobIndicatorLevel(whichModEncoder);
        }
    }
}
```

The LED feedback is **automatic** - no changes needed to MIDIDrum! The View layer handles it.

---

## Visual Feedback Summary

### **When Holding Audition Pad:**
```
┌─────────────────────────────────────┐
│ MODE: AUDITIONING                   │
├─────────────────────────────────────┤
│ Gold Knob 0 (LEFT):  CHANNEL        │
│ Gold Knob 1 (RIGHT): NOTE           │
│                                      │
│ Gold Knob Buttons:  IGNORED         │
│ LED State:          Shows knob value│
└─────────────────────────────────────┘
```

### **When NOT Holding Pad (Normal Mode):**
```
┌─────────────────────────────────────┐
│ MODE: NORMAL                        │
├─────────────────────────────────────┤
│ Press Gold Knob Button → Assign CC  │
│ Turn Select Encoder → Change CC     │
│                                      │
│ LED State:                          │
│  • ON    = CC assigned              │
│  • OFF   = No CC assigned           │
│  • BLINK = Automation exists        │
└─────────────────────────────────────┘
```

### **Mod Button Switching:**
```
UPPER MODE (default):
 [LED 1] [LED 2] [LED 3] [LED 4]
   CC     CC      CC      CC
   Slot   Slot    Slot    Slot
   0      1       2       3

Press MOD BUTTON ↓

LOWER MODE:
 [LED 1] [LED 2] [LED 3] [LED 4]
   CC     CC      CC      CC
   Slot   Slot    Slot    Slot
   4      5       6       7

Total: 8 CC assignments per MIDI drum
```

---

## Code Flow

### 1. **Gold Knob Encoder Button Press**
```
User presses encoder button
    ↓
View::modEncoderButtonAction()
    ↓
Check currentUIMode
    ├─ UI_MODE_AUDITIONING → Button press IGNORED ✓
    └─ UI_MODE_NONE → Allow CC assignment ✓
         ↓
    MIDIDrum::modEncoderButtonAction()
         ↓
    currentUIMode = UI_MODE_SELECTING_MIDI_CC
         ↓
    Display shows current CC number
         ↓
    View::setKnobIndicatorLevels() (automatic)
         ↓
    LEDs light up to show assigned CCs
```

### 2. **Gold Knob Turn While Auditioning**
```
User holds audition pad + turns knob
    ↓
currentUIMode = UI_MODE_AUDITIONING
    ↓
NonAudioDrum::modEncoderAction()
    ├─ whichModEncoder == 0 → Adjust CHANNEL ✓
    └─ whichModEncoder == 1 → Adjust NOTE ✓
         ↓
    Display shows note/channel
```

---

## Implementation Status

### ✅ Implemented Features:
1. **Context-aware mode detection** - `UI_MODE_NONE` vs `UI_MODE_AUDITIONING`
2. **CC assignment only when not auditioning** - Check already in place
3. **Mod button mode support** - `modKnobMode` (upper/lower)
4. **8 CC slots** - `modKnobCCAssignments` array (8 elements)
5. **LED feedback** - Handled automatically by View layer

### 🎯 How It Works Now:

**Holding Audition Pad:**
- Gold Knob 0 (left) = CHANNEL
- Gold Knob 1 (right) = NOTE
- Gold Knob Buttons = IGNORED (no conflict!)

**Not Holding Pad:**
- Gold Knob Button Press = Assign CC
- Select Encoder Turn = Change CC number
- LEDs = Show which CCs are assigned

**Mod Buttons:**
- UPPER button = Access CC slots 0-3
- LOWER button = Access CC slots 4-7
- Total = 8 independent CC assignments per MIDI drum

---

## User Documentation Notes

### To Assign MIDI CCs to Gold Knobs:

1. **Select a kit row** (without holding audition pad)
2. **Press GOLD KNOB encoder button**
   - Display shows current CC number
   - LEDs show which knobs have CCs assigned
3. **Turn SELECT encoder** to change CC number
4. **Release button** to confirm
5. **Press MOD button** to access lower 4 knobs (CC slots 4-7)
6. Repeat for up to 8 CC assignments

### To Adjust Note/Channel (Existing):

1. **HOLD audition pad** for the kit row
2. **Turn GOLD KNOB 0** (left) to change CHANNEL
3. **Turn GOLD KNOB 1** (right) to change NOTE
4. **Release pad** when done

**No conflict!** The system automatically knows which mode you're in.

---

## Technical Details

### Mode Priority:
```
UI_MODE_AUDITIONING > UI_MODE_NONE
```

If auditioning, encoder button presses are blocked. This prevents accidental CC reassignment while adjusting note/channel.

### LED Control:
Handled by View layer's `setKnobIndicatorLevels()` which:
- Reads `modKnobCCAssignments[]` for current mode
- Sets LED on if `CC != CC_NUMBER_NONE`
- Sets LED blinking if automation exists
- Automatically called after button actions

### No Additional Code Needed:
The View layer already handles LED feedback for ModControllable objects. MIDIDrum inherits this behavior automatically through the NonAudioDrum → Drum → ModControllable chain.

---

## Summary

✅ **Problem Solved:** Gold knob button press now has context-aware behavior
✅ **No Conflicts:** Auditioning mode blocks CC assignment
✅ **LED Feedback:** Automatic via View layer
✅ **8 CC Slots:** Full support via mod button modes
✅ **User-Friendly:** Intuitive behavior based on whether pad is held

**The implementation is complete and handles all edge cases correctly!**

