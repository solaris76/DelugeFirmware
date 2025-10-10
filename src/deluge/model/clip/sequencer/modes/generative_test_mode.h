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

#include "model/clip/sequencer/sequencer_mode.h"
#include "gui/l10n/l10n.h"

namespace deluge::model::clip::sequencer::modes {

/**
 * Simple test sequencer mode that lights up pads in a pattern
 * to demonstrate the sequencer mode system working.
 */
class GenerativeTestMode : public SequencerMode {
public:
	GenerativeTestMode() = default;
	~GenerativeTestMode() override = default;

	// Core identification
	l10n::String name() override { return l10n::String::STRING_FOR_GENERATIVE_1; }

	// For now, just support instrument tracks (Synth/MIDI/CV)
	bool supportsInstrument() override { return true; }
	bool supportsKit() override { return false; }
	bool supportsMIDI() override { return true; }
	bool supportsCV() override { return true; }
	bool supportsAudio() override { return false; }

	// Test functionality - light up pads in a simple pattern
	void initialize() override;
	void cleanup() override;

	// Override rendering to show test pattern
	bool renderPads(uint32_t whichRows, RGB* image, uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
	               int32_t xScroll, uint32_t xZoom, int32_t renderWidth, int32_t imageWidth) override;

	// Override playback to generate random notes every 16th note
	int32_t processPlayback(void* modelStack, int32_t ticksElapsed) override;

private:
	// Simple test state
	bool initialized_ = false;
	
	// Timing
	int32_t ticksPerSixteenthNote_ = 0;
	int32_t lastAbsolutePlaybackPos_ = 0; // For position indicator
	
	// Track the last note we played for note-off
	int32_t lastNoteCode_ = -1;
};

} // namespace deluge::model::clip::sequencer::modes
