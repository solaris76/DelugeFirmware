# Parameter System & ModelStack Documentation

## Table of Contents

1. [Overview](#overview)
2. [ModelStack System](#modelstack-system)
3. [Parameter Change System](#parameter-change-system)
4. [How to Hook Into Parameter Changes](#how-to-hook-into-parameter-changes)
5. [Real-Time Notifications](#real-time-notifications)
6. [Examples](#examples)

---

## Overview

This document explains two interconnected systems in the Deluge firmware:

1. **ModelStack** - A context carrier system that passes object references (Song, Clip, NoteRow, etc.) between functions
2. **Parameter Change System** - How parameters are stored, modified, and notified in real-time

**Key Insight:** ModelStack provides **context** (which objects are involved), while AutoParam/ParamCollection handle the **actual parameter data and changes**.

---

## ModelStack System

### What is ModelStack?

ModelStack is a system introduced in 2020 to help functions keep track of the "things" (objects) they're dealing with during execution. Instead of passing many individual objects as function arguments, ModelStack provides a "stack" of relevant model objects that can be passed between functions.

### Important: What ModelStack Does NOT Do

**ModelStack does NOT handle parameter changes or real-time updates.** ModelStack is purely a **context carrier** - it's like a container that holds references to objects so functions know what they're working with.

### ModelStack Type Hierarchy

```
ModelStack (base)
  └─ ModelStackWithTimelineCounter
      └─ ModelStackWithNoteRowId
          └─ ModelStackWithNoteRow
              └─ ModelStackWithModControllable
                  └─ ModelStackWithThreeMainThings
                      ├─ ModelStackWithParamCollection
                      │   └─ ModelStackWithParamId
                      │       └─ ModelStackWithAutoParam
                      └─ ModelStackWithSoundFlags
```

### Quick Reference: ModelStack Types

| Type | Contains | Use Case |
|------|----------|----------|
| `ModelStack` | Song | Base level |
| `ModelStackWithTimelineCounter` | Song + Clip | Working with clips |
| `ModelStackWithNoteRow` | Song + Clip + NoteRow | Working with note rows |
| `ModelStackWithThreeMainThings` | Song + Clip + NoteRow + ModControllable + ParamManager | Working with parameters |
| `ModelStackWithAutoParam` | All above + ParamCollection + ParamId + AutoParam | Working with a specific parameter |

### Basic Usage

```cpp
// Allocate ModelStack on stack
char modelStackMemory[MODEL_STACK_MAX_SIZE];

// Create ModelStack with Song
ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);

// Add Clip
ModelStackWithTimelineCounter* modelStackWithClip =
    modelStack->addTimelineCounter(clip);

// Add NoteRow and get parameter context
ModelStackWithThreeMainThings* modelStackWithParams =
    modelStackWithClip->addNoteRow(noteRowId, noteRow)
    ->addOtherTwoThings(modControllable, paramManager);
```

**For detailed ModelStack documentation, see the original comments in `src/deluge/model/model_stack.h` (lines 44-121).**

---

## Parameter Change System

### Architecture Overview

The parameter system has four main layers:

```
User Input / Automation
    ↓
AutoParam (stores value + automation)
    ↓
ParamCollection (manages collection of AutoParams)
    ↓
ParamManager (manages all ParamCollections)
    ↓
Notification System (notifies subscribers)
```

### Core Components

#### 1. AutoParam

**Location:** `src/deluge/modulation/automation/auto_param.h`

**Purpose:** Stores the actual parameter value and automation data (nodes).

**Key Methods:**
- `setCurrentValueInResponseToUserInput()` - Called when user changes a parameter
- `setValuePossiblyForRegion()` - Sets value, possibly recording automation
- `setValueForRegion()` - Sets value for a specific time region
- `getCurrentValue()` - Gets the current parameter value
- `processCurrentPos()` - Processes automation during playback

**Important:** AutoParam stores:
- Current value (`currentValue`)
- Automation nodes (`nodes`)
- Interpolation state (`valueIncrementPerHalfTick`)

#### 2. ParamCollection

**Location:** `src/deluge/modulation/params/param_collection.h`

**Purpose:** Manages a collection of AutoParams. Different types:
- `PatchedParamSet` - Patched parameters (e.g., filter cutoff, resonance)
- `UnpatchedParamSet` - Unpatched parameters (e.g., reverb, sidechain)
- `MIDIParamCollection` - MIDI parameters
- `PatchCableSet` - Patch cable parameters
- `ExpressionParamSet` - Expression parameters (MPE)

**Key Method:**
- `notifyParamModifiedInSomeWay()` - **This is where parameter change notifications are triggered**

#### 3. ParamManager

**Location:** `src/deluge/modulation/params/param_manager.h`

**Purpose:** Manages all ParamCollections for a given context (e.g., a Sound, SoundDrum, or Clip).

**Key Method:**
- `notifyParamModifiedInSomeWay()` - Notifies the view and handles automation events

#### 4. ModelStack

**Purpose:** Provides context (which Song, Clip, NoteRow, ParamManager, etc.) to parameter operations.

---

## Parameter Change Flow

### Complete Flow Diagram

```
User Action (knob turn, menu change, etc.)
    ↓
[Entry Point] - Various entry points (see below)
    ↓
AutoParam::setCurrentValueInResponseToUserInput()
    OR
AutoParam::setValuePossiblyForRegion()
    OR
AutoParam::setValueForRegion()
    ↓
AutoParam updates currentValue and automation nodes
    ↓
ParamCollection::notifyParamModifiedInSomeWay()
    ↓
├─→ ParamManager::notifyParamModifiedInSomeWay()
│   ├─→ View::notifyParamAutomationOccurred()
│   └─→ Automation event handling
│
└─→ SysexParamStream::handleParamChange() [IF subscribers exist]
    ├─→ KitSysex::notifyDrumParameterChanged() [IF kit drum]
    └─→ SynthSysex::notifyParameterChanged() [IF synth/global]
        └─→ Send SysEx message to subscribers
```

### Entry Points: How Parameters Can Change

Parameters can be changed through multiple entry points:

#### 1. Mod Encoder (Hardware Knob)

**Location:** `src/deluge/gui/views/view.cpp::modEncoderAction()`

**Flow:**
```cpp
View::modEncoderAction(whichModEncoder, offset)
  → getModelStackWithParam(whichModEncoder)
  → paramCollection->knobPosToParamValue(newKnobPos)
  → autoParam->setValuePossiblyForRegion(newValue, modelStack, modPos, modLength)
```

**When:** User turns one of the mod encoders (gold knobs) assigned to a parameter.

#### 2. Select Encoder (Menu Navigation)

**Location:** `src/deluge/gui/menu_item/patched_param/integer.cpp::writeCurrentValue()`

**Flow:**
```cpp
MenuItem::selectEncoderAction(offset)
  → MenuItem::writeCurrentValue()
  → UnpatchedParam/PatchedParam::writeCurrentValue()
  → autoParam->setCurrentValueInResponseToUserInput(value, modelStack)
```

**When:** User navigates to a parameter in the sound editor and turns the select encoder.

#### 3. Menu Item Value Change

**Location:** `src/deluge/gui/menu_item/value.h::selectEncoderAction()`

**Flow:**
```cpp
Value::selectEncoderAction(offset)
  → writeCurrentValue() [virtual]
  → [For non-parameter properties] getNonParamPropertyName()
  → SynthSysex::notifyNonParamPropertyChanged()
```

**When:** User changes a non-parameter property (e.g., polyphonic mode, OSC type) via menu.

#### 4. Automation Playback

**Location:** `src/deluge/modulation/automation/auto_param.cpp::processCurrentPos()`

**Flow:**
```cpp
ParamCollection::processCurrentPos()
  → autoParam->processCurrentPos(modelStack)
  → autoParam updates currentValue from automation nodes
  → ParamCollection::notifyParamModifiedInSomeWay()
```

**When:** During playback, automation nodes are processed and parameter values change.

#### 5. SysEx API

**Location:** `src/deluge/io/midi/sysex/synth_sysex.cpp::setParameter()`

**Flow:**
```cpp
SynthSysex::setParameter()
  → getParamStack() [builds ModelStackWithAutoParam]
  → autoParam->setCurrentValueInResponseToUserInput(value, modelStack)
```

**When:** External application sends SysEx command to set a parameter.

#### 6. MIDI CC / MIDI Follow

**Location:** `src/deluge/io/midi/midi_follow.cpp`

**Flow:**
```cpp
MIDIFollow::processCC()
  → getModelStackWithParam()
  → autoParam->setCurrentValueInResponseToUserInput(value, modelStack)
```

**When:** MIDI CC message received for a learned parameter.

#### 7. Automation Recording

**Location:** `src/deluge/modulation/automation/auto_param.cpp::setCurrentValueInResponseToUserInput()`

**Flow:**
```cpp
[During recording]
  → autoParam->setCurrentValueInResponseToUserInput()
  → Creates/updates automation nodes at current play position
  → ParamCollection::notifyParamModifiedInSomeWay()
```

**When:** User changes parameter while recording automation.

---

## How to Hook Into Parameter Changes

### Method 1: Hook at ParamCollection Level (Recommended)

**Location:** `src/deluge/modulation/params/param_collection.cpp::notifyParamModifiedInSomeWay()`

**Why:** This is the **central notification point** - all parameter changes go through here, regardless of entry point.

**Current Implementation:**
```cpp
void ParamCollection::notifyParamModifiedInSomeWay(
    ModelStackWithAutoParam const* modelStack,
    int32_t oldValue,
    bool automationChanged,
    bool automatedBefore,
    bool automatedNow) {

    int32_t currentValue = modelStack->autoParam->getCurrentValue();
    bool currentValueChanged = (oldValue != currentValue);

    // Notify ParamManager
    if (currentValueChanged || automationChanged) {
        modelStack->paramManager->notifyParamModifiedInSomeWay(
            modelStack, currentValueChanged, automationChanged, automatedNow);
    }

    // SysEx notifications (current implementation)
    if (SynthSysex::hasParameterSubscribers() ||
        KitSysex::hasDrumParameterSubscribers()) {
        SysexParamStream::handleParamChange(modelStack);
    }

    // YOUR HOOK HERE - Add your notification system
    // Example:
    // if (YourSystem::hasSubscribers()) {
    //     YourSystem::notifyParameterChanged(modelStack, currentValue);
    // }
}
```

**Advantages:**
- Catches **all** parameter changes (mod encoder, select encoder, automation, SysEx, MIDI CC)
- Single point of integration
- Has access to full ModelStack context
- Can distinguish between value changes and automation changes

**Example Hook:**
```cpp
// In param_collection.cpp
#include "your_system/your_notifier.h"

void ParamCollection::notifyParamModifiedInSomeWay(...) {
    // ... existing code ...

    // Your custom notification
    if (YourNotifier::hasSubscribers()) {
        int32_t currentValue = modelStack->autoParam->getCurrentValue();
        Kind paramKind = summary->paramCollection->getParamKind();
        YourNotifier::notifyParameterChanged(
            modelStack, paramKind, modelStack->paramId, currentValue);
    }
}
```

### Method 2: Hook at ParamManager Level

**Location:** `src/deluge/modulation/params/param_manager.cpp::notifyParamModifiedInSomeWay()`

**When to Use:** If you need to hook at a higher level (before ParamCollection-specific logic).

**Current Implementation:**
```cpp
void ParamManager::notifyParamModifiedInSomeWay(
    ModelStackWithAutoParam const* modelStack,
    int32_t currentValueChanged,
    bool automationChanged,
    bool paramAutomatedNow) {

    if (automationChanged && paramAutomatedNow) {
        toForTimeline()->expectEvent(modelStack);
    }

    if (currentValueChanged) {
        view.notifyParamAutomationOccurred(this);
    }

    // YOUR HOOK HERE
}
```

**Note:** This is called **after** ParamCollection::notifyParamModifiedInSomeWay(), so SysEx notifications have already been sent.

### Method 3: Hook at AutoParam Level

**Location:** `src/deluge/modulation/automation/auto_param.cpp::setCurrentValueInResponseToUserInput()`

**When to Use:** If you need very low-level control or want to intercept before automation recording.

**Warning:** This is called from many places and may be called multiple times for a single user action. Not recommended unless you have specific needs.

### Method 4: Hook Non-Parameter Properties

**Location:** `src/deluge/gui/menu_item/value.h::selectEncoderAction()`

**When to Use:** For properties that are NOT AutoParams but still need notifications (e.g., polyphonic mode, OSC type, synth mode).

**Current Implementation:**
```cpp
template <typename T>
void Value<T>::selectEncoderAction(int32_t offset) {
    writeCurrentValue();

    // Notify SysEx subscribers of non-parameter property changes
    if (SynthSysex::hasParameterSubscribers() &&
        getCurrentUI() == &soundEditor) {
        int32_t value;
        const char* propName = this->getNonParamPropertyName(&value);
        if (propName) {
            SynthSysex::notifyNonParamPropertyChanged(propName, value);
        }
    }
}
```

**To Add Your Own:**
1. Implement `getNonParamPropertyName()` in your MenuItem subclass
2. Add your notification call in `Value::selectEncoderAction()`

---

## Real-Time Notifications

### Current SysEx Implementation

The firmware currently implements real-time parameter notifications via SysEx. Here's how it works:

#### 1. Subscription System

**Location:** `src/deluge/io/midi/sysex/synth_sysex.h`

```cpp
namespace SynthSysex {
    // Subscriber list
    extern SysexCommon::SubscriberList parameterSubscribers;

    // Check if there are subscribers
    bool hasParameterSubscribers();

    // Notify subscribers
    void notifyParameterChanged(int32_t paramKind, int32_t paramId,
                                const char* paramName, int32_t value);

    // Notify non-parameter properties
    void notifyNonParamPropertyChanged(const char* paramName, int32_t value);
}
```

#### 2. Notification Flow

```
Parameter Change
    ↓
ParamCollection::notifyParamModifiedInSomeWay()
    ↓
SysexParamStream::handleParamChange()
    ↓
├─→ KitSysex::notifyDrumParameterChanged() [IF kit drum]
│   └─→ Send SysEx: {"^drumParameterChanged": {"index": X, "name": "...", "value": Y}}
│
└─→ SynthSysex::notifyParameterChanged() [IF synth/global]
    └─→ Send SysEx: {"^parameterChanged": {"kind": X, "id": Y, "name": "...", "value": Z}}
```

#### 3. Message Format

**Parameter Change:**
```json
{
  "^parameterChanged": {
    "kind": 1,           // 0=non-param, 1=patched, 2=unpatched
    "id": 30,            // Parameter ID
    "name": "cutoff",    // Parameter name
    "value": 671088640   // Current value (raw 32-bit int)
  }
}
```

**Drum Parameter Change:**
```json
{
  "^drumParameterChanged": {
    "index": 0,          // Drum index in kit
    "name": "cutoff",    // Parameter name
    "value": 671088640   // Current value
  }
}
```

### Building Your Own Notification System

#### Step 1: Create Notification Functions

```cpp
// In your_notifier.h
namespace YourNotifier {
    extern SysexCommon::SubscriberList subscribers;

    bool hasSubscribers();
    void notifyParameterChanged(ModelStackWithAutoParam const* modelStack,
                                Kind paramKind, int32_t paramId, int32_t value);
}
```

#### Step 2: Implement Notification Logic

```cpp
// In your_notifier.cpp
namespace YourNotifier {
    SysexCommon::SubscriberList subscribers;
    static JsonSerializer notifyWriter;

    bool hasSubscribers() {
        return subscribers.size() > 0;
    }

    void notifyParameterChanged(ModelStackWithAutoParam const* modelStack,
                                Kind paramKind, int32_t paramId, int32_t value) {
        if (!hasSubscribers() || !modelStack || !modelStack->autoParam) {
            return;
        }

        // Get parameter name
        const char* paramName = getParamName(paramKind, paramId);
        if (!paramName) {
            return;
        }

        // Send to all subscribers
        subscribers.forEach([&](MIDICable& destination) {
            notifyWriter.reset();
            notifyWriter.setMemoryBased();
            smSysex::startDirect(notifyWriter);
            notifyWriter.writeOpeningTag("^yourParameterChanged", false, true);
            notifyWriter.writeAttribute("kind", (int32_t)paramKind);
            notifyWriter.writeAttribute("id", paramId);
            notifyWriter.writeAttribute("name", paramName);
            notifyWriter.writeAttribute("value", value);
            notifyWriter.closeTag(true);
            smSysex::sendMsg(destination, notifyWriter);
        });
    }
}
```

#### Step 3: Hook Into ParamCollection

```cpp
// In param_collection.cpp
#include "your_system/your_notifier.h"

void ParamCollection::notifyParamModifiedInSomeWay(...) {
    // ... existing code ...

    // Your notification hook
    if (YourNotifier::hasSubscribers()) {
        int32_t currentValue = modelStack->autoParam->getCurrentValue();
        Kind paramKind = summary->paramCollection->getParamKind();
        YourNotifier::notifyParameterChanged(
            modelStack, paramKind, modelStack->paramId, currentValue);
    }
}
```

#### Step 4: Add Subscription Commands

```cpp
// In your_sysex.cpp (or wherever you handle SysEx commands)
void handleSubscribeParameters(MIDICable& source) {
    YourNotifier::subscribers.add(source);
}

void handleUnsubscribeParameters(MIDICable& source) {
    YourNotifier::subscribers.remove(source);
}
```

### Best Practices

1. **Check for Subscribers First**: Always check `hasSubscribers()` before doing expensive work
2. **Use ModelStack Context**: Extract all needed information from ModelStack (Song, Clip, NoteRow, etc.)
3. **Handle NULL Cases**: Always check if ModelStack and AutoParam are valid
4. **Get Current Value**: Always call `autoParam->getCurrentValue()` to get the latest value
5. **Respect Parameter Kinds**: Different parameter kinds (Patched, Unpatched, MIDI) may need different handling
6. **Consider Performance**: Notifications happen in real-time - keep them fast
7. **Thread Safety**: Be aware that parameter changes can happen from multiple contexts (UI, audio thread, MIDI)

---

## Examples

### Example 1: Logging All Parameter Changes

```cpp
// In param_collection.cpp
#include "io/debug/log.h"

void ParamCollection::notifyParamModifiedInSomeWay(...) {
    // ... existing code ...

    // Log parameter changes
    if (modelStack && modelStack->autoParam) {
        int32_t currentValue = modelStack->autoParam->getCurrentValue();
        Kind paramKind = summary->paramCollection->getParamKind();
        D_PRINTLN("Param changed: kind=%d, id=%d, value=%d",
                  (int)paramKind, modelStack->paramId, currentValue);
    }
}
```

### Example 2: Sending Parameter Changes via MIDI CC

```cpp
// In your_notifier.cpp
void YourNotifier::notifyParameterChanged(...) {
    if (!hasSubscribers()) return;

    // Convert parameter to MIDI CC
    int32_t ccValue = paramValueToMIDICC(value);
    int32_t ccNumber = paramIdToMIDICC(paramId);

    // Send MIDI CC to all subscribers
    subscribers.forEach([&](MIDICable& destination) {
        destination.sendCC(ccNumber, ccValue);
    });
}
```

### Example 3: Filtering Specific Parameters

```cpp
// In param_collection.cpp
void ParamCollection::notifyParamModifiedInSomeWay(...) {
    // ... existing code ...

    // Only notify for specific parameters
    Kind paramKind = summary->paramCollection->getParamKind();
    if (paramKind == Kind::PATCHED) {
        int32_t paramId = modelStack->paramId;

        // Only notify for filter parameters
        if (paramId == Param::Local::LPF_FREQ ||
            paramId == Param::Local::LPF_RESONANCE) {
            YourNotifier::notifyParameterChanged(...);
        }
    }
}
```

### Example 4: Tracking Parameter Changes Per Clip

```cpp
// In your_notifier.cpp
struct ClipParamChange {
    Clip* clip;
    int32_t paramId;
    int32_t value;
    uint32_t timestamp;
};

deluge::vector<ClipParamChange> paramChangeHistory;

void YourNotifier::notifyParameterChanged(...) {
    if (!hasSubscribers()) return;

    // Track change history
    Clip* clip = modelStack->getTimelineCounterAllowNull()
        ? (Clip*)modelStack->getTimelineCounter()
        : nullptr;

    if (clip) {
        ClipParamChange change;
        change.clip = clip;
        change.paramId = paramId;
        change.value = value;
        change.timestamp = AudioEngine::audioSampleTimer;
        paramChangeHistory.push(change);
    }

    // ... send notification ...
}
```

### Example 5: Rate-Limited Notifications

```cpp
// In your_notifier.cpp
uint32_t lastNotificationTime = 0;
const uint32_t NOTIFICATION_INTERVAL_MS = 50; // 20 updates per second max

void YourNotifier::notifyParameterChanged(...) {
    if (!hasSubscribers()) return;

    uint32_t currentTime = AudioEngine::audioSampleTimer / 44; // Convert to ms

    // Rate limit notifications
    if (currentTime - lastNotificationTime < NOTIFICATION_INTERVAL_MS) {
        return; // Skip this notification
    }

    lastNotificationTime = currentTime;

    // ... send notification ...
}
```

---

## Parameter Value Formats

### Raw Values

Parameters use 32-bit signed integers (`int32_t`):
- Range: `-2147483648` to `2147483647` (0x80000000 to 0x7FFFFFFF)
- Neutral/Middle: Usually `0` or `2147483647` (0x7FFFFFFF)
- Maximum: `2147483647` (0x7FFFFFFF) for most parameters

### Knob Position Conversion

**To Knob Position (0-128):**
```cpp
int32_t knobPos = paramCollection->paramValueToKnobPos(paramValue, modelStack);
```

**From Knob Position:**
```cpp
int32_t paramValue = paramCollection->knobPosToParamValue(knobPos, modelStack);
```

### Parameter Kinds

```cpp
enum class Kind {
    NONE = 0,
    PATCHED = 1,           // Patched parameters (filter, envelope, etc.)
    UNPATCHED_SOUND = 2,   // Unpatched sound parameters (reverb, sidechain)
    MIDI = 3,              // MIDI parameters
    PATCH_CABLE = 4,       // Patch cable parameters
    EXPRESSION = 5         // Expression parameters (MPE)
};
```

### Getting Parameter Names

```cpp
// In param_stream.cpp
const char* getParamName(Kind paramKind, int32_t paramId) {
    // Returns parameter name string (e.g., "cutoff", "resonance")
    // Returns nullptr if not found
}
```

---

## Troubleshooting

### Parameter Changes Not Being Notified

1. **Check Entry Point**: Ensure the parameter change goes through `AutoParam::setCurrentValueInResponseToUserInput()` or similar
2. **Check Notification Hook**: Verify `ParamCollection::notifyParamModifiedInSomeWay()` is being called
3. **Check Subscribers**: Ensure `hasSubscribers()` returns true
4. **Check ModelStack**: Verify ModelStack is valid and contains required objects

### Notifications Too Frequent

1. **Rate Limiting**: Implement rate limiting (see Example 5)
2. **Filter Parameters**: Only notify for specific parameters (see Example 3)
3. **Debouncing**: Add debouncing logic for rapid changes

### Missing Non-Parameter Properties

1. **Implement `getNonParamPropertyName()`**: Override in MenuItem subclass
2. **Hook in `Value::selectEncoderAction()`**: Add notification call
3. **Check UI Context**: Ensure `getCurrentUI() == &soundEditor`

### Performance Issues

1. **Check Subscriber Count**: Large subscriber lists can slow down notifications
2. **Optimize Message Building**: Reuse JsonSerializer instances
3. **Avoid Heavy Processing**: Keep notification logic fast and simple

---

## Related Files

### Core Parameter System
- `src/deluge/modulation/automation/auto_param.h` - AutoParam class
- `src/deluge/modulation/automation/auto_param.cpp` - AutoParam implementation
- `src/deluge/modulation/params/param_collection.h` - ParamCollection base class
- `src/deluge/modulation/params/param_collection.cpp` - ParamCollection implementation
- `src/deluge/modulation/params/param_manager.h` - ParamManager class
- `src/deluge/modulation/params/param_manager.cpp` - ParamManager implementation

### ModelStack
- `src/deluge/model/model_stack.h` - ModelStack definitions
- `src/deluge/model/model_stack.cpp` - ModelStack implementation

### Notification System
- `src/deluge/io/midi/sysex/param_stream.h` - Parameter stream handler
- `src/deluge/io/midi/sysex/param_stream.cpp` - Parameter stream implementation
- `src/deluge/io/midi/sysex/synth_sysex.h` - Synth SysEx notifications
- `src/deluge/io/midi/sysex/synth_sysex.cpp` - Synth SysEx implementation
- `src/deluge/io/midi/sysex/kit_sysex.h` - Kit SysEx notifications
- `src/deluge/io/midi/sysex/kit_sysex.cpp` - Kit SysEx implementation

### Entry Points
- `src/deluge/gui/views/view.cpp` - Mod encoder handling
- `src/deluge/gui/menu_item/patched_param/integer.cpp` - Patched parameter menu items
- `src/deluge/gui/menu_item/unpatched_param.cpp` - Unpatched parameter menu items
- `src/deluge/gui/menu_item/value.h` - Value menu item base class

---

## Summary

**ModelStack** = Context carrier (which objects are involved)
**AutoParam** = Parameter value storage and automation
**ParamCollection** = Collection management and notification hub
**ParamManager** = High-level parameter management
**Notification System** = Real-time change notifications

**To hook into parameter changes:**
1. Add your notification call in `ParamCollection::notifyParamModifiedInSomeWay()`
2. Check for subscribers before processing
3. Extract information from ModelStack
4. Send notifications to your subscribers

**Key Insight:** All parameter changes flow through `ParamCollection::notifyParamModifiedInSomeWay()`, making it the ideal hook point for real-time notifications.
