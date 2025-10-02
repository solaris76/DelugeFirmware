# Pulse Sequencer TODO List

## Current Status
- ✅ Basic sequencer functionality working
- ✅ Note-on/note-off messages working
- ✅ Red pad flash for active stages
- ✅ Gate types: OFF, SINGLE, MULTIPLE, HELD
- ✅ Timing with 16th note sync
- ✅ Stage length controls (x8-x15)
- ✅ Play orders: Forwards, Backwards, Ping Pong, Random

## High Priority Issues

### 1. Note Value Entry Options
- [ ] **Current**: Only scale notes with octave/transpose
- [ ] **Add**: Direct MIDI note entry (0-127)
- [ ] **Add**: Note name entry (C, D, E, F, G, A, B)
- [ ] **Add**: Chord entry (C major, D minor, etc.)
- [ ] **Add**: Preset note patterns

### 2. Accumulator Not Working
- [ ] **Issue**: Hold note pad + vertical encoder should change note value (-7 to +7)
- [ ] **Fix**: Implement accumulator logic in `handleVerticalEncoder()`
- [ ] **Visual**: Pad should light magenta when accumulator is non-zero
- [ ] **Reset**: Accumulator should reset when pad is released

### 3. Randomizer Features
- [ ] **Random Octave**: y1 x8-15 (cyan pads)
- [ ] **Random Gate**: y2 x8-15 (lime pads) 
- [ ] **Randomizer Lock**: y0 x15 (yellow toggle)
- [ ] **Random Note**: Random note selection within scale
- [ ] **Random Velocity**: Random velocity per note

### 4. Velocity Spread (Randomizer)
- [ ] **Current**: Fixed velocity (default)
- [ ] **Add**: Velocity spread control (0-127 range)
- [ ] **Add**: Random velocity per note
- [ ] **Add**: Velocity curves (linear, exponential, etc.)

### 5. Note Probability (Randomizer)
- [ ] **Add**: Probability control per stage (0-100%)
- [ ] **Add**: Random note skipping based on probability
- [ ] **Visual**: Dim pads when probability < 100%
- [ ] **Add**: Probability patterns (every 2nd, 3rd, etc.)

## Medium Priority Features

### 6. Other Play Orders
- [ ] **Current**: Forwards, Backwards, Ping Pong, Random
- [ ] **Add**: Custom pattern (user-defined sequence)
- [ ] **Add**: Euclidean rhythms
- [ ] **Add**: Polyrhythmic patterns
- [ ] **Add**: Probability-based play orders

### 7. Skips
- [ ] **Add**: Skip control per stage
- [ ] **Add**: Skip patterns (every 2nd, 3rd, etc.)
- [ ] **Add**: Random skips
- [ ] **Visual**: Show skipped stages differently
- [ ] **Add**: Skip probability

## Low Priority Enhancements

### 8. Performance Controls
- [ ] **Add**: Swing control
- [ ] **Add**: Groove templates
- [ ] **Add**: Humanization (timing/velocity)
- [ ] **Add**: Micro-timing adjustments

### 9. Visual Improvements
- [ ] **Add**: Better color coding for different states
- [ ] **Add**: Animation for play orders
- [ ] **Add**: Visual feedback for randomizer states
- [ ] **Add**: Progress indicators

### 10. Integration
- [ ] **Add**: Save/load patterns
- [ ] **Add**: Pattern variations
- [ ] **Add**: MIDI learn for controls
- [ ] **Add**: CV control integration

## Technical Debt
- [ ] **Clean up**: Remove unused functions
- [ ] **Optimize**: Reduce memory usage
- [ ] **Document**: Add code comments
- [ ] **Test**: Add unit tests for core functions

## Notes
- Current implementation uses direct `sendNote()` calls
- Red pad flash works with `lastPlayedStage` tracking
- Timing uses arpeggiator's sync system
- Gate length based on arp settings (1-50 as percentage)
- All track types supported (synth, MIDI, CV)
