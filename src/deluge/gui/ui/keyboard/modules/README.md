# Deluge Keyboard Modules System

## Overview

The Deluge Keyboard Modules System is a modular architecture that allows extending the Deluge's keyboard interface with additional functionality like timing displays, audio visualizers, custom controls, and more.

## Architecture

### Core Components

1. **KeyboardModule** (Base Interface)
   - Abstract base class for all modules
   - Defines standard interface: init(), update(), render(), handlePadPress(), handleEncoder()
   - Provides metadata: name, author, version, description
   - Supports priority-based rendering and enable/disable functionality

2. **ModuleManager** (Coordinator)
   - Manages all registered modules
   - Handles event routing (pad presses, encoder turns)
   - Coordinates rendering in priority order
   - Provides module registration and configuration

3. **KeyboardScreen Integration**
   - Modified keyboard screen to use module system
   - Modules render on top of base keyboard layouts
   - Event handling passes through modules first

### Module Types

#### Built-in Modules

1. **TimingModule** (Priority 10)
   - Shows beat position indicator
   - Displays tempo visualization
   - Shows playback state (playing/stopped)
   - Renders on rows 0-2

2. **TestModule** (Priority 5)
   - Demonstrates module functionality
   - Shows animated checkerboard pattern
   - Renders on rows 4-7

## Usage

### Creating a New Module

```cpp
class MyModule : public KeyboardModule {
public:
    MyModule() {
        setPriority(15); // Higher priority = renders on top
    }

    bool init() override {
        // Initialize your module
        return true;
    }

    void update(uint32_t deltaTime) override {
        // Update module state
    }

    void render(RGB image[][kDisplayWidth + kSideBarWidth],
                uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override {
        // Draw on LED grid
    }

    bool handlePadPress(int32_t x, int32_t y, int32_t velocity) override {
        // Handle pad presses
        return false; // Let other modules handle if not processed
    }

    bool handleEncoder(int32_t offset, bool isVertical) override {
        // Handle encoder events
        return false; // Let other modules handle if not processed
    }

    const char* getName() const override { return "My Module"; }
    const char* getAuthor() const override { return "Your Name"; }
    const char* getVersion() const override { return "1.0.0"; }
    const char* getDescription() const override { return "My awesome module"; }
};
```

### Registering a Module

```cpp
// In module_manager.cpp init() method:
registerModule(new MyModule());
```

## Technical Details

### Rendering Order
- Modules are sorted by priority (highest first)
- Higher priority modules render on top of lower priority modules
- Base keyboard layout renders first, then modules

### Event Handling
- Pad presses and encoder events are passed to modules in priority order
- First module to return `true` handles the event
- If no module handles the event, it passes to the base keyboard system

### Memory Management
- Modules are allocated on the heap
- ModuleManager handles cleanup in destructor
- Uses Deluge's memory allocator system

## Future Enhancements

### Planned Features
1. **SD Card Module Loading** - Load modules from JSON configuration files
2. **Module Configuration** - Save/load module settings
3. **Dynamic Module Loading** - Load modules at runtime
4. **Module Dependencies** - Modules that depend on other modules
5. **Module Communication** - Inter-module messaging system

### Potential Module Types
1. **Audio Visualizer** - Real-time audio spectrum display
2. **Scale Display** - Show current scale notes
3. **Chord Progression** - Visual chord progression display
4. **Performance Controls** - Custom performance interface
5. **Learning Mode** - Interactive tutorials and guides
6. **Custom Animations** - User-defined visual effects

## Files

### Core Files
- `src/deluge/gui/ui/keyboard/modules/module_base.h` - Base module interface
- `src/deluge/gui/ui/keyboard/modules/module_manager.h` - Module manager header
- `src/deluge/gui/ui/keyboard/modules/module_manager.cpp` - Module manager implementation
- `src/deluge/gui/ui/keyboard/modules/timing_module.h` - Timing module header
- `src/deluge/gui/ui/keyboard/modules/timing_module.cpp` - Timing module implementation
- `src/deluge/gui/ui/keyboard/modules/test_module.h` - Test module header
- `src/deluge/gui/ui/keyboard/modules/test_module.cpp` - Test module implementation

### Integration Files
- `src/deluge/gui/ui/keyboard/keyboard_screen.cpp` - Modified to use module system

## Benefits

1. **Extensibility** - Easy to add new functionality
2. **Modularity** - Each module is independent
3. **Priority System** - Control rendering order
4. **Event Handling** - Modules can respond to user input
5. **Animation Support** - Modules can create dynamic visuals
6. **Memory Efficient** - Uses Deluge's memory management
7. **Type Safe** - C++ interface ensures type safety

## Conclusion

The Deluge Keyboard Modules System provides a powerful foundation for extending the Deluge's keyboard interface. It allows developers to create unique interfaces that can control almost every aspect of the Deluge while maintaining the existing keyboard functionality.

The system is designed to be:
- **Easy to use** - Simple interface for module developers
- **Powerful** - Full access to Deluge's systems
- **Efficient** - Minimal performance impact
- **Extensible** - Easy to add new features

This modular approach opens up endless possibilities for customizing the Deluge experience and creating unique interfaces for different use cases.
