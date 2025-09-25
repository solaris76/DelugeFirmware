/*
 * Copyright © 2014-2024 Synthstrom Audible Limited
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

#include "sequencer_clip.h"
#include "model/song/song.h"
#include "model/model_stack.h"
#include "model/action/action_logger.h"
#include "model/instrument/melodic_instrument.h"
#include "model/instrument/midi_instrument.h"
#include "model/instrument/cv_instrument.h"
#include "playback/playback_handler.h"
#include "storage/storage_manager.h"
#include "gui/l10n/l10n.h"
#include <cstdlib>
#include <array>

SequencerClip::SequencerClip(Song* song) : Clip(ClipType::SEQUENCER) {
	// Initialize with default generative settings
	settings_.sequencerType = SequencerType::RANDOM;
	settings_.patternLength = 8;
	settings_.currentStep = 0;
	settings_.clockDivision = 4; // 16th notes
	settings_.density = 50; // 50% probability
	settings_.velocityMin = 64;
	settings_.velocityMax = 127;
	settings_.octaveRange = 2;
	settings_.gateLength = 50;

	sequencerActive_ = false;
	ticksTilNextSequencerEvent = 0;
	sequencerNumTicksBehindClip = 0;

	if (song) {
		loopLength = song->getBarLength() * 4; // Default to 4 bars
	}
}

SequencerClip::~SequencerClip() {
	stopGenerativeSequencer();
}

Error SequencerClip::clone(ModelStackWithTimelineCounter* modelStack, bool shouldFlattenReversing) const {
	return Error::NONE; // TODO: Implement cloning
}

void SequencerClip::copyBasicsFrom(Clip const* otherClip) {
	Clip::copyBasicsFrom(otherClip);
	// Copy sequencer-specific settings if otherClip is also a SequencerClip
	if (otherClip->type == ClipType::SEQUENCER) {
		const SequencerClip* otherSequencerClip = static_cast<const SequencerClip*>(otherClip);
		settings_ = otherSequencerClip->settings_;
	}
}

void SequencerClip::expectNoFurtherTicks(Song* song, bool actuallySoundChange) {
	// Sequencer clips don't need this
}

void SequencerClip::resumePlayback(ModelStackWithTimelineCounter* modelStack, bool mayMakeSound) {
	if (mayMakeSound) {
		ticksTilNextSequencerEvent = 0;
		sequencerNumTicksBehindClip = 0;
	}
}

void SequencerClip::processCurrentPos(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) {
	// Process sequencer events

	// Simple timing logic - generate events based on clock division
	uint32_t ticksPerStep = (modelStack->song->getBarLength() * 4) / settings_.clockDivision;

	sequencerNumTicksBehindClip += ticksSinceLast;

	while (sequencerNumTicksBehindClip >= ticksPerStep) {
		sequencerNumTicksBehindClip -= ticksPerStep;
		processCurrentStep();
		settings_.currentStep = (settings_.currentStep + 1) % settings_.patternLength;
	}
}

void SequencerClip::incrementPos(ModelStackWithTimelineCounter* modelStack, int32_t numTicks) {
	// Update timing variables
	sequencerNumTicksBehindClip += numTicks;
}

bool SequencerClip::shiftHorizontally(ModelStackWithTimelineCounter* modelStack, int32_t amount, bool shiftAutomation, bool shiftSequenceAndMPE) {
	// TODO: Implement horizontal shifting
	return false;
}

bool SequencerClip::isEmpty(bool displayPopup) {
	// Sequencer clips are never "empty" as they generate content
	return false;
}

bool SequencerClip::renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth], uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	// Simple sidebar rendering - just show pattern type indicator
	if (image) {
		RGB color = RGB{64, 64, 64}; // Default gray
		switch (settings_.sequencerType) {
			case SequencerType::RANDOM:
				color = RGB{255, 0, 0}; // Red
				break;
			case SequencerType::PULSE:
				color = RGB{0, 255, 0}; // Green
				break;
			case SequencerType::EUCLIDEAN_ENHANCED:
				color = RGB{0, 0, 255}; // Blue
				break;
			case SequencerType::ARPEGGIATOR:
				color = RGB{255, 255, 0}; // Yellow
				break;
		}

		// Fill sidebar with pattern color
		for (int y = 0; y < kDisplayHeight; y++) {
			image[y][kDisplayWidth] = color;
		}
	}
	return true;
}

Error SequencerClip::claimOutput(ModelStackWithTimelineCounter* modelStack) {
	// Sequencer clips need an output to send notes to
	if (!output) {
		return Error::NONE; // Will be set later
	}
	return Error::NONE;
}

void SequencerClip::detachFromOutput(ModelStackWithTimelineCounter* modelStack, bool shouldRememberDrumName, bool shouldDeleteEmptyNoteRowsAtEndOfList, bool shouldRetainLinksToSounds, bool keepNoteRowsWithMIDIInput, bool shouldGrabMidiCommands, bool shouldBackUpExpressionParamsToo) {
	output = nullptr;
}

Clip* SequencerClip::cloneAsNewOverdub(ModelStackWithTimelineCounter* modelStack, OverDubType newOverdubNature) {
	return nullptr; // TODO: Implement
}


void SequencerClip::clear(Action* action, ModelStackWithTimelineCounter* modelStack, bool clearAutomation, bool clearSequenceAndMPE) {
	// Reset sequencer to default state
	settings_ = SequencerSettings{};
	settings_.currentStep = 0;
}

char const* SequencerClip::getXMLTag() {
	return "sequencerClip";
}

Error SequencerClip::readFromFile(Deserializer& reader, Song* song) {
	return Error::NONE; // TODO: Implement file loading
}

void SequencerClip::writeDataToFile(Serializer& writer, Song* song) {
	// TODO: Implement file saving
}

bool SequencerClip::isAbandonedOverdub() {
	return false;
}

bool SequencerClip::wantsToBeginLinearRecording(Song* song) {
	return false; // Sequencers don't do linear recording
}

void SequencerClip::quantizeLengthForArrangementRecording(ModelStackWithTimelineCounter* modelStack, int32_t lengthSoFar, uint32_t timeRemainder, int32_t suggestedLength, int32_t alternativeLongerLength) {
	// TODO: Implement quantization
}

void SequencerClip::abortRecording() {
	// No recording to abort
}

void SequencerClip::stopAllNotesPlaying(Song* song, bool actuallySoundChange) {
	// TODO: Stop any playing notes
}

bool SequencerClip::cloneOutput(ModelStackWithTimelineCounter* modelStack) {
	return false; // TODO: Implement
}

void SequencerClip::finishLinearRecording(ModelStackWithTimelineCounter* modelStack, Clip* nextPendingLoop, int32_t buttonLatencyForTempolessRecord) {
	// Sequencer clips don't do linear recording
}

Error SequencerClip::beginLinearRecording(ModelStackWithTimelineCounter* modelStack, int32_t buttonPressLatency) {
	return Error::NONE; // Sequencer clips don't do linear recording
}

bool SequencerClip::getCurrentlyRecordingLinearly() {
	return false; // Sequencer clips don't do linear recording
}

bool SequencerClip::currentlyScrollableAndZoomable() {
	return true; // Sequencer clips can be scrolled and zoomed
}

void SequencerClip::setSequencerType(SequencerType type) {
	settings_.sequencerType = type;
}

void SequencerClip::processCurrentStep() {
	// Simple pattern generation
	switch (settings_.sequencerType) {
		case SequencerType::RANDOM:
			generateRandomPattern();
			break;
		case SequencerType::PULSE:
		case SequencerType::EUCLIDEAN_ENHANCED:
		case SequencerType::ARPEGGIATOR:
			// TODO: Implement other pattern types
			break;
	}
}

void SequencerClip::generateRandomPattern() {
	// Simple random note generation
	if (settings_.density > 0 && (rand() % 100) < settings_.density) {
		// Generate a random note within the velocity and octave range
		// TODO: Implement proper note sending
		// For now, just a placeholder
	}
}

// Generative Sequencer Management
void SequencerClip::startGenerativeSequencer() {
	sequencerActive_ = true;
	resetGenerativeSequencer();
}

void SequencerClip::stopGenerativeSequencer() {
	sequencerActive_ = false;
	// Stop any playing notes
	// TODO: Implement note stopping
}

void SequencerClip::resetGenerativeSequencer() {
	settings_.currentStep = 0;
	ticksTilNextSequencerEvent = 0;
	sequencerNumTicksBehindClip = 0;
}


// Step Management
void SequencerClip::setStepActive(uint32_t step, bool active) {
	if (step < steps_.size()) {
		steps_[step].active = active;
	}
}

bool SequencerClip::isStepActive(uint32_t step) const {
	if (step < steps_.size()) {
		return steps_[step].active;
	}
	return false;
}

void SequencerClip::setStepVelocity(uint32_t step, uint32_t velocity) {
	if (step < steps_.size()) {
		steps_[step].velocity = velocity;
	}
}

uint32_t SequencerClip::getStepVelocity(uint32_t step) const {
	if (step < steps_.size()) {
		return steps_[step].velocity;
	}
	return 100;
}

// Pattern Generation Methods
void SequencerClip::generatePulsePattern() {
	// TODO: Implement pulse pattern generation
}

void SequencerClip::generateEuclideanEnhancedPattern() {
	// TODO: Implement enhanced euclidean pattern generation
}

void SequencerClip::generateArpeggiatorPattern() {
	// TODO: Implement arpeggiator pattern generation
}

// Random Sequencer Helper Methods
uint32_t SequencerClip::generateRandomNoteFromScale(Song* song) {
	// TODO: Generate random note based on song's scale
	return 60; // Middle C for now
}

uint32_t SequencerClip::generateRandomVelocity() {
	uint32_t range = settings_.velocityMax - settings_.velocityMin;
	return settings_.velocityMin + (rand() % (range + 1));
}

void SequencerClip::scheduleNoteOff(uint32_t note, uint32_t gateTime) {
	// TODO: Schedule note off event
}

// Output Management
void SequencerClip::sendNoteOn(uint32_t note, uint32_t velocity) {
	Output* output = this->output;
	if (output == nullptr) {
		return;
	}

	// Only send to melodic instruments (synth, MIDI, CV)
	if (output->type == OutputType::SYNTH ||
	    output->type == OutputType::MIDI_OUT ||
	    output->type == OutputType::CV) {

		// Follow InstrumentClip pattern for note sending
		// Create a minimal ModelStack for note sending
		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
		ModelStackWithTimelineCounter* modelStackWithTimelineCounter = modelStack->addTimelineCounter(this);

		// Create model stack with the output and param manager
		ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
		    modelStackWithTimelineCounter->addOtherTwoThingsButNoNoteRow(output->toModControllable(), &paramManager);

		// Send the note using the proper MelodicInstrument interface
		MelodicInstrument* melodicOutput = static_cast<MelodicInstrument*>(output);
		int16_t zeroMPEValues[kNumExpressionDimensions] = {0}; // No MPE for now
		melodicOutput->sendNote(modelStackWithThreeMainThings, true, static_cast<int32_t>(note), zeroMPEValues,
		                       MIDI_CHANNEL_NONE, static_cast<uint8_t>(velocity), 0, 0);
	}
}

void SequencerClip::sendNoteOff(uint32_t note) {
	// TODO: Implement note off sending
}

// UI Support
void SequencerClip::renderPads(uint32_t* padColors, uint32_t numPads) {
	// TODO: Implement pad rendering
}

void SequencerClip::handlePadPress(uint32_t padIndex, bool pressed) {
	// TODO: Implement pad press handling
}

void SequencerClip::handleEncoderTurn(int32_t offset, bool shiftPressed) {
	// TODO: Implement encoder handling
}
