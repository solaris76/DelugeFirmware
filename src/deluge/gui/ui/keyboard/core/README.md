# Core Features Library

A clean, well-defined library that provides keyboard layouts with easy access to Deluge's core functionality.

## Overview

The Core Features library is designed to give keyboard layout developers simple, safe access to Deluge's core systems without having to understand the complex internal architecture.

## Features

### Timing and Playback
- `isPlaying()` - Check if playback is active
- `getCurrentBPM()` - Get current tempo
- `getBeatPosition()` - Get current beat position
- `getCurrentTick()` - Get current tick count
- `getTicksPerBeat()` - Get ticks per beat
- `getTicksPerBar()` - Get ticks per bar

### Audio Clip Information
- `isCurrentClipAudio()` - Check if current clip is audio
- `isCurrentClipInstrument()` - Check if current clip is instrument
- `isCurrentClipKit()` - Check if current clip is kit
- `isCurrentClipMidi()` - Check if current clip is MIDI
- `isCurrentClipCV()` - Check if current clip is CV

### Waveform Rendering
- `renderWaveform()` - Render waveform data to LED grid

### Arpeggiator Access
- `isArpeggiatorEnabled()` - Check if arpeggiator is on
- `sendNoteToCurrentInstrument()` - Send notes to current instrument

### Arpeggiator Settings Access
- `getArpeggiatorMode()` / `setArpeggiatorMode()` - Enable/disable arpeggiator
- `getArpeggiatorPreset()` / `setArpeggiatorPreset()` - Get/set arpeggiator preset
- `getArpeggiatorOctaveMode()` / `setArpeggiatorOctaveMode()` - Get/set octave mode
- `getArpeggiatorNoteMode()` / `setArpeggiatorNoteMode()` - Get/set note mode
- `getArpeggiatorNumOctaves()` / `setArpeggiatorNumOctaves()` - Get/set number of octaves
- `getArpeggiatorSyncLevel()` / `setArpeggiatorSyncLevel()` - Get/set sync level
- `getArpeggiatorSyncType()` / `setArpeggiatorSyncType()` - Get/set sync type
- `getArpeggiatorStepRepeats()` / `setArpeggiatorStepRepeats()` - Get/set step repeats
- `getArpeggiatorRandomizerLock()` / `setArpeggiatorRandomizerLock()` - Get/set randomizer lock

### Arpeggiator Note Management
- `addNoteToArpeggiator()` - Add note to arpeggiator
- `removeNoteFromArpeggiator()` - Remove note from arpeggiator
- `clearArpeggiatorNotes()` - Clear all arpeggiator notes
- `getArpeggiatorActiveNoteCount()` - Get number of active notes
- `isNoteActiveInArpeggiator()` - Check if note is active

### Arpeggiator Playback Control
- `resetArpeggiator()` - Reset arpeggiator state
- `triggerArpeggiatorStep()` - Trigger arpeggiator step
- `isArpeggiatorGateActive()` - Check if gate is active
- `getArpeggiatorCurrentNote()` - Get current note index
- `getArpeggiatorCurrentOctave()` - Get current octave

### Scale and Key Information
- `isScaleModeEnabled()` - Check if scale mode is enabled
- `getRootNote()` - Get current root note
- `getCurrentScale()` - Get current scale
- `getSongRootNote()` - Get song root note
- `getSongScale()` - Get song scale

### UI Feedback
- `showPopup()` - Show popup message on OLED

## Usage

```cpp
#include "gui/ui/keyboard/core/core_features.h"

// In your keyboard layout:
void MyLayout::renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) {
    // Check if playing
    if (CoreFeatures::isPlaying()) {
        // Show play indicator
        image[0][0] = RGB(255, 255, 0); // Yellow
    }

    // Get timing info
    uint32_t beat = CoreFeatures::getBeatPosition();
    float bpm = CoreFeatures::getCurrentBPM();

    // Show popup
    CoreFeatures::showPopup("Layout Active");

    // Arpeggiator control
    if (CoreFeatures::isArpeggiatorEnabled()) {
        // Show arpeggiator status
        image[1][0] = RGB(0, 255, 0); // Green for active

        // Display current settings
        int32_t numOctaves = CoreFeatures::getArpeggiatorNumOctaves();
        int32_t syncLevel = CoreFeatures::getArpeggiatorSyncLevel();

        // Visualize octaves (rows 2-5)
        for (int i = 0; i < numOctaves && i < 4; i++) {
            image[2 + i][0] = RGB(255, 255, 0); // Yellow for each octave
        }

        // Visualize sync level (columns 1-15)
        for (int i = 0; i < syncLevel && i < 15; i++) {
            image[0][1 + i] = RGB(0, 0, 255); // Blue for sync level
        }
    }

    // Add notes to arpeggiator
    CoreFeatures::addNoteToArpeggiator(60, 100); // Middle C
    CoreFeatures::addNoteToArpeggiator(64, 100); // E
    CoreFeatures::addNoteToArpeggiator(67, 100); // G

    // Control arpeggiator settings
    CoreFeatures::setArpeggiatorMode(true);
    CoreFeatures::setArpeggiatorNumOctaves(3);
    CoreFeatures::setArpeggiatorSyncLevel(8);
}
```

## Benefits

1. **Clean API** - Simple, intuitive function names
2. **Safe Access** - No need to understand complex internal systems
3. **Consistent** - Same interface across all keyboard layouts
4. **Extensible** - Easy to add new features
5. **Well-Documented** - Clear documentation and examples

## Future Enhancements

- Real-time audio analysis
- MIDI input/output handling
- Advanced waveform rendering
- Scale-aware note generation
- Performance monitoring
- Custom UI elements

This library makes it much easier to create powerful, feature-rich keyboard layouts that integrate seamlessly with Deluge's core functionality.
