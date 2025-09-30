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


public:
	// Gate types enum
	enum class GateType : int32_t {
		OFF = 0,
		SINGLE = 1,
		MULTIPLE = 2,
		HELD = 3
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
	} sequencerState;

	// Stage data (8 stages, one per column)
	struct StageData {
		GateType gateType = GateType::OFF;
		int32_t noteIndex = 0; // Index in current scale
		int32_t octave = 0; // Octave offset from base
		int32_t pulseCount = 1; // 1-7, default is 1
	};

	StageData stages[8];

	// Performance controls (for future implementation)
	struct {
		int32_t transpose = 0;    // Pre-scale transpose
		int32_t octave = 0;       // Octave shift
		int32_t clockRate = 1;    // Clock rate multiplier
		int32_t numStages = 8;    // Number of active stages (1-8)
	} performanceControls;
};

}; // namespace deluge::gui::ui::keyboard::layout
