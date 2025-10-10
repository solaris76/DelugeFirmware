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
	
	int16_t mpeValues[kNumExpressionDimensions];
	memset(mpeValues, 0, sizeof(mpeValues));
	
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithThreeMainThings* modelStackWithThreeMainThings = 
	    setupModelStackWithThreeMainThingsButNoNoteRow(modelStackMemory, modelStack->song, 
	                                                   instrument->toModControllable(), clip, &clip->paramManager);
	
	instrument->sendNote(modelStackWithThreeMainThings, false, noteCode, mpeValues,
	                    MIDI_CHANNEL_NONE, 64, 0, 0);
}

} // namespace deluge::model::clip::sequencer
