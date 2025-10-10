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
#include "hid/led/pad_leds.h"
#include <array>

namespace deluge::model::clip::sequencer::modes {

constexpr int32_t kMaxStages = 8;
constexpr int32_t kMaxPulseCount = 8;

class PulseSequencerMode : public SequencerMode {
public:
	l10n::String name() override { return l10n::String::STRING_FOR_PULSE_SEQ; }

	// Support only melodic tracks
	bool supportsInstrument() override { return true; }
	bool supportsKit() override { return false; }
	bool supportsMIDI() override { return true; }
	bool supportsCV() override { return true; }
	bool supportsAudio() override { return false; }

	void initialize() override;
	void cleanup() override;

	// Override rendering to show pulse pattern
	bool renderPads(uint32_t whichRows, RGB* image, uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
	               int32_t xScroll, uint32_t xZoom, int32_t renderWidth, int32_t imageWidth) override;
	
	// Override sidebar rendering
	bool renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                  uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override;

	// Override pad input to handle user interaction
	bool handlePadPress(int32_t x, int32_t y, int32_t velocity) override;

	// Override vertical encoder for view scrolling
	bool handleVerticalEncoder(int32_t offset) override;

	// Override playback to generate pulsed notes
	int32_t processPlayback(void* modelStack, int32_t absolutePlaybackPos) override;

	// Gate types enum
	enum class GateType : int32_t { OFF = 0, SINGLE = 1, MULTIPLE = 2, HELD = 3 };

	// Play order presets enum
	enum class PlayOrder : int32_t {
		FORWARDS = 0,
		BACKWARDS = 1,
		PING_PONG = 2,
		RANDOM = 3,
		PEDAL = 4,
		SKIP_2 = 5,
		PENDULUM = 6,
		SPIRAL = 7
	};

private:
	bool initialized_ = false;
	int32_t ticksPerSixteenthNote_ = 0;
	int32_t lastAbsolutePlaybackPos_ = 0; // Track for position indicator

	// Stage data (8 stages, one per column)
	struct StageData {
		GateType gateType = GateType::OFF;
		int32_t noteIndex = 0;     // Index in current scale
		int32_t octave = 0;        // Octave offset from base
		int32_t pulseCount = 1;    // 1-8, default is 1
		int32_t velocitySpread = 0; // 0-127, randomization amount
		int32_t probability = 100;  // 0-100%, chance to play
		int32_t gateLength = 50;    // 0-100%, note length as % of period
	};

	std::array<StageData, kMaxStages> stages_;

	// Sequencer state
	struct {
		int32_t currentPulse = 0;
		int32_t lastPlayedStage = -1;
		int32_t totalPatternLength = 8;

		// Visual feedback
		bool gatePadFlashing = false;
		uint32_t flashStartTime = 0;
		uint32_t flashDuration = 50;

		// Per-note tracking for proper note-off handling
		std::array<int16_t, 16> noteCodeActive;
		std::array<uint32_t, 16> noteGatePos;
		std::array<bool, 16> noteActive;
		std::array<int32_t, 16> noteSourceStage;
	} sequencerState_;

	// Performance controls
	struct {
		int32_t transpose = 0;
		int32_t octave = 0;
		int32_t clockDivider = 2;
		int32_t numStages = 8;
		PlayOrder playOrder = PlayOrder::FORWARDS;
		int32_t pingPongDirection = 1;
		int32_t currentStage = 0;
		std::array<bool, kMaxStages> stageEnabled = {true, true, true, true, true, true, true, true};

		// Play order state variables (instance-based, not static)
		int32_t pedalNextStage = 1;
		bool skip2OddPhase = true;
		int32_t pendulumLow = 0;
		int32_t pendulumHigh = 1;
		bool pendulumGoingUp = true;
		int32_t spiralLow = 0;
		int32_t spiralHigh = 7;
		bool spiralFromLow = true;
	} performanceControls_;

	// Display state
	struct {
		int32_t gateLineOffset = 0; // 0-3, maps to Y positions 4-7
	} displayState_;

	// Helper methods
	void generateNotes(void* modelStack);
	void playNoteForStage(void* modelStack, int32_t stage);
	void switchNoteOff(void* modelStack, int32_t noteSlot);
	void advanceToNextEnabledStage();
	bool evaluateRhythmPattern(int32_t stage, int32_t pulsePosition);
	int32_t calculateTotalPatternLength() const;
	int32_t getGateLineY() const { return displayState_.gateLineOffset + 4; }
	const char* getGateTypeName(GateType type) const;

	// Pad input handlers
	void handleGateType(int32_t stage);
	void handleNoteSelection(int32_t stage);
	void handleOctaveAdjustment(int32_t stage, int32_t direction);
	void handlePulseCount(int32_t stage, int32_t position);
	void handleVelocitySpread(int32_t stage);
	void handleProbability(int32_t stage);
	void handleGateLength(int32_t stage);
	void handleStageCountChange(int32_t numStages);
	void handlePlayOrderChange(int32_t playOrderIndex);
	void handleTransposeChange(int32_t direction);
	void handleOctaveChange(int32_t direction);
	void handleStageToggle(int32_t stage);
	void resetToDefaults();
	void resetPerformanceControls();
	void randomizeSequence();
	void evolveSequence();
};

} // namespace deluge::model::clip::sequencer::modes
