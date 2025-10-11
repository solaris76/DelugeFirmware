# Sequencer Control Columns - Revised Architecture

## Overview

After reviewing the chord memory column implementation, this revised design uses **per-pad configurable controls** instead of fixed column types. Each of the 8 pads in both sidebar columns (x16, x17) can be independently configured.

## Key Design Decisions

### 1. Per-Pad Storage (Chord Memory Pattern)
- Each pad (y0-y7) stores its own configuration
- User assigns control type to each pad individually
- Values persist with the clip/song

### 2. Interaction Model

**Assigning Control Type:**
- Short press pad → cycles through control types
- Visual feedback via pad color:
  - 🔴 Red = Clock Division
  - 🔵 Blue = Octave
  - 🟢 Cyan = Transpose
  - 🟡 Yellow = Probability
  - 🟣 Magenta = Velocity
  - 🟠 Orange = Gate Length
  - ⚪ White = Swing

**Setting Value:**
- Hold pad + turn horizontal encoder → adjust value
- Display shows current value on OLED

**Toggle Mode:**
- Hold pad + press horizontal encoder → toggle between:
  - **Toggle mode**: Press once = stays on, press again = off
  - **Momentary mode**: Hold = on, release = off
- Visual indicator: brightness/blink pattern

**Clearing Pad:**
- Hold SHIFT + press pad → clear pad configuration

### 3. Multiple Active Controls
- Multiple toggle pads can be active simultaneously
- Effects stack (e.g., clock div 2x + transpose +7 + octave +1)
- Momentary pads activate only while held

---

## Architecture

### Directory Structure

```
src/deluge/model/clip/sequencer/
├── control_columns/
│   ├── sequencer_control_column.h/.cpp
│   └── control_column_state.h
└── sequencer_mode.h/.cpp (existing - will be extended)
```

### Data Structures

```cpp
// src/deluge/model/clip/sequencer/control_columns/sequencer_control_column.h

#pragma once

#include "definitions_cxx.hpp"
#include "gui/colour/rgb.h"
#include "hid/led/pad_leds.h"
#include <cstdint>

namespace deluge::model::clip::sequencer {

// Control types that can be assigned to pads
enum class ControlType : uint8_t {
	NONE = 0,          // Empty pad
	CLOCK_DIVISION,    // Clock div: 1/4, 1/2, 1x, 2x, 4x, 8x, 16x, 32x
	OCTAVE,            // Octave shift: -2, -1, 0, +1, +2, +3, +4, +5
	TRANSPOSE,         // Semitone transpose: -12 to +12
	PROBABILITY,       // Note probability: 0-100%
	VELOCITY,          // Velocity multiplier: 50-150%
	GATE_LENGTH,       // Gate length multiplier: 25%, 50%, 75%, 100%, 125%, 150%, 200%
	SWING,             // Swing amount: 50-75%
	MAX_TYPES
};

// Pad behavior mode
enum class PadMode : uint8_t {
	TOGGLE = 0,        // Press to activate, press again to deactivate
	MOMENTARY = 1,     // Hold to activate, release to deactivate
};

// Configuration for a single pad
struct PadConfig {
	ControlType type = ControlType::NONE;
	int32_t value = 0;              // Control-specific value
	PadMode mode = PadMode::TOGGLE;
	bool isActive = false;          // Current activation state (for toggle mode)

	// Color for rendering (based on control type)
	RGB getColor() const;
	
	// Get display name for this control
	const char* getName() const;
	
	// Get value range for this control type
	int32_t getMinValue() const;
	int32_t getMaxValue() const;
	int32_t getDefaultValue() const;
	
	// Format value for display
	void formatValue(char* buffer, size_t bufferSize) const;
};

// Control column state (8 pads per column)
class SequencerControlColumn {
public:
	SequencerControlColumn() = default;

	// Render the column
	void renderColumn(RGB image[][kDisplayWidth + kSideBarWidth], 
	                  int32_t column, 
	                  int8_t heldPad) const;

	// Handle pad press/release
	// Returns true if state changed (needs redraw)
	bool handlePad(int8_t padY, bool pressed, bool shiftPressed);

	// Handle encoder while pad is held
	// offset: encoder rotation amount
	// encoderPressed: true if encoder button pressed during turn
	// Returns true if handled
	bool handleEncoder(int8_t heldPad, int32_t offset, bool encoderPressed);

	// Get effective control values for playback
	// Returns accumulated effect of all active pads
	struct ActiveControls {
		float clockDivMultiplier = 1.0f;  // Multiply clock speed
		int32_t octaveShift = 0;          // Octave offset
		int32_t transpose = 0;            // Semitone offset
		int32_t probability = 100;        // Note probability (0-100)
		float velocityMultiplier = 1.0f;  // Velocity scale
		float gateLengthMultiplier = 1.0f; // Gate length scale
		float swing = 0.5f;               // Swing amount (0.5 = no swing)
	};
	
	ActiveControls getActiveControls() const;

	// Serialization
	void writeToFile(Serializer& writer, const char* tagName) const;
	void readFromFile(Deserializer& reader);

private:
	PadConfig pads_[kDisplayHeight]; // 8 pads (y0-y7)
	
	// Cycle to next control type
	ControlType cycleControlType(ControlType current) const;
};

} // namespace deluge::model::clip::sequencer
```

### Control Column State

```cpp
// src/deluge/model/clip/sequencer/control_columns/control_column_state.h

#pragma once

#include "model/clip/sequencer/control_columns/sequencer_control_column.h"

namespace deluge::model::clip::sequencer {

struct ControlColumnState {
	SequencerControlColumn leftColumn;
	SequencerControlColumn rightColumn;

	// Get combined active controls from both columns
	SequencerControlColumn::ActiveControls getActiveControls() const {
		auto left = leftColumn.getActiveControls();
		auto right = rightColumn.getActiveControls();
		
		// Combine effects from both columns
		SequencerControlColumn::ActiveControls combined;
		combined.clockDivMultiplier = left.clockDivMultiplier * right.clockDivMultiplier;
		combined.octaveShift = left.octaveShift + right.octaveShift;
		combined.transpose = left.transpose + right.transpose;
		combined.probability = std::min(left.probability, right.probability); // Most restrictive
		combined.velocityMultiplier = left.velocityMultiplier * right.velocityMultiplier;
		combined.gateLengthMultiplier = left.gateLengthMultiplier * right.gateLengthMultiplier;
		combined.swing = (left.swing + right.swing) / 2.0f; // Average
		
		return combined;
	}

	// Serialization
	void writeToFile(Serializer& writer) {
		leftColumn.writeToFile(writer, "leftControlColumn");
		rightColumn.writeToFile(writer, "rightControlColumn");
	}

	void readFromFile(Deserializer& reader) {
		char const* tagName;
		reader.match('{');
		while (*(tagName = reader.readNextTagOrAttributeName())) {
			if (!strcmp(tagName, "leftControlColumn")) {
				leftColumn.readFromFile(reader);
			}
			else if (!strcmp(tagName, "rightControlColumn")) {
				rightColumn.readFromFile(reader);
			}
			else {
				reader.exitTag(tagName);
			}
		}
		reader.match('}');
	}
};

} // namespace deluge::model::clip::sequencer
```

### Integration with SequencerMode

```cpp
// Add to sequencer_mode.h

class SequencerMode {
public:
	// ... existing methods ...

	// Control column integration
	ControlColumnState& getControlColumnState() { return controlColumnState_; }
	const ControlColumnState& getControlColumnState() const { return controlColumnState_; }

	// Get active control effects for use in playback
	SequencerControlColumn::ActiveControls getActiveControls() const {
		return controlColumnState_.getActiveControls();
	}

protected:
	ControlColumnState controlColumnState_;
};
```

---

## Control Type Details

### Clock Division
- **Values**: -4 (1/4), -2 (1/2), 1 (1x), 2, 4, 8, 16, 32
- **Color**: Red (255, 0, 0)
- **Display**: "1/4", "1/2", "1x", "2x", "4x", "8x", "16x", "32x"
- **Effect**: Multiplies clock speed (negative = division, positive = multiplication)

### Octave
- **Values**: -2, -1, 0, +1, +2, +3, +4, +5
- **Color**: Blue (0, 0, 255)
- **Display**: "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5"
- **Effect**: Shifts notes by N octaves (12 semitones each)

### Transpose
- **Values**: -12 to +12
- **Color**: Cyan (0, 255, 255)
- **Display**: "-12", "-7", "0", "+5", "+12", etc.
- **Effect**: Shifts notes by N semitones

### Probability
- **Values**: 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
- **Color**: Yellow (255, 255, 0)
- **Display**: "0%", "50%", "100%", etc.
- **Effect**: Random chance for notes to play

### Velocity
- **Values**: 50-150 (percentage)
- **Color**: Magenta (255, 0, 255)
- **Display**: "50%", "100%", "125%", etc.
- **Effect**: Multiplies note velocity

### Gate Length
- **Values**: 25, 50, 75, 100, 125, 150, 200 (percentage)
- **Color**: Orange (255, 128, 0)
- **Display**: "25%", "100%", "200%", etc.
- **Effect**: Multiplies note length

### Swing
- **Values**: 50-75 (percentage)
- **Color**: White (255, 255, 255)
- **Display**: "50%", "60%", "75%", etc.
- **Effect**: Swing timing (50% = straight, 75% = max swing)

---

## Rendering

### Pad States
- **Empty pad**: Dim gray (16, 16, 16)
- **Configured pad**: Control type color at medium brightness (128)
- **Active toggle pad**: Control type color at full brightness (255)
- **Held pad (adjusting)**: Control type color blinking
- **Momentary mode**: Slightly dimmer than toggle mode

### Visual Indicators
```
Normal:     ████  (solid color, medium brightness)
Active:     ████  (solid color, full brightness)
Momentary:  ▓▓▓▓  (solid color, slightly dim)
Adjusting:  ◐◑◐◑  (blinking)
```

---

## Example Usage

### Scenario 1: Simple Octave Control
1. Press pad y0 x16 → cycles to Octave (blue)
2. Hold pad y0 x16 + turn encoder → set to +2 octaves
3. Release → display shows "OCTAVE +2"
4. Pad stays blue at full brightness (toggle mode, active)
5. All sequencer notes now play +2 octaves higher

### Scenario 2: Momentary Transpose
1. Press pad y1 x16 repeatedly → cycles to Transpose (cyan)
2. Hold pad y1 x16 + turn encoder → set to +7 semitones
3. Hold pad y1 x16 + press encoder → toggle to momentary mode
4. Pad shows cyan at medium brightness (momentary mode, inactive)
5. Hold pad y1 x16 → notes transpose +7 while held
6. Release → notes return to normal

### Scenario 3: Complex Combination
- Pad y0 x16: Clock Div 2x (toggle, active) = double speed
- Pad y1 x16: Octave +1 (toggle, active) = one octave up
- Pad y2 x16: Transpose +3 (momentary, held) = +3 semitones while held
- Pad y0 x17: Probability 50% (toggle, active) = 50% note chance
- **Result**: Double speed + octave up + +3 semitones (while held) + 50% probability

---

## Serialization Format

```xml
<sequencerMode name="PULSE SEQ">
	<leftControlColumn>
		<pad y="0" type="clock_div" value="2" mode="toggle" active="true"/>
		<pad y="1" type="octave" value="1" mode="toggle" active="true"/>
		<pad y="2" type="transpose" value="3" mode="momentary" active="false"/>
	</leftControlColumn>
	<rightControlColumn>
		<pad y="0" type="probability" value="50" mode="toggle" active="true"/>
	</rightControlColumn>
</sequencerMode>
```

---

## Implementation Plan

### Phase 1: Core Data Structures ✅
1. Create `control_columns/` directory
2. Implement `PadConfig` struct
3. Implement `SequencerControlColumn` class
4. Implement `ControlColumnState` struct

### Phase 2: Basic Interaction
5. Implement pad press cycling through control types
6. Implement rendering with color-coded pads
7. Implement encoder value adjustment
8. Add OLED display feedback

### Phase 3: Toggle/Momentary
9. Implement encoder button press to toggle mode
10. Implement toggle activation state tracking
11. Implement momentary activation logic
12. Add visual indicators for mode

### Phase 4: Integration
13. Add control column state to `SequencerMode`
14. Hook pad events in clip view (when sequencer mode active)
15. Hook encoder events
16. Implement `getActiveControls()` for playback use

### Phase 5: Serialization
17. Implement XML write for control column state
18. Implement XML read for control column state
19. Integrate with clip save/load
20. Test persistence across sessions

### Phase 6: Polish
21. Add shift+pad to clear configuration
22. Add pad long-press feedback
23. Optimize rendering performance
24. Add error handling for invalid values

---

## Next Steps

**Ready to implement Phase 1?** 

I'll create:
1. The directory structure
2. `PadConfig` struct with color/name/range helpers
3. `SequencerControlColumn` class with basic methods
4. `ControlColumnState` struct

This establishes the foundation without touching any existing functionality.

Shall I proceed?

