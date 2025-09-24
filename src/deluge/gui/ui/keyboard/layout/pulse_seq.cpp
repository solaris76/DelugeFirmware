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

	// Check for start/stop button press (column 16, gate line position)
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		int32_t gateLineY = getGateLineY();

		// Look for pad press at column 16 (rightmost column) on the gate line
		for (int32_t idxPress = 0; idxPress < kMaxNumKeyboardPadPresses; ++idxPress) {
			auto pressed = presses[idxPress];
			if (pressed.active && pressed.x == 15 && pressed.y == gateLineY) { // Column 16 (0-based = 15), gate line
				if (currentClip->isPulseSeqActive()) {
					Debug::println("Stopping pulse sequencer from start/stop button");
					currentClip->stopPulseSeq();
				}
				else {
					Debug::println("Starting pulse sequencer from start/stop button");
					currentClip->startPulseSeq();
				}
				break; // Only handle one press
			}
		}
	}

	for (int32_t idxPress = 0; idxPress < kMaxNumKeyboardPadPresses; ++idxPress) {
		auto pressed = presses[idxPress];
		if (pressed.active && pressed.x < kDisplayWidth) {
			int32_t gateLineY = getGateLineY();

			// Skip start/stop button (column 16 on gate line)
			if (pressed.x == 15 && pressed.y == gateLineY) {
				continue; // Skip processing for start/stop button
			}

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

	// Handle vertical scrolling for gate line position
	KeyboardStatePulseSeq& state = getState().pulseSeq;
	int32_t newOffset = state.gateLineOffset + offset;

	// Allow gate line to move between Y=3 and Y=7 (offset 0-4 maps to Y 3-7)
	if (newOffset >= 0 && newOffset <= 4) {
		state.gateLineOffset = newOffset;
		precalculate(); // Trigger UI refresh
	}
}

void KeyboardLayoutPulseSeq::handleHorizontalEncoder(int32_t offset, bool shiftEnabled,
                                                     PressedPad presses[kMaxNumKeyboardPadPresses],
                                                     bool encoderPressed) {
	if (horizontalEncoderHandledByColumns(offset, shiftEnabled)) {
		return;
	}

	// Horizontal encoder functionality removed - use pad presses to start/stop instead

	// Horizontal encoder moves selection between stages (columns)
	// This could be used for selecting which stage to edit
	// For now, we'll just ignore horizontal encoder input
}

int32_t KeyboardLayoutPulseSeq::getGateLineY() {
	KeyboardStatePulseSeq& state = getState().pulseSeq;
	return 3 + state.gateLineOffset; // Y=3 when offset=0 (default), Y=7 when offset=4 (max)
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

		// Check if pulse sequencer is active from InstrumentClip
		bool isPulseSeqActive = false;
		InstrumentClip* currentClip = getCurrentInstrumentClip();
		if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
			isPulseSeqActive = currentClip->isPulseSeqActive();
		}

		int32_t currentStage = getCurrentStage();
		bool isCurrentStage = (x == currentStage && isPulseSeqActive);

		// Debug output removed to prevent E410 error

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

// Pulse Sequencer methods delegate to InstrumentClip

void KeyboardLayoutPulseSeq::startPulseSeq() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		currentClip->startPulseSeq();
	}
}

void KeyboardLayoutPulseSeq::stopPulseSeq() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		currentClip->stopPulseSeq();
	}
}

void KeyboardLayoutPulseSeq::resetPulseSeq() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		currentClip->resetPulseSeq();
	}
}

int32_t KeyboardLayoutPulseSeq::getCurrentStage() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		return currentClip->getCurrentPulseSeqStage();
	}
	return 0;
}

int32_t KeyboardLayoutPulseSeq::getPulsesRemainingInStage() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		return currentClip->getPulsesRemainingInStage();
	}
	return 1;
}

// Parameter access methods (delegate to InstrumentClip)
int32_t KeyboardLayoutPulseSeq::getGateTypeValue(int32_t column) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		return currentClip->getGateTypeValue(column);
	}
	return 0;
}

void KeyboardLayoutPulseSeq::setGateTypeValue(int32_t column, int32_t value) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		currentClip->setGateTypeValue(column, value);
	}
}

int32_t KeyboardLayoutPulseSeq::getScaleNoteValue(int32_t column) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		return currentClip->getScaleNoteValue(column);
	}
	return 0;
}

void KeyboardLayoutPulseSeq::setScaleNoteValue(int32_t column, int32_t value) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		currentClip->setScaleNoteValue(column, value);
	}
}

int32_t KeyboardLayoutPulseSeq::getOctaveValue(int32_t column) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		return currentClip->getOctaveValue(column);
	}
	return 0;
}

void KeyboardLayoutPulseSeq::setOctaveValue(int32_t column, int32_t value) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		currentClip->setOctaveValue(column, value);
	}
}

int32_t KeyboardLayoutPulseSeq::getPulseCountValue(int32_t column) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		return currentClip->getPulseCountValue(column);
	}
	return 1;
}

void KeyboardLayoutPulseSeq::setPulseCountValue(int32_t column, int32_t value) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		currentClip->setPulseCountValue(column, value);
	}
}

// Note generation helpers (delegate to InstrumentClip)
int32_t KeyboardLayoutPulseSeq::getActualNoteValue(int32_t column) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		return currentClip->getActualNoteValue(column);
	}
	return 60; // Default to middle C
}

std::string KeyboardLayoutPulseSeq::getNoteName(int32_t column) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (currentClip && currentClip->paramManager.summaries[0].paramCollection) {
		return currentClip->getNoteName(column);
	}
	return "C4"; // Default note name
}

}; // namespace deluge::gui::ui::keyboard::layout
