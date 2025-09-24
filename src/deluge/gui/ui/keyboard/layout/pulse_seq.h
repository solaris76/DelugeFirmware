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

#pragma once

#include "gui/ui/keyboard/layout/column_controls.h"

namespace deluge::gui::ui::keyboard::layout {

class KeyboardLayoutPulseSeq : public ColumnControlsKeyboard {
public:
	KeyboardLayoutPulseSeq() {}
	~KeyboardLayoutPulseSeq() override {}

	void evaluatePads(PressedPad presses[kMaxNumKeyboardPadPresses]) override;
	void handleVerticalEncoder(int32_t offset) override;
	void handleHorizontalEncoder(int32_t offset, bool shiftEnabled, PressedPad presses[kMaxNumKeyboardPadPresses],
	                             bool encoderPressed = false) override;
	void precalculate() override;

	void renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) override;

	l10n::String name() override { return l10n::String::STRING_FOR_KEYBOARD_LAYOUT_PULSE_SEQ; }
	bool supportsInstrument() override { return true; }
	bool supportsKit() override { return false; }

	// Pulse Sequencer specific methods
	void processPulseSeqTick();
	void processPulseSeqTiming();
	void startPulseSeq();
	void stopPulseSeq();
	void resetPulseSeq();
	int32_t getCurrentStage();
	int32_t getPulsesRemainingInStage();

private:
	// Helper methods for parameter access
	int32_t getGateTypeValue(int32_t column);
	void setGateTypeValue(int32_t column, int32_t value);
	int32_t getScaleNoteValue(int32_t column);
	void setScaleNoteValue(int32_t column, int32_t value);
	int32_t getOctaveValue(int32_t column);
	void setOctaveValue(int32_t column, int32_t value);
	int32_t getPulseCountValue(int32_t column);
	void setPulseCountValue(int32_t column, int32_t value);

	// Note generation helpers
	int32_t getActualNoteValue(int32_t column);
	std::string getNoteName(int32_t column);

	// Gate line calculation
	int32_t getGateLineY() { return 3 + getState().pulseSeq.gateLineOffset; }

	// Color arrays for different parameter types
	RGB gateColors[4] = {
	    RGB{32, 32, 32}, // Off - dim gray
	    RGB{0, 0, 200},  // Single - blue
	    RGB{0, 255, 0},  // Multiple - green
	    RGB{128, 0, 255} // Hold - purple
	};

	RGB scaleNoteColors[4] = {
	    RGB{255, 100, 0}, // Pitch down - orange
	    RGB{255, 200, 0}, // Pitch up - yellow-orange
	    RGB{255, 255, 0}, // Octave down - yellow
	    RGB{200, 255, 0}  // Octave up - yellow-green
	};

	RGB pulseCountColors[8] = {
	    RGB{255, 0, 100},   // 1 pulse - pink-red
	    RGB{255, 0, 150},   // 2 pulses - pink
	    RGB{255, 0, 200},   // 3 pulses - light pink
	    RGB{255, 100, 255}, // 4 pulses - magenta
	    RGB{200, 100, 255}, // 5 pulses - light purple
	    RGB{150, 100, 255}, // 6 pulses - purple-blue
	    RGB{100, 100, 255}, // 7 pulses - blue-purple
	    RGB{50, 50, 255}    // 8 pulses - dark blue
	};
};

}; // namespace deluge::gui::ui::keyboard::layout
