/*
 * Copyright © 2025 Synthstrom Audible Limited
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

#include "gui/l10n/strings.h"
#include "gui/ui/keyboard/layout/column_controls.h"

// Forward declarations
class ArpeggiatorSettings;
class Arpeggiator;

namespace deluge::gui::ui::keyboard::layout {

/// Pulse sequence keyboard layout for creating rhythmic pulse patterns
class KeyboardLayoutPulseSeq : public ColumnControlsKeyboard {
public:
	KeyboardLayoutPulseSeq() = default;
	~KeyboardLayoutPulseSeq() override = default;

	void evaluatePads(PressedPad presses[kMaxNumKeyboardPadPresses]) override;
	void handleVerticalEncoder(int32_t offset) override;
	void handleHorizontalEncoder(int32_t offset, bool shiftEnabled, PressedPad presses[kMaxNumKeyboardPadPresses],
	                             bool encoderPressed = false) override;
	void precalculate() override;

	void renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) override;

	l10n::String name() override { return l10n::String::STRING_FOR_KEYBOARD_LAYOUT_PULSE_SEQUENCER; }
	bool supportsInstrument() override { return true; }
	bool supportsKit() override { return false; }
	bool supportsTiming() override { return true; }

	/// Arpeggiator-style timing - returns ticks until next event
	int32_t doTickForward(uint32_t clipCurrentPos, bool currentlyPlayingReversed,
	                      ArpReturnInstruction* instruction) override;

private:
	/// Get the current arpeggiator settings from the active clip
	ArpeggiatorSettings* getArpSettings();

	/// Generate note using ArpReturnInstruction
	void switchNoteOn(ArpReturnInstruction* instruction);

	/// Send note-off using ArpReturnInstruction
	void switchAnyNoteOff(ArpReturnInstruction* instruction);

	/// Calculate gate length in ticks
	uint32_t calculateGateLength();

	/// Get the current gate line Y position (constrained to y4-y7)
	int32_t getGateLineY() const;

	/// Handle gate type cycling for a specific stage
	void handleGateType(int32_t stage);

	/// Handle note selection for a specific stage
	void handleNoteSelection(int32_t stage);

	/// Handle octave adjustment for a specific stage
	void handleOctaveAdjustment(int32_t stage, int32_t direction);

	/// Handle stage count change (performance control)
	void handleStageCountChange(int32_t numStages);

	/// Handle play order preset selection
	void handlePlayOrderChange(int32_t playOrderIndex);

	/// Handle transpose change
	void handleTransposeChange(int32_t direction);

	/// Handle octave change
	void handleOctaveChange(int32_t direction);

	/// Handle pulse count adjustment for a specific stage
	void handlePulseCount(int32_t stage, int32_t position);

	/// Handle velocity spread control
	void handleVelocitySpread(int32_t x);

	/// Handle note probability control
	void handleNoteProbability(int32_t x);

	/// Handle gate control
	void handleGate(int32_t x);

	/// Handle stage enable/disable toggle
	void handleStageToggle(int32_t stage);

	/// Advance to next enabled stage based on play order
	void advanceToNextEnabledStage();

	/// Reset all performance controls to default values
	void resetToDefaults();

	/// Randomize sequence settings (notes, octaves, pulse counts, gate types)
	void randomizeSequence();

	/// Evolve sequence with subtle changes to notes only
	void evolveSequence();

private:
	/// Helper to set arpeggiator parameters for both synth and MIDI tracks
	void setArpParameter(int32_t paramId, int32_t value, bool useStandardScaling = true);

	/// Render fader-style control with visual feedback
	void renderFaderControl(RGB image[][kDisplayWidth + kSideBarWidth], int32_t row, const int32_t* values,
	                        int32_t lastTouchedPad, RGB activeColor, RGB dimColor);

	/// Display popup with automatic timer
	void showPopupWithTimer(const char* message);

	/// Validate stage index is within bounds
	bool isValidStage(int32_t stage) const { return stage >= 0 && stage < kMaxStages; }

public:
	/// Evaluate rhythm pattern to determine if note should play (auto-generated from gate type and pulse count)
	bool evaluateRhythmPattern(int32_t stage, int32_t pulsePosition);

	/// Calculate total pattern length based on all stage pulse counts
	int32_t calculateTotalPatternLength() const;

	/// Reset sequencer to start of pattern
	void resetToPatternStart();

	/// Pulse sequencer engine methods
	void resetSequencerState();

	/// Note handling methods
	void generateNotes(ArpReturnInstruction* instruction);
	int32_t findStageForPulse(int32_t pulse);
	void playNoteForStage(ArpReturnInstruction* instruction, int32_t stage);
	void switchNoteOff(ArpReturnInstruction* instruction, int32_t noteSlot);
	void sendAllNotesOff();

public:
	// Gate types enum
	enum class GateType : int32_t { OFF = 0, SINGLE = 1, MULTIPLE = 2, HELD = 3 };

	// Play order presets enum
	enum class PlayOrder : int32_t {
		FORWARDS = 0,  // 1,2,3,4,5,6,7,8
		BACKWARDS = 1, // 8,7,6,5,4,3,2,1
		PING_PONG = 2, // 1,2,3,4,5,6,7,8,7,6,5,4,3,2,1,2,3...
		RANDOM = 3     // Random order each cycle
	};

	// OLED display helpers
	void displayGateTypePopup(int32_t stage);
	void displayNotePopup(int32_t stage);
	void displayOctavePopup(int32_t stage, int32_t direction);
	void displayPulseCountPopup(int32_t stage);
	const char* getGateTypeName(GateType type) const;
	const char* getNoteName(int32_t noteIndex, int32_t octave);

private:
	// Display state
	struct {
		bool needsRefresh = true;
		bool wasPlaying = false;
		int32_t gateLineOffset = 0; // 0-3, maps to Y positions 4-7 (bottom left is y0 x0)
	} displayState;

	// Track which note pad is being held for encoder adjustment
	int32_t heldNotePad = -1; // -1 = none, 0-7 = stage index

	// Pulse sequencer engine state
	struct {
		bool isPlaying = false;
		int32_t currentPulse = 0;       // Current pulse (0 to totalPatternLength-1)
		int32_t currentVisualStage = 0; // Which stage pad to flash (visual only)
		int32_t currentStagePulse = 0;  // Pulse within current visual stage
		int32_t lastPlayedStage = -1;   // Last stage that played (for flash feedback)
		int32_t totalPatternLength = 8; // Total length (sum of all pulse counts)

		// Visual feedback state
		bool gatePadFlashing = false;
		uint32_t flashStartTime = 0;
		uint32_t flashDuration = 50; // Flash duration in milliseconds (shorter for multiple notes)
		int32_t flashPosition = 0;   // Position across the gate pad (0-7)

		// Per-note tracking for proper note-off handling
		std::array<int16_t, ARP_MAX_INSTRUCTION_NOTES> noteCodeCurrentlyOnPostArp = {ARP_NOTE_NONE};
		std::array<uint8_t, ARP_MAX_INSTRUCTION_NOTES> outputMIDIChannelForNoteCurrentlyOnPostArp = {MIDI_CHANNEL_NONE};
		std::array<uint32_t, ARP_MAX_INSTRUCTION_NOTES> noteGatePos = {0};     // Track gate position for each note
		std::array<bool, ARP_MAX_INSTRUCTION_NOTES> noteActive = {false};      // Track if each note is active
		std::array<int32_t, ARP_MAX_INSTRUCTION_NOTES> noteSourceStage = {-1}; // Track which stage triggered each note
	} sequencerState;

	// Stage data (8 stages, one per column)
	struct StageData {
		GateType gateType = GateType::OFF;
		int32_t noteIndex = 0;   // Index in current scale
		int32_t octave = 0;      // Octave offset from base
		int32_t pulseCount = 1;  // 1-7, default is 1
		int32_t accumulator = 0; // -7 to +7, pitch accumulator for this stage
	};

	StageData stages[8];

	// Arpeggiator-style note for the instruction system
	ArpNote currentNote;

	// Performance controls (for future implementation)
	struct {
		int32_t transpose = 0;    // Pre-scale transpose
		int32_t octave = 0;       // Octave shift
		int32_t clockDivider = 2; // Clock divider (/1=32nd, /2=16th, /4=8th, /8=qtr, /16=half, /32=whole)
		int32_t numStages = 8;    // Number of active stages (1-8)
		PlayOrder playOrder = PlayOrder::FORWARDS; // Stage play order
		int32_t pingPongDirection = 1;             // 1 = forwards, -1 = backwards (for ping pong)

		// Velocity spread values for each pad (0-50)
		int32_t velocitySpreadValues[8] = {0, 5, 10, 15, 20, 25, 30, 50};
		int32_t lastTouchedVelocityPad = -1;

		// Note probability values for each pad (0-100, where 100 = OFF/default)
		int32_t noteProbabilityValues[8] = {0, 10, 25, 50, 75, 90, 95, 100};
		int32_t lastTouchedProbabilityPad = -1;

		// Stage enable/disable state (true = enabled, false = disabled/skipped)
		bool stageEnabled[8] = {true, true, true, true, true, true, true, true};

		// Current stage (separate from pulse position for stage skipping)
		int32_t currentStage = 0;

		// Gate values for each pad (0-50)
		int32_t gateValues[8] = {1, 5, 12, 20, 25, 30, 40, 50};
		int32_t lastTouchedGatePad = -1;
	} performanceControls;
};

}; // namespace deluge::gui::ui::keyboard::layout
