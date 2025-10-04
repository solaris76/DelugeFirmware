# Core Features Library - Modern C++ Implementation

## 🎯 **Direct Property Access - Much Simpler!**

The CoreFeatures API now uses **direct property access** instead of many getter/setter functions. This makes the API much cleaner and more intuitive:

### **❌ Old Way (Many Functions):**
```cpp
core->effects().reverb().setEnabled(true);
core->effects().reverb().setSendAmount(50);
core->effects().reverb().setRoomSize(0.8f);

bool enabled = core->effects().reverb().isEnabled();
int32_t amount = core->effects().reverb().getSendAmount();
float roomSize = core->effects().reverb().getRoomSize();
```

### **✅ New Way (Direct Properties):**
```cpp
core->effects().reverb().enabled = true;
core->effects().reverb().sendAmount = 50;
core->effects().reverb().roomSize = 0.8f;

bool enabled = core->effects().reverb().enabled;
int32_t amount = core->effects().reverb().sendAmount;
float roomSize = core->effects().reverb().roomSize;
```

This is **exactly how C++ works** and is often preferred for simple data access!

## Overview

The Core Features Library provides a modern, C++-idiomatic API for keyboard layouts to access Deluge's core functionalities. This implementation follows C++ best practices including RAII, dependency injection, and proper encapsulation.

## Key Benefits

### 🎯 **Modern C++ Design**
- **RAII Compliant**: Automatic resource management
- **Dependency Injection**: Easy testing and configuration
- **Clear Ownership**: Smart pointers instead of raw pointers
- **Exception Safety**: Proper error handling
- **Const Correctness**: Immutable interfaces where appropriate

### 🧪 **Testability**
- **Mockable Controllers**: Each controller can be easily mocked for testing
- **Isolated Dependencies**: Clear separation of concerns
- **Factory Pattern**: Controlled object creation

### 🔒 **Safety**
- **No Global State**: Each instance is independent
- **Memory Safety**: Automatic cleanup with smart pointers
- **Thread Safety**: Designed for safe concurrent access

## Usage

### Basic Usage

```cpp
#include "core_features.h"

// Create an instance (RAII)
auto core = CoreFeatures::create();

// Access controllers
if (core->timing().isPlaying()) {
    core->display().setPadLED(0, 0, RGB_RED);
    core->display().showPopup("Playing!");
}

// Fader functionality
core->display().fader().setHorizontalFader(0, 0, 75, 100, 0x001100, 0x00FF00); // Volume fader
core->display().fader().setVerticalFader(0, 0, 80, 100, 0x111100, 0xFFFF00); // Filter cutoff
core->display().fader().setShortHorizontalFader(0, 2, 50, 100, 0x001111, 0x00FFFF); // Attack fader

// Direct access to settings
core->arpeggiator().settings().mode = ArpMode::ARP;
core->arpeggiator().settings().numOctaves = 2;

// Effect control
core->effects().reverb().setEnabled(true);
core->effects().reverb().setRoomSize(0.8f);
core->effects().delay().setSyncLevel(2);
```

### Advanced Usage

```cpp
// Const access for read-only operations
const auto& timing = core->timing();
if (timing.isPlaying()) {
    int32_t currentTick = timing.getCurrentTick();
    int32_t bpm = timing.getCurrentBPM();
}

// Effect chaining
auto& effects = core->effects();
effects.reverb().setEnabled(true);
effects.delay().setEnabled(true);
effects.filter().setEnabled(true);

// Arpeggiator control
auto& arp = core->arpeggiator();
arp.addNote(60, 127); // Middle C
arp.setEnabled(true);
arp.settings().mode = ArpMode::ARP;
arp.settings().numOctaves = 3;
```

## Architecture

### Controller Pattern

Each controller is responsible for a specific domain:

- **`TimingController`**: Playback state, BPM, tick management
- **`DisplayController`**: LED control, popups, rendering
- **`AudioController`**: Note sending, waveform rendering
- **`ArpeggiatorController`**: Arpeggiator settings and operations
- **`EffectsController`**: Audio effects (reverb, delay, filter)
- **`ScaleController`**: Scale and key information

### Factory Pattern

```cpp
// Factory method ensures proper initialization
auto core = CoreFeatures::create();

// Private constructor prevents direct instantiation
// CoreFeatures core; // ❌ Compilation error
```

### RAII and Smart Pointers

```cpp
class CoreFeatures {
private:
    std::unique_ptr<TimingController> timing_;
    std::unique_ptr<DisplayController> display_;
    // ... other controllers

public:
    // Automatic cleanup when CoreFeatures goes out of scope
    ~CoreFeatures() = default; // Smart pointers handle cleanup
};
```

## API Reference

### CoreFeatures

```cpp
class CoreFeatures {
public:
    // Factory method
    static std::unique_ptr<CoreFeatures> create();

    // Controller access
    TimingController& timing();
    DisplayController& display();
    AudioController& audio();
    ArpeggiatorController& arpeggiator();
    EffectsController& effects();
    ScaleController& scale();

    // Const access
    const TimingController& timing() const;
    const DisplayController& display() const;
    // ... etc
};
```

### TimingController

```cpp
class TimingController {
public:
    bool isPlaying() const;
    int32_t getCurrentTick() const;
    int32_t getCurrentBPM() const;

    void setBPM(int32_t bpm);
    void play();
    void pause();
    void stop();
    void doTickForward();
    void doTickBackward();
};
```

### AudioController

```cpp
class AudioController {
public:
    // Output type detection
    OutputType getCurrentClipOutputType() const;

    // Convenience methods
    bool isCurrentClipInstrument() const;
    bool isCurrentClipAudio() const;

    // Note operations
    void sendNoteToCurrentInstrument(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel = 0);
    void stopNoteOnCurrentInstrument(int32_t noteCode, int32_t fromMIDIChannel = 0);

    // Clip access
    InstrumentClip* getCurrentInstrumentClip() const;

    // Waveform rendering
    void renderWaveform(RGB image[][kDisplayWidth + kSideBarWidth],
                       uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
                       int32_t xScroll, int32_t xZoom, int32_t whichKernel, int32_t whichKernelStartedThis);
};
```

### ArpeggiatorController

```cpp
class ArpeggiatorController {
public:
    // Direct access to settings
    ArpeggiatorSettings& settings();
    const ArpeggiatorSettings& settings() const;

    // High-level operations
    void addNote(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel = 0);
    void removeNote(int32_t noteCode);
    void clearNotes();
    void reset();
    void triggerStep();

    // State queries
    bool isEnabled() const;
    void setEnabled(bool enabled);
    // ... more methods
};
```

### EffectsController

```cpp
class EffectsController {
public:
    // Effect objects
    ReverbEffect& reverb();
    DelayEffect& delay();
    FilterEffect& filter();

    // Const access
    const ReverbEffect& reverb() const;
    const DelayEffect& delay() const;
    const FilterEffect& filter() const;
};

// Individual effect classes
class ReverbEffect {
public:
    bool isEnabled() const;
    void setEnabled(bool enabled);
    float getRoomSize() const;
    void setRoomSize(float roomSize);
    // ... more methods
};
```

## Fader Functionality

The `DisplayController::Fader` provides easy-to-use fader controls for visual feedback:

### Horizontal Faders (y0-7, x0-15)
```cpp
// Volume fader on row 0
int32_t volume = 75; // 75% volume
core->display().fader().setHorizontalFader(0, 0, volume, 100, 0x001100, 0x00FF00);
//                                                      startX y  value max dimmed lit
```

### Vertical Faders (x0-7, y0-15)
```cpp
// Filter cutoff fader on column 0
int32_t cutoff = 80; // 80% cutoff
core->display().fader().setVerticalFader(0, 0, cutoff, 100, 0x111100, 0xFFFF00);
//                                                    x startY value max dimmed lit
```

### Short Horizontal Faders (x0-7 only)
```cpp
// Attack fader on row 2
int32_t attack = 50; // 50% attack
core->display().fader().setShortHorizontalFader(0, 2, attack, 100, 0x001111, 0x00FFFF);
//                                                           startX y  value max dimmed lit
```

### Reading Fader Values
```cpp
// Get value from pad position
int32_t padX = 8, padY = 0;
int32_t faderValue = core->display().fader().getValueFromHorizontalFader(0, 0, padX, 100);
// Returns: 50 (for pad at x=8 in a 16-pad fader)

int32_t verticalValue = core->display().fader().getValueFromVerticalFader(0, 0, padY, 100);
int32_t shortValue = core->display().fader().getValueFromShortHorizontalFader(0, 2, padX, 100);
```

### Dynamic Fader Updates
```cpp
// Update fader based on current BPM
int32_t currentBPM = core->timing().getCurrentBPM();
int32_t bpmPercentage = (currentBPM * 100) / 200; // Scale 0-200 BPM to 0-100%
core->display().fader().setHorizontalFader(0, 7, bpmPercentage, 100, 0x000011, 0x0000FF);

// Update fader based on arpeggiator state
if (core->arpeggiator().isEnabled()) {
    int32_t activeNotes = core->arpeggiator().getActiveNoteCount();
    int32_t notePercentage = (activeNotes * 100) / 8; // Scale 0-8 notes to 0-100%
    core->display().fader().setHorizontalFader(0, 6, notePercentage, 100, 0x110000, 0xFF0000);
}
```

## Usage Examples

### Output Type Detection

```cpp
auto core = CoreFeatures::create();

// Get the current clip's output type
OutputType outputType = core->audio().getCurrentClipOutputType();

switch (outputType) {
    case OutputType::SYNTH:
        // Handle synth clip
        core->audio().sendNoteToCurrentInstrument(60, 100); // C4
        break;

    case OutputType::KIT:
        // Handle kit clip (drums)
        core->audio().sendNoteToCurrentInstrument(36, 80); // Kick
        break;

    case OutputType::CV:
        // Handle CV output
        core->audio().sendNoteToCurrentInstrument(60, 100);
        break;

    case OutputType::MIDI_OUT:
        // Handle MIDI output
        core->audio().sendNoteToCurrentInstrument(60, 100);
        break;

    case OutputType::AUDIO:
        // Audio clip - effects available
        core->effects().reverb().setEnabled(true);
        break;

    case OutputType::SYNTH:
    case OutputType::KIT:
        // Synth and Kit tracks also support effects
        core->effects().reverb().setEnabled(true);
        core->effects().delay().setEnabled(true);
        break;

    case OutputType::NONE:
    default:
        // No current clip
        break;
}

// Convenience methods
if (core->audio().isCurrentClipInstrument()) {
    printf("Can send notes to this clip\n");
}

if (core->audio().isCurrentClipAudio()) {
    printf("This is an audio clip\n");
}
```

### Timing and Playback

```cpp
auto core = CoreFeatures::create();

// Check playback state
if (core->timing().isPlaying()) {
    int32_t currentTick = core->timing().getCurrentTick();
    int32_t bpm = core->timing().getCurrentBPM();

    printf("Playing at tick %d, BPM %d\n", currentTick, bpm);
}

// Control playback
core->timing().play();
core->timing().setBPM(120);
```

### Effects Control

```cpp
auto core = CoreFeatures::create();

// Reverb control
if (core->effects().reverb().isEnabled()) {
    int32_t sendAmount = core->effects().reverb().getSendAmount();
    printf("Reverb send: %d%%\n", sendAmount);
}

// Enable effects
core->effects().reverb().setEnabled(true);
core->effects().reverb().setSendAmount(50);

core->effects().delay().setEnabled(true);
core->effects().delay().setSyncLevel(2);
```

## Migration from Static Approach

### Before (Static Objects)
```cpp
// Global static objects - can cause initialization issues
CoreFeatures::timing.isPlaying();
CoreFeatures::reverb.setEnabled(true);
CoreFeatures::arpeggiator.settings->mode = ArpMode::ARP;
```

### After (RAII + Dependency Injection)
```cpp
// Create instance with proper initialization
auto core = CoreFeatures::create();

// Clean, safe access
if (core->timing().isPlaying()) {
    core->display().setPadLED(0, 0, RGB_RED);
}

core->effects().reverb().setEnabled(true);
core->arpeggiator().settings().mode = ArpMode::ARP;

// Automatic cleanup when core goes out of scope
```

## Testing

### Unit Testing Example

```cpp
#include <gtest/gtest.h>
#include "core_features.h"

class MockTimingController : public TimingController {
public:
    MOCK_METHOD(bool, isPlaying, (), (const, override));
    MOCK_METHOD(int32_t, getCurrentTick, (), (const, override));
    // ... mock other methods
};

TEST(CoreFeaturesTest, PlaybackState) {
    auto core = CoreFeatures::create();
    auto mockTiming = std::make_unique<MockTimingController>();

    EXPECT_CALL(*mockTiming, isPlaying())
        .WillOnce(Return(true));

    // Test with mock
    ASSERT_TRUE(core->timing().isPlaying());
}
```

## Performance Considerations

### Memory Usage
- **Smart Pointers**: Minimal overhead compared to raw pointers
- **RAII**: Automatic cleanup prevents memory leaks
- **Stack Allocation**: Controllers are allocated on the heap but managed automatically

### Runtime Performance
- **Virtual Calls**: Minimal overhead for controller access
- **Const Correctness**: Compiler optimizations for const methods
- **Move Semantics**: Efficient object transfers

## Best Practices

### 1. Use RAII
```cpp
// ✅ Good - automatic cleanup
{
    auto core = CoreFeatures::create();
    // ... use core
} // Automatic cleanup here

// ❌ Bad - manual management
CoreFeatures* core = new CoreFeatures();
// ... use core
delete core; // Easy to forget
```

### 2. Prefer Const Access
```cpp
// ✅ Good - const access for read-only operations
const auto& timing = core->timing();
if (timing.isPlaying()) {
    // ... read-only operations
}

// ❌ Bad - non-const access when not needed
auto& timing = core->timing();
if (timing.isPlaying()) {
    // ... read-only operations
}
```

### 3. Use Factory Pattern
```cpp
// ✅ Good - controlled creation
auto core = CoreFeatures::create();

// ❌ Bad - direct instantiation
CoreFeatures core; // Compilation error - constructor is private
```

## Future Enhancements

### Planned Features
- **Async Operations**: Non-blocking audio operations
- **Event System**: Observer pattern for state changes
- **Plugin Architecture**: Dynamic loading of custom controllers
- **Configuration**: Runtime configuration of controller behavior

### Extension Points
- **Custom Controllers**: Implement your own controllers
- **Middleware**: Add logging, profiling, or other cross-cutting concerns
- **Testing**: Comprehensive mock framework

## Conclusion

This modern C++ implementation provides a robust, maintainable, and testable foundation for keyboard layouts. The RAII approach ensures proper resource management, while the controller pattern provides clear separation of concerns. The factory pattern enables controlled object creation and easy testing.

The API is designed to be intuitive while following C++ best practices, making it easy for developers to create sophisticated keyboard layouts that integrate seamlessly with Deluge's core functionality.
