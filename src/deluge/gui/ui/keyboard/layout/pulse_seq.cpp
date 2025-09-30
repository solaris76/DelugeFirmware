/*
 * Copyright © 2025 Synthstrom Audible Limited
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
#include "gui/colour/colour.h"
#include "hid/display/display.h"
#include "util/d_string.h"
#include "io/debug/print.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/model_stack.h"
#include "model/scale/note_set.h"
#include "model/scale/preset_scales.h"
#include "playback/playback_handler.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound.h"
#include "model/song/song.h"
#include <climits>
#include <cstdint>
#include <cstring>

namespace deluge::gui::ui::keyboard::layout {

void KeyboardLayoutPulseSeq::evaluatePads(PressedPad presses[kMaxNumKeyboardPadPresses]) {
	currentNotesState = NotesState{}; // Erase active notes

	int32_t gateLineY = getGateLineY();

	for (int32_t idxPress = 0; idxPress < kMaxNumKeyboardPadPresses; ++idxPress) {
		auto pressed = presses[idxPress];
		if (pressed.active && pressed.x < kDisplayWidth) {
			int32_t x = pressed.x;
			int32_t y = pressed.y;

			// Gate line (y0-y3, x0-x7) - bottom left is y0 x0
			if (y == gateLineY && x < 8) {
				handleGateType(x);
			}
			// Note selection (above gate line) - 8 columns only
			else if (y == gateLineY + 1 && x < 8) {
				handleNoteSelection(x);
			}
			// Octave controls (above note selection) - 8 columns only
			else if (y == gateLineY + 2 && x < 8) {
				handleOctaveAdjustment(x, -1); // Octave down
			}
			else if (y == gateLineY + 3 && x < 8) {
				handleOctaveAdjustment(x, 1); // Octave up
			}
			// Pulse count pads (below gate line)
			else if (y < gateLineY && x < 8) {
				handlePulseCount(x, gateLineY - 1 - y);
			}
		}
	}

	// Should be called last so currentNotesState can be read
	ColumnControlsKeyboard::evaluatePads(presses);
}

void KeyboardLayoutPulseSeq::handleVerticalEncoder(int32_t offset) {
	// Scroll the gate line to reveal pulse count pads below
	int32_t newOffset = displayState.gateLineOffset + offset;
	newOffset = std::clamp(newOffset, static_cast<int32_t>(0), static_cast<int32_t>(3)); // 0-3 maps to y4-y7

	if (newOffset != displayState.gateLineOffset) {
		displayState.gateLineOffset = newOffset;
		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleHorizontalEncoder(int32_t offset, bool shiftEnabled, PressedPad presses[kMaxNumKeyboardPadPresses],
                                                     bool encoderPressed) {
	if (horizontalEncoderHandledByColumns(offset, shiftEnabled)) {
		return;
	}

	// TODO: Implement horizontal encoder handling for pulse sequence
	Debug::println("Pulse sequence horizontal encoder");
}

void KeyboardLayoutPulseSeq::precalculate() {
	// Pre-calculate any colors or values needed for rendering
	// For now, we don't need to pre-calculate anything
}

void KeyboardLayoutPulseSeq::updateAnimation() {
	// No animation needed for this layout
}

// OLED display helpers
void KeyboardLayoutPulseSeq::displayGateTypePopup(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	const char* gateTypeName = getGateTypeName(stages[stage].gateType);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "GATE %d: %s", stage + 1, gateTypeName);

	display->displayPopup(buffer);

	// Force UI update
	if (display->haveOLED()) {
		renderUIsForOled();
	}
}

void KeyboardLayoutPulseSeq::displayNotePopup(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	const char* noteName = getNoteName(stages[stage].noteIndex, stages[stage].octave);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "NOTE %d: %s", stage + 1, noteName);

	display->displayPopup(buffer);

	// Force UI update
	if (display->haveOLED()) {
		renderUIsForOled();
	}
}

void KeyboardLayoutPulseSeq::displayOctavePopup(int32_t stage, int32_t direction) {
	if (stage < 0 || stage >= 8) return;

	const char* noteName = getNoteName(stages[stage].noteIndex, stages[stage].octave);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "NOTE %d: %s", stage + 1, noteName);

	display->displayPopup(buffer);

	// Force UI update
	if (display->haveOLED()) {
		renderUIsForOled();
	}
}

void KeyboardLayoutPulseSeq::displayPulseCountPopup(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	char buffer[32];
	snprintf(buffer, sizeof(buffer), "STAGE %d: %d PULSES", stage + 1, stages[stage].pulseCount);

	display->displayPopup(buffer);

	// Force UI update
	if (display->haveOLED()) {
		renderUIsForOled();
	}
}

const char* KeyboardLayoutPulseSeq::getGateTypeName(GateType type) const {
	switch (type) {
		case GateType::OFF: return "OFF";
		case GateType::SINGLE: return "SINGLE";
		case GateType::MULTIPLE: return "MULTIPLE";
		case GateType::HELD: return "HELD";
		default: return "UNKNOWN";
	}
}

const char* KeyboardLayoutPulseSeq::getNoteName(int32_t noteIndex, int32_t octave) {
	// Get current scale notes
	NoteSet& noteSet = getScaleNotes();
	int32_t numNotes = getScaleNoteCount();

	if (noteIndex < 0 || noteIndex >= numNotes) {
		return "C";
	}

	// Get the note from the scale - NoteSet uses array indexing
	int32_t note = noteSet[noteIndex];

	// Convert to note name (simplified - just use C, D, E, F, G, A, B)
	const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	const char* noteName = noteNames[note % 12];

	// Calculate octave number (C3 = 0, so C4 = 1, etc.)
	int32_t octaveNum = 3 + octave; // Base octave is 3

	static char buffer[16];
	snprintf(buffer, sizeof(buffer), "%s%d", noteName, octaveNum);
	return buffer;
}

void KeyboardLayoutPulseSeq::renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Clear all pads first
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		for (int32_t x = 0; x < kDisplayWidth; x++) {
			image[y][x] = RGB{0, 0, 0}; // Black
		}
	}

	int32_t gateLineY = getGateLineY();

	// Render gate line (x0-x7)
	for (int32_t x = 0; x < 8; x++) {
		image[gateLineY][x] = getGateTypeColor(x);
	}

	// Render note selection pads (above gate line) - 8 columns only
	for (int32_t x = 0; x < 8; x++) {
		if (gateLineY + 1 < kDisplayHeight) {
			image[gateLineY + 1][x] = getNoteSelectionColor(x);
		}
	}

	// Render octave controls (above note selection) - 8 columns only
	for (int32_t x = 0; x < 8; x++) {
		if (gateLineY + 2 < kDisplayHeight) {
			image[gateLineY + 2][x] = getOctaveControlColor(); // Octave down
		}
		if (gateLineY + 3 < kDisplayHeight) {
			image[gateLineY + 3][x] = getOctaveControlColor(); // Octave up
		}
	}

	// Render pulse count display (below gate line, 7 pads with gradient on all 8 columns)
	for (int32_t i = 0; i < 7; i++) {
		if (gateLineY - 1 - i >= 0) {
			for (int32_t x = 0; x < 8; x++) {
				image[gateLineY - 1 - i][x] = getPulseCountColor(x, i);
			}
		}
	}
}

ArpeggiatorSettings* KeyboardLayoutPulseSeq::getArpSettings() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip) return nullptr;
	return &clip->arpSettings;
}

Arpeggiator* KeyboardLayoutPulseSeq::getArpeggiator() {
	// TODO: Implement arpeggiator access if needed
	return nullptr;
}

int32_t KeyboardLayoutPulseSeq::getGateLineY() const {
	return displayState.gateLineOffset + 4; // y4-y7 (bottom left is y0 x0)
}

void KeyboardLayoutPulseSeq::handleGateType(int32_t stage) {
	if (stage < 0 || stage >= 8) return; // Gate line only on first 8 columns

	// Cycle through gate types: OFF -> SINGLE -> MULTIPLE -> HELD -> OFF
	int32_t currentType = static_cast<int32_t>(stages[stage].gateType);
	int32_t nextType = (currentType + 1) % 4;
	stages[stage].gateType = static_cast<GateType>(nextType);

	displayGateTypePopup(stage);
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handleNoteSelection(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	// Get current scale notes
	NoteSet& noteSet = getScaleNotes();
	int32_t numNotes = getScaleNoteCount();

	// Cycle through scale notes
	if (numNotes > 0) {
		stages[stage].noteIndex = (stages[stage].noteIndex + 1) % numNotes;
		displayNotePopup(stage);
		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleOctaveAdjustment(int32_t stage, int32_t direction) {
	if (stage < 0 || stage >= 8) return;

	// Adjust octave with reasonable limits
	int32_t newOctave = stages[stage].octave + direction;
	newOctave = std::clamp(newOctave, static_cast<int32_t>(-2), static_cast<int32_t>(3)); // -2 to +3 octaves

	if (newOctave != stages[stage].octave) {
		stages[stage].octave = newOctave;
		displayOctavePopup(stage, direction);
		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handlePulseCount(int32_t stage, int32_t position) {
	if (stage < 0 || stage >= 8) return;
	if (position < 0 || position >= 7) return;

	// Set pulse count to position + 1 (position 0 = pulse count 1, position 6 = pulse count 7)
	int32_t newPulseCount = position + 1;

	if (newPulseCount != stages[stage].pulseCount) {
		stages[stage].pulseCount = newPulseCount;
		displayPulseCountPopup(stage);
		displayState.needsRefresh = true;
	}
}

RGB KeyboardLayoutPulseSeq::getGateTypeColor(int32_t stage) const {
	if (stage < 0 || stage >= 8) return RGB{0, 0, 0}; // Gate line only on first 8 columns

	switch (stages[stage].gateType) {
		case GateType::OFF:
			return RGB{255, 255, 255}; // White
		case GateType::SINGLE:
			return RGB{0, 255, 0}; // Green
		case GateType::MULTIPLE:
			return RGB{0, 0, 255}; // Blue
		case GateType::HELD:
			return RGB{255, 0, 255}; // Magenta
		default:
			return RGB{0, 0, 0}; // Black
	}
}

RGB KeyboardLayoutPulseSeq::getNoteSelectionColor(int32_t stage) const {
	if (stage < 0 || stage >= 8) return RGB{0, 0, 0};

	// Pink color for note selection
	return RGB{255, 100, 150};
}

RGB KeyboardLayoutPulseSeq::getOctaveControlColor() const {
	// Light blue for octave controls
	return RGB{100, 150, 255};
}

RGB KeyboardLayoutPulseSeq::getPulseCountColor(int32_t stage, int32_t position) const {
	if (stage < 0 || stage >= 8) return RGB{0, 0, 0};
	if (position < 0 || position >= 7) return RGB{0, 0, 0};

	// Check if this position should be lit based on the stage's pulse count
	if (position < stages[stage].pulseCount) {
		// Active pulse positions - cyan to purple gradient
		// Position 0 (top) = cyan, position 6 (bottom) = purple
		int32_t intensity = (position * 255) / 6; // 0-255
		uint8_t red = static_cast<uint8_t>(intensity);
		uint8_t green = static_cast<uint8_t>(255 - intensity);
		uint8_t blue = 255;
		return RGB{red, green, blue};
	} else {
		// Inactive pulse positions - black
		return RGB{0, 0, 0};
	}
}

} // namespace deluge::gui::ui::keyboard::layout
