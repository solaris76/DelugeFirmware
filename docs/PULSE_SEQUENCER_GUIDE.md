# Pulse Sequencer Mode - User Guide

## Overview

The Pulse Sequencer is an alternative sequencer mode for melodic instruments (Synth/MIDI/CV tracks) that offers pattern-based, non-linear sequencing. Unlike the traditional piano roll, the Pulse Sequencer uses 8 stages arranged horizontally across the grid, where each stage can have multiple pulses, different gate types, and unique note configurations.

## Accessing the Pulse Sequencer

1. Open an instrument clip (Synth, MIDI, or CV)
2. Hold **CLIP** button + turn **SELECT** encoder
3. Select "PULSE SEQ" mode

## Grid Layout

The Deluge's 16×8 grid is divided into two main sections:

### Left Side (x0-7): Stage Programming
**Scrollable** - Use vertical encoder to scroll up/down

### Right Side (x8-15): Global Controls
**Fixed** - Always visible, don't scroll

---

## Left Side Controls (x0-7) - Stage Programming

Each of the 8 columns (x0-7) represents one stage in the sequence.

### Y-Axis Layout (from bottom to top):

#### Pulse Count Rows (y0-y3, below gate line)
- **What it does**: Sets how many pulses/subdivisions each stage has (1-8)
- **How to use**: Press any pad in this area to set pulse count for that stage
- **Visual**: Cyan-to-purple gradient showing active pulses
- **Example**: Stage 1 with 3 pulses = that stage will last for 3 clock divisions

#### Gate Line (y4 by default)
- **What it does**: Controls gate type for each stage
- **Gate types**:
  - **GREY** = OFF (stage is silent)
  - **GREEN** = SINGLE (plays note only on first pulse)
  - **BLUE** = MULTIPLE (plays note on every pulse)
  - **MAGENTA** = HELD (plays note once, holds for entire stage duration at 95% length)
- **How to use**: Press pad to cycle through gate types: OFF → SINGLE → MULTIPLE → HELD → OFF
- **Playback indicator**: Current stage shows **BRIGHT RED**

#### Octave Down (y5, gate + 1)
- **What it does**: Shifts stage pitch down by octaves
- **Colors**:
  - **WHITE** = no octave shift (default)
  - **DIM ORANGE** = octave up is active (this is inactive)
  - **BRIGHT ORANGE** = octave down active (intensity shows -1 or -2)
- **How to use**: Press to toggle octave down for that stage
- **Range**: -2 to 0 octaves

#### Octave Up (y6, gate + 2)
- **What it does**: Shifts stage pitch up by octaves
- **Colors**:
  - **WHITE** = no octave shift (default)
  - **DIM ORANGE** = octave down is active (this is inactive)
  - **BRIGHT ORANGE** = octave up active (intensity shows +1, +2, or +3)
- **How to use**: Press to toggle octave up for that stage
- **Range**: 0 to +3 octaves

#### Note Pads (y7+, gate + 3 and above)
- **What it does**: Select which note from the current scale plays for each stage
- **Colors**:
  - **BRIGHT YELLOW** = selected note for this stage
  - **BLACK** = unselected notes
  - **BRIGHT RED** = currently playing note (playback indicator)
- **How to use**: Press a note pad to assign that scale degree to a stage
- **Note**: Number of rows depends on current scale (7 notes for major, 12 for chromatic)

---

## Right Side Controls (x8-15) - Global Settings

### Clock Divider (y0, x8-15) - RED
Controls the tempo/speed of the sequencer:
- **x8** = ×2 (32nd notes - twice as fast)
- **x9** = ×1 (16th notes - **DEFAULT, bright red**)
- **x10** = ÷2 (8th notes)
- **x11** = ÷4 (quarter notes)
- **x12** = ÷8
- **x13** = ÷16
- **x14** = ÷32
- **x15** = ÷64 (very slow)

**Visual**: Selected divider is bright red, others are dim red

### Play Order Presets (y1, x8-15) - CYAN
Controls how stages advance:
- **x8** = FORWARDS (1→2→3→4→5→6→7→8→1)
- **x9** = BACKWARDS (8→7→6→5→4→3→2→1→8)
- **x10** = PING PONG (1→2→3→4→5→6→7→8→7→6→5→4→3→2→1)
- **x11** = RANDOM (random enabled stage each time)
- **x12** = PEDAL (1,2,1,3,1,4,1,5,1,6,1,7,1,8)
- **x13** = SKIP 2 (1,3,5,7,2,4,6,8)
- **x14** = PENDULUM (1,2,3,2,3,4,3,4,5,4,5,6,5,6,7,6,7,8)
- **x15** = SPIRAL (1,8,2,7,3,6,4,5)

**Visual**: Selected order is bright cyan, others are dim cyan

### Gate Length (y2, x8-15) - GREEN
Controls note duration as percentage of clock period (per stage):
- Cycles through: **10%, 25%, 50%, 75%, 90%, 100%**
- **Visual**: Green gradient (brighter = longer gate)
- **Default**: 50%

### Stage Enable/Disable (y3, x8-15) - ORANGE
Toggle individual stages on/off:
- **BRIGHT ORANGE** = stage enabled
- **BLACK** = stage disabled (skipped during playback)
- Disabled stages show all controls dimmed on left side

### Stage Count (y4, x8-15) - YELLOW
Sets total number of active stages:
- **x8** = 1 stage
- **x9** = 2 stages
- **x10** = 3 stages
- ...
- **x15** = 8 stages (default)

**Visual**: Bright yellow for active stage count, black for unused

### Velocity Spread (y5, x8-15) - CYAN
Randomizes velocity for each note (per stage):
- Cycles through: **0, 20, 40, 60, 80, 100, 127**
- **0** = no randomization (default)
- **127** = maximum variation
- **Visual**: Cyan gradient (brighter = more spread)

### Probability (y6, x8-15) - BLUE
Chance each note will play (per stage):
- Cycles through: **100%, 80%, 60%, 40%, 20%**
- **100%** = always plays (default)
- **20%** = rarely plays (1 in 5)
- **Visual**: Blue gradient (brighter = higher probability)

### Control Buttons (y7, x8-15) - Various Colors
- **x8 (Purple)** = RESET ALL - Resets entire sequencer to defaults
- **x9 (Magenta)** = RANDOMIZE - Randomly generates new pattern (all stages)
- **x10 (Cyan)** = EVOLVE - Subtly modifies current pattern (1-4 random changes)
- **x11 (Blue)** = RESET PERF - Resets velocity spread, probability, gate length to defaults
- **x12/x13 (Orange)** = TRANSPOSE ↓/↑ - Shift all notes down/up (±12 semitones)
- **x14/x15 (Magenta)** = OCTAVE ↓/↑ - Shift all notes down/up by octaves (±3 octaves)

---

## Workflow Examples

### Example 1: Simple 4-Step Sequence
1. Set stage count to 4 (press y4, x11)
2. Set all 4 stages to SINGLE gate (press each gate pad until green)
3. Select notes for each stage by pressing note pads above
4. Press PLAY to hear it

### Example 2: Euclidean-Style Rhythm
1. Stage 1: 3 pulses, MULTIPLE gate
2. Stage 2: 2 pulses, SINGLE gate
3. Stage 3: 3 pulses, MULTIPLE gate
4. Stage 4: 2 pulses, SINGLE gate
5. Result: 3+2+3+2 = 10-step pattern with varied density

### Example 3: Generative Pattern
1. Press RANDOMIZE (y7, x9) to generate random pattern
2. Press EVOLVE (y7, x10) repeatedly to gradually change it
3. Adjust probability on stages to make some notes sparse
4. Use RANDOM play order for unpredictable variations

### Example 4: Bass Line with Held Notes
1. Set stages to HELD gate type
2. Select root notes and fifths from scale
3. Set stage count to 4-6
4. Use slower clock divider (÷4 or ÷8)
5. Result: Smooth, sustained bass line

### Example 5: Fast Arpeggios
1. Set all stages to MULTIPLE gate
2. Give each stage 2-4 pulses
3. Select ascending scale notes
4. Use ×2 clock multiplier for 32nd notes
5. Add velocity spread for humanization

---

## Tips & Tricks

### Visual Feedback
- **Red indicators** show current playback position on both gate line and note pad
- **Dimmed pads** indicate disabled stages
- **Popup messages** confirm all parameter changes

### Scrolling
- Use **vertical encoder** (◀▶ buttons) to scroll the left side up/down
- Useful for accessing higher notes in the scale
- Gate line, pulse counts, octave controls, and notes all scroll together

### Performance Controls
- **Transpose** and **Octave** controls affect ALL stages globally
- Great for live performance without changing programmed patterns
- Use RESET PERF to quickly return spread/probability/gate to defaults

### Clock Dividers
- **Faster** (×2, ×1): Good for melodies, arpeggios, hi-hats
- **Medium** (÷2, ÷4): Good for bass lines, chords
- **Slower** (÷8 to ÷64): Good for pads, textures, evolving drones

### Gate Types Strategy
- **SINGLE**: Clean, staccato hits
- **MULTIPLE**: Rolls, fast repeats (use with pulse count)
- **HELD**: Pads, bass notes, sustained sounds
- **OFF**: Create rests, syncopation, breathing room

### Probability & Velocity
- Low probability (20-40%) creates sparse, glitchy patterns
- Velocity spread adds humanization and dynamics
- Combine both for realistic, organic sequences

### Play Orders
- **FORWARDS/BACKWARDS**: Predictable, musical
- **PING PONG**: Good for melodies that build and release
- **RANDOM**: Experimental, generative
- **PEDAL/PENDULUM**: Creates interesting repeating motifs
- **SKIP 2/SPIRAL**: Unusual rhythmic patterns

---

## Technical Details

### Pattern Length Calculation
Total pattern length = sum of all active stage pulse counts

Example:
- Stage 1: 4 pulses
- Stage 2: 2 pulses  
- Stage 3: 3 pulses
- Stage 4: 1 pulse
- **Total**: 10 pulses before pattern loops

### Note Calculation
For each stage:
1. Start with selected note from scale
2. Add global transpose (±12 semitones)
3. Add stage octave offset (±2 to ±3 octaves)
4. Add global octave offset (±3 octaves)
5. Clamp to MIDI range (0-127)

### Scale Integration
- Automatically uses current song scale and root note
- Chromatic mode: Shows all 12 notes
- Scale mode: Shows only notes in active scale
- Note pads update automatically when scale changes

---

## Keyboard Shortcuts

- **CLIP** + SELECT encoder = Switch sequencer modes
- **Vertical encoder** (◀▶) = Scroll left side view up/down
- **PLAY** = Start/stop playback
- **SHIFT** + CLIP = Return to automation view (standard Deluge behavior)

---

## Default Settings

When first entering Pulse Sequencer mode:

**Stages 1-4:**
- Gate Type: SINGLE (green)
- Note: Ascending scale degrees (0, 1, 2, 3)
- Octave: 0
- Pulse Count: 1
- Velocity Spread: 0
- Probability: 100%
- Gate Length: 50%

**Stages 5-8:**
- Gate Type: OFF (grey)
- All other parameters: same as 1-4

**Global Settings:**
- Clock Divider: ×1 (16th notes)
- Play Order: FORWARDS
- Stage Count: 8
- All stages enabled
- Transpose: 0
- Octave: 0

---

## Troubleshooting

**Q: I don't hear any sound**
- Check that at least one stage has a gate type other than OFF (green, blue, or magenta)
- Verify stage is enabled (check y3 is orange, not black)
- Make sure stage count includes the stages you want to hear
- Check clip playback is active (PLAY button pressed)

**Q: Pattern sounds too fast/slow**
- Adjust clock divider (y0, x8-15)
- Default ×1 (16th notes) is at x9
- Try ÷4 (x11) for slower, ÷2 (x10) for medium, or ×2 (x8) for faster

**Q: Notes are out of key**
- Pulse Sequencer uses the song's current scale
- Change scale using SCALE button in song view
- Notes will automatically update to new scale

**Q: Red indicator doesn't show during playback**
- Red indicator only appears during active playback
- Indicator shows on both gate line AND selected note pad
- Refreshes continuously, even with slow clock dividers

**Q: Can't see higher notes**
- Use vertical encoder (◀▶ buttons) to scroll view up
- Gate line will move down, revealing more note rows above

**Q: Pattern length changed unexpectedly**
- Pattern length = sum of pulse counts for active stages
- Changing pulse count or stage count affects total length
- Check y4 for active stage count

---

## Creative Ideas

### 1. Polyrhythmic Sequences
- Set different pulse counts per stage (3, 5, 7, 2)
- Use FORWARDS play order
- Pattern will evolve over multiple loops

### 2. Generative Melodies
- Set medium probability (60-80%) on all stages
- Use RANDOM play order
- Add velocity spread for variation
- Result: Never-repeating melodies

### 3. Call and Response
- Stages 1-4: High notes, SINGLE gates, 1 pulse each
- Stages 5-8: Low notes, HELD gates, 2-3 pulses each
- Use PEDAL play order for alternating pattern

### 4. Techno Bass Line
- All stages SINGLE gate, 1 pulse
- Root and fifth notes only
- Add one or two octave-down stages
- Set probability to 80% on some stages for variation
- Use ÷4 or ÷8 clock divider

### 5. Evolving Pads
- All stages HELD gate with 4-6 pulses
- Various octave offsets (-1, 0, +1)
- Use ÷16 or ÷32 clock divider for slow movement
- Press EVOLVE occasionally for gradual change

### 6. Glitch Sequences
- Mix SINGLE and MULTIPLE gates randomly
- Low probability (20-40%) on half the stages
- High velocity spread (80-127)
- RANDOM or SPIRAL play order
- Fast clock (×2 or ×1)

---

## Advanced Techniques

### Combining Parameters
- **Pulse Count + MULTIPLE gate** = fast rolls/trills
- **Pulse Count + HELD gate** = sustained chords
- **Low Probability + High Velocity Spread** = realistic drums/perc
- **PING PONG order + Ascending notes** = melodic waves

### Live Performance
- Use **Transpose** (y7, x12/x13) to shift key without reprogramming
- Use **Octave** (y7, x14/x15) for dramatic pitch shifts
- Toggle stage enable/disable (y3) to mute/unmute sections
- Change play order mid-performance for instant variation

### Saving Patterns
The Pulse Sequencer state is saved with your clip when you save the song. All stage settings, performance controls, and play order are preserved.

---

## Limitations

- **Melodic instruments only**: Not available for kits or audio clips
- **8 stages maximum**: Designed for compact, memorable patterns
- **No per-step automation**: Use standard piano roll mode for detailed automation
- **Non-destructive**: Pattern lives in RAM, doesn't modify the underlying note data until you record it

---

## Color Reference Guide

| Element | Color | Meaning |
|---------|-------|---------|
| **Pulse Counts** | Cyan-Purple gradient | Active pulses per stage |
| **Gate OFF** | Grey | Stage silent |
| **Gate SINGLE** | Green | Note on first pulse only |
| **Gate MULTIPLE** | Blue | Note on every pulse |
| **Gate HELD** | Magenta | Sustained note |
| **Octave Controls** | White/Orange | Pitch offset (white=default) |
| **Note Selected** | Bright Yellow | Note assigned to stage |
| **Playback Position** | Bright Red | Current stage (gate + note) |
| **Clock Divider** | Red | Tempo control |
| **Play Order** | Cyan | Sequence advancement |
| **Gate Length** | Green gradient | Note duration |
| **Stage Enable** | Orange | Stage on/off |
| **Stage Count** | Yellow | Active stages |
| **Velocity Spread** | Cyan gradient | Randomization |
| **Probability** | Blue gradient | Play chance |
| **Reset** | Purple | Reset all |
| **Randomize** | Magenta | Generate pattern |
| **Evolve** | Cyan | Mutate pattern |
| **Reset Perf** | Blue | Reset performance params |
| **Transpose** | Orange | Key shift |
| **Octave** | Magenta | Octave shift |

---

## Version History

**Current Version**: Initial implementation
- 8-stage pulse sequencer
- 4 gate types with visual feedback
- 8 play order patterns
- Clock divider with 8 speeds (×2 to ÷64)
- Per-stage controls: note, octave, pulse count, gate length, velocity spread, probability
- Global controls: transpose, octave, play order, stage count
- Pattern generation: randomize, evolve
- Scale-aware note selection
- Continuous playback tracking with red indicators

---

## Credits

Part of the Deluge Sequencer Modes project - adding alternative pattern-based sequencing to the Synthstrom Audible Deluge firmware.

For more information, updates, and community discussion, visit the Deluge community forums.

