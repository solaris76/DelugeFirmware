/*
 * Copyright © 2016-2024 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "gui/ui/keyboard/layout/pulse_seq.h"
#include "gui/ui/keyboard/keyboard_screen.h"
#include "io/debug/print.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/model_stack.h"
#include "model/scale/note_set.h"
#include "model/scale/preset_scales.h"
#include "playback/playback_handler.h"
#include "processing/engines/audio_engine.h"
#include <climits>
#include <cstdint>
#include <cstring>

namespace deluge::gui::ui::keyboard::layout {

void KeyboardLayoutPulseSeq::evaluatePads(PressedPad presses[kMaxNumKeyboardPadPresses]) {
	currentNotesState = NotesState{}; // Erase active notes

	for (int32_t idxPress = 0; idxPress < kMaxNumKeyboardPadPresses; ++idxPress) {
		auto pressed = presses[idxPress];
		if (pressed.active && pressed.x < kDisplayWidth) {
			int32_t gateLineY = getGateLineY();

			// Handle gate type pad (on the gate line)
			if (pressed.y == gateLineY) {
				// Cycle through gate types: Off -> Single -> Multiple -> Hold -> Off
				int32_t currentGateType = getGateTypeValue(pressed.x);
				int32_t newGateType = (currentGateType + 1) % 4;
				setGateTypeValue(pressed.x, newGateType);

				// Show parameter value popup
				const char* gateNames[] = {"Off", "Single", "Multiple", "Hold"};
				display->displayNotification("Gate", gateNames[newGateType]);
			}
			// Handle pulse count pads (above gate line)
			else if (pressed.y < gateLineY && pressed.y >= gateLineY - 8) {
				int32_t pulsePadIndex = gateLineY - pressed.y - 1; // 0-7
				int32_t newPulseCount = pulsePadIndex + 1;         // 1-8
				setPulseCountValue(pressed.x, newPulseCount);

				// Show pulse count popup
				char pulseStr[8];
				snprintf(pulseStr, sizeof(pulseStr), "%d", newPulseCount);
				display->displayNotification("Pulse", pulseStr);
			}
			// Handle scale note navigation pads (below gate line)
			else if (pressed.y > gateLineY && pressed.y <= gateLineY + 4) {
				int32_t pitchPadIndex = pressed.y - gateLineY - 1; // 0-3
				switch (pitchPadIndex) {
				case 0: // Pitch down
					setScaleNoteValue(pressed.x, getScaleNoteValue(pressed.x) - 1);
					break;
				case 1: // Pitch up
					setScaleNoteValue(pressed.x, getScaleNoteValue(pressed.x) + 1);
					break;
				case 2: // Octave down
					setOctaveValue(pressed.x, getOctaveValue(pressed.x) - 1);
					break;
				case 3: // Octave up
					setOctaveValue(pressed.x, getOctaveValue(pressed.x) + 1);
					break;
				}

				// Show note name popup
				std::string noteName = getNoteName(pressed.x);
				display->displayNotification("Pitch", noteName.c_str());
			}
		}
	}

	// Should be called last so currentNotesState can be read
	ColumnControlsKeyboard::evaluatePads(presses);
}

void KeyboardLayoutPulseSeq::handleVerticalEncoder(int32_t offset) {
	if (verticalEncoderHandledByColumns(offset)) {
		return;
	}

	// Move gate line up/down
	KeyboardStatePulseSeq& state = getState().pulseSeq;
	state.gateLineOffset += offset;

	// Clamp gate line offset to keep gate line in valid range (Y=0 to Y=4)
	state.gateLineOffset = std::max(static_cast<int32_t>(0), std::min(static_cast<int32_t>(4), state.gateLineOffset));

	precalculate();
}

void KeyboardLayoutPulseSeq::handleHorizontalEncoder(int32_t offset, bool shiftEnabled,
                                                     PressedPad presses[kMaxNumKeyboardPadPresses],
                                                     bool encoderPressed) {
	if (horizontalEncoderHandledByColumns(offset, shiftEnabled)) {
		return;
	}

	// Horizontal encoder moves selection between stages (columns)
	// This could be used for selecting which stage to edit
	// For now, we'll just ignore horizontal encoder input
}

void KeyboardLayoutPulseSeq::precalculate() {
	// Pre-calculate any colors or values needed for rendering
	// For now, we don't need to pre-calculate anything
}

void KeyboardLayoutPulseSeq::renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Clear all pads first
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		for (int32_t x = 0; x < kDisplayWidth; x++) {
			image[y][x] = RGB{0, 0, 0}; // Black
		}
	}

	int32_t gateLineY = getGateLineY();

	// Render each column (stage)
	for (int32_t x = 0; x < 8; x++) {
		// Get parameters for this stage
		int32_t gateType = getGateTypeValue(x);
		int32_t pulseCount = getPulseCountValue(x);
		bool isCurrentStage = (x == getCurrentStage() && getState().pulseSeq.isActive);

		// Draw gate type pad on gate line
		if (gateLineY >= 0 && gateLineY < kDisplayHeight) {
			RGB gateColor = gateColors[gateType];

			if (isCurrentStage) {
				// Current stage: bright red highlight
				image[gateLineY][x] = RGB{255, 0, 0}; // Bright red
			}
			else {
				// Normal gate color
				image[gateLineY][x] = gateColor;
			}
		}

		// Draw pulse count pads (above gate line, only up to pulse count value)
		if (pulseCount > 0) {
			for (int32_t i = 0; i < pulseCount && i < 8; i++) {
				int32_t y = gateLineY - 1 - i; // Go up from gate line
				if (y >= 0 && y < kDisplayHeight) {
					image[y][x] = pulseCountColors[i];
				}
			}
		}

		// Draw scale note navigation pads (below gate line, 4 pads)
		for (int32_t i = 0; i < 4; i++) {
			int32_t y = gateLineY + 1 + i; // Go down from gate line
			if (y >= 0 && y < kDisplayHeight) {
				image[y][x] = scaleNoteColors[i];
			}
		}
	}
}

// Pulse Sequencer timing methods
void KeyboardLayoutPulseSeq::processPulseSeqTick() {
	KeyboardStatePulseSeq& state = getState().pulseSeq;
	if (!state.isActive) {
		return;
	}

	// Debug: Show current state
	char debugMsg[64];
	snprintf(debugMsg, sizeof(debugMsg), "Tick: Stage=%d PulsesLeft=%d", state.currentStage,
	         state.pulsesRemainingInStage);
	Debug::println(debugMsg);

	// Decrement pulses remaining in current stage
	state.pulsesRemainingInStage--;

	// If we've used up all pulses in this stage, move to next stage
	if (state.pulsesRemainingInStage <= 0) {
		// Move to next stage
		state.currentStage++;
		if (state.currentStage >= 8) {
			state.currentStage = 0; // Loop back to stage 0
		}

		// Set pulses remaining for new stage
		int32_t pulseCount = getPulseCountValue(state.currentStage);
		state.pulsesRemainingInStage = pulseCount;

		// Debug: Show stage advancement
		snprintf(debugMsg, sizeof(debugMsg), "Advanced to Stage=%d PulseCount=%d", state.currentStage, pulseCount);
		Debug::println(debugMsg);
	}

	// Generate note for current stage based on gate type
	int32_t currentStage = state.currentStage;
	int32_t gateType = getGateTypeValue(currentStage);

	if (gateType != 0) { // Not OFF
		// Get the note to play based on scale note and octave
		int32_t note = getActualNoteValue(currentStage);
		int32_t velocity = 100; // Default velocity

		switch (gateType) {
		case 1: // Single - play once when entering stage
			if (state.pulsesRemainingInStage == getPulseCountValue(currentStage)) {
				// We just entered this stage, play the note
				enableNote(note, velocity);
				Debug::println("Single gate triggered");
			}
			break;

		case 2: // Multiple - play on every 16th note while in stage
			enableNote(note, velocity);
			Debug::println("Multiple gate triggered");
			break;

		case 3: // Hold - sustained note for entire stage duration
			if (state.pulsesRemainingInStage == getPulseCountValue(currentStage)) {
				// Start holding the note when entering stage
				enableNote(note, velocity);
				Debug::println("Hold gate started");
			}
			// Note: For hold, we'd need to implement note-off when leaving stage
			// This would require tracking note states, which is more complex
			break;
		}
	}

	// Force UI refresh to show red pad movement
	// This ensures the red highlighting moves as the sequencer advances
	keyboardScreen.requestRendering();
}

// Proper sequencer timing method - synced to Deluge's internal clock
void KeyboardLayoutPulseSeq::processPulseSeqTiming() {
	KeyboardStatePulseSeq& state = getState().pulseSeq;
	if (!state.isActive) {
		return;
	}

	// Use proper sequencer timing like other keyboard features
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (!currentClip) {
		return;
	}

	// Get current sequencer position using the same method as keyboard tick squares
	extern ::PlaybackHandler playbackHandler;

	if (!::playbackHandler.isEitherClockActive()) {
		return;
	}

	// Calculate current position in sequencer ticks
	uint32_t currentPos =
	    currentClip->lastProcessedPos + ::playbackHandler.getNumSwungTicksInSinceLastActionedSwungTick();

	// Use the same 16th note timing calculation as the arpeggiator and old pulse_seq_view
	const uint32_t syncLevel = 6; // Try SYNC_LEVEL_32ND to get 16th note timing
	const uint32_t syncType = 0;  // SYNC_TYPE_EVEN

	// Calculate ticks per period (same formula as arpeggiator)
	uint32_t ticksPer16thNote = 3 << (9 - syncLevel);
	if (syncType == 1) { // SYNC_TYPE_TRIPLET
		ticksPer16thNote = ticksPer16thNote * 2 / 3;
	}
	else if (syncType == 2) { // SYNC_TYPE_DOTTED
		ticksPer16thNote = ticksPer16thNote * 3 / 2;
	}

	static uint32_t lastProcessedPos = 0;

	// Only process when we've advanced by a 16th note
	if (currentPos - lastProcessedPos >= ticksPer16thNote) {
		processPulseSeqTick();
		lastProcessedPos = currentPos;
	}
}

void KeyboardLayoutPulseSeq::startPulseSeq() {
	KeyboardStatePulseSeq& state = getState().pulseSeq;
	state.isActive = true;
	state.currentStage = 0;
	state.pulsesRemainingInStage = getPulseCountValue(0);
	Debug::println("PulseSeq started");
}

void KeyboardLayoutPulseSeq::stopPulseSeq() {
	KeyboardStatePulseSeq& state = getState().pulseSeq;
	state.isActive = false;
	Debug::println("PulseSeq stopped");
}

void KeyboardLayoutPulseSeq::resetPulseSeq() {
	KeyboardStatePulseSeq& state = getState().pulseSeq;
	state.currentStage = 0;
	state.pulsesRemainingInStage = getPulseCountValue(0);
	Debug::println("PulseSeq reset");
}

int32_t KeyboardLayoutPulseSeq::getCurrentStage() {
	return getState().pulseSeq.currentStage;
}

int32_t KeyboardLayoutPulseSeq::getPulsesRemainingInStage() {
	return getState().pulseSeq.pulsesRemainingInStage;
}

// Parameter access methods
int32_t KeyboardLayoutPulseSeq::getGateTypeValue(int32_t column) {
	return getState().pulseSeq.gateType[column];
}

void KeyboardLayoutPulseSeq::setGateTypeValue(int32_t column, int32_t value) {
	getState().pulseSeq.gateType[column] = value % 4; // Clamp to 0-3
}

int32_t KeyboardLayoutPulseSeq::getScaleNoteValue(int32_t column) {
	return getState().pulseSeq.scaleNote[column];
}

void KeyboardLayoutPulseSeq::setScaleNoteValue(int32_t column, int32_t value) {
	getState().pulseSeq.scaleNote[column] = value % 7; // Clamp to 0-6 (scale degrees)
}

int32_t KeyboardLayoutPulseSeq::getOctaveValue(int32_t column) {
	return getState().pulseSeq.octave[column];
}

void KeyboardLayoutPulseSeq::setOctaveValue(int32_t column, int32_t value) {
	getState().pulseSeq.octave[column] = value; // Allow any octave
}

int32_t KeyboardLayoutPulseSeq::getPulseCountValue(int32_t column) {
	return std::max(1, std::min(8, (int)getState().pulseSeq.pulseCount[column])); // Clamp to 1-8
}

void KeyboardLayoutPulseSeq::setPulseCountValue(int32_t column, int32_t value) {
	getState().pulseSeq.pulseCount[column] = std::max(1, std::min(8, (int)value)); // Clamp to 1-8
}

// Note generation helpers
int32_t KeyboardLayoutPulseSeq::getActualNoteValue(int32_t column) {
	int32_t scaleNote = getScaleNoteValue(column);
	int32_t octave = getOctaveValue(column);

	// Get root note from song
	int32_t rootNote = currentSong->key.rootNote;

	// Calculate base note (root + octave offset)
	int32_t baseNote = rootNote + (octave * kOctaveSize);

	// Add scale degree
	if (getScaleModeEnabled()) {
		// Use scale intervals
		NoteSet& scaleNotes = getScaleNotes();
		uint8_t scaleNoteCount = getScaleNoteCount();

		if (scaleNote < scaleNoteCount) {
			int32_t scaleInterval = scaleNotes[scaleNote];
			return baseNote + scaleInterval;
		}
	}

	// Fallback to chromatic
	return baseNote + scaleNote;
}

std::string KeyboardLayoutPulseSeq::getNoteName(int32_t column) {
	int32_t note = getActualNoteValue(column);

	// Convert MIDI note to note name
	int32_t octave = (note / kOctaveSize) - 1;
	int32_t noteInOctave = note % kOctaveSize;

	const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

	char noteStr[16];
	snprintf(noteStr, sizeof(noteStr), "%s%d", noteNames[noteInOctave], octave);

	return std::string(noteStr);
}

}; // namespace deluge::gui::ui::keyboard::layout
