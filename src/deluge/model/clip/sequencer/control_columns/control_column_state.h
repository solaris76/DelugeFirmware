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

#include "model/clip/sequencer/control_columns/sequencer_control_column.h"
#include <algorithm>
#include <cstring>

namespace deluge::model::clip::sequencer {

/**
 * State for both control columns (left x16 and right x17 sidebars).
 * Manages the two independent columns and provides combined control values.
 */
struct ControlColumnState {
	SequencerControlColumn leftColumn;
	SequencerControlColumn rightColumn;

	/**
	 * Get combined active controls from both columns.
	 * Effects from both columns are stacked/combined:
	 * - Clock div: multiplied together
	 * - Octave/transpose: added together
	 * - Probability: most restrictive (minimum)
	 * - Velocity/gate length: multiplied together
	 * - Swing: averaged
	 */
	SequencerControlColumn::ActiveControls getActiveControls() const {
		auto left = leftColumn.getActiveControls();
		auto right = rightColumn.getActiveControls();

		// Combine effects from both columns
		SequencerControlColumn::ActiveControls combined;

		// Multiplicative effects
		combined.clockDivMultiplier = left.clockDivMultiplier * right.clockDivMultiplier;
		combined.velocityMultiplier = left.velocityMultiplier * right.velocityMultiplier;
		combined.gateLengthMultiplier = left.gateLengthMultiplier * right.gateLengthMultiplier;

		// Additive effects
		combined.octaveShift = left.octaveShift + right.octaveShift;
		combined.transpose = left.transpose + right.transpose;

		// Most restrictive probability
		combined.probability = std::min(left.probability, right.probability);

		// Average swing
		combined.swing = (left.swing + right.swing) / 2.0f;

		return combined;
	}

	/**
	 * Serialization - write both columns to XML.
	 */
	void writeToFile(Serializer& writer) const {
		leftColumn.writeToFile(writer, "leftControlColumn");
		rightColumn.writeToFile(writer, "rightControlColumn");
	}

	/**
	 * Deserialization - read both columns from XML.
	 */
	void readFromFile(Deserializer& reader);
};

} // namespace deluge::model::clip::sequencer

