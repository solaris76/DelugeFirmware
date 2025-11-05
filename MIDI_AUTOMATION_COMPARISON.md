# MIDI Automation Feature Comparison
## MIDIInstrument vs MIDIDrum

This document compares automation features between MIDIInstrument and MIDIDrum to ensure kit rows (MIDI drums) have the same functionality as MIDI instrument tracks.

---

## Core Automation Methods

| Method | MIDIInstrument | MIDIDrum | Status | Notes |
|--------|---------------|----------|--------|-------|
| `modEncoderAction()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Handles gold knob turning |
| `modEncoderButtonAction()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Handles gold knob button press for CC assignment |
| `modButtonAction()` | ✅ Yes | ❌ No | ⚠️ MISSING | Handles mod button (upper/lower) switching |
| `getParamFromModEncoder()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Gets parameter from gold knob |
| `getParamToControlFromInputMIDIChannel()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Gets MIDI CC parameter for automation |
| `doesAutomationExistOnMIDIParam()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Checks if automation exists on CC |

---

## CC Management Methods

| Method | MIDIInstrument | MIDIDrum | Status | Notes |
|--------|---------------|----------|--------|-------|
| `changeControlNumberForModKnob()` | ✅ Yes | ❌ No | ⚠️ MISSING | Changes CC number assignment for a gold knob |
| `getFirstUnusedCC()` | ✅ Yes | ❌ No | ⚠️ MISSING | Finds first unused CC number |
| `moveAutomationToDifferentCC()` (2 overloads) | ✅ Yes | ❌ No | ⚠️ MISSING | Moves automation from one CC to another |
| `getKnobPosForNonExistentParam()` | ✅ Yes | ❌ No | ⚠️ MISSING | Returns knob position for non-existent param |
| `modKnobCCAssignments` array | ✅ Yes | ✅ Yes | ✅ COMPLETE | Stores CC assignments |

---

## Device Definition Support

| Feature | MIDIInstrument | MIDIDrum | Status | Notes |
|---------|---------------|----------|--------|-------|
| `readDeviceDefinitionFile()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Reads device definition XML |
| `writeDeviceDefinitionFile()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Writes device definition XML |
| `readCCLabelsFromFile()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Reads CC label names |
| `writeCCLabelsToFile()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Writes CC label names |
| `getNameFromCC()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Gets custom name for CC |
| `setNameForCC()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Sets custom name for CC |
| `deviceDefinitionFileName` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Stores definition file name |
| `loadDeviceDefinitionFile` flag | ✅ Yes | ✅ Yes | ✅ COMPLETE | Flags if definition should be loaded |
| `labels` map | ✅ Yes | ✅ Yes | ✅ COMPLETE | Stores CC name mappings |

---

## Serialization

| Method | MIDIInstrument | MIDIDrum | Status | Notes |
|--------|---------------|----------|--------|-------|
| `readModKnobAssignmentsFromFile()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Reads CC assignments from XML |
| `writeModKnobAssignmentsToFile()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Writes CC assignments to XML |
| Device definition in write | ✅ Yes | ✅ Yes | ✅ COMPLETE | Saves CC labels with preset/song |
| Device definition in read | ✅ Yes | ✅ Yes | ✅ COMPLETE | Loads CC labels from preset/song |

---

## Mod Knob Mode Support

| Feature | MIDIInstrument | MIDIDrum | Status | Notes |
|---------|---------------|----------|--------|-------|
| `modKnobMode` variable | ✅ Yes (uint8_t) | ❌ No | ⚠️ MISSING | Tracks current mod button mode (upper/lower) |
| `getModKnobMode()` | ✅ Yes | ❌ No | ⚠️ MISSING | Returns pointer to modKnobMode |
| `modButtonAction()` | ✅ Yes | ❌ No | ⚠️ MISSING | Switches between upper/lower modes |

**Impact:** MIDIDrum currently hardcodes `modKnobMode = 0` in methods, should support both upper/lower modes like MIDIInstrument.

---

## Value Change Sensitivity

| Method | MIDIInstrument | MIDIDrum | Status | Notes |
|--------|---------------|----------|--------|-------|
| `valueChangedEnoughToMatter()` | ✅ Yes | ❌ No | ⚠️ MISSING | Determines if parameter change is significant enough to record |

**Impact:** Uses different thresholds for:
- Pitch bend: 14-bit (shift >> 18)
- Aftertouch/mod wheel: Positive only (shift >> 24)
- Regular CCs: 7-bit (shift >> 25)

Without this, MIDIDrum may record too many or too few automation points.

---

## Device Selection

| Feature | MIDIInstrument | MIDIDrum | Status | Notes |
|---------|---------------|----------|--------|-------|
| `outputDevice` index | ✅ Yes | ✅ Yes | ✅ COMPLETE | 0=ALL, 1=DIN, 2+=USB |
| `outputDeviceName` | ✅ Yes | ✅ Yes | ✅ COMPLETE | For device name matching |
| Device-filtered `sendCC()` | ✅ Yes | ✅ Yes | ✅ COMPLETE | Sends to specific device |

---

## Missing Methods Analysis

### 1. **`modButtonAction()`** - Priority: MEDIUM
**What it does:**
Switches between upper/lower mod button modes and handles UI cleanup when leaving CC selection mode.

**Why needed:**
Users press MOD buttons to switch between 8 different CC slots (4 upper + 4 lower). Without this, MIDI drums are stuck in upper mode only.

**MIDIInstrument implementation:**
```cpp
void MIDIInstrument::modButtonAction(uint8_t whichModButton, bool on, ParamManagerForTimeline* paramManager) {
    if (currentUIMode == UI_MODE_SELECTING_MIDI_CC) {
        currentUIMode = UI_MODE_NONE;
        // Clean up display
    }
}
```

**Required for MIDIDrum:** Yes, for full feature parity.

---

### 2. **`changeControlNumberForModKnob()`** - Priority: HIGH
**What it does:**
Changes the CC number assigned to a gold knob when user turns the encoder during CC assignment mode.

**Why needed:**
When user presses a gold knob button, they need to be able to **turn the SELECT encoder** to change which CC is assigned. Currently MIDIDrum shows the CC but can't change it!

**MIDIInstrument implementation:**
```cpp
int32_t MIDIInstrument::changeControlNumberForModKnob(int32_t offset, int32_t whichModEncoder, int32_t modKnobMode) {
    int8_t* cc = &modKnobCCAssignments[modKnobMode * kNumPhysicalModKnobs + whichModEncoder];

    int32_t newCC = *cc + offset;
    // Handle wraparound
    if (newCC < 0) newCC += kNumCCNumbersIncludingFake;
    else if (newCC >= kNumCCNumbersIncludingFake) newCC -= kNumCCNumbersIncludingFake;

    *cc = newCC;
    editedByUser = true;
    return newCC;
}
```

**Required for MIDIDrum:** **YES - CRITICAL!** Without this, users can't assign CCs!

---

### 3. **`moveAutomationToDifferentCC()`** - Priority: MEDIUM
**What it does:**
When changing CC assignment, moves existing automation data to the new CC number.

**Why needed:**
Preserves user's automation when they reassign a gold knob to a different CC. Without this, automation is lost.

**Two overloads:**
1. `moveAutomationToDifferentCC(oldCC, newCC, modelStack)` - Moves specific CC
2. `moveAutomationToDifferentCC(offset, whichEncoder, modKnobMode, modelStack)` - Moves with encoder offset

**Required for MIDIDrum:** Medium priority - nice to have but not critical for basic functionality.

---

### 4. **`getFirstUnusedCC()`** - Priority: LOW
**What it does:**
Finds the first CC number that doesn't have automation, used when moving automation to avoid conflicts.

**Why needed:**
Helper method for `moveAutomationToDifferentCC()`.

**Required for MIDIDrum:** Only if implementing `moveAutomationToDifferentCC()`.

---

### 5. **`getKnobPosForNonExistentParam()`** - Priority: LOW
**What it does:**
Returns the knob position for parameters that don't exist yet (shows 0 for MIDI CCs, default for others).

**Why needed:**
UI feedback - shows where the knob is even if no automation exists.

**MIDIInstrument implementation:**
```cpp
int32_t MIDIInstrument::getKnobPosForNonExistentParam(int32_t whichModEncoder, ModelStackWithAutoParam* modelStack) {
    if (modelStack->autoParam && (modelStack->paramId < kNumRealCCNumbers || modelStack->paramId == CC_NUMBER_PITCH_BEND)) {
        return 0; // MIDI CCs start at 0
    }
    return ModControllable::getKnobPosForNonExistentParam(whichModEncoder, modelStack);
}
```

**Required for MIDIDrum:** Low priority - minor UI polish.

---

### 6. **`valueChangedEnoughToMatter()`** - Priority: MEDIUM
**What it does:**
Determines if a parameter value change is significant enough to record as automation.

**Why needed:**
Prevents recording too many automation points. Different parameters have different resolutions:
- Pitch bend: 14-bit (compare at >> 18)
- Aftertouch/mod wheel: Positive only, 8-bit range (compare at >> 24)
- Regular CCs: 7-bit (compare at >> 25)

**Required for MIDIDrum:** Medium priority - affects automation quality/size.

---

### 7. **Mod Knob Mode Support** - Priority: HIGH
**What's needed:**
- Add `uint8_t modKnobMode{0}` member variable
- Add `uint8_t* getModKnobMode()` method
- Update all methods to use actual `modKnobMode` instead of hardcoded `0`

**Why needed:**
Users have 8 gold knobs via MOD buttons (upper/lower). Currently MIDI drums only support upper 4.

**Required for MIDIDrum:** **YES - IMPORTANT!** Users expect 8 CC assignments per drum.

---

## Priority Implementation Order

### ⭐ **CRITICAL** (Must Have)
1. ✅ **Device definition support** - Already complete!
2. 🔴 **`changeControlNumberForModKnob()`** - Can't assign CCs without this!
3. 🔴 **Mod knob mode support** - Only have 4 CCs instead of 8!

### ⚠️ **HIGH** (Should Have)
4. `modButtonAction()` - Needed for mode switching UI
5. `valueChangedEnoughToMatter()` - Affects automation quality

### 📋 **MEDIUM** (Nice to Have)
6. `moveAutomationToDifferentCC()` - Preserves automation when changing CC
7. `getFirstUnusedCC()` - Helper for above

### 🔧 **LOW** (Polish)
8. `getKnobPosForNonExistentParam()` - Minor UI improvement

---

## Recommended Action Plan

### Step 1: Add Critical Missing Methods (Required for v1.0)

**A. Add mod knob mode support:**
```cpp
// In midi_drum.h:
public:
    uint8_t modKnobMode{0};  // 0 = upper, 1 = lower
    uint8_t* getModKnobMode() { return &modKnobMode; }
```

**B. Add `changeControlNumberForModKnob()`:**
```cpp
int32_t changeControlNumberForModKnob(int32_t offset, int32_t whichModEncoder, int32_t modKnobMode);
```

**C. Update existing methods to use `this->modKnobMode` instead of hardcoded `0`:**
- `modEncoderButtonAction()`
- `getParamFromModEncoder()`

---

### Step 2: Add Important Methods (Recommended for v1.0)

**D. Add `modButtonAction()`:**
```cpp
void modButtonAction(uint8_t whichModButton, bool on, ParamManagerForTimeline* paramManager);
```

**E. Add `valueChangedEnoughToMatter()`:**
```cpp
bool valueChangedEnoughToMatter(int32_t old_value, int32_t new_value,
                                deluge::modulation::params::Kind kind, uint32_t paramID);
```

---

### Step 3: Optional Enhancements (Future)

**F. Add automation movement methods:**
```cpp
Error moveAutomationToDifferentCC(int32_t oldCC, int32_t newCC, ModelStackWithThreeMainThings* modelStack);
int32_t moveAutomationToDifferentCC(int32_t offset, int32_t whichModEncoder, int32_t modKnobMode,
                                    ModelStackWithThreeMainThings* modelStack);
int32_t getFirstUnusedCC(ModelStackWithThreeMainThings* modelStack, int32_t direction,
                         int32_t startAt, int32_t stopAt);
```

---

## Current Status Summary

### ✅ What We Have (Complete)
- Basic MIDI CC automation
- Gold knob CC assignments (4 upper mode only)
- Device selection & routing
- Device definition & CC label support
- Serialization (save/load)
- Parameter access methods
- Real-time MIDI CC sending

### 🔴 What's Missing (Critical)
- **Can't change CC assignments via encoder** - users are stuck with default assignments!
- **Only 4 CC slots instead of 8** - mod button mode not implemented
- Mod button switching

### ⚠️ What's Missing (Important)
- Value change threshold (may record too many automation points)
- UI polish methods

### 📊 Feature Completion: **75%**
- Core automation: ✅ 100%
- CC management: ❌ 40%
- Mod button support: ❌ 33%
- Device definitions: ✅ 100%
- Serialization: ✅ 100%

---

## Conclusion

MIDIDrum has **most** of the automation features from MIDIInstrument, but is **missing critical UI/workflow methods** that make it actually usable:

1. **Users cannot change CC assignments** (`changeControlNumberForModKnob` missing)
2. **Users only get 4 gold knobs instead of 8** (mod knob mode not implemented)
3. **Automation quality may suffer** (`valueChangedEnoughToMatter` missing)

**Recommendation:** Implement Step 1 (critical methods) before releasing. This will bring feature parity to ~95%.

