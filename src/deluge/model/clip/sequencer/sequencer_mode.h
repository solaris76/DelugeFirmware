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
#include "gui/l10n/l10n.h"
#include "hid/led/pad_leds.h"

namespace deluge::model::clip::sequencer {

/**
 * Base class for alternative sequencer modes that can replace linear clip playback
 * with pattern-based, algorithmic, or other non-linear sequencing approaches.
 *
 * This is the foundation for step sequencers, euclidean sequencers, granular modes,
 * generative sequencers, and other creative sequencing paradigms.
 */
class SequencerMode {
public:
	virtual ~SequencerMode() = default;

	// Core identification
	virtual l10n::String name() = 0;

	// Minimal interface - will be expanded in future steps
	virtual void initialize() {}
	virtual void cleanup() {}

	// Rendering - allow sequencer modes to override pad display
	// Returns true if the mode handled rendering, false to use default linear rendering
	virtual bool renderPads(uint32_t whichRows, RGB* image, uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
	                       int32_t xScroll, uint32_t xZoom, int32_t renderWidth, int32_t imageWidth) { return false; }

	// Track type compatibility (default: support all)
	virtual bool supportsInstrument() { return true; }
	virtual bool supportsKit() { return true; }
	virtual bool supportsMIDI() { return true; }
	virtual bool supportsCV() { return true; }
	virtual bool supportsAudio() { return false; } // Audio modes need special handling

protected:
	// Protected constructor - only concrete implementations can be instantiated
	SequencerMode() = default;
};

} // namespace deluge::model::clip::sequencer
