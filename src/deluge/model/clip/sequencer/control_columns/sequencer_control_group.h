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

#include "definitions_cxx.hpp"
#include "gui/ui/ui.h"
#include <array>

namespace deluge::model::clip::sequencer {

// Control types available for sequencer columns
enum class ControlType {
	CLOCK_DIV,
	OCTAVE,
	TRANSPOSE,
	SCENE, // Future
	GATE_LENGTH, // Step sequencer specific
	MAX
};

// Pad behavior mode
enum class PadMode {
	TOGGLE,     // Press to activate, press again to deactivate
	MOMENTARY   // Hold to activate, release to deactivate
};

// Represents a group of 4 control pads
class SequencerControlGroup {
public:
	SequencerControlGroup() = default;

	// Lifecycle
	void initialize(ControlType type);

	// Rendering
	void render(RGB image[][kDisplayWidth + kSideBarWidth], int32_t x, int32_t yStart);

	// Input handling
	bool handlePad(int32_t yLocal, int32_t velocity); // yLocal = 0-3 within group
	bool handleVerticalEncoder(int32_t yLocal, int32_t offset); // Adjust value for pad
	bool handleVerticalEncoderButton(int32_t yLocal); // Toggle momentary/toggle mode

	// Control type management
	void setType(ControlType newType);
	ControlType getType() const { return type_; }
	const char* getTypeName() const;

	// Value queries
	int32_t getActiveValue() const; // Returns value of active pad, or default
	bool isActive() const; // Returns true if any pad is active
	int32_t getActivePad() const { return activePad_; }
	bool isPadHeld(int32_t yLocal) const { return heldPad_ == yLocal; }

	// Get effect values (for applying to notes)
	int32_t getClockDivider() const;  // Returns divider if CLOCK_DIV, else 1
	int32_t getOctaveShift() const;   // Returns octave if OCTAVE, else 0
	int32_t getTranspose() const;     // Returns semitones if TRANSPOSE, else 0
	int32_t getGateLength() const;    // Returns percentage if GATE_LENGTH, else 75

private:
	struct PadData {
		int32_t valueIndex = 0;        // Index into available values array
		PadMode mode = PadMode::TOGGLE; // Toggle or momentary
	};

	ControlType type_ = ControlType::CLOCK_DIV;
	std::array<PadData, 4> pads_; // Per-pad configuration
	int32_t activePad_ = -1;  // Which pad is currently active (-1 = none)
	int32_t heldPad_ = -1;    // Which pad is currently held down (-1 = none)

	RGB getColorForType() const;
	const char* formatValue(int32_t value) const;
	int32_t getValue(int32_t padIndex) const; // Get actual value for pad
	const int32_t* getAvailableValues() const;
	int32_t getNumAvailableValues() const;
};

} // namespace deluge::model::clip::sequencer

