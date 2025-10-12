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

#pragma once

#include "model/clip/sequencer/control_columns/sequencer_control_group.h"

namespace deluge::model::clip::sequencer {

// Combined effects from all active control groups
struct CombinedEffects {
	int32_t clockDivider = 1;   // From CLOCK_DIV group
	int32_t octaveShift = 0;    // From OCTAVE group
	int32_t transpose = 0;      // From TRANSPOSE group
	int32_t sceneIndex = -1;    // From SCENE group (-1 = none)
	int32_t direction = 0;      // From DIRECTION group (0=forward, 1=backward, 2=pingpong, 3=random)
};

// Individual pad configuration
struct ControlPad {
	ControlType type = ControlType::NONE;
	int32_t valueIndex = 0;
	PadMode mode = PadMode::TOGGLE;
	bool active = false;
	bool held = false;

	// Scene data (only used when type == SCENE)
	static constexpr size_t kMaxSceneDataSize = 512;
	uint8_t sceneData[kMaxSceneDataSize];
	size_t sceneSize = 0;
	bool sceneValid = false;
};

// Manages all 16 individual control pads for a sequencer mode
class SequencerControlState {
public:
	SequencerControlState();

	// Initialize with default control types
	void initialize();

	// Rendering
	void render(RGB image[][kDisplayWidth + kSideBarWidth], uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]);

	// Input handling
	bool handlePad(int32_t x, int32_t y, int32_t velocity, class SequencerMode* mode = nullptr);
	bool handleHorizontalEncoder(int32_t heldX, int32_t heldY, int32_t offset);
	bool handleVerticalEncoder(int32_t heldX, int32_t heldY, int32_t offset);
	bool handleVerticalEncoderButton(int32_t heldX, int32_t heldY);

	// Get combined effects from all active pads
	CombinedEffects getCombinedEffects() const;

	// Check if any control column pad is currently held
	bool isAnyPadHeld() const;

	// Scene capture/restore (excludes scene pads)
	size_t captureState(void* buffer, size_t maxSize) const;
	bool restoreState(const void* buffer, size_t size);

private:
	// 16 individual control pads:
	// [0-7] = x16 (y0-y7)
	// [8-15] = x17 (y0-y7)
	std::array<ControlPad, 16> pads_;

	// Helper to map x,y to pad index
	int32_t getPadIndex(int32_t x, int32_t y) const;
};

} // namespace deluge::model::clip::sequencer

