/*
 * Copyright © 2019-2023 Synthstrom Audible Limited
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

#include "random_sequencer_clip_view.h"
#include "gui/colour/colour.h"
#include "gui/ui/sound_editor.h"
#include "model/clip/instrument_clip.h"
#include "model/clip/sequencer_clip.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include <algorithm>
#include <array>

RandomSequencerClipView randomSequencerClipView{};

void RandomSequencerClipView::renderSequencerControls(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// With new approach, work with InstrumentClip in generative mode
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || !clip->inGenerativeMode) {
		return;
	}

	// Clear the main pad area first
	for (int y = 0; y < kDisplayHeight; y++) {
		for (int x = 0; x < kDisplayWidth; x++) {
			image[y][x] = RGB{0, 0, 0};
		}
	}

	// Render density control on top row
	renderDensityControl(image);

	// Add some distinctive colored pads to show we're in generative mode
	// Bottom row: Show some colored indicators
	for (int x = 0; x < 4; x++) {
		image[kDisplayHeight - 1][x] = RGB{0, 255, 0}; // Green indicators
	}

	// Right side: Show some pattern indicators
	for (int y = 2; y < 6; y++) {
		image[y][kDisplayWidth - 1] = RGB{255, 0, 255}; // Magenta pattern indicators
	}

	// Center area: Show some generative activity
	image[3][7] = RGB{255, 255, 0}; // Yellow center
	image[4][8] = RGB{255, 255, 0}; // Yellow center
}

void RandomSequencerClipView::handleEncoderTurn(int32_t offset) {
	handleDensityChange(offset);
}

void RandomSequencerClipView::renderOLEDInfo(deluge::hid::display::oled_canvas::Canvas& canvas) {
	// Don't override with density info - let the base class show "SYNTH: SEQ" format
	// The density info can be shown elsewhere or on demand
}

void RandomSequencerClipView::renderDensityControl(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// With new approach, work with InstrumentClip in generative mode
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || !clip->inGenerativeMode) {
		return;
	}

	// For now, show a fixed density of 50% - we can make this adjustable later
	uint32_t density = 50; // Fixed for now
	int32_t densityBars = (static_cast<int32_t>(density) * kDisplayWidth) / 100;

	for (int x = 0; x < kDisplayWidth; x++) {
		if (x < densityBars) {
			image[0][x] = RGB{255, 255, 0}; // Yellow for density bars
		}
		else {
			image[0][x] = RGB{32, 32, 32}; // Dark gray for empty
		}
	}
}

void RandomSequencerClipView::handleDensityChange(int32_t offset) {
	SequencerClip* clip = getCurrentSequencerClip();
	if (!clip) {
		return;
	}

	SequencerSettings& settings = clip->getSettings();
	int32_t newDensity = static_cast<int32_t>(settings.density) + offset;
	newDensity = (newDensity < 0) ? 0 : (newDensity > 100) ? 100 : newDensity;
	settings.density = newDensity;
}
