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
#include "hid/display/display.h"
#include "util/d_string.h"
#include "io/debug/print.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/instrument/kit.h"
#include "gui/menu_item/value_scaling.h"
#include "model/model_stack.h"
#include "model/scale/note_set.h"
#include "processing/engines/audio_engine.h"
#include "model/scale/preset_scales.h"
#include "playback/playback_handler.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound.h"
#include "model/song/song.h"
#include <climits>
#include <cstdint>
#include <cstring>

namespace deluge::gui::ui::keyboard::layout {

void KeyboardLayoutPulseSeq::evaluatePads(PressedPad presses[kMaxNumKeyboardPadPresses]) {
	currentNotesState = NotesState{}; // Erase active notes

	int32_t gateLineY = getGateLineY();

	for (int32_t idxPress = 0; idxPress < kMaxNumKeyboardPadPresses; ++idxPress) {
		auto pressed = presses[idxPress];
		if (pressed.active && pressed.x < kDisplayWidth) {
			int32_t x = pressed.x;
			int32_t y = pressed.y;

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
		}
	}

	// Should be called last so currentNotesState can be read
	ColumnControlsKeyboard::evaluatePads(presses);
}

void KeyboardLayoutPulseSeq::handleVerticalEncoder(int32_t offset) {
	// Scroll the gate line to reveal pulse count pads below
	int32_t newOffset = displayState.gateLineOffset + offset;
	if (newOffset < 0) newOffset = 0;
	if (newOffset > 3) newOffset = 3; // 0-3 maps to y4-y7

	if (newOffset != displayState.gateLineOffset) {
		displayState.gateLineOffset = newOffset;
		displayState.needsRefresh = true;
	}
}

void KeyboardLayoutPulseSeq::handleHorizontalEncoder(int32_t offset, bool shiftEnabled, PressedPad presses[kMaxNumKeyboardPadPresses],
                                                     bool encoderPressed) {
	if (horizontalEncoderHandledByColumns(offset, shiftEnabled)) {
		return;
	}

	// TODO: Implement horizontal encoder handling for pulse sequence
}

void KeyboardLayoutPulseSeq::precalculate() {
	// Pre-calculate any colors or values needed for rendering
	// For now, we don't need to pre-calculate anything
}

void KeyboardLayoutPulseSeq::updateAnimation() {
	static uint32_t animationCounter = 0;
	animationCounter++;

	// Initialize first stage to SINGLE gate type for testing (only once)
	static bool initialized = false;
	if (!initialized) {
		stages[0].gateType = GateType::SINGLE;
		initialized = true;
	}

	// Check if Deluge playback state changed
	bool isCurrentlyPlaying = isDelugePlaying();

	if (isCurrentlyPlaying != displayState.wasPlaying) {
		displayState.wasPlaying = isCurrentlyPlaying;
		displayState.needsRefresh = true;

		if (!isCurrentlyPlaying) {
			// Deluge stopped - reset sequencer state
			resetSequencerState();
		}
	}

    // Update sequencer if playing
    if (isCurrentlyPlaying) {
        updateSequencer();

        // Use the arpeggiator's timing system
        InstrumentClip* clip = getCurrentInstrumentClip();
        if (clip) {
            ArpeggiatorSettings* arpSettings = &clip->arpSettings;

            // Only run when arpeggiator is OFF (so it doesn't interfere with our sequencer)
            if (arpSettings->mode != ArpMode::OFF) {
                return;
            }

            // Use the arpeggiator's sync-based timing system for accurate 16th notes
            // Use global playback position instead of clip position (which loops)
            // This allows the pulse sequencer to have its own variable pattern length
            int64_t globalTickCount = playbackHandler.getCurrentInternalTickCount();

            // Use syncLevel = 3 for 16th notes (192 ticks per beat)
            // This is the same timing the arpeggiator uses
            uint32_t ticksPerPeriod = 3 << (9 - arpSettings->syncLevel);

            // Check if we're at the start of a new period
            int32_t howFarIntoPeriod = globalTickCount % ticksPerPeriod;

            if (howFarIntoPeriod == 0) {
                StageData& currentStageData = stages[sequencerState.currentStage];

                // Increment pulse counter for this stage
                sequencerState.currentPulseInStage++;
                sequencerState.currentPatternPosition++;

                // Check if we should play a note based on rhythm pattern
                bool shouldPlayNote = evaluateRhythmPattern(sequencerState.currentStage, sequencerState.currentPulseInStage - 1);

                // Handle different gate types
                if (shouldPlayNote) {
                    switch (currentStageData.gateType) {
                        case GateType::SINGLE:
                            // One note on stage entry, wait for next stage if longer
                            if (sequencerState.currentPulseInStage == 1) {
                                generateNote();
                            }
                            break;

                        case GateType::MULTIPLE:
                            // Play the stage note multiple times in time with arp rate
                            generateNote();
                            break;

                        case GateType::HELD:
                            // One sustained note for duration of the stage
                            if (sequencerState.currentPulseInStage == 1) {
                                generateNote();
                            }
                            break;

                        case GateType::OFF:
                            // No note, just like a rest
                            break;
                    }
                }

                // Check if we've completed the required number of pulses for this stage
                if (sequencerState.currentPulseInStage >= currentStageData.pulseCount) {
                    advanceStage();
                }

                // Check if we've completed the entire pattern (all stages)
                if (sequencerState.currentPatternPosition >= sequencerState.totalPatternLength) {
                    resetToPatternStart();
                }

                // Force display refresh to show pad changes
                keyboardScreen.requestMainPadsRendering();
            }
        }
    }

	// Force display refresh if needed
	if (displayState.needsRefresh) {
		keyboardScreen.requestMainPadsRendering();
		if (display->haveOLED()) {
			renderUIsForOled();
		}
		displayState.needsRefresh = false;
	}
}

// OLED display helpers
void KeyboardLayoutPulseSeq::displayGateTypePopup(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	const char* gateTypeName = getGateTypeName(stages[stage].gateType);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "GATE %d: %s", stage + 1, gateTypeName);

	display->displayPopup(buffer);

	// Force UI update
	if (display->haveOLED()) {
		renderUIsForOled();
	}
}

void KeyboardLayoutPulseSeq::displayNotePopup(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	const char* noteName = getNoteName(stages[stage].noteIndex, stages[stage].octave);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "NOTE %d: %s", stage + 1, noteName);

	display->displayPopup(buffer);

	// Force UI update
	if (display->haveOLED()) {
		renderUIsForOled();
	}
}

void KeyboardLayoutPulseSeq::displayOctavePopup(int32_t stage, int32_t direction) {
	if (stage < 0 || stage >= 8) return;

	const char* noteName = getNoteName(stages[stage].noteIndex, stages[stage].octave);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "NOTE %d: %s", stage + 1, noteName);

	display->displayPopup(buffer);

	// Force UI update
	if (display->haveOLED()) {
		renderUIsForOled();
	}
}

void KeyboardLayoutPulseSeq::displayPulseCountPopup(int32_t stage) {
	if (stage < 0 || stage >= 8) return;

	char buffer[32];
	snprintf(buffer, sizeof(buffer), "STAGE %d: %d PULSES", stage + 1, stages[stage].pulseCount);

	display->displayPopup(buffer);

	// Force UI update
	if (display->haveOLED()) {
		renderUIsForOled();
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
void KeyboardLayoutPulseSeq::updateSequencer() {
	if (!isDelugePlaying()) {
		return;
	}

	// Get current stage data
	StageData& currentStageData = stages[sequencerState.currentStage];

	// Check if current stage is complete
	bool stageComplete = false;

	switch (currentStageData.gateType) {
		case GateType::OFF:
			// Rest/hold for pulse count duration
			stageComplete = (sequencerState.currentPulseInStage >= currentStageData.pulseCount);
			break;
		case GateType::SINGLE:
			stageComplete = (sequencerState.currentPulseInStage >= 1);
			break;
		case GateType::MULTIPLE:
			stageComplete = (sequencerState.currentPulseInStage >= currentStageData.pulseCount);
			break;
		case GateType::HELD:
			stageComplete = (sequencerState.currentPulseInStage >= 1);
			break;
	}

	if (stageComplete) {
		advanceStage();
	} else {
		// Generate note for current pulse (if not OFF gate)
		if (currentStageData.gateType != GateType::OFF) {
			generateNote();

			// Trigger gate pad flash for visual feedback
			triggerGatePadFlash(sequencerState.currentStage, sequencerState.currentPulseInStage);
		}

		// Update visual feedback
		updateVisualFeedback();

		// Increment pulse counter
		sequencerState.currentPulseInStage++;
	}
}

void KeyboardLayoutPulseSeq::resetSequencerState() {
	sequencerState.isPlaying = false;
	sequencerState.currentStage = 0;        // Stage 1
	sequencerState.currentPulseInStage = 0; // First pulse
	sequencerState.stageStartTime = 0;
	sequencerState.gateCurrentlyActive = false;
	sequencerState.gatePos = 0;

	// Reset pattern state
	sequencerState.totalPatternLength = calculateTotalPatternLength();
	sequencerState.currentPatternPosition = 0;

	// Reset visual feedback state
	sequencerState.gatePadFlashing = false;
	sequencerState.flashStartTime = 0;
	sequencerState.flashPosition = 0;
}

void KeyboardLayoutPulseSeq::advanceStage() {
	sequencerState.currentStage++;
	if (sequencerState.currentStage >= performanceControls.numStages) {
		sequencerState.currentStage = 0; // Loop back to stage 1
	}
	sequencerState.currentPulseInStage = 0; // Reset pulse counter for new stage
	sequencerState.stageStartTime = 0; // Reset stage start time
}

bool KeyboardLayoutPulseSeq::isDelugePlaying() const {
	// Check if Deluge is currently playing
	return playbackHandler.playbackState != 0;
}

void KeyboardLayoutPulseSeq::generateNote() {
    // Get current stage data
    StageData& currentStageData = stages[sequencerState.currentStage];

    // Only generate notes for non-OFF gate types
    if (currentStageData.gateType == GateType::OFF) {
        return; // No note generation for rest
    }

    // Generate the note using the instrument's noteOn method
    InstrumentClip* clip = getCurrentInstrumentClip();
    if (!clip || !clip->output) {
        return;
    }

    // Calculate note from scale
    NoteSet& scaleNotes = getScaleNotes();
    uint8_t scaleNoteCount = getScaleNoteCount();
    
    // Get note from stage's noteIndex and octave
    int32_t note = getRootNote() + scaleNotes[currentStageData.noteIndex % scaleNoteCount] 
                   + (currentStageData.octave * kOctaveSize);

    // Clamp note to valid range
    if (note < 0) note = 0;
    if (note > 127) note = 127;

    // Get default velocity and apply velocity spread like arpeggiator does
    uint8_t baseVelocity = getDefaultVelocity();

    // Apply velocity spread from arp settings
    if (!clip) {
        return;
    }

    ArpeggiatorSettings* arpSettings = &clip->arpSettings;

    // Calculate velocity spread for current step (simplified version)
    int32_t spreadVelocityForCurrentStep = 0;
    if (arpSettings->spreadVelocity != 0) {
        // Simple random spread calculation (simplified from arpeggiator)
        uint32_t randomValue = getRandom255();
        if (randomValue < (arpSettings->spreadVelocity >> 1)) {
            spreadVelocityForCurrentStep = (randomValue % (arpSettings->spreadVelocity >> 1)) + 1;
        }
        else if (randomValue < arpSettings->spreadVelocity) {
            spreadVelocityForCurrentStep = -((randomValue % (arpSettings->spreadVelocity >> 1)) + 1);
        }
    }

    // Apply velocity spread calculation (same as arpeggiator)
    uint8_t velocity = baseVelocity;
    if (spreadVelocityForCurrentStep != 0) {
        int32_t signedVelocity = (int32_t)velocity;
        int32_t diff = 0;
        if (spreadVelocityForCurrentStep < 0) {
            // Reducing velocity
            diff = -(q31_mult((-spreadVelocityForCurrentStep) << 24, signedVelocity - 1));
        }
        else {
            // Increasing velocity
            diff = (q31_mult(spreadVelocityForCurrentStep << 24, 127 - signedVelocity));
        }
        signedVelocity = signedVelocity + diff;

        // Clamp to valid range
        if (signedVelocity < 1) {
            signedVelocity = 1;
        }
        else if (signedVelocity > 127) {
            signedVelocity = 127;
        }
        velocity = (uint8_t)signedVelocity;
    }

    // Create a simple note-on event
    MelodicInstrument* melodicInstrument = (MelodicInstrument*)clip->output;
    if (melodicInstrument) {
        // Create a ModelStack for the note-on event using soundEditor
        if (soundEditor.setup(clip, nullptr, 0)) {
            char modelStackMemory[MODEL_STACK_MAX_SIZE];
            ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);

            if (modelStack) {
                // Calculate gate length from arp settings
                uint32_t gateLength = computeFinalValueForStandardMenuItem(arpSettings->gate);

                // Trigger a note-on event with gate length
                melodicInstrument->sendNote(modelStack, true, note, nullptr, MIDI_CHANNEL_NONE, velocity, gateLength);
            }
        }
    }
}

void KeyboardLayoutPulseSeq::updateVisualFeedback() {
	// Update visual feedback for the current stage and pulse
	// Check if flash duration has expired
	if (sequencerState.gatePadFlashing) {
		uint32_t currentTime = AudioEngine::audioSampleTimer;
		if (currentTime - sequencerState.flashStartTime >= sequencerState.flashDuration) {
			sequencerState.gatePadFlashing = false;
		}
	}

	// Mark that we need a refresh
	displayState.needsRefresh = true;
}

void KeyboardLayoutPulseSeq::triggerGatePadFlash(int32_t stage, int32_t pulsePosition) {
	sequencerState.gatePadFlashing = true;
	sequencerState.flashStartTime = AudioEngine::audioSampleTimer;
	sequencerState.flashPosition = pulsePosition;
	displayState.needsRefresh = true;
}

bool KeyboardLayoutPulseSeq::isGatePadFlashing() const {
	return sequencerState.gatePadFlashing;
}

void KeyboardLayoutPulseSeq::updateDisplay() {
	// Force pad LED refresh using the correct method
	keyboardScreen.requestMainPadsRendering();
	if (display->haveOLED()) {
		renderUIsForOled();
	}
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
		image[gateLineY][x] = getGateTypeColor(x);
	}

	// Render note selection pads (above gate line) - 8 columns only
	for (int32_t x = 0; x < 8; x++) {
		if (gateLineY + 1 < kDisplayHeight) {
			image[gateLineY + 1][x] = getNoteSelectionColor(x);
		}
	}

	// Render octave controls (above note selection) - 8 columns only
	for (int32_t x = 0; x < 8; x++) {
		if (gateLineY + 2 < kDisplayHeight) {
			image[gateLineY + 2][x] = getOctaveControlColor(); // Octave down
		}
		if (gateLineY + 3 < kDisplayHeight) {
			image[gateLineY + 3][x] = getOctaveControlColor(); // Octave up
		}
	}

	// Rhythm pattern selection removed - patterns are auto-generated from gate type and pulse count

	// Render pulse count display (below gate line, 7 pads with gradient on all 8 columns)
	for (int32_t i = 0; i < 7; i++) {
		if (gateLineY - 1 - i >= 0) {
			for (int32_t x = 0; x < 8; x++) {
				image[gateLineY - 1 - i][x] = getPulseCountColor(x, i);
			}
		}
	}
}

ArpeggiatorSettings* KeyboardLayoutPulseSeq::getArpSettings() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip) return nullptr;
	return &clip->arpSettings;
}

Arpeggiator* KeyboardLayoutPulseSeq::getArpeggiator() {
	// TODO: Implement arpeggiator access if needed
	return nullptr;
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

RGB KeyboardLayoutPulseSeq::getGateTypeColor(int32_t stage) const {
	if (stage < 0 || stage >= 8) return RGB{0, 0, 0}; // Gate line only on first 8 columns

	// Check if this stage is currently flashing
	if (isGatePadFlashing() && sequencerState.currentStage == stage) {
		// Flash white during note trigger
		return RGB{255, 255, 255}; // White flash
	}

	// Normal gate type colors
	switch (stages[stage].gateType) {
		case GateType::OFF:
			return RGB{255, 255, 255}; // White
		case GateType::SINGLE:
			return RGB{0, 255, 0}; // Green
		case GateType::MULTIPLE:
			return RGB{0, 0, 255}; // Blue
		case GateType::HELD:
			return RGB{255, 0, 255}; // Magenta
		default:
			return RGB{0, 0, 0}; // Black
	}
}

RGB KeyboardLayoutPulseSeq::getNoteSelectionColor(int32_t stage) const {
	if (stage < 0 || stage >= 8) return RGB{0, 0, 0};

	// Pink color for note selection
	return RGB{255, 100, 150};
}

RGB KeyboardLayoutPulseSeq::getOctaveControlColor() const {
	// Light blue for octave controls
	return RGB{100, 150, 255};
}

RGB KeyboardLayoutPulseSeq::getPulseCountColor(int32_t stage, int32_t position) const {
	if (stage < 0 || stage >= 8) return RGB{0, 0, 0};
	if (position < 0 || position >= 7) return RGB{0, 0, 0};

	// Check if this position should be lit based on the stage's pulse count
	if (position < stages[stage].pulseCount) {
		// Active pulse positions - cyan to purple gradient
		// Position 0 (top) = cyan, position 6 (bottom) = purple
		int32_t intensity = (position * 255) / 6; // 0-255
		uint8_t red = static_cast<uint8_t>(intensity);
		uint8_t green = static_cast<uint8_t>(255 - intensity);
		uint8_t blue = 255;
		return RGB{red, green, blue};
	} else {
		// Inactive pulse positions - black
		return RGB{0, 0, 0};
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
