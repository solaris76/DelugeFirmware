# Sequencer Modes User Guide

Alternative sequencing modes for the Synthstrom Deluge that transform how you create patterns.

---

## Quick Start

### Accessing Sequencer Modes

1. Create or open a **Synth, MIDI, or CV clip**
2. Press **SHIFT + CLIP** to open the Clip menu
3. Select **"CLIP TYPE"**
4. Choose your sequencer mode:
   - **PIANO ROLL** - Traditional note view (default)
   - **STEP SEQ** - 16-step analog-style sequencer
   - **PULSE SEQ** - 8-stage euclidean/polyrhythmic sequencer

The pads will immediately change to show your selected sequencer!

### Switching Back to Piano Roll

1. Press **SHIFT + CLIP**
2. Select **"CLIP TYPE"**
3. Choose **"PIANO ROLL"**

Your sequencer pattern is preserved - you can switch back anytime!

---

## Step Sequencer

Classic 16-step sequencer inspired by analog hardware like the SH-101.

### Layout (Main Grid)

Each **column (x0-x15)** is one step (1-16):

**Per Column:**
- **y0** - Gate (OFF/ON/SKIP)
- **y1** - Octave Down (blue)
- **y2** - Octave Up (blue)
- **y3-y7** - Note selection (5 visible notes from current scale)

### How to Use

**Programming Steps:**
1. **Set gate** - Tap y0 to cycle: ON → OFF → SKIP
   - ON = play note
   - OFF = rest (step counts)
   - SKIP = jump over step
2. **Choose note** - Tap y3-y7 pads (bottom = lowest note)
3. **Adjust octave** - Tap y1 (down) or y2 (up)

**Scrolling Notes:**
- **Vertical encoder** - scroll through more scale notes
- Shows 5 notes at a time, scroll to access full scale

**Playback:**
- Steps advance every **16th note**
- Respects CLOCK DIV control (see Control Pads below)
- Respects DIRECTION control (forward/backward/ping-pong/random)

### Tips

- **Melodic patterns**: Use in-scale mode for instant melodies
- **Chromatic**: Works great in chromatic mode too
- **Live tweaking**: Change notes during playback
- **Generative**: Use RANDOM and EVOLVE controls (see Control Pads)

---

## Pulse Sequencer

Euclidean/polyrhythmic sequencer with 8 stages - perfect for complex rhythms and generative patterns.

### Layout (Main Grid)

**8 Columns (x0-x7)** - One stage per column

**Per Column (from bottom up):**
- **y0-y2** - Gate type (OFF/SINGLE/MULTIPLE/HELD)
- **y3-y4** - Pulse count (1-8 pulses per stage)
- **y5-y7** - Note selection

**Right Side Performance Controls:**
- **x8, y0-y3** - Number of active stages (1-8)
- **x8, y4-y7** - Play order (FORWARD/BACKWARD/PING-PONG/RANDOM/etc.)
- **x9, y0-y3** - Clock divider
- **x10-x15, y0-y7** - Stage enable/disable toggles

### How to Use

**Programming a Stage:**
1. **Set gate** - Tap y0-y2 area to cycle gate types:
   - OFF = silent
   - SINGLE = one note
   - MULTIPLE = euclidean pattern (based on pulse count)
   - HELD = sustained note
2. **Set pulse count** - Tap y3-y4 area (1-8 pulses)
3. **Choose note** - Tap y5-y7 area
4. **Adjust octave** - While holding stage, tap OCTAVE control pad

**Performance Controls:**
- **Number of stages** - Tap x8 (y0-y3) to set 1-8 active stages
- **Play order** - Tap x8 (y4-y7) to change sequence order
- **Clock divider** - Tap x9 to change timing
- **Toggle stages** - Tap x10-x15 to enable/disable individual stages

**Vertical Encoder:**
- Scroll the view up/down to see different parameters

### Tips

- **Euclidean rhythms**: Set MULTIPLE gate + pulse count (e.g., 3 pulses in 8 steps = 3-3-2)
- **Polyrhythms**: Different pulse counts per stage creates complex patterns
- **Live remixing**: Change play order and stage count during performance
- **Generative**: Combine with RANDOM/EVOLVE controls

---

## Control Pads (Sidebar Columns)

The **right two columns (x16-x17)** provide performance controls and scene management.

### How Control Pads Work

**Each pad (16 total)** can be configured with different functions:

**To Configure a Pad:**
1. Hold a pad on x16 or x17
2. Turn **horizontal encoder** to cycle through control types
3. Turn **vertical encoder** to adjust the value

**Pad Modes:**
- **TOGGLE** - Tap once to activate, tap again to deactivate
- **MOMENTARY** - Active only while held
- Press **vertical encoder button** while holding pad to toggle mode

### Available Control Types

**Performance:**
- **CLOCK DIV** - Change playback speed (1/4x to 32x)
- **OCTAVE** - Shift all notes up/down by octaves
- **TRANSPOSE** - Transpose pattern (semitones)
- **DIRECTION** - Forward/Backward/Ping-Pong/Random (Step Seq only)

**Scenes:**
- **SCENE** (8 scenes available)
  - Normal press = recall scene
  - SAVE + press = capture current state
  - SHIFT + press = clear scene

**Generative:**
- **RESET** - Reset pattern to init state
- **RANDOM** - Randomize pattern (value = chaos %)
- **EVOLVE** - Gentle pattern mutation (value = mutation %)

### Default Layout

**Column x16 (Left):**
- y0 - OCTAVE
- y1 - TRANSPOSE
- y2-y5 - (available)
- y6-y7 - (available)

**Column x17 (Right):**
- y0-y5 - SCENE (8 scenes)
- y6 - RANDOM
- y7 - RESET

You can reconfigure any pad to any control type!

### Combining Controls

- **Multiple pads active** - Effects stack (e.g., OCTAVE +1 + TRANSPOSE +3)
- **Scenes capture everything** - Pattern state + active controls
- **Per-mode settings** - Each sequencer mode has its own control pad layout

---

## Saving & Loading Patterns

Patterns save the **active view** - think of them like scene snapshots.

### Saving a Pattern

1. Program your sequencer pattern
2. Hold **SAVE** button
3. Press **horizontal encoder** (the ◀▶ knob)
4. Name your pattern
5. Press **SAVE** or **select encoder** to confirm

**Where Patterns Are Saved:**
- Piano Roll → `PATTERNS/MELODIC/`
- Step Sequencer → `PATTERNS/MELODIC/SEQUENCER/STEP/`
- Pulse Sequencer → `PATTERNS/MELODIC/SEQUENCER/PULSE/`

### Loading a Pattern

1. Be in the sequencer mode you want to load (Step/Pulse/Piano)
2. Hold **LOAD** button
3. Press **horizontal encoder**
4. Browse patterns (automatically shows correct folder)
5. Press **LOAD** or **select encoder** to load

**What Gets Loaded:**
- All sequencer steps/stages
- Control pad configurations
- Scene data (if saved)
- Piano roll notes (if any)

### Quick Pattern Switching

Hold **LOAD + horizontal encoder** and scroll through patterns - they preview in real-time!

---

## Saving Songs

Songs save **everything** - all sequencer modes plus piano roll.

**Standard SAVE workflow:**
1. Press **SAVE** button (short press)
2. Name your song
3. Save

**What's Saved:**
- Current sequencer mode and all its data
- Piano roll notes
- Control pad configurations
- All 8 scenes
- All standard clip settings

When you load a song, your sequencer mode is automatically activated with all settings restored!

---

## Tips & Tricks

### Step Sequencer

**Instant Melodies:**
1. Set clip to in-scale mode (SHIFT + SCALE)
2. Program random note pads (y3-y7)
3. Add octave variation (y1/y2)
4. Use EVOLVE control to generate variations

**Rhythm Patterns:**
1. Use SKIP gates to create swing
2. Combine with DIRECTION controls for variations
3. Try RANDOM direction for generative rhythms

### Pulse Sequencer

**Euclidean Rhythms:**
1. Set stage to MULTIPLE gate
2. Set pulse count (e.g., 3, 5, 7)
3. Adjust number of stages (e.g., 8, 16)
4. Result: automatic euclidean distribution

**Polyrhythms:**
1. Stage 1: 3 pulses
2. Stage 2: 5 pulses
3. Stage 3: 7 pulses
4. Play order: FORWARDS = layered polyrhythm

**Generative Patterns:**
1. Program basic pattern
2. Set RANDOM play order
3. Use stage toggles (x10-x15) to mute/unmute
4. Add OCTAVE/TRANSPOSE controls for variation

### Control Pads

**Performance Setups:**
- Map CLOCK DIV to different values (1/2x, 1x, 2x, 4x) on different pads
- Tap between them for instant tempo changes
- Combine with OCTAVE pads for key changes

**Scene Workflow:**
1. Program base pattern
2. SAVE + scene pad 1 = capture
3. Tweak pattern (change notes, add OCTAVE control, etc.)
4. SAVE + scene pad 2 = capture variation
5. Tap scene pads during performance to switch

**Generative Sessions:**
1. Start with simple pattern
2. Set up RANDOM and EVOLVE pads
3. Tap EVOLVE repeatedly for gentle mutations
4. Tap RANDOM for dramatic changes
5. When you like it, SAVE + scene pad to capture

---

## Workflow Examples

### Building a Track

1. **Create bass line** (Step Sequencer)
   - 16 steps, simple root notes
   - Save pattern: "BASS_01"
   
2. **Create lead** (Pulse Sequencer)
   - 3-4 stages, MULTIPLE gates
   - Different pulse counts for euclidean feel
   - Save pattern: "LEAD_01"

3. **Layer and perform**
   - Load patterns into different clips
   - Use control pads for variations
   - Save song when ready

### Live Performance

1. Load base patterns
2. Use SCENE pads for pattern variations
3. Use CLOCK DIV for tempo changes
4. Use TRANSPOSE for key changes
5. EVOLVE patterns during drops
6. Everything stays in sync!

---

## Keyboard Shortcuts

**Pattern Save/Load:**
- **SAVE + horizontal encoder** - Save pattern
- **LOAD + horizontal encoder** - Load pattern

**Mode Switching:**
- **SHIFT + CLIP** → CLIP TYPE → choose mode

**Control Pads:**
- **Hold pad + horizontal encoder** - change control type
- **Hold pad + vertical encoder** - adjust value
- **Hold pad + vertical encoder button** - toggle momentary/toggle mode

**Scenes:**
- **SAVE + scene pad** - capture scene
- **Tap scene pad** - recall scene
- **SHIFT + scene pad** - clear scene

**Generative:**
- **Tap RESET** - reset to init pattern
- **Tap RANDOM** - randomize (value = chaos %)
- **Tap EVOLVE** - mutate pattern (value = mutation %)

---

## Troubleshooting

**Pattern won't load:**
- Check you're in the correct sequencer mode
- Step patterns only load in Step Sequencer
- Pulse patterns only load in Pulse Sequencer

**Can't find saved patterns:**
- Load UI automatically shows correct folder
- Step patterns: `PATTERNS/MELODIC/SEQUENCER/STEP/`
- Pulse patterns: `PATTERNS/MELODIC/SEQUENCER/PULSE/`

**Pads not responding:**
- Step Sequencer: x0-x15 are steps, x16-x17 are controls
- Pulse Sequencer: x0-x7 are stages, x8-x15 are performance controls, x16-x17 are control pads

**Scale notes don't match:**
- Sequencers respect clip's scale mode
- Change scale: SHIFT + SCALE button
- Vertical encoder scrolls through scale notes

---

## Credits

Deluge Sequencer Modes - Community Firmware Extension  
Part of the Deluge Community Firmware Project

For more info, documentation, and updates:
- https://github.com/SynthstromAudible/DelugeFirmware

---

**Version:** Alpha 0.6  
**Last Updated:** October 2025

