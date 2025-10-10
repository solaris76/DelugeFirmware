# Deluge Sequencer Modes Implementation Plan

## Overview

This document outlines the implementation plan for creating a RAII-formatted library that enables additional sequencer modes as alternatives to the Deluge's default clip views. The system will maintain the existing clip views (piano roll for instrument clips, waveform view for audio clips) as the master defaults while allowing seamless integration of new sequencer types that work universally with **all track types: Sound, MIDI, CV, and Audio tracks**.

**Key Innovation**: This system fundamentally breaks the Deluge's traditional **linear playback convention**. Instead of playing clips linearly through time, sequencer modes introduce **pattern-based, non-linear playback** where the sequencer mode determines what gets played when, rather than simply following the timeline.

- **Traditional**: Timeline position → Direct playback of stored data
- **Sequencer Modes**: Timeline position → Pattern logic → Dynamic content generation

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Design Rationale](#design-rationale)
3. [Core Components](#core-components)
4. [Implementation Phases](#implementation-phases)
5. [Integration Points](#integration-points)
6. [Universal Track Support](#universal-track-support)
7. [Example Implementations](#example-implementations)
8. [Testing Strategy](#testing-strategy)
9. [Future Extensions](#future-extensions)

## Architecture Overview

The sequencer modes system follows the proven **keyboard layouts pattern** already established in the Deluge codebase. This approach provides:

- **Consistency**: Uses familiar patterns from `gui/ui/keyboard/layout/`
- **Modularity**: Each sequencer mode is a self-contained class
- **Extensibility**: Easy registration and discovery of new modes
- **RAII Design**: Automatic resource management with smart pointers
- **Universal Compatibility**: Works with all instrument types (Sound/MIDI/CV)

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Sequencer Mode System                    │
├─────────────────────────────────────────────────────────────┤
│  SequencerModeManager (RAII Factory & Registry)            │
├─────────────────────────────────────────────────────────────┤
│  SequencerMode (Abstract Base Class)                       │
│  ├─ InstrumentSequencerMode (Note-based)                   │
│  └─ AudioSequencerMode (Sample-based)                      │
├─────────────────────────────────────────────────────────────┤
│  Concrete Implementations:                                 │
│  • StepSequencerMode (Instrument & Audio variants)        │
│  • EuclideanSequencerMode (Instrument & Audio variants)   │
│  • GranularSequencerMode (Audio-specific)                 │
│  • SliceSequencerMode (Audio-specific)                    │
│  • GenerativeSequencerMode (Universal)                    │
├─────────────────────────────────────────────────────────────┤
│  Integration Layer:                                        │
│  • InstrumentClip (note-based playback processing)        │
│  • InstrumentClipView (note-based rendering)              │
│  • AudioClip (sample-based playback processing)           │
│  • AudioClipView (sample-based rendering)                 │
├─────────────────────────────────────────────────────────────┤
│  Universal Output Layer:                                   │
│  • Sound::noteOn/noteOff (Synth tracks)                   │
│  • MIDIInstrument::noteOn/noteOff (MIDI tracks)           │
│  • CVInstrument::noteOn/noteOff (CV tracks)               │
│  • AudioClip::render() (Audio tracks)                     │
└─────────────────────────────────────────────────────────────┘
```

## Design Rationale

### 1. **Breaking Linear Playback: The Core Innovation**

The Deluge's current architecture is fundamentally **linear**:

```cpp
// Current Linear Playback Model
void InstrumentClip::processCurrentPos(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) {
    // Find notes at current timeline position
    // Play those notes directly
    // Advance timeline position
}

void AudioClip::processCurrentPos(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) {
    // Calculate sample position from timeline position  
    // Output audio samples directly from that position
    // Advance timeline position
}
```

Our sequencer modes introduce **pattern-based logic** that intercepts this linear flow:

```cpp
// New Pattern-Based Playback Model
void InstrumentClip::processCurrentPos(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) {
    if (hasSequencerMode()) {
        // Let sequencer mode determine what to play based on its own pattern logic
        sequencerMode->processPlayback(modelStack, ticksSinceLast);
    } else {
        // Fall back to traditional linear playback
        processCurrentPosLinear(modelStack, ticksSinceLast);
    }
}
```

**This paradigm shift enables:**
- **Step sequencers**: Fixed-pattern rhythmic playback
- **Euclidean sequencers**: Mathematical rhythm generation  
- **Granular sequencers**: Non-linear audio fragment playback
- **Generative sequencers**: AI-driven dynamic content creation
- **Slice sequencers**: Audio chopping and rearrangement

**The timeline still advances linearly**, but **content generation becomes pattern-driven** rather than data-driven.

**Crucially**: Sequencer modes can **record their output back to linear clips**, enabling powerful hybrid workflows:
- Generate patterns algorithmically → Record to piano roll → Edit linearly
- Create granular audio textures → Record to new audio clip → Further process
- Use Euclidean rhythms → Capture variations → Build arrangements

### 2. **Why Follow the Keyboard Layout Pattern?**

The keyboard layout system (`gui/ui/keyboard/layout/`) demonstrates excellent architectural principles:

- **Polymorphic Design**: `KeyboardLayout` base class with concrete implementations
- **Clean Interface**: Well-defined virtual methods for input handling and rendering
- **State Management**: Each layout manages its own state independently
- **Easy Extension**: New layouts can be added without modifying existing code

### 2. **Why RAII?**

- **Automatic Resource Management**: Smart pointers handle memory automatically
- **Exception Safety**: Resources are cleaned up even if exceptions occur
- **Clear Ownership**: Explicit ownership semantics prevent memory leaks
- **Modern C++**: Follows contemporary C++ best practices

### 3. **Why Universal Track Support?**

The Deluge's instrument system already provides universal note routing:
- `Sound` instruments → synthesizer engine
- `MIDIInstrument` → MIDI output
- `CVInstrument` → CV/Gate output

By using the existing `noteOn/noteOff` interface, sequencer modes automatically work with all track types without additional code.

### 4. **Why Maintain Piano Roll as Master?**

- **Backward Compatibility**: Existing songs continue to work unchanged
- **User Familiarity**: Piano roll remains the primary editing interface
- **Data Persistence**: All note data is stored in the piano roll format
- **Flexibility**: Users can switch between modes without losing data

### 5. **Why Extend to Audio Tracks?**

**Creative Benefits:**
- **Performance Tools**: Real-time audio manipulation and sequencing
- **Live Looping**: Advanced loop-based performance capabilities
- **Granular Control**: Micro-level audio manipulation for texture and rhythm
- **Slice Sequencing**: Chop and rearrange audio in creative ways
- **Unified Workflow**: Consistent sequencer interface across all track types

**Technical Benefits:**
- **Leverages Existing Architecture**: Uses the same base classes and patterns
- **Minimal Code Duplication**: Shared infrastructure with instrument sequencers
- **Performance Optimized**: Direct audio manipulation without note conversion overhead
- **Future-Proof**: Extensible foundation for advanced audio processing modes

**User Experience Benefits:**
- **Consistent Interface**: Same pad layouts and encoder behaviors across track types
- **Seamless Switching**: Easy mode changes without workflow disruption
- **Creative Exploration**: New possibilities for audio-based composition and performance

### 6. **Bidirectional Recording: Pattern ↔ Linear Workflows**

A key innovation is the ability to **record sequencer mode output back to linear clips**, creating powerful hybrid workflows:

**Pattern → Linear Recording Workflows:**
- **Generative to Piano Roll**: AI generates melodies → Record best variations → Edit traditionally
- **Euclidean to Drums**: Mathematical rhythms → Capture variations → Use in arrangements  
- **Granular to Audio**: Texture generation → Record as new samples → Further processing
- **Step Sequencer to MIDI**: Pattern programming → Record to external gear → Import back

**Recording Implementation:**
```cpp
enum class RecordingTarget {
    PIANO_ROLL,        // Record notes to current instrument clip
    NEW_AUDIO_CLIP,    // Record audio output to new audio clip
    EXTERNAL_MIDI,     // Record MIDI to external sequencer
    ARRANGEMENT        // Record directly to arrangement view
};

class SequencerMode {
    virtual void startRecording(RecordingTarget target) {}
    virtual void stopRecording() {}
    virtual bool isRecording() const { return false; }
    
protected:
    void recordNoteIfRecording(int32_t noteCode, int32_t velocity, uint32_t pos);
    void recordAudioIfRecording(std::span<q31_t> audioData);
};
```

**"Render" Clip Mode - Dedicated Pattern-to-Linear Conversion:**

A special **"Render Mode"** that explicitly converts sequencer mode output to linear clips:

```cpp
class RenderMode {
public:
    enum class RenderTarget {
        REPLACE_CURRENT,    // Replace current clip with rendered version
        NEW_CLIP,          // Create new clip with rendered output  
        OVERDUB,           // Layer onto existing linear content
        BOUNCE_TO_AUDIO    // Render instrument clips to audio
    };
    
    struct RenderSettings {
        RenderTarget target;
        uint32_t renderLength;     // How many bars/beats to render
        bool includeAutomation;    // Render parameter changes too
        bool quantizeOutput;       // Quantize rendered notes/audio
        float fadeIn, fadeOut;     // Audio fade settings
    };
    
    void startRender(SequencerMode* sourceMode, RenderSettings settings);
    void processRender(ModelStackWithTimelineCounter* modelStack);
    bool isRenderComplete() const;
    Clip* getRenderedClip() const;
};
```

**Streamlined Recording Interface:**

**RECORD Button = Universal "Capture to Linear"**
- **Press RECORD** while in any sequencer mode → Start recording to master linear clip
- **Press RECORD again** → Stop recording  
- **SHIFT + RECORD** → Recording options (overdub, new clip, etc.)
- **Visual feedback** → Recording indicator shows capture in progress

```cpp
class InstrumentClip {
    void handleRecordButton(bool pressed) {
        if (hasSequencerMode()) {
            if (pressed) {
                // Start recording sequencer output to piano roll
                sequencerMode->startRecordingToLinear();
                showRecordingIndicator();
            } else {
                // Stop recording and return to sequencer mode
                sequencerMode->stopRecordingToLinear();
                hideRecordingIndicator();
            }
        } else {
            // Traditional linear recording behavior
            handleLinearRecording(pressed);
        }
    }
};

class AudioClip {
    void handleRecordButton(bool pressed) {
        if (hasSequencerMode()) {
            if (pressed) {
                // Start recording sequencer audio output to waveform
                sequencerMode->startRecordingToLinear();
                showRecordingIndicator();
            } else {
                // Stop recording and return to sequencer mode
                sequencerMode->stopRecordingToLinear();
                hideRecordingIndicator();
            }
        } else {
            // Traditional audio recording behavior
            handleLinearRecording(pressed);
        }
    }
};
```

**Seamless Recording Workflows:**

1. **Live Pattern Capture**
   ```
   Step Sequencer → Tweak pattern live → Press RECORD → Capture to piano roll
   ```

2. **Generative Exploration**
   ```  
   AI Sequencer → Generate interesting variation → Press RECORD → Save to linear
   ```

3. **Audio Texture Creation**
   ```
   Granular Mode → Create perfect texture → Press RECORD → Capture to waveform
   ```

4. **Performance Recording**
   ```
   Any sequencer mode → Perform with encoders/pads → Press RECORD → Capture performance
   ```

**Recording Behavior by Mode:**

| Sequencer Mode | RECORD Button Action | Output Destination |
|----------------|---------------------|-------------------|
| Step Sequencer | Records note triggers | Piano roll (master clip) |
| Euclidean | Records generated rhythm | Piano roll (master clip) |
| Generative | Records AI output | Piano roll (master clip) |
| Granular | Records audio output | Waveform (master clip) |
| Slice Sequencer | Records slice playback | Waveform (master clip) |
| Any Mode + SHIFT | Shows recording options | User choice |

**Advanced Render Features:**
- **Multi-pass rendering**: Render multiple pattern variations automatically
- **Conditional rendering**: "Render only when pattern does X"
- **Batch rendering**: Render all sequencer clips in song to linear
- **Stem rendering**: Render each drum/instrument separately

**Creative Benefits:**
- **Capture happy accidents**: Record unexpected algorithmic variations
- **Build arrangements**: Use pattern-generated clips in traditional song structure  
- **Layer complexity**: Combine multiple recorded pattern variations
- **Performance recording**: Capture real-time parameter manipulation of patterns

## Core Components

### 1. **SequencerMode Base Class**

```cpp
namespace deluge::sequencer {

class SequencerMode {
public:
    virtual ~SequencerMode() = default;
    
    // Core Interface (following KeyboardLayout pattern)
    virtual void evaluatePads(PressedPad presses[kMaxNumPadPresses]) = 0;
    virtual void handleVerticalEncoder(int32_t offset) = 0;
    virtual void handleHorizontalEncoder(int32_t offset, bool shiftEnabled) = 0;
    virtual void renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) = 0;
    virtual void renderSidebar(RGB image[][kDisplayWidth + kSideBarWidth]) = 0;
    
    // Sequencer-Specific Interface
    virtual void processPlayback(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) = 0;
    virtual void generateNotes(NoteEventBuffer& noteBuffer, uint32_t currentPos, uint32_t ticksToProcess) = 0;
    virtual void reset() = 0;
    virtual void initialize(InstrumentClip* clip) = 0;
    
    // Properties
    virtual l10n::String name() = 0;
    virtual bool supportsInstrument() { return true; }
    virtual bool supportsKit() { return true; }
    virtual bool supportsMIDI() { return true; }
    virtual bool supportsCV() { return true; }
    
    // Piano Roll Integration & Recording
    virtual bool canRecordToPianoRoll() { return true; }
    virtual void syncFromPianoRoll(InstrumentClip* clip) {}
    virtual void syncToPianoRoll(InstrumentClip* clip) {}
    
    // Recording Output Back to Linear Clips
    virtual void startRecording(RecordingTarget target) {}
    virtual void stopRecording() {}
    virtual bool isRecording() const { return false; }
    
protected:
    // Helper methods for note output
    void sendNoteOn(int32_t noteCode, int32_t velocity, ModelStackWithTimelineCounter* modelStack);
    void sendNoteOff(int32_t noteCode, ModelStackWithTimelineCounter* modelStack);
    void recordToPianoRoll(int32_t noteCode, int32_t velocity, uint32_t pos, ModelStackWithTimelineCounter* modelStack);
    
    // State access helpers
    InstrumentClip* getCurrentClip() const;
    Instrument* getCurrentInstrument() const;
    OutputType getCurrentOutputType() const;
};

} // namespace deluge::sequencer
```

### 2. **SequencerModeManager (RAII Factory)**

```cpp
namespace deluge::sequencer {

class SequencerModeManager {
public:
    static SequencerModeManager& instance() {
        static SequencerModeManager instance;
        return instance;
    }
    
    // RAII factory methods
    template<typename T>
    void registerMode(const std::string& name) {
        static_assert(std::is_base_of_v<SequencerMode, T>);
        factories_[name] = []() { return std::make_unique<T>(); };
        modeNames_.push_back(name);
    }
    
    std::unique_ptr<SequencerMode> createMode(const std::string& name) {
        auto it = factories_.find(name);
        return it != factories_.end() ? it->second() : nullptr;
    }
    
    const std::vector<std::string>& getAvailableModes() const {
        return modeNames_;
    }
    
    bool isValidMode(const std::string& name) const {
        return factories_.find(name) != factories_.end();
    }
    
private:
    std::map<std::string, std::function<std::unique_ptr<SequencerMode>()>> factories_;
    std::vector<std::string> modeNames_;
};

// Convenient registration macro
#define REGISTER_SEQUENCER_MODE(ClassName, ModeName) \
    namespace { \
        static auto registered_##ClassName = []() { \
            SequencerModeManager::instance().registerMode<ClassName>(ModeName); \
            return true; \
        }(); \
    }

} // namespace deluge::sequencer
```

### 3. **AudioSequencerMode Base Class (for audio-specific modes)**

```cpp
namespace deluge::sequencer {

class AudioSequencerMode : public SequencerMode {
public:
    // Audio-specific interface
    virtual void processAudioPlayback(AudioClip* audioClip, ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) = 0;
    virtual void renderAudioBuffer(AudioClip* audioClip, std::span<q31_t> output, int32_t amplitude, int32_t amplitudeIncrement) = 0;
    
    // Override base methods to work with audio clips
    void processPlayback(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) override {
        auto audioClip = dynamic_cast<AudioClip*>(getCurrentClip());
        if (audioClip) {
            processAudioPlayback(audioClip, modelStack, ticksSinceLast);
        }
    }
    
    void generateNotes(NoteEventBuffer& noteBuffer, uint32_t currentPos, uint32_t ticksToProcess) override {
        // Audio modes don't generate note events, they manipulate audio directly
        // This method is not used for audio sequencer modes
    }
    
    // Piano roll integration not applicable for audio modes
    bool canRecordToPianoRoll() override { return false; }
    void syncFromPianoRoll(InstrumentClip* clip) override {}
    void syncToPianoRoll(InstrumentClip* clip) override {}
    
    // Audio modes only support audio tracks
    bool supportsInstrument() override { return false; }
    bool supportsMIDI() override { return false; }
    bool supportsCV() override { return false; }
    
protected:
    // Helper methods for audio manipulation
    void triggerAudioSegment(AudioClip* audioClip, float startPos, float endPos, float pitch = 1.0f, float volume = 1.0f);
    void setAudioPlaybackPosition(AudioClip* audioClip, float position); // 0.0 to 1.0
    void setAudioPlaybackSpeed(AudioClip* audioClip, float speed);
    void applyAudioEffect(AudioClip* audioClip, AudioEffect effect, float intensity);
    
    // Audio analysis helpers
    std::vector<float> detectTransients(AudioClip* audioClip, float threshold = 0.1f);
    float getAudioLevel(AudioClip* audioClip, float position);
    float getAudioSpectralCentroid(AudioClip* audioClip, float position, int32_t windowSize = 1024);
};

class InstrumentSequencerMode : public SequencerMode {
public:
    // Standard note-based sequencing (existing implementation)
    // This is the base class for modes that work with instrument clips
    
    // Audio modes don't apply to instrument sequencers
    bool supportsAudio() { return false; }
    
protected:
    // All the existing helper methods for note output remain the same
    using SequencerMode::sendNoteOn;
    using SequencerMode::sendNoteOff;
    using SequencerMode::recordToPianoRoll;
};

} // namespace deluge::sequencer
```

### 4. **NoteEventBuffer (for efficient note scheduling)**

```cpp
namespace deluge::sequencer {

struct NoteEvent {
    enum Type { NOTE_ON, NOTE_OFF };
    Type type;
    int32_t noteCode;
    int32_t velocity;
    uint32_t timestamp;
};

class NoteEventBuffer {
public:
    void addNoteOn(int32_t noteCode, int32_t velocity, uint32_t timestamp);
    void addNoteOff(int32_t noteCode, uint32_t timestamp);
    void clear();
    void processEvents(ModelStackWithTimelineCounter* modelStack, uint32_t currentTime);
    
private:
    std::vector<NoteEvent> events_;
    void sortEventsByTime();
};

} // namespace deluge::sequencer
```

## Implementation Phases

### Phase 1: Core Infrastructure (Week 1-2)

**Deliverables:**
- `SequencerMode` base class
- `SequencerModeManager` RAII factory
- `NoteEventBuffer` for event scheduling
- Basic integration points in `InstrumentClip` and `InstrumentClipView`
- Unit tests for core components

**Files to Create:**
```
src/deluge/sequencer/
├── sequencer_mode.h
├── sequencer_mode.cpp
├── sequencer_mode_manager.h
├── sequencer_mode_manager.cpp
├── note_event_buffer.h
└── note_event_buffer.cpp
```

**Files to Modify:**
```
src/deluge/model/clip/instrument_clip.h
src/deluge/model/clip/instrument_clip.cpp
src/deluge/gui/views/instrument_clip_view.h
src/deluge/gui/views/instrument_clip_view.cpp
```

### Phase 2: Basic Step Sequencer (Week 3)

**Deliverables:**
- Complete step sequencer implementation
- Pad interaction handling
- Visual rendering
- Mode switching UI

**Files to Create:**
```
src/deluge/sequencer/modes/
├── step_sequencer_mode.h
└── step_sequencer_mode.cpp
```

### Phase 3: Piano Roll Integration (Week 4)

**Deliverables:**
- Bidirectional sync with piano roll
- Recording capabilities
- Mode persistence in song files
- Comprehensive testing

**Files to Modify:**
```
src/deluge/storage/storage_manager.cpp  (for persistence)
src/deluge/gui/ui/menus.cpp            (for mode selection UI)
```

### Phase 4: Advanced Sequencer Modes (Week 5-6)

**Deliverables:**
- Euclidean sequencer
- Generative/algorithmic sequencer
- Performance optimizations
- Documentation

## Integration Points

### 1. **InstrumentClip Integration**

```cpp
class InstrumentClip : public Clip {
public:
    // Add sequencer mode support
    void setSequencerMode(const std::string& modeName);
    void clearSequencerMode();
    bool hasAlternativeSequencer() const { return sequencerMode_ != nullptr; }
    SequencerMode* getSequencerMode() const { return sequencerMode_.get(); }
    
    // Modified playback processing
    void processCurrentPos(ModelStackWithTimelineCounter* modelStack, uint32_t posIncrement) override;
    
private:
    std::unique_ptr<deluge::sequencer::SequencerMode> sequencerMode_;
    std::string sequencerModeName_; // For persistence
    
    // Original method renamed for fallback
    void processCurrentPosOriginal(ModelStackWithTimelineCounter* modelStack, uint32_t posIncrement);
};
```

### 2. **InstrumentClipView Integration**

```cpp
class InstrumentClipView : public ClipView {
public:
    // Modified rendering
    bool renderMainPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                        uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
                        bool drawUndefinedArea = false) override;
    
    bool renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                       uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override;
    
    // Modified input handling
    ActionResult padAction(int32_t x, int32_t y, int32_t velocity) override;
    ActionResult verticalEncoderAction(int32_t offset, bool inCardRoutine) override;
    ActionResult horizontalEncoderAction(int32_t offset) override;
    
    // Mode switching
    void enterSequencerMode(const std::string& modeName);
    void exitSequencerMode();
    
private:
    bool useSequencerModeRendering() const;
    bool useSequencerModeInput() const;
};
```

### 3. **Menu System Integration**

Add sequencer mode selection to the clip settings menu:

```cpp
// In menus.cpp
MenuItem sequencerModeMenu[] = {
    { "PIANO ROLL", selectPianoRollMode },
    { "STEP SEQ", selectStepSequencerMode },
    { "EUCLIDEAN", selectEuclideanMode },
    { "GENERATIVE", selectGenerativeMode },
    // ... more modes
};
```

## Universal Track Support

### How It Works

The beauty of this system is that it leverages the Deluge's existing instrument architecture. Each instrument type already knows how to handle `noteOn/noteOff` calls:

```cpp
void SequencerMode::sendNoteOn(int32_t noteCode, int32_t velocity, ModelStackWithTimelineCounter* modelStack) {
    auto instrument = getCurrentInstrument();
    
    // This automatically routes to the correct implementation:
    // - Sound instruments → synthesizer engine
    // - MIDIInstrument → MIDI output  
    // - CVInstrument → CV/Gate output
    instrument->noteOn(modelStack, noteCode, velocity);
}
```

### Track Type Compatibility Matrix

| Sequencer Mode | Sound Tracks | MIDI Tracks | CV Tracks | Kit Tracks | Audio Tracks |
|----------------|--------------|-------------|-----------|------------|--------------|
| Step Sequencer | ✅ | ✅ | ✅ | ✅ | ✅ |
| Euclidean      | ✅ | ✅ | ✅ | ✅ | ✅ |
| Generative     | ✅ | ✅ | ✅ | ⚠️* | ✅ |
| Granular       | ❌ | ❌ | ❌ | ❌ | ✅ |
| Slice Sequencer| ❌ | ❌ | ❌ | ❌ | ✅ |
| Stutter/Glitch | ❌ | ❌ | ❌ | ❌ | ✅ |
| Piano Roll     | ✅ | ✅ | ✅ | ✅ | ❌ |
| Waveform View  | ❌ | ❌ | ❌ | ❌ | ✅ |

*Kit track support may be limited for some generative modes depending on implementation.
**Audio-specific modes work only with audio tracks, while universal modes work with all track types.

## Example Implementations

### 1. **Step Sequencer Mode**

```cpp
class StepSequencerMode : public SequencerMode {
public:
    StepSequencerMode() : steps_(16), currentStep_(0), stepLength_(kSixteenthNote) {
        stepData_.resize(steps_);
        for (auto& step : stepData_) {
            step.active = false;
            step.velocity = 64;
            step.note = 60; // Middle C
            step.probability = 100;
        }
    }
    
    void processPlayback(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) override {
        uint32_t currentPos = modelStack->timelineCounter->getLivePos();
        uint32_t newStep = (currentPos / stepLength_) % steps_;
        
        if (newStep != currentStep_) {
            // Handle step change
            handleStepChange(newStep, modelStack);
            currentStep_ = newStep;
        }
    }
    
    void evaluatePads(PressedPad presses[kMaxNumPadPresses]) override {
        for (int32_t i = 0; i < kMaxNumPadPresses; ++i) {
            if (presses[i].active && presses[i].x < steps_ && presses[i].y == 0) {
                // Toggle step
                stepData_[presses[i].x].active = !stepData_[presses[i].x].active;
            }
        }
    }
    
    void renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) override {
        // Clear display
        for (int y = 0; y < kDisplayHeight; y++) {
            for (int x = 0; x < kDisplayWidth; x++) {
                image[y][x] = colours::black;
            }
        }
        
        // Render steps
        for (int x = 0; x < std::min(steps_, kDisplayWidth); x++) {
            if (stepData_[x].active) {
                image[0][x] = colours::green;
            }
            if (x == currentStep_) {
                image[0][x] = colours::white; // Current step indicator
            }
        }
    }
    
    l10n::String name() override { return l10n::String::STRING_FOR_STEP_SEQUENCER; }
    
private:
    struct StepData {
        bool active;
        int32_t note;
        int32_t velocity;
        int32_t probability;
    };
    
    std::vector<StepData> stepData_;
    int32_t steps_;
    int32_t currentStep_;
    uint32_t stepLength_;
    
    void handleStepChange(int32_t newStep, ModelStackWithTimelineCounter* modelStack);
};

// Register the mode
REGISTER_SEQUENCER_MODE(StepSequencerMode, "step_sequencer");
```

### 2. **Euclidean Sequencer Mode**

```cpp
class EuclideanSequencerMode : public SequencerMode {
public:
    EuclideanSequencerMode() : steps_(16), pulses_(4), rotation_(0) {
        generateEuclideanPattern();
    }
    
    void generateEuclideanPattern() {
        pattern_.clear();
        pattern_.resize(steps_, false);
        
        // Euclidean algorithm implementation
        if (pulses_ == 0) return;
        
        std::vector<int> bucket(steps_, 0);
        for (int i = 0; i < steps_; i++) {
            bucket[i] = (pulses_ * (i + 1)) / steps_ - (pulses_ * i) / steps_;
        }
        
        for (int i = 0; i < steps_; i++) {
            pattern_[(i + rotation_) % steps_] = (bucket[i] > 0);
        }
    }
    
    void handleVerticalEncoder(int32_t offset) override {
        pulses_ = std::clamp(pulses_ + offset, 0, steps_);
        generateEuclideanPattern();
    }
    
    void handleHorizontalEncoder(int32_t offset, bool shiftEnabled) override {
        if (shiftEnabled) {
            rotation_ = (rotation_ + offset + steps_) % steps_;
        } else {
            steps_ = std::clamp(steps_ + offset, 1, 32);
            pulses_ = std::min(pulses_, steps_);
        }
        generateEuclideanPattern();
    }
    
    l10n::String name() override { return l10n::String::STRING_FOR_EUCLIDEAN_SEQUENCER; }
    
private:
    std::vector<bool> pattern_;
    int32_t steps_;
    int32_t pulses_;
    int32_t rotation_;
    int32_t currentStep_ = 0;
};

REGISTER_SEQUENCER_MODE(EuclideanSequencerMode, "euclidean_sequencer");
```

### 3. **Audio-Specific: Granular Sequencer Mode**

```cpp
class GranularSequencerMode : public AudioSequencerMode {
public:
    GranularSequencerMode() : grainSize_(1000), grainDensity_(8), grainPosition_(0.5f) {
        // Initialize grain parameters
        for (auto& grain : grains_) {
            grain.active = false;
            grain.position = 0.0f;
            grain.size = grainSize_;
            grain.pitch = 1.0f;
            grain.pan = 0.0f;
        }
    }
    
    void processPlayback(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) override {
        auto audioClip = static_cast<AudioClip*>(getCurrentClip());
        if (!audioClip || !audioClip->sampleHolder.audioFile) return;
        
        // Calculate grain triggers based on density and timing
        uint32_t currentPos = modelStack->timelineCounter->getLivePos();
        uint32_t grainInterval = (kSampleRate * 60) / (currentSong->timePerTimerTick * grainDensity_);
        
        if ((currentPos % grainInterval) == 0) {
            triggerGrain(audioClip, modelStack);
        }
        
        // Update active grains
        updateActiveGrains(audioClip, modelStack);
    }
    
    void evaluatePads(PressedPad presses[kMaxNumPadPresses]) override {
        for (int32_t i = 0; i < kMaxNumPadPresses; ++i) {
            if (presses[i].active) {
                if (presses[i].y == 0) {
                    // Top row: grain position (0.0 to 1.0 across sample)
                    grainPosition_ = static_cast<float>(presses[i].x) / kDisplayWidth;
                }
                else if (presses[i].y == 1) {
                    // Second row: grain size
                    grainSize_ = 100 + (presses[i].x * 100); // 100ms to 1.6s
                }
                else if (presses[i].y == 2) {
                    // Third row: grain density
                    grainDensity_ = 1 + presses[i].x; // 1 to 16 grains per beat
                }
            }
        }
    }
    
    void renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) override {
        // Clear display
        for (int y = 0; y < kDisplayHeight; y++) {
            for (int x = 0; x < kDisplayWidth; x++) {
                image[y][x] = colours::black;
            }
        }
        
        // Row 0: Grain position indicator
        int32_t posX = static_cast<int32_t>(grainPosition_ * kDisplayWidth);
        if (posX < kDisplayWidth) {
            image[0][posX] = colours::blue;
        }
        
        // Row 1: Grain size visualization
        int32_t sizeSteps = (grainSize_ - 100) / 100;
        for (int x = 0; x < std::min(sizeSteps, kDisplayWidth); x++) {
            image[1][x] = colours::green;
        }
        
        // Row 2: Grain density visualization
        for (int x = 0; x < std::min(grainDensity_, kDisplayWidth); x++) {
            image[2][x] = colours::red;
        }
        
        // Show active grains
        for (const auto& grain : grains_) {
            if (grain.active) {
                int32_t grainX = static_cast<int32_t>(grain.position * kDisplayWidth);
                if (grainX < kDisplayWidth) {
                    image[3][grainX] = colours::white;
                }
            }
        }
    }
    
    l10n::String name() override { return l10n::String::STRING_FOR_GRANULAR_SEQUENCER; }
    bool supportsInstrument() override { return false; }
    bool supportsKit() override { return false; }
    bool supportsMIDI() override { return false; }
    bool supportsCV() override { return false; }
    
private:
    struct Grain {
        bool active;
        float position;     // Position in sample (0.0 to 1.0)
        int32_t size;       // Size in samples
        float pitch;        // Pitch multiplier
        float pan;          // Pan position
        int32_t playhead;   // Current playback position within grain
    };
    
    std::array<Grain, 32> grains_;
    int32_t grainSize_;     // Size in milliseconds
    int32_t grainDensity_;  // Grains per beat
    float grainPosition_;   // Position in sample (0.0 to 1.0)
    
    void triggerGrain(AudioClip* audioClip, ModelStackWithTimelineCounter* modelStack);
    void updateActiveGrains(AudioClip* audioClip, ModelStackWithTimelineCounter* modelStack);
};

REGISTER_SEQUENCER_MODE(GranularSequencerMode, "granular_sequencer");
```

### 4. **Audio-Specific: Slice Sequencer Mode**

```cpp
class SliceSequencerMode : public AudioSequencerMode {
public:
    SliceSequencerMode() : numSlices_(16), currentStep_(0) {
        // Initialize slice pattern
        slicePattern_.resize(16);
        for (int i = 0; i < 16; i++) {
            slicePattern_[i] = i % numSlices_; // Default: play slices in order
        }
        
        // Auto-detect slices from audio transients (simplified)
        detectSlices();
    }
    
    void processPlayback(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) override {
        auto audioClip = static_cast<AudioClip*>(getCurrentClip());
        if (!audioClip) return;
        
        uint32_t currentPos = modelStack->timelineCounter->getLivePos();
        uint32_t stepLength = kSixteenthNote; // 16th note steps
        uint32_t newStep = (currentPos / stepLength) % slicePattern_.size();
        
        if (newStep != currentStep_) {
            // Trigger new slice
            int32_t sliceIndex = slicePattern_[newStep];
            if (sliceIndex >= 0 && sliceIndex < numSlices_) {
                triggerSlice(audioClip, sliceIndex, modelStack);
            }
            currentStep_ = newStep;
        }
    }
    
    void evaluatePads(PressedPad presses[kMaxNumPadPresses]) override {
        for (int32_t i = 0; i < kMaxNumPadPresses; ++i) {
            if (presses[i].active && presses[i].x < slicePattern_.size()) {
                if (presses[i].y == 0) {
                    // Top row: slice pattern (which slice to play at each step)
                    slicePattern_[presses[i].x] = (slicePattern_[presses[i].x] + 1) % numSlices_;
                }
                else if (presses[i].y == 1) {
                    // Second row: mute/unmute steps
                    slicePattern_[presses[i].x] = (slicePattern_[presses[i].x] == -1) ? 0 : -1;
                }
            }
        }
    }
    
    void renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) override {
        // Clear display
        for (int y = 0; y < kDisplayHeight; y++) {
            for (int x = 0; x < kDisplayWidth; x++) {
                image[y][x] = colours::black;
            }
        }
        
        // Row 0: Slice pattern
        for (int x = 0; x < std::min(static_cast<int>(slicePattern_.size()), kDisplayWidth); x++) {
            if (slicePattern_[x] >= 0) {
                // Color based on slice number
                RGB sliceColor = getSliceColor(slicePattern_[x]);
                image[0][x] = sliceColor;
                
                if (x == currentStep_) {
                    image[0][x] = colours::white; // Current step indicator
                }
            }
        }
        
        // Row 1: Step mute status
        for (int x = 0; x < std::min(static_cast<int>(slicePattern_.size()), kDisplayWidth); x++) {
            if (slicePattern_[x] == -1) {
                image[1][x] = colours::red; // Muted step
            }
        }
        
        // Rows 2-7: Slice visualization (show slice boundaries)
        renderSliceVisualization(image);
    }
    
    l10n::String name() override { return l10n::String::STRING_FOR_SLICE_SEQUENCER; }
    bool supportsInstrument() override { return false; }
    bool supportsKit() override { return false; }
    bool supportsMIDI() override { return false; }
    bool supportsCV() override { return false; }
    
private:
    std::vector<int32_t> slicePattern_; // -1 = muted, 0+ = slice index
    int32_t numSlices_;
    int32_t currentStep_;
    std::vector<float> slicePositions_; // Normalized positions (0.0 to 1.0)
    
    void detectSlices();
    void triggerSlice(AudioClip* audioClip, int32_t sliceIndex, ModelStackWithTimelineCounter* modelStack);
    RGB getSliceColor(int32_t sliceIndex);
    void renderSliceVisualization(RGB image[][kDisplayWidth + kSideBarWidth]);
};

REGISTER_SEQUENCER_MODE(SliceSequencerMode, "slice_sequencer");
```

## Testing Strategy

### 1. **Unit Tests**

```cpp
// Test sequencer mode registration
TEST(SequencerModeManagerTest, RegisterAndCreateModes) {
    auto& manager = SequencerModeManager::instance();
    
    // Test registration
    manager.registerMode<MockSequencerMode>("test_mode");
    EXPECT_TRUE(manager.isValidMode("test_mode"));
    
    // Test creation
    auto mode = manager.createMode("test_mode");
    ASSERT_NE(mode, nullptr);
    EXPECT_EQ(mode->name(), "Test Mode");
}

// Test note event buffer
TEST(NoteEventBufferTest, EventOrdering) {
    NoteEventBuffer buffer;
    buffer.addNoteOn(60, 100, 1000);
    buffer.addNoteOff(60, 1500);
    buffer.addNoteOn(64, 100, 1200);
    
    // Events should be processed in chronological order
    MockModelStack modelStack;
    buffer.processEvents(&modelStack, 2000);
    
    // Verify correct order of note events
    EXPECT_EQ(modelStack.noteEvents.size(), 3);
    EXPECT_EQ(modelStack.noteEvents[0].timestamp, 1000);
    EXPECT_EQ(modelStack.noteEvents[1].timestamp, 1200);
    EXPECT_EQ(modelStack.noteEvents[2].timestamp, 1500);
}
```

### 2. **Integration Tests**

```cpp
// Test with different instrument types
TEST(SequencerModeIntegrationTest, UniversalInstrumentSupport) {
    // Test with Sound instrument
    testSequencerWithInstrument(OutputType::SYNTH);
    
    // Test with MIDI instrument  
    testSequencerWithInstrument(OutputType::MIDI_OUT);
    
    // Test with CV instrument
    testSequencerWithInstrument(OutputType::CV);
}

// Test piano roll integration
TEST(SequencerModeIntegrationTest, PianoRollSync) {
    auto clip = createTestClip();
    auto stepSequencer = std::make_unique<StepSequencerMode>();
    
    // Add notes to piano roll
    addNoteToPianoRoll(clip, 60, 100, 0);
    addNoteToPianoRoll(clip, 64, 100, 960);
    
    // Sync to step sequencer
    stepSequencer->syncFromPianoRoll(clip);
    
    // Verify step sequencer has correct pattern
    EXPECT_TRUE(stepSequencer->isStepActive(0));
    EXPECT_TRUE(stepSequencer->isStepActive(4));
}
```

### 3. **Performance Tests**

```cpp
TEST(SequencerModePerformanceTest, RealTimeConstraints) {
    auto stepSequencer = std::make_unique<StepSequencerMode>();
    MockModelStack modelStack;
    
    // Measure processing time
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; i++) {
        stepSequencer->processPlayback(&modelStack, 32); // ~1ms at 44.1kHz
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should complete well under real-time constraints
    EXPECT_LT(duration.count(), 10000); // 10ms for 1000 iterations
}
```

## Future Extensions

### 1. **Additional Sequencer Modes**

**Instrument-Based Modes:**
- **Probability Sequencer**: Steps with probability percentages
- **Polyrhythmic Sequencer**: Multiple independent rhythm tracks
- **Markov Chain Sequencer**: AI-driven note generation
- **Chord Progression Sequencer**: Harmonic sequence programming
- **Microtonal Sequencer**: Non-12TET scale support

**Audio-Based Modes:**
- **Granular Sequencer**: Micro-timing and texture control ✅ (already designed)
- **Slice Sequencer**: Audio chopping and rearrangement ✅ (already designed)
- **Stutter/Glitch Sequencer**: Rhythmic audio effects and stutters
- **Spectral Sequencer**: Frequency-domain manipulation
- **Live Looper Mode**: Real-time recording and overdubbing
- **Convolution Sequencer**: Impulse response-based audio processing
- **Formant Sequencer**: Vocal formant manipulation
- **Tape Stop/Start Sequencer**: Analog tape machine effects

**Universal Modes:**
- **Generative Sequencer**: AI-driven pattern generation (works with both note and audio data)
- **Gesture Sequencer**: Motion-based control recording and playback
- **Probability Matrix**: Complex probability relationships between elements

### 2. **Advanced Features**

- **Mode Chaining**: Automatically switch between modes
- **Parameter Automation**: Automate sequencer parameters
- **MIDI Learn**: Map external controllers to sequencer parameters
- **Preset System**: Save and recall sequencer configurations
- **Performance Mode**: Real-time parameter control

### 3. **User Interface Enhancements**

- **Visual Feedback**: Enhanced LED patterns for each mode
- **Context Menus**: Mode-specific parameter access
- **Gesture Control**: Multi-touch pad interactions
- **External Display**: Detailed parameter visualization on OLED

### 4. **Integration Possibilities**

- **Song Arrangement**: Sequencer modes in arrangement view
- **Cross-Clip Sync**: Synchronize patterns across multiple clips
- **Template System**: Pre-configured sequencer setups
- **Export/Import**: Share sequencer patterns between users

## Conclusion

This implementation plan provides a robust, extensible foundation for adding diverse sequencer modes to the Deluge while maintaining full compatibility with existing functionality. The RAII design ensures clean resource management, the universal instrument support eliminates code duplication, and the modular architecture allows for easy extension with new sequencer types.

The phased approach ensures steady progress with testable milestones, while the comprehensive testing strategy guarantees reliability and performance. By following the established keyboard layout pattern, we maintain consistency with the existing codebase and leverage proven architectural decisions.

This system will significantly expand the Deluge's creative possibilities while preserving the intuitive workflow that users expect.
