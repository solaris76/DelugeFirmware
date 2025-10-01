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

#include "gui/ui/keyboard/layout/column_controls.h"
#include "gui/l10n/strings.h"

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

	/// Update animation and check if display needs refreshing
	void updateAnimation();

	/// Handle swung tick - called from playback handler on musical ticks
	void handleSwungTick(uint64_t currentTick);

	/// Direct pad LED update for real-time animation
	void updatePadLEDsDirect();

	/// Update display for both OLED and 7-segment
	void updateDisplay();

private:
	/// Get the current arpeggiator settings from the active clip
	ArpeggiatorSettings* getArpSettings();

	/// Get the current arpeggiator instance from the active instrument
	Arpeggiator* getArpeggiator();

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

	/// Handle gate control (note length adjustment)
	void handleGateControl(int32_t gateIndex);

	/// Handle play order preset selection
	void handlePlayOrderChange(int32_t playOrderIndex);

	/// Handle transpose change
	void handleTransposeChange(int32_t direction);

	/// Handle octave change
	void handleOctaveChange(int32_t direction);

	/// Handle pulse count adjustment for a specific stage
	void handlePulseCount(int32_t stage, int32_t position);

	/// Evaluate rhythm pattern to determine if note should play (auto-generated from gate type and pulse count)
	bool evaluateRhythmPattern(int32_t stage, int32_t pulsePosition);

	/// Calculate total pattern length based on all stage pulse counts
	int32_t calculateTotalPatternLength() const;

	/// Reset sequencer to start of pattern
	void resetToPatternStart();

	/// Pulse sequencer engine methods
	void updateSequencer();
	void resetSequencerState();
	void advanceStage();
	bool isDelugePlaying() const;
	void generateNote();
	void generateSimpleTestNote(); // TEST: Simple quarter note test function
	void updateVisualFeedback();
	void triggerGatePadFlash(int32_t stage, int32_t pulsePosition);
	bool isGatePadFlashing() const;

	/// Get gate type color for a specific stage
	RGB getGateTypeColor(int32_t stage) const;

	/// Get note selection color for a specific stage
	RGB getNoteSelectionColor(int32_t stage) const;

	/// Get octave control color
	RGB getOctaveControlColor() const;

	/// Get pulse count color for a specific stage and position
	RGB getPulseCountColor(int32_t stage, int32_t position) const;

	/// Get gate control color for performance pad
	RGB getGateControlColor(int32_t gateIndex) const;

	/// Get play order preset color
	RGB getPlayOrderColor(int32_t playOrderIndex) const;

	/// Get transpose control color
	RGB getTransposeColor(int32_t direction) const;

	/// Get octave control color
	RGB getOctaveControlColor(int32_t direction) const;


public:
	// Gate types enum
	enum class GateType : int32_t {
		OFF = 0,
		SINGLE = 1,
		MULTIPLE = 2,
		HELD = 3
	};

	// Play order presets enum
	enum class PlayOrder : int32_t {
		FORWARDS = 0,    // 1,2,3,4,5,6,7,8
		BACKWARDS = 1,   // 8,7,6,5,4,3,2,1
		PING_PONG = 2,   // 1,2,3,4,5,6,7,8,7,6,5,4,3,2,1,2,3...
		RANDOM = 3       // Random order each cycle
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

	// Track which gate control pad was last touched for LED feedback
	int32_t lastTouchedGatePad = -1; // -1 = none, 0-7 = gate index

	// Pulse sequencer engine state
	struct {
		bool isPlaying = false;
		int32_t currentStage = 0;        // 0-7 (stages 1-8)
		int32_t currentPulseInStage = 0; // 0 to pulseCount-1
		int32_t stageStartTime = 0;      // When current stage started
		bool gateCurrentlyActive = false;
		uint32_t gatePos = 0;

		// Pattern state for variable-length patterns
		int32_t totalPatternLength = 8;  // Total length of current pattern (8-56)
		int32_t currentPatternPosition = 0; // Position within the full pattern (0 to totalPatternLength-1)

		// Visual feedback state
		bool gatePadFlashing = false;
		uint32_t flashStartTime = 0;
		uint32_t flashDuration = 100; // Flash duration in milliseconds
		int32_t flashPosition = 0;    // Position across the gate pad (0-7)

		// Note tracking for proper note-off handling
		int32_t activeNote = -1; // Currently playing note (-1 = none)
		uint64_t noteOnTick = 0; // Tick when note was triggered
		uint32_t noteGateLength = 0; // Gate length for current note
	} sequencerState;

	// Stage data (8 stages, one per column)
	struct StageData {
		GateType gateType = GateType::OFF;
		int32_t noteIndex = 0; // Index in current scale
		int32_t octave = 0; // Octave offset from base
		int32_t pulseCount = 1; // 1-7, default is 1
		int32_t accumulator = 0; // -7 to +7, pitch accumulator for this stage
	};

	StageData stages[8];

	// Performance controls (for future implementation)
	struct {
		int32_t transpose = 0;    // Pre-scale transpose
		int32_t octave = 0;       // Octave shift
		int32_t clockDivider = 2; // Clock divider (1=32nd, 2=16th, 4=8th, 8=quarter, 16=half, 32=whole)
		int32_t numStages = 8;    // Number of active stages (1-8)
		PlayOrder playOrder = PlayOrder::FORWARDS; // Stage play order
		int32_t pingPongDirection = 1; // 1 = forwards, -1 = backwards (for ping pong)
	} performanceControls;
};

}; // namespace deluge::gui::ui::keyboard::layout
