#include "gui/ui/keyboard/modules/core_features_module.h"
#include "gui/ui/keyboard/keyboard_screen.h"
#include "hid/led/pad_leds.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/instrument.h"

namespace deluge::gui::ui::keyboard::modules {

CoreFeaturesModule::CoreFeaturesModule() {
	// Set module priority (higher than test module)
	setPriority(8);

	// Initialize colors
	timingColor_ = RGB(0, 255, 0);   // Green for timing
	audioColor_ = RGB(255, 0, 0);    // Red for audio
	waveformColor_ = RGB(0, 0, 255); // Blue for waveform
}

bool CoreFeaturesModule::init() {
	// Module initialization successful
	return true;
}

void CoreFeaturesModule::update(uint32_t deltaTime) {
	if (!isEnabled()) {
		return;
	}

	// Update core system information
	updateTimingInfo();

	// Update animation
	animationFrame_++;

	// Request rendering for updates
	requestRendering();
}

void CoreFeaturesModule::render(RGB image[][kDisplayWidth + kSideBarWidth],
                                uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	if (!isEnabled()) {
		return;
	}

	// Render different core features
	renderTimingInfo(image);
	renderAudioInfo(image);
	renderWaveform(image);
}

bool CoreFeaturesModule::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	if (!isEnabled()) {
		return false;
	}

	// Handle core features pad presses
	// For example, pad press could trigger audio analysis
	if (y >= 4 && y <= 7) {
		// This is in the waveform area - could trigger sample analysis
		return true; // We handled this event
	}

	return false; // Let other modules handle the event
}

bool CoreFeaturesModule::handleEncoder(int32_t offset, bool isVertical) {
	if (!isEnabled()) {
		return false;
	}

	// Handle encoder events for core features
	// For example, encoder could change BPM or zoom waveform
	if (isVertical) {
		// Vertical encoder could change BPM
		// This is just an example - you'd need to implement actual BPM change
		return true; // We handled this event
	}

	return false; // Let other modules handle the event
}

void CoreFeaturesModule::updateTimingInfo() {
	// Access Deluge's timing system
	isPlaying_ = isPlaybackActive();
	currentBPM_ = getCurrentBPM();
	lastTickCount_ = getCurrentTickCount();
}

void CoreFeaturesModule::renderTimingInfo(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render timing information on top row (y=0)
	// Show current BPM as bars
	int32_t bpmBars = (int32_t)((currentBPM_ - 60.0f) / 120.0f * kDisplayWidth);
	bpmBars = etl::clamp(bpmBars, (int32_t)0, kDisplayWidth);

	for (int x = 0; x < kDisplayWidth; x++) {
		if (x < bpmBars) {
			image[0][x] = timingColor_;
		}
		else {
			image[0][x] = RGB(16, 16, 16); // Dark gray
		}
	}

	// Show playback state on second row (y=1)
	RGB stateColor = isPlaying_ ? RGB(0, 255, 0) : RGB(255, 0, 0);
	for (int x = 0; x < kDisplayWidth; x++) {
		image[1][x] = stateColor;
	}
}

void CoreFeaturesModule::renderAudioInfo(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render audio information on third row (y=2)
	// Check if we have an audio clip
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->output && currentClip->output->type == OutputType::AUDIO) {
		// Show audio clip info
		RGB audioClipColor = RGB(255, 255, 0); // Yellow for audio clip
		for (int x = 0; x < kDisplayWidth; x++) {
			image[2][x] = audioClipColor;
		}
	}
	else {
		// No audio clip
		RGB noAudioColor = RGB(64, 64, 64); // Gray for no audio
		for (int x = 0; x < kDisplayWidth; x++) {
			image[2][x] = noAudioColor;
		}
	}
}

void CoreFeaturesModule::renderWaveform(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render simplified waveform on rows 4-7
	// This is a placeholder - in a real implementation you'd use the waveform renderer

	// Create a simple animated pattern to show waveform area
	for (int y = 4; y < 8; y++) {
		for (int x = 0; x < kDisplayWidth; x++) {
			// Create a simple wave pattern
			int waveHeight = (int)(4 * sin((x + animationFrame_) * 0.3));
			if (y == 4 + (waveHeight + 4) / 2) {
				image[y][x] = waveformColor_;
			}
			else {
				image[y][x] = RGB(16, 16, 16); // Dark gray
			}
		}
	}
}

float CoreFeaturesModule::getCurrentBPM() {
	// Access Deluge's BPM system
	// For now, return a default BPM - you'd need to find the correct API
	return 120.0f; // Default BPM
}

uint32_t CoreFeaturesModule::getCurrentTickCount() {
	// Access Deluge's tick system
	if (playbackHandler.isEitherClockActive()) {
		return playbackHandler.getActualSwungTickCount();
	}
	return 0;
}

bool CoreFeaturesModule::isPlaybackActive() {
	// Access Deluge's playback state
	return playbackHandler.isEitherClockActive();
}

} // namespace deluge::gui::ui::keyboard::modules
