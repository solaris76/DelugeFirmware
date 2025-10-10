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

} // namespace deluge::model::clip::sequencer::modes

// Register this mode with the manager
namespace {
	static auto registered_generative_test_mode = []() {
		deluge::model::clip::sequencer::SequencerModeManager::instance().registerMode<deluge::model::clip::sequencer::modes::GenerativeTestMode>("generative_test");
		return true;
	}();
}
