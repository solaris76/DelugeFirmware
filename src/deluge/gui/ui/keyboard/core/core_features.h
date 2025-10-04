#pragma once

#include "gui/l10n/l10n.h"
#include "hid/led/pad_leds.h"
#include "model/global_effectable/global_effectable_for_clip.h"
#include "modulation/arpeggiator.h"
#include <cstdint>
#include <memory>

namespace deluge::gui::ui::keyboard::core {

// Forward declarations
class TimingController;
class DisplayController;
class AudioController;
class ArpeggiatorController;
class EffectsController;
class ScaleController;

/**
 * Modern C++ Core Features API
 *
 * Benefits:
 * - RAII compliant
 * - Dependency injection friendly
 * - Easy to test
 * - Clear ownership semantics
 * - Thread-safe design
 */
class CoreFeatures {
public:
	// Factory method for creating instances
	static std::unique_ptr<CoreFeatures> create();

	// Controllers provide clean, focused interfaces
	TimingController& timing() { return *timing_; }
	DisplayController& display() { return *display_; }
	AudioController& audio() { return *audio_; }
	ArpeggiatorController& arpeggiator() { return *arpeggiator_; }
	EffectsController& effects() { return *effects_; }
	ScaleController& scale() { return *scale_; }

	// Const access for read-only operations
	const TimingController& timing() const { return *timing_; }
	const DisplayController& display() const { return *display_; }
	const AudioController& audio() const { return *audio_; }
	const ArpeggiatorController& arpeggiator() const { return *arpeggiator_; }
	const EffectsController& effects() const { return *effects_; }
	const ScaleController& scale() const { return *scale_; }

private:
	CoreFeatures(); // Private constructor - use create()

	std::unique_ptr<TimingController> timing_;
	std::unique_ptr<DisplayController> display_;
	std::unique_ptr<AudioController> audio_;
	std::unique_ptr<ArpeggiatorController> arpeggiator_;
	std::unique_ptr<EffectsController> effects_;
	std::unique_ptr<ScaleController> scale_;
};

/**
 * Timing Controller - RAII compliant, focused responsibility
 */
class TimingController {
public:
	// Direct property access - much simpler!
	bool playing = false;
	int32_t currentTick = 0;
	int32_t bpm = 120;
	bool paused = false;
	bool stopped = true;

	// Methods for Deluge integration
	bool isPlaying() const;
	int32_t getCurrentTick() const;
	int32_t getCurrentBPM() const;
	void setBPM(int32_t bpm);
	void play();
	void pause();
	void stop();
	void doTickForward();
	void doTickBackward();

private:
	// Internal state management
	bool playing_ = false;
	int32_t currentTick_ = 0;
	int32_t bpm_ = 120;
};

/**
 * Display Controller - Clean interface for display operations
 */
class DisplayController {
public:
	// Direct property access - much simpler!
	bool needsRendering = false;
	int32_t padColors[16][8]; // Store current pad colors
	bool padStates[16][8];    // Store current pad states
	int32_t popupText[32];    // Store popup text
	bool popupVisible = false;

	// Methods for Deluge integration
	void requestRendering();
	void setPadLED(int32_t x, int32_t y, int32_t color);
	void setPadLEDBrightness(int32_t x, int32_t y, int32_t brightness);
	void clearAllPads();
	void showPopup(const char* message);
	void showPopup(int32_t number);

	// Fader functionality
	class Fader {
	public:
		// Direct property access for fader state
		bool horizontalActive = false;
		bool verticalActive = false;
		bool shortHorizontalActive = false;
		int32_t horizontalStartX = 0;
		int32_t verticalStartY = 0;
		int32_t shortHorizontalStartX = 0;
		uint32_t dimmedColor = 0x404040;
		uint32_t litColor = 0xFFFFFF;

		// Horizontal fader (y0-7, x0-15)
		void setHorizontalFader(int32_t startX, int32_t y, int32_t value, int32_t maxValue, uint32_t dimmedColor,
		                        uint32_t litColor);

		// Vertical fader (x0-7, y0-15)
		void setVerticalFader(int32_t x, int32_t startY, int32_t value, int32_t maxValue, uint32_t dimmedColor,
		                      uint32_t litColor);

		// Short horizontal fader (x0-7 only)
		void setShortHorizontalFader(int32_t startX, int32_t y, int32_t value, int32_t maxValue, uint32_t dimmedColor,
		                             uint32_t litColor);

		// Get fader value from pad position
		int32_t getValueFromHorizontalFader(int32_t startX, int32_t y, int32_t padX, int32_t maxValue);
		int32_t getValueFromVerticalFader(int32_t x, int32_t startY, int32_t padY, int32_t maxValue);
		int32_t getValueFromShortHorizontalFader(int32_t startX, int32_t y, int32_t padX, int32_t maxValue);

	private:
		Fader() = default;
		friend class DisplayController;
	};

	Fader& fader() { return fader_; }

private:
	// Display state management
	bool needsRendering_ = false;
	Fader fader_;
};

/**
 * Audio Controller - Handles audio operations
 */
class AudioController {
public:
	// Direct property access - much simpler!
	OutputType outputType = OutputType::NONE;
	bool hasEffects = false;
	bool isValidClip = false;
	int32_t currentNote = 0;
	uint8_t currentVelocity = 0;

	// Methods for Deluge integration
	bool isCurrentClipInstrument() const;
	bool isCurrentClipAudio() const;
	OutputType getCurrentClipOutputType() const;
	void sendNoteToCurrentInstrument(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel = 0);
	void stopNoteOnCurrentInstrument(int32_t noteCode, int32_t fromMIDIChannel = 0);
	class InstrumentClip* getCurrentInstrumentClip() const;
	void renderWaveform(RGB image[][kDisplayWidth + kSideBarWidth],
	                    uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], int32_t xScroll, int32_t xZoom,
	                    int32_t whichKernel, int32_t whichKernelStartedThis);
	bool isAudioClip() const;

	// Helper functions for internal use
	bool hasEffectsSupport() const;
	bool isValidInstrumentClip() const;

private:
	// Audio state management
	bool audioClipActive_ = false;
};

/**
 * Arpeggiator Controller - Manages arpeggiator state and operations
 */
class ArpeggiatorController {
public:
	// Direct property access - much simpler!
	ArpeggiatorSettings settings;
	bool enabled = false;
	bool gateActive = false;
	int32_t currentNote = 0;
	int32_t currentOctave = 0;
	int32_t activeNoteCount = 0;

	// High-level operations
	void addNote(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel = 0);
	void removeNote(int32_t noteCode);
	void clearNotes();
	void reset();
	void triggerStep();

	// State queries (for Deluge integration)
	int32_t getActiveNoteCount() const;
	bool isNoteActive(int32_t noteCode) const;
	bool isGateActive() const;
	int32_t getCurrentNote() const;
	int32_t getCurrentOctave() const;
	bool isEnabled() const;
	void setEnabled(bool enabled);

private:
	ArpeggiatorSettings settings_;
	// Additional internal state as needed

	// Helper functions
	ArpeggiatorSettings* getCurrentArpSettings() const;
	ArpeggiatorBase* getCurrentArpeggiator() const;
};

/**
 * Effects Controller - Manages audio effects
 */
class EffectsController {
public:
	// Reverb control
	class ReverbEffect {
	public:
		// Direct property access - much simpler!
		bool enabled = false;
		int32_t sendAmount = 0;
		float roomSize = 0.5f;
		float damping = 0.5f;
		float width = 1.0f;
		float lpf = 0.0f;
		float hpf = 0.0f;

		// Methods for actual Deluge integration
		bool isEnabled() const;
		void setEnabled(bool enabled);
		int32_t getSendAmount() const;
		void setSendAmount(int32_t amount);
	};

	// Delay control
	class DelayEffect {
	public:
		// Direct property access - much simpler!
		bool enabled = false;
		int32_t syncLevel = 0;
		int32_t syncType = 0;
		int32_t feedbackAmount = 0;
		bool pingPong = false;
		bool analog = false;

		// Methods for actual Deluge integration
		bool isEnabled() const;
		void setEnabled(bool enabled);
		int32_t getSyncLevel() const;
		void setSyncLevel(int32_t syncLevel);
		int32_t getFeedbackAmount() const;
		void setFeedbackAmount(int32_t amount);
	};

	// Filter control
	class FilterEffect {
	public:
		// Direct property access - much simpler!
		bool enabled = false;
		int32_t lpfFrequency = 0;
		int32_t lpfResonance = 0;
		int32_t lpfMode = 0;
		int32_t lpfMorph = 0;
		int32_t hpfFrequency = 0;
		int32_t hpfResonance = 0;
		int32_t hpfMode = 0;
		int32_t hpfMorph = 0;
		int32_t routing = 0;

		// Methods for actual Deluge integration
		bool isEnabled() const;
		void setEnabled(bool enabled);
		int32_t getLPFFrequency() const;
		void setLPFFrequency(int32_t frequency);
		int32_t getLPFResonance() const;
		void setLPFResonance(int32_t resonance);
		int32_t getLPFMorph() const;
		void setLPFMorph(int32_t morph);
		int32_t getHPFFrequency() const;
		void setHPFFrequency(int32_t frequency);
		int32_t getHPFResonance() const;
		void setHPFResonance(int32_t resonance);
		int32_t getHPFMorph() const;
		void setHPFMorph(int32_t morph);
	};

	// Effect objects
	ReverbEffect& reverb() { return reverb_; }
	DelayEffect& delay() { return delay_; }
	FilterEffect& filter() { return filter_; }

	const ReverbEffect& reverb() const { return reverb_; }
	const DelayEffect& delay() const { return delay_; }
	const FilterEffect& filter() const { return filter_; }

	// Helper function for internal use
	bool isValidForEffects() const;

private:
	ReverbEffect reverb_;
	DelayEffect delay_;
	FilterEffect filter_;
};

/**
 * Scale Controller - Manages scale and key information
 */
class ScaleController {
public:
	// Direct property access - much simpler!
	bool scaleModeEnabled = true;
	int32_t rootNote = 0;
	int32_t currentScale = 0;
	int32_t songRootNote = 0;
	int32_t songScale = 0;

	// Methods for Deluge integration
	bool isScaleModeEnabled() const;
	int32_t getRootNote() const;
	int32_t getCurrentScale() const;
	int32_t getSongRootNote() const;
	int32_t getSongScale() const;

private:
	bool scaleModeEnabled_ = true;
	int32_t rootNote_ = 0;
	int32_t currentScale_ = 0;
	int32_t songRootNote_ = 0;
	int32_t songScale_ = 0;
};

} // namespace deluge::gui::ui::keyboard::core
