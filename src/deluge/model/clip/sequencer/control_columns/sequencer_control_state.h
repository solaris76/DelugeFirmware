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
};

// Manages all 4 control groups for a sequencer mode
class SequencerControlState {
public:
	SequencerControlState();

	// Initialize with default control types
	void initialize();

	// Rendering
	void render(RGB image[][kDisplayWidth + kSideBarWidth]);

	// Input handling - returns which group (if any) handled the input
	bool handlePad(int32_t x, int32_t y, int32_t velocity);
	bool handleHorizontalEncoder(int32_t heldX, int32_t heldY, int32_t offset);
	bool handleVerticalEncoder(int32_t heldX, int32_t heldY, int32_t offset);
	bool handleVerticalEncoderButton(int32_t heldX, int32_t heldY);

	// Get combined effects from all active groups
	CombinedEffects getCombinedEffects() const;

	// Check if any control column pad is currently held (for overriding scroll)
	bool isAnyPadHeld() const;

	// Group access
	SequencerControlGroup& getGroup(int32_t groupIndex); // 0-3
	const SequencerControlGroup& getGroup(int32_t groupIndex) const;

private:
	// 4 control groups:
	// [0] = x16 top (y4-y7)
	// [1] = x16 bottom (y0-y3)
	// [2] = x17 top (y4-y7)
	// [3] = x17 bottom (y0-y3)
	std::array<SequencerControlGroup, 4> groups_;

	// Helper to map x,y to group index and local y
	bool mapToGroup(int32_t x, int32_t y, int32_t& groupIndex, int32_t& yLocal) const;
};

} // namespace deluge::model::clip::sequencer

