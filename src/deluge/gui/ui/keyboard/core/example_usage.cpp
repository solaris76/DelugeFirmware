// Example: How to retrieve effect parameters using CoreFeatures
// This shows practical usage patterns for keyboard layouts

#include "core/core_features.h"
#include <cstdio>

using namespace deluge::gui::ui::keyboard::core;

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
		float roomSize = core->effects().reverb().getRoomSize();
		float damping = core->effects().reverb().getDamping();
		float width = core->effects().reverb().getWidth();

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
		int32_t syncType = core->effects().delay().getSyncType();
		bool pingPong = core->effects().delay().isPingPong();
		bool analog = core->effects().delay().isAnalog();

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
		int32_t routing = core->effects().filter().getRouting();
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
