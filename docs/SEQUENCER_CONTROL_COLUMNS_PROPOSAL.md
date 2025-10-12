# Sequencer Control Columns Architecture Proposal

## Overview

This document proposes adapting the keyboard control column system for use with sequencer modes, allowing users to configure performative parameters (clock division, octave, transpose, etc.) on the sidebar columns (x16-17).

## Existing Keyboard Architecture Analysis

### Components

1. **Base Class** (`control_column.h`)
   - Pure virtual interface with 4 methods:
     - `renderColumn()` - Draw the column
     - `handleVerticalEncoder()` - Respond to vertical encoder while holding a pad
     - `handleLeavingColumn()` - Clean up when switching columns
     - `handlePad()` - Handle pad press/release events

2. **Concrete Implementations** (`column_controls/`)
   - Each control type is a separate class (VelocityColumn, ModColumn, ChordColumn, etc.)
   - Self-contained state management
   - Custom rendering logic per control type
   - Parameter persistence (e.g., `storedVelocity`, `velocityMin`, `velocityMax`)

3. **State Management** (`column_control_state.h`)
   - Single struct holding all control column instances
   - Pointers to active left/right columns
   - Enum for column function types
   - Factory method `getColumnForFunc()`
   - Serialization support (XML read/write)

4. **Manager Class** (`ColumnControlsKeyboard`)
   - Mix-in style class (inherits from KeyboardLayout)
   - Routes pad events to correct column
   - Handles column switching (hold pad 7, turn encoder)
   - Tracks which column pads are held (`leftColHeld`, `rightColHeld`)
   - Allows/disallows certain column types per layout

### Key Design Patterns

- **Polymorphism**: Base interface + concrete implementations
- **State Pattern**: Column switching changes active column pointer
- **Factory Pattern**: `getColumnForFunc()` creates appropriate column instance
- **Serialization**: State persists to XML for clip/song storage
- **Mix-in**: `ColumnControlsKeyboard` adds column support to any layout

---

## Proposed Sequencer Control Column Architecture

### 1. Directory Structure

```
src/deluge/model/clip/sequencer/
├── sequencer_mode.h/.cpp (existing)
├── sequencer_mode_manager.h/.cpp (existing)
├── control_columns/
│   ├── sequencer_control_column.h (base class)
│   ├── clock_division.h/.cpp (clock div: 1/4, 1/2, 1x, 2x, 4x, 8x, 16x, 32x)
│   ├── octave.h/.cpp (octave shift: -2, -1, 0, +1, +2, +3, +4, +5)
│   ├── transpose.h/.cpp (semitone transpose: -7 to +7)
│   ├── probability.h/.cpp (note probability: 0%, 20%, 40%, 60%, 80%, 100%)
│   ├── velocity.h/.cpp (velocity control: similar to keyboard)
│   ├── gate_length.h/.cpp (note length multiplier)
│   └── swing.h/.cpp (swing amount per step)
└── control_column_state.h (state management)
```

### 2. Base Class: `SequencerControlColumn`

```cpp
// src/deluge/model/clip/sequencer/control_columns/sequencer_control_column.h

#pragma once

#include "gui/colour/rgb.h"
#include "hid/led/pad_leds.h"
#include "model/model_stack.h"

namespace deluge::model::clip::sequencer {
class SequencerMode;
}

namespace deluge::model::clip::sequencer::controls {

/**
 * Base class for sequencer control columns (sidebar x16-17).
 * Each concrete implementation provides a different parameter control
 * that can be assigned to the left or right sidebar column.
 */
class SequencerControlColumn {
public:
	virtual ~SequencerControlColumn() = default;

	// Render the column on the grid (y0-7 at specified x column)
	virtual void renderColumn(RGB image[][kDisplayWidth + kSideBarWidth], 
	                          int32_t column, 
	                          SequencerMode* mode) = 0;

	// Handle vertical encoder while holding a pad in this column
	// pad: y position (0-7) of held pad
	// offset: encoder rotation amount
	// Returns true if handled, false if not applicable
	virtual bool handleVerticalEncoder(int8_t pad, int32_t offset) = 0;

	// Called when switching away from this column (to restore defaults)
	virtual void handleLeavingColumn(ModelStackWithTimelineCounter* modelStack,
	                                 SequencerMode* mode) = 0;

	// Handle pad press/release in this column
	// pad: PressedPad struct with x, y, velocity, active, etc.
	virtual void handlePad(ModelStackWithTimelineCounter* modelStack,
	                       PressedPad pad,
	                       SequencerMode* mode) = 0;

	// Get the current value (for reading by sequencer mode)
	// Each column defines what this means (clock div multiplier, octave shift, etc.)
	virtual int32_t getValue() const = 0;

	// Get display name for UI
	virtual const char* getName() const = 0;

	// Serialization
	virtual void writeToFile(Serializer& writer) {}
	virtual void readFromFile(Deserializer& reader) {}

protected:
	SequencerControlColumn() = default;
};

} // namespace deluge::model::clip::sequencer::controls
```

### 3. State Management: `ControlColumnState`

```cpp
// src/deluge/model/clip/sequencer/control_column_state.h

#pragma once

#include "model/clip/sequencer/control_columns/sequencer_control_column.h"
#include "model/clip/sequencer/control_columns/clock_division.h"
#include "model/clip/sequencer/control_columns/octave.h"
#include "model/clip/sequencer/control_columns/transpose.h"
#include "model/clip/sequencer/control_columns/probability.h"
#include "model/clip/sequencer/control_columns/velocity.h"
#include "model/clip/sequencer/control_columns/gate_length.h"
#include "model/clip/sequencer/control_columns/swing.h"

namespace deluge::model::clip::sequencer {

enum SequencerControlFunction : int8_t {
	SEQ_CTRL_CLOCK_DIV = 0,
	SEQ_CTRL_OCTAVE,
	SEQ_CTRL_TRANSPOSE,
	SEQ_CTRL_PROBABILITY,
	SEQ_CTRL_VELOCITY,
	SEQ_CTRL_GATE_LENGTH,
	SEQ_CTRL_SWING,
	SEQ_CTRL_MAX,
};

struct ControlColumnState {
	// All available control column instances
	controls::ClockDivisionColumn clockDivColumn;
	controls::OctaveColumn octaveColumn;
	controls::TransposeColumn transposeColumn;
	controls::ProbabilityColumn probabilityColumn;
	controls::VelocityColumn velocityColumn;
	controls::GateLengthColumn gateLengthColumn;
	controls::SwingColumn swingColumn;

	// Active column configuration
	SequencerControlFunction leftColFunc = SEQ_CTRL_CLOCK_DIV;
	SequencerControlFunction rightColFunc = SEQ_CTRL_OCTAVE;
	controls::SequencerControlColumn* leftCol = &clockDivColumn;
	controls::SequencerControlColumn* rightCol = &octaveColumn;

	// Factory method
	controls::SequencerControlColumn* getColumnForFunc(SequencerControlFunction func);

	// Serialization
	void writeToFile(Serializer& writer);
	void readFromFile(Deserializer& reader);
};

} // namespace deluge::model::clip::sequencer
```

### 4. Integration with SequencerMode

Add to `SequencerMode` class:

```cpp
class SequencerMode {
public:
	// ... existing methods ...

	// NEW: Control column integration
	virtual bool allowControlColumn(SequencerControlFunction func) { 
		return true; // Default: allow all
	}

	// Access control column state
	ControlColumnState& getControlColumnState() { return controlColumnState_; }
	const ControlColumnState& getControlColumnState() const { return controlColumnState_; }

protected:
	// Control column state (per-instance)
	ControlColumnState controlColumnState_;
};
```

### 5. Example Implementation: Clock Division Column

```cpp
// src/deluge/model/clip/sequencer/control_columns/clock_division.h

#pragma once

#include "model/clip/sequencer/control_columns/sequencer_control_column.h"

namespace deluge::model::clip::sequencer::controls {

class ClockDivisionColumn : public SequencerControlColumn {
public:
	ClockDivisionColumn() = default;

	void renderColumn(RGB image[][kDisplayWidth + kSideBarWidth], 
	                  int32_t column, 
	                  SequencerMode* mode) override;

	bool handleVerticalEncoder(int8_t pad, int32_t offset) override;

	void handleLeavingColumn(ModelStackWithTimelineCounter* modelStack,
	                         SequencerMode* mode) override;

	void handlePad(ModelStackWithTimelineCounter* modelStack,
	               PressedPad pad,
	               SequencerMode* mode) override;

	int32_t getValue() const override { return clockDivisions_[currentDivisionIdx_]; }

	const char* getName() const override { return "CLOCK DIV"; }

	void writeToFile(Serializer& writer) override;
	void readFromFile(Deserializer& reader) override;

private:
	// Clock divisions: 1/4, 1/2, 1x, 2x, 4x, 8x, 16x, 32x
	// Stored as numerators (negative = division, positive = multiplication)
	static constexpr int32_t clockDivisions_[8] = {-4, -2, 1, 2, 4, 8, 16, 32};
	int32_t currentDivisionIdx_ = 2; // Default: 1x (no change)
	int32_t storedDivisionIdx_ = 2;
};

} // namespace deluge::model::clip::sequencer::controls
```

---

## Implementation Plan

### Phase 1: Core Infrastructure
1. ✅ Create `control_columns/` directory
2. ✅ Implement `SequencerControlColumn` base class
3. ✅ Implement `ControlColumnState` struct
4. ✅ Add control column state to `SequencerMode`

### Phase 2: Basic Control Columns
5. Implement `ClockDivisionColumn` (simple, high impact)
6. Implement `OctaveColumn` (similar to clock div)
7. Implement `TransposeColumn` (similar to octave)

### Phase 3: Advanced Control Columns
8. Implement `ProbabilityColumn`
9. Implement `VelocityColumn` (adapt from keyboard)
10. Implement `GateLengthColumn`
11. Implement `SwingColumn`

### Phase 4: Integration
12. Add column switching UI (hold pad 7, turn encoder)
13. Add sidebar rendering hooks to `SequencerMode::renderSidebar()`
14. Add pad routing in clip view when sequencer mode is active
15. Add serialization support to save column state with clips

### Phase 5: Testing & Polish
16. Test column switching
17. Test parameter persistence
18. Test per-mode column restrictions
19. Add display popups for parameter values

---

## Benefits of This Approach

1. **Follows Existing Patterns**: Uses proven architecture from keyboard system
2. **RAII Design**: Each column manages its own state
3. **Extensible**: Easy to add new control column types
4. **Per-Mode Customization**: Modes can allow/disallow specific columns
5. **Serialization Ready**: State persists with clips/songs
6. **Performative**: Real-time parameter control during playback
7. **Consistent UX**: Same interaction model as keyboard columns

---

## Usage Example

Once implemented, users can:

1. Hold CLIP button to enter sequencer mode view
2. Select a generative/pattern sequencer mode
3. Hold top-right pad (y7, x16) and turn horizontal encoder to switch left column
4. Hold top-right pad (y7, x17) and turn horizontal encoder to switch right column
5. Press pads in columns to set values (e.g., pad 0 = 1/4 clock div, pad 7 = 32x)
6. Hold pad and turn vertical encoder for fine adjustments
7. Parameters persist with the clip

This creates a powerful, performative sequencer system where users can configure pattern behaviors in real-time!

---

## Next Steps

**Question for you:** Should we proceed with Phase 1 (core infrastructure) first? 

I can implement:
1. The base class `SequencerControlColumn`
2. The state struct `ControlColumnState`
3. Integration hooks in `SequencerMode`
4. A simple first implementation (ClockDivisionColumn) as proof of concept

This would give us a working foundation without breaking anything, following our "safety first" approach.

What do you think?


