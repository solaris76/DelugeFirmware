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
#include "gui/ui/keyboard/keyboard_screen.h"
#include "gui/colour/colour.h"
#include "model/instrument/non_audio_instrument.h"
#include "hid/display/display.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/model_stack.h"
#include "model/scale/note_set.h"
#include "playback/playback_handler.h"
#include "model/song/song.h"
#include "gui/ui_timer_manager.h"
#include "gui/menu_item/value_scaling.h"
#include "gui/ui/sound_editor.h"

namespace deluge::gui::ui::keyboard::layout {

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
			// y5: Gate control (note length)
			else if (y == 5 && x >= 8 && x < kDisplayWidth) {
				handleGateControl(x - 8); // 0-7
			}
			// y3: Play order presets
			else if (y == 3 && x >= 8 && x < 12) {
				handlePlayOrderChange(x - 8); // 0-3
			}
			// y7: Transpose and octave controls
			else if (y == 7 && x >= 12 && x < kDisplayWidth) {
				if (x == 12) {
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
		if (newAccumulator < -7) newAccumulator = -7;
		if (newAccumulator > 7) newAccumulator = 7;

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
				uiTimerManager.setTimer(TimerName::DISPLAY, 2000);
			}

			displayState.needsRefresh = true;
		}
	}
	else {
		// No pad held - scroll the gate line to reveal pulse count pads below
		int32_t newOffset = displayState.gateLineOffset + offset;
		if (newOffset < 0) newOffset = 0;
		if (newOffset > 3) newOffset = 3; // 0-3 maps to y4-y7

		if (newOffset != displayState.gateLineOffset) {
			displayState.gateLineOffset = newOffset;
			displayState.needsRefresh = true;
		}
	}
}

void KeyboardLayoutPulseSeq::handleHorizontalEncoder(int32_t offset, bool shiftEnabled, PressedPad presses[kMaxNumKeyboardPadPresses],
                                                     bool encoderPressed) {
	if (horizontalEncoderHandledByColumns(offset, shiftEnabled)) {
		return;
	}

	// Clock divider control: 1 (32nd) to 32 (whole note)
	// Powers of 2: 1, 2, 4, 8, 16, 32
	int32_t newDivider = performanceControls.clockDivider;

	if (offset > 0) {
		// Increase divider (slower tempo)
		if (newDivider < 32) {
			newDivider *= 2;
		}
	}
	else if (offset < 0) {
		// Decrease divider (faster tempo)
		if (newDivider > 1) {
			newDivider /= 2;
		}
	}

	if (newDivider != performanceControls.clockDivider) {
		performanceControls.clockDivider = newDivider;

		// Show popup with clock divider name
		if (display->haveOLED()) {
			const char* dividerNames[] = {
				"32nd Notes", "16th Notes", "8th Notes", "Quarter Notes", "Half Notes", "Whole Notes"
			};
			// Map divider to array index: 1->0, 2->1, 4->2, 8->3, 16->4, 32->5
			int32_t nameIndex = 0;
			int32_t tempDiv = newDivider;
			while (tempDiv > 1) {
				nameIndex++;
				tempDiv /= 2;
			}

			char text[30];
			strcpy(text, "Clock: ");
			strcat(text, dividerNames[nameIndex]);
			display->displayPopup(text);

			// Set custom 2-second timeout for OLED
			uiTimerManager.setTimer(TimerName::DISPLAY, 2000);
		}

		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::precalculate() {
	// Pre-calculate any colors or values needed for rendering
	// For now, we don't need to pre-calculate anything
}


int32_t KeyboardLayoutPulseSeq::doTickForward(uint32_t clipCurrentPos, bool currentlyPlayingReversed, ArpReturnInstruction* instruction) {
    // Only generate notes if arpeggiator is off
    ArpeggiatorSettings* arpSettings = getArpSettings();
    if (arpSettings && arpSettings->mode != ArpMode::OFF) {
        return 2147483647; // No timing support when arpeggiator is on
    }

    // Reset sequencer state when playback starts (every play press)
    static uint32_t lastPos = 0;
    if (clipCurrentPos < lastPos || clipCurrentPos == 0) {
        resetSequencerState();
    }
    lastPos = clipCurrentPos;

    // Calculate ticks per period based on clock divider
    // Use the same sync level as the arpeggiator for consistency
    uint32_t syncLevel = arpSettings ? arpSettings->syncLevel : 4; // Default to 16th notes if no arp settings
    uint32_t ticksPerPeriod = 3 << (9 - syncLevel);
    ticksPerPeriod *= performanceControls.clockDivider; // Scale by clock divider

    int32_t howFarIntoPeriod = clipCurrentPos % ticksPerPeriod;

    // Check for note-off when gate expires (per-note tracking)
    bool anyNoteActive = false;
    uint32_t gateLength = calculateGateLength();

    for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
        if (sequencerState.noteActive[n]) {
            anyNoteActive = true;
            sequencerState.noteGatePos[n]++;

            if (sequencerState.noteGatePos[n] >= gateLength) {
                // Debug: show when note-off is triggered
                display->displayPopup("NOTE OFF");
                switchNoteOff(instruction, n);
            }
        }
    }

    // Update overall gate state
    sequencerState.gateCurrentlyActive = anyNoteActive;

    if (!howFarIntoPeriod) {

        // Check if we should play a note based on rhythm pattern (BEFORE advancement)
        bool shouldPlayNote = evaluateRhythmPattern(sequencerState.currentStage, sequencerState.currentPulseInStage);

        // Always flash the current stage (for both notes and OFF gates)
        sequencerState.gatePadFlashing = true;
        sequencerState.flashStartTime = playbackHandler.getCurrentInternalTickCount();
        sequencerState.lastPlayedStage = sequencerState.currentStage;
        keyboardScreen.requestMainPadsRendering();

        if (shouldPlayNote) {
            // Generate note for the CURRENT stage (before advancement)
            switchNoteOn(instruction);
            // Debug: show when note is generated
            display->displayPopup("NOTE ON");
        }


        // Advance sequencer state AFTER generating note
        sequencerState.currentPulseInStage++;
        sequencerState.currentPatternPosition++;

        // Check if stage is complete
        StageData& currentStageData = stages[sequencerState.currentStage];
        if (sequencerState.currentPulseInStage >= currentStageData.pulseCount) {
            advanceStage();
        }

        // Check if pattern is complete
        if (sequencerState.currentPatternPosition >= sequencerState.totalPatternLength) {
            resetToPatternStart();
        }
    }
    else {
        if (!currentlyPlayingReversed) {
            howFarIntoPeriod = ticksPerPeriod - howFarIntoPeriod;
        }
    }

    return howFarIntoPeriod;
}

void KeyboardLayoutPulseSeq::switchNoteOn(ArpReturnInstruction* instruction) {
    // Get current stage data
    StageData& currentStageData = stages[sequencerState.currentStage];

    // Only generate notes for non-OFF gate types
    if (currentStageData.gateType == GateType::OFF) {
        return; // No note generation for rest
    }

    // Calculate note from scale
    NoteSet& scaleNotes = getScaleNotes();
    uint8_t scaleNoteCount = getScaleNoteCount();

    // Get note from stage's noteIndex and octave
    // Start from C3 (MIDI note 48) as base octave for better range
    constexpr int32_t kBaseOctave = 48; // C3

    // Calculate base note index with accumulator applied
    int32_t noteIndexWithAccumulator = currentStageData.noteIndex + currentStageData.accumulator;

    // Wrap around the scale if needed
    while (noteIndexWithAccumulator < 0) noteIndexWithAccumulator += scaleNoteCount;
    while (noteIndexWithAccumulator >= scaleNoteCount) noteIndexWithAccumulator -= scaleNoteCount;

    int32_t note = kBaseOctave + getRootNote() + scaleNotes[noteIndexWithAccumulator]
                   + (currentStageData.octave * kOctaveSize)
                   + performanceControls.transpose  // Apply transpose (within scale)
                   + (performanceControls.octave * kOctaveSize); // Apply global octave shift

    // Clamp note to valid range
    if (note < 0) note = 0;
    if (note > 127) note = 127;

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
                nonAudioInstrument->arpeggiator.noteOn(arpSettings, note, velocity, instruction, MIDI_CHANNEL_NONE, nullptr);
            }
        }
    }

    // Find an available slot for this note (for MULTIPLE gate type)
    int32_t noteSlot = -1;
    for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
        if (!sequencerState.noteActive[n]) {
            noteSlot = n;
            break;
        }
    }

    // If no slot available, find the oldest note to replace
    if (noteSlot == -1) {
        // Find the note with the highest gate position (oldest)
        uint32_t maxGatePos = 0;
        for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
            if (sequencerState.noteGatePos[n] > maxGatePos) {
                maxGatePos = sequencerState.noteGatePos[n];
                noteSlot = n;
            }
        }
        // If still no slot found, use slot 0
        if (noteSlot == -1) {
            noteSlot = 0;
        }
    }

    // Store note for note-off tracking
    sequencerState.noteCodeCurrentlyOnPostArp[noteSlot] = note;
    sequencerState.outputMIDIChannelForNoteCurrentlyOnPostArp[noteSlot] = MIDI_CHANNEL_NONE;
    sequencerState.noteGatePos[noteSlot] = 0;
    sequencerState.noteActive[noteSlot] = true;
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

uint32_t KeyboardLayoutPulseSeq::calculateGateLength() {
    // Simple gate length calculation based on arp settings
    ArpeggiatorSettings* arpSettings = getArpSettings();
    if (!arpSettings) return 24; // Default 24 ticks

    // Get gate length from arp settings (1-50, where 50 = 100%)
    uint32_t gatePercent = computeFinalValueForStandardMenuItem(arpSettings->gate);

    // Convert to ticks (50 = 24 ticks, 25 = 12 ticks, etc.)
    uint32_t gateLength = (gatePercent * 24) / 50;

    // Ensure minimum gate length
    if (gateLength < 5) {
        gateLength = 5;
    }

    return gateLength;
}

// OLED display helpers
void KeyboardLayoutPulseSeq::displayGateTypePopup(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	const char* gateTypeName = getGateTypeName(stages[stage].gateType);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "GATE %d: %s", stage + 1, gateTypeName);

	display->displayPopup(buffer);

	// Set custom 2-second timeout for OLED
	if (display->haveOLED()) {
		uiTimerManager.setTimer(TimerName::DISPLAY, 2000);
	}
}

void KeyboardLayoutPulseSeq::displayNotePopup(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	const char* noteName = getNoteName(stages[stage].noteIndex, stages[stage].octave);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "NOTE %d: %s", stage + 1, noteName);

	display->displayPopup(buffer);

	// Set custom 2-second timeout for OLED
	if (display->haveOLED()) {
		uiTimerManager.setTimer(TimerName::DISPLAY, 2000);
	}
}

void KeyboardLayoutPulseSeq::displayOctavePopup(int32_t stage, int32_t direction) {
	if (stage < 0 || stage >= 8) return;

	const char* noteName = getNoteName(stages[stage].noteIndex, stages[stage].octave);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "NOTE %d: %s", stage + 1, noteName);

	display->displayPopup(buffer);

	// Set custom 2-second timeout for OLED
	if (display->haveOLED()) {
		uiTimerManager.setTimer(TimerName::DISPLAY, 2000);
	}
}

void KeyboardLayoutPulseSeq::displayPulseCountPopup(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	char buffer[32];
	snprintf(buffer, sizeof(buffer), "STAGE %d: %d PULSES", stage + 1, stages[stage].pulseCount);

	display->displayPopup(buffer);

	// Set custom 2-second timeout for OLED
	if (display->haveOLED()) {
		uiTimerManager.setTimer(TimerName::DISPLAY, 2000);
	}
}

const char* KeyboardLayoutPulseSeq::getGateTypeName(GateType type) const {
	switch (type) {
		case GateType::OFF: return "OFF";
		case GateType::SINGLE: return "SINGLE";
		case GateType::MULTIPLE: return "MULTIPLE";
		case GateType::HELD: return "HELD";
		default: return "UNKNOWN";
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
	sequencerState.isPlaying = false;
	sequencerState.currentStage = 0;        // Stage 1
	sequencerState.currentPulseInStage = 0; // First pulse
	sequencerState.stageStartTime = 0;
	sequencerState.gateCurrentlyActive = false;
	sequencerState.gatePos = 0;
	sequencerState.lastPlayedStage = -1;

	// Reset pattern state
	sequencerState.totalPatternLength = calculateTotalPatternLength();
	sequencerState.currentPatternPosition = 0;

	// Reset visual feedback state
	sequencerState.gatePadFlashing = false;
	sequencerState.flashStartTime = 0;
	sequencerState.flashPosition = 0;

	// All stages start as OFF by default - user must enable them manually
}

void KeyboardLayoutPulseSeq::advanceStage() {
	// Advance to next stage based on play order
	switch (performanceControls.playOrder) {
		case PlayOrder::FORWARDS:
			sequencerState.currentStage++;
			if (sequencerState.currentStage >= performanceControls.numStages) {
				sequencerState.currentStage = 0; // Loop back to stage 1
			}
			break;

		case PlayOrder::BACKWARDS:
			sequencerState.currentStage--;
			if (sequencerState.currentStage < 0) {
				sequencerState.currentStage = performanceControls.numStages - 1; // Loop to last stage
			}
			break;

		case PlayOrder::PING_PONG:
			// Move in current direction
			sequencerState.currentStage += performanceControls.pingPongDirection;

			// Check boundaries and reverse direction
			if (sequencerState.currentStage >= performanceControls.numStages) {
				sequencerState.currentStage = performanceControls.numStages - 1; // Go to last stage
				performanceControls.pingPongDirection = -1; // Reverse direction
			}
			else if (sequencerState.currentStage < 0) {
				sequencerState.currentStage = 0; // Go to first stage
				performanceControls.pingPongDirection = 1; // Reverse direction
			}
			break;

		case PlayOrder::RANDOM:
			// Generate random stage within active range
			sequencerState.currentStage = (rand() % performanceControls.numStages);
			break;
	}

	// Reset pulse counter for new stage
	sequencerState.currentPulseInStage = 0;
	sequencerState.stageStartTime = 0;
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
			} else {
				// Flash duration expired, stop flashing
				sequencerState.gatePadFlashing = false;
			}
		}

		if (shouldFlash) {
			// Different flash colors for different gate types
			if (stages[x].gateType == GateType::OFF) {
				image[gateLineY][x] = RGB{255, 100, 0}; // Orange flash for OFF gates
			} else {
				image[gateLineY][x] = RGB{255, 0, 0}; // Red flash for active gates
			}
		} else {
			// Normal gate type colors
			switch (stages[x].gateType) {
				case GateType::OFF:
					image[gateLineY][x] = RGB{100, 100, 100}; // Dim white
					break;
				case GateType::SINGLE:
					image[gateLineY][x] = RGB{0, 255, 0}; // Green
					break;
				case GateType::MULTIPLE:
					image[gateLineY][x] = RGB{0, 0, 255}; // Blue
					break;
				case GateType::HELD:
					image[gateLineY][x] = RGB{255, 0, 255}; // Magenta
					break;
				default:
					image[gateLineY][x] = RGB{0, 0, 0}; // Black
					break;
			}
		}
	}

	// Render note selection pads (above gate line) - 8 columns only
	for (int32_t x = 0; x < 8; x++) {
		if (gateLineY + 1 < kDisplayHeight) {
			// Note selection colors - magenta when accumulator is non-zero, pink when zero
			image[gateLineY + 1][x] = (stages[x].accumulator != 0) ? RGB{255, 0, 255} : RGB{255, 100, 150};
		}
	}

	// Render octave controls (above note selection) - 8 columns only
	for (int32_t x = 0; x < 8; x++) {
		if (gateLineY + 2 < kDisplayHeight) {
			image[gateLineY + 2][x] = RGB{100, 150, 255}; // Octave down
		}
		if (gateLineY + 3 < kDisplayHeight) {
			image[gateLineY + 3][x] = RGB{100, 150, 255}; // Octave up
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
					image[gateLineY - 1 - i][x] = RGB{static_cast<uint8_t>(intensity), static_cast<uint8_t>(255 - intensity), 255};
				} else {
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
			image[4][x] = RGB{0, 100, 200}; // Blue (active stages)
		}
		else {
			image[4][x] = RGB{0, 0, 0}; // Black (inactive stages)
		}
	}

	// y5: Gate control (note length) - 8 pads
	for (int32_t x = 8; x < kDisplayWidth; x++) {
		// Gate control colors - light selected and all pads to the left
		int32_t gateIndex = x - 8;
		int32_t selectedGate = (lastTouchedGatePad >= 0) ? lastTouchedGatePad : 3;
		if (gateIndex <= selectedGate || (selectedGate == 0 && gateIndex == 0)) {
			image[5][x] = RGB{0, 255, 0}; // Bright green for selected and all pads to the left
		} else {
			image[5][x] = RGB{0, 0, 0}; // Black for unselected pads
		}
	}

	// y3: Play order presets - 4 pads (x8-11)
	for (int32_t x = 8; x < 12; x++) {
		// Play order colors - highlight selected, dim others
		if (static_cast<int32_t>(performanceControls.playOrder) == (x - 8)) {
			image[3][x] = RGB{0, 255, 255}; // Bright cyan for selected
		} else {
			image[3][x] = RGB{0, 128, 128}; // Dim cyan for others
		}
	}

	// y7: Transpose and octave controls - 4 pads (x12-15)
	// Transpose controls with direction-specific colors
	if (performanceControls.transpose != 0) {
		if (performanceControls.transpose < 0) {
			image[7][12] = RGB{255, 128, 0}; // Orange for active -1
			image[7][13] = RGB{64, 32, 0};   // Dim orange for inactive +1
		} else {
			image[7][12] = RGB{64, 32, 0};   // Dim orange for inactive -1
			image[7][13] = RGB{255, 128, 0}; // Orange for active +1
		}
	} else {
		image[7][12] = RGB{64, 32, 0}; // Dim orange for inactive
		image[7][13] = RGB{64, 32, 0}; // Dim orange for inactive
	}
	// Octave controls with direction-specific colors
	if (performanceControls.octave != 0) {
		if (performanceControls.octave < 0) {
			image[7][14] = RGB{255, 0, 255}; // Magenta for active down
			image[7][15] = RGB{64, 0, 64};   // Dim magenta for inactive up
		} else {
			image[7][14] = RGB{64, 0, 64};   // Dim magenta for inactive down
			image[7][15] = RGB{255, 0, 255}; // Magenta for active up
		}
	} else {
		image[7][14] = RGB{64, 0, 64}; // Dim magenta for inactive
		image[7][15] = RGB{64, 0, 64}; // Dim magenta for inactive
	}

	// Dim gate pads that are beyond the active stage count (x0-x7 only)
	for (int32_t x = 0; x < 8; x++) {
		if (x >= performanceControls.numStages) {
			// Dim only the gate pad for disabled stages
			image[gateLineY][x] = RGB{20, 20, 20}; // Very dim for disabled gate pads
		}
	}
}

ArpeggiatorSettings* KeyboardLayoutPulseSeq::getArpSettings() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip) return nullptr;
	return &clip->arpSettings;
}


int32_t KeyboardLayoutPulseSeq::getGateLineY() const {
	return displayState.gateLineOffset + 4; // y4-y7 (bottom left is y0 x0)
}

void KeyboardLayoutPulseSeq::handleGateType(int32_t stage) {
	if (stage < 0 || stage >= 8) return; // Gate line only on first 8 columns

	// Cycle through gate types: OFF -> SINGLE -> MULTIPLE -> HELD -> OFF
	int32_t currentType = static_cast<int32_t>(stages[stage].gateType);
	int32_t nextType = (currentType + 1) % 4;
	stages[stage].gateType = static_cast<GateType>(nextType);

	displayGateTypePopup(stage);
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handleNoteSelection(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

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
	if (stage < 0 || stage >= 8) return;

	// Adjust octave with reasonable limits
	int32_t newOctave = stages[stage].octave + direction;
	if (newOctave < -2) newOctave = -2;
	if (newOctave > 3) newOctave = 3; // -2 to +3 octaves

	if (newOctave != stages[stage].octave) {
		stages[stage].octave = newOctave;
		displayOctavePopup(stage, direction);
		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleStageCountChange(int32_t numStages) {
	if (numStages < 1) numStages = 1;
	if (numStages > 8) numStages = 8;

	if (performanceControls.numStages != numStages) {
		performanceControls.numStages = numStages;

		// If current stage is beyond new limit, loop back to start
		if (sequencerState.currentStage >= numStages) {
			sequencerState.currentStage = 0;
			sequencerState.currentPulseInStage = 0;
		}

		// Show popup
		if (display->haveOLED()) {
			char text[30];
			strcpy(text, "Stages: ");
			intToString(numStages, text + strlen(text));
			display->popupText(text);
		}

		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleGateControl(int32_t gateIndex) {
	if (gateIndex < 0 || gateIndex >= 8) return;

	// Track the last touched gate pad for LED feedback
	lastTouchedGatePad = gateIndex;

	// Gate values: same as arp_control
	int32_t gateValues[8] = {1, 10, 20, 25, 35, 40, 45, 50};
	int32_t newGate = gateValues[gateIndex];

	// Get arp settings
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip) return;
	ArpeggiatorSettings* settings = &clip->arpSettings;

	// Check output type to determine which approach to use
	OutputType outputType = getCurrentOutputType();

	if (outputType == OutputType::SYNTH) {
		// Use soundEditor.setup() for synth tracks
		UI* originalUI = getCurrentUI();

		if (soundEditor.setup(clip, nullptr, 0)) {
			char modelStackMemory[MODEL_STACK_MAX_SIZE];
			ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);
			ModelStackWithAutoParam* modelStackWithParam = modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_ARP_GATE);

			if (modelStackWithParam && modelStackWithParam->autoParam) {
				int32_t finalValue = computeFinalValueForStandardMenuItem(newGate);
				modelStackWithParam->autoParam->setCurrentValueInResponseToUserInput(finalValue, modelStackWithParam);
			}

			originalUI->focusRegained();
		}
	}
	else {
		// Use direct parameter setting for CV/MIDI tracks
		int32_t scaledValue = computeFinalValueForStandardMenuItem(newGate);
		settings->gate = scaledValue;
	}

	// Show popup
	if (display->haveOLED()) {
		char text[30];
		strcpy(text, "Gate: ");
		intToString(newGate, text + strlen(text));
		display->popupText(text);
	}

	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handlePlayOrderChange(int32_t playOrderIndex) {
	if (playOrderIndex < 0 || playOrderIndex > 3) return;

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
		}

		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleTransposeChange(int32_t direction) {
	// Transpose affects all notes by moving within the scale
	// direction: -1 for down, +1 for up
	performanceControls.transpose += direction;

	// Keep transpose within reasonable bounds (-12 to +12)
	if (performanceControls.transpose < -12) performanceControls.transpose = -12;
	if (performanceControls.transpose > 12) performanceControls.transpose = 12;

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
		uiTimerManager.setTimer(TimerName::DISPLAY, 2000);
	}

	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handleOctaveChange(int32_t direction) {
	// Octave affects all notes by shifting octaves
	// direction: -1 for down, +1 for up
	performanceControls.octave += direction;

	// Keep octave within reasonable bounds (-3 to +3)
	if (performanceControls.octave < -3) performanceControls.octave = -3;
	if (performanceControls.octave > 3) performanceControls.octave = 3;

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
		uiTimerManager.setTimer(TimerName::DISPLAY, 2000);
	}

	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::handlePulseCount(int32_t stage, int32_t position) {
	if (stage < 0 || stage >= 8) return;
	if (position < 0 || position >= 7) return;

	// Set pulse count to position + 1 (position 0 = pulse count 1, position 6 = pulse count 7)
	int32_t newPulseCount = position + 1;

	if (newPulseCount != stages[stage].pulseCount) {
		stages[stage].pulseCount = newPulseCount;

		// Recalculate total pattern length when pulse counts change
		sequencerState.totalPatternLength = calculateTotalPatternLength();

		displayPulseCountPopup(stage);
		displayState.needsRefresh = true;
	}
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
	for (int32_t i = 0; i < 8; i++) {
		totalLength += stages[i].pulseCount;
	}
	return totalLength;
}

void KeyboardLayoutPulseSeq::resetToPatternStart() {
	sequencerState.currentStage = 0;
	sequencerState.currentPulseInStage = 0;
	sequencerState.currentPatternPosition = 0;
	sequencerState.totalPatternLength = calculateTotalPatternLength();
}


} // namespace deluge::gui::ui::keyboard::layout
