// Example: How to retrieve effect parameters using CoreFeatures
// This shows practical usage patterns for keyboard layouts

#include "core_features.h"
#include <cstdio>

using namespace deluge::gui::ui::keyboard::core;

void demonstrateDirectPropertyAccess() {
	auto core = CoreFeatures::create();

	// Direct property access - much simpler!
	bool reverbEnabled = core->effects().reverb().enabled;
	int32_t reverbAmount = core->effects().reverb().sendAmount;
	float roomSize = core->effects().reverb().roomSize;

	// Safe parameter access
	if (core->effects().isValidForEffects()) {
		core->effects().reverb().enabled = true;
		core->effects().reverb().sendAmount = 75;
		core->effects().reverb().roomSize = 0.8f;
	}

	// Complete effect status check
	auto& reverb = core->effects().reverb();
	if (reverb.enabled) {
		int32_t amount = reverb.sendAmount;
		float room = reverb.roomSize;
		// Use the values...
	}
}

void demonstrateOutputTypeUsage() {
	auto core = CoreFeatures::create();

	// Get the current clip's output type
	OutputType outputType = core->audio().getCurrentClipOutputType();

	switch (outputType) {
	case OutputType::SYNTH:
		printf("Current clip is a SYNTH\n");
		// Send notes to synth
		core->audio().sendNoteToCurrentInstrument(60, 100); // C4, velocity 100
		break;

	case OutputType::KIT:
		printf("Current clip is a KIT\n");
		// Send notes to kit (drum sounds)
		core->audio().sendNoteToCurrentInstrument(36, 80); // Kick drum
		break;

	case OutputType::CV:
		printf("Current clip is CV output\n");
		// Send CV notes
		core->audio().sendNoteToCurrentInstrument(60, 100);
		break;

	case OutputType::AUDIO:
		printf("Current clip is AUDIO\n");
		// Audio clips don't support note sending, but effects are available
		core->effects().reverb().setEnabled(true);
		break;

	case OutputType::MIDI_OUT:
		printf("Current clip is MIDI output\n");
		// Send MIDI notes
		core->audio().sendNoteToCurrentInstrument(60, 100); // C4
		break;

	case OutputType::NONE:
	default:
		printf("No current clip or invalid output type\n");
		break;
	}

	// You can also use the boolean convenience methods
	if (core->audio().isCurrentClipInstrument()) {
		printf("Current clip supports note sending\n");
		// Effects are also available on instrument tracks (SYNTH, KIT, AUDIO)
		if (outputType == OutputType::SYNTH || outputType == OutputType::KIT) {
			printf("Effects are available on this instrument track\n");
			core->effects().reverb().setEnabled(true);
		}
	}

	if (core->audio().isCurrentClipAudio()) {
		printf("Current clip is an audio clip\n");
	}
}

void demonstrateFaderUsage() {
	// Create a CoreFeatures instance
	auto core = CoreFeatures::create();

	// Clear all pads first
	core->display().clearAllPads();

	// ============================================================================
	// HORIZONTAL FADER EXAMPLES (y0-7, x0-15)
	// ============================================================================

	// Volume fader on row 0 (x0-15)
	int32_t volume = 75; // 75% volume
	int32_t maxVolume = 100;
	uint32_t dimmedColor = 0x001100; // Dark green
	uint32_t litColor = 0x00FF00;    // Bright green
	core->display().fader().setHorizontalFader(0, 0, volume, maxVolume, dimmedColor, litColor);

	// Reverb send fader on row 1 (x0-15)
	int32_t reverbSend = 50;                                                                     // 50% reverb send
	core->display().fader().setHorizontalFader(0, 1, reverbSend, maxVolume, 0x110011, 0xFF00FF); // Purple

	// ============================================================================
	// VERTICAL FADER EXAMPLES (x0-7, y0-15)
	// ============================================================================

	// Filter cutoff fader on column 0 (y0-15)
	int32_t filterCutoff = 80;                                                                   // 80% cutoff
	core->display().fader().setVerticalFader(0, 0, filterCutoff, maxVolume, 0x111100, 0xFFFF00); // Yellow

	// Resonance fader on column 1 (y0-15)
	int32_t resonance = 30;                                                                   // 30% resonance
	core->display().fader().setVerticalFader(1, 0, resonance, maxVolume, 0x110000, 0xFF0000); // Red

	// ============================================================================
	// SHORT HORIZONTAL FADER EXAMPLES (x0-7 only)
	// ============================================================================

	// Attack fader on row 2 (x0-7)
	int32_t attack = 25;                                                                          // 25% attack
	core->display().fader().setShortHorizontalFader(0, 2, attack, maxVolume, 0x001111, 0x00FFFF); // Cyan

	// Decay fader on row 3 (x0-7)
	int32_t decay = 60;                                                                          // 60% decay
	core->display().fader().setShortHorizontalFader(0, 3, decay, maxVolume, 0x111100, 0xFFFF00); // Yellow

	// ============================================================================
	// READING FADER VALUES FROM PAD POSITIONS
	// ============================================================================

	// Simulate user touching pad at x=8, y=0 (should be 50% of max value for horizontal fader)
	int32_t padX = 8;
	int32_t padY = 0;
	int32_t faderValue = core->display().fader().getValueFromHorizontalFader(0, 0, padX, maxVolume);
	printf("Pad at x=%d, y=%d gives fader value: %d%%\n", padX, padY, faderValue);

	// Simulate user touching pad at x=0, y=12 (should be 75% of max value for vertical fader)
	padX = 0;
	padY = 12;
	faderValue = core->display().fader().getValueFromVerticalFader(0, 0, padY, maxVolume);
	printf("Pad at x=%d, y=%d gives fader value: %d%%\n", padX, padY, faderValue);

	// Simulate user touching pad at x=4, y=2 (should be 50% of max value for short horizontal fader)
	padX = 4;
	padY = 2;
	faderValue = core->display().fader().getValueFromShortHorizontalFader(0, 2, padX, maxVolume);
	printf("Pad at x=%d, y=%d gives fader value: %d%%\n", padX, padY, faderValue);

	// ============================================================================
	// DYNAMIC FADER UPDATES
	// ============================================================================

	// Update volume fader based on current BPM
	int32_t currentBPM = core->timing().getCurrentBPM();
	int32_t bpmPercentage = (currentBPM * 100) / 200; // Scale 0-200 BPM to 0-100%
	core->display().fader().setHorizontalFader(0, 7, bpmPercentage, 100, 0x000011, 0x0000FF); // Blue

	// Update reverb fader based on arpeggiator state
	if (core->arpeggiator().isEnabled()) {
		int32_t activeNotes = core->arpeggiator().getActiveNoteCount();
		int32_t notePercentage = (activeNotes * 100) / 8; // Scale 0-8 notes to 0-100%
		core->display().fader().setHorizontalFader(0, 6, notePercentage, 100, 0x110000, 0xFF0000); // Red
	}

	// Request rendering to update the display
	core->display().requestRendering();

	printf("Fader demonstration complete!\n");
	printf("- Row 0: Volume fader (green)\n");
	printf("- Row 1: Reverb send fader (purple)\n");
	printf("- Row 2: Attack fader (cyan, short)\n");
	printf("- Row 3: Decay fader (yellow, short)\n");
	printf("- Row 6: Arpeggiator notes fader (red)\n");
	printf("- Row 7: BPM fader (blue)\n");
	printf("- Column 0: Filter cutoff fader (yellow, vertical)\n");
	printf("- Column 1: Resonance fader (red, vertical)\n");
}

void demonstrateEffectParameterRetrieval() {
	// Create a CoreFeatures instance
	auto core = CoreFeatures::create();

	// ============================================================================
	// REVERB PARAMETERS
	// ============================================================================

	// Check if reverb is enabled
	bool reverbEnabled = core->effects().reverb().isEnabled();
	printf("Reverb enabled: %s\n", reverbEnabled ? "Yes" : "No");

	if (reverbEnabled) {
		// Get reverb send amount (0-100%)
		int32_t reverbAmount = core->effects().reverb().getSendAmount();
		printf("Reverb send amount: %d%%\n", reverbAmount);

		// Get other reverb parameters (currently placeholders)
		// Direct property access - much simpler!
		float roomSize = core->effects().reverb().roomSize;
		float damping = core->effects().reverb().damping;
		float width = core->effects().reverb().width;

		printf("Room size: %.2f, Damping: %.2f, Width: %.2f\n", roomSize, damping, width);
	}

	// ============================================================================
	// DELAY PARAMETERS
	// ============================================================================

	// Check if delay is enabled
	bool delayEnabled = core->effects().delay().isEnabled();
	printf("Delay enabled: %s\n", delayEnabled ? "Yes" : "No");

	if (delayEnabled) {
		// Get delay sync level (0-7)
		int32_t syncLevel = core->effects().delay().getSyncLevel();
		printf("Delay sync level: %d\n", syncLevel);

		// Get delay feedback amount (0-100%)
		int32_t feedbackAmount = core->effects().delay().getFeedbackAmount();
		printf("Delay feedback: %d%%\n", feedbackAmount);

		// Get other delay parameters (currently placeholders)
		// Direct property access - much simpler!
		int32_t syncType = core->effects().delay().syncType;
		bool pingPong = core->effects().delay().pingPong;
		bool analog = core->effects().delay().analog;

		printf("Sync type: %d, Ping-pong: %s, Analog: %s\n", syncType, pingPong ? "Yes" : "No", analog ? "Yes" : "No");
	}

	// ============================================================================
	// FILTER PARAMETERS
	// ============================================================================

	// Check if filters are enabled
	bool filterEnabled = core->effects().filter().isEnabled();
	printf("Filter enabled: %s\n", filterEnabled ? "Yes" : "No");

	if (filterEnabled) {
		// Low-pass filter parameters
		int32_t lpfFreq = core->effects().filter().getLPFFrequency();
		int32_t lpfRes = core->effects().filter().getLPFResonance();
		int32_t lpfMorph = core->effects().filter().getLPFMorph();

		printf("LPF - Freq: %d%%, Resonance: %d%%, Morph: %d%%\n", lpfFreq, lpfRes, lpfMorph);

		// High-pass filter parameters
		int32_t hpfFreq = core->effects().filter().getHPFFrequency();
		int32_t hpfRes = core->effects().filter().getHPFResonance();
		int32_t hpfMorph = core->effects().filter().getHPFMorph();

		printf("HPF - Freq: %d%%, Resonance: %d%%, Morph: %d%%\n", hpfFreq, hpfRes, hpfMorph);

		// Filter routing (currently placeholder)
		// Direct property access - much simpler!
		int32_t routing = core->effects().filter().routing;
		printf("Filter routing: %d\n", routing);
	}
}

// ============================================================================
// PRACTICAL KEYBOARD LAYOUT EXAMPLE
// ============================================================================

class EffectVisualizerLayout {
private:
	std::unique_ptr<CoreFeatures> core_;

public:
	EffectVisualizerLayout() : core_(CoreFeatures::create()) {}

	void renderEffectStatus() {
		// Visualize reverb status on pads
		if (core_->effects().reverb().isEnabled()) {
			int32_t amount = core_->effects().reverb().getSendAmount();
			// Light up pads based on reverb amount
			for (int i = 0; i < 8; i++) {
				bool shouldLight = (amount > (i * 12)); // 0-12%, 12-24%, etc.
				core_->display().setPadLED(i, 0, shouldLight ? 0xFF0000 : 0x000000);
			}
		}

		// Visualize delay status
		if (core_->effects().delay().isEnabled()) {
			int32_t syncLevel = core_->effects().delay().getSyncLevel();
			// Light up pads based on sync level (0-7)
			for (int i = 0; i < 8; i++) {
				bool shouldLight = (i <= syncLevel);
				core_->display().setPadLED(i, 1, shouldLight ? 0x00FF00 : 0x000000);
			}
		}

		// Visualize filter status
		if (core_->effects().filter().isEnabled()) {
			int32_t lpfFreq = core_->effects().filter().getLPFFrequency();
			int32_t hpfFreq = core_->effects().filter().getHPFFrequency();

			// LPF visualization (row 2)
			for (int i = 0; i < 8; i++) {
				bool shouldLight = (lpfFreq > (i * 12));
				core_->display().setPadLED(i, 2, shouldLight ? 0x0000FF : 0x000000);
			}

			// HPF visualization (row 3)
			for (int i = 0; i < 8; i++) {
				bool shouldLight = (hpfFreq > (i * 12));
				core_->display().setPadLED(i, 3, shouldLight ? 0xFFFF00 : 0x000000);
			}
		}

		// Request display update
		core_->display().requestRendering();
	}

	void handlePadPress(int x, int y) {
		// Example: Toggle reverb when pressing row 0
		if (y == 0) {
			bool currentlyEnabled = core_->effects().reverb().isEnabled();
			core_->effects().reverb().setEnabled(!currentlyEnabled);

			// Show feedback
			core_->display().showPopup(currentlyEnabled ? "Reverb OFF" : "Reverb ON");
		}

		// Example: Adjust delay sync level when pressing row 1
		if (y == 1) {
			int32_t currentSync = core_->effects().delay().getSyncLevel();
			int32_t newSync = (currentSync + 1) % 8; // Cycle through 0-7
			core_->effects().delay().setSyncLevel(newSync);

			core_->display().showPopup(newSync);
		}
	}
};

// ============================================================================
// SIMPLE USAGE PATTERNS
// ============================================================================

void simpleEffectChecks() {
	auto core = CoreFeatures::create();

	// Quick effect status check
	if (core->effects().reverb().isEnabled()) {
		printf("Reverb is on at %d%%\n", core->effects().reverb().getSendAmount());
	}

	if (core->effects().delay().isEnabled()) {
		printf("Delay is on at sync level %d\n", core->effects().delay().getSyncLevel());
	}

	if (core->effects().filter().isEnabled()) {
		printf("LPF at %d%%, HPF at %d%%\n", core->effects().filter().getLPFFrequency(),
		       core->effects().filter().getHPFFrequency());
	}
}

void effectParameterModification() {
	auto core = CoreFeatures::create();

	// Enable reverb with 50% send
	core->effects().reverb().setEnabled(true);
	core->effects().reverb().setSendAmount(50);

	// Enable delay with sync level 3
	core->effects().delay().setEnabled(true);
	core->effects().delay().setSyncLevel(3);
	core->effects().delay().setFeedbackAmount(30);

	// Enable filter with LPF at 75%
	core->effects().filter().setEnabled(true);
	core->effects().filter().setLPFFrequency(75);
	core->effects().filter().setLPFResonance(25);

	printf("Effects configured!\n");
}
