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

#include "gui/l10n/l10n.h"
#include "model/clip/sequencer/sequencer_mode.h"
#include <array>

namespace deluge::model::clip::sequencer::modes {

/**
 * Gen Sequencer Mode - Simple generative sequencer based on note range and probability
 *
 * This is a purely generative sequencer - no per-step control, just three parameters:
 * - y7, x0-x15: Note type range fader (pressing pad x sets range to 0-x, all pads from 0 to x light up)
 * - y6, x0-x15: Note density fader (pressing pad x sets density, all pads from 0 to x light up)
 * - y5, x0-x15: Sequence length fader (pressing pad x sets length, all pads from 0 to x light up)
 */
class GenSequencerMode : public SequencerMode {
public:
	GenSequencerMode() = default;
	~GenSequencerMode() override = default;

	// Core identification
	l10n::String name() override { return l10n::String::STRING_FOR_GEN_SEQ; }

	// Support instrument tracks (Synth/MIDI/CV)
	bool supportsInstrument() override { return true; }
	bool supportsKit() override { return false; }
	bool supportsMIDI() override { return true; }
	bool supportsCV() override { return true; }
	bool supportsAudio() override { return false; }

	// Gen Sequencer supports all control types
	bool supportsControlType(ControlType type) override { return true; }

	// Lifecycle
	void initialize() override;
	void cleanup() override;

	// Rendering
	bool renderPads(uint32_t whichRows, RGB* image, uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
	                int32_t xScroll, uint32_t xZoom, int32_t renderWidth, int32_t imageWidth) override;

	bool renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                   uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override;

	// Pad input
	bool handlePadPress(int32_t x, int32_t y, int32_t velocity) override;

public:
	// Playback - generates notes based on probability and note range
	int32_t processPlayback(void* modelStack, int32_t absolutePlaybackPos) override;

	// Stop all notes - prevents hung notes when playback stops
	void stopAllNotes(void* modelStack) override;

	// Scene management
	size_t captureScene(void* buffer, size_t maxSize) override;
	bool recallScene(const void* buffer, size_t size) override;

	// Generative mutations
	void resetToInit() override;
	void randomizeAll(int32_t mutationRate = 100) override;
	void evolveNotes(int32_t mutationRate = 30) override;

	// Pattern persistence
	void writeToFile(Serializer& writer, bool includeScenes = true) override;
	Error readFromFile(Deserializer& reader) override;

private:
	static constexpr int32_t kMaxScaleNotes = 32; // Enough for 2 octaves of most scales (max 24 for chromatic)
	static constexpr int32_t kMaxSequenceLength = 16;
	static constexpr int32_t kFaderMax = 15;     // Maximum fader position (0-15)
	static constexpr int32_t kFaderMin = 0;      // Minimum fader position

	// Single set of parameters for the entire generative sequencer
	int32_t noteTypeRange_ = 0;    // 0-15: rightmost enabled pad in y7 (fader position)
	int32_t noteDensity_ = 0;      // 0-15: rightmost enabled pad in y6 (fader position, maps to 0-100%)
	int32_t sequenceLength_ = 0;   // 0-15: rightmost enabled pad in y5 (fader position, maps to 1-16)
	int32_t velocitySpread_ = 0;   // 0-15: rightmost enabled pad in y4 (fader position, maps to 0-127)
	int32_t gateLength_ = 0;       // 0-15: rightmost enabled pad in y3 (fader position, maps to 0-100%)

	// Generated sequence storage (stores note indices in scale, -1 means no note/rest)
	std::array<int32_t, kMaxSequenceLength> generatedSequence_;
	int32_t actualSequenceLength_ = 0; // Actual length of generated sequence (1-16)
	bool sequenceNeedsRegeneration_ = true; // Flag to regenerate sequence when parameters change

	// State
	bool initialized_ = false;

	// Scale notes cache
	int32_t scaleNotes_[kMaxScaleNotes];
	int32_t numScaleNotes_ = 0;

	// Timing
	int32_t ticksPerSixteenthNote_ = 0;
	int32_t currentStep_ = 0; // Current step in the generated sequence (0 to actualSequenceLength_-1)
	int32_t lastAbsolutePlaybackPos_ = 0;

	// Currently playing note (for note-off)
	int32_t activeNoteCode_ = -1;

	// Helpers
	void updateScaleNotes(void* modelStackPtr);
	void generateSequence(); // Generate the sequence once based on current parameters
	int32_t calculateNoteCode(int32_t noteIndexInScale, const CombinedEffects& effects) const;
	int32_t getRandomNoteFromRange() const;
	int32_t getSequenceLength() const; // Returns 1-16 based on sequenceLength_ (0-15)
	int32_t getNoteDensityPercent() const; // Convert fader position (0-15) to percentage (0-100%)
	int32_t getVelocitySpread() const; // Convert fader position (0-15) to velocity spread (0-127)
	int32_t getGateLengthPercent() const; // Convert fader position (0-15) to percentage (0-100%)
	void advanceStep(int32_t direction);
	
	// Helper functions
	static int32_t clampFader(int32_t value) { return (value < kFaderMin) ? kFaderMin : ((value > kFaderMax) ? kFaderMax : value); }
	int32_t calculateNumNotesInRange(int32_t faderPosition) const;
};

} // namespace deluge::model::clip::sequencer::modes

