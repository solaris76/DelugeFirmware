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

#include "model/clip/sequencer/sequencer_mode.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include "util/functions.h"

namespace deluge::model::clip::sequencer {

// ========== HELPER IMPLEMENTATIONS ==========

int32_t SequencerMode::getScaleNotes(void* modelStackPtr, int32_t* noteArray, int32_t maxNotes,
                                     int32_t octaveRange, int32_t baseOctave) {
	ModelStackWithTimelineCounter* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);
	InstrumentClip* clip = static_cast<InstrumentClip*>(modelStack->getTimelineCounter());

	int32_t noteCount = 0;

	if (clip->inScaleMode) {
		// In scale mode - fill array with scale notes
		MusicalKey& key = modelStack->song->key;
		int32_t scaleSize = key.modeNotes.count();

		// Fill array with scale notes across octaves
		for (int32_t octave = 0; octave < octaveRange && noteCount < maxNotes; octave++) {
			for (int32_t degree = 0; degree < scaleSize && noteCount < maxNotes; degree++) {
				int32_t semitone = key.modeNotes[degree];
				int32_t note = key.rootNote + ((baseOctave + octave) * 12) + semitone;

				// Only add if in MIDI range
				if (note >= 0 && note <= 127) {
					noteArray[noteCount++] = note;
				}
			}
		}
	} else {
		// Chromatic mode - all 12 notes per octave
		for (int32_t octave = 0; octave < octaveRange && noteCount < maxNotes; octave++) {
			for (int32_t semitone = 0; semitone < 12 && noteCount < maxNotes; semitone++) {
				int32_t note = 60 + ((baseOctave + octave) * 12) + semitone;

				// Only add if in MIDI range
				if (note >= 0 && note <= 127) {
					noteArray[noteCount++] = note;
				}
			}
		}
	}

	return noteCount;
}

uint8_t SequencerMode::applyVelocitySpread(uint8_t baseVelocity, int32_t spread) {
	if (spread == 0) {
		return baseVelocity;
	}

	// Random variation ± spread amount
	int32_t variation = (getRandom255() % (spread * 2 + 1)) - spread;
	int32_t newVelocity = baseVelocity + variation;

	// Clamp to valid MIDI velocity range
	if (newVelocity < 1) newVelocity = 1;
	if (newVelocity > 127) newVelocity = 127;

	return static_cast<uint8_t>(newVelocity);
}

bool SequencerMode::shouldPlayBasedOnProbability(int32_t probability) {
	if (probability >= 100) {
		return true; // Always play
	}
	if (probability <= 0) {
		return false; // Never play
	}

	// Random check: probability is 0-100
	return (getRandom255() % 100) < probability;
}

void SequencerMode::playNote(void* modelStackPtr, int32_t noteCode, uint8_t velocity, int32_t length) {
	ModelStackWithTimelineCounter* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);
	InstrumentClip* clip = static_cast<InstrumentClip*>(modelStack->getTimelineCounter());
	MelodicInstrument* instrument = static_cast<MelodicInstrument*>(clip->output);

	int16_t mpeValues[kNumExpressionDimensions];
	memset(mpeValues, 0, sizeof(mpeValues));

	char newModelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    setupModelStackWithThreeMainThingsButNoNoteRow(newModelStackMemory, modelStack->song,
	                                                   instrument->toModControllable(), clip, &clip->paramManager);

	instrument->sendNote(modelStackWithThreeMainThings, true, noteCode, mpeValues,
	                    MIDI_CHANNEL_NONE, velocity, length, 0);
}

void SequencerMode::stopNote(void* modelStackPtr, int32_t noteCode) {
	ModelStackWithTimelineCounter* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);
	InstrumentClip* clip = static_cast<InstrumentClip*>(modelStack->getTimelineCounter());
	MelodicInstrument* instrument = static_cast<MelodicInstrument*>(clip->output);

	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    setupModelStackWithThreeMainThingsButNoNoteRow(modelStackMemory, modelStack->song,
	                                                   instrument->toModControllable(), clip, &clip->paramManager);

	instrument->sendNote(modelStackWithThreeMainThings, false, noteCode, nullptr,
	                    MIDI_CHANNEL_NONE, 64, 0, 0);
}

void SequencerMode::renderPlaybackPosition(RGB* image, uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
                                           int32_t imageWidth, int32_t absolutePlaybackPos, int32_t totalLength,
                                           RGB color, bool enabled) {
	if (!enabled || totalLength == 0) {
		return;
	}

	// Calculate which pad on y7 should be lit (0-15)
	int32_t positionInPattern = absolutePlaybackPos % totalLength;
	int32_t padX = (positionInPattern * kDisplayWidth) / totalLength;

	// Clamp to valid range
	if (padX < 0) padX = 0;
	if (padX >= kDisplayWidth) padX = kDisplayWidth - 1;

	// Light up the pad on y7
	int32_t y = 7;
	image[y * imageWidth + padX] = color;
	if (occupancyMask) {
		occupancyMask[y][padX] = 64;
	}
}

} // namespace deluge::model::clip::sequencer
