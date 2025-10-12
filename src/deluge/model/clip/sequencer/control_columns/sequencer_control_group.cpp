/*
 * Copyright © 2024 Synthstrom Audible Limited
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

#include "model/clip/sequencer/control_columns/sequencer_control_group.h"
#include "hid/display/display.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"

namespace deluge::model::clip::sequencer {

// Available values for each control type
namespace {
	// Clock divider: *2, /1, /2, /3, ... /64
	constexpr int32_t kClockDivValues[] = {
		-2, // *2 (negative = multiply)
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
		33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
		49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64
	};

	// Octave: -5 to +5
	constexpr int32_t kOctaveValues[] = {
		-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5
	};

	// Transpose: -12 to +12
	constexpr int32_t kTransposeValues[] = {
		-12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1,
		0,
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
	};

	// Scene: placeholder
	constexpr int32_t kSceneValues[] = {0, 1, 2, 3};
}

void SequencerControlGroup::initialize(ControlType type) {
	type_ = type;
	activePad_ = -1;
	heldPad_ = -1;

	// Initialize pad data with default value indices
	for (int32_t i = 0; i < 4; i++) {
		pads_[i].valueIndex = 0; // Start at first value
		pads_[i].mode = PadMode::TOGGLE;
	}
}

void SequencerControlGroup::setType(ControlType newType) {
	if (newType != type_) {
		type_ = newType;
		activePad_ = -1; // Reset active pad when changing type
		heldPad_ = -1;
		initialize(type_); // Reload values
	}
}

const int32_t* SequencerControlGroup::getAvailableValues() const {
	switch (type_) {
	case ControlType::CLOCK_DIV:  return kClockDivValues;
	case ControlType::OCTAVE:     return kOctaveValues;
	case ControlType::TRANSPOSE:  return kTransposeValues;
	case ControlType::SCENE:      return kSceneValues;
	default:                      return nullptr;
	}
}

int32_t SequencerControlGroup::getNumAvailableValues() const {
	switch (type_) {
	case ControlType::CLOCK_DIV:  return sizeof(kClockDivValues) / sizeof(kClockDivValues[0]);
	case ControlType::OCTAVE:     return sizeof(kOctaveValues) / sizeof(kOctaveValues[0]);
	case ControlType::TRANSPOSE:  return sizeof(kTransposeValues) / sizeof(kTransposeValues[0]);
	case ControlType::SCENE:      return sizeof(kSceneValues) / sizeof(kSceneValues[0]);
	default:                      return 0;
	}
}

int32_t SequencerControlGroup::getValue(int32_t padIndex) const {
	if (padIndex < 0 || padIndex >= 4) {
		return 0;
	}

	const int32_t* values = getAvailableValues();
	int32_t numValues = getNumAvailableValues();

	if (!values || numValues == 0) {
		return 0;
	}

	int32_t valueIndex = pads_[padIndex].valueIndex;
	if (valueIndex < 0 || valueIndex >= numValues) {
		return values[0]; // Default to first value
	}

	return values[valueIndex];
}

const char* SequencerControlGroup::getTypeName() const {
	switch (type_) {
	case ControlType::CLOCK_DIV:  return "CLOCK DIV";
	case ControlType::OCTAVE:     return "OCTAVE";
	case ControlType::TRANSPOSE:  return "TRANSPOSE";
	case ControlType::SCENE:      return "SCENE";
	default:                      return "UNKNOWN";
	}
}

RGB SequencerControlGroup::getColorForType() const {
	switch (type_) {
	case ControlType::CLOCK_DIV:  return RGB{255, 0, 0};    // Red
	case ControlType::OCTAVE:     return RGB{255, 128, 0};  // Orange
	case ControlType::TRANSPOSE:  return RGB{255, 255, 0};  // Yellow
	case ControlType::SCENE:      return RGB{255, 0, 255};  // Magenta
	default:                      return RGB{128, 128, 128}; // Gray
	}
}

const char* SequencerControlGroup::formatValue(int32_t value) const {
	static char buffer[16];

	switch (type_) {
	case ControlType::CLOCK_DIV:
		if (value < 0) {
			// Multiply (negative values)
			buffer[0] = '*';
			int32_t absValue = -value;
			buffer[1] = '0' + absValue;
			buffer[2] = '\0';
			return buffer;
		}
		else if (value == 1) {
			return "/1";
		}
		else {
			buffer[0] = '/';
			if (value < 10) {
				buffer[1] = '0' + value;
				buffer[2] = '\0';
			}
			else {
				buffer[1] = '0' + (value / 10);
				buffer[2] = '0' + (value % 10);
				buffer[3] = '\0';
			}
			return buffer;
		}
	case ControlType::OCTAVE:
	case ControlType::TRANSPOSE:
		if (value > 0) {
			buffer[0] = '+';
			if (value < 10) {
				buffer[1] = '0' + value;
				buffer[2] = '\0';
			}
			else {
				buffer[1] = '0' + (value / 10);
				buffer[2] = '0' + (value % 10);
				buffer[3] = '\0';
			}
		}
		else if (value < 0) {
			buffer[0] = '-';
			int32_t absValue = -value;
			if (absValue < 10) {
				buffer[1] = '0' + absValue;
				buffer[2] = '\0';
			}
			else {
				buffer[1] = '0' + (absValue / 10);
				buffer[2] = '0' + (absValue % 10);
				buffer[3] = '\0';
			}
		}
		else {
			buffer[0] = '0';
			buffer[1] = '\0';
		}
		return buffer;
	case ControlType::SCENE:
		buffer[0] = 'S';
		buffer[1] = '0' + (value + 1); // 1-indexed for display
		buffer[2] = '\0';
		return buffer;
	default:
		return "?";
	}
}

void SequencerControlGroup::render(RGB image[][kDisplayWidth + kSideBarWidth], int32_t x, int32_t yStart) {
	RGB baseColor = getColorForType();

	for (int32_t i = 0; i < 4; i++) {
		int32_t y = yStart + i;
		bool isActive = (activePad_ == i);

		// Active pad is bright, inactive is dim
		uint8_t brightness = isActive ? 255 : 64;
		image[y][x] = RGB{
			static_cast<uint8_t>((baseColor.r * brightness) >> 8),
			static_cast<uint8_t>((baseColor.g * brightness) >> 8),
			static_cast<uint8_t>((baseColor.b * brightness) >> 8)
		};
	}
}

bool SequencerControlGroup::handlePad(int32_t yLocal, int32_t velocity) {
	if (yLocal < 0 || yLocal >= 4) {
		return false;
	}

	bool pressed = (velocity > 0);
	PadMode mode = pads_[yLocal].mode;

	if (pressed) {
		// Pad pressed
		heldPad_ = yLocal;

		if (mode == PadMode::TOGGLE) {
			// Toggle: flip state
			if (activePad_ == yLocal) {
				activePad_ = -1;
				if (display) {
					display->displayPopup("OFF");
				}
			}
			else {
				activePad_ = yLocal;
				if (display) {
					display->displayPopup(formatValue(getValue(yLocal)));
				}
			}
		}
		else { // MOMENTARY
			// Momentary: activate on press
			activePad_ = yLocal;
			if (display) {
				display->displayPopup(formatValue(getValue(yLocal)));
			}
		}
	}
	else {
		// Pad released
		if (heldPad_ == yLocal) {
			heldPad_ = -1;

			// If momentary mode, deactivate on release
			if (mode == PadMode::MOMENTARY && activePad_ == yLocal) {
				activePad_ = -1;
			}
		}
	}

	// Trigger UI refresh to update pad brightness
	uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0);

	return true;
}

bool SequencerControlGroup::handleVerticalEncoder(int32_t yLocal, int32_t offset) {
	if (yLocal < 0 || yLocal >= 4) {
		return false;
	}

	// Cycle through available values
	int32_t numValues = getNumAvailableValues();
	if (numValues == 0) {
		return false;
	}

	int32_t& valueIndex = pads_[yLocal].valueIndex;
	valueIndex += offset;

	// Wrap around
	while (valueIndex < 0) {
		valueIndex += numValues;
	}
	while (valueIndex >= numValues) {
		valueIndex -= numValues;
	}

	// Show current value
	if (display) {
		display->displayPopup(formatValue(getValue(yLocal)));
	}

	// Trigger UI refresh
	uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0);

	return true;
}

bool SequencerControlGroup::handleVerticalEncoderButton(int32_t yLocal) {
	if (yLocal < 0 || yLocal >= 4) {
		return false;
	}

	// Toggle between momentary and toggle mode
	PadMode& mode = pads_[yLocal].mode;
	mode = (mode == PadMode::TOGGLE) ? PadMode::MOMENTARY : PadMode::TOGGLE;

	// Show current mode
	if (display) {
		display->displayPopup(mode == PadMode::TOGGLE ? "TOGGLE" : "MOMENTARY");
	}

	return true;
}

int32_t SequencerControlGroup::getActiveValue() const {
	if (activePad_ >= 0 && activePad_ < 4) {
		return getValue(activePad_);
	}
	return 0; // Default neutral value
}

bool SequencerControlGroup::isActive() const {
	// Active if we have an active pad
	// For momentary mode, pad must be both active AND held
	if (activePad_ < 0) {
		return false;
	}

	// If toggle mode, just check if active
	if (pads_[activePad_].mode == PadMode::TOGGLE) {
		return true;
	}

	// If momentary mode, must also be held
	return (heldPad_ == activePad_);
}

int32_t SequencerControlGroup::getClockDivider() const {
	if (type_ == ControlType::CLOCK_DIV && isActive()) {
		return getValue(activePad_);
	}
	return 1; // Default: no division
}

int32_t SequencerControlGroup::getOctaveShift() const {
	if (type_ == ControlType::OCTAVE && isActive()) {
		return getValue(activePad_);
	}
	return 0; // Default: no shift
}

int32_t SequencerControlGroup::getTranspose() const {
	if (type_ == ControlType::TRANSPOSE && isActive()) {
		return getValue(activePad_);
	}
	return 0; // Default: no transpose
}

} // namespace deluge::model::clip::sequencer

