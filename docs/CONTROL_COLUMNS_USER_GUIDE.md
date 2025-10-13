# Control Columns User Guide

## Overview

The Control Columns system gives you 16 individually configurable pads on the right side of your Deluge (columns x16 and x17) that provide real-time control over your sequencer modes. Each pad can be assigned to any control type, giving you complete flexibility to customize your workflow.

---

## Physical Layout

```
Column x16 (left)    Column x17 (right)
┌──────────┐        ┌──────────┐
│  Pad 7   │        │  Pad 15  │  ← y7 (top)
├──────────┤        ├──────────┤
│  Pad 6   │        │  Pad 14  │  ← y6
├──────────┤        ├──────────┤
│  Pad 5   │        │  Pad 13  │  ← y5
├──────────┤        ├──────────┤
│  Pad 4   │        │  Pad 12  │  ← y4
├──────────┤        ├──────────┤
│  Pad 3   │        │  Pad 11  │  ← y3
├──────────┤        ├──────────┤
│  Pad 2   │        │  Pad 10  │  ← y2
├──────────┤        ├──────────┤
│  Pad 1   │        │  Pad 9   │  ← y1
├──────────┤        ├──────────┤
│  Pad 0   │        │  Pad 8   │  ← y0 (bottom)
└──────────┘        └──────────┘
```

---

## Default Configuration

Out of the box, the pads are configured as follows:

### Column x16 (Pads 0-7)
- **Pads 0-3** (bottom): OCTAVE controls (0, -1, +1, +2)
- **Pads 4-7** (top): CLOCK controls (/1, /2, /4, /16)

### Column x17 (Pads 8-15)
- **Pad 8** (y0): SCENE 1
- **Pad 9** (y1): SCENE 2
- **Pads 10-14** (y2-y6): NONE (empty - configure as needed)
- **Pad 15** (y7): RESET
- **Pad 7** (y7 x16): RANDOM

---

## Basic Operations

### Using a Control Pad

**TOGGLE Mode** (default for most controls):
- Press once to activate
- Press again to deactivate
- Pad stays bright when active

**MOMENTARY Mode**:
- Hold to activate
- Release to deactivate
- Pad only bright while held

### Configuring a Pad

1. **Change Control Type**:
   - Hold the pad you want to configure
   - Rotate the ⟲ **horizontal encoder**
   - Cycles through all available control types
   - OLED shows the new type name

2. **Change Control Value**:
   - Hold the pad
   - Rotate the ⟲ **vertical encoder**
   - Adjusts the value for that control type
   - OLED shows type and current value (e.g., "CLOCK: /4")

3. **Toggle Mode** (for applicable controls):
   - Hold the pad
   - Press the ⟲ **vertical encoder button**
   - Switches between TOGGLE and MOMENTARY modes
   - OLED shows the new mode

---

## Control Types

### 🔴 CLOCK (Clock Divider)
**Color**: Red  
**Values**: *2, /1, /2, /3, /4 ... /64  
**Purpose**: Changes the playback speed of your sequencer

- Negative values multiply (*2 = double speed)
- Positive values divide (/4 = quarter speed)
- Applies to the entire sequence playback rate
- Example: /4 with 120 BPM = effectively 30 BPM
- **Mode Support**: TOGGLE or MOMENTARY

### 🟠 OCTAVE (Octave Shift)
**Color**: Orange  
**Values**: -5 to +5 octaves  
**Purpose**: Shifts all notes up or down by octaves

- Transposes all notes by 12 semitones per octave
- Multiple octave pads can be active (they stack additively)
- Great for quick range changes during performance
- **Mode Support**: TOGGLE or MOMENTARY

### 🟡 TRANSPOSE (Semitone Shift)
**Color**: Yellow  
**Values**: -12 to +12 semitones  
**Purpose**: Transposes notes within the current scale

- Keeps notes in scale (not chromatic)
- Multiple transpose pads can be active (they stack)
- Example: +2 in C Major moves C→D, D→E, etc.
- **Mode Support**: TOGGLE or MOMENTARY

### 🔵 SCENE (Scene Recall/Capture)
**Color**: Blue  
**Values**: Scene slots 1-4  
**Purpose**: Store and recall complete sequencer states

**Operations**:
- **Press pad** = Recall scene (if captured)
- **SAVE + pad** = Capture current state to this scene
- **SHIFT + pad** = Clear this scene

**What's Stored**:
- All sequencer data (notes, gates, timing)
- Control column settings (other pads' configs)
- Octave/Transpose/Clock states
- Everything except other scene pads

**Visual Feedback**:
- Bright blue = scene is active
- Dim blue = scene has data
- Very dim blue = empty scene
- **Mode**: Always TOGGLE

### 🩵 DIRECTION (Playback Direction)
**Color**: Cyan  
**Values**: FWD, BACK, PING, RAND  
**Purpose**: Changes how the sequencer steps through notes

- **FWD** = Forward (normal)
- **BACK** = Backward (reverse)
- **PING** = Ping-pong (bounce back and forth)
- **RAND** = Random step selection
- Only one direction can be active at a time
- **Mode Support**: TOGGLE or MOMENTARY

### 🟦 RESET (Pattern Reset)
**Color**: Light Blue  
**Values**: None  
**Purpose**: Resets the pattern to its initial state

- Instant action (press = reset immediately)
- Initializes pattern to ascending scale
- Cannot be "held active" (trigger only)
- Useful for returning to a known state
- **Mode**: Always MOMENTARY (instant trigger)

### 🟣 RANDOM (Randomize)
**Color**: Light Magenta  
**Values**: 10% to 100% mutation rate  
**Purpose**: Randomizes the entire pattern

- Randomizes gates, notes, and octaves
- Mutation rate controls intensity
- 100% = completely new random pattern
- 10% = subtle changes
- Instant action (press = randomize immediately)
- **Mode**: Always MOMENTARY (instant trigger)

### 🩷 EVOLVE (Light Evolution)
**Color**: Pink  
**Values**: 10% to 100% mutation rate  
**Purpose**: Evolves the pattern with adaptive behavior

- **Low % (10-70%)**: Gentle melodic drift
  - Small note changes only (-1, 0, +1 steps in scale)
  - Octaves and gates remain unchanged
  - Perfect for subtle variations
  
- **High % (>70%)**: Chaotic evolution
  - Larger note jumps (-2 to +2 steps)
  - 40% chance to also shift octaves
  - 25% chance to flip gate types
  - Creates dramatic variations

- Instant action (press = evolve immediately)
- **Mode**: Always MOMENTARY (instant trigger)

### ⚫ NONE (Disabled)
**Color**: Off/Black  
**Values**: None  
**Purpose**: Disables a pad

- Pad does nothing when pressed
- Useful for "blanking out" unused pads
- Pad remains dark/off

---

## Scene System

### What Are Scenes?

Scenes capture the complete state of your sequencer and control settings, allowing you to instantly switch between different configurations.

### Capturing a Scene

1. Set up your pattern and controls exactly how you want them
2. Hold **SAVE** button
3. Press a **SCENE** pad
4. Release both
5. OLED displays "CAPTURED"
6. Scene pad now glows dimly (indicating it has data)

### Recalling a Scene

1. Simply press a **SCENE** pad that has been captured
2. Everything instantly switches to that saved state
3. Scene pad glows brightly (indicating it's active)
4. OLED shows scene number

### Clearing a Scene

1. Hold **SHIFT** button
2. Press the **SCENE** pad you want to clear
3. Release both
4. OLED displays "CLEARED"
5. Scene pad becomes very dim (empty)

### What Scenes Store

**Sequencer Data**:
- All notes and their positions
- Gate states (on/off/accent)
- Step timing
- Pattern length
- Octave offsets per note

**Control Settings**:
- Which pads are configured to what types
- Current values for each pad
- Toggle/Momentary mode settings
- Active state of each pad

**Note**: Scenes do NOT store other scene pads' data (prevents recursion).

---

## Usage Examples

### Example 1: Performance Setup

Configure pads for live performance:

```
x16:                    x17:
Pad 7: CLOCK /1        Pad 15: RESET
Pad 6: CLOCK /2        Pad 14: RANDOM 50%
Pad 5: CLOCK /4        Pad 13: EVOLVE 70%
Pad 4: CLOCK /8        Pad 12: EVOLVE 30%
Pad 3: OCTAVE +2       Pad 11: SCENE 4
Pad 2: OCTAVE +1       Pad 10: SCENE 3
Pad 1: OCTAVE 0        Pad 9:  SCENE 2
Pad 0: OCTAVE -1       Pad 8:  SCENE 1
```

**Workflow**:
- Left column: Quick clock changes and octave shifts
- Right top: Generative mutations for variation
- Right bottom: Scene switching for song sections

### Example 2: Generative Focus

Set up multiple evolution intensities:

```
x16:                    x17:
Pad 7: EVOLVE 10%      Pad 15: RESET
Pad 6: EVOLVE 30%      Pad 14: EVOLVE 90%
Pad 5: EVOLVE 50%      Pad 13: EVOLVE 100%
Pad 4: EVOLVE 80%      Pad 12: RANDOM 30%
Pad 3: RANDOM 20%      Pad 11: SCENE 4
Pad 2: RANDOM 50%      Pad 10: SCENE 3
Pad 1: RANDOM 80%      Pad 9:  SCENE 2
Pad 0: CLOCK /4        Pad 8:  SCENE 1
```

**Workflow**:
- Tap EVOLVE 10-50% for gentle melodic drift
- Tap EVOLVE 80-100% for chaotic evolution (notes + octaves + gates)
- RANDOM pads for complete randomization at different intensities
- RESET to return to start

### Example 3: Transpose/Octave Control

Focus on melodic variation:

```
x16:                    x17:
Pad 7: TRANSPOSE +12   Pad 15: TRANSPOSE -7
Pad 6: TRANSPOSE +7    Pad 14: TRANSPOSE -5
Pad 5: TRANSPOSE +5    Pad 13: TRANSPOSE -3
Pad 4: TRANSPOSE +3    Pad 12: OCTAVE +2
Pad 3: OCTAVE +1       Pad 11: SCENE 4
Pad 2: OCTAVE 0        Pad 10: SCENE 3
Pad 1: OCTAVE -1       Pad 9:  SCENE 2
Pad 0: OCTAVE -2       Pad 8:  SCENE 1
```

**Workflow**:
- Multiple transpose options for key changes
- Octave shifts for range exploration
- Combine multiple pads for complex shifts
- Use TOGGLE mode to layer multiple shifts

---

## Advanced Techniques

### Stacking Effects

Many control types can be active simultaneously:

**Octave Stacking**:
- Activate OCTAVE +1 and OCTAVE +2
- Total shift = +3 octaves
- All active octave pads add together

**Transpose Stacking**:
- Activate TRANSPOSE +3 and TRANSPOSE +5
- Total transpose = +8 semitones (within scale)
- All active transpose pads add together

**Note**: CLOCK and DIRECTION do not stack (last activated wins).

### Momentary Performance Tricks

Set pads to MOMENTARY mode for live manipulation:

1. **Clock Doubling**:
   - Set CLOCK *2 to MOMENTARY
   - Hold during fills for double-time energy
   - Release to return to normal tempo

2. **Octave Drops**:
   - Set OCTAVE -2 to MOMENTARY
   - Hold for bass emphasis
   - Release for return to normal range

3. **Random Variations**:
   - All generative types are MOMENTARY by default
   - Tap for instant variation
   - Rapid tapping creates controlled chaos

### Scene-Based Composition

Use scenes to structure a song:

1. **SCENE 1**: Intro (sparse pattern, /4 clock)
2. **SCENE 2**: Verse (full pattern, /2 clock)
3. **SCENE 3**: Chorus (octave up, /1 clock)
4. **SCENE 4**: Breakdown (mutated pattern, /8 clock)

Switch between scenes during playback for instant song structure.

---

## Tips & Tricks

### Workflow Tips

1. **Start Simple**: Begin with just clock and octave controls, add complexity as needed

2. **Color Code**: The color system helps you quickly identify pad types at a glance

3. **Group Related Controls**: Keep similar controls together (all clocks, all octaves, etc.)

4. **Scene Snapshots**: Capture scenes often during experimentation - you can always clear them

5. **Mutation Percentages**: 
   - 10-30% = subtle, musical variations
   - 40-60% = noticeable but controlled changes
   - 70-100% = dramatic, experimental results

### Performance Tips

1. **Pre-configure**: Set up your pads before the performance

2. **Muscle Memory**: Consistent layouts help with blind operation

3. **Scene Safety Net**: Always keep one scene with your "safe" starting pattern

4. **Generative Flow**: 
   - Start with RESET
   - Tap EVOLVE 20% a few times
   - When you like it, capture to SCENE
   - Continue evolving for more variations

5. **Clock Exploration**: Try different clock divisions to find the right groove

### Creative Ideas

1. **Random Walk**: 
   - Start with RESET
   - Tap EVOLVE 30% repeatedly
   - Let it wander naturally
   - Capture interesting results to scenes

2. **Scene Morphing**:
   - Capture 4 very different scenes
   - Switch between them rhythmically
   - Creates evolving macro-patterns

3. **Generative Cascade**:
   - RESET → EVOLVE 20% → EVOLVE 40% → EVOLVE 80% → RANDOM 30%
   - Each step adds more variation
   - Creates organic-sounding progressions from subtle to chaotic

4. **Tempo Games**:
   - Switch rapidly between /1, /2, /4, /8
   - Creates polyrhythmic effects
   - Use MOMENTARY mode for rhythm breaks

---

## Color Reference Quick Guide

| Type | Color | Example Value |
|------|-------|---------------|
| CLOCK | 🔴 Red | /4 |
| OCTAVE | 🟠 Orange | +1 |
| TRANSPOSE | 🟡 Yellow | +3 |
| SCENE | 🔵 Blue | Scene 1 |
| DIRECTION | 🩵 Cyan | FWD |
| RESET | 🟦 Light Blue | (trigger) |
| RANDOM | 🟣 Magenta | 50% |
| EVOLVE | 🩷 Pink | 30% |
| NONE | ⚫ Off/Black | (disabled) |

**Brightness Indicators**:
- **Bright** = Active/On
- **Dim** = Available (or scene has data)
- **Very Dim** = Empty (scene only) or disabled

---

## Troubleshooting

### Pad not responding?
- Check if it's set to NONE type
- Try re-configuring the pad type
- Verify you're in a sequencer clip (not Song mode)

### Scene won't capture?
- Make sure you're holding SAVE (not SHIFT)
- Verify the pad is set to SCENE type
- Check that you have an active sequencer pattern

### Weird pitch behavior?
- Check for multiple active OCTAVE or TRANSPOSE pads
- Remember they stack additively
- Use RESET to return to a known state

### Control not affecting playback?
- Some controls only work in certain sequencer modes
- Ensure the pad is bright (active)
- Try toggling the pad off and on again

### Lost your configuration?
- Scenes store pad configurations too
- Recall a scene to restore previous setup
- Or manually reconfigure pads as needed

---

## Quick Reference Card

**Basic Controls**:
- Press pad = Activate/Deactivate
- Hold pad + ⟲ horizontal = Change type
- Hold pad + ⟲ vertical = Change value
- Hold pad + ⟲ vertical button = Toggle mode

**Scene Controls**:
- SAVE + pad = Capture scene
- Press pad = Recall scene
- SHIFT + pad = Clear scene

**Default Layout**:
- x16 bottom: OCTAVE (+1), TRANSPOSE (+5)
- x16 top: (empty pads for configuration)
- x17 bottom: SCENE 1, SCENE 2
- x17 top: RESET, RANDOM

---

*Enjoy your Control Columns! Experiment, explore, and create unique performances!*

