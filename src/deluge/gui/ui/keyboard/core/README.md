# Core Features Library - Modern C++ Implementation

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
