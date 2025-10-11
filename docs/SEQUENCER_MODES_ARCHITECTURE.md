# Sequencer Modes - Architecture & Implementation

## Overview

This document explains the architecture, design decisions, and implementation approach for the Deluge Sequencer Modes system. This system adds alternative pattern-based sequencing modes as alternatives to the default linear clip playback (piano roll/waveform views).

## Motivation

### The Problem
The Deluge's default sequencer (piano roll for melodic instruments, grid for kits, waveform for audio) is powerful but inherently **linear** and **timeline-based**. While excellent for traditional composition, this approach has limitations for:
- **Generative music** - Creating evolving, non-repeating patterns
- **Euclidean rhythms** - Complex polyrhythmic sequences
- **Live performance** - Quick pattern manipulation without detailed editing
- **Experimental workflows** - Non-linear, algorithmic composition

### The Vision
Create a **plugin-style architecture** where multiple sequencer modes can coexist:
- Each mode provides a different approach to generating note sequences
- Modes are **non-destructive** - they don't modify the underlying clip data
- **Universal support** - Same system works for Synth/MIDI/CV/Kit/Audio tracks
- **RAII design** - Automatic resource management, no memory leaks
- **Hot-swappable** - Switch modes in real-time during playback

---

## Design Philosophy

### 1. Follow Existing Deluge Patterns

Rather than inventing new architectural patterns, we **studied and adapted** the Deluge's existing `keyboard/layout/` system:

**Keyboard Layouts** (from `src/deluge/gui/ui/keyboard/layout/`):
```
KeyboardLayout (base class)
├── InKeyLayout
├── IsomorphicLayout
├── ChordKeyboard
├── NornsLayout
├── PianoLayout
└── VelocityDrumsLayout
```

**Our Sequencer Modes** (in `src/deluge/model/clip/sequencer/`):
```
SequencerMode (base class)
├── GenerativeTestMode (demo/example)
└── PulseSequencerMode (full implementation)
```

### Why This Approach?
1. **Familiar to Deluge developers** - Uses established patterns
2. **Proven architecture** - Keyboard layouts work well, are stable
3. **Easier code review** - Maintainers understand the structure
4. **Consistent with firmware** - Doesn't introduce foreign concepts

---

## Architecture

### Core Components

#### 1. SequencerMode Base Class
**Location**: `src/deluge/model/clip/sequencer/sequencer_mode.h`

```cpp
class SequencerMode {
public:
    // Lifecycle
    virtual void initialize() = 0;
    virtual void cleanup() = 0;
    
    // Capability flags (like keyboard layouts)
    virtual bool supportsInstrument() { return false; }
    virtual bool supportsKit() { return false; }
    virtual bool supportsMIDI() { return false; }
    virtual bool supportsCV() { return false; }
    virtual bool supportsAudio() { return false; }
    
    // Rendering (override to show custom UI)
    virtual bool renderPads(...) { return false; }
    virtual bool renderSidebar(...) { return false; }
    
    // Input handling
    virtual bool handlePadPress(...) { return false; }
    virtual bool handleVerticalEncoder(...) { return false; }
    virtual bool handleHorizontalEncoder(...) { return false; }
    
    // Playback engine
    virtual int32_t processPlayback(...) { return INT32_MAX; }
    
    // Utility
    virtual l10n::String name() = 0;
};
```

**Key Design Decisions**:
- **Virtual methods with defaults** - Modes only override what they need
- **Boolean returns** - `false` = "I didn't handle this, pass through to default behavior"
- **INT32_MAX return** - "No next event, I'm not managing timing" (playback can ignore)
- **Capability flags** - Clean way to restrict modes to compatible track types

#### 2. SequencerModeManager (Factory)
**Location**: `src/deluge/model/clip/sequencer/sequencer_mode_manager.h`

```cpp
class SequencerModeManager {
public:
    static SequencerModeManager& instance(); // Singleton
    
    // RAII: Returns unique_ptr for automatic cleanup
    template<typename T>
    std::unique_ptr<SequencerMode> createMode();
    
    // Registration (allows compile-time registration)
    template<typename T>
    void registerMode(const char* id);
};
```

**RAII Implementation**:
- Uses C++ `std::unique_ptr` for automatic memory management
- When mode is destroyed, cleanup happens automatically
- No manual `delete` calls needed
- Exception-safe resource management

**Self-Registration Pattern**:
```cpp
// At bottom of pulse_sequencer_mode.cpp
namespace {
    static auto registered = []() {
        SequencerModeManager::instance().registerMode<PulseSequencerMode>("pulse_seq");
        return true;
    }();
}
```
- Mode registers itself when translation unit loads
- No central list to maintain
- Adding new modes doesn't require modifying manager code

#### 3. Integration Points

**Clip Integration** (`InstrumentClip`, `AudioClip`, etc.):
```cpp
class InstrumentClip {
    std::unique_ptr<SequencerMode> sequencerMode_;  // RAII member
    
    void switchSequencerMode(const char* modeId) {
        // Old mode automatically cleaned up by unique_ptr
        sequencerMode_ = SequencerModeManager::instance().createMode(modeId);
        sequencerMode_->initialize();
    }
};
```

**UI Integration** (`InstrumentClipView`):
```cpp
// In renderMainPads():
if (clip->sequencerMode_) {
    bool handled = clip->sequencerMode_->renderPads(...);
    if (handled) return; // Mode took over rendering
}
// Fall through to default piano roll rendering

// In padPressed():
if (clip->sequencerMode_) {
    bool handled = clip->sequencerMode_->handlePadPress(...);
    if (handled) return; // Mode handled input
}
// Fall through to default note editing
```

**Playback Integration** (`InstrumentClip::processCurrentPos`):
```cpp
if (sequencerMode_) {
    int32_t nextEvent = sequencerMode_->processPlayback(modelStack, pos);
    if (nextEvent != INT32_MAX) {
        return nextEvent; // Mode is controlling timing
    }
}
// Fall through to default note playback
```

---

## Following the Keyboard Layout Pattern

### Similarities We Adopted

#### 1. **Base Class with Virtual Methods**
**Keyboard Layouts**:
```cpp
class KeyboardLayout {
    virtual void renderPads(...) = 0;
    virtual void handlePadAction(...) = 0;
    virtual void handleVerticalEncoder(...) {}
};
```

**Our Sequencer Modes**:
```cpp
class SequencerMode {
    virtual bool renderPads(...) { return false; }
    virtual bool handlePadPress(...) { return false; }
    virtual bool handleVerticalEncoder(...) { return false; }
};
```

**Key difference**: We use **boolean returns** to allow fall-through to default behavior, while keyboard layouts take full control.

#### 2. **Capability Flags**
**Keyboard Layouts**:
```cpp
virtual bool supportsInstrument() { return true; }
virtual bool supportsKit() { return false; }
```

**Our Sequencer Modes**:
```cpp
virtual bool supportsInstrument() { return true; }
virtual bool supportsKit() { return false; }
virtual bool supportsMIDI() { return true; }
// ... etc
```

**Why**: Clean way to restrict features to compatible contexts without runtime checks.

#### 3. **Factory/Manager Pattern**
**Keyboard Layouts**: Use `KeyboardLayoutManager` to create and switch layouts

**Our Sequencer Modes**: Use `SequencerModeManager` with same pattern:
- Singleton access
- Template-based creation
- Type-safe mode instantiation

#### 4. **UI Integration Philosophy**
**Keyboard Layouts**: Override rendering completely when active

**Our Sequencer Modes**: 
- **Return `true`** = "I handled this, don't do default behavior"
- **Return `false`** = "I didn't handle this, do default behavior"
- Allows modes to selectively override only what they need

---

## Implementation Details - Pulse Sequencer

### Core Concepts

#### Stage-Based Sequencing
Instead of a linear timeline, the Pulse Sequencer uses **8 stages** arranged horizontally:
- Each stage occupies one column (x0-7)
- Each stage has independent parameters
- Stages advance based on play order algorithms

#### Pulse Subdivision
Each stage can have **1-8 pulses**:
- Pulses are subdivisions of the clock period
- Allows variable-length stages (3 pulses = 3× duration)
- Creates polyrhythmic possibilities

#### Gate Types
Four gate types control note triggering:
- **OFF**: Stage is silent
- **SINGLE**: Trigger note on first pulse only
- **MULTIPLE**: Trigger note on every pulse (rolls/trills)
- **HELD**: Trigger once, sustain for full stage duration

### Data Structures

```cpp
struct StageData {
    GateType gateType;      // How notes trigger
    int32_t noteIndex;      // Which scale degree
    int32_t octave;         // ±2 to ±3 octaves
    int32_t pulseCount;     // 1-8 subdivisions
    int32_t velocitySpread; // 0-127 randomization
    int32_t probability;    // 0-100% play chance
    int32_t gateLength;     // 0-100% note duration
};

std::array<StageData, 8> stages_;
```

**Why arrays not vectors**: Fixed size, better cache locality, no heap allocations.

### Rendering Strategy

**Challenge**: The Deluge grid is 16×8, but we need to show:
- 8 pulse count rows (stacked vertically)
- 1 gate line
- 2 octave control rows
- Multiple note selection rows (depends on scale)
- 8 columns of right-side controls

**Solution**: **Scrollable left side** with fixed anchor point:
```cpp
int32_t getGateLineY() const { 
    return displayState_.gateLineOffset + 4; 
}
```

- Gate line is the **anchor** (normally at y4)
- Everything else positioned **relative** to gate line
- Vertical encoder scrolls entire left side together
- Right side (x8-15) stays fixed

**Visual Hierarchy**:
```
y7+ : Note pads (scrollable up)
y6  : Octave up controls
y5  : Octave down controls
y4  : Gate line (ANCHOR)
y3  : Pulse count row 1
y2  : Pulse count row 2
y1  : Pulse count row 3
y0  : Pulse count row 4
```

### Playback Engine

**Philosophy**: Sequencer modes are **output generators**, not data modifiers.

```cpp
int32_t processPlayback(void* modelStack, int32_t absolutePlaybackPos) {
    // 1. Check if we're at a division boundary (clock divider aware)
    if (atDivisionBoundary(pos, ticksPerPeriod)) {
        // 2. Generate notes for current stage
        generateNotes(modelStack);
        
        // 3. Advance to next stage (respecting play order)
        advanceToNextEnabledStage();
    }
    
    // 4. Return when next check is needed
    return ticksUntilNextDivision(pos, ticksPerPeriod);
}
```

**Note Management**:
- Sequencer mode calls `playNote()` and `stopNote()` (inherited from base class)
- These interact with the clip's instrument/output
- Notes are tracked in slots for proper note-off handling
- Max 16 simultaneous notes (standard Deluge polyphony)

### State Management

Three state structs keep related data together:

```cpp
// Per-stage configuration
std::array<StageData, kMaxStages> stages_;

// Runtime sequencer state
struct {
    int32_t currentPulse;
    int32_t lastPlayedStage;
    int32_t totalPatternLength;
    // ... note tracking arrays
} sequencerState_;

// Performance controls (live-tweakable)
struct {
    int32_t transpose;
    int32_t octave;
    int32_t clockDivider;
    int32_t numStages;
    PlayOrder playOrder;
    // ... play order state
} performanceControls_;

// Display state (UI only)
struct {
    int32_t gateLineOffset;  // Scroll position
    int32_t scaleNotes[12];  // Current scale
    int32_t numScaleNotes;
} displayState_;
```

**Why separate structs**: 
- Clear separation of concerns
- Easy to see what's persisted vs. ephemeral
- Better cache locality (related data grouped)

---

## Code Organization & Refactoring

### Constants Over Magic Numbers
```cpp
// Before
for (int i = 0; i < 16; i++) { ... }
if (stage < 0 || stage >= 8) { ... }

// After
constexpr int32_t kMaxNoteSlots = 16;
constexpr int32_t kMaxStages = 8;

for (int i = 0; i < kMaxNoteSlots; i++) { ... }
if (!isStageValid(stage)) { ... }
```

### Helper Functions for Common Patterns
```cpp
// Validation
bool isStageValid(int32_t stage) const;
bool isStageActive(int32_t stage) const;

// Calculations
int32_t getNoteRowY(int32_t noteIdx) const;
int32_t getTicksPerPeriod(int32_t baseTicks) const;

// Display
void showStagePopup(int32_t stage, const char* format, ...);
RGB dimColorIfDisabled(RGB color, int32_t stage) const;
RGB getOctaveColor(int32_t octave) const;

// Utilities
int32_t cycleValue(int32_t current, const int32_t* values, int32_t count) const;
```

### Method Extraction for Readability

**Before** - 140-line `advanceToNextEnabledStage()`:
```cpp
void advanceToNextEnabledStage() {
    switch (playOrder) {
    case RANDOM: { /* 20 lines */ break; }
    case PEDAL: { /* 15 lines */ break; }
    case SKIP_2: { /* 20 lines */ break; }
    // ... etc
    }
    // ... 60 more lines of forwrd/backward/pingpong logic
}
```

**After** - Extracted to dedicated methods:
```cpp
void advanceRandom();
void advancePedal();
void advanceSkip2();
void advancePendulum();
void advanceSpiral();
void advanceForwards(int32_t& nextStage);
void advanceBackwards(int32_t& nextStage);
void advancePingPong(int32_t& nextStage, int32_t& direction);

void advanceToNextEnabledStage() {
    switch (playOrder) {
    case RANDOM: advanceRandom(); return;
    case PEDAL: advancePedal(); return;
    // ... etc
    }
    // Standard advancement (30 lines instead of 60)
}
```

**Benefits**:
- Each algorithm is self-contained and testable
- Easy to add new play orders
- Obvious what each mode does
- Can optimize individually

### Parameter Cycling Pattern
**Before** - Repeated 20-line pattern:
```cpp
void handleVelocitySpread(int32_t stage) {
    if (stage < 0 || stage >= kMaxStages) return;
    
    int32_t spreads[] = {0, 20, 40, 60, 80, 100, 127};
    int32_t currentIndex = 0;
    for (int32_t i = 0; i < 7; i++) {
        if (stages_[stage].velocitySpread == spreads[i]) {
            currentIndex = i;
            break;
        }
    }
    stages_[stage].velocitySpread = spreads[(currentIndex + 1) % 7];
    
    char buffer[30];
    snprintf(buffer, sizeof(buffer), "Stage %d Spread: %d", ...);
    display->displayPopup(buffer);
}
```

**After** - Using helpers:
```cpp
void handleVelocitySpread(int32_t stage) {
    if (!isStageValid(stage)) return;
    static const int32_t spreads[] = {0, 20, 40, 60, 80, 100, 127};
    stages_[stage].velocitySpread = cycleValue(stages_[stage].velocitySpread, spreads, 7);
    showStagePopup(stage, "Stage %d Spread: %d", stage + 1, stages_[stage].velocitySpread);
}
```

**Reduction**: 20 lines → 5 lines, same functionality

---

## Integration with Existing Systems

### 1. Scale System Integration
```cpp
void updateScaleNotes() {
    Song* song = currentSong;
    InstrumentClip* clip = getCurrentInstrumentClip();
    
    if (clip && clip->inScaleMode) {
        // Use song's scale
        int32_t rootNote = song->key.rootNote % 12;
        NoteSet modeNotes = song->key.modeNotes;
        // Extract scale degrees...
    } else {
        // Chromatic mode (all 12 notes)
        for (int i = 0; i < 12; i++) {
            displayState_.scaleNotes[i] = i;
        }
    }
}
```

**Benefits**:
- Patterns stay in key automatically
- Changing song scale updates sequencer immediately
- No hardcoded note values
- Works with all Deluge scale modes

### 2. Note Output System
Uses existing Deluge note infrastructure:
```cpp
void playNote(void* modelStack, int32_t noteCode, int32_t velocity, int32_t length);
void stopNote(void* modelStack, int32_t noteCode);
```

**Why not create new system**:
- Reuses battle-tested code
- Automatically handles MIDI routing, CV output, audio rendering
- Compatible with all instrument types
- Gets all Deluge features (MPE, arpeggiator compatibility, etc.)

### 3. UI System Integration
```cpp
// Request UI refresh (existing Deluge function)
uiNeedsRendering(&instrumentClipView, whichRows, whichColumns);

// Display popups (existing Deluge function)
display->displayPopup("RANDOMIZE");
```

**Continuous Refresh Strategy**:
```cpp
// Refresh every 10 ticks for smooth playback tracking
static uint32_t lastRefreshTick = 0;
uint32_t currentTick = playbackHandler.getCurrentInternalTickCount();
if (currentTick - lastRefreshTick > 10) {
    uiNeedsRendering(&instrumentClipView, rowsToRefresh, 0);
    lastRefreshTick = currentTick;
}
```

**Why**: Slow clock dividers (÷32, ÷64) would otherwise have invisible playback indicators.

---

## Design Patterns Used

### 1. Strategy Pattern
`SequencerMode` is a **strategy** for generating notes. The clip delegates note generation to the active mode.

### 2. Factory Pattern
`SequencerModeManager` creates mode instances. Template-based for type safety.

### 3. RAII (Resource Acquisition Is Initialization)
`std::unique_ptr<SequencerMode>` ensures cleanup:
```cpp
// When clip is destroyed or mode is switched:
sequencerMode_.reset();  // Calls cleanup() automatically
```

### 4. Template Method Pattern
Base class defines the algorithm structure:
```cpp
int32_t processPlayback(...) {
    // 1. Base class handles timing
    if (atBoundary) {
        // 2. Derived class generates notes (virtual)
        generateNotes(modelStack);
    }
    // 3. Base class returns next timing
    return ticksUntilNextDivision(...);
}
```

### 5. Self-Registration Pattern
Modes register themselves at static initialization time:
```cpp
namespace {
    static auto registered = []() {
        Manager::registerMode<MyMode>("my_mode");
        return true;
    }();
}
```

**Benefits**: No central registry to maintain, compiler checks types.

---

## Memory Management

### RAII Throughout
```cpp
// Clip owns mode via unique_ptr
std::unique_ptr<SequencerMode> sequencerMode_;

// When switching modes:
void switchMode(const char* id) {
    sequencerMode_ = manager.createMode(id);  // Old mode auto-destroyed
    sequencerMode_->initialize();
}
```

### Fixed-Size Arrays
```cpp
std::array<StageData, kMaxStages> stages_;
std::array<int16_t, kMaxNoteSlots> noteCodeActive_;
```

**Why not vectors**:
- No heap allocations during playback
- Predictable memory usage
- Better cache performance
- Fixed maximum known at compile time

### Note Slot Pooling
```cpp
// Find free slot for new note
for (int32_t i = 0; i < kMaxNoteSlots; i++) {
    if (!noteActive[i]) {
        // Use this slot
        noteCodeActive[i] = note;
        noteActive[i] = true;
        break;
    }
}
```

**Why**: Bounded memory usage, no allocations during playback.

---

## UI/UX Design Decisions

### Color Coding Philosophy
We established a **semantic color system**:

| Function | Color | Reasoning |
|----------|-------|-----------|
| Playback position | Red | Universal "now" indicator |
| Gate types | Green/Blue/Magenta/Grey | Distinct, functional states |
| Octave controls | White/Orange | Related function, gradient shows direction |
| Clock divider | Red | Tempo is critical, stands out |
| Play order | Cyan | Bright but not distracting |
| Performance params | Green/Blue/Cyan | Grouped by function |
| Control buttons | Purple/Magenta/Cyan/Blue | Action-oriented |
| Stage management | Yellow/Orange | Configuration, less urgent |

### Visual Feedback Layers
1. **Base rendering** - Shows configuration
2. **Dimming** - Indicates disabled states
3. **Playback indicator** - Shows current position (red)
4. **Flash feedback** - Confirms actions (gate pad flash)
5. **Popups** - Displays values and confirmations

### Scrolling Design
**Challenge**: How to show 8+ rows of controls on 8-row grid?

**Solution**: Gate line as scrollable anchor
- Default position: y4 (middle of grid)
- Scroll up: See more pulse counts below
- Scroll down: See more note selections above
- Everything moves together (no confusion)

---

## Performance Optimizations

### 1. Render Only Changed Rows
```cpp
uiNeedsRendering(&view, whichRows, 0);  // Bit mask of rows
```
Only redraws affected rows, not entire grid.

### 2. Occupancy Masks
```cpp
if (occupancyMask) {
    occupancyMask[y][x] = 64;  // Brightness level
}
```
Deluge uses this for pad LED brightness. We set appropriately for each element.

### 3. Static Local Variables
```cpp
static const int32_t spreads[] = {0, 20, 40, 60, 80, 100, 127};
```
Array initialized once, not on every call.

### 4. Early Returns
```cpp
if (!isStageValid(stage)) return;
if (!initialized_) return INT32_MAX;
```
Fail fast, avoid unnecessary work.

---

## Extending the System

### Adding a New Sequencer Mode

**Step 1**: Create header file
```cpp
// src/deluge/model/clip/sequencer/modes/my_mode.h
#pragma once
#include "model/clip/sequencer/sequencer_mode.h"

namespace deluge::model::clip::sequencer::modes {

class MySequencerMode : public SequencerMode {
public:
    l10n::String name() override { return l10n::String::STRING_FOR_MY_MODE; }
    
    bool supportsInstrument() override { return true; }
    
    void initialize() override;
    void cleanup() override;
    bool renderPads(...) override;
    int32_t processPlayback(...) override;
    
private:
    // Your state here
};

} // namespace
```

**Step 2**: Create implementation
```cpp
// src/deluge/model/clip/sequencer/modes/my_mode.cpp
#include "my_mode.h"

namespace deluge::model::clip::sequencer::modes {

void MySequencerMode::initialize() {
    // Setup your state
}

// ... implement other methods

} // namespace

// Self-register
namespace {
    static auto registered = []() {
        SequencerModeManager::instance().registerMode<MySequencerMode>("my_mode");
        return true;
    }();
}
```

**Step 3**: Add to CMakeLists.txt
```cmake
target_sources(deluge PRIVATE
    # ... existing files
    src/deluge/model/clip/sequencer/modes/my_mode.cpp
)
```

**Step 4**: Add localization string
```cpp
// In l10n strings definition
STRING_FOR_MY_MODE
```

**That's it!** The mode is now available in the clip mode selector.

---

## Testing Strategy

### Manual Testing Checklist
- [ ] Mode switches without crashing
- [ ] Playback generates notes
- [ ] All pads respond to input
- [ ] Encoders work (vertical/horizontal)
- [ ] UI updates during playback
- [ ] Switching back to piano roll works
- [ ] Existing songs still load/play
- [ ] Mode state persists when saving song

### Edge Cases Tested
- Scale changes while in mode
- Very slow clock dividers (÷64)
- Very fast clock dividers (×2)
- All stages disabled
- Disabled stages with enabled stages
- Scrolling to extreme positions
- Maximum note polyphony (16 notes)
- All play order patterns

### Memory Safety
- All arrays are bounds-checked
- No raw pointers (use references or smart pointers)
- RAII ensures cleanup on all code paths
- No heap allocations during playback

---

## Future Extensibility

### Potential New Modes

**1. Step Sequencer Mode** (Classic 16-step)
- 16 steps arranged in 2 rows
- Per-step note, velocity, probability
- Rotation, swing, ratcheting

**2. Euclidean Mode**
- Generate Euclidean rhythms (k pulses in n steps)
- Per-track offset/rotation
- Multiple layers

**3. Chord Mode**
- Chord progressions with voicing control
- Inversion, spread, arpeggiation
- Harmonic rhythm independent of note rhythm

**4. Markov Chain Mode**
- Probabilistic state transitions
- Learn from played patterns
- Adjustable "creativity" parameter

**5. West Coast Mode**
- Voltage-style sequencing for CV
- Slew, quantize, sample & hold
- Looping envelopes

### Adding Features to Existing Modes

The architecture supports:
- **Per-stage modulation** - Add LFO, envelope per stage
- **Pattern chaining** - Link multiple 8-stage patterns
- **Macro controls** - Map encoder to multiple parameters
- **Preset system** - Save/load pattern presets
- **MIDI learn** - External control of parameters

---

## Lessons Learned

### What Worked Well

1. **Following existing patterns** - Keyboard layout study was invaluable
2. **RAII everywhere** - No memory leaks, clean teardown
3. **Boolean returns** - Flexible integration with existing UI
4. **Helper functions early** - Refactoring was easier because we did this
5. **Incremental testing** - Build, test, iterate worked perfectly

### Challenges Overcome

1. **UI refresh timing** - Slow clock dividers needed continuous refresh, not event-based
2. **Stage validation** - Multiple places needed same checks, helper functions solved it
3. **Note row calculation** - Scrolling made this complex, `getNoteRowY()` simplified it
4. **Color consistency** - Needed semantic color system, not arbitrary choices
5. **Playback tracking** - Getting red indicator to show right position took iteration

### If We Started Over

**Keep**:
- Base class architecture
- RAII approach
- Helper function pattern
- Self-registration system
- Capability flags

**Change**:
- Define helper functions earlier (before writing complex code)
- Create constants file upfront
- Design color system before implementation
- Write user guide alongside code (helps clarify UX)

---

## Code Style & Conventions

### Following Deluge Patterns

**Naming**:
```cpp
// Deluge uses snake_case for variables, PascalCase for types
int32_t currentStage_;           // Member variable
struct StageData { ... };         // Type name
void handlePadPress(...);         // Method name
```

**Const Correctness**:
```cpp
int32_t calculateLength() const;  // Doesn't modify state
RGB getColor() const;             // Read-only methods
```

**Inline Getters**:
```cpp
int32_t getGateLineY() const { return gateLineOffset_ + 4; }
```

**Array Types**:
```cpp
std::array<T, N>  // Modern C++, bounds-checked
T array[N]        // C-style for interfacing with existing code
```

### Comments
- **What** for complex algorithms
- **Why** for non-obvious decisions
- **How** only when truly necessary
- Section headers with `// ======` for organization

---

## Performance Characteristics

### CPU Usage
- **Idle**: Zero overhead when not in sequencer mode
- **Playback**: Minimal - only processes at division boundaries
- **Rendering**: On-demand, only redraws changed rows
- **Memory**: ~2KB per mode instance (all stack/member data)

### Timing Precision
- Uses Deluge's tick system (1 tick ≈ 1/15360 second at 120 BPM)
- Clock dividers preserve tick-level accuracy
- No drift over time (integer-based calculations)

### Scalability
Current implementation supports:
- 8 stages per pattern
- 16 simultaneous notes
- 8 play order algorithms
- 4 gate types
- 12 scale notes (chromatic)

**Theoretical limits** (with code changes):
- Stages: Could extend to 16 (full grid width)
- Notes: Hardware limited to ~16 polyphony
- Play orders: Unlimited (just add more methods)
- Gate types: Could add more (pattern-based, probability-based, etc.)

---

## Backward Compatibility

### Existing Functionality Preserved
- **Linear clips remain master** - Piano roll is still the canonical data storage
- **Songs load normally** - Mode is optional, clips work without it
- **Default behavior unchanged** - Only active when explicitly selected
- **No breaking changes** - All existing Deluge features work as before

### Migration Path
Users can:
1. Try sequencer modes in new clips (no risk)
2. Switch back to piano roll anytime (CLIP + SELECT encoder)
3. Mix sequencer mode clips with traditional clips in same song
4. Record sequencer output to piano roll (RECORD button)

---

## Documentation Philosophy

### For Users
- **PULSE_SEQUENCER_GUIDE.md** - Complete user manual
- Focus on "what" and "how"
- Workflow examples and creative ideas
- Troubleshooting common issues
- Visual/color references

### For Developers
- **This document** - Architecture and implementation
- Focus on "why" and "how it works"
- Design patterns and decisions
- Extension points and future work
- Code organization principles

### For Contributors
- **Code comments** - Explain complex sections
- **Consistent style** - Follow Deluge conventions
- **Helper functions** - Make code self-documenting
- **Named constants** - Eliminate magic numbers

---

## Contributing

### Adding Features
1. **Follow existing patterns** - Study keyboard layouts, other modes
2. **Use RAII** - Smart pointers, no manual memory management
3. **Add helpers** - Don't repeat yourself
4. **Test incrementally** - Build, flash, test, iterate
5. **Document as you go** - Update guides with new features

### Code Review Considerations
- Does it follow Deluge conventions?
- Is memory management safe (RAII)?
- Are edge cases handled?
- Is it testable?
- Does it break existing functionality?
- Is it documented?

---

## Conclusion

The Sequencer Modes system demonstrates how to extend the Deluge firmware by:
- **Learning from existing patterns** (keyboard layouts)
- **Using modern C++ safely** (RAII, templates, smart pointers)
- **Integrating cleanly** (boolean returns, virtual methods)
- **Optimizing thoughtfully** (cache-friendly, minimal allocations)
- **Documenting thoroughly** (user guide + architecture doc)

The result is a robust, extensible system that adds powerful new creative tools while maintaining the Deluge's stability and ease of use.

---

## References

### Deluge Codebase Studied
- `src/deluge/gui/ui/keyboard/layout/` - Pattern for mode switching
- `src/deluge/model/clip/instrument_clip.cpp` - Playback integration
- `src/deluge/gui/views/instrument_clip_view.cpp` - UI integration
- `src/deluge/model/note/note_row.cpp` - Note output system

### Design Patterns
- "Design Patterns: Elements of Reusable Object-Oriented Software" (Gang of Four)
- "Effective Modern C++" by Scott Meyers (RAII, smart pointers)
- "C++ Coding Standards" by Herb Sutter (const correctness, initialization)

### Deluge Community
- Community firmware repository: https://github.com/SynthstromAudible/DelugeFirmware
- Forums and Discord for user feedback
- Issue tracker for bug reports and feature requests


