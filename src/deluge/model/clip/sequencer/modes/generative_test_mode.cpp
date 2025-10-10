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

#include "model/clip/sequencer/modes/generative_test_mode.h"
#include "model/clip/sequencer/sequencer_mode_manager.h"

namespace deluge::model::clip::sequencer::modes {

void GenerativeTestMode::initialize() {
	initialized_ = true;
	// TODO: Add initialization logic when we connect to clip system
}

void GenerativeTestMode::cleanup() {
	initialized_ = false;
	// TODO: Add cleanup logic when we connect to clip system
}

bool GenerativeTestMode::renderPads(uint32_t whichRows, RGB* image, uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
                                   int32_t xScroll, uint32_t xZoom, int32_t renderWidth, int32_t imageWidth) {
	// Light up pads in a simple test pattern to show the mode is active

	for (int32_t yDisplay = 0; yDisplay < kDisplayHeight; yDisplay++) {
		if (whichRows & (1 << yDisplay)) {

			// Create a simple pattern - light up every 4th pad in a diagonal
			for (int32_t xDisplay = 0; xDisplay < renderWidth; xDisplay++) {

				// Clear the row first
				image[yDisplay * imageWidth + xDisplay] = {0, 0, 0};
				if (occupancyMask) {
					occupancyMask[yDisplay][xDisplay] = 0;
				}

				// Light up diagonal pattern - every 4th pad, offset by row
				if ((xDisplay + yDisplay) % 4 == 0) {
					// Use a bright purple color to make it obvious this is our test mode
					image[yDisplay * imageWidth + xDisplay] = {255, 0, 255}; // Bright magenta
					if (occupancyMask) {
						occupancyMask[yDisplay][xDisplay] = 64; // Full occupancy
					}
				}
			}
		}
	}

	return true; // We handled the rendering
}

} // namespace deluge::model::clip::sequencer::modes

// Register this mode with the manager
namespace {
	static auto registered_generative_test_mode = []() {
		deluge::model::clip::sequencer::SequencerModeManager::instance().registerMode<deluge::model::clip::sequencer::modes::GenerativeTestMode>("generative_test");
		return true;
	}();
}
