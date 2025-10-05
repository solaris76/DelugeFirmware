# 🎵 Pulse Sequencer User Guide - Alpha 0.1

## Overview

The Pulse Sequencer is a powerful, professional-grade step sequencer built into the Deluge's keyboard system. It provides 8 independent stages, each with customizable notes, octaves, pulse counts, and gate types, plus comprehensive performance controls for live manipulation.

## 🎛️ Interface Layout

### Main Sequencer Area (x0-7)

#### **Gate Line (y4)** - Stage Control
- **8 pads** representing stages 1-8
- **Colors indicate gate types:**
  - 🔘 **Gray**: OFF (no sound)
  - 🟢 **Green**: SINGLE (one hit per stage)
  - 🔵 **Blue**: MULTIPLE (hit on each pulse)
  - 🟣 **Magenta**: HELD (sustained for entire stage)
- **Press pad**: Cycle through gate types
- **Visual feedback**: Pads flash red when stage plays

#### **Note Selection (y5)** - Melody Control
- **8 pads** for selecting notes within current scale
- **Colors:**
  - 🟣 **Magenta**: Note with accumulator applied
  - 🩷 **Pink**: Standard note
- **Press pad**: Cycle through scale notes for that stage

#### **Octave Controls (y6-7)** - Pitch Range
- **y6**: Octave down (-1)
- **y7**: Octave up (+1)
- **Range**: -2 to +3 octaves per stage
- **Color**: 🔵 Light blue
- **Press pad**: Adjust octave for that stage

#### **Pulse Count Display (y0-3)** - Rhythm Patterns
- **7 columns** showing pulse counts 1-7
- **Gradient colors**: Purple/pink to cyan
- **Visual representation**: Lit pads show active pulses
- **Press pad**: Set pulse count for that stage (1-7 pulses)

### Performance Controls (x8-15)

#### **y7: Creative Controls**
- **x8**: 🟣 **RESET** - Restore all defaults
- **x9**: 🟣 **RANDOMIZE** - Complete randomization
- **x10**: 🔵 **EVOLVE** - Subtle melodic variations
- **x12**: 🟠 **Transpose -** (within scale)
- **x13**: 🟠 **Transpose +** (within scale)
- **x14**: 🟣 **Octave -** (global)
- **x15**: 🟣 **Octave +** (global)

#### **y6: Note Probability** 🔵 (Fader Style)
- **Values**: 0%, 10%, 25%, 50%, 75%, 90%, 95%, 100%
- **100%**: All notes play (default)
- **Lower values**: Random note dropouts
- **Fader effect**: Black pads above current setting
- **Works with**: MIDI tracks (synth support limited)

#### **y5: Velocity Spread** 🔷 (Fader Style)
- **Values**: 0, 5, 10, 15, 20, 25, 30, 50
- **0**: No velocity variation
- **Higher values**: More dynamic range
- **Fader effect**: Black pads above current setting
- **Works with**: Both synth and MIDI tracks

#### **y4: Stage Count** 🟡
- **Range**: 1-8 active stages
- **Yellow pads**: Show number of active stages
- **Inactive stages**: Dimmed throughout interface

#### **y3: Stage Enable/Disable** 🟠
- **8 pads** for individual stage control
- **Bright orange**: Stage enabled
- **Dim orange**: Stage disabled (skipped entirely)
- **Live control**: Toggle stages during performance

#### **y2: Gate Control** 🟢 (Fader Style)
- **Values**: 5, 10, 15, 20, 25, 30, 40, 50
- **Low values**: Short, staccato notes
- **High values**: Long, legato notes
- **Fader effect**: Black pads above current setting
- **Works with**: Both synth and MIDI tracks

#### **y1: Play Order** 🔵
- **x8**: FORWARDS (1→2→3→4→5→6→7→8)
- **x9**: BACKWARDS (8→7→6→5→4→3→2→1)
- **x10**: PING PONG (1→2→3→4→5→6→7→8→7→6→5→4→3→2→1)
- **x11**: RANDOM (random stage each step)

## 🎵 Basic Operation

### Getting Started
1. **Select Pulse Sequencer**: Navigate to keyboard layouts, select "Pulse Sequencer"
2. **Press Reset** (🟣 y7 x8): Start with clean defaults
3. **Set gate types**: Press gate line pads to set SINGLE, MULTIPLE, or HELD
4. **Adjust notes**: Press note selection pads to choose scale notes
5. **Press Play**: Start the Deluge to hear your sequence

### Creating Your First Pattern
1. **Stage 1**: Set to SINGLE gate, choose root note
2. **Stage 2**: Set to SINGLE gate, choose 3rd of scale
3. **Stage 3**: Set to MULTIPLE gate, set 2 pulses, choose 5th of scale
4. **Stage 4**: Set to HELD gate, set 4 pulses, choose octave up root
5. **Result**: Musical 4-stage pattern with varied rhythms

## 🎛️ Advanced Features

### Stage Skipping
- **Disable stages**: Use orange toggle pads (y3) to skip stages entirely
- **Example**: Disable stage 2 → sequence goes 1→3→4→5→6→7→8→1
- **Live performance**: Build up patterns by enabling stages gradually

### Play Orders
- **FORWARDS**: Standard 1-8 progression
- **BACKWARDS**: Reverse 8-1 progression
- **PING PONG**: Bounces 1-8-7-6-5-4-3-2-1-2-3...
- **RANDOM**: Jumps to random enabled stages

### Performance Controls
- **Velocity Spread**: Add dynamics and humanization
- **Note Probability**: Create rhythmic gaps and variations
- **Gate Control**: Adjust note length from staccato to legato
- **All fader-style**: Visual feedback shows active range

### Creative Workflow
1. **RESET** 🟣: Clean slate with defaults
2. **RANDOMIZE** 🟣: Generate completely new patterns
3. **EVOLVE** 🔵: Make subtle melodic variations
4. **Manual tweaking**: Fine-tune individual elements
5. **Repeat**: Iterate to perfection

## ⚡ Timing System

### Clock Divider (Horizontal Encoder)
- **Range**: /1 to /64
- **Base**: 32nd notes (/1 = fastest)
- **Default**: /2 (16th notes)
- **Examples**:
  - **/1**: 32nd notes (very fast)
  - **/2**: 16th notes (standard)
  - **/3**: 16th triplets
  - **/4**: 8th notes
  - **/8**: Quarter notes
  - **/16**: Half notes
  - **/32**: Whole notes
  - **/64**: Ultra-slow (ambient)

### Pulse Counts
- **Range**: 1-7 pulses per stage
- **Visual**: Gradient display shows active pulses
- **Musical use**: Create complex rhythmic patterns
- **Example**: Stage with 3 pulses = 3 beats before next stage

## 🎹 Musical Applications

### Rhythmic Patterns
- **Drum programming**: Use MULTIPLE gates with varied pulse counts
- **Polyrhythms**: Combine different pulse counts across stages
- **Breakdowns**: Disable stages for dynamic arrangements

### Melodic Sequences
- **Bass lines**: Use SINGLE gates with root/5th patterns
- **Arpeggios**: Use MULTIPLE gates with chord tones
- **Pads**: Use HELD gates for sustained harmonies

### Ambient Textures
- **Slow timing**: Use /32-64 clock divisions
- **Long gates**: High gate values for overlapping notes
- **Evolve**: Gradually transform patterns over time

## 🔧 Technical Notes

### Arpeggiator Integration
- **Arpeggiator must be OFF**: Pulse sequencer handles its own timing
- **Uses arp settings**: Gate, velocity spread, note probability
- **Hybrid approach**: Pulse sequencer triggers, arpeggiator applies effects

### Track Compatibility
- **Synth tracks**: Full support for all features
- **MIDI tracks**: Full support for all features
- **Note probability**: Currently MIDI-only (synth support in development)

### Performance Tips
- **Stage dimming**: Inactive stages are dimmed for clear visual feedback
- **Fader controls**: Black pads show inactive range
- **Live tweaking**: All controls respond immediately during playback
- **Reset safety**: Purple reset button restores known good state

## 🎯 Workflow Examples

### Building a Pattern
1. Start with RESET for clean slate
2. Set 4 stages active (y4)
3. Set basic gate types (SINGLE for most)
4. Choose scale notes for melody
5. Add one MULTIPLE gate stage for rhythm
6. Adjust pulse counts for groove
7. Fine-tune with performance controls

### Live Performance
1. Start with a good base pattern
2. Use stage toggles to build/break down
3. EVOLVE for subtle variations
4. Adjust velocity spread for dynamics
5. Change play order for dramatic shifts
6. Use gate control for articulation changes

### Sound Design
1. Set very slow timing (/32-64)
2. Use HELD gates for sustained textures
3. High velocity spread for organic feel
4. Low note probability for sparse patterns
5. EVOLVE repeatedly for gradual transformation

## 🚀 Future Development

### Planned Features
- **Synth note probability**: Full support for synth tracks
- **More gate types**: Additional rhythmic patterns
- **Pattern chaining**: Link multiple pulse sequences
- **MIDI export**: Save patterns as MIDI files

---

**🎉 Congratulations on Alpha 0.1!**

The Pulse Sequencer is now a complete, professional-grade performance instrument. From rapid-fire 32nd note patterns to ultra-slow ambient textures, from simple melodies to complex polyrhythms - you have everything you need to create amazing music.

**Happy sequencing!** 🎵✨
