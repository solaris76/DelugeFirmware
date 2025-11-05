# Kit Row MIDI CC Automation Implementation Prompt

## Goal
Implement comprehensive MIDI CC automation for Kit drum rows (MIDI, Synth, and Sample types), building on top of the existing MIDI device selection implementation.

## Context
- **Base Implementation**: MIDI Device Selection PR #4091 (already implemented and working)
- **Reference PR**: https://github.com/SynthstromAudible/DelugeFirmware/pull/4090/files
- **Current State**: We have MIDI device selection working for both MIDI instruments and kit drum rows
- **New Feature**: Add automation capabilities for kit rows similar to what MIDI instruments have

---

## Requirements

### 1. **Kit Row MIDI CC Automation**

Enable MIDI CC automation for all kit drum types:
- **MIDI Drums** - Automate MIDI CC messages sent from MIDI drum rows
- **Synth Drums** - Automate synth parameters on a per-row basis
- **Sample Drums** - Automate sample parameters on a per-row basis

**Key Behaviors:**
- When "Affect Entire" is OFF → Automation applies to individual kit rows
- When "Affect Entire" is ON → Automation applies to entire kit (existing behavior)
- Automation view should work seamlessly for kit rows like it does for MIDI instruments
- Each kit row can have independent automation
- Automation should be saved per-row in song files

### 2. **Gold Knob MIDI CC Control (CRITICAL)**

**Problem to Solve:**
Currently, gold knobs in automation view for MIDI instruments send MIDI CC values in real-time. This should also work for MIDI drum kit rows.

**Required Behavior:**
```
In Automation View (Automation Instrument Clip):
- MIDI Instrument: Gold knobs send MIDI CC ✅ (already works)
- MIDI Drum (Kit Row): Gold knobs should ALSO send MIDI CC ❌ (needs implementation)

In Clip View (with kit row selected):
- Gold knobs should send MIDI CC for the selected MIDI drum row
- User should be able to "play" MIDI CC values with gold knobs while viewing clip
```

**Implementation Notes:**
- When editing a MIDI drum kit row in automation view, turning gold knobs should:
  1. Record automation for that CC
  2. Send MIDI CC messages in real-time (like MIDI instruments do)
  3. Use the correct MIDI channel for that drum row
  4. Respect the device selection (send to selected device only)

### 3. **Gold Knob Function Swap in Clip View**

**New Feature:**
When in clip view with a kit row selected (especially MIDI drums), enable gold knobs to send MIDI CC directly.

**User Workflow:**
```
1. User selects a MIDI drum row in a kit
2. User presses SHIFT + gold knob (or similar) to assign CC
3. Gold knobs now send MIDI CC for that selected drum row
4. User can "perform" with gold knobs while arranging notes
5. Optionally can record this as automation
```

**Benefits:**
- Quick MIDI CC tweaking while composing
- No need to switch to automation view for simple CC changes
- More intuitive workflow for multi-device setups

---

## Technical Implementation Guide

### Current Architecture (Already Implemented)

**MIDI Device Selection:**
- `MIDIDrum::outputDevice` - Device index (0=ALL, 1=DIN, 2+=USB)
- `MIDIDrum::outputDeviceName` - Device name for matching
- `MIDIInstrument::outputDevice` - Device index
- `MIDIInstrument::outputDeviceName` - Device name
- Helper functions in `src/deluge/io/midi/midi_device_helper.h`

**MIDI Engine:**
- `sendNote()` accepts device index parameter
- `sendCC()` needs to accept device index parameter (TODO)
- `sendMidi()` accepts device index parameter

### What Needs to Be Implemented

#### 1. **Data Model for Kit Row Automation**

**Existing (from reference PR #4090):**
```cpp
// In Kit class or NoteRow
ParamManager paramManager; // For per-row automation

// Need to ensure MIDI drums have access to automation
```

**Key Question:** Where should MIDI CC automation data live for kit rows?
- Option A: In the `NoteRow` (alongside the drum)
- Option B: In the `Drum` itself (each drum has its own param manager)
- Option C: In the `Kit` with per-row indexing

**Recommendation:** Follow the pattern used in PR #4090 - likely per-NoteRow ParamManager.

#### 2. **Automation View Integration**

**Files to Modify:**
- `src/deluge/gui/views/automation_view.cpp`
- `src/deluge/gui/views/automation_view.h`

**Required Changes:**
```cpp
// When in automation view with kit row selected:
class AutomationView {
    // Detect if we're editing a kit row vs entire kit
    bool editingKitRow();

    // Get the correct ParamManager for kit row automation
    ParamManager* getParamManagerForKitRow();

    // Handle mod encoder action for kit rows (send MIDI CC)
    void modEncoderActionForKitRow(int offset, int whichModEncoder);
};
```

**Critical: Gold Knob MIDI CC Sending**

When turning gold knobs in automation view for MIDI drum kit row:
```cpp
// Similar to MIDI instrument behavior
void AutomationView::modEncoderAction(int offset, int whichModEncoder) {
    // If editing MIDI drum kit row:
    if (isEditingMIDIDrumKitRow()) {
        MIDIDrum* midiDrum = getCurrentMIDIDrum();
        int32_t cc = getCurrentCC();
        int32_t value = getCurrentCCValue();

        // Send MIDI CC in real-time (THIS IS CRITICAL!)
        midiEngine.sendCC(midiDrum, midiDrum->channel, cc, value,
                          kMIDIOutputFilterNoMPE, midiDrum->outputDevice);
    }
}
```

#### 3. **Gold Knob CC Assignment for MIDI Drums**

**Similar to MIDI Instrument:**
```cpp
// In MIDIInstrument.h/cpp:
std::array<int8_t, kNumModButtons * kNumPhysicalModKnobs> modKnobCCAssignments;

// Need equivalent for MIDIDrum:
class MIDIDrum {
    std::array<int8_t, kNumModButtons * kNumPhysicalModKnobs> modKnobCCAssignments;

    // Or shared at Kit level if all drums share same CC assignments
};
```

**UI for CC Assignment:**
```cpp
// When user presses mod encoder button on MIDI drum kit row:
// - Show current CC assignment
// - Allow user to change CC number
// - Store assignment per drum or per kit
```

#### 4. **Clip View Gold Knob Integration**

**New Feature Implementation:**

**Files to Modify:**
- `src/deluge/gui/views/instrument_clip_view.cpp`
- `src/deluge/gui/views/instrument_clip_view.h`

**Required Functionality:**
```cpp
class InstrumentClipView {
    // When kit row is selected and gold knob turned:
    void modEncoderAction(int whichModEncoder, int offset) override {
        if (isKitRowSelected() && kitRowIsMIDIDrum()) {
            MIDIDrum* midiDrum = getSelectedMIDIDrum();

            // Check if in MIDI CC mode for this knob
            if (midiDrumModKnobMode == MIDI_CC_MODE) {
                int32_t cc = midiDrum->modKnobCCAssignments[whichModEncoder];

                // Update value and send MIDI CC
                int32_t newValue = calculateNewCCValue(offset);
                midiEngine.sendCC(midiDrum, midiDrum->channel, cc, newValue,
                                  kMIDIOutputFilterNoMPE, midiDrum->outputDevice);

                // Optionally record automation if recording
                if (recording) {
                    recordAutomation(cc, newValue);
                }
            }
        }
    }
};
```

#### 5. **MIDI Engine Enhancement**

**Add device filtering to `sendCC()`:**

```cpp
// In midi_engine.h
void sendCC(MIDISource source, int32_t channel, int32_t cc, int32_t value,
            int32_t filter, uint8_t deviceFilter);

// In midi_engine.cpp
void MidiEngine::sendCC(MIDISource source, int32_t channel, int32_t cc,
                        int32_t value, int32_t filter, uint8_t deviceFilter) {
    MIDIMessage message = MIDIMessage::cc(channel, cc, value);
    sendMidi(source, message, filter, true, deviceFilter);
}
```

#### 6. **Serialization**

**Kit Row Automation in XML:**
```xml
<soundSources>
    <midiOutput channel="0" note="36" outputDevice="2" outputDeviceName="Elektron Digitakt">
        <arpeggiator .../>
        <!-- Per-row automation data -->
        <midiCC cc="74" value="0x4A3D70A4">
            <!-- Automation nodes -->
        </midiCC>
    </midiOutput>
</soundSources>
```

**Ensure:**
- Kit row automation is saved per-row (not shared across kit)
- MIDI CC values are saved with automation nodes
- Device selection works with automation

---

## Implementation Checklist

### Phase 1: Basic Kit Row Automation
- [ ] Enable automation view for kit rows (detect when editing kit row vs entire kit)
- [ ] Route automation to correct ParamManager (per-row, not per-kit when Affect Entire OFF)
- [ ] Ensure automation view displays correctly for kit rows
- [ ] Test with Synth drums, Sample drums, MIDI drums

### Phase 2: MIDI CC Automation for MIDI Drums
- [ ] Add MIDI CC parameter collection for MIDIDrum
- [ ] Implement `getParamToControlFromInputMIDIChannel()` for MIDIDrum
- [ ] Enable MIDI CC recording in automation view for MIDI drum rows
- [ ] Add serialization for per-row MIDI CC automation

### Phase 3: Gold Knob Real-time MIDI CC (CRITICAL)
- [ ] Detect when editing MIDI drum kit row in automation view
- [ ] Send MIDI CC when gold knobs are turned (like MIDI instruments do)
- [ ] Use correct MIDI channel from `MIDIDrum::channel`
- [ ] Respect device selection (`MIDIDrum::outputDevice`)
- [ ] Update display to show CC value changing

### Phase 4: Gold Knob CC Assignment
- [ ] Add mod knob CC assignments to MIDIDrum (or shared at Kit level)
- [ ] Implement UI for assigning CC to gold knobs (press encoder button)
- [ ] Save/load CC assignments with kit presets
- [ ] Display current CC assignment when encoder pressed

### Phase 5: Clip View Gold Knob Mode
- [ ] Add mode toggle for gold knobs in clip view
- [ ] When kit row selected + MIDI CC mode active:
  - [ ] Gold knobs send MIDI CC for selected drum row
  - [ ] Display shows CC values changing
  - [ ] Can record as automation if recording enabled
- [ ] Add UI indicator for MIDI CC mode

### Phase 6: Integration & Testing
- [ ] Ensure automation works with device selection
- [ ] Test multi-row automation (different CCs per row)
- [ ] Test gold knob MIDI CC sending in both automation and clip view
- [ ] Verify serialization (save/load)
- [ ] Test with different device selections per row

---

## Key Design Patterns to Follow

### 1. **Follow MIDI Instrument Pattern**

Look at how `MIDIInstrument` handles:
- Mod knob CC assignments
- Gold knob encoder actions
- MIDI CC sending in automation view
- Parameter automation

Apply similar patterns to `MIDIDrum` for kit rows.

### 2. **Use Existing Device Selection**

Don't reinvent device routing - use the already-implemented system:
```cpp
// When sending MIDI CC from kit row:
midiEngine.sendCC(midiDrum, midiDrum->channel, cc, value,
                  kMIDIOutputFilterNoMPE, midiDrum->outputDevice);
                  //                      ^^^^^^^^^^^^^^^^^^^
                  //                      Use existing device selection!
```

### 3. **Respect "Affect Entire" Toggle**

```cpp
if (affectEntire) {
    // Use kit-level automation (existing behavior)
} else {
    // Use per-row automation (new behavior)
}
```

### 4. **Centralize Helper Functions**

Create helpers in `midi_device_helper.h` or new file for:
- Getting ParamManager for kit row
- Detecting if editing kit row vs entire kit
- Common MIDI CC sending logic

---

## Critical Implementation Notes

### **Gold Knobs Must Send MIDI CC!**

This is the most important requirement. When user turns a gold knob in automation view while editing a MIDI drum kit row:

```cpp
// Current behavior for MIDI Instrument:
MIDIInstrument::modEncoderAction() {
    int32_t cc = modKnobCCAssignments[whichKnob];
    int32_t value = calculateValue();
    midiEngine.sendCC(this, channel, cc, value, filter, outputDevice); // ✅ Sends!
}

// Required behavior for MIDI Drum kit row:
MIDIDrum::modEncoderAction() {
    int32_t cc = modKnobCCAssignments[whichKnob];
    int32_t value = calculateValue();
    midiEngine.sendCC(this, channel, cc, value, filter, outputDevice); // ❌ NOT IMPLEMENTED
}
```

**This must be implemented** or the feature is incomplete!

### **Device Selection Integration**

All MIDI CC sends MUST respect the device selection:
```cpp
// WRONG:
midiEngine.sendCC(drum, channel, cc, value, filter); // Sends to ALL devices

// CORRECT:
midiEngine.sendCC(drum, channel, cc, value, filter, drum->outputDevice); // Respects selection
```

### **Clip View Gold Knob Mode**

**User Flow:**
1. In clip view, select a MIDI drum row (audition pad)
2. Press SHIFT + MOD_ENCODER (or similar) to enter MIDI CC mode
3. Turn gold knobs → sends MIDI CC for that drum row
4. Press again to exit mode

**Implementation:**
```cpp
// Add UI mode:
enum UIMode {
    // ... existing modes ...
    UI_MODE_KIT_ROW_MIDI_CC
};

// In InstrumentClipView:
void modEncoderButtonAction(int whichEncoder, bool on) {
    if (isKitRowSelected() && isMIDIDrum()) {
        if (on) {
            // Enter MIDI CC assignment mode
            currentUIMode = UI_MODE_SELECTING_MIDI_CC;
            displayCurrentCC(whichEncoder);
        }
    }
}

void modEncoderAction(int whichEncoder, int offset) {
    if (currentUIMode == UI_MODE_KIT_ROW_MIDI_CC) {
        MIDIDrum* drum = getSelectedMIDIDrum();
        sendCCForKitRowKnob(drum, whichEncoder, offset);
    }
}
```

---

## Files to Modify/Create

### Core Model Files
1. **`src/deluge/model/drum/midi_drum.h`**
   - Add `modKnobCCAssignments` array (like MIDIInstrument)
   - Add methods for mod encoder handling
   - Add MIDI CC parameter collection support

2. **`src/deluge/model/drum/midi_drum.cpp`**
   - Implement `modEncoderAction()` to send MIDI CC
   - Implement `modEncoderButtonAction()` for CC assignment
   - Add serialization for CC assignments
   - Add `getParamToControlFromInputMIDIChannel()` equivalent

3. **`src/deluge/model/drum/drum.h/cpp`** (maybe)
   - If MIDI CC should work for all drum types (not just MIDI drums)

### Automation View Files
4. **`src/deluge/gui/views/automation_view.h`**
   - Add methods for kit row automation detection
   - Add ParamManager retrieval for kit rows

5. **`src/deluge/gui/views/automation_view.cpp`**
   - Implement kit row automation support
   - Route mod encoder actions to correct drum
   - Ensure MIDI CC is sent when gold knobs turned
   - Handle kit row selection changes

### Clip View Files
6. **`src/deluge/gui/views/instrument_clip_view.h`**
   - Add MIDI CC mode state for kit rows
   - Add gold knob routing logic

7. **`src/deluge/gui/views/instrument_clip_view.cpp`**
   - Implement gold knob MIDI CC mode
   - Add UI for entering/exiting CC mode
   - Route encoder actions to MIDI CC sending

### MIDI Engine
8. **`src/deluge/io/midi/midi_engine.h`**
   - Ensure `sendCC()` has device filter parameter (may need overload)

9. **`src/deluge/io/midi/midi_engine.cpp`**
   - Implement device-filtered `sendCC()` if not already present

### Kit Management
10. **`src/deluge/model/instrument/kit.h/cpp`**
    - Ensure `getModelStackWithParamForKitRow()` works correctly
    - Support per-row automation when Affect Entire is OFF

---

## Specific Implementation Details

### A. **Add MIDI CC Support to MIDIDrum**

```cpp
// In midi_drum.h
class MIDIDrum : public NonAudioDrum {
public:
    // CC assignments (like MIDIInstrument)
    std::array<int8_t, kNumModButtons * kNumPhysicalModKnobs> modKnobCCAssignments;

    // Mod encoder handling
    bool modEncoderButtonAction(uint8_t whichModEncoder, bool on,
                                ModelStackWithThreeMainThings* modelStack);
    void modEncoderAction(int8_t offset, uint8_t whichModEncoder,
                          ModelStackWithThreeMainThings* modelStack);

    // Get param for CC automation
    ModelStackWithAutoParam* getParamToControlFromInputMIDIChannel(
        int32_t cc, ModelStackWithThreeMainThings* modelStack);

    // Read/write CC assignments to file
    Error readModKnobAssignmentsFromFile(Deserializer& reader);
    void writeModKnobAssignmentsToFile(Serializer& writer);
};
```

### B. **Detect Kit Row Context in Automation View**

```cpp
// In automation_view.cpp
bool AutomationView::isEditingKitRow() {
    if (getCurrentOutputType() != OutputType::KIT) {
        return false;
    }

    // Check if Affect Entire is OFF
    Kit* kit = getCurrentKit();
    InstrumentClip* clip = getCurrentInstrumentClip();
    return (kit && clip && !clip->affectEntire);
}

Drum* AutomationView::getCurrentDrumForAutomation() {
    if (!isEditingKitRow()) {
        return nullptr;
    }

    Kit* kit = getCurrentKit();
    return kit ? kit->selectedDrum : nullptr;
}
```

### C. **Send MIDI CC from Gold Knobs**

```cpp
// In automation_view.cpp (or wherever mod encoders are handled)
void AutomationView::modEncoderAction(int whichModEncoder, int offset) {
    // ... existing code ...

    // NEW: Handle MIDI drum kit row
    if (isEditingKitRow()) {
        Drum* drum = getCurrentDrumForAutomation();
        if (drum && drum->type == DrumType::MIDI) {
            MIDIDrum* midiDrum = static_cast<MIDIDrum*>(drum);

            // Get CC assignment for this knob
            int32_t cc = midiDrum->modKnobCCAssignments[currentModKnobMode * kNumPhysicalModKnobs + whichModEncoder];

            if (cc != CC_NUMBER_NONE) {
                // Get current value from automation
                ModelStackWithAutoParam* modelStack = getParamForCC(cc);
                if (modelStack && modelStack->autoParam) {
                    int32_t currentValue = modelStack->autoParam->getCurrentValue();
                    int32_t newValue = currentValue + (offset << 25); // Scale appropriately

                    // Update automation
                    modelStack->autoParam->setCurrentValue(newValue);

                    // CRITICAL: Send MIDI CC in real-time!
                    int32_t ccValue = newValue >> 25; // Convert to 7-bit
                    midiEngine.sendCC(midiDrum, midiDrum->channel, cc, ccValue,
                                      kMIDIOutputFilterNoMPE, midiDrum->outputDevice);
                }
            }
        }
    }
}
```

### D. **Clip View Gold Knob Mode**

```cpp
// Add UI mode for kit row MIDI CC
#define UI_MODE_KIT_ROW_MIDI_CC_MODE 0x__ // Find next available mode number

// In instrument_clip_view.cpp
ActionResult InstrumentClipView::modEncoderButtonAction(uint8_t whichModEncoder, bool on) {
    if (on && getCurrentOutputType() == OutputType::KIT) {
        Kit* kit = getCurrentKit();
        if (kit && kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
            // Enter MIDI CC assignment mode
            currentUIMode = UI_MODE_SELECTING_MIDI_CC;
            MIDIDrum* midiDrum = static_cast<MIDIDrum*>(kit->selectedDrum);

            int32_t cc = midiDrum->modKnobCCAssignments[whichModEncoder];
            displayMIDICCNumber(cc);
            return ActionResult::DEALT_WITH;
        }
    }
    // ... existing code ...
}
```

---

## Testing Requirements

### Functional Testing
- [ ] Create kit with MIDI drums
- [ ] Enter automation view with Affect Entire OFF
- [ ] Turn gold knobs → should send MIDI CC in real-time
- [ ] Verify MIDI CC is sent to correct device (respect outputDevice selection)
- [ ] Record automation → playback should work
- [ ] Switch between kit rows → automation should be independent per row

### Gold Knob Testing
- [ ] In automation view: Gold knobs send MIDI CC for MIDI drum rows
- [ ] In clip view: Gold knobs can be switched to MIDI CC mode
- [ ] CC assignment works (press encoder to change CC number)
- [ ] Values display correctly on screen
- [ ] MIDI is sent to correct channel and device

### Integration Testing
- [ ] Works with device selection (DIN, USB, ALL)
- [ ] Works with arpeggiator
- [ ] Saves/loads correctly with songs
- [ ] Multiple kit rows with different CCs work
- [ ] Affect Entire toggle works correctly

---

## Reference Code Locations

### Study These for Patterns

**MIDI Instrument Gold Knob Handling:**
- `src/deluge/model/instrument/midi_instrument.cpp:modEncoderButtonAction()`
- `src/deluge/model/instrument/midi_instrument.cpp:modEncoderAction()`
- `src/deluge/model/instrument/midi_instrument.cpp:getParamToControlFromInputMIDIChannel()`

**Kit Automation (Existing):**
- `src/deluge/model/instrument/kit.cpp:getModelStackWithParamForKitRow()`
- `src/deluge/gui/views/automation_view.cpp` (automation view logic)

**MIDI CC Sending:**
- `src/deluge/io/midi/midi_engine.cpp:sendCC()`
- Check if device filter parameter exists or needs to be added

---

## Common Pitfalls to Avoid

### 1. **Don't Forget Device Selection!**
Every MIDI CC send must use `drum->outputDevice`:
```cpp
// WRONG:
midiEngine.sendCC(drum, channel, cc, value, filter);

// RIGHT:
midiEngine.sendCC(drum, channel, cc, value, filter, drum->outputDevice);
```

### 2. **Gold Knobs Must Send MIDI in Real-Time**
Not just record automation - must actually send MIDI CC when turned!

### 3. **Use Correct Channel**
```cpp
// Use the drum's channel, not the kit's channel
midiDrum->channel // ✅
kit->channel      // ❌ (kits don't have a channel)
```

### 4. **ParamManager Context**
Make sure you're getting the right ParamManager:
```cpp
// For kit row (Affect Entire OFF):
noteRow->paramManager

// For entire kit (Affect Entire ON):
kit->paramManager
```

### 5. **Handle All Drum Types**
- MIDI drums → Send MIDI CC
- Synth drums → Automate synth parameters
- Sample drums → Automate sample parameters

Don't assume only MIDI drums need automation!

---

## Success Criteria

### Minimum Viable Implementation
1. ✅ Kit row automation works in automation view
2. ✅ Gold knobs send MIDI CC for MIDI drum rows
3. ✅ Automation is saved/loaded per row
4. ✅ Device selection is respected

### Full Feature Complete
1. ✅ All above +
2. ✅ Gold knob CC assignment UI
3. ✅ Clip view gold knob MIDI CC mode
4. ✅ Works for all drum types (MIDI, Synth, Sample)
5. ✅ Comprehensive testing passed

---

## Questions to Answer During Implementation

1. **Where to store CC assignments?**
   - Per MIDIDrum? Per Kit? Global?
   - Recommendation: Per MIDIDrum for flexibility

2. **Should all drum types support MIDI CC automation?**
   - Synth drums could send internal parameter automation
   - Sample drums could send parameter automation
   - Or limit to MIDI drums only?

3. **How to indicate MIDI CC mode in clip view?**
   - LED indicator?
   - Display message?
   - Mode toggle button combo?

4. **Should gold knob assignments be shared across a kit?**
   - Share: Simpler, consistent
   - Per-drum: More flexible
   - Recommendation: Start with shared, add per-drum if needed

---

## Expected Outcome

After implementation, users should be able to:

1. **In Automation View:**
   - Select a MIDI drum kit row
   - Turn gold knobs to send and record MIDI CC automation
   - See CC values update in real-time
   - Have automation play back correctly
   - Each kit row has independent automation

2. **In Clip View:**
   - Select a MIDI drum kit row
   - Switch gold knobs to MIDI CC mode
   - Turn knobs to send MIDI CC while composing
   - Optionally record this as automation

3. **Multi-Device Setup:**
   - Different kit rows can route to different MIDI devices
   - Each row's MIDI CC goes to its selected device only
   - Automation respects device selection

---

## File Comparison to Original PR #4090

**Key differences to maintain:**
- ✅ Keep simplified device index system (no bitmasks)
- ✅ Use existing `midi_device_helper.h` utilities
- ✅ Don't duplicate device logic - reuse what's already there
- ✅ Ensure all MIDI sends use device parameter

**What to adapt from PR #4090:**
- Kit row automation architecture
- ParamManager routing for kit rows
- Automation view integration patterns
- UI mode handling

---

## Summary

Implement kit row MIDI CC automation by:
1. Following MIDIInstrument patterns for MIDI CC handling
2. Using existing device selection infrastructure
3. Ensuring gold knobs send MIDI CC in real-time (CRITICAL!)
4. Adding clip view gold knob mode for direct CC control
5. Maintaining clean, centralized code architecture

**Focus Areas:**
- 🔴 **CRITICAL**: Gold knobs must send MIDI CC when turned
- 🟡 **Important**: Respect device selection in all MIDI sends
- 🟢 **Nice-to-have**: Clip view gold knob mode

Good luck with the implementation! 🚀

