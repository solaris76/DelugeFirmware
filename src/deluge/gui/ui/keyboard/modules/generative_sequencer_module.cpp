#include "gui/ui/keyboard/modules/generative_sequencer_module.h"
#include "gui/ui/keyboard/keyboard_screen.h"
#include "model/clip/clip.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"

namespace deluge::gui::ui::keyboard::modules {

GenerativeSequencerModule::GenerativeSequencerModule() {
	// Set high priority to render on top
	setPriority(15);

	// Initialize colors
	activeColor_ = RGB(255, 255, 0);  // Bright yellow
	inactiveColor_ = RGB(64, 64, 64); // Gray
}

bool GenerativeSequencerModule::init() {
	return true;
}

void GenerativeSequencerModule::update(uint32_t deltaTime) {
	if (!isEnabled()) {
		return;
	}

	// Update animation
	animationFrame_++;

	// Generate notes based on timing
	uint32_t currentTime = playbackHandler.getActualSwungTickCount();
	if (isPlaying_ && (currentTime - lastNoteTime_) >= noteInterval_) {
		generateNextNote();
		lastNoteTime_ = currentTime;
	}

	// Request rendering for animation
	requestRendering();
}

void GenerativeSequencerModule::render(RGB image[][kDisplayWidth + kSideBarWidth],
                                       uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	if (!isEnabled()) {
		return;
	}

	// Render different aspects of the sequencer
	renderSequencer(image);
	renderPattern(image);
	renderControls(image);
}

bool GenerativeSequencerModule::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	if (!isEnabled()) {
		return false;
	}

	// Handle sequencer control
	if (velocity != 0) {
		if (y == 0) {
			// Top row: Toggle play/stop
			isPlaying_ = !isPlaying_;
			if (isPlaying_) {
				lastNoteTime_ = playbackHandler.getActualSwungTickCount();
			}
			return true;
		}
		else if (y == 1) {
			// Second row: Generate single note
			generateNextNote();
			return true;
		}
		else if (y >= 2 && y < 6) {
			// Rows 2-5: Pattern visualization/editing
			patternStep_ = (patternStep_ + 1) % patternLength_;
			return true;
		}
	}

	return false;
}

bool GenerativeSequencerModule::handleEncoder(int32_t offset, bool isVertical) {
	if (!isEnabled()) {
		return false;
	}

	// Handle encoder events for parameter control
	if (isVertical) {
		// Vertical encoder: Change note interval
		if (offset > 0) {
			noteInterval_ = (noteInterval_ < 2000) ? noteInterval_ + 50 : noteInterval_;
		}
		else {
			noteInterval_ = (noteInterval_ > 100) ? noteInterval_ - 50 : noteInterval_;
		}
		return true;
	}
	else {
		// Horizontal encoder: Change note range
		if (offset > 0) {
			noteRange_ = (noteRange_ < 48) ? noteRange_ + 4 : noteRange_;
		}
		else {
			noteRange_ = (noteRange_ > 12) ? noteRange_ - 4 : noteRange_;
		}
		return true;
	}
}

void GenerativeSequencerModule::generateNextNote() {
	// Generate next note using different algorithms
	int32_t noteCode = getPatternNote();

	// Send note to current instrument
	sendNoteToInstrument(noteCode, velocity_, true);

	// Schedule note off (simplified - in real implementation you'd track note offs)
	// For now, we'll just send note on events

	// Update pattern step
	patternStep_ = (patternStep_ + 1) % patternLength_;
}

int32_t GenerativeSequencerModule::getRandomNote() {
	// Generate random note within range
	int32_t baseNote = 60; // Middle C
	int32_t randomOffset = (getRandom255() % noteRange_) - (noteRange_ / 2);
	return baseNote + randomOffset;
}

int32_t GenerativeSequencerModule::getPatternNote() {
	// Generate note based on pattern step
	int32_t baseNote = 60; // Middle C
	int32_t patternOffset = (patternStep_ * noteRange_) / patternLength_ - (noteRange_ / 2);
	return baseNote + patternOffset;
}

int32_t GenerativeSequencerModule::getScaleNote() {
	// Generate note based on scale (simplified major scale)
	int32_t baseNote = 60;                           // Middle C
	int32_t scalePattern[] = {0, 2, 4, 5, 7, 9, 11}; // Major scale intervals
	int32_t scaleIndex = patternStep_ % 7;
	int32_t octave = (patternStep_ / 7) * 12;
	return baseNote + scalePattern[scaleIndex] + octave;
}

void GenerativeSequencerModule::renderSequencer(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render sequencer status on top row
	for (int x = 0; x < kDisplayWidth; x++) {
		if (isPlaying_) {
			image[0][x] = activeColor_;
		}
		else {
			image[0][x] = inactiveColor_;
		}
	}
}

void GenerativeSequencerModule::renderPattern(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render pattern visualization on rows 2-5
	for (int y = 2; y < 6; y++) {
		for (int x = 0; x < kDisplayWidth; x++) {
			if (x == patternStep_) {
				image[y][x] = activeColor_;
			}
			else {
				image[y][x] = inactiveColor_;
			}
		}
	}
}

void GenerativeSequencerModule::renderControls(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render control indicators on row 1
	for (int x = 0; x < kDisplayWidth; x++) {
		if (x < (noteInterval_ / 100)) {  // Show interval as bars
			image[1][x] = RGB(0, 255, 0); // Green for interval
		}
		else {
			image[1][x] = RGB(16, 16, 16); // Dark for unused
		}
	}
}

InstrumentClip* GenerativeSequencerModule::getCurrentInstrumentClip() {
	// Get current instrument clip from the global currentSong
	Clip* currentClip = currentSong->getCurrentClip();
	if (currentClip && currentClip->type == ClipType::INSTRUMENT) {
		return (InstrumentClip*)currentClip;
	}
	return nullptr;
}

void GenerativeSequencerModule::sendNoteToInstrument(int32_t noteCode, uint8_t velocity, bool isOn) {
	// Send note to current instrument
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->output) {
		// Create a simple note instruction
		// This is a simplified version - in reality you'd need proper model stack setup
		// For now, we'll just trigger the visual feedback

		// In a real implementation, you'd do something like:
		// ModelStackWithThreeMainThings* modelStack = setupModelStackWithSong(...);
		// currentClip->output->sendNote(modelStack, isOn, noteCode, velocity, ...);

		// For now, we'll just update our visual state
		currentNote_ = noteCode;
	}
}

} // namespace deluge::gui::ui::keyboard::modules
