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
#include "hid/display/display.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"

namespace deluge::model::clip::sequencer {

SequencerControlState::SequencerControlState() {
	initialize();
}

void SequencerControlState::initialize() {
	// Default configuration:
	// Group 0 (x14 top): Clock Divider
	// Group 1 (x14 bottom): Octave
	// Group 2 (x15 top): Transpose
	// Group 3 (x15 bottom): Clock Divider
	groups_[0].initialize(ControlType::CLOCK_DIV);
	groups_[1].initialize(ControlType::OCTAVE);
	groups_[2].initialize(ControlType::TRANSPOSE);
	groups_[3].initialize(ControlType::CLOCK_DIV);
}

void SequencerControlState::render(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// x16 column (kDisplayWidth = 16)
	groups_[0].render(image, kDisplayWidth, 4);     // Top group (y4-y7)
	groups_[1].render(image, kDisplayWidth, 0);     // Bottom group (y0-y3)

	// x17 column
	groups_[2].render(image, kDisplayWidth + 1, 4); // Top group (y4-y7)
	groups_[3].render(image, kDisplayWidth + 1, 0); // Bottom group (y0-y3)
}

bool SequencerControlState::mapToGroup(int32_t x, int32_t y, int32_t& groupIndex, int32_t& yLocal) const {
	// Check if this is a control column pad (x16 or x17)
	if (x != kDisplayWidth && x != (kDisplayWidth + 1)) {
		return false;
	}

	// Determine which group based on column and y position
	bool isX16 = (x == kDisplayWidth);
	bool isTopHalf = (y >= 4);

	if (isX16) {
		if (isTopHalf) {
			groupIndex = 0; // x16 top
			yLocal = y - 4; // Map y4-y7 to 0-3
		}
		else {
			groupIndex = 1; // x16 bottom
			yLocal = y;     // Map y0-y3 to 0-3
		}
	}
	else { // x17
		if (isTopHalf) {
			groupIndex = 2; // x17 top
			yLocal = y - 4;
		}
		else {
			groupIndex = 3; // x17 bottom
			yLocal = y;
		}
	}

	return true;
}

bool SequencerControlState::handlePad(int32_t x, int32_t y, int32_t velocity) {
	int32_t groupIndex, yLocal;
	if (!mapToGroup(x, y, groupIndex, yLocal)) {
		return false;
	}

	return groups_[groupIndex].handlePad(yLocal, velocity);
}

bool SequencerControlState::handleHorizontalEncoder(int32_t heldX, int32_t heldY, int32_t offset) {
	// Check if holding a "mode switch" pad (y7 or y3)
	if (heldY != 7 && heldY != 3) {
		return false;
	}

	// Determine which group to change
	int32_t groupIndex = -1;
	if (heldX == kDisplayWidth) {
		// x16 column
		groupIndex = (heldY == 7) ? 0 : 1; // Top or bottom group
	}
	else if (heldX == (kDisplayWidth + 1)) {
		// x17 column
		groupIndex = (heldY == 7) ? 2 : 3; // Top or bottom group
	}
	else {
		return false;
	}

	// Cycle control type
	auto& group = groups_[groupIndex];
	int32_t currentType = static_cast<int32_t>(group.getType());
	int32_t maxType = static_cast<int32_t>(ControlType::TRANSPOSE) + 1; // Skip SCENE for now

	int32_t newType = currentType + (offset > 0 ? 1 : -1);
	if (newType < 0) {
		newType = maxType - 1;
	}
	else if (newType >= maxType) {
		newType = 0;
	}

	group.setType(static_cast<ControlType>(newType));

	// Show popup
	if (display) {
		display->displayPopup(group.getTypeName());
	}

	// Trigger UI refresh to update pad colors
	uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0);

	return true;
}

bool SequencerControlState::handleVerticalEncoder(int32_t heldX, int32_t heldY, int32_t offset) {
	int32_t groupIndex, yLocal;
	if (!mapToGroup(heldX, heldY, groupIndex, yLocal)) {
		return false;
	}

	return groups_[groupIndex].handleVerticalEncoder(yLocal, offset);
}

bool SequencerControlState::handleVerticalEncoderButton(int32_t heldX, int32_t heldY) {
	int32_t groupIndex, yLocal;
	if (!mapToGroup(heldX, heldY, groupIndex, yLocal)) {
		return false;
	}

	return groups_[groupIndex].handleVerticalEncoderButton(yLocal);
}

bool SequencerControlState::isAnyPadHeld() const {
	for (const auto& group : groups_) {
		if (group.getActivePad() >= 0) {
			return true;
		}
	}
	return false;
}

CombinedEffects SequencerControlState::getCombinedEffects() const {
	CombinedEffects effects;

	// Collect effects from all active groups
	for (const auto& group : groups_) {
		if (group.isActive()) {
			switch (group.getType()) {
			case ControlType::CLOCK_DIV:
				effects.clockDivider = group.getClockDivider();
				break;
			case ControlType::OCTAVE:
				effects.octaveShift += group.getOctaveShift();
				break;
			case ControlType::TRANSPOSE:
				effects.transpose += group.getTranspose();
				break;
			case ControlType::SCENE:
				effects.sceneIndex = group.getActiveValue();
				break;
			default:
				break;
			}
		}
	}

	return effects;
}

SequencerControlGroup& SequencerControlState::getGroup(int32_t groupIndex) {
	return groups_[groupIndex];
}

const SequencerControlGroup& SequencerControlState::getGroup(int32_t groupIndex) const {
	return groups_[groupIndex];
}

} // namespace deluge::model::clip::sequencer

