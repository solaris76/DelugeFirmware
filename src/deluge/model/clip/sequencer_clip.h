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

#pragma once

#include "definitions_cxx.hpp"
#include "model/clip/clip.h"
#include <cstdint>

class Song;
class ModelStackWithTimelineCounter;
class Output;

// Generative Sequencer Types
enum class SequencerType {
	RANDOM,             // Random probability-based generative patterns (start simple!)
	PULSE,              // Pulse-based generative sequencer (pulses per step)
	EUCLIDEAN_ENHANCED, // Enhanced euclidean with more parameters
	ARPEGGIATOR,        // Generative arpeggiator patterns (leverage existing arp system)
};

// Generative Sequencer Engine Settings
struct SequencerSettings {
	SequencerType sequencerType = SequencerType::RANDOM;

	// Timing and Clock
	uint32_t clockDivision = 4; // 16th note timing by default
	// Note: Global swing is handled by Song::swingAmount and Song::swingInterval
	// We can add per-sequencer swing override if needed
	uint32_t sequencerSwingOverride = 0; // 0 = use global swing, >0 = override amount

	// Generative Pattern Parameters
	uint32_t patternLength = 8; // Number of steps in generative pattern
	uint32_t currentStep = 0;   // Current step in pattern

	// Random Pattern Parameters
	uint32_t density = 50;      // Probability of note generation (0-100)
	uint32_t velocityMin = 64;  // Minimum velocity for generated notes
	uint32_t velocityMax = 127; // Maximum velocity for generated notes
	uint32_t octaveRange = 2;   // Number of octaves to generate notes across
	uint32_t gateLength = 50;   // Gate length as percentage of step (0-100)

	// Pulse Pattern Parameters
	uint32_t pulseCount = 4;    // Number of pulses per pattern
	uint32_t pulseRotation = 0; // Rotation offset for pulse pattern

	// Euclidean Enhanced Parameters
	uint32_t euclideanHits = 4;          // Number of hits in euclidean pattern
	uint32_t euclideanSteps = 8;         // Number of steps in euclidean pattern
	uint32_t euclideanRotation = 0;      // Rotation offset
	uint32_t euclideanProbability = 100; // Probability per hit (0-100)

	// Arpeggiator Parameters
	uint32_t arpOctaves = 1;   // Number of octaves to arpeggiate
	uint32_t arpDirection = 0; // 0=up, 1=down, 2=up-down, 3=random
};

// Individual step data for sequencer patterns
struct SequencerStep {
	bool active = false;        // Whether this step is active
	uint32_t velocity = 100;    // Velocity for this step
	uint32_t noteOffset = 0;    // Note offset from root (in semitones)
	uint32_t probability = 100; // Probability this step will trigger (0-100)
};

class SequencerClip final : public Clip {
public:
	explicit SequencerClip(Song* song = nullptr);
	~SequencerClip() override;

	// Required Clip interface methods (simplified)
	Error clone(ModelStackWithTimelineCounter* modelStack, bool shouldFlattenReversing = false) const override;
	void copyBasicsFrom(Clip const* otherClip) override;
	void expectNoFurtherTicks(Song* song, bool actuallySoundChange = true) override;
	void resumePlayback(ModelStackWithTimelineCounter* modelStack, bool mayMakeSound = true) override;
	void processCurrentPos(ModelStackWithTimelineCounter* modelStack, uint32_t ticksSinceLast) override;
	void incrementPos(ModelStackWithTimelineCounter* modelStack, int32_t numTicks) override;
	bool shiftHorizontally(ModelStackWithTimelineCounter* modelStack, int32_t amount, bool shiftAutomation,
	                       bool shiftSequenceAndMPE) override;

	// Required Clip interface methods
	bool isEmpty(bool displayPopup = true) override;
	bool renderSidebar(uint32_t whichRows = 0, RGB image[][kDisplayWidth + kSideBarWidth] = nullptr,
	                   uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth] = nullptr) override;
	Error claimOutput(ModelStackWithTimelineCounter* modelStack) override;
	void finishLinearRecording(ModelStackWithTimelineCounter* modelStack, Clip* nextPendingLoop = nullptr,
	                           int32_t buttonLatencyForTempolessRecord = 0) override;
	Error beginLinearRecording(ModelStackWithTimelineCounter* modelStack, int32_t buttonPressLatency) override;
	bool getCurrentlyRecordingLinearly() override;
	bool currentlyScrollableAndZoomable() override;
	void detachFromOutput(ModelStackWithTimelineCounter* modelStack, bool shouldRememberDrumName,
	                      bool shouldDeleteEmptyNoteRowsAtEndOfList = false, bool shouldRetainLinksToSounds = false,
	                      bool keepNoteRowsWithMIDIInput = true, bool shouldGrabMidiCommands = false,
	                      bool shouldBackUpExpressionParamsToo = true) override;
	Clip* cloneAsNewOverdub(ModelStackWithTimelineCounter* modelStack, OverDubType newOverdubNature) override;
	void clear(Action* action, ModelStackWithTimelineCounter* modelStack, bool clearAutomation,
	           bool clearSequenceAndMPE) override;
	char const* getXMLTag() override;
	Error readFromFile(Deserializer& reader, Song* song) override;
	void writeDataToFile(Serializer& writer, Song* song) override;
	bool isAbandonedOverdub() override;
	bool wantsToBeginLinearRecording(Song* song) override;
	void quantizeLengthForArrangementRecording(ModelStackWithTimelineCounter* modelStack, int32_t lengthSoFar,
	                                           uint32_t timeRemainder, int32_t suggestedLength,
	                                           int32_t alternativeLongerLength) override;
	void abortRecording() override;
	void stopAllNotesPlaying(Song* song, bool actuallySoundChange = true) override;
	bool cloneOutput(ModelStackWithTimelineCounter* modelStack) override;

	// Generative Sequencer-specific methods
	void startGenerativeSequencer();
	void stopGenerativeSequencer();
	void resetGenerativeSequencer();
	bool isGenerativeSequencerActive() const { return sequencerActive_; }

	// Generative Sequencer Management
	void setSequencerType(SequencerType type);
	SequencerType getSequencerType() const { return settings_.sequencerType; }

	// Step Management
	void setStepActive(uint32_t step, bool active);
	bool isStepActive(uint32_t step) const;
	void setStepVelocity(uint32_t step, uint32_t velocity);
	uint32_t getStepVelocity(uint32_t step) const;

	// Settings Access
	SequencerSettings& getSettings() { return settings_; }
	const SequencerSettings& getSettings() const { return settings_; }

	// Current State
	uint32_t getCurrentStep() const { return settings_.currentStep; }
	uint32_t getPatternLength() const { return settings_.patternLength; }

	// UI Support
	void renderPads(uint32_t* padColors, uint32_t numPads);
	void handlePadPress(uint32_t padIndex, bool pressed);
	void handleEncoderTurn(int32_t offset, bool shiftPressed);

private:
	// Core sequencer state
	SequencerSettings settings_;
	std::array<SequencerStep, 16> steps_; // Support up to 16 steps
	bool sequencerActive_ = false;

	// Timing state (following InstrumentClip pattern)
	uint32_t ticksTilNextSequencerEvent = 0;
	uint32_t sequencerNumTicksBehindClip = 0;

	// Internal pattern generation methods
	void processCurrentStep();
	void generateRandomPattern();
	void generatePulsePattern();
	void generateEuclideanEnhancedPattern();
	void generateArpeggiatorPattern();

	// Random sequencer helper methods
	uint32_t generateRandomNoteFromScale(Song* song);
	uint32_t generateRandomVelocity();
	void scheduleNoteOff(uint32_t note, uint32_t gateTime);

	// Output management
	void sendNoteOn(uint32_t note, uint32_t velocity);
	void sendNoteOff(uint32_t note);
};
