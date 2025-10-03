#include "gui/ui/keyboard/modules/timing_module.h"
#include "gui/ui/keyboard/keyboard_screen.h"
#include "hid/led/pad_leds.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include "util/functions.h"

namespace deluge::gui::ui::keyboard::modules {

TimingModule::TimingModule() {
	// Set module priority (higher = renders on top)
	setPriority(10);

	// Initialize colors
	beatColor_ = RGB(255, 255, 0);    // Yellow for beat position
	tempoColor_ = RGB(0, 255, 255);   // Cyan for tempo
	inactiveColor_ = RGB(32, 32, 32); // Dark gray for inactive
}

bool TimingModule::init() {
	// Module initialization successful
	return true;
}

void TimingModule::update(uint32_t deltaTime) {
	if (!isEnabled()) {
		return;
	}

	updateTimingInfo();
	requestRendering(); // Request display update for timing changes
}

void TimingModule::render(RGB image[][kDisplayWidth + kSideBarWidth],
                          uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	if (!isEnabled()) {
		return;
	}

	// Render different timing elements
	if (showBeatPosition_) {
		renderBeatPosition(image);
	}

	if (showTempoVisualization_) {
		renderTempoVisualization(image);
	}

	if (showPlaybackState_) {
		renderPlaybackState(image);
	}
}

bool TimingModule::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	if (!isEnabled()) {
		return false;
	}

	// Handle timing-related pad presses
	// For now, we don't handle any specific pad presses
	// This could be extended to allow tempo changes, etc.

	return false; // Let other modules handle the event
}

bool TimingModule::handleEncoder(int32_t offset, bool isVertical) {
	if (!isEnabled()) {
		return false;
	}

	// Handle encoder events for timing control
	// For now, we don't handle encoder events
	// This could be extended to allow tempo changes, etc.

	return false; // Let other modules handle the event
}

void TimingModule::updateTimingInfo() {
	// Get current playback state
	isPlaying_ = playbackHandler.isEitherClockActive();

	// Get current BPM (using a simplified approach)
	bpm_ = 120.0f; // TODO: Get actual BPM from playback handler

	// Calculate current beat position
	// This is a simplified calculation - in reality you'd want more precise timing
	uint32_t currentTime = playbackHandler.getActualSwungTickCount();
	currentBeat_ = (currentTime / 96) % 16; // Assuming 96 ticks per beat
}

void TimingModule::renderBeatPosition(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render beat position on top row (y=0) - make it more obvious
	for (int x = 0; x < kDisplayWidth; x++) {
		if (x == currentBeat_) {
			image[0][x] = RGB(255, 255, 0); // Bright yellow for current beat
		}
		else {
			image[0][x] = RGB(255, 0, 0); // Bright red for other positions
		}
	}
}

void TimingModule::renderTempoVisualization(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render tempo visualization on second row (y=1)
	// Map BPM to visual intensity
	float normalizedBpm = (bpm_ - 60.0f) / 120.0f; // Normalize 60-180 BPM to 0-1
	normalizedBpm = etl::clamp(normalizedBpm, 0.0f, 1.0f);

	int tempoBars = (int)(normalizedBpm * kDisplayWidth);

	for (int x = 0; x < kDisplayWidth; x++) {
		if (x < tempoBars) {
			image[1][x] = calculateTempoColor(normalizedBpm);
		}
		else {
			image[1][x] = inactiveColor_;
		}
	}
}

void TimingModule::renderPlaybackState(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render playback state on third row (y=2)
	RGB stateColor = isPlaying_ ? RGB(0, 255, 0) : RGB(255, 0, 0); // Green = playing, Red = stopped

	for (int x = 0; x < kDisplayWidth; x++) {
		image[2][x] = stateColor;
	}
}

RGB TimingModule::calculateTempoColor(float intensity) {
	// Create a color gradient based on tempo intensity
	// Low tempo = blue, high tempo = red
	uint8_t red = (uint8_t)(intensity * 255);
	uint8_t blue = (uint8_t)((1.0f - intensity) * 255);

	return RGB(red, 0, blue);
}

} // namespace deluge::gui::ui::keyboard::modules
