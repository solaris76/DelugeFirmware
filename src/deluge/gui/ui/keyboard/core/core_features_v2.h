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
	void requestRendering();
	void setPadLED(int32_t x, int32_t y, int32_t color);
	void setPadLEDBrightness(int32_t x, int32_t y, int32_t brightness);
	void clearAllPads();
	void showPopup(const char* message);
	void showPopup(int32_t number);

private:
	// Display state management
	bool needsRendering_ = false;
};

/**
 * Audio Controller - Handles audio operations
 */
class AudioController {
public:
	bool isCurrentClipInstrument() const;
	bool isCurrentClipAudio() const;
	void sendNoteToCurrentInstrument(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel = 0);
	void stopNoteOnCurrentInstrument(int32_t noteCode, int32_t fromMIDIChannel = 0);
	class InstrumentClip* getCurrentInstrumentClip();
	void renderWaveform(RGB image[][kDisplayWidth + kSideBarWidth],
	                    uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], int32_t xScroll, int32_t xZoom,
	                    int32_t whichKernel, int32_t whichKernelStartedThis);
	bool isAudioClip() const;

private:
	// Audio state management
	bool audioClipActive_ = false;
};

/**
 * Arpeggiator Controller - Manages arpeggiator state and operations
 */
class ArpeggiatorController {
public:
	// Direct access to settings (with proper encapsulation)
	ArpeggiatorSettings& settings() { return settings_; }
	const ArpeggiatorSettings& settings() const { return settings_; }

	// High-level operations
	void addNote(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel = 0);
	void removeNote(int32_t noteCode);
	void clearNotes();
	void reset();
	void triggerStep();

	// State queries
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
};

/**
 * Effects Controller - Manages audio effects
 */
class EffectsController {
public:
	// Reverb control
	class ReverbEffect {
	public:
		bool isEnabled() const;
		void setEnabled(bool enabled);
		float getRoomSize() const;
		void setRoomSize(float roomSize);
		float getDamping() const;
		void setDamping(float damping);
		float getWidth() const;
		void setWidth(float width);
		float getLPF() const;
		void setLPF(float lpf);
		float getHPF() const;
		void setHPF(float hpf);
		int32_t getSendAmount() const;
		void setSendAmount(int32_t amount);

	private:
		bool enabled_ = false;
		float roomSize_ = 0.5f;
		float damping_ = 0.5f;
		float width_ = 1.0f;
		float lpf_ = 0.0f;
		float hpf_ = 0.0f;
		int32_t sendAmount_ = 0;
	};

	// Delay control
	class DelayEffect {
	public:
		bool isEnabled() const;
		void setEnabled(bool enabled);
		int32_t getSyncLevel() const;
		void setSyncLevel(int32_t syncLevel);
		int32_t getSyncType() const;
		void setSyncType(int32_t syncType);
		int32_t getFeedbackAmount() const;
		void setFeedbackAmount(int32_t amount);
		bool isPingPong() const;
		void setPingPong(bool pingPong);
		bool isAnalog() const;
		void setAnalog(bool analog);

	private:
		bool enabled_ = false;
		int32_t syncLevel_ = 0;
		int32_t syncType_ = 0;
		int32_t feedbackAmount_ = 0;
		bool pingPong_ = false;
		bool analog_ = false;
	};

	// Filter control
	class FilterEffect {
	public:
		bool isEnabled() const;
		void setEnabled(bool enabled);
		int32_t getLPFFrequency() const;
		void setLPFFrequency(int32_t frequency);
		int32_t getLPFResonance() const;
		void setLPFResonance(int32_t resonance);
		int32_t getLPFMode() const;
		void setLPFMode(int32_t mode);
		int32_t getLPFMorph() const;
		void setLPFMorph(int32_t morph);
		int32_t getHPFFrequency() const;
		void setHPFFrequency(int32_t frequency);
		int32_t getHPFResonance() const;
		void setHPFResonance(int32_t resonance);
		int32_t getHPFMode() const;
		void setHPFMode(int32_t mode);
		int32_t getHPFMorph() const;
		void setHPFMorph(int32_t morph);
		int32_t getRouting() const;
		void setRouting(int32_t routing);

	private:
		bool enabled_ = false;
		int32_t lpfFrequency_ = 0;
		int32_t lpfResonance_ = 0;
		int32_t lpfMode_ = 0;
		int32_t lpfMorph_ = 0;
		int32_t hpfFrequency_ = 0;
		int32_t hpfResonance_ = 0;
		int32_t hpfMode_ = 0;
		int32_t hpfMorph_ = 0;
		int32_t routing_ = 0;
	};

	// Effect objects
	ReverbEffect& reverb() { return reverb_; }
	DelayEffect& delay() { return delay_; }
	FilterEffect& filter() { return filter_; }

	const ReverbEffect& reverb() const { return reverb_; }
	const DelayEffect& delay() const { return delay_; }
	const FilterEffect& filter() const { return filter_; }

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
