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
#include "hid/led/pad_leds.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"

namespace deluge::model::clip::sequencer {

namespace {
	// Helper: Request UI refresh for sidebar
	void refreshSidebar() {
		uiNeedsRendering(&instrumentClipView, 0, 0xFFFFFFFF);
		PadLEDs::sendOutSidebarColoursSoon();
	}
}

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

void SequencerControlState::render(RGB image[][kDisplayWidth + kSideBarWidth],
                                    uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	// x16 column (kDisplayWidth = 16)
	groups_[0].render(image, kDisplayWidth, 4);     // Top group (y4-y7)
	groups_[1].render(image, kDisplayWidth, 0);     // Bottom group (y0-y3)

	// x17 column
	groups_[2].render(image, kDisplayWidth + 1, 4); // Top group (y4-y7)
	groups_[3].render(image, kDisplayWidth + 1, 0); // Bottom group (y0-y3)

	// Set occupancy mask for all control column pads
	if (occupancyMask) {
		for (int32_t y = 0; y < kDisplayHeight; y++) {
			occupancyMask[y][kDisplayWidth] = 64;     // x16
			occupancyMask[y][kDisplayWidth + 1] = 64; // x17
		}
	}
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

bool SequencerControlState::handlePad(int32_t x, int32_t y, int32_t velocity, SequencerMode* mode) {
	int32_t groupIndex, yLocal;
	if (!mapToGroup(x, y, groupIndex, yLocal)) {
		return false;
	}

	bool handled = groups_[groupIndex].handlePad(yLocal, velocity, mode, groupIndex);
	if (handled) {
		refreshSidebar();
	}
	return handled;
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

	// Available types for cycling
	constexpr int32_t availableTypes[] = {
		static_cast<int32_t>(ControlType::CLOCK_DIV),
		static_cast<int32_t>(ControlType::OCTAVE),
		static_cast<int32_t>(ControlType::TRANSPOSE),
		static_cast<int32_t>(ControlType::SCENE),
		static_cast<int32_t>(ControlType::GENERATIVE)
	};
	constexpr int32_t numTypes = sizeof(availableTypes) / sizeof(availableTypes[0]);

	// Find current type index
	int32_t currentIndex = 0;
	for (int32_t i = 0; i < numTypes; i++) {
		if (availableTypes[i] == currentType) {
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

	group.setType(static_cast<ControlType>(availableTypes[newIndex]));

	if (display) {
		display->displayPopup(group.getTypeName());
	}

	refreshSidebar();
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

size_t SequencerControlState::captureState(void* buffer, size_t maxSize, int32_t excludeGroupIndex) const {
	uint8_t* ptr = static_cast<uint8_t*>(buffer);
	size_t offset = 0;

	// Store which groups we're saving (as a bitmask)
	uint8_t groupMask = 0;
	for (size_t i = 0; i < groups_.size(); ++i) {
		if (static_cast<int32_t>(i) != excludeGroupIndex) {
			groupMask |= (1 << i);
		}
	}
	ptr[offset++] = groupMask;

	// For each group (except the excluded one)
	for (size_t i = 0; i < groups_.size(); ++i) {
		if (static_cast<int32_t>(i) == excludeGroupIndex) {
			continue; // Skip scene group itself
		}

		const auto& group = groups_[i];

		// Check buffer space (1 byte type + 16 bytes valueIndex + 4 bytes mode = 21 bytes per group)
		if (offset + 21 > maxSize) {
			return 0; // Not enough space
		}

		// Save control type (1 byte)
		ptr[offset++] = static_cast<uint8_t>(group.getType());

		// Save each pad's valueIndex (4 * 4 bytes = 16 bytes)
		for (int32_t padIdx = 0; padIdx < 4; ++padIdx) {
			int32_t valueIndex = group.getPadValueIndex(padIdx);
			memcpy(&ptr[offset], &valueIndex, sizeof(int32_t));
			offset += sizeof(int32_t);
		}

		// Save each pad's mode (4 * 1 byte = 4 bytes)
		for (int32_t padIdx = 0; padIdx < 4; ++padIdx) {
			ptr[offset++] = static_cast<uint8_t>(group.getPadMode(padIdx));
		}
	}

	return offset;
}

bool SequencerControlState::restoreState(const void* buffer, size_t size) {
	const uint8_t* ptr = static_cast<const uint8_t*>(buffer);
	size_t offset = 0;

	// Check minimum size
	if (size < 1) {
		return false;
	}

	// Read which groups were saved (bitmask)
	uint8_t groupMask = ptr[offset++];

	// For each group that was saved
	for (size_t i = 0; i < groups_.size(); ++i) {
		// Check if this group was saved
		if ((groupMask & (1 << i)) == 0) {
			continue; // This group was not saved (it was the scene group)
		}

		auto& group = groups_[i];

		// Check if we have enough data
		if (offset + 21 > size) {
			return false; // Not enough data
		}

		// Restore control type (1 byte)
		ControlType type = static_cast<ControlType>(ptr[offset++]);
		group.setType(type);

		// Restore each pad's valueIndex (4 * 4 bytes = 16 bytes)
		for (int32_t padIdx = 0; padIdx < 4; ++padIdx) {
			int32_t valueIndex;
			memcpy(&valueIndex, &ptr[offset], sizeof(int32_t));
			offset += sizeof(int32_t);
			group.setPadValueIndex(padIdx, valueIndex);
		}

		// Restore each pad's mode (4 * 1 byte = 4 bytes)
		for (int32_t padIdx = 0; padIdx < 4; ++padIdx) {
			PadMode mode = static_cast<PadMode>(ptr[offset++]);
			group.setPadMode(padIdx, mode);
		}
	}

	return true;
}

} // namespace deluge::model::clip::sequencer

