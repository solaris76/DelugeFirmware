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

#include "gui/ui/keyboard/layout/pulse_seq.h"
#include "gui/colour/colour.h"
#include "gui/menu_item/value_scaling.h"
#include "gui/ui/keyboard/keyboard_screen.h"
#include "gui/ui/sound_editor.h"
#include "gui/ui_timer_manager.h"
#include "hid/display/display.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/instrument/midi_instrument.h"
#include "model/instrument/non_audio_instrument.h"
#include "model/model_stack.h"
#include "model/scale/note_set.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"

namespace deluge::gui::ui::keyboard::layout {

// Constants for better code readability
constexpr int32_t kMaxStages = 8;
constexpr int32_t kMaxPulseCount = 7;
constexpr int32_t kPopupTimeoutMs = 2000;

void KeyboardLayoutPulseSeq::evaluatePads(PressedPad presses[kMaxNumKeyboardPadPresses]) {
	currentNotesState = NotesState{}; // Reset active notes

	int32_t gateLineY = getGateLineY();

	// Reset held pad tracking
	heldNotePad = -1;

	for (int32_t idxPress = 0; idxPress < kMaxNumKeyboardPadPresses; ++idxPress) {
		auto pressed = presses[idxPress];
		if (pressed.active && pressed.x < kDisplayWidth) {
			int32_t x = pressed.x;
			int32_t y = pressed.y;

			// Track if a note pad is being held (y = gateLineY + 1, x < 8)
			if (y == gateLineY + 1 && x < 8) {
				heldNotePad = x; // Store which stage's note pad is held
			}

			// Gate line (y0-y3, x0-x7) - bottom left is y0 x0
			if (y == gateLineY && x < 8) {
				handleGateType(x);
			}
			// Note selection (above gate line) - 8 columns only
			else if (y == gateLineY + 1 && x < 8) {
				handleNoteSelection(x);
			}
			// Octave controls (above note selection) - 8 columns only
			else if (y == gateLineY + 2 && x < 8) {
				handleOctaveAdjustment(x, -1); // Octave down
			}
			else if (y == gateLineY + 3 && x < 8) {
				handleOctaveAdjustment(x, 1); // Octave up
			}
			// Rhythm pattern selection removed - patterns are auto-generated from gate type and pulse count
			// Pulse count pads (below gate line)
			else if (y < gateLineY && x < 8) {
				handlePulseCount(x, gateLineY - 1 - y);
			}
			// Performance controls on right side (x8-15)
			// y4: Stage count control
			else if (y == 4 && x >= 8 && x < kDisplayWidth) {
				handleStageCountChange(x - 7); // 1-8
			}
			// y1: Play order presets
			else if (y == 1 && x >= 8 && x < 12) {
				handlePlayOrderChange(x - 8); // 0-3
			}
			// y2: Gate control (x8-15)
			else if (y == 2 && x >= 8 && x < kDisplayWidth) {
				handleGate(x - 8); // 0-7
			}
			// y3: Stage enable/disable toggle (x8-15)
			else if (y == 3 && x >= 8 && x < kDisplayWidth) {
				handleStageToggle(x - 8); // 0-7
			}
			// y5: Velocity spread control (x8-15)
			else if (y == 5 && x >= 8 && x < kDisplayWidth) {
				handleVelocitySpread(x - 8); // 0-7
			}
			// y6: Note probability control (x8-15)
			else if (y == 6 && x >= 8 && x < kDisplayWidth) {
				handleNoteProbability(x - 8); // 0-7
			}
			// y7: Reset button, randomize button, evolve button, and transpose/octave controls
			else if (y == 7 && x >= 8 && x < kDisplayWidth) {
				if (x == 8) {
					resetToDefaults(); // Reset all settings
				}
				else if (x == 9) {
					randomizeSequence(); // Randomize sequence settings
				}
				else if (x == 10) {
					evolveSequence(); // Evolve sequence subtly
				}
				else if (x == 12) {
					handleTransposeChange(-1); // Transpose -1
				}
				else if (x == 13) {
					handleTransposeChange(1); // Transpose +1
				}
				else if (x == 14) {
					handleOctaveChange(-1); // Octave down
				}
				else if (x == 15) {
					handleOctaveChange(1); // Octave up
				}
			}
		}
	}

	// Should be called last so currentNotesState can be read
	ColumnControlsKeyboard::evaluatePads(presses);
}

void KeyboardLayoutPulseSeq::handleVerticalEncoder(int32_t offset) {
	// If a note pad is being held, adjust that stage's accumulator instead of scrolling
	if (heldNotePad >= 0 && heldNotePad < 8) {
		// Adjust accumulator value (-7 to +7)
		int32_t newAccumulator = stages[heldNotePad].accumulator + offset;

		// Clamp to range -7 to +7
		if (newAccumulator < -7)
			newAccumulator = -7;
		if (newAccumulator > 7)
			newAccumulator = 7;

		if (stages[heldNotePad].accumulator != newAccumulator) {
			stages[heldNotePad].accumulator = newAccumulator;

			// Show accumulator popup
			if (display->haveOLED()) {
				char text[30];
				strcpy(text, "Accumulator: ");
				if (newAccumulator >= 0) {
					strcat(text, "+");
				}
				intToString(newAccumulator, text + strlen(text));
				display->displayPopup(text);
				uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
			}

			displayState.needsRefresh = true;
		}
	}
	else {
		// No pad held - scroll the gate line to reveal pulse count pads below
		int32_t newOffset = displayState.gateLineOffset + offset;
		if (newOffset < 0)
			newOffset = 0;
		if (newOffset > 3)
			newOffset = 3; // 0-3 maps to y4-y7

		if (newOffset != displayState.gateLineOffset) {
			displayState.gateLineOffset = newOffset;
			displayState.needsRefresh = true;
		}
	}
}

void KeyboardLayoutPulseSeq::handleHorizontalEncoder(int32_t offset, bool shiftEnabled,
                                                     PressedPad presses[kMaxNumKeyboardPadPresses],
                                                     bool encoderPressed) {
	if (horizontalEncoderHandledByColumns(offset, shiftEnabled)) {
		return;
	}

	// Clock divider control: 1 (/1 = 32nd notes) to 64 (/64 = ultra slow)
	// Incremental: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10... up to 64
	int32_t newDivider = performanceControls.clockDivider;

	if (offset > 0) {
		// Increase divider (slower tempo)
		if (newDivider < 64) {
			newDivider++;
		}
	}
	else if (offset < 0) {
		// Decrease divider (faster tempo)
		if (newDivider > 1) {
			newDivider--;
		}
	}

	if (newDivider != performanceControls.clockDivider) {
		performanceControls.clockDivider = newDivider;

		// Show popup with clock divider as fraction (all values 1-64)
		if (display->haveOLED()) {
			char text[20];
			sprintf(text, "Clock: /%d", newDivider);
			display->popupText(text);

			// Set custom 2-second timeout for OLED
			uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
		}

		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::precalculate() {
	// Pre-calculate any colors or values needed for rendering
	// Initialize total pattern length for the first time
	if (sequencerState.totalPatternLength == 8) {
		sequencerState.totalPatternLength = calculateTotalPatternLength();
	}
}

int32_t KeyboardLayoutPulseSeq::doTickForward(uint32_t clipCurrentPos, bool currentlyPlayingReversed,
                                              ArpReturnInstruction* instruction) {
	// Only generate notes if arpeggiator is off
	ArpeggiatorSettings* arpSettings = getArpSettings();
	if (arpSettings && arpSettings->mode != ArpMode::OFF) {
		return 2147483647; // No timing support when arpeggiator is on
	}

	// Handle playback start/stop detection
	static bool hasBeenInitialized = false;
	static bool wasPlayingLastTime = false;
	bool isCurrentlyPlaying = (clipCurrentPos > 0);

	if (!hasBeenInitialized) {
		resetSequencerState();
		hasBeenInitialized = true;
		wasPlayingLastTime = isCurrentlyPlaying;
	}
	else if (wasPlayingLastTime && !isCurrentlyPlaying) {
		// Playback just stopped - send all notes off
		sendAllNotesOff();
		sequencerState.isPlaying = false;
	}
	else if (!wasPlayingLastTime && isCurrentlyPlaying) {
		// Playback just started
		sequencerState.isPlaying = true;
	}

	wasPlayingLastTime = isCurrentlyPlaying;

	// Calculate ticks per period based on clock divider
	// Use the same sync level as the arpeggiator for consistency
	uint32_t syncLevel = arpSettings ? arpSettings->syncLevel : 6; // Default to 16th notes if no arp settings
	uint32_t ticksPerPeriod = 3 << (9 - syncLevel);
	ticksPerPeriod *= performanceControls.clockDivider; // Clock divider - multiply to make slower
	ticksPerPeriod /= 2;                                // Correct timing - was running at half speed

	int32_t howFarIntoPeriod = clipCurrentPos % ticksPerPeriod;

	// Handle note-offs manually since arpeggiator mode is OFF
	// Calculate gate length for SINGLE/MULTIPLE gates (short, per-pulse duration)
	uint32_t gateLength = ticksPerPeriod / 16; // Very short default gate for single pulses

	if (arpSettings) {
		uint32_t gatePercent = computeCurrentValueForStandardMenuItem(arpSettings->gate);
		// Scale gate to be appropriate for single pulse duration
		gateLength = (gatePercent * ticksPerPeriod) / 400; // Much shorter gates for single pulses
		if (gateLength < 1)
			gateLength = 1; // Minimum gate length
		if (gateLength > ticksPerPeriod / 8)
			gateLength = ticksPerPeriod / 8; // Max 12.5% of period for single pulses
	}

	// Track note-offs for active notes
	for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
		if (sequencerState.noteActive[n]) {
			sequencerState.noteGatePos[n]++;

			// Check if this note should be turned off
			uint32_t noteGateLength = gateLength;

			// Use the source stage that triggered this note for gate calculation
			int32_t sourceStage = sequencerState.noteSourceStage[n];
			if (sourceStage >= 0 && sourceStage < 8) {
				if (stages[sourceStage].gateType == GateType::HELD) {
					// For HELD gate types, extend gate length to cover entire stage duration
					noteGateLength = ticksPerPeriod * stages[sourceStage].pulseCount;
				}
				// For other gate types, use normal gate length (already set above)
			}

			if (sequencerState.noteGatePos[n] >= noteGateLength) {
				switchNoteOff(instruction, n);
			}
		}
	}

	if (!howFarIntoPeriod) {

		// Flash pad for visual feedback
		sequencerState.gatePadFlashing = true;
		sequencerState.flashStartTime = playbackHandler.getCurrentInternalTickCount();
		sequencerState.lastPlayedStage = sequencerState.currentVisualStage;
		keyboardScreen.requestMainPadsRendering();

		// Generate notes based on rhythm pattern
		generateNotes(instruction);
	}
	else {
		if (!currentlyPlayingReversed) {
			howFarIntoPeriod = ticksPerPeriod - howFarIntoPeriod;
		}
	}

	return howFarIntoPeriod;
}

void KeyboardLayoutPulseSeq::generateNotes(ArpReturnInstruction* instruction) {
	// Use current stage directly (skip disabled stages completely)
	int32_t stage = performanceControls.currentStage;

	if (stage >= 0 && stage < performanceControls.numStages && performanceControls.stageEnabled[stage]) {
		StageData& stageData = stages[stage];

		// Calculate pulse position within the current stage
		int32_t pulseInStage = sequencerState.currentPulse;

		// Use gate type logic to determine if we should play
		if (evaluateRhythmPattern(stage, pulseInStage)) {
			playNoteForStage(instruction, stage);
		}
	}

	// Advance visual stage for pad flashing
	sequencerState.lastPlayedStage = performanceControls.currentStage;

	// Advance to next pulse within current stage, or next stage
	sequencerState.currentPulse++;
	if (sequencerState.currentPulse >= stages[performanceControls.currentStage].pulseCount) {
		// Finished current stage, move to next enabled stage
		sequencerState.currentPulse = 0;
		advanceToNextEnabledStage();
	}
}

int32_t KeyboardLayoutPulseSeq::findStageForPulse(int32_t pulse) {
	int32_t stageStartPulse = 0;

	for (int32_t stage = 0; stage < performanceControls.numStages; stage++) {
		int32_t stageEndPulse = stageStartPulse + stages[stage].pulseCount;

		if (pulse >= stageStartPulse && pulse < stageEndPulse) {
			return stage;
		}

		stageStartPulse = stageEndPulse;
	}

	return -1; // Not found
}

void KeyboardLayoutPulseSeq::playNoteForStage(ArpReturnInstruction* instruction, int32_t stage) {
	StageData& stageData = stages[stage];

	// Only generate notes for non-OFF gate types
	if (stageData.gateType == GateType::OFF) {
		return; // No note generation for rest
	}

	// Calculate note from scale
	NoteSet& scaleNotes = getScaleNotes();
	uint8_t scaleNoteCount = getScaleNoteCount();

	// Get note from stage's noteIndex and octave
	// Start from C3 (MIDI note 48) as base octave for better range
	constexpr int32_t kBaseOctave = 48; // C3

	// Calculate base note index with accumulator applied
	int32_t noteIndexWithAccumulator = stageData.noteIndex + stageData.accumulator;

	// Wrap around the scale if needed
	while (noteIndexWithAccumulator < 0)
		noteIndexWithAccumulator += scaleNoteCount;
	while (noteIndexWithAccumulator >= scaleNoteCount)
		noteIndexWithAccumulator -= scaleNoteCount;

	int32_t note = kBaseOctave + getRootNote() + scaleNotes[noteIndexWithAccumulator] + (stageData.octave * kOctaveSize)
	               + performanceControls.transpose               // Apply transpose (within scale)
	               + (performanceControls.octave * kOctaveSize); // Apply global octave shift

	// Clamp note to valid range
	if (note < 0)
		note = 0;
	if (note > 127)
		note = 127;

	// Get default velocity
	uint8_t velocity = getDefaultVelocity();

	// Add note to arpeggiator's internal list first
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (clip) {
		MelodicInstrument* melodicInstrument = (MelodicInstrument*)clip->output;
		if (melodicInstrument) {
			NonAudioInstrument* nonAudioInstrument = (NonAudioInstrument*)melodicInstrument;
			if (nonAudioInstrument) {
				// Add note to arpeggiator's internal list
				ArpeggiatorSettings* arpSettings = getArpSettings();
				nonAudioInstrument->arpeggiator.noteOn(arpSettings, note, velocity, instruction, MIDI_CHANNEL_NONE,
				                                       nullptr);
			}
		}
	}

	// Track this note for manual gate timing (since arp mode is OFF)
	int32_t noteSlot = -1;
	for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
		if (!sequencerState.noteActive[n]) {
			noteSlot = n;
			break;
		}
	}

	// If no slot available, use slot 0 (simple replacement)
	if (noteSlot == -1) {
		noteSlot = 0;
	}

	// Activate the note slot for gate timing
	sequencerState.noteActive[noteSlot] = true;
	sequencerState.noteGatePos[noteSlot] = 0;
	sequencerState.noteCodeCurrentlyOnPostArp[noteSlot] = note;
	sequencerState.noteSourceStage[noteSlot] = stage; // Track which stage triggered this note
}

void KeyboardLayoutPulseSeq::switchNoteOff(ArpReturnInstruction* instruction, int32_t noteSlot) {
	if (noteSlot < 0 || noteSlot >= ARP_MAX_INSTRUCTION_NOTES) {
		return;
	}

	if (!sequencerState.noteActive[noteSlot]) {
		return;
	}

	// Send note-off via arpeggiator - it already handles note-off when OFF
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (clip) {
		MelodicInstrument* melodicInstrument = (MelodicInstrument*)clip->output;
		if (melodicInstrument) {
			NonAudioInstrument* nonAudioInstrument = (NonAudioInstrument*)melodicInstrument;
			if (nonAudioInstrument) {
				// Get the note that was played
				int32_t note = sequencerState.noteCodeCurrentlyOnPostArp[noteSlot];
				if (note != ARP_NOTE_NONE) {
					// Call arpeggiator's noteOff - it will send note-off when ArpMode::OFF
					ArpeggiatorSettings* arpSettings = getArpSettings();
					nonAudioInstrument->arpeggiator.noteOff(arpSettings, note, instruction);
				}
			}
		}
	}

	// Clear this note's state
	sequencerState.noteCodeCurrentlyOnPostArp[noteSlot] = ARP_NOTE_NONE;
	sequencerState.outputMIDIChannelForNoteCurrentlyOnPostArp[noteSlot] = MIDI_CHANNEL_NONE;
	sequencerState.noteGatePos[noteSlot] = 0;
	sequencerState.noteActive[noteSlot] = false;
}

void KeyboardLayoutPulseSeq::switchAnyNoteOff(ArpReturnInstruction* instruction) {
	// Turn off all active notes
	for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
		if (sequencerState.noteActive[n]) {
			switchNoteOff(instruction, n);
		}
	}
}

// Gate length is now handled entirely by the arpeggiator system

// OLED display helpers
void KeyboardLayoutPulseSeq::displayGateTypePopup(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages)
		return;

	const char* gateTypeName = getGateTypeName(stages[stage].gateType);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "GATE %d: %s", stage + 1, gateTypeName);

	display->displayPopup(buffer);

	// Set custom 2-second timeout for OLED
	if (display->haveOLED()) {
		uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
	}
}

void KeyboardLayoutPulseSeq::displayNotePopup(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages)
		return;

	const char* noteName = getNoteName(stages[stage].noteIndex, stages[stage].octave);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "NOTE %d: %s", stage + 1, noteName);

	display->displayPopup(buffer);

	// Set custom 2-second timeout for OLED
	if (display->haveOLED()) {
		uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
	}
}

void KeyboardLayoutPulseSeq::displayOctavePopup(int32_t stage, int32_t direction) {
	if (stage < 0 || stage >= kMaxStages)
		return;

	const char* noteName = getNoteName(stages[stage].noteIndex, stages[stage].octave);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "NOTE %d: %s", stage + 1, noteName);

	display->displayPopup(buffer);

	// Set custom 2-second timeout for OLED
	if (display->haveOLED()) {
		uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
	}
}

void KeyboardLayoutPulseSeq::displayPulseCountPopup(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages)
		return;

	char buffer[32];
	snprintf(buffer, sizeof(buffer), "STAGE %d: %d PULSES", stage + 1, stages[stage].pulseCount);

	display->displayPopup(buffer);

	// Set custom 2-second timeout for OLED
	if (display->haveOLED()) {
		uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
	}
}

const char* KeyboardLayoutPulseSeq::getGateTypeName(GateType type) const {
	switch (type) {
	case GateType::OFF:
		return "OFF";
	case GateType::SINGLE:
		return "SINGLE";
	case GateType::MULTIPLE:
		return "MULTIPLE";
	case GateType::HELD:
		return "HELD";
	default:
		return "UNKNOWN";
	}
}

const char* KeyboardLayoutPulseSeq::getNoteName(int32_t noteIndex, int32_t octave) {
	// Get current scale notes
	NoteSet& noteSet = getScaleNotes();
	int32_t numNotes = getScaleNoteCount();

	if (noteIndex < 0 || noteIndex >= numNotes) {
		return "C";
	}

	// Get the note from the scale - NoteSet uses array indexing
	int32_t note = noteSet[noteIndex];

	// Convert to note name (simplified - just use C, D, E, F, G, A, B)
	const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	const char* noteName = noteNames[note % 12];

	// Calculate octave number (C3 = 0, so C4 = 1, etc.)
	int32_t octaveNum = 3 + octave; // Base octave is 3

	static char buffer[16];
	snprintf(buffer, sizeof(buffer), "%s%d", noteName, octaveNum);
	return buffer;
}

// Pulse sequencer engine implementation

void KeyboardLayoutPulseSeq::resetSequencerState() {
	// Send all notes off when Deluge stops
	sendAllNotesOff();

	sequencerState.isPlaying = false;
	sequencerState.currentPulse = 0;
	sequencerState.lastPlayedStage = -1;

	// DON'T reset totalPatternLength - keep the user's custom pulse counts!
	// The pattern length should only be set when pulse counts are changed, not on every play

	// Reset visual feedback state
	sequencerState.gatePadFlashing = false;
	sequencerState.flashStartTime = 0;

	// All stages start as OFF by default - user must enable them manually
}

void KeyboardLayoutPulseSeq::renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Clear all pads first
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		for (int32_t x = 0; x < kDisplayWidth; x++) {
			image[y][x] = RGB{0, 0, 0}; // Black
		}
	}

	int32_t gateLineY = getGateLineY();

	// Render gate line (x0-x7)
	for (int32_t x = 0; x < 8; x++) {
		// Gate type colors with red flash for active stage
		bool shouldFlash = false;
		if (sequencerState.gatePadFlashing && sequencerState.lastPlayedStage == x) {
			// Check if flash duration has expired
			uint32_t currentTime = playbackHandler.getCurrentInternalTickCount();
			uint32_t flashElapsed = currentTime - sequencerState.flashStartTime;
			if (flashElapsed < sequencerState.flashDuration) {
				shouldFlash = true;
			}
			else {
				// Flash duration expired, stop flashing
				sequencerState.gatePadFlashing = false;
			}
		}

		if (shouldFlash) {
			// Different flash colors for different gate types
			if (stages[x].gateType == GateType::OFF) {
				image[gateLineY][x] = RGB{255, 100, 0}; // Orange flash for OFF gates
			}
			else {
				image[gateLineY][x] = RGB{255, 0, 0}; // Red flash for active gates
			}
		}
		else {
			// Normal gate type colors - dim if stage is disabled
			RGB color;
			switch (stages[x].gateType) {
			case GateType::OFF:
				color = RGB{100, 100, 100}; // Dim white
				break;
			case GateType::SINGLE:
				color = RGB{0, 255, 0}; // Green
				break;
			case GateType::MULTIPLE:
				color = RGB{0, 0, 255}; // Blue
				break;
			case GateType::HELD:
				color = RGB{255, 0, 255}; // Magenta
				break;
			default:
				color = RGB{0, 0, 0}; // Black
				break;
			}

			// Dim the color if stage is disabled OR beyond active stage count
			if (!performanceControls.stageEnabled[x] || x >= performanceControls.numStages) {
				color.r /= 8;
				color.g /= 8;
				color.b /= 8;
			}

			image[gateLineY][x] = color;
		}
	}

	// Render note selection pads (above gate line) - 8 columns only
	for (int32_t x = 0; x < 8; x++) {
		if (gateLineY + 1 < kDisplayHeight) {
			// Note selection colors - magenta when accumulator is non-zero, pink when zero
			RGB color = (stages[x].accumulator != 0) ? RGB{255, 0, 255} : RGB{255, 100, 150};

			// Dim if stage is disabled OR beyond active stage count
			if (!performanceControls.stageEnabled[x] || x >= performanceControls.numStages) {
				color.r /= 8;
				color.g /= 8;
				color.b /= 8;
			}

			image[gateLineY + 1][x] = color;
		}
	}

	// Render octave controls (above note selection) - 8 columns only
	for (int32_t x = 0; x < 8; x++) {
		if (gateLineY + 2 < kDisplayHeight) {
			RGB color = RGB{100, 150, 255}; // Octave down
			if (!performanceControls.stageEnabled[x] || x >= performanceControls.numStages) {
				color.r /= 8;
				color.g /= 8;
				color.b /= 8;
			}
			image[gateLineY + 2][x] = color;
		}
		if (gateLineY + 3 < kDisplayHeight) {
			RGB color = RGB{100, 150, 255}; // Octave up
			if (!performanceControls.stageEnabled[x] || x >= performanceControls.numStages) {
				color.r /= 8;
				color.g /= 8;
				color.b /= 8;
			}
			image[gateLineY + 3][x] = color;
		}
	}

	// Rhythm pattern selection removed - patterns are auto-generated from gate type and pulse count

	// Render pulse count display (below gate line, 7 pads with gradient on all 8 columns)
	for (int32_t i = 0; i < 7; i++) {
		if (gateLineY - 1 - i >= 0) {
			for (int32_t x = 0; x < 8; x++) {
				// Inline pulse count color calculation
				if (i < stages[x].pulseCount) {
					// Active pulse positions - purple/pink to cyan gradient (reversed)
					int32_t intensity = ((6 - i) * 255) / 6;
					RGB color = RGB{static_cast<uint8_t>(intensity), static_cast<uint8_t>(255 - intensity), 255};

					// Dim if stage is disabled OR beyond active stage count
					if (!performanceControls.stageEnabled[x] || x >= performanceControls.numStages) {
						color.r /= 8;
						color.g /= 8;
						color.b /= 8;
					}

					image[gateLineY - 1 - i][x] = color;
				}
				else {
					image[gateLineY - 1 - i][x] = RGB{0, 0, 0}; // Inactive pulse positions
				}
			}
		}
	}

	// Render performance controls on right side (x8-15)
	// y4: Stage count control (1-8 stages) - bottom row
	for (int32_t x = 8; x < kDisplayWidth; x++) {
		int32_t stageNum = x - 7; // 1-8 (x8=stage1, x15=stage8)
		if (stageNum <= performanceControls.numStages) {
			image[4][x] = RGB{255, 255, 0}; // Yellow (active stages)
		}
		else {
			image[4][x] = RGB{0, 0, 0}; // Black (inactive stages)
		}
	}

	// y1: Play order presets - 4 pads (x8-11)
	for (int32_t x = 8; x < 12; x++) {
		// Play order colors - highlight selected, dim others
		if (static_cast<int32_t>(performanceControls.playOrder) == (x - 8)) {
			image[1][x] = RGB{0, 255, 255}; // Bright cyan for selected
		}
		else {
			image[1][x] = RGB{0, 128, 128}; // Dim cyan for others
		}
	}

	// y2: Gate control (x8-15) - Green fader (5 left, 50 right)
	for (int32_t x = 8; x < kDisplayWidth; x++) {
		int32_t padIndex = x - 8;
		int32_t currentGate = performanceControls.lastTouchedGatePad >= 0
		                          ? performanceControls.gateValues[performanceControls.lastTouchedGatePad]
		                          : 25;
		int32_t thisPadGate = performanceControls.gateValues[padIndex];

		if (performanceControls.lastTouchedGatePad == padIndex) {
			image[2][x] = RGB{0, 255, 0}; // Bright green for selected
		}
		else if (thisPadGate <= currentGate) {
			image[2][x] = RGB{0, 128, 0}; // Dim green for active range
		}
		else {
			image[2][x] = RGB{0, 0, 0}; // Black for inactive range (fader effect)
		}
	}

	// y3: Stage enable/disable toggle (x8-15) - Orange
	for (int32_t x = 8; x < kDisplayWidth; x++) {
		int32_t stageIndex = x - 8;
		if (performanceControls.stageEnabled[stageIndex]) {
			image[3][x] = RGB{255, 128, 0}; // Bright orange for enabled stages
		}
		else {
			image[3][x] = RGB{64, 32, 0}; // Dim orange for disabled stages
		}
	}

	// y5: Velocity spread control (x8-15) - Light blue fader (0 left, 50 right)
	for (int32_t x = 8; x < kDisplayWidth; x++) {
		int32_t padIndex = x - 8;
		int32_t currentVelocity =
		    performanceControls.lastTouchedVelocityPad >= 0
		        ? performanceControls.velocitySpreadValues[performanceControls.lastTouchedVelocityPad]
		        : 0;
		int32_t thisPadVelocity = performanceControls.velocitySpreadValues[padIndex];

		if (performanceControls.lastTouchedVelocityPad == padIndex) {
			image[5][x] = RGB{100, 200, 255}; // Bright light blue for selected
		}
		else if (thisPadVelocity <= currentVelocity) {
			image[5][x] = RGB{50, 100, 128}; // Dim light blue for active range
		}
		else {
			image[5][x] = RGB{0, 0, 0}; // Black for inactive range (fader effect)
		}
	}

	// y6: Note probability control (x8-15) - Blue fader (0% left, 100% right)
	for (int32_t x = 8; x < kDisplayWidth; x++) {
		int32_t padIndex = x - 8;
		int32_t currentProbability =
		    performanceControls.lastTouchedProbabilityPad >= 0
		        ? performanceControls.noteProbabilityValues[performanceControls.lastTouchedProbabilityPad]
		        : 100;
		int32_t thisPadProbability = performanceControls.noteProbabilityValues[padIndex];

		if (performanceControls.lastTouchedProbabilityPad == padIndex) {
			image[6][x] = RGB{0, 150, 255}; // Bright blue for selected
		}
		else if (thisPadProbability <= currentProbability) {
			image[6][x] = RGB{0, 75, 128}; // Dim blue for active range
		}
		else {
			image[6][x] = RGB{0, 0, 0}; // Black for inactive range (fader effect)
		}
	}

	// y7: Reset button (x8), randomize button (x9), evolve button (x10), and transpose/octave controls (x12-15)
	// Reset button - purple
	image[7][8] = RGB{128, 0, 255}; // Purple reset button
	// Randomize button - bright magenta
	image[7][9] = RGB{255, 0, 128}; // Bright magenta randomize button
	// Evolve button - cyan
	image[7][10] = RGB{0, 255, 255}; // Cyan evolve button

	// Transpose controls with direction-specific colors
	if (performanceControls.transpose != 0) {
		if (performanceControls.transpose < 0) {
			image[7][12] = RGB{255, 128, 0}; // Orange for active -1
			image[7][13] = RGB{64, 32, 0};   // Dim orange for inactive +1
		}
		else {
			image[7][12] = RGB{64, 32, 0};   // Dim orange for inactive -1
			image[7][13] = RGB{255, 128, 0}; // Orange for active +1
		}
	}
	else {
		image[7][12] = RGB{64, 32, 0}; // Dim orange for inactive
		image[7][13] = RGB{64, 32, 0}; // Dim orange for inactive
	}
	// Octave controls with direction-specific colors
	if (performanceControls.octave != 0) {
		if (performanceControls.octave < 0) {
			image[7][14] = RGB{255, 0, 255}; // Magenta for active down
			image[7][15] = RGB{64, 0, 64};   // Dim magenta for inactive up
		}
		else {
			image[7][14] = RGB{64, 0, 64};   // Dim magenta for inactive down
			image[7][15] = RGB{255, 0, 255}; // Magenta for active up
		}
	}
	else {
		image[7][14] = RGB{64, 0, 64}; // Dim magenta for inactive
		image[7][15] = RGB{64, 0, 64}; // Dim magenta for inactive
	}

	// Stage dimming is now handled comprehensively in each rendering section above
}

ArpeggiatorSettings* KeyboardLayoutPulseSeq::getArpSettings() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip)
		return nullptr;
	return &clip->arpSettings;
}

int32_t KeyboardLayoutPulseSeq::getGateLineY() const {
	return displayState.gateLineOffset + 4; // y4-y7 (bottom left is y0 x0)
}

void KeyboardLayoutPulseSeq::handleGateType(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages)
		return; // Gate line only on first 8 columns

	// Cycle through gate types: OFF -> SINGLE -> MULTIPLE -> HELD -> OFF
	int32_t currentType = static_cast<int32_t>(stages[stage].gateType);
	int32_t nextType = (currentType + 1) % 4;
	stages[stage].gateType = static_cast<GateType>(nextType);

	displayGateTypePopup(stage);
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handleNoteSelection(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages)
		return;

	// Get current scale notes
	NoteSet& noteSet = getScaleNotes();
	int32_t numNotes = getScaleNoteCount();

	// Cycle through scale notes
	if (numNotes > 0) {
		stages[stage].noteIndex = (stages[stage].noteIndex + 1) % numNotes;
		displayNotePopup(stage);
		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleOctaveAdjustment(int32_t stage, int32_t direction) {
	if (stage < 0 || stage >= kMaxStages)
		return;

	// Adjust octave with reasonable limits
	int32_t newOctave = stages[stage].octave + direction;
	if (newOctave < -2)
		newOctave = -2;
	if (newOctave > 3)
		newOctave = 3; // -2 to +3 octaves

	if (newOctave != stages[stage].octave) {
		stages[stage].octave = newOctave;
		displayOctavePopup(stage, direction);
		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleStageCountChange(int32_t numStages) {
	if (numStages < 1)
		numStages = 1;
	if (numStages > 8)
		numStages = 8;

	if (performanceControls.numStages != numStages) {
		performanceControls.numStages = numStages;

		// Recalculate total pattern length when stage count changes
		sequencerState.totalPatternLength = calculateTotalPatternLength();

		// Show popup
		if (display->haveOLED()) {
			char text[30];
			strcpy(text, "Stages: ");
			intToString(numStages, text + strlen(text));
			display->popupText(text);

			// Set custom 2-second timeout for OLED
			uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
		}

		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handlePlayOrderChange(int32_t playOrderIndex) {
	if (playOrderIndex < 0 || playOrderIndex > 3)
		return;

	PlayOrder newPlayOrder = static_cast<PlayOrder>(playOrderIndex);

	if (performanceControls.playOrder != newPlayOrder) {
		performanceControls.playOrder = newPlayOrder;

		// Reset ping pong direction when changing play order
		performanceControls.pingPongDirection = 1;

		// Show popup
		if (display->haveOLED()) {
			const char* orderNames[] = {"FORWARDS", "BACKWARDS", "PING PONG", "RANDOM"};
			char text[20];
			strcpy(text, "Order: ");
			strcat(text, orderNames[playOrderIndex]);
			display->popupText(text);

			// Set custom 2-second timeout for OLED
			uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
		}

		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleTransposeChange(int32_t direction) {
	// Transpose affects all notes by moving within the scale
	// direction: -1 for down, +1 for up
	performanceControls.transpose += direction;

	// Keep transpose within reasonable bounds (-12 to +12)
	if (performanceControls.transpose < -12)
		performanceControls.transpose = -12;
	if (performanceControls.transpose > 12)
		performanceControls.transpose = 12;

	// Show popup
	if (display->haveOLED()) {
		char text[30];
		strcpy(text, "Transpose: ");
		if (performanceControls.transpose >= 0) {
			strcat(text, "+");
		}
		intToString(performanceControls.transpose, text + strlen(text));
		display->displayPopup(text);

		// Set custom 2-second timeout for OLED
		uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
	}

	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handleOctaveChange(int32_t direction) {
	// Octave affects all notes by shifting octaves
	// direction: -1 for down, +1 for up
	performanceControls.octave += direction;

	// Keep octave within reasonable bounds (-3 to +3)
	if (performanceControls.octave < -3)
		performanceControls.octave = -3;
	if (performanceControls.octave > 3)
		performanceControls.octave = 3;

	// Show popup
	if (display->haveOLED()) {
		char text[30];
		strcpy(text, "Octave: ");
		if (performanceControls.octave >= 0) {
			strcat(text, "+");
		}
		intToString(performanceControls.octave, text + strlen(text));
		display->displayPopup(text);

		// Set custom 2-second timeout for OLED
		uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);
	}

	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handlePulseCount(int32_t stage, int32_t position) {
	if (stage < 0 || stage >= kMaxStages)
		return;
	if (position < 0 || position >= 7)
		return;

	// Set pulse count to position + 1 (position 0 = pulse count 1, position 6 = pulse count 7)
	int32_t newPulseCount = position + 1;

	if (newPulseCount != stages[stage].pulseCount) {
		int32_t oldPulseCount = stages[stage].pulseCount;
		stages[stage].pulseCount = newPulseCount;

		// Recalculate total pattern length when pulse counts change
		sequencerState.totalPatternLength = calculateTotalPatternLength();

		// Restart pulse index if it becomes invalid
		if (sequencerState.currentPulse >= sequencerState.totalPatternLength) {
			sequencerState.currentPulse = 0;
		}

		displayPulseCountPopup(stage);
		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleVelocitySpread(int32_t x) {
	// Track the last touched velocity pad for LED feedback
	performanceControls.lastTouchedVelocityPad = x;

	// Direct velocity spread control - each pad has its own value
	int32_t newVelocity = performanceControls.velocitySpreadValues[x];

	// Get arpeggiator settings to apply velocity spread
	ArpeggiatorSettings* settings = getArpSettings();
	if (settings) {
		// Check output type to determine which approach to use
		OutputType outputType = getCurrentOutputType();

		if (outputType == OutputType::SYNTH) {
			// Use soundEditor.setup() for synth tracks (works properly)
			InstrumentClip* clip = getCurrentInstrumentClip();
			if (clip) {
				UI* originalUI = getCurrentUI();

				// Set up sound editor context like the official menu
				if (soundEditor.setup(clip, nullptr, 0)) {
					// Now we're in sound editor context - use the official approach
					char modelStackMemory[MODEL_STACK_MAX_SIZE];
					ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);
					ModelStackWithAutoParam* modelStackWithParam =
					    modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_SPREAD_VELOCITY);

					if (modelStackWithParam && modelStackWithParam->autoParam) {
						// Use signed scaling like the menu system
						int32_t finalValue = computeFinalValueForStandardMenuItem(newVelocity);
						modelStackWithParam->autoParam->setCurrentValueInResponseToUserInput(finalValue,
						                                                                     modelStackWithParam);
					}

					// Exit sound editor context back to original UI
					originalUI->focusRegained();
				}
			}
		}
		else {
			// For MIDI/CV tracks, spreadVelocity is in the randomizer menu, not the main arp menu
			// Use direct parameter setting for CV/MIDI tracks (avoids crash)
			// Use proper value scaling like the official menu system (unsigned scaling)
			int32_t scaledValue = computeFinalValueForUnsignedMenuItem(newVelocity);
			settings->spreadVelocity = scaledValue;
		}
	}

	// Display "OFF" for value 0, otherwise show the value
	if (newVelocity == 0) {
		display->displayPopup("Vel. Spread: OFF");
	}
	else {
		char buffer[30];
		sprintf(buffer, "Vel. Spread: %d", newVelocity);
		display->displayPopup(buffer);
	}

	// Set custom 2-second timeout for OLED
	uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);

	// Force UI update
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handleNoteProbability(int32_t x) {
	// Track the last touched probability pad for LED feedback
	performanceControls.lastTouchedProbabilityPad = x;

	// Direct note probability control - each pad has its own value
	int32_t newProbability = performanceControls.noteProbabilityValues[x];

	// Get arpeggiator settings to apply note probability
	ArpeggiatorSettings* settings = getArpSettings();
	if (settings) {
		// Check output type to determine which approach to use
		OutputType outputType = getCurrentOutputType();

		if (outputType == OutputType::SYNTH) {
			// Use soundEditor.setup() for synth tracks (works properly)
			InstrumentClip* clip = getCurrentInstrumentClip();
			if (clip) {
				UI* originalUI = getCurrentUI();

				// Set up sound editor context like the official menu
				if (soundEditor.setup(clip, nullptr, 0)) {
					// Now we're in sound editor context - use the official approach
					char modelStackMemory[MODEL_STACK_MAX_SIZE];
					ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);
					ModelStackWithAutoParam* modelStackWithParam =
					    modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_NOTE_PROBABILITY);

					if (modelStackWithParam && modelStackWithParam->autoParam) {
						// Use unsigned scaling like the menu system
						int32_t finalValue = computeFinalValueForUnsignedMenuItem(newProbability);
						modelStackWithParam->autoParam->setCurrentValueInResponseToUserInput(finalValue,
						                                                                     modelStackWithParam);
					}

					// Exit sound editor context back to original UI
					originalUI->focusRegained();
				}
			}
		}
		else {
			// For MIDI/CV tracks, use direct parameter setting
			int32_t scaledValue = computeFinalValueForUnsignedMenuItem(newProbability);
			settings->noteProbability = scaledValue;
		}
	}

	// Display the percentage (100% = always play, 0% = never play)
	if (newProbability == 100) {
		display->displayPopup("Probability: 100%");
	}
	else {
		char buffer[30];
		sprintf(buffer, "Probability: %d%%", newProbability);
		display->displayPopup(buffer);
	}

	// Set custom 2-second timeout for OLED
	uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);

	// Force UI update
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handleGate(int32_t x) {
	// Track the last touched gate pad for LED feedback
	performanceControls.lastTouchedGatePad = x;

	// Direct gate control - each pad has its own value
	int32_t newGate = performanceControls.gateValues[x];

	// Check output type to determine which approach to use
	OutputType outputType = getCurrentOutputType();

	if (outputType == OutputType::SYNTH) {
		// Use soundEditor.setup() for synth tracks (works properly)
		InstrumentClip* clip = getCurrentInstrumentClip();
		if (clip) {
			UI* originalUI = getCurrentUI();

			// Set up sound editor context like the official menu
			if (soundEditor.setup(clip, nullptr, 0)) {
				// Now we're in sound editor context - use the official approach
				char modelStackMemory[MODEL_STACK_MAX_SIZE];
				ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);
				ModelStackWithAutoParam* modelStackWithParam =
				    modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_ARP_GATE);

				if (modelStackWithParam && modelStackWithParam->autoParam) {
					// Use absolute value (0-50) without scaling
					int32_t finalValue = computeFinalValueForStandardMenuItem(newGate);
					modelStackWithParam->autoParam->setCurrentValueInResponseToUserInput(finalValue,
					                                                                     modelStackWithParam);
				}

				// Exit sound editor context back to original UI
				originalUI->focusRegained();
			}
		}
	}
	else {
		// Use direct parameter setting for CV/MIDI tracks (avoids crash)
		// Use proper value scaling like the official menu system
		ArpeggiatorSettings* settings = getArpSettings();
		if (settings) {
			int32_t scaledValue = computeFinalValueForStandardMenuItem(newGate);
			settings->gate = scaledValue;
		}
	}

	// Display the gate value
	char buffer[30];
	sprintf(buffer, "Gate: %d", newGate);
	display->displayPopup(buffer);

	// Set custom 2-second timeout for OLED
	uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);

	// Force UI update
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handleStageToggle(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages)
		return;

	// Toggle the stage enabled state
	performanceControls.stageEnabled[stage] = !performanceControls.stageEnabled[stage];

	// Display the new state
	if (performanceControls.stageEnabled[stage]) {
		char buffer[30];
		sprintf(buffer, "Stage %d: ON", stage + 1);
		display->displayPopup(buffer);
	}
	else {
		char buffer[30];
		sprintf(buffer, "Stage %d: OFF", stage + 1);
		display->displayPopup(buffer);
	}

	// Set custom 2-second timeout for OLED
	uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);

	// Force UI update
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::advanceToNextEnabledStage() {
	int32_t nextStage = performanceControls.currentStage;
	int32_t attempts = 0;

	// Determine direction based on play order
	int32_t direction = 1; // Default forward
	switch (performanceControls.playOrder) {
	case PlayOrder::FORWARDS:
		direction = 1;
		break;
	case PlayOrder::BACKWARDS:
		direction = -1;
		break;
	case PlayOrder::PING_PONG:
		direction = performanceControls.pingPongDirection;
		break;
	case PlayOrder::RANDOM:
		// Handle random separately
		int32_t enabledStages[8];
		int32_t enabledCount = 0;

		for (int32_t i = 0; i < performanceControls.numStages; i++) {
			if (performanceControls.stageEnabled[i]) {
				enabledStages[enabledCount++] = i;
			}
		}

		if (enabledCount > 0) {
			int32_t randomIndex = getRandom255() % enabledCount;
			performanceControls.currentStage = enabledStages[randomIndex];
		}
		return;
	}

	// Find the next enabled stage in the given direction
	do {
		nextStage += direction;
		attempts++;

		// Handle wrapping
		if (nextStage >= performanceControls.numStages) {
			if (performanceControls.playOrder == PlayOrder::PING_PONG) {
				nextStage = performanceControls.numStages - 2;
				performanceControls.pingPongDirection = -1;
				direction = -1;
			}
			else {
				nextStage = 0; // Wrap to beginning
			}
		}
		else if (nextStage < 0) {
			if (performanceControls.playOrder == PlayOrder::PING_PONG) {
				nextStage = 1;
				performanceControls.pingPongDirection = 1;
				direction = 1;
			}
			else {
				nextStage = performanceControls.numStages - 1; // Wrap to end
			}
		}

		// Safety check to prevent infinite loops
		if (attempts > performanceControls.numStages) {
			return; // If all stages are disabled, stay on current stage
		}

	} while (!performanceControls.stageEnabled[nextStage]);

	performanceControls.currentStage = nextStage;
}

void KeyboardLayoutPulseSeq::resetToDefaults() {
	// Reset all performance controls to default values
	performanceControls.transpose = 0;
	performanceControls.octave = 0;
	performanceControls.clockDivider = 2;
	performanceControls.numStages = 8;
	performanceControls.playOrder = PlayOrder::FORWARDS;
	performanceControls.pingPongDirection = 1;
	performanceControls.lastTouchedVelocityPad = -1;
	performanceControls.lastTouchedProbabilityPad = -1;
	performanceControls.currentStage = 0;

	// Reset all stage enabled states to true
	for (int32_t i = 0; i < 8; i++) {
		performanceControls.stageEnabled[i] = true;
	}

	// Reset all stage data to defaults
	for (int32_t i = 0; i < 8; i++) {
		stages[i].gateType = GateType::OFF;
		stages[i].noteIndex = 0;
		stages[i].octave = 0;
		stages[i].pulseCount = 1;
		stages[i].accumulator = 0;
	}

	// Reset arpeggiator settings to defaults
	ArpeggiatorSettings* settings = getArpSettings();
	if (settings) {
		// Check output type to determine which approach to use
		OutputType outputType = getCurrentOutputType();

		if (outputType == OutputType::SYNTH) {
			// Reset synth parameters via sound editor
			InstrumentClip* clip = getCurrentInstrumentClip();
			if (clip) {
				UI* originalUI = getCurrentUI();
				if (soundEditor.setup(clip, nullptr, 0)) {
					char modelStackMemory[MODEL_STACK_MAX_SIZE];
					ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);

					// Reset velocity spread
					ModelStackWithAutoParam* velSpreadParam =
					    modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_SPREAD_VELOCITY);
					if (velSpreadParam && velSpreadParam->autoParam) {
						velSpreadParam->autoParam->setCurrentValueInResponseToUserInput(0, velSpreadParam);
					}

					// Reset note probability
					ModelStackWithAutoParam* noteProbParam =
					    modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_NOTE_PROBABILITY);
					if (noteProbParam && noteProbParam->autoParam) {
						int32_t maxValue = computeFinalValueForUnsignedMenuItem(100); // 100% = always play
						noteProbParam->autoParam->setCurrentValueInResponseToUserInput(maxValue, noteProbParam);
					}

					// Reset gate to default
					ModelStackWithAutoParam* gateParam =
					    modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_ARP_GATE);
					if (gateParam && gateParam->autoParam) {
						int32_t defaultGate = computeFinalValueForStandardMenuItem(25); // Default gate 25 (50%)
						gateParam->autoParam->setCurrentValueInResponseToUserInput(defaultGate, gateParam);
					}

					originalUI->focusRegained();
				}
			}
		}
		else {
			// Reset MIDI/CV parameters directly
			settings->spreadVelocity = 0;
			settings->noteProbability = 4294967295u;                   // Max value = 100%
			settings->syncLevel = static_cast<SyncLevel>(6);           // 16th notes
			settings->gate = computeFinalValueForStandardMenuItem(25); // Default gate 25 (50%)
		}
	}

	// Reset sequencer state
	sequencerState.totalPatternLength = calculateTotalPatternLength();

	// Show confirmation popup
	display->displayPopup("RESET TO DEFAULTS");
	uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);

	// Force UI update
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::randomizeSequence() {
	// Get current scale info for note randomization
	NoteSet& scaleNotes = getScaleNotes();
	uint8_t scaleNoteCount = getScaleNoteCount();

	// Randomize all stage data (but keep performance controls unchanged)
	for (int32_t i = 0; i < 8; i++) {
		// Randomize gate type (avoid OFF for more interesting patterns)
		int32_t gateTypeIndex = getRandom255() % 3;                    // 0-2 (SINGLE, MULTIPLE, HELD - skip OFF)
		stages[i].gateType = static_cast<GateType>(gateTypeIndex + 1); // +1 to skip OFF

		// Randomize note index within current scale
		if (scaleNoteCount > 0) {
			stages[i].noteIndex = getRandom255() % scaleNoteCount;
		}

		// Randomize octave (-2 to +3)
		stages[i].octave = (getRandom255() % 6) - 2; // -2 to +3

		// Randomize pulse count (1-7, with bias toward lower values)
		uint8_t random = getRandom255();
		if (random < 128) {
			stages[i].pulseCount = 1; // 50% chance of 1 pulse
		}
		else if (random < 192) {
			stages[i].pulseCount = 2; // 25% chance of 2 pulses
		}
		else if (random < 224) {
			stages[i].pulseCount = 3; // 12.5% chance of 3 pulses
		}
		else if (random < 240) {
			stages[i].pulseCount = 4; // 6.25% chance of 4 pulses
		}
		else {
			stages[i].pulseCount = (getRandom255() % 3) + 5; // 6.25% chance of 5-7 pulses
		}

		// Reset accumulator
		stages[i].accumulator = 0;
	}

	// Recalculate pattern length
	sequencerState.totalPatternLength = calculateTotalPatternLength();

	// Show confirmation popup
	display->displayPopup("SEQUENCE RANDOMIZED");
	uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);

	// Force UI update
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::evolveSequence() {
	// Get current scale info for note evolution
	NoteSet& scaleNotes = getScaleNotes();
	uint8_t scaleNoteCount = getScaleNoteCount();

	// Evolve sequence with subtle changes (only modify some stages)
	int32_t numStagesToChange = (getRandom255() % 4) + 1; // Change 1-4 stages randomly

	for (int32_t change = 0; change < numStagesToChange; change++) {
		int32_t stageToChange = getRandom255() % 8; // Pick random stage

		// 70% chance to change note, 30% chance to change octave
		if (getRandom255() < 179) { // 70% chance (179/255)
			// Evolve note within a small range (±2 semitones in scale)
			if (scaleNoteCount > 0) {
				int32_t currentNote = stages[stageToChange].noteIndex;
				int32_t noteChange = (getRandom255() % 5) - 2; // -2 to +2
				int32_t newNote = currentNote + noteChange;

				// Wrap around scale
				while (newNote < 0)
					newNote += scaleNoteCount;
				while (newNote >= scaleNoteCount)
					newNote -= scaleNoteCount;

				stages[stageToChange].noteIndex = newNote;
			}
		}
		else {
			// Evolve octave within a small range (±1 octave)
			int32_t currentOctave = stages[stageToChange].octave;
			int32_t octaveChange = (getRandom255() < 128) ? -1 : 1; // 50/50 chance up/down
			int32_t newOctave = currentOctave + octaveChange;

			// Keep within reasonable bounds
			if (newOctave < -2)
				newOctave = -2;
			if (newOctave > 3)
				newOctave = 3;

			stages[stageToChange].octave = newOctave;
		}
	}

	// Show confirmation popup
	display->displayPopup("SEQUENCE EVOLVED");
	uiTimerManager.setTimer(TimerName::DISPLAY, kPopupTimeoutMs);

	// Force UI update
	displayState.needsRefresh = true;
}

bool KeyboardLayoutPulseSeq::evaluateRhythmPattern(int32_t stage, int32_t pulsePosition) {
	StageData& stageData = stages[stage];

	// Generate rhythm pattern based on gate type and pulse count
	switch (stageData.gateType) {
	case GateType::SINGLE:
		// Single hit at the beginning, then rest for remaining pulses
		return (pulsePosition == 0);

	case GateType::MULTIPLE:
		// Multiple hits for the pulse count, then rest
		return (pulsePosition < stageData.pulseCount);

	case GateType::HELD:
		// One hit at the beginning, held for the pulse count duration
		return (pulsePosition == 0);

	case GateType::OFF:
		// No hits - always rest
		return false;

	default:
		return false;
	}
}

int32_t KeyboardLayoutPulseSeq::calculateTotalPatternLength() const {
	int32_t totalLength = 0;
	// Only sum pulse counts for active stages (0 to numStages-1)
	for (int32_t i = 0; i < performanceControls.numStages; i++) {
		totalLength += stages[i].pulseCount;
	}
	return totalLength;
}

void KeyboardLayoutPulseSeq::resetToPatternStart() {
	sequencerState.currentPulse = 0;
	sequencerState.totalPatternLength = calculateTotalPatternLength();
	// Clear stage flash so it doesn't interfere with next playback
	sequencerState.gatePadFlashing = false;
}

void KeyboardLayoutPulseSeq::sendAllNotesOff() {
	// Send all notes off via instrument
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (clip) {
		MelodicInstrument* melodicInstrument = (MelodicInstrument*)clip->output;
		if (melodicInstrument) {
			// Cast to MIDIInstrument to access allNotesOff
			MIDIInstrument* midiInstrument = (MIDIInstrument*)melodicInstrument;
			if (midiInstrument) {
				midiInstrument->allNotesOff();
			}

			// Also clear the arpeggiator's note list to prevent hung notes
			NonAudioInstrument* nonAudioInstrument = (NonAudioInstrument*)melodicInstrument;
			if (nonAudioInstrument) {
				nonAudioInstrument->arpeggiator.reset();
			}
		}
	}
}

} // namespace deluge::gui::ui::keyboard::layout
