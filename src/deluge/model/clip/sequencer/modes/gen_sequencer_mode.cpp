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

#include "model/clip/sequencer/modes/gen_sequencer_mode.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/led/pad_leds.h"
#include "model/clip/instrument_clip.h"
#include "model/clip/sequencer/sequencer_mode_manager.h"
#include "model/model_stack.h"
#include "model/scale/musical_key.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include "storage/storage_manager.h"
#include "util/functions.h"
#include <cstdio>
#include <string_view>

namespace deluge::model::clip::sequencer::modes {

// Row masks for UI refresh
constexpr uint32_t kNoteTypeRow = (1 << 7);
constexpr uint32_t kNoteDensityRow = (1 << 6);
constexpr uint32_t kSequenceLengthRow = (1 << 5);
constexpr uint32_t kVelocitySpreadRow = (1 << 4);
constexpr uint32_t kGateLengthRow = (1 << 3);
constexpr uint32_t kAllRows = 0xFFFFFFFF;

void GenSequencerMode::initialize() {
	initialized_ = true;
	currentStep_ = 0;
	activeNoteCode_ = -1;
	ticksPerSixteenthNote_ = 0;
	lastAbsolutePlaybackPos_ = 0;
	sequenceNeedsRegeneration_ = true;

	// Clear the white progress column from normal clip mode
	// (set all tick squares to 255 = not displayed)
	uint8_t tickSquares[kDisplayHeight];
	uint8_t colours[kDisplayHeight];
	memset(tickSquares, 255, kDisplayHeight); // 255 = not displayed
	memset(colours, 0, kDisplayHeight);
	PadLEDs::setTickSquares(tickSquares, colours);

	// Update scale notes first
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
	ModelStackWithTimelineCounter* modelStackWithTimelineCounter = modelStack->addTimelineCounter(getCurrentClip());
	updateScaleNotes(modelStackWithTimelineCounter);

	// Initialize with defaults if current data matches the exact default pattern
	if (noteTypeRange_ == 0 && noteDensity_ == 0 && sequenceLength_ == 0 && velocitySpread_ == 0 && gateLength_ == 0) {
		resetToInit();
	}

	// Generate initial sequence
	generateSequence();
}

void GenSequencerMode::cleanup() {
	// Stop any playing notes before cleanup
	if (activeNoteCode_ >= 0) {
		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
		ModelStackWithTimelineCounter* modelStackWithTimelineCounter = modelStack->addTimelineCounter(getCurrentClip());
		stopNote(modelStackWithTimelineCounter, activeNoteCode_);
	}

	initialized_ = false;
	currentStep_ = 0;
	activeNoteCode_ = -1;
	numScaleNotes_ = 0;
}

void GenSequencerMode::updateScaleNotes(void* modelStackPtr) {
	// Use base class helper to get scale notes starting from C3 and spanning 3 octaves
	// This gives us notes from C3 to C5 (2 octaves higher) for maximum range
	// baseOctave=3 means C3, octaveRange=3 gives us C3, C4, C5
	numScaleNotes_ = getScaleNotes(modelStackPtr, scaleNotes_, kMaxScaleNotes, 3, 3);

	// If we got no notes, fall back to single octave
	if (numScaleNotes_ == 0) {
		ModelStackWithTimelineCounter* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);
		Song* song = modelStack->song;
		if (!song) {
			numScaleNotes_ = 0;
			return;
		}

		InstrumentClip* clip = static_cast<InstrumentClip*>(modelStack->getTimelineCounter());

		if (!clip || !clip->inScaleMode) {
			// Chromatic mode - all 12 notes
			numScaleNotes_ = 12;
			for (int32_t i = 0; i < 12; i++) {
				scaleNotes_[i] = i; // 0-11 (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
			}
		}
		else {
			// Scale mode - get notes from the scale (just degrees 0-11)
			NoteSet modeNotes = song->key.modeNotes;

			numScaleNotes_ = 0;
			for (int32_t i = 0; i < 12; i++) {
				if (modeNotes.has(i)) {
					scaleNotes_[numScaleNotes_++] = i;
				}
			}
		}
	}
}

int32_t GenSequencerMode::getNoteDensityPercent() const {
	// Map fader position (0-15) to percentage (0-100%)
	// 0 = 0%, 15 = 100%
	return (noteDensity_ * 100) / 15;
}

int32_t GenSequencerMode::getVelocitySpread() const {
	// Map fader position (0-15) to velocity spread (0-127)
	// 0 = 0, 15 = 127
	return (velocitySpread_ * 127) / 15;
}

int32_t GenSequencerMode::getGateLengthPercent() const {
	// Map fader position (0-15) to percentage (0-100%)
	// 0 = 0%, 15 = 100%
	return (gateLength_ * 100) / 15;
}

int32_t GenSequencerMode::getRandomNoteFromRange() const {
	if (numScaleNotes_ == 0)
		return 0;

	int32_t range = clampFader(noteTypeRange_);

	// Map fader position (0-15) to number of notes in range
	// Position 0 = 1 note, Position 15 = all notes (up to 2 octaves)
	// Linear mapping: position 0 -> 1 note, position 15 -> numScaleNotes_ notes
	int32_t numNotesInRange;
	if (range == 0) {
		numNotesInRange = 1; // Single note
	}
	else {
		// Map 1-15 to 2-numScaleNotes_ (linear interpolation)
		// Formula: 1 + (range * (numScaleNotes_ - 1)) / 15
		numNotesInRange = 1 + ((range * (numScaleNotes_ - 1)) / 15);
		if (numNotesInRange > numScaleNotes_)
			numNotesInRange = numScaleNotes_;
	}

	// Select a random note from the first numNotesInRange notes
	if (numNotesInRange <= 0)
		return 0;
	if (numNotesInRange == 1)
		return 0; // First note only

	// Randomly select from 0 to (numNotesInRange - 1)
	int32_t randomIndex = random(numNotesInRange - 1);
	return randomIndex;
}

int32_t GenSequencerMode::getSequenceLength() const {
	// Sequence length is stored as fader position (0-15), map to 1-16
	int32_t faderPos = clampFader(sequenceLength_);
	// Map 0-15 to 1-16
	return faderPos + 1;
}

void GenSequencerMode::generateSequence() {
	// Generate a sequence based on current parameters
	actualSequenceLength_ = getSequenceLength();
	if (actualSequenceLength_ < 1)
		actualSequenceLength_ = 1;
	if (actualSequenceLength_ > kMaxSequenceLength)
		actualSequenceLength_ = kMaxSequenceLength;

	int32_t probability = getNoteDensityPercent();

	// Generate each step in the sequence
	for (int32_t i = 0; i < actualSequenceLength_; i++) {
		// Check probability - should this step have a note?
		if (shouldPlayBasedOnProbability(probability)) {
			// Generate a note index from the enabled range
			generatedSequence_[i] = getRandomNoteFromRange();
		}
		else {
			// Rest/silence
			generatedSequence_[i] = -1;
		}
	}

	sequenceNeedsRegeneration_ = false;
}

int32_t GenSequencerMode::calculateNoteCode(int32_t noteIndexInScale, const CombinedEffects& effects) const {
	if (numScaleNotes_ == 0)
		return 60; // Default to middle C

	// scaleNotes_ now contains actual MIDI note values (from getScaleNotes across 2 octaves)
	// Apply transpose to note index (keeps us in scale!)
	int32_t transposedIndex = noteIndexInScale + effects.transpose;

	// Wrap to scale
	while (transposedIndex < 0)
		transposedIndex += numScaleNotes_;
	while (transposedIndex >= numScaleNotes_)
		transposedIndex -= numScaleNotes_;

	// Get the actual MIDI note value from the scale notes array
	int32_t noteCode = scaleNotes_[transposedIndex];

	// Apply octave shift from control columns
	noteCode += (effects.octaveShift * 12);

	// Clamp to MIDI range
	if (noteCode < 0)
		noteCode = 0;
	if (noteCode > 127)
		noteCode = 127;

	return noteCode;
}

void GenSequencerMode::advanceStep(int32_t direction) {
	if (actualSequenceLength_ < 1)
		actualSequenceLength_ = 1;

	switch (direction) {
	case 0: // Forward
		currentStep_ = (currentStep_ + 1) % actualSequenceLength_;
		break;

	case 1: // Backward
		currentStep_ = (currentStep_ - 1);
		if (currentStep_ < 0)
			currentStep_ = actualSequenceLength_ - 1;
		break;

	case 2: // Ping Pong
		static int32_t pingPongDirection = 1;
		currentStep_ += pingPongDirection;
		if (currentStep_ >= actualSequenceLength_) {
			currentStep_ = actualSequenceLength_ - 2;
			pingPongDirection = -1;
		}
		else if (currentStep_ < 0) {
			currentStep_ = 1;
			pingPongDirection = 1;
		}
		break;

	case 3: // Random
		currentStep_ = random(actualSequenceLength_ - 1);
		break;

	default: // Fallback to forward
		currentStep_ = (currentStep_ + 1) % actualSequenceLength_;
		break;
	}
}

bool GenSequencerMode::renderPads(uint32_t whichRows, RGB* image,
                                  uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], int32_t xScroll,
                                  uint32_t xZoom, int32_t renderWidth, int32_t imageWidth) {

	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
	ModelStackWithTimelineCounter* modelStackWithTimelineCounter = modelStack->addTimelineCounter(getCurrentClip());

	// Always update scale notes
	updateScaleNotes(modelStackWithTimelineCounter);

	// Render faders - same value across all columns (x0-x15)
	// Show playback position indicator on top row if needed
	int32_t seqLength = getSequenceLength();
	bool showPlaybackPos = (currentStep_ >= 0 && currentStep_ < seqLength);

	for (int32_t x = 0; x < kDisplayWidth; x++) {
		for (int32_t y = 0; y < kDisplayHeight; y++) {
			if (!(whichRows & (1 << y)))
				continue;

			RGB color{0, 0, 0};

			// y7: Note type range fader (all pads from 0 to noteTypeRange_ are lit)
			if (y == 7) {
				if (x <= noteTypeRange_) {
					// Show playback position indicator if this is the current step
					if (showPlaybackPos && x == currentStep_) {
						color = RGB{255, 0, 0}; // Red for current step
					}
					else {
						color = RGB{0, 255, 255}; // Cyan for enabled
					}
				}
				else {
					color = RGB{0, 0, 0}; // Black for disabled
				}
			}
			// y6: Note density fader (all pads from 0 to noteDensity_ are lit)
			else if (y == 6) {
				if (x <= noteDensity_) {
					// Color intensity based on density position (0-15)
					uint8_t intensity = ((noteDensity_ + 1) * 255) / 16;
					// Show playback position indicator if this is the current step
					if (showPlaybackPos && x == currentStep_) {
						color = RGB{255, 0, 0}; // Red for current step
					}
					else {
						color = RGB{0, intensity, 0}; // Green intensity based on density
					}
				}
				else {
					color = RGB{0, 0, 0};
				}
			}
			// y5: Sequence length fader (all pads from 0 to sequenceLength_ are lit)
			else if (y == 5) {
				if (x <= sequenceLength_) {
					// Color intensity based on length position (0-15)
					uint8_t intensity = ((sequenceLength_ + 1) * 255) / 16;
					// Show playback position indicator if this is the current step
					if (showPlaybackPos && x == currentStep_) {
						color = RGB{255, 0, 0}; // Red for current step
					}
					else {
						color = RGB{intensity, intensity, 0}; // Yellow intensity based on length
					}
				}
				else {
					color = RGB{0, 0, 0};
				}
			}
			// y4: Velocity spread fader (all pads from 0 to velocitySpread_ are lit)
			else if (y == 4) {
				if (x <= velocitySpread_) {
					// Color intensity based on spread position (0-15)
					uint8_t intensity = ((velocitySpread_ + 1) * 255) / 16;
					// Show playback position indicator if this is the current step
					if (showPlaybackPos && x == currentStep_) {
						color = RGB{255, 0, 0}; // Red for current step
					}
					else {
						color = RGB{intensity, 0, intensity}; // Magenta intensity based on spread
					}
				}
				else {
					color = RGB{0, 0, 0};
				}
			}
			// y3: Gate length fader (all pads from 0 to gateLength_ are lit)
			else if (y == 3) {
				if (x <= gateLength_) {
					// Color intensity based on gate length position (0-15)
					uint8_t intensity = ((gateLength_ + 1) * 255) / 16;
					// Show playback position indicator if this is the current step
					if (showPlaybackPos && x == currentStep_) {
						color = RGB{255, 0, 0}; // Red for current step
					}
					else {
						color = RGB{intensity, intensity / 2, 0}; // Orange intensity based on gate length
					}
				}
				else {
					color = RGB{0, 0, 0};
				}
			}

			image[y * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[y][x] = (color.r || color.g || color.b) ? 64 : 0;
			}
		}
	}

	return true;
}

bool GenSequencerMode::renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                                     uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	// Use base class implementation to render control columns
	return SequencerMode::renderSidebar(whichRows, image, occupancyMask);
}

bool GenSequencerMode::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	// Let base class handle control columns (x16-x17)
	if (x >= kDisplayWidth) {
		return SequencerMode::handlePadPress(x, y, velocity);
	}

	// If Shift is pressed, don't handle pad presses - let instrument clip view handle it
	// (for editing synth parameters, etc.)
	if (Buttons::isShiftButtonPressed()) {
		return false;
	}

	// Only handle main grid pads (x0-x15) and presses (not releases)
	if (x < 0 || velocity == 0) {
		return false;
	}

	// y7: Note type range - fader behavior (pressing any pad x sets range to 0-x)
	if (y == 7) {
		if (x >= 0 && x < kDisplayWidth) {
			noteTypeRange_ = clampFader(x);
			sequenceNeedsRegeneration_ = true;

			if (display) {
				char buffer[32];
				int32_t numNotesInRange = calculateNumNotesInRange(noteTypeRange_);
				snprintf(buffer, sizeof(buffer), "Note range: %d", numNotesInRange);
				display->displayPopup(std::string_view(buffer));
			}

			uiNeedsRendering(&instrumentClipView, kNoteTypeRow, 0);
			return true;
		}
	}
	// y6: Note density - fader behavior (pressing any pad x sets density to position x)
	else if (y == 6) {
		if (x >= 0 && x < kDisplayWidth) {
			noteDensity_ = clampFader(x);
			sequenceNeedsRegeneration_ = true;

			if (display) {
				char buffer[32];
				snprintf(buffer, sizeof(buffer), "Note Density: %d%%", getNoteDensityPercent());
				display->displayPopup(std::string_view(buffer));
			}

			uiNeedsRendering(&instrumentClipView, kNoteDensityRow, 0);
			return true;
		}
	}
	// y5: Sequence length - fader behavior (pressing any pad x sets length to x+1)
	else if (y == 5) {
		if (x >= 0 && x < kDisplayWidth) {
			sequenceLength_ = clampFader(x);
			sequenceNeedsRegeneration_ = true;

			if (display) {
				char buffer[32];
				snprintf(buffer, sizeof(buffer), "Seq Length: %d", getSequenceLength());
				display->displayPopup(std::string_view(buffer));
			}

			uiNeedsRendering(&instrumentClipView, kSequenceLengthRow, 0);
			return true;
		}
	}
	// y4: Velocity spread - fader behavior (pressing any pad x sets spread to position x)
	else if (y == 4) {
		if (x >= 0 && x < kDisplayWidth) {
			velocitySpread_ = clampFader(x);

			if (display) {
				char buffer[32];
				snprintf(buffer, sizeof(buffer), "Velocity Spread: %d", getVelocitySpread());
				display->displayPopup(std::string_view(buffer));
			}

			uiNeedsRendering(&instrumentClipView, kVelocitySpreadRow, 0);
			return true;
		}
	}
	// y3: Gate length - fader behavior (pressing any pad x sets gate length to position x)
	else if (y == 3) {
		if (x >= 0 && x < kDisplayWidth) {
			gateLength_ = clampFader(x);

			if (display) {
				char buffer[32];
				snprintf(buffer, sizeof(buffer), "Gate Length: %d%%", getGateLengthPercent());
				display->displayPopup(std::string_view(buffer));
			}

			uiNeedsRendering(&instrumentClipView, kGateLengthRow, 0);
			return true;
		}
	}

	return false;
}

int32_t GenSequencerMode::calculateNumNotesInRange(int32_t faderPosition) const {
	if (faderPosition == 0) {
		return 1;
	}
	int32_t numNotesInRange = 1 + ((faderPosition * (numScaleNotes_ - 1)) / kFaderMax);
	if (numNotesInRange > numScaleNotes_)
		numNotesInRange = numScaleNotes_;
	return numNotesInRange;
}

int32_t GenSequencerMode::processPlayback(void* modelStackPtr, int32_t absolutePlaybackPos) {
	if (!initialized_) {
		return 2147483647;
	}

	ModelStackWithTimelineCounter* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);

	// Get control column effects
	CombinedEffects effects = getCombinedEffects();

	// Calculate base timing on first call
	if (ticksPerSixteenthNote_ == 0) {
		ticksPerSixteenthNote_ = modelStack->song->getSixteenthNoteLength();
	}

	// Apply clock divider to timing
	int32_t adjustedTicksPerStep = ticksPerSixteenthNote_;
	if (effects.clockDivider > 1) {
		adjustedTicksPerStep *= effects.clockDivider; // Divide: slower
	}
	else if (effects.clockDivider < -1) {
		adjustedTicksPerStep /= (-effects.clockDivider); // Multiply: faster
	}

	// Always update scale notes
	int32_t oldNumScaleNotes = numScaleNotes_;
	updateScaleNotes(modelStackPtr);

	// Regenerate sequence if parameters changed, sequence length changed, or scale changed
	if (sequenceNeedsRegeneration_ || actualSequenceLength_ != getSequenceLength() || oldNumScaleNotes != numScaleNotes_) {
		generateSequence();
	}

	// Check if we're at a step boundary
	if (!atDivisionBoundary(absolutePlaybackPos, adjustedTicksPerStep)) {
		return ticksUntilNextDivision(absolutePlaybackPos, adjustedTicksPerStep);
	}

	// Stop previous note if still playing
	if (activeNoteCode_ >= 0) {
		stopNote(modelStackPtr, activeNoteCode_);
		activeNoteCode_ = -1;
	}

	// Play note from stored sequence
	if (currentStep_ >= 0 && currentStep_ < actualSequenceLength_) {
		int32_t noteIndexInScale = generatedSequence_[currentStep_];

		if (noteIndexInScale >= 0) {
			// This step has a note - calculate and play it
			int32_t noteCode = calculateNoteCode(noteIndexInScale, effects);

			if (noteCode >= 0 && noteCode <= 127) {
				// Apply velocity spread
				uint8_t baseVelocity = 100;
				int32_t spread = getVelocitySpread();
				uint8_t velocity = applyVelocitySpread(baseVelocity, spread);

				// Apply gate length (as percentage of step duration)
				int32_t gatePercent = getGateLengthPercent();
				int32_t noteLength = (adjustedTicksPerStep * gatePercent) / 100;
				if (noteLength < 1)
					noteLength = 1; // Ensure at least 1 tick

				playNote(modelStackPtr, noteCode, velocity, noteLength);
				activeNoteCode_ = noteCode;
			}
		}
		// If noteIndexInScale is -1, this is a rest/silence - don't play anything
	}

	// Advance to next step
	advanceStep(effects.direction);

	lastAbsolutePlaybackPos_ = absolutePlaybackPos;

		// Refresh rows that show playback position
		uiNeedsRendering(&instrumentClipView, kNoteTypeRow | kNoteDensityRow | kSequenceLengthRow | kVelocitySpreadRow | kGateLengthRow, 0);

	return adjustedTicksPerStep;
}

void GenSequencerMode::stopAllNotes(void* modelStackPtr) {
	// Stop any currently playing note
	if (activeNoteCode_ >= 0) {
		stopNote(modelStackPtr, activeNoteCode_);
		activeNoteCode_ = -1;
	}
}

// ================================================================================================
// SCENE MANAGEMENT
// ================================================================================================

size_t GenSequencerMode::captureScene(void* buffer, size_t maxSize) {
	// Scene structure: five parameters + control values
	struct Scene {
		int32_t noteTypeRange;
		int32_t noteDensity;
		int32_t sequenceLength;
		int32_t velocitySpread;
		int32_t gateLength;
		int32_t clockDivider;
		int32_t octaveShift;
		int32_t transpose;
		int32_t direction;
	};

	if (maxSize < sizeof(Scene)) {
		return 0; // Buffer too small
	}

	Scene* scene = static_cast<Scene*>(buffer);

	// Save pattern data
	scene->noteTypeRange = noteTypeRange_;
	scene->noteDensity = noteDensity_;
	scene->sequenceLength = sequenceLength_;
	scene->velocitySpread = velocitySpread_;
	scene->gateLength = gateLength_;

	// Save active control values
	CombinedEffects effects = getCombinedEffects();
	scene->clockDivider = effects.clockDivider;
	scene->octaveShift = effects.octaveShift;
	scene->transpose = effects.transpose;
	scene->direction = effects.direction;

	return sizeof(Scene);
}

bool GenSequencerMode::recallScene(const void* buffer, size_t size) {
	struct Scene {
		int32_t noteTypeRange;
		int32_t noteDensity;
		int32_t sequenceLength;
		int32_t velocitySpread;
		int32_t gateLength;
		int32_t clockDivider;
		int32_t octaveShift;
		int32_t transpose;
		int32_t direction;
	};

	if (size < sizeof(Scene)) {
		return false; // Invalid data
	}

	const Scene* scene = static_cast<const Scene*>(buffer);

	// Restore pattern data
	noteTypeRange_ = scene->noteTypeRange;
	noteDensity_ = scene->noteDensity;
	sequenceLength_ = scene->sequenceLength;
	velocitySpread_ = scene->velocitySpread;
	gateLength_ = scene->gateLength;

	// Clamp values
	noteTypeRange_ = clampFader(noteTypeRange_);
	noteDensity_ = clampFader(noteDensity_);
	sequenceLength_ = clampFader(sequenceLength_);
	velocitySpread_ = clampFader(velocitySpread_);
	gateLength_ = clampFader(gateLength_);

	// Regenerate sequence with restored parameters
	sequenceNeedsRegeneration_ = true;
	generateSequence();

	// Restore control values by activating matching pads
	int32_t unmatchedClock, unmatchedOctave, unmatchedTranspose, unmatchedDirection;
	controlColumnState_.applyControlValues(scene->clockDivider, scene->octaveShift, scene->transpose, scene->direction,
	                                       &unmatchedClock, &unmatchedOctave, &unmatchedTranspose, &unmatchedDirection);

	// Apply unmatched values to base controls
	setBaseClockDivider(unmatchedClock);
	setBaseOctaveShift(unmatchedOctave);
	setBaseTranspose(unmatchedTranspose);
	setBaseDirection(unmatchedDirection);

	return true;
}

// ========== PATTERN PERSISTENCE ==========

void GenSequencerMode::writeToFile(Serializer& writer, bool includeScenes) {
	// Write gen sequencer with hex-encoded data
	writer.writeOpeningTagBeginning("genSequencer");
	writer.writeAttribute("currentStep", currentStep_);
	writer.writeAttribute("noteTypeRange", noteTypeRange_);
	writer.writeAttribute("noteDensity", noteDensity_);
	writer.writeAttribute("sequenceLength", sequenceLength_);
	writer.writeAttribute("velocitySpread", velocitySpread_);
	writer.writeAttribute("gateLength", gateLength_);
	writer.closeTag();

	// Write control columns and scenes
	controlColumnState_.writeToFile(writer, includeScenes);
}

Error GenSequencerMode::readFromFile(Deserializer& reader) {
	char const* tagName;

	// Read genSequencer tag contents
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "currentStep")) {
			currentStep_ = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "noteTypeRange")) {
			noteTypeRange_ = clampFader(reader.readTagOrAttributeValueInt());
		}
		else if (!strcmp(tagName, "noteDensity")) {
			noteDensity_ = clampFader(reader.readTagOrAttributeValueInt());
		}
		else if (!strcmp(tagName, "sequenceLength")) {
			sequenceLength_ = clampFader(reader.readTagOrAttributeValueInt());
		}
		else if (!strcmp(tagName, "velocitySpread")) {
			velocitySpread_ = clampFader(reader.readTagOrAttributeValueInt());
		}
		else if (!strcmp(tagName, "gateLength")) {
			gateLength_ = clampFader(reader.readTagOrAttributeValueInt());
		}
		else {
			// Unknown tag - let the caller handle it
			break;
		}
	}

	// After loading, update scale notes cache
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
	ModelStackWithTimelineCounter* modelStackWithTimelineCounter = modelStack->addTimelineCounter(getCurrentClip());
	updateScaleNotes(modelStackWithTimelineCounter);

	// Regenerate sequence with loaded parameters
	sequenceNeedsRegeneration_ = true;
	generateSequence();

	return Error::NONE;
}

void GenSequencerMode::resetToInit() {
	// Reset to default state
	noteTypeRange_ = 0;     // First note type only (pad 0)
	noteDensity_ = 7;        // ~50% probability (pad 7 of 15)
	sequenceLength_ = 15;   // Full sequence length (pad 15 = length 16)
	velocitySpread_ = 0;    // No velocity spread (pad 0)
	gateLength_ = 11;       // ~75% gate length (pad 11 of 15)
	currentStep_ = 0;
	sequenceNeedsRegeneration_ = true;

	// Regenerate sequence with new defaults
	generateSequence();

	uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0xFFFFFFFF);
}

void GenSequencerMode::randomizeAll(int32_t mutationRate) {
	// Randomize parameters based on mutation rate
	if (random(99) < mutationRate) {
		// Random note type range (0-15)
		noteTypeRange_ = random(15);

		// Random density (0-15)
		noteDensity_ = random(15);

		// Random sequence length (0-15)
		sequenceLength_ = random(15);

		// Random velocity spread (0-15)
		velocitySpread_ = random(15);

		// Random gate length (0-15)
		gateLength_ = random(15);

		// Regenerate sequence with new random parameters
		sequenceNeedsRegeneration_ = true;
		generateSequence();
	}

	uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0xFFFFFFFF);
}

void GenSequencerMode::evolveNotes(int32_t mutationRate) {
	// Evolve pattern with adaptive behavior
	bool isHighRate = mutationRate > 70;
	bool parametersChanged = false;

	if (random(99) < mutationRate) {
		// Mutate note type range (gentle changes)
		if (random(99) < 50) {
			int32_t change = (isHighRate) ? (random(3) - 1) : (random(2) - 1); // -1, 0, or +1 (or -1 to +1 for high rate)
			int32_t oldValue = noteTypeRange_;
			noteTypeRange_ = clampFader(noteTypeRange_ + change);
			if (noteTypeRange_ != oldValue) parametersChanged = true;
		}

		// Mutate density (gentle changes)
		if (random(99) < 40) {
			int32_t change = random(2) - 1; // -1, 0, or +1
			int32_t oldValue = noteDensity_;
			noteDensity_ = clampFader(noteDensity_ + change);
			if (noteDensity_ != oldValue) parametersChanged = true;
		}

		// Mutate sequence length (gentle changes)
		if (random(99) < 30) {
			int32_t change = random(2) - 1; // -1, 0, or +1
			int32_t oldValue = sequenceLength_;
			sequenceLength_ = clampFader(sequenceLength_ + change);
			if (sequenceLength_ != oldValue) parametersChanged = true;
		}

		// Mutate velocity spread (gentle changes)
		if (random(99) < 25) {
			int32_t change = random(2) - 1; // -1, 0, or +1
			int32_t oldValue = velocitySpread_;
			velocitySpread_ = clampFader(velocitySpread_ + change);
			if (velocitySpread_ != oldValue) parametersChanged = true;
		}

		// Mutate gate length (gentle changes)
		if (random(99) < 25) {
			int32_t change = random(2) - 1; // -1, 0, or +1
			int32_t oldValue = gateLength_;
			gateLength_ = clampFader(gateLength_ + change);
			if (gateLength_ != oldValue) parametersChanged = true;
		}

		// Regenerate sequence if parameters changed
		if (parametersChanged) {
			sequenceNeedsRegeneration_ = true;
			generateSequence();
		}
	}

	uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0xFFFFFFFF);
}

} // namespace deluge::model::clip::sequencer::modes

