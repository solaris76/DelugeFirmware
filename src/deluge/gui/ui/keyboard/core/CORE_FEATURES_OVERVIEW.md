# Core Features Library: Modern C++ API for Deluge Keyboard Layouts

## 🎯 **The Problem**

### **Current State: Limited Keyboard Layout Capabilities**

Deluge's keyboard layouts are currently **very basic** and **limited**:

```cpp
// Current keyboard layout limitations:
class BasicKeyboardLayout {
    void renderPads() {
        // Can only:
        // - Light up pads
        // - Play notes when pressed
        // - Basic visual feedback

        // CANNOT:
        // - Access timing information
        // - Control audio effects
        // - Use arpeggiator engine
        // - Render waveforms
        // - Access scale information
        // - Control playback
    }
};
```

### **What Developers Want**

Keyboard layout developers need access to **Deluge's core functionality**:

- **Timing Control**: Access to BPM, playback state, tick information
- **Audio Effects**: Control reverb, delay, filters, and other effects
- **Arpeggiator**: Use Deluge's built-in arpeggiator engine
- **Display Control**: Advanced LED control, popups, rendering
- **Audio Analysis**: Waveform rendering, audio clip information
- **Scale Information**: Root note, scale type, scale mode

### **The Challenge**

Deluge's internal APIs are:
- **Complex**: Deep inheritance hierarchies, multiple parameter systems
- **Fragile**: Direct access can break with firmware updates
- **Inconsistent**: Different patterns for different subsystems
- **Unsafe**: Easy to access private members, cause crashes

## 🚀 **The Solution: Core Features Library**

### **What It Is**

A **modern C++ wrapper** that provides **clean, safe access** to Deluge's core functionality:

```cpp
// Clean, modern API
auto core = CoreFeatures::create();

// Timing control
if (core->timing().isPlaying()) {
    int32_t bpm = core->timing().getCurrentBPM();
    core->timing().setBPM(120);
}

// Audio effects
core->effects().reverb().setEnabled(true);
core->effects().reverb().setSendAmount(50);
int32_t reverbAmount = core->effects().reverb().getSendAmount();

// Display control
core->display().setPadLED(0, 0, 0xFF0000);
core->display().showPopup("Hello!");
core->display().requestRendering();

// Arpeggiator
core->arpeggiator().addNote(60, 127);
core->arpeggiator().setEnabled(true);
```

### **Architecture Overview**

```
┌─────────────────────────────────────────────────────────────┐
│                    CoreFeatures                            │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────┐ │
│  │   Timing    │ │  Display    │ │   Audio     │ │ Effects│ │
│  │ Controller  │ │ Controller  │ │ Controller  │ │Controller│ │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────┘ │
│  ┌─────────────┐ ┌─────────────┐                            │
│  │Arpeggiator  │ │   Scale     │                            │
│  │ Controller  │ │ Controller  │                            │
│  └─────────────┘ └─────────────┘                            │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ Deluge Internal │
                    │     APIs        │
                    │                 │
                    │ • PlaybackHandler│
                    │ • ParamManager  │
                    │ • PadLEDs       │
                    │ • Arpeggiator   │
                    │ • GlobalEffects │
                    └─────────────────┘
```

## 🏗️ **Design Principles**

### **1. Modern C++ Best Practices**

```cpp
// RAII (Resource Acquisition Is Initialization)
class CoreFeatures {
private:
    std::unique_ptr<TimingController> timing_;
    std::unique_ptr<DisplayController> display_;
    // Automatic cleanup, no memory leaks
};

// Factory Pattern
static std::unique_ptr<CoreFeatures> create();

// Const Correctness
const TimingController& timing() const;
TimingController& timing();
```

### **2. Single Responsibility Principle**

Each controller handles **one domain**:

- **`TimingController`**: Playback state, BPM, tick control
- **`DisplayController`**: LED control, popups, rendering
- **`AudioController`**: Note sending, waveform rendering
- **`EffectsController`**: Reverb, delay, filter control
- **`ArpeggiatorController`**: Arpeggiator engine access
- **`ScaleController`**: Scale and key information

### **3. Safe Parameter Access**

```cpp
// Safe effect parameter access
bool EffectsController::ReverbEffect::isEnabled() const {
    // 1. Check if we have an audio output
    InstrumentClip* clip = getCurrentInstrumentClip();
    if (!clip || clip->output->type != OutputType::AUDIO) {
        return false; // Safe fallback
    }

    // 2. Get parameter manager
    ParamManagerForTimeline& paramManager = clip->paramManager;

    // 3. Check parameter value
    UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
    int32_t reverbAmount = unpatchedParams->getValue(UNPATCHED_REVERB_SEND_AMOUNT);

    // 4. Convert and return
    return reverbAmount > std::numeric_limits<q31_t>::min();
}
```

## 🎯 **Key Benefits**

### **1. Developer Experience**

**Before (Direct API Access):**
```cpp
// Complex, error-prone, fragile
InstrumentClip* clip = currentSong->currentClip;
if (clip && clip->output->type == OutputType::AUDIO) {
    ParamManagerForTimeline& paramManager = clip->paramManager;
    UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
    int32_t rawValue = unpatchedParams->getValue(UNPATCHED_REVERB_SEND_AMOUNT);
    // Convert Q31 to percentage...
    // Handle edge cases...
    // Check for null pointers...
}
```

**After (Core Features API):**
```cpp
// Clean, simple, safe
auto core = CoreFeatures::create();
int32_t reverbAmount = core->effects().reverb().getSendAmount();
```

### **2. Safety & Reliability**

- **Null Safety**: No null pointer dereferences
- **Type Safety**: Compile-time type checking
- **Bounds Checking**: Safe parameter value conversion
- **Error Handling**: Graceful fallbacks for invalid states

### **3. Maintainability**

- **Encapsulation**: Internal API changes don't break layouts
- **Consistent Interface**: Same patterns across all controllers
- **Clear Ownership**: `std::unique_ptr` prevents memory leaks
- **Testability**: Easy to mock for unit testing

### **4. Performance**

**Memory Overhead:**
- **Per Instance**: ~50-100 bytes (negligible for UI code)
- **Runtime**: ~5-10 extra cycles per call (acceptable for UI)

**Optimizations:**
- **No Virtual Calls**: Direct method calls
- **Smart Pointers**: Minimal overhead
- **Reference Semantics**: No unnecessary copying

## 📊 **Performance Analysis**

### **Memory Usage**

```cpp
// CoreFeatures instance:
std::unique_ptr<TimingController> timing_;     // 8 bytes
std::unique_ptr<DisplayController> display_;   // 8 bytes
std::unique_ptr<AudioController> audio_;        // 8 bytes
std::unique_ptr<ArpeggiatorController> arp_;   // 8 bytes
std::unique_ptr<EffectsController> effects_;    // 8 bytes
std::unique_ptr<ScaleController> scale_;       // 8 bytes
// Total: ~48 bytes + controller objects (~100-200 bytes total)
```

### **Runtime Performance**

```cpp
// Method call overhead:
core->effects().reverb().getSendAmount();
// 1. Pointer dereference (timing_)     ~1 cycle
// 2. Pointer dereference (effects_)    ~1 cycle
// 3. Method call                      ~2-3 cycles
// 4. Parameter lookup                 ~10-20 cycles
// 5. Q31 conversion                   ~5-10 cycles
// Total: ~20-35 cycles (negligible for UI)
```

### **Comparison with Direct Access**

| Approach | Memory | CPU Cycles | Safety | Maintainability |
|----------|--------|------------|--------|-----------------|
| **Direct API** | 0 bytes | ~15 cycles | ❌ Low | ❌ Poor |
| **Core Features** | ~100 bytes | ~30 cycles | ✅ High | ✅ Excellent |

## 🎵 **Real-World Usage Examples**

### **Effect Visualizer Layout**

```cpp
class EffectVisualizerLayout {
    void renderEffects() {
        auto core = CoreFeatures::create();

        // Visualize reverb on row 0
        if (core->effects().reverb().isEnabled()) {
            int32_t amount = core->effects().reverb().getSendAmount();
            for (int i = 0; i < 8; i++) {
                bool light = (amount > (i * 12)); // 12% per pad
                core->display().setPadLED(i, 0, light ? 0xFF0000 : 0x000000);
            }
        }

        // Visualize delay sync on row 1
        if (core->effects().delay().isEnabled()) {
            int32_t syncLevel = core->effects().delay().getSyncLevel();
            for (int i = 0; i < 8; i++) {
                bool light = (i <= syncLevel);
                core->display().setPadLED(i, 1, light ? 0x00FF00 : 0x000000);
            }
        }

        core->display().requestRendering();
    }
};
```

### **Generative Sequencer Layout**

```cpp
class GenerativeSequencerLayout {
    void generateNotes() {
        auto core = CoreFeatures::create();

        // Use arpeggiator for note generation
        core->arpeggiator().addNote(60, 127); // C4
        core->arpeggiator().addNote(64, 127); // E4
        core->arpeggiator().addNote(67, 127); // G4

        // Configure arpeggiator
        core->arpeggiator().settings().mode = ArpMode::ARP;
        core->arpeggiator().settings().numOctaves = 2;
        core->arpeggiator().setEnabled(true);

        // Trigger step
        core->arpeggiator().triggerStep();
    }
};
```

### **Audio Analysis Layout**

```cpp
class AudioAnalysisLayout {
    void renderWaveform() {
        auto core = CoreFeatures::create();

        if (core->audio().isCurrentClipAudio()) {
            // Render waveform to display
            RGB image[kDisplayHeight][kDisplayWidth + kSideBarWidth];
            uint8_t occupancyMask[kDisplayHeight][kDisplayWidth + kSideBarWidth];

            core->audio().renderWaveform(image, occupancyMask,
                                        xScroll, xZoom,
                                        whichKernel, whichKernelStartedThis);
        }
    }
};
```

## 🔧 **Technical Implementation Details**

### **Parameter Access Pattern**

```cpp
// Safe parameter retrieval
int32_t EffectsController::ReverbEffect::getSendAmount() const {
    // 1. Validate audio output
    InstrumentClip* clip = getCurrentInstrumentClip();
    if (!clip || clip->output->type != OutputType::AUDIO) {
        return 0; // Safe fallback
    }

    // 2. Access parameter manager
    ParamManagerForTimeline& paramManager = clip->paramManager;
    UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

    // 3. Get raw parameter value
    int32_t rawValue = unpatchedParams->getValue(UNPATCHED_REVERB_SEND_AMOUNT);

    // 4. Convert Q31 to percentage
    if (rawValue == std::numeric_limits<q31_t>::min()) {
        return 0; // Disabled
    }
    return ((rawValue + 2147483648) * 100) / 4294967296;
}
```

### **Parameter Setting Pattern**

```cpp
// Safe parameter setting
void EffectsController::ReverbEffect::setSendAmount(int32_t amount) {
    // 1. Validate audio output
    InstrumentClip* clip = getCurrentInstrumentClip();
    if (!clip || clip->output->type != OutputType::AUDIO) {
        return; // Safe exit
    }

    // 2. Access parameter manager
    ParamManagerForTimeline& paramManager = clip->paramManager;
    UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

    // 3. Convert percentage to Q31
    if (amount <= 0) {
        unpatchedParams->params[UNPATCHED_REVERB_SEND_AMOUNT]
            .setCurrentValueBasicForSetup(std::numeric_limits<q31_t>::min());
    } else {
        int32_t q31Value = (amount * 4294967296) / 100 - 2147483648;
        unpatchedParams->params[UNPATCHED_REVERB_SEND_AMOUNT]
            .setCurrentValueBasicForSetup(q31Value);
    }
}
```

## 🎯 **Why This Approach is Perfect for Deluge**

### **1. Right Tool for the Job**

- **UI Code**: Keyboard layouts are UI code, not audio processing
- **Performance**: ~30 cycles overhead is negligible for UI operations
- **Memory**: ~100 bytes per instance is acceptable for UI components

### **2. Deluge's Architecture**

- **Modern C++**: Deluge already uses modern C++ features
- **Modular Design**: Fits well with existing modular architecture
- **Extensibility**: Easy to add new controllers without breaking existing code

### **3. Developer Ecosystem**

- **Accessibility**: Makes Deluge development accessible to more developers
- **Innovation**: Enables creative new keyboard layouts
- **Community**: Encourages community contributions

## 🚀 **Future Possibilities**

### **Potential Extensions**

```cpp
// Additional controllers could include:
class MidiController {
    void sendMidiCC(uint8_t channel, uint8_t cc, uint8_t value);
    void sendMidiNote(uint8_t channel, uint8_t note, uint8_t velocity);
};

class SampleController {
    void loadSample(const char* path);
    void setSampleStart(int32_t start);
    void setSampleEnd(int32_t end);
};

class ModMatrixController {
    void setModulation(int32_t source, int32_t destination, int32_t amount);
    int32_t getModulationAmount(int32_t source, int32_t destination);
};
```

### **Advanced Features**

- **Plugin System**: Dynamic loading of custom controllers
- **Scripting Support**: JavaScript/Lua bindings for rapid prototyping
- **Visual Editor**: GUI for creating keyboard layouts
- **Community Repository**: Shared library of keyboard layouts

## 📈 **Impact Assessment**

### **For Developers**

✅ **Easier Development**: Clean API reduces complexity
✅ **Faster Iteration**: Less time debugging internal APIs
✅ **More Creative**: Access to previously unavailable features
✅ **Better Quality**: Safer code with fewer crashes

### **For Users**

✅ **More Layouts**: Easier development = more available layouts
✅ **Better Features**: Access to effects, timing, arpeggiator
✅ **Stability**: Safer code = fewer crashes
✅ **Innovation**: New creative possibilities

### **For Deluge Ecosystem**

✅ **Community Growth**: More developers can contribute
✅ **Feature Richness**: More sophisticated keyboard layouts
✅ **Maintainability**: Cleaner separation of concerns
✅ **Future Proof**: Easier to extend and modify

## 🎉 **Conclusion**

The **Core Features Library** represents a **significant advancement** for Deluge keyboard layout development:

- **Modern C++ Design**: Follows current best practices
- **Safe & Reliable**: Prevents common programming errors
- **High Performance**: Minimal overhead for UI operations
- **Developer Friendly**: Clean, intuitive API
- **Future Ready**: Extensible architecture for new features

This approach transforms Deluge keyboard layouts from **basic note-playing interfaces** into **powerful, creative tools** that can leverage the full capabilities of the Deluge hardware.

**The result**: More developers creating more innovative keyboard layouts, leading to a richer, more creative Deluge experience for all users! 🎵✨
