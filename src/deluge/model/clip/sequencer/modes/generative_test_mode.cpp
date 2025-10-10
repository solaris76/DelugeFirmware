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
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include "model/scale/musical_key.h"
#include "playback/playback_handler.h"
#include "util/functions.h"

namespace deluge::model::clip::sequencer::modes {

void GenerativeTestMode::initialize() {
	initialized_ = true;
	lastNoteCode_ = -1;
	ticksPerSixteenthNote_ = 0; // Will be calculated during first playback call
}

void GenerativeTestMode::cleanup() {
	initialized_ = false;
	ticksPerSixteenthNote_ = 0;
	lastNoteCode_ = -1;
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

int32_t GenerativeTestMode::processPlayback(void* modelStackPtr, int32_t absolutePlaybackPos) {
	if (!initialized_) {
		return 2147483647; // Not ready, come back never
	}

	// Cast the model stack
	ModelStackWithTimelineCounter* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);
	InstrumentClip* clip = static_cast<InstrumentClip*>(modelStack->getTimelineCounter());

	// Only work with melodic instruments for now
	if (clip->output->type != OutputType::SYNTH && clip->output->type != OutputType::MIDI_OUT && clip->output->type != OutputType::CV) {
		return 2147483647;
	}

	// Calculate 16th note ticks using Song's built-in method
	// This accounts for song resolution/tick magnitude automatically
	if (ticksPerSixteenthNote_ == 0) {
		ticksPerSixteenthNote_ = modelStack->song->getSixteenthNoteLength();
	}

	MelodicInstrument* instrument = static_cast<MelodicInstrument*>(clip->output);

	// Use helper to check if we're at a 16th note boundary using ABSOLUTE playback position
	bool atBoundary = atDivisionBoundary(absolutePlaybackPos, ticksPerSixteenthNote_);

	// Only play a note if we're AT the boundary
	if (atBoundary) {
		// Stop previous note if any
		if (lastNoteCode_ >= 0) {
			stopNote(modelStackPtr, lastNoteCode_);
			lastNoteCode_ = -1;
		}
		
		// Get all scale notes across 2 octaves
		int32_t scaleNotes[32]; // Max notes: 12 semitones * 2 octaves + some buffer
		int32_t numNotes = getScaleNotes(modelStackPtr, scaleNotes, 32, 2, 0);
		
		if (numNotes > 0) {
			// Pick a random note from the scale
			int32_t randomNote = scaleNotes[getRandom255() % numNotes];
			
			// Random velocity (64-127 for some dynamics)
			uint8_t velocity = 64 + (getRandom255() % 64);
			
			// Note length: 75% of a 16th note (staccato feel)
			int32_t noteLength = (ticksPerSixteenthNote_ * 3) / 4;
			
			// Play the note
			playNote(modelStackPtr, randomNote, velocity, noteLength);
			
			// Remember this note
			lastNoteCode_ = randomNote;
		}
	}

	// Use helper to calculate when we need to be called next (based on absolute position)
	return ticksUntilNextDivision(absolutePlaybackPos, ticksPerSixteenthNote_);
}

} // namespace deluge::model::clip::sequencer::modes

// Register this mode with the manager
namespace {
	static auto registered_generative_test_mode = []() {
		deluge::model::clip::sequencer::SequencerModeManager::instance().registerMode<deluge::model::clip::sequencer::modes::GenerativeTestMode>("generative_test");
		return true;
	}();
}
