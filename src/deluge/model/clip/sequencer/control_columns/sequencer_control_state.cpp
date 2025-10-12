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

#include "model/clip/sequencer/control_columns/sequencer_control_state.h"
#include "model/clip/sequencer/sequencer_mode.h"
#include "hid/display/display.h"
#include "hid/led/pad_leds.h"
#include "hid/buttons.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"
#include <cstring>

namespace deluge::model::clip::sequencer {

// Forward declare helper functions from sequencer_control_group.cpp
namespace helpers {
	const char* getTypeName(ControlType type);
	RGB getColorForType(ControlType type);
	const int32_t* getAvailableValues(ControlType type);
	int32_t getNumAvailableValues(ControlType type);
	int32_t getValue(ControlType type, int32_t valueIndex);
	const char* formatValue(ControlType type, int32_t value);
}

namespace {
	// Helper: Request UI refresh for sidebar
	void refreshSidebar() {
		uiNeedsRendering(&instrumentClipView, 0, 0xFFFFFFFF);
		PadLEDs::sendOutSidebarColoursSoon();
	}
}

SequencerControlState::SequencerControlState() {
	// Initialize scene buffers
	for (size_t i = 0; i < kMaxScenes; ++i) {
		sceneSizes_[i] = 0;
	}
	initialize();
}

void SequencerControlState::initialize() {
	// Default configuration for 16 pads:
	// x16 (pads 0-7):
	//   y0: OCTAVE +1
	//   y1: TRANSPOSE +5
	//   y2-y7: NONE (user configurable)
	// x17 (pads 8-15):
	//   y0: SCENE 1
	//   y1: SCENE 2
	//   y2-y5: NONE (user configurable)
	//   y6: RANDOM
	//   y7: RESET

	// x16 column - mostly empty for user configuration
	pads_[0].type = ControlType::OCTAVE;
	pads_[1].type = ControlType::TRANSPOSE;
	pads_[2].type = ControlType::NONE;
	pads_[3].type = ControlType::NONE;
	pads_[4].type = ControlType::NONE;
	pads_[5].type = ControlType::NONE;
	pads_[6].type = ControlType::NONE;
	pads_[7].type = ControlType::NONE;

	// x17 column - basic scene + generative controls
	pads_[8].type = ControlType::SCENE;
	pads_[9].type = ControlType::SCENE;
	pads_[10].type = ControlType::NONE;
	pads_[11].type = ControlType::NONE;
	pads_[12].type = ControlType::NONE;
	pads_[13].type = ControlType::NONE;
	pads_[14].type = ControlType::RANDOM;
	pads_[15].type = ControlType::RESET;

	// Set useful default values
	// Octave +1 (valueIndex 5 in kOctaveValues = +1)
	pads_[0].valueIndex = 6;  // +1 octave (kOctaveValues[6] = +1)

	// Transpose +5 (valueIndex 17 in kTransposeValues = +5)
	pads_[1].valueIndex = 17; // +5 semitones (kTransposeValues[17] = +5)

	// Scenes: 1-2
	pads_[8].valueIndex = 0;  // Scene 1
	pads_[9].valueIndex = 1;  // Scene 2

	// Random: default to 50%
	pads_[14].valueIndex = 4; // 50% for random

	// Reset has no value
	pads_[15].valueIndex = 0;
}

int32_t SequencerControlState::getPadIndex(int32_t x, int32_t y) const {
	if (x == kDisplayWidth) {
		// x16 column
		if (y >= 0 && y < 8) {
			return y; // pads 0-7
		}
	}
	else if (x == (kDisplayWidth + 1)) {
		// x17 column
		if (y >= 0 && y < 8) {
			return 8 + y; // pads 8-15
		}
	}
	return -1; // Invalid
}

void SequencerControlState::render(RGB image[][kDisplayWidth + kSideBarWidth],
                                    uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	// Render x16 column (pads 0-7)
	for (int32_t y = 0; y < 8; y++) {
		ControlPad& pad = pads_[y];
		RGB color = helpers::getColorForType(pad.type);

		// Adjust brightness
		bool isBright = pad.active || pad.held;
		bool isEmpty = (pad.type == ControlType::SCENE && !pad.sceneValid) || (pad.type == ControlType::NONE);

		if (isBright) {
			image[y][kDisplayWidth] = color;
		}
		else if (isEmpty) {
			// Very dim for empty scenes or unused pads
			image[y][kDisplayWidth] = RGB{
				static_cast<uint8_t>(color.r / 16),
				static_cast<uint8_t>(color.g / 16),
				static_cast<uint8_t>(color.b / 16)
			};
		}
		else {
			// Normal dim
			image[y][kDisplayWidth] = RGB{
				static_cast<uint8_t>(color.r / 8),
				static_cast<uint8_t>(color.g / 8),
				static_cast<uint8_t>(color.b / 8)
			};
		}

		if (occupancyMask) {
			occupancyMask[y][kDisplayWidth] = 64;
		}
	}

	// Render x17 column (pads 8-15)
	for (int32_t y = 0; y < 8; y++) {
		ControlPad& pad = pads_[8 + y];
		RGB color = helpers::getColorForType(pad.type);

		bool isBright = pad.active || pad.held;
		bool isEmpty = (pad.type == ControlType::SCENE && !pad.sceneValid) || (pad.type == ControlType::NONE);

		if (isBright) {
			image[y][kDisplayWidth + 1] = color;
		}
		else if (isEmpty) {
			image[y][kDisplayWidth + 1] = RGB{
				static_cast<uint8_t>(color.r / 16),
				static_cast<uint8_t>(color.g / 16),
				static_cast<uint8_t>(color.b / 16)
			};
		}
		else {
			image[y][kDisplayWidth + 1] = RGB{
				static_cast<uint8_t>(color.r / 8),
				static_cast<uint8_t>(color.g / 8),
				static_cast<uint8_t>(color.b / 8)
			};
		}

		if (occupancyMask) {
			occupancyMask[y][kDisplayWidth + 1] = 64;
		}
	}
}

bool SequencerControlState::handlePad(int32_t x, int32_t y, int32_t velocity, SequencerMode* mode) {
	int32_t padIndex = getPadIndex(x, y);
	if (padIndex < 0) {
		return false;
	}

	ControlPad& pad = pads_[padIndex];
	bool pressed = (velocity > 0);

	// SCENE TYPE: Special handling
	if (pad.type == ControlType::SCENE && mode) {
		if (pressed) {
			// SAVE button = capture scene
			if (Buttons::isButtonPressed(deluge::hid::button::SAVE)) {
				int32_t sceneNum = pad.valueIndex;
				if (sceneNum >= 0 && sceneNum < kMaxScenes) {
					// Simplified: Only capture mode-specific pattern data to shared buffer
					size_t modeDataSize = mode->captureScene(sceneBuffers_[sceneNum], kMaxSceneDataSize);
					
					if (modeDataSize > 0 && modeDataSize <= kMaxSceneDataSize) {
						sceneSizes_[sceneNum] = modeDataSize;
						pad.sceneValid = true;

						if (display) {
							display->displayPopup("CAPTURED");
						}
					} else {
						if (display) {
							display->displayPopup(modeDataSize > kMaxSceneDataSize ? "SCENE TOO BIG" : "CAPTURE FAILED");
						}
					}
				}
				pad.held = true;
				refreshSidebar();
				return true;
			}

			// SHIFT button = clear scene
			if (Buttons::isShiftButtonPressed()) {
				int32_t sceneNum = pad.valueIndex;
				if (sceneNum >= 0 && sceneNum < kMaxScenes) {
					sceneSizes_[sceneNum] = 0;
					pad.sceneValid = false;
					if (display) {
						display->displayPopup("CLEARED");
					}
				}
				pad.held = true;
				refreshSidebar();
				return true;
			}

			// Otherwise = recall scene
			if (pad.sceneValid) {
				int32_t sceneNum = pad.valueIndex;
				if (sceneNum >= 0 && sceneNum < kMaxScenes && sceneSizes_[sceneNum] > 0) {
					// Simplified: Only restore mode-specific pattern data from shared buffer
					bool success = mode->recallScene(sceneBuffers_[sceneNum], sceneSizes_[sceneNum]);
					if (success) {
						pad.active = true;
						uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0xFFFFFFFF);

						if (display) {
							int32_t val = helpers::getValue(pad.type, pad.valueIndex);
							display->displayPopup(helpers::formatValue(pad.type, val));
						}
					}
				}
			}
			else {
				if (display) {
					display->displayPopup("EMPTY");
				}
			}

			pad.held = true;
			refreshSidebar();
			return true;
		}
		else {
			// Release
			pad.held = false;
			refreshSidebar();
			return true;
		}
	}

	// RESET TYPE: Instant trigger, no state
	if (pad.type == ControlType::RESET && mode) {
		if (pressed) {
			mode->resetToInit();
			if (display) {
				display->displayPopup("RESET");
			}
			pad.held = true;
			pad.active = true;
			uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0xFFFFFFFF);
			refreshSidebar();
			return true;
		}
		else {
			pad.held = false;
			pad.active = false;
			refreshSidebar();
			return true;
		}
	}

	// RANDOM/EVOLVE/MUTATE: Instant trigger with % value
	if ((pad.type == ControlType::RANDOM || pad.type == ControlType::EVOLVE || pad.type == ControlType::MUTATE) && mode) {
		if (pressed) {
			int32_t mutationRate = helpers::getValue(pad.type, pad.valueIndex);

			if (pad.type == ControlType::RANDOM) {
				mode->randomizeAll();
			}
			else if (pad.type == ControlType::EVOLVE) {
				mode->evolveNotesLow(); // Uses mutation rate from pad value
			}
			else if (pad.type == ControlType::MUTATE) {
				mode->evolveNotesHigh(); // Uses mutation rate from pad value
			}

			if (display) {
				static char popup[40];
				snprintf(popup, sizeof(popup), "%s: %s", helpers::getTypeName(pad.type),
				         helpers::formatValue(pad.type, mutationRate));
				display->displayPopup(popup);
			}

			pad.held = true;
			pad.active = true;
			uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0xFFFFFFFF);
			refreshSidebar();
			return true;
		}
		else {
			pad.held = false;
			pad.active = false;
			refreshSidebar();
			return true;
		}
	}

	// NONE TYPE: Show configuration hint
	if (pad.type == ControlType::NONE) {
		if (pressed) {
			if (display) {
				display->displayPopup("<> TO CONFIGURE");
			}
			pad.held = true;
			refreshSidebar();
			return true;
		}
		else {
			pad.held = false;
			refreshSidebar();
			return true;
		}
	}

	// NORMAL CONTROLS: Clock, Octave, Transpose, Direction
	if (pressed) {
		pad.held = true;

		if (pad.mode == PadMode::TOGGLE) {
			// Toggle mode: flip state
			pad.active = !pad.active;

			if (display && pad.type != ControlType::NONE) {
				if (pad.active) {
					int32_t val = helpers::getValue(pad.type, pad.valueIndex);
					static char popup[40];
					snprintf(popup, sizeof(popup), "%s: %s", helpers::getTypeName(pad.type),
					         helpers::formatValue(pad.type, val));
					display->displayPopup(popup);
				}
				else {
					display->displayPopup("OFF");
				}
			}
		}
		else {
			// Momentary mode: activate on press
			pad.active = true;
			if (display && pad.type != ControlType::NONE) {
				int32_t val = helpers::getValue(pad.type, pad.valueIndex);
				static char popup[40];
				snprintf(popup, sizeof(popup), "%s: %s", helpers::getTypeName(pad.type),
				         helpers::formatValue(pad.type, val));
				display->displayPopup(popup);
			}
		}
	}
	else {
		// Release
		if (pad.mode == PadMode::MOMENTARY) {
			pad.active = false;
		}
		pad.held = false;
	}

	refreshSidebar();
	return true;
}

bool SequencerControlState::handleHorizontalEncoder(int32_t heldX, int32_t heldY, int32_t offset) {
	int32_t padIndex = getPadIndex(heldX, heldY);
	if (padIndex < 0) {
		return false;
	}

	ControlPad& pad = pads_[padIndex];

	// Cycle through available control types
	constexpr ControlType availableTypes[] = {
		ControlType::NONE,
		ControlType::CLOCK_DIV,
		ControlType::OCTAVE,
		ControlType::TRANSPOSE,
		ControlType::SCENE,
		ControlType::DIRECTION,
		ControlType::RESET,
		ControlType::RANDOM,
		ControlType::EVOLVE,
		ControlType::MUTATE
	};
	constexpr int32_t numTypes = sizeof(availableTypes) / sizeof(availableTypes[0]);

	// Find current type index
	int32_t currentIndex = 0;
	for (int32_t i = 0; i < numTypes; i++) {
		if (availableTypes[i] == pad.type) {
			currentIndex = i;
			break;
		}
	}

	// Cycle to next/prev type
	int32_t newIndex = currentIndex + (offset > 0 ? 1 : -1);
	if (newIndex < 0) {
		newIndex = numTypes - 1;
	}
	else if (newIndex >= numTypes) {
		newIndex = 0;
	}

	// Set new type
	pad.type = availableTypes[newIndex];
	pad.active = false; // Reset active state
	pad.valueIndex = 0; // Reset to first value

	// Set sensible default value based on type
	switch (pad.type) {
	case ControlType::OCTAVE:
		pad.valueIndex = 5; // 0 octaves
		break;
	case ControlType::TRANSPOSE:
		pad.valueIndex = 12; // 0 semitones
		break;
	case ControlType::CLOCK_DIV:
		pad.valueIndex = 1; // /1
		break;
	case ControlType::RANDOM:
	case ControlType::EVOLVE:
	case ControlType::MUTATE:
		pad.valueIndex = 4; // 50%
		break;
	default:
		break;
	}

	if (display) {
		display->displayPopup(helpers::getTypeName(pad.type));
	}

	refreshSidebar();
	return true;
}

bool SequencerControlState::handleVerticalEncoder(int32_t heldX, int32_t heldY, int32_t offset) {
	int32_t padIndex = getPadIndex(heldX, heldY);
	if (padIndex < 0) {
		return false;
	}

	ControlPad& pad = pads_[padIndex];

	// Skip types with no values
	if (pad.type == ControlType::NONE || pad.type == ControlType::RESET) {
		return false;
	}

	int32_t numValues = helpers::getNumAvailableValues(pad.type);
	if (numValues == 0) {
		return false;
	}

	// Special handling for SCENE type: skip scene numbers already assigned to other pads
	if (pad.type == ControlType::SCENE) {
		int32_t startIndex = pad.valueIndex;
		int32_t direction = (offset > 0) ? 1 : -1;
		int32_t attempts = 0;

		// Keep trying until we find an available scene number
		while (attempts < numValues) {
			// Move to next/prev value
			pad.valueIndex = (pad.valueIndex + direction + numValues) % numValues;

			// Get the actual scene number
			int32_t sceneNum = helpers::getValue(pad.type, pad.valueIndex);

			// Check if this scene number is already assigned to another pad
			bool isAvailable = true;
			for (size_t i = 0; i < pads_.size(); ++i) {
				if (i != static_cast<size_t>(padIndex) && pads_[i].type == ControlType::SCENE) {
					int32_t otherSceneNum = helpers::getValue(pads_[i].type, pads_[i].valueIndex);
					if (otherSceneNum == sceneNum) {
						isAvailable = false;
						break;
					}
				}
			}

			// If available, we're done
			if (isAvailable) {
				break;
			}

			attempts++;
		}

		// If all scene numbers are taken, stay at current
		if (attempts >= numValues) {
			pad.valueIndex = startIndex;
			if (display) {
				display->displayPopup("ALL SCENES USED");
			}
			return true;
		}
	}
	else {
		// Normal cycling for non-SCENE types
		pad.valueIndex = (pad.valueIndex + offset) % numValues;
		if (pad.valueIndex < 0) {
			pad.valueIndex += numValues;
		}
	}

	// Show current value
	if (display) {
		int32_t val = helpers::getValue(pad.type, pad.valueIndex);
		static char popup[40];
		snprintf(popup, sizeof(popup), "%s: %s", helpers::getTypeName(pad.type),
		         helpers::formatValue(pad.type, val));
		display->displayPopup(popup);
	}

	refreshSidebar();
	return true;
}

bool SequencerControlState::handleVerticalEncoderButton(int32_t heldX, int32_t heldY) {
	int32_t padIndex = getPadIndex(heldX, heldY);
	if (padIndex < 0) {
		return false;
	}

	ControlPad& pad = pads_[padIndex];

	// Toggle mode for pads that support it
	if (pad.type != ControlType::NONE && pad.type != ControlType::SCENE
	    && pad.type != ControlType::RESET && pad.type != ControlType::RANDOM
	    && pad.type != ControlType::EVOLVE && pad.type != ControlType::MUTATE) {

		pad.mode = (pad.mode == PadMode::TOGGLE) ? PadMode::MOMENTARY : PadMode::TOGGLE;

		if (display) {
			display->displayPopup(pad.mode == PadMode::TOGGLE ? "TOGGLE" : "MOMENTARY");
		}

		return true;
	}

	return false;
}

CombinedEffects SequencerControlState::getCombinedEffects() const {
	CombinedEffects effects;

	// Collect effects from all active pads
	for (const auto& pad : pads_) {
		if (pad.active) {
			int32_t value = helpers::getValue(pad.type, pad.valueIndex);

			switch (pad.type) {
			case ControlType::CLOCK_DIV:
				effects.clockDivider = value;
				break;
			case ControlType::OCTAVE:
				effects.octaveShift += value;
				break;
			case ControlType::TRANSPOSE:
				effects.transpose += value;
				break;
			case ControlType::SCENE:
				effects.sceneIndex = value;
				break;
			case ControlType::DIRECTION:
				effects.direction = value;
				break;
			default:
				break;
			}
		}
	}

	return effects;
}

bool SequencerControlState::isAnyPadHeld() const {
	for (const auto& pad : pads_) {
		if (pad.held) {
			return true;
		}
	}
	return false;
}

size_t SequencerControlState::captureState(void* buffer, size_t maxSize) const {
	uint8_t* ptr = static_cast<uint8_t*>(buffer);
	size_t offset = 0;

	// For each pad (excluding SCENE pads)
	for (size_t i = 0; i < pads_.size(); ++i) {
		const auto& pad = pads_[i];

		// Skip scene pads (they store the scene data itself)
		if (pad.type == ControlType::SCENE) {
			continue;
		}

		// Check buffer space (1 byte type + 4 bytes valueIndex + 1 byte mode + 1 byte active = 7 bytes)
		if (offset + 7 > maxSize) {
			return 0; // Not enough space
		}

		// Save pad data
		ptr[offset++] = static_cast<uint8_t>(pad.type);
		memcpy(&ptr[offset], &pad.valueIndex, sizeof(int32_t));
		offset += sizeof(int32_t);
		ptr[offset++] = static_cast<uint8_t>(pad.mode);
		ptr[offset++] = pad.active ? 1 : 0;
	}

	return offset;
}

bool SequencerControlState::restoreState(const void* buffer, size_t size) {
	const uint8_t* ptr = static_cast<const uint8_t*>(buffer);
	size_t offset = 0;

	// For each pad (excluding scene pads)
	for (size_t i = 0; i < pads_.size(); ++i) {
		auto& pad = pads_[i];

		// Skip scene pads
		if (pad.type == ControlType::SCENE) {
			continue;
		}

		// Check if we have enough data
		if (offset + 7 > size) {
			return false;
		}

		// Restore pad data
		pad.type = static_cast<ControlType>(ptr[offset++]);
		memcpy(&pad.valueIndex, &ptr[offset], sizeof(int32_t));
		offset += sizeof(int32_t);
		pad.mode = static_cast<PadMode>(ptr[offset++]);
		pad.active = (ptr[offset++] != 0);
	}

	return true;
}

} // namespace deluge::model::clip::sequencer
