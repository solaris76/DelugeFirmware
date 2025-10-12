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
#include "model/clip/sequencer/sequencer_mode.h"
#include "hid/display/display.h"
#include "hid/led/pad_leds.h"
#include "hid/buttons.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"
#include <cstring>

namespace deluge::model::clip::sequencer {

// Constants
namespace {
	constexpr int32_t kNumPadsPerGroup = 4;
	constexpr uint8_t kFullBrightness = 255;
	constexpr uint8_t kDimBrightness = 32; // 12.5% brightness (255 / 8)

	// Available values for each control type
	constexpr int32_t kClockDivValues[] = {
		-2, // *2 (negative = multiply)
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
		33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
		49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64
	};

	constexpr int32_t kOctaveValues[] = {
		-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5
	};

	constexpr int32_t kTransposeValues[] = {
		-12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1,
		0,
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
	};

	constexpr int32_t kSceneValues[] = {0, 1, 2, 3};

	// Helper: Format signed integer with + or - prefix
	void formatSignedInt(char* buffer, int32_t value) {
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
	}

	// Helper: Request UI refresh for sidebar
	void refreshSidebar() {
		uiNeedsRendering(&instrumentClipView, 0, 0xFFFFFFFF);
		PadLEDs::sendOutSidebarColoursSoon();
	}
}

void SequencerControlGroup::initialize(ControlType type) {
	type_ = type;
	activePad_ = -1;
	heldPad_ = -1;

	// Set default value indices for each pad based on control type
	int32_t defaultIndices[kNumPadsPerGroup] = {0, 0, 0, 0};

	switch (type) {
	case ControlType::CLOCK_DIV:
		// /1, /2, /4, /16
		// kClockDivValues: [-2, 1, 2, 3, 4, ...]
		defaultIndices[0] = 1;  // /1 (value: 1)
		defaultIndices[1] = 2;  // /2 (value: 2)
		defaultIndices[2] = 4;  // /4 (value: 4)
		defaultIndices[3] = 16; // /16 (value: 16)
		break;

	case ControlType::OCTAVE:
		// -1, 0, +1, +2
		// kOctaveValues: [-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5]
		defaultIndices[0] = 4;  // -1 (value: -1)
		defaultIndices[1] = 5;  // 0 (value: 0)
		defaultIndices[2] = 6;  // +1 (value: 1)
		defaultIndices[3] = 7;  // +2 (value: 2)
		break;

	case ControlType::TRANSPOSE:
		// -7, -3, +3, +5
		// kTransposeValues: [-12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, ...]
		defaultIndices[0] = 5;  // -7 (value: -7)
		defaultIndices[1] = 9;  // -3 (value: -3)
		defaultIndices[2] = 15; // +3 (value: 3)
		defaultIndices[3] = 17; // +5 (value: 5)
		break;

	case ControlType::SCENE:
	default:
		// Default: first 4 values (0, 1, 2, 3)
		for (int32_t i = 0; i < kNumPadsPerGroup; i++) {
			defaultIndices[i] = i;
		}
		break;
	}

	// Initialize pads with default values
	for (int32_t i = 0; i < kNumPadsPerGroup; i++) {
		pads_[i].valueIndex = defaultIndices[i];
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
	case ControlType::CLOCK_DIV: return kClockDivValues;
	case ControlType::OCTAVE:    return kOctaveValues;
	case ControlType::TRANSPOSE: return kTransposeValues;
	case ControlType::SCENE:     return kSceneValues;
	default:                     return nullptr;
	}
}

int32_t SequencerControlGroup::getNumAvailableValues() const {
	switch (type_) {
	case ControlType::CLOCK_DIV: return sizeof(kClockDivValues) / sizeof(kClockDivValues[0]);
	case ControlType::OCTAVE:    return sizeof(kOctaveValues) / sizeof(kOctaveValues[0]);
	case ControlType::TRANSPOSE: return sizeof(kTransposeValues) / sizeof(kTransposeValues[0]);
	case ControlType::SCENE:     return sizeof(kSceneValues) / sizeof(kSceneValues[0]);
	default:                     return 0;
	}
}

int32_t SequencerControlGroup::getValue(int32_t padIndex) const {
	if (padIndex < 0 || padIndex >= kNumPadsPerGroup) {
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
	case ControlType::CLOCK_DIV: return "CLOCK DIV";
	case ControlType::OCTAVE:    return "OCTAVE";
	case ControlType::TRANSPOSE: return "TRANSPOSE";
	case ControlType::SCENE:     return "SCENE";
	default:                     return "UNKNOWN";
	}
}

RGB SequencerControlGroup::getColorForType() const {
	switch (type_) {
	case ControlType::CLOCK_DIV: return RGB{255, 0, 0};    // Red
	case ControlType::OCTAVE:    return RGB{255, 128, 0};  // Orange
	case ControlType::TRANSPOSE: return RGB{255, 255, 0};  // Yellow
	case ControlType::SCENE:     return RGB{0, 128, 255};  // Blue
	default:                     return RGB{128, 128, 128}; // Gray
	}
}

const char* SequencerControlGroup::formatValue(int32_t value) const {
	static char buffer[16];

	switch (type_) {
	case ControlType::CLOCK_DIV:
		if (value < 0) {
			buffer[0] = '*';
			buffer[1] = '0' + (-value);
			buffer[2] = '\0';
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
		}
		return buffer;

	case ControlType::OCTAVE:
	case ControlType::TRANSPOSE:
		formatSignedInt(buffer, value);
		return buffer;

	case ControlType::SCENE:
		buffer[0] = 'S';
		buffer[1] = 'C';
		buffer[2] = 'N';
		buffer[3] = ' ';
		buffer[4] = '0' + (value + 1); // 1-indexed (SCN 1, SCN 2, etc.)
		buffer[5] = '\0';
		return buffer;

	default:
		return "?";
	}
}

void SequencerControlGroup::render(RGB image[][kDisplayWidth + kSideBarWidth], int32_t x, int32_t yStart) {
	RGB baseColor = getColorForType();

	for (int32_t i = 0; i < kNumPadsPerGroup; i++) {
		int32_t y = yStart + i;
		bool isBright = (activePad_ == i) || (heldPad_ == i);
		bool isValid = (type_ == ControlType::SCENE) ? pads_[i].sceneValid : true;

		if (isBright) {
			image[y][x] = baseColor;
		}
		else if (!isValid) {
			// Empty scene slot - very dim
			image[y][x] = RGB{
				static_cast<uint8_t>(baseColor.r / 16),
				static_cast<uint8_t>(baseColor.g / 16),
				static_cast<uint8_t>(baseColor.b / 16)
			};
		}
		else {
			// Normal dim: 12.5% brightness
			image[y][x] = RGB{
				static_cast<uint8_t>(baseColor.r / 8),
				static_cast<uint8_t>(baseColor.g / 8),
				static_cast<uint8_t>(baseColor.b / 8)
			};
		}
	}
}

bool SequencerControlGroup::handlePad(int32_t yLocal, int32_t velocity, SequencerMode* mode) {
	if (yLocal < 0 || yLocal >= kNumPadsPerGroup) {
		return false;
	}

	bool pressed = (velocity > 0);
	PadMode padMode = pads_[yLocal].mode;

	// SCENE MODE: Special handling
	if (type_ == ControlType::SCENE && mode) {
		if (pressed) {
			// Check if BACK button is held for scene capture
			if (Buttons::isButtonPressed(deluge::hid::button::BACK)) {
				captureSceneToSlot(yLocal, mode);
				heldPad_ = yLocal;
				refreshSidebar();
				return true;
			}

			// Otherwise, recall scene
			heldPad_ = yLocal;
			if (recallSceneFromSlot(yLocal, mode)) {
				activePad_ = yLocal;
				uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0xFFFFFFFF); // Full refresh after scene recall
			}
			refreshSidebar();
			return true;
		}
		else {
			// Release
			if (heldPad_ == yLocal) {
				heldPad_ = -1;
			}
			refreshSidebar();
			return true;
		}
	}

	// NORMAL CONTROLS: Clock, Octave, Transpose
	if (pressed) {
		heldPad_ = yLocal;

		if (padMode == PadMode::TOGGLE) {
			// Toggle mode: flip state
			bool wasActive = (activePad_ == yLocal);
			activePad_ = wasActive ? -1 : yLocal;

			if (display) {
				const char* message = wasActive ? "OFF" : formatValue(getValue(yLocal));
				display->displayPopup(message);
			}
		}
		else {
			// Momentary mode: activate on press
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

			// Momentary mode: deactivate on release
			if (padMode == PadMode::MOMENTARY && activePad_ == yLocal) {
				activePad_ = -1;
			}
		}
	}

	refreshSidebar();
	return true;
}

bool SequencerControlGroup::handleVerticalEncoder(int32_t yLocal, int32_t offset) {
	if (yLocal < 0 || yLocal >= kNumPadsPerGroup) {
		return false;
	}

	int32_t numValues = getNumAvailableValues();
	if (numValues == 0) {
		return false;
	}

	// Cycle through available values with wrapping
	int32_t& valueIndex = pads_[yLocal].valueIndex;
	valueIndex = (valueIndex + offset) % numValues;
	if (valueIndex < 0) {
		valueIndex += numValues;
	}

	// Show current value
	if (display) {
		display->displayPopup(formatValue(getValue(yLocal)));
	}

	refreshSidebar();
	return true;
}

bool SequencerControlGroup::handleVerticalEncoderButton(int32_t yLocal) {
	if (yLocal < 0 || yLocal >= kNumPadsPerGroup) {
		return false;
	}

	// Toggle between momentary and toggle mode
	PadMode& mode = pads_[yLocal].mode;
	mode = (mode == PadMode::TOGGLE) ? PadMode::MOMENTARY : PadMode::TOGGLE;

	if (display) {
		display->displayPopup(mode == PadMode::TOGGLE ? "TOGGLE" : "MOMENTARY");
	}

	return true;
}

int32_t SequencerControlGroup::getActiveValue() const {
	if (activePad_ >= 0 && activePad_ < kNumPadsPerGroup) {
		return getValue(activePad_);
	}
	return 0;
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

bool SequencerControlGroup::captureSceneToSlot(int32_t padIndex, SequencerMode* mode) {
	if (type_ != ControlType::SCENE || !mode) {
		return false;
	}

	if (padIndex < 0 || padIndex >= kNumPadsPerGroup) {
		return false;
	}

	// Capture scene from mode
	PadData& pad = pads_[padIndex];
	pad.sceneSize = mode->captureScene(pad.sceneData, kMaxSceneDataSize);
	pad.sceneValid = (pad.sceneSize > 0);

	if (pad.sceneValid && display) {
		display->displayPopup("CAPTURED");
	}

	return pad.sceneValid;
}

bool SequencerControlGroup::recallSceneFromSlot(int32_t padIndex, SequencerMode* mode) {
	if (type_ != ControlType::SCENE || !mode) {
		return false;
	}

	if (padIndex < 0 || padIndex >= kNumPadsPerGroup) {
		return false;
	}

	const PadData& pad = pads_[padIndex];
	if (!pad.sceneValid) {
		if (display) {
			display->displayPopup("EMPTY");
		}
		return false;
	}

	// Recall scene to mode
	bool success = mode->recallScene(pad.sceneData, pad.sceneSize);

	if (success && display) {
		display->displayPopup(formatValue(getValue(padIndex)));
	}

	return success;
}

bool SequencerControlGroup::isSceneValid(int32_t padIndex) const {
	if (type_ != ControlType::SCENE) {
		return false;
	}

	if (padIndex < 0 || padIndex >= kNumPadsPerGroup) {
		return false;
	}

	return pads_[padIndex].sceneValid;
}

} // namespace deluge::model::clip::sequencer

