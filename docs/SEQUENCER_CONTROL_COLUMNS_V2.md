# Sequencer Control Columns V2

## Overview
Sequencer modes get configurable control columns similar to keyboard view, but with a split-column design for more flexibility.

## Design

### Column Layout
- **x16 (Left Sidebar Column)**: 8 pads split into 2 groups of 4
  - Top 4 pads (y4-y7): One control type
  - Bottom 4 pads (y0-y3): Another control type
- **x17 (Right Sidebar Column)**: Same split design
  - Top 4 pads (y4-y7): One control type
  - Bottom 4 pads (y0-y3): Another control type

This gives us **4 separate control groups** per sequencer view.

### Control Types (Phase 1)

1. **Clock Divider**
   - Values: `*2`, `/1`, `/2`, `/3`, `/4`, `/6`, `/8`, `/12`, `/16`, `/24`, `/32`, `/48`, `/64`
   - Each pad in the group stores a different divider value
   - Affects playback timing when pad is active

2. **Octave Control**
   - Range: `-5` to `+5` (11 values)
   - Each pad stores an octave shift value
   - Affects note pitch when pad is active

3. **Transpose**
   - Range: `-12` to `+12` semitones (25 values)
   - Each pad stores a transpose amount
   - Affects note pitch when pad is active

4. **Scenes** (Phase 2)
   - Each pad saves/recalls complete sequencer state
   - Like arranger scenes but for sequencer parameters

### Interaction Model

#### Changing Control Type for a Group
- **Hold y7 on x16** + turn horizontal encoder → cycle control types for **top 4 pads** of x16
- **Hold y3 on x16** + turn horizontal encoder → cycle control types for **bottom 4 pads** of x16
- **Hold y7 on x17** + turn horizontal encoder → cycle control types for **top 4 pads** of x17
- **Hold y3 on x17** + turn horizontal encoder → cycle control types for **bottom 4 pads** of x17
- Display shows control type name (e.g., "CLOCK DIV", "OCTAVE", "TRANSPOSE")

#### Setting Values
- **Hold any control pad** + turn horizontal encoder → adjust value for that pad
- Display shows current value (e.g., "/4", "+2", "-5")
- Values persist per pad

#### Activating Controls
- **Tap a control pad** → toggle that control on/off
- **Bright when active**, dim when inactive
- Multiple pads in same group = last pressed wins
- Controls affect currently playing notes/sequences

### Visual Design

```
x16              x17
y7 [CLK: /4 ]    [OCT: +2]  <- Top group
y6 [CLK: /8 ]    [OCT: +1]
y5 [CLK: /16]    [OCT:  0]
y4 [CLK: /32]    [OCT: -1]
y3 [TRN: +5 ]    [CLK: /2]  <- Bottom group
y2 [TRN: +3 ]    [CLK: /4]
y1 [TRN:  0 ]    [CLK: /8]
y0 [TRN: -7 ]    [CLK:/16]
```

### Color Coding
- **Clock Divider**: Orange/Amber tones
- **Octave**: Blue tones
- **Transpose**: Cyan tones
- **Scenes**: Purple/Magenta tones
- **Active pad**: Full brightness
- **Inactive pad**: 25% brightness
- **Group switcher pads** (y7, y3): White when held

## Implementation Plan

### Phase 1: Core Infrastructure
1. Create `SequencerControlColumn` class
   - Manages 4 pads
   - Stores control type and values
   - Handles rendering and interaction
2. Create `SequencerControlState` class
   - Manages 4 control groups (2 per column)
   - Serialization to/from XML
3. Integrate with `SequencerMode` base class
   - Add control column rendering
   - Add pad/encoder delegation
4. Implement Clock Divider, Octave, Transpose

### Phase 2: Scenes
1. Add scene capture/recall
2. Scene interpolation (morph between scenes)

### Phase 3: Polish
1. Animation for active controls
2. Visual feedback for transitions
3. Performance optimizations

## Architecture

```
SequencerMode
├── SequencerControlState
│   ├── topLeftGroup (SequencerControlGroup)
│   ├── bottomLeftGroup (SequencerControlGroup)
│   ├── topRightGroup (SequencerControlGroup)
│   └── bottomRightGroup (SequencerControlGroup)
```

```cpp
class SequencerControlGroup {
    ControlType type_;           // CLOCK_DIV, OCTAVE, TRANSPOSE, SCENE
    std::array<int32_t, 4> values_; // Values for 4 pads
    int32_t activePad_ = -1;     // Which pad is active (-1 = none)
    
    void render(RGB image[][kDisplayWidth + kSideBarWidth], int32_t x, int32_t yStart);
    void handlePad(int32_t yLocal, int32_t velocity); // yLocal = 0-3
    void handleEncoder(int32_t yLocal, int32_t offset); // Adjust value
    void cycleType(int32_t offset); // Change control type
};
```

## Benefits
- **Flexible**: 4 independent control groups
- **Performative**: Quick parameter changes without menu diving
- **Intuitive**: Similar to keyboard control columns
- **Powerful**: Combine multiple control types simultaneously
- **Extensible**: Easy to add new control types

## Example Use Cases
1. **Live Performance**: x14 top = clock divisions for rhythmic variation, x14 bottom = octave shifts, x15 = transpose for key changes
2. **Composition**: x14 = scenes for different pattern variations, x15 = clock + octave for groove control
3. **Experimentation**: All 4 groups = different octave/transpose combinations for instant harmonic exploration

