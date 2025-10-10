/*
 * Copyright © 2024 Synthstrom Audible Deluge Firmware.
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

#include "model/clip/sequencer/modes/pulse_sequencer_mode.h"
#include "model/clip/sequencer/sequencer_mode_manager.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include "util/functions.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/display/display.h"
#include <algorithm>
#include <cstdio>

// Forward declaration for note name conversion
extern void noteCodeToString(int32_t noteCode, char* buffer, int32_t* getLengthWithoutDot, bool appendOctaveNo);

namespace deluge::model::clip::sequencer::modes {

void PulseSequencerMode::initialize() {
	initialized_ = true;
	ticksPerSixteenthNote_ = 0;

	// Initialize all stages with a simple test pattern
	for (int32_t i = 0; i < kMaxStages; i++) {
		stages_[i].gateType = (i < 4) ? GateType::SINGLE : GateType::OFF; // First 4 stages active
		stages_[i].noteIndex = i; // Different note for each stage
		stages_[i].octave = 0;
		stages_[i].pulseCount = 1;
		stages_[i].velocitySpread = 0;
		stages_[i].probability = 100;
		stages_[i].gateLength = 50;
	}

	// Initialize sequencer state
	sequencerState_.currentPulse = 0;
	sequencerState_.lastPlayedStage = -1;
	sequencerState_.totalPatternLength = calculateTotalPatternLength();
	sequencerState_.gatePadFlashing = false;

	for (int32_t i = 0; i < 16; i++) {
		sequencerState_.noteCodeActive[i] = -1;
		sequencerState_.noteGatePos[i] = 0;
		sequencerState_.noteActive[i] = false;
		sequencerState_.noteSourceStage[i] = -1;
	}

	// Initialize performance controls
	performanceControls_.transpose = 0;
	performanceControls_.octave = 0;
	performanceControls_.clockDivider = 1;  // Default: 16th notes (1=16th, 2=8th, 4=quarter)
	performanceControls_.numStages = 8;
	performanceControls_.playOrder = PlayOrder::FORWARDS;
	performanceControls_.currentStage = 0;
}

void PulseSequencerMode::cleanup() {
	// Send all notes off before cleanup
	for (int32_t n = 0; n < 16; n++) {
		if (sequencerState_.noteActive[n] && sequencerState_.noteCodeActive[n] >= 0) {
			// We don't have modelStack here, so just mark as inactive
			// The notes will be stopped when mode switches
			sequencerState_.noteActive[n] = false;
			sequencerState_.noteCodeActive[n] = -1;
		}
	}
	initialized_ = false;
}

bool PulseSequencerMode::renderPads(uint32_t whichRows, RGB* image, uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
                                   int32_t xScroll, uint32_t xZoom, int32_t renderWidth, int32_t imageWidth) {
	// Clear all pads first
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		if (whichRows & (1 << y)) {
			for (int32_t x = 0; x < renderWidth; x++) {
				image[y * imageWidth + x] = {0, 0, 0};
				if (occupancyMask) {
					occupancyMask[y][x] = 0;
				}
			}
		}
	}

	int32_t gateLineY = getGateLineY();

	// Render gate line (x0-x7) - bottom left is y0 x0
	if (whichRows & (1 << gateLineY)) {
		for (int32_t x = 0; x < 8; x++) {
			bool shouldFlash = false;
			if (sequencerState_.gatePadFlashing && sequencerState_.lastPlayedStage == x) {
				uint32_t currentTime = playbackHandler.getCurrentInternalTickCount();
				uint32_t flashElapsed = currentTime - sequencerState_.flashStartTime;
				if (flashElapsed < sequencerState_.flashDuration) {
					shouldFlash = true;
				}
				else {
					sequencerState_.gatePadFlashing = false;
				}
			}

			RGB color;
			if (shouldFlash) {
				color = (stages_[x].gateType == GateType::OFF) ? RGB{255, 100, 0} : RGB{255, 0, 0};
			}
			else {
				switch (stages_[x].gateType) {
				case GateType::OFF:
					color = RGB{100, 100, 100};
					break;
				case GateType::SINGLE:
					color = RGB{0, 255, 0};
					break;
				case GateType::MULTIPLE:
					color = RGB{0, 0, 255};
					break;
				case GateType::HELD:
					color = RGB{255, 0, 255};
					break;
				}

				// Dim if stage is disabled or beyond active stage count
				if (!performanceControls_.stageEnabled[x] || x >= performanceControls_.numStages) {
					color.r /= 8;
					color.g /= 8;
					color.b /= 8;
				}
			}

			image[gateLineY * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[gateLineY][x] = 64;
			}
		}
	}

	// Render note selection pads (above gate line) - 8 columns only
	if (gateLineY + 1 < kDisplayHeight && (whichRows & (1 << (gateLineY + 1)))) {
		for (int32_t x = 0; x < 8; x++) {
			RGB color = RGB{255, 100, 150};
			if (!performanceControls_.stageEnabled[x] || x >= performanceControls_.numStages) {
				color.r /= 8;
				color.g /= 8;
				color.b /= 8;
			}
			image[(gateLineY + 1) * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[gateLineY + 1][x] = 48;
			}
		}
	}

	// Render octave controls (above note selection) - 8 columns only
	for (int32_t offset = 2; offset <= 3; offset++) {
		if (gateLineY + offset < kDisplayHeight && (whichRows & (1 << (gateLineY + offset)))) {
			for (int32_t x = 0; x < 8; x++) {
				RGB color = RGB{100, 150, 255};
				if (!performanceControls_.stageEnabled[x] || x >= performanceControls_.numStages) {
					color.r /= 8;
					color.g /= 8;
					color.b /= 8;
				}
				image[(gateLineY + offset) * imageWidth + x] = color;
				if (occupancyMask) {
					occupancyMask[gateLineY + offset][x] = 32;
				}
			}
		}
	}

	// Render pulse count display (below gate line) - 8 pads with gradient
	for (int32_t i = 0; i < 8; i++) {
		int32_t yPos = gateLineY - 1 - i;
		if (yPos >= 0 && (whichRows & (1 << yPos))) {
			for (int32_t x = 0; x < 8; x++) {
				if (i < stages_[x].pulseCount) {
					// Active pulse positions - cyan to purple/pink gradient
					int32_t intensity = (i * 255) / 7;
					RGB color = RGB{static_cast<uint8_t>(intensity), static_cast<uint8_t>(255 - intensity), 255};

					if (!performanceControls_.stageEnabled[x] || x >= performanceControls_.numStages) {
						color.r /= 8;
						color.g /= 8;
						color.b /= 8;
					}

					image[yPos * imageWidth + x] = color;
					if (occupancyMask) {
						occupancyMask[yPos][x] = 32 + (i * 4);
					}
				}
			}
		}
	}

	// Render performance controls on right side (x8-15)
	// y4: Stage count control (1-8 stages)
	if (whichRows & (1 << 4)) {
		for (int32_t x = 8; x < kDisplayWidth; x++) {
			int32_t stageNum = x - 7;
			RGB color = (stageNum <= performanceControls_.numStages) ? RGB{255, 255, 0} : RGB{0, 0, 0};
			image[4 * imageWidth + x] = color;
			if (occupancyMask && stageNum <= performanceControls_.numStages) {
				occupancyMask[4][x] = 64;
			}
		}
	}

	// y3: Stage enable/disable toggle (x8-15)
	if (whichRows & (1 << 3)) {
		for (int32_t x = 8; x < kDisplayWidth; x++) {
			int32_t stageIndex = x - 8;
			RGB color = performanceControls_.stageEnabled[stageIndex] ? RGB{255, 128, 0} : RGB{0, 0, 0};
			image[3 * imageWidth + x] = color;
			if (occupancyMask && performanceControls_.stageEnabled[stageIndex]) {
				occupancyMask[3][x] = 48;
			}
		}
	}

	// y1: Play order presets (x8-15)
	if (whichRows & (1 << 1)) {
		for (int32_t x = 8; x < 16; x++) {
			RGB color = (static_cast<int32_t>(performanceControls_.playOrder) == (x - 8))
			    ? RGB{0, 255, 255} : RGB{0, 128, 128};
			image[1 * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[1][x] = (static_cast<int32_t>(performanceControls_.playOrder) == (x - 8)) ? 64 : 32;
			}
		}
	}

	// y5: Velocity spread per stage (x8-15) - Cyan gradient
	if (whichRows & (1 << 5)) {
		for (int32_t x = 8; x < 16; x++) {
			int32_t stage = x - 8;
			int32_t spread = stages_[stage].velocitySpread;
			int32_t intensity = (spread * 255) / 127;  // 0-127 -> 0-255
			int32_t g = (intensity > 32) ? intensity : 32;
			int32_t b = (intensity > 32) ? intensity : 32;
			RGB color = RGB{0, static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
			image[5 * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[5][x] = 32;
			}
		}
	}

	// y6: Probability per stage (x8-15) - Magenta gradient
	if (whichRows & (1 << 6)) {
		for (int32_t x = 8; x < 16; x++) {
			int32_t stage = x - 8;
			int32_t prob = stages_[stage].probability;
			int32_t intensity = (prob * 255) / 100;  // 0-100 -> 0-255
			int32_t val = (intensity > 32) ? intensity : 32;
			RGB color = RGB{static_cast<uint8_t>(val), 0, static_cast<uint8_t>(val)};
			image[6 * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[6][x] = 32;
			}
		}
	}

	// y2: Gate length per stage (x8-15) - GREEN gradient
	if (whichRows & (1 << 2)) {
		for (int32_t x = 8; x < 16; x++) {
			int32_t stage = x - 8;
			int32_t gateLen = stages_[stage].gateLength;
			int32_t intensity = (gateLen * 255) / 100;  // 0-100 -> 0-255
			int32_t g = (intensity > 32) ? intensity : 32;
			RGB color = RGB{0, static_cast<uint8_t>(g), 0};
			image[2 * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[2][x] = 32;
			}
		}
	}

	// y5: Octave down controls for stages (x0-7 only)
	if (whichRows & (1 << 5)) {
		for (int32_t x = 0; x < 8; x++) {
			bool isActive = (stages_[x].octave < 0);
			RGB color = isActive ? RGB{100, 150, 255} : RGB{30, 50, 90};

			if (!performanceControls_.stageEnabled[x] || x >= performanceControls_.numStages) {
				color.r /= 8;
				color.g /= 8;
				color.b /= 8;
			}

			image[5 * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[5][x] = isActive ? 48 : 24;
			}
		}
	}

	// y6: Octave up controls for stages (x0-7 only)
	if (whichRows & (1 << 6)) {
		for (int32_t x = 0; x < 8; x++) {
			bool isActive = (stages_[x].octave > 0);
			RGB color = isActive ? RGB{255, 128, 0} : RGB{90, 50, 30};

			if (!performanceControls_.stageEnabled[x] || x >= performanceControls_.numStages) {
				color.r /= 8;
				color.g /= 8;
				color.b /= 8;
			}

			image[6 * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[6][x] = isActive ? 48 : 24;
			}
		}
	}

	// y7: Note selection toggle for stages (x0-7) + Control buttons (x8-15)
	if (whichRows & (1 << 7)) {
		// Note selection pads for each stage (x0-7)
		for (int32_t x = 0; x < 8; x++) {
			RGB color = RGB{255, 100, 150}; // Pink for note selection

			if (!performanceControls_.stageEnabled[x] || x >= performanceControls_.numStages) {
				color.r /= 8;
				color.g /= 8;
				color.b /= 8;
			}

			image[7 * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[7][x] = 48;
			}
		}

		// Control buttons (x8-15)
		image[7 * imageWidth + 8] = RGB{128, 0, 255};  // Purple reset all
		image[7 * imageWidth + 9] = RGB{255, 0, 128};  // Magenta randomize
		image[7 * imageWidth + 10] = RGB{0, 255, 255}; // Cyan evolve
		image[7 * imageWidth + 11] = RGB{0, 100, 255}; // Blue reset performance controls

		// Transpose controls
		if (performanceControls_.transpose != 0) {
			image[7 * imageWidth + 12] = (performanceControls_.transpose < 0) ? RGB{255, 128, 0} : RGB{64, 32, 0};
			image[7 * imageWidth + 13] = (performanceControls_.transpose > 0) ? RGB{255, 128, 0} : RGB{64, 32, 0};
		}
		else {
			image[7 * imageWidth + 12] = RGB{64, 32, 0};
			image[7 * imageWidth + 13] = RGB{64, 32, 0};
		}

		// Octave controls
		if (performanceControls_.octave != 0) {
			image[7 * imageWidth + 14] = (performanceControls_.octave < 0) ? RGB{255, 0, 255} : RGB{64, 0, 64};
			image[7 * imageWidth + 15] = (performanceControls_.octave > 0) ? RGB{255, 0, 255} : RGB{64, 0, 64};
		}
		else {
			image[7 * imageWidth + 14] = RGB{64, 0, 64};
			image[7 * imageWidth + 15] = RGB{64, 0, 64};
		}

		if (occupancyMask) {
			for (int32_t x = 8; x < 16; x++) {
				occupancyMask[7][x] = 48;
			}
		}
	}

	// After rendering all pads, brighten the playback position pad
	// Calculate which pad the playback position is on
	if (ticksPerSixteenthNote_ > 0 && lastAbsolutePlaybackPos_ >= 0) {
		int32_t totalLengthTicks = ticksPerSixteenthNote_ * performanceControls_.clockDivider * sequencerState_.totalPatternLength;
		if (totalLengthTicks > 0) {
			int32_t positionInPattern = lastAbsolutePlaybackPos_ % totalLengthTicks;
			int32_t currentStage = 0;
			int32_t ticksAccumulated = 0;

			// Find which stage we're in
			for (int32_t s = 0; s < performanceControls_.numStages; s++) {
				int32_t stageTicks = ticksPerSixteenthNote_ * performanceControls_.clockDivider * stages_[performanceControls_.currentStage].pulseCount;
				if (positionInPattern < ticksAccumulated + stageTicks) {
					currentStage = s;
					break;
				}
				ticksAccumulated += stageTicks;
			}

			// Brighten the gate pad for the current stage
			int32_t gateLineY = getGateLineY();
			if ((whichRows & (1 << gateLineY)) && currentStage < 8) {
				RGB& gatePad = image[gateLineY * imageWidth + currentStage];
				// Brighten by adding white
				gatePad.r = std::min(255, gatePad.r + 100);
				gatePad.g = std::min(255, gatePad.g + 100);
				gatePad.b = std::min(255, gatePad.b + 100);
				if (occupancyMask) {
					occupancyMask[gateLineY][currentStage] = 64;
				}
			}
		}
	}

	return true;
}

bool PulseSequencerMode::renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                                       uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	// Blank out the sidebar - no mute/audition pads in sequencer mode
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		if (whichRows & (1 << y)) {
			image[y][kDisplayWidth] = RGB{0, 0, 0};      // x16 - blank
			image[y][kDisplayWidth + 1] = RGB{0, 0, 0};  // x17 - blank
			if (occupancyMask) {
				occupancyMask[y][kDisplayWidth] = 0;
				occupancyMask[y][kDisplayWidth + 1] = 0;
			}
		}
	}

	return true; // We handled it
}

int32_t PulseSequencerMode::processPlayback(void* modelStackPtr, int32_t absolutePlaybackPos) {
	if (!initialized_) {
		return 2147483647;
	}

	ModelStackWithTimelineCounter* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);
	InstrumentClip* clip = static_cast<InstrumentClip*>(modelStack->getTimelineCounter());

	// Only work with melodic instruments
	if (clip->output->type != OutputType::SYNTH && clip->output->type != OutputType::MIDI_OUT
	    && clip->output->type != OutputType::CV) {
		return 2147483647;
	}

	// Calculate base tick rate (16th notes from song)
	if (ticksPerSixteenthNote_ == 0) {
		ticksPerSixteenthNote_ = modelStack->song->getSixteenthNoteLength();
	}

	// Apply clock divider to timing
	// clockDivider = 1 -> 16th notes (default)
	// clockDivider = 2 -> 8th notes
	// clockDivider = 4 -> quarter notes
	int32_t ticksPerPeriod = ticksPerSixteenthNote_ * performanceControls_.clockDivider;

	// Store clip position for position indicator and request refresh if pad position changed
	int32_t oldPadX = -1;
	int32_t totalLengthTicks = ticksPerSixteenthNote_ * performanceControls_.clockDivider * sequencerState_.totalPatternLength;
	if (totalLengthTicks > 0) {
		oldPadX = (lastAbsolutePlaybackPos_ * kDisplayWidth) / totalLengthTicks;
	}

	lastAbsolutePlaybackPos_ = clip->lastProcessedPos;

	// Request UI refresh if position indicator moved to a new pad
	if (totalLengthTicks > 0) {
		int32_t newPadX = (lastAbsolutePlaybackPos_ * kDisplayWidth) / totalLengthTicks;
		if (newPadX != oldPadX) {
			uiNeedsRendering(&instrumentClipView, 1 << 7, 0); // Refresh y7 only
		}
	}

	// Check for any notes that need to be turned off
	for (int32_t i = 0; i < 16; i++) {
		if (sequencerState_.noteActive[i]) {
			// Check if note should end now
			if (absolutePlaybackPos >= sequencerState_.noteGatePos[i]) {
				stopNote(modelStackPtr, sequencerState_.noteCodeActive[i]);
				sequencerState_.noteActive[i] = false;
				sequencerState_.noteCodeActive[i] = -1;
			}
		}
	}

	// Check if we're at a period boundary
	bool atBoundary = atDivisionBoundary(absolutePlaybackPos, ticksPerPeriod);

	if (atBoundary) {
		// Store old stage to detect changes
		int32_t oldStage = sequencerState_.lastPlayedStage;

		// Flash pad for visual feedback
		sequencerState_.gatePadFlashing = true;
		sequencerState_.flashStartTime = playbackHandler.getCurrentInternalTickCount();
		sequencerState_.lastPlayedStage = performanceControls_.currentStage;

		// Generate notes
		generateNotes(modelStackPtr);

		// Refresh the gate line and octave pads when we move to a new stage
		if (oldStage != performanceControls_.currentStage) {
			int32_t gateLineY = getGateLineY();
			uint32_t rowsToRefresh = (1 << gateLineY) | (1 << (gateLineY + 1)) | (1 << (gateLineY + 2)) | (1 << (gateLineY + 3));
			uiNeedsRendering(&instrumentClipView, rowsToRefresh, 0);
		}
	}

	return ticksUntilNextDivision(absolutePlaybackPos, ticksPerPeriod);
}

void PulseSequencerMode::generateNotes(void* modelStackPtr) {
	int32_t stage = performanceControls_.currentStage;

	if (stage >= 0 && stage < performanceControls_.numStages && performanceControls_.stageEnabled[stage]) {
		int32_t pulseInStage = sequencerState_.currentPulse;

		if (evaluateRhythmPattern(stage, pulseInStage)) {
			playNoteForStage(modelStackPtr, stage);
		}
	}

	sequencerState_.lastPlayedStage = performanceControls_.currentStage;

	// Advance to next pulse
	sequencerState_.currentPulse++;
	if (sequencerState_.currentPulse >= stages_[performanceControls_.currentStage].pulseCount) {
		sequencerState_.currentPulse = 0;
		advanceToNextEnabledStage();
	}
}

void PulseSequencerMode::playNoteForStage(void* modelStackPtr, int32_t stage) {
	StageData& stageData = stages_[stage];

	if (stageData.gateType == GateType::OFF) {
		return;
	}

	// Get all scale notes starting from C3 (MIDI 60) for better range
	int32_t scaleNotes[64];
	int32_t numNotes = getScaleNotes(modelStackPtr, scaleNotes, 64, 6, 0); // 6 octaves starting from root

	if (numNotes == 0) {
		return;
	}

	// Calculate note index with transpose
	int32_t noteIndexInScale = stageData.noteIndex + performanceControls_.transpose;

	// Wrap to scale
	while (noteIndexInScale < 0) noteIndexInScale += numNotes;
	while (noteIndexInScale >= numNotes) noteIndexInScale -= numNotes;

	// Get note from scale (already includes the root note from getScaleNotes)
	int32_t note = scaleNotes[noteIndexInScale];

	// Add base octave offset to start from C3 (MIDI 60) instead of very low notes
	// The root note from scale is already included, so we just shift up to C3 range
	note += 48; // Shift up 4 octaves to C3 range

	// Apply stage octave and global octave offsets
	note += (stageData.octave * 12) + (performanceControls_.octave * 12);

	// Clamp to MIDI range
	if (note < 0) note = 0;
	if (note > 127) note = 127;

	// Calculate note length based on gate type
	ModelStackWithTimelineCounter* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);
	InstrumentClip* clip = static_cast<InstrumentClip*>(modelStack->getTimelineCounter());

	int32_t noteLength;
	if (stageData.gateType == GateType::HELD) {
		// HELD: note lasts for entire stage duration
		noteLength = ticksPerSixteenthNote_ * performanceControls_.clockDivider * stageData.pulseCount;
	}
	else {
		// SINGLE/MULTIPLE: short staccato notes (50% of period)
		noteLength = (ticksPerSixteenthNote_ * performanceControls_.clockDivider) / 2;
	}

	// Find free slot to track this note
	int32_t freeSlot = -1;
	for (int32_t i = 0; i < 16; i++) {
		if (!sequencerState_.noteActive[i]) {
			freeSlot = i;
			break;
		}
	}

	if (freeSlot >= 0) {
		// Apply probability check
		if (!shouldPlayBasedOnProbability(stageData.probability)) {
			return; // Don't play this note
		}

		// Apply velocity spread
		uint8_t baseVelocity = 100;
		uint8_t velocity = applyVelocitySpread(baseVelocity, stageData.velocitySpread);

		// Apply gate length to note duration
		noteLength = (noteLength * stageData.gateLength) / 100;
		if (noteLength < 1) noteLength = 1; // Ensure at least 1 tick

		// Send note-on (Deluge convention: pass length but we still track note-off ourselves)
		playNote(modelStackPtr, note, velocity, noteLength);

		// Track for automatic note-off
		sequencerState_.noteCodeActive[freeSlot] = note;
		sequencerState_.noteGatePos[freeSlot] = clip->lastProcessedPos + noteLength;
		sequencerState_.noteActive[freeSlot] = true;
	}
}

void PulseSequencerMode::switchNoteOff(void* modelStackPtr, int32_t noteSlot) {
	if (noteSlot < 0 || noteSlot >= 16 || !sequencerState_.noteActive[noteSlot]) {
		return;
	}

	int32_t note = sequencerState_.noteCodeActive[noteSlot];
	if (note >= 0) {
		stopNote(modelStackPtr, note);
	}

	sequencerState_.noteCodeActive[noteSlot] = -1;
	sequencerState_.noteGatePos[noteSlot] = 0;
	sequencerState_.noteActive[noteSlot] = false;
}

void PulseSequencerMode::advanceToNextEnabledStage() {
	int32_t nextStage = performanceControls_.currentStage;
	int32_t direction = 1;
	int32_t attempts = 0;

	switch (performanceControls_.playOrder) {
	case PlayOrder::FORWARDS:
		direction = 1;
		break;

	case PlayOrder::BACKWARDS:
		direction = -1;
		break;

	case PlayOrder::PING_PONG:
		direction = performanceControls_.pingPongDirection;
		break;

	case PlayOrder::RANDOM: {
		int32_t enabledStages[8];
		int32_t enabledCount = 0;
		for (int32_t i = 0; i < performanceControls_.numStages; i++) {
			if (performanceControls_.stageEnabled[i]) {
				enabledStages[enabledCount++] = i;
			}
		}
		if (enabledCount > 0) {
			performanceControls_.currentStage = enabledStages[getRandom255() % enabledCount];
		}
		return;
	}

	case PlayOrder::PEDAL:
		// Always return to stage 1: 1,2,1,3,1,4,1,5,1,6,1,7,1,8
		if (performanceControls_.currentStage == 0) {
			performanceControls_.currentStage = performanceControls_.pedalNextStage;
			performanceControls_.pedalNextStage++;
			if (performanceControls_.pedalNextStage >= performanceControls_.numStages) {
				performanceControls_.pedalNextStage = 1;
			}
		}
		else {
			performanceControls_.currentStage = 0;
		}
		return;

	case PlayOrder::SKIP_2:
		// Skip every 2nd: 1,3,5,7,2,4,6,8
		if (performanceControls_.skip2OddPhase) {
			performanceControls_.currentStage += 2;
			if (performanceControls_.currentStage >= performanceControls_.numStages) {
				performanceControls_.currentStage = 1;
				performanceControls_.skip2OddPhase = false;
			}
		}
		else {
			performanceControls_.currentStage += 2;
			if (performanceControls_.currentStage >= performanceControls_.numStages) {
				performanceControls_.currentStage = 0;
				performanceControls_.skip2OddPhase = true;
			}
		}
		return;

	case PlayOrder::PENDULUM:
		// Swing pattern: 1,2,3,2,3,4,3,4,5,4,5,6,5,6,7,6,7,8
		if (performanceControls_.pendulumGoingUp) {
			performanceControls_.currentStage = performanceControls_.pendulumHigh;
			performanceControls_.pendulumGoingUp = false;
		}
		else {
			performanceControls_.currentStage = performanceControls_.pendulumLow;
			performanceControls_.pendulumGoingUp = true;

			performanceControls_.pendulumLow++;
			performanceControls_.pendulumHigh++;

			if (performanceControls_.pendulumHigh >= performanceControls_.numStages) {
				performanceControls_.pendulumLow = 0;
				performanceControls_.pendulumHigh = 1;
			}
		}
		return;

	case PlayOrder::SPIRAL:
		// Spiral inward: 1,8,2,7,3,6,4,5
		if (performanceControls_.spiralFromLow) {
			performanceControls_.currentStage = performanceControls_.spiralLow;
			performanceControls_.spiralLow++;
			performanceControls_.spiralFromLow = false;
		}
		else {
			performanceControls_.currentStage = performanceControls_.spiralHigh;
			performanceControls_.spiralHigh--;
			performanceControls_.spiralFromLow = true;
		}

		if (performanceControls_.spiralLow > performanceControls_.spiralHigh) {
			performanceControls_.spiralLow = 0;
			performanceControls_.spiralHigh = performanceControls_.numStages - 1;
		}
		return;
	}

	// Find next enabled stage (for FORWARDS/BACKWARDS/PING_PONG)
	do {
		nextStage += direction;
		attempts++;

		if (nextStage >= performanceControls_.numStages) {
			if (performanceControls_.playOrder == PlayOrder::PING_PONG) {
				nextStage = performanceControls_.numStages - 2;
				performanceControls_.pingPongDirection = -1;
				direction = -1;
			}
			else {
				nextStage = 0;
			}
		}
		else if (nextStage < 0) {
			if (performanceControls_.playOrder == PlayOrder::PING_PONG) {
				nextStage = 1;
				performanceControls_.pingPongDirection = 1;
				direction = 1;
			}
			else {
				nextStage = performanceControls_.numStages - 1;
			}
		}

		if (attempts > performanceControls_.numStages) {
			return;
		}

	} while (!performanceControls_.stageEnabled[nextStage]);

	performanceControls_.currentStage = nextStage;
}

bool PulseSequencerMode::evaluateRhythmPattern(int32_t stage, int32_t pulsePosition) {
	StageData& stageData = stages_[stage];

	switch (stageData.gateType) {
	case GateType::SINGLE:
		return (pulsePosition == 0);
	case GateType::MULTIPLE:
		return (pulsePosition < stageData.pulseCount);
	case GateType::HELD:
		return (pulsePosition == 0);
	case GateType::OFF:
		return false;
	}

	return false;
}

int32_t PulseSequencerMode::calculateTotalPatternLength() const {
	int32_t totalLength = 0;
	for (int32_t i = 0; i < performanceControls_.numStages; i++) {
		totalLength += stages_[i].pulseCount;
	}
	return totalLength;
}

const char* PulseSequencerMode::getGateTypeName(GateType type) const {
	switch (type) {
	case GateType::OFF:
		return "OFF";
	case GateType::SINGLE:
		return "SINGLE";
	case GateType::MULTIPLE:
		return "MULTIPLE";
	case GateType::HELD:
		return "HELD";
	}
	return "UNKNOWN";
}

// ================================================================================================
// PAD INPUT HANDLING
// ================================================================================================

bool PulseSequencerMode::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	// Only handle presses (not releases) for most controls
	if (velocity == 0) {
		return false; // Let releases pass through
	}

	// FIXED POSITION CONTROLS - Check these FIRST before dynamic scrolling controls
	// y5: Octave down (x0-7) OR Velocity spread (x8-15)
	if (y == 5) {
		if (x < 8) {
			handleOctaveAdjustment(x, -1); // Octave down
			return true;
		}
		else if (x >= 8 && x < 16) {
			handleVelocitySpread(x - 8);
			return true;
		}
	}
	// y6: Octave up (x0-7) OR Probability (x8-15)
	else if (y == 6) {
		if (x < 8) {
			handleOctaveAdjustment(x, 1); // Octave up
			return true;
		}
		else if (x >= 8 && x < 16) {
			handleProbability(x - 8);
			return true;
		}
	}
	// y7: Note selection (x0-7) + Control buttons (x8-15)
	else if (y == 7) {
		if (x < 8) {
			// Note selection toggle for stage
			handleNoteSelection(x);
			return true;
		}
		else if (x == 8) {
			resetToDefaults();
			return true;
		}
		else if (x == 9) {
			randomizeSequence();
			return true;
		}
		else if (x == 10) {
			evolveSequence();
			return true;
		}
		else if (x == 11) {
			resetPerformanceControls();
			return true;
		}
		else if (x == 12) {
			handleTransposeChange(-1);
			return true;
		}
		else if (x == 13) {
			handleTransposeChange(1);
			return true;
		}
		else if (x == 14) {
			handleOctaveChange(-1);
			return true;
		}
		else if (x == 15) {
			handleOctaveChange(1);
			return true;
		}
	}
	
	// DYNAMIC SCROLLING CONTROLS - Check these AFTER fixed positions
	int32_t gateLineY = getGateLineY();

	// Gate line (y = gateLineY, x0-x7)
	if (y == gateLineY && x < 8) {
		handleGateType(x);
		return true;
	}
	// Note selection (above gate line) - only if NOT on fixed rows
	else if (y == gateLineY + 1 && x < 8 && y != 5 && y != 6 && y != 7) {
		handleNoteSelection(x);
		return true;
	}
	// Octave controls (above note selection) - only if NOT on fixed rows
	else if (y == gateLineY + 2 && x < 8 && y != 5 && y != 6 && y != 7) {
		handleOctaveAdjustment(x, -1); // Octave down
		return true;
	}
	else if (y == gateLineY + 3 && x < 8 && y != 5 && y != 6 && y != 7) {
		handleOctaveAdjustment(x, 1); // Octave up
		return true;
	}
	// Pulse count pads (below gate line)
	else if (y < gateLineY && x < 8) {
		handlePulseCount(x, gateLineY - 1 - y);
		return true;
	}
	// Performance controls on right side (x8-15)
	// y4: Stage count control
	else if (y == 4 && x >= 8 && x < kDisplayWidth) {
		handleStageCountChange(x - 7); // 1-8
		return true;
	}
	// y1: Play order presets
	else if (y == 1 && x >= 8 && x < 16) {
		handlePlayOrderChange(x - 8); // 0-7
		return true;
	}
	// y3: Stage enable/disable toggle
	else if (y == 3 && x >= 8 && x < kDisplayWidth) {
		handleStageToggle(x - 8); // 0-7
		return true;
	}
	// y2: Gate length (x8-15)
	else if (y == 2 && x >= 8 && x < 16) {
		handleGateLength(x - 8);
		return true;
	}

	return false; // Didn't handle this pad
}

bool PulseSequencerMode::handleVerticalEncoder(int32_t offset) {
	// Scroll the view vertically to access all 8 pulse count rows
	displayState_.gateLineOffset += offset;

	// Clamp to valid range (0-3, maps to gate line Y positions 4-7)
	if (displayState_.gateLineOffset < 0) displayState_.gateLineOffset = 0;
	if (displayState_.gateLineOffset > 3) displayState_.gateLineOffset = 3;

	// Show popup indicating scroll position
	char buffer[20];
	snprintf(buffer, sizeof(buffer), "View: %d", displayState_.gateLineOffset);
	display->displayPopup(buffer);

	return true; // We handled it
}

// ================================================================================================
// PAD INPUT HANDLERS
// ================================================================================================

void PulseSequencerMode::handleGateType(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages) {
		return;
	}

	// Cycle through gate types: OFF -> SINGLE -> MULTIPLE -> HELD -> OFF
	int32_t currentType = static_cast<int32_t>(stages_[stage].gateType);
	int32_t nextType = (currentType + 1) % 4;
	stages_[stage].gateType = static_cast<GateType>(nextType);

	// Show popup
	const char* gateNames[] = {"OFF", "SINGLE", "MULTIPLE", "HELD"};
	char buffer[30];
	snprintf(buffer, sizeof(buffer), "Stage %d: %s", stage + 1, gateNames[nextType]);
	display->displayPopup(buffer);
}

void PulseSequencerMode::handleNoteSelection(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages) {
		return;
	}

	// Cycle through note indices (0-15 for more range)
	stages_[stage].noteIndex = (stages_[stage].noteIndex + 1) % 16;

	// Calculate the actual note to show its name
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (clip && clip->output->type == OutputType::SYNTH) {
		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStackWithTimelineCounter* modelStack = setupModelStackWithTimelineCounter(modelStackMemory, currentSong, clip);
		
		int32_t scaleNotes[64];
		int32_t numNotes = getScaleNotes(modelStack, scaleNotes, 64, 6, 0);
		
		if (numNotes > 0) {
			// Calculate note with current settings
			int32_t noteIndexInScale = stages_[stage].noteIndex + performanceControls_.transpose;
			while (noteIndexInScale < 0) noteIndexInScale += numNotes;
			while (noteIndexInScale >= numNotes) noteIndexInScale -= numNotes;
			
			int32_t note = scaleNotes[noteIndexInScale] + 48; // Base C3 offset
			note += (stages_[stage].octave * 12) + (performanceControls_.octave * 12);
			
			if (note < 0) note = 0;
			if (note > 127) note = 127;
			
			// Convert to note name
			char noteNameBuffer[10];
			int32_t lengthDummy = 0;
			noteCodeToString(note, noteNameBuffer, &lengthDummy, true);
			
			char buffer[30];
			snprintf(buffer, sizeof(buffer), "Stage %d: %s", stage + 1, noteNameBuffer);
			display->displayPopup(buffer);
			return;
		}
	}
	
	// Fallback
	char buffer[30];
	snprintf(buffer, sizeof(buffer), "Stage %d Note: %d", stage + 1, stages_[stage].noteIndex + 1);
	display->displayPopup(buffer);
}

void PulseSequencerMode::handleOctaveAdjustment(int32_t stage, int32_t direction) {
	if (stage < 0 || stage >= kMaxStages) {
		return;
	}

	int32_t newOctave = stages_[stage].octave + direction;
	if (newOctave < -2) newOctave = -2;
	if (newOctave > 3) newOctave = 3;

	stages_[stage].octave = newOctave;

	// Show popup with octave value
	char buffer[30];
	snprintf(buffer, sizeof(buffer), "Stage %d Oct: %+d", stage + 1, stages_[stage].octave);
	display->displayPopup(buffer);
}

void PulseSequencerMode::handlePulseCount(int32_t stage, int32_t position) {
	if (stage < 0 || stage >= kMaxStages) {
		return;
	}
	if (position < 0 || position >= kMaxPulseCount) {
		return;
	}

	int32_t newPulseCount = position + 1;
	if (newPulseCount != stages_[stage].pulseCount) {
		stages_[stage].pulseCount = newPulseCount;
		sequencerState_.totalPatternLength = calculateTotalPatternLength();

		if (sequencerState_.currentPulse >= sequencerState_.totalPatternLength) {
			sequencerState_.currentPulse = 0;
		}
	}
}

void PulseSequencerMode::handleStageCountChange(int32_t numStages) {
	if (numStages < 1) numStages = 1;
	if (numStages > 8) numStages = 8;

	if (performanceControls_.numStages != numStages) {
		performanceControls_.numStages = numStages;
		sequencerState_.totalPatternLength = calculateTotalPatternLength();
	}
}

void PulseSequencerMode::handlePlayOrderChange(int32_t playOrderIndex) {
	if (playOrderIndex < 0 || playOrderIndex > 7) {
		return;
	}

	PlayOrder newPlayOrder = static_cast<PlayOrder>(playOrderIndex);
	if (performanceControls_.playOrder != newPlayOrder) {
		performanceControls_.playOrder = newPlayOrder;
		performanceControls_.pingPongDirection = 1;

		// Show popup with order name
		const char* orderNames[] = {"FORWARDS", "BACKWARDS", "PING PONG", "RANDOM", "PEDAL", "SKIP 2", "PENDULUM", "SPIRAL"};
		display->displayPopup(orderNames[playOrderIndex]);
	}
}

void PulseSequencerMode::handleTransposeChange(int32_t direction) {
	performanceControls_.transpose += direction;

	if (performanceControls_.transpose < -12) {
		performanceControls_.transpose = -12;
	}
	if (performanceControls_.transpose > 12) {
		performanceControls_.transpose = 12;
	}

	// Show popup
	char buffer[20];
	snprintf(buffer, sizeof(buffer), "Transpose: %+d", performanceControls_.transpose);
	display->displayPopup(buffer);
}

void PulseSequencerMode::handleOctaveChange(int32_t direction) {
	performanceControls_.octave += direction;

	if (performanceControls_.octave < -3) {
		performanceControls_.octave = -3;
	}
	if (performanceControls_.octave > 3) {
		performanceControls_.octave = 3;
	}

	// Show popup
	char buffer[20];
	snprintf(buffer, sizeof(buffer), "Octave: %+d", performanceControls_.octave);
	display->displayPopup(buffer);
}

void PulseSequencerMode::handleStageToggle(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages) {
		return;
	}

	performanceControls_.stageEnabled[stage] = !performanceControls_.stageEnabled[stage];
}

void PulseSequencerMode::handleVelocitySpread(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages) {
		return;
	}

	// Cycle through velocity spread values: 0, 20, 40, 60, 80, 100, 127
	int32_t spreads[] = {0, 20, 40, 60, 80, 100, 127};
	int32_t currentIndex = 0;
	for (int32_t i = 0; i < 7; i++) {
		if (stages_[stage].velocitySpread == spreads[i]) {
			currentIndex = i;
			break;
		}
	}
	stages_[stage].velocitySpread = spreads[(currentIndex + 1) % 7];

	// Show popup
	char buffer[30];
	snprintf(buffer, sizeof(buffer), "Stage %d Spread: %d", stage + 1, stages_[stage].velocitySpread);
	display->displayPopup(buffer);
}

void PulseSequencerMode::handleProbability(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages) {
		return;
	}

	// Cycle through probability values: 100%, 80%, 60%, 40%, 20%
	int32_t probs[] = {100, 80, 60, 40, 20};
	int32_t currentIndex = 0;
	for (int32_t i = 0; i < 5; i++) {
		if (stages_[stage].probability == probs[i]) {
			currentIndex = i;
			break;
		}
	}
	stages_[stage].probability = probs[(currentIndex + 1) % 5];

	// Show popup
	char buffer[30];
	snprintf(buffer, sizeof(buffer), "Stage %d Prob: %d%%", stage + 1, stages_[stage].probability);
	display->displayPopup(buffer);
}

void PulseSequencerMode::handleGateLength(int32_t stage) {
	if (stage < 0 || stage >= kMaxStages) {
		return;
	}

	// Cycle through gate length values: 10%, 25%, 50%, 75%, 90%, 100%
	int32_t lengths[] = {10, 25, 50, 75, 90, 100};
	int32_t currentIndex = 2; // default 50%
	for (int32_t i = 0; i < 6; i++) {
		if (stages_[stage].gateLength == lengths[i]) {
			currentIndex = i;
			break;
		}
	}
	stages_[stage].gateLength = lengths[(currentIndex + 1) % 6];

	// Show popup
	char buffer[30];
	snprintf(buffer, sizeof(buffer), "Stage %d Gate: %d%%", stage + 1, stages_[stage].gateLength);
	display->displayPopup(buffer);
}

void PulseSequencerMode::resetToDefaults() {
	performanceControls_.transpose = 0;
	performanceControls_.octave = 0;
	performanceControls_.clockDivider = 1;  // Default: 16th notes (1=16th, 2=8th, 4=quarter)
	performanceControls_.numStages = 8;
	performanceControls_.playOrder = PlayOrder::FORWARDS;
	performanceControls_.pingPongDirection = 1;
	performanceControls_.currentStage = 0;

	for (int32_t i = 0; i < 8; i++) {
		performanceControls_.stageEnabled[i] = true;
		stages_[i].gateType = GateType::OFF;
		stages_[i].noteIndex = 0;
		stages_[i].octave = 0;
		stages_[i].pulseCount = 1;
		stages_[i].velocitySpread = 0;
		stages_[i].probability = 100;
		stages_[i].gateLength = 50;
	}

	sequencerState_.totalPatternLength = calculateTotalPatternLength();
	display->displayPopup("RESET ALL");
}

void PulseSequencerMode::resetPerformanceControls() {
	// Reset all stages to default performance values
	for (int32_t i = 0; i < 8; i++) {
		stages_[i].velocitySpread = 0;
		stages_[i].probability = 100;
		stages_[i].gateLength = 50;
	}
	display->displayPopup("RESET PERF");
}

void PulseSequencerMode::randomizeSequence() {
	for (int32_t i = 0; i < 8; i++) {
		// Randomize gate type (skip OFF for interesting patterns)
		int32_t gateTypeIndex = getRandom255() % 3;
		stages_[i].gateType = static_cast<GateType>(gateTypeIndex + 1);

		// Randomize note index (0-7)
		stages_[i].noteIndex = getRandom255() % 8;

		// Randomize octave (-2 to +3)
		stages_[i].octave = (getRandom255() % 6) - 2;

		// Randomize pulse count (bias toward lower values)
		uint8_t random = getRandom255();
		if (random < 128) {
			stages_[i].pulseCount = 1;
		}
		else if (random < 192) {
			stages_[i].pulseCount = 2;
		}
		else if (random < 224) {
			stages_[i].pulseCount = 3;
		}
		else if (random < 240) {
			stages_[i].pulseCount = 4;
		}
		else {
			stages_[i].pulseCount = (getRandom255() % 3) + 5;
		}
	}

	sequencerState_.totalPatternLength = calculateTotalPatternLength();
}

void PulseSequencerMode::evolveSequence() {
	int32_t numStagesToChange = (getRandom255() % 4) + 1;

	for (int32_t change = 0; change < numStagesToChange; change++) {
		int32_t stageToChange = getRandom255() % 8;

		if (getRandom255() < 179) { // 70% chance - change note
			int32_t currentNote = stages_[stageToChange].noteIndex;
			int32_t noteChange = (getRandom255() % 5) - 2; // -2 to +2
			int32_t newNote = currentNote + noteChange;

			// Wrap around
			while (newNote < 0) newNote += 8;
			while (newNote >= 8) newNote -= 8;

			stages_[stageToChange].noteIndex = newNote;
		}
		else { // 30% chance - change octave
			int32_t currentOctave = stages_[stageToChange].octave;
			int32_t octaveChange = (getRandom255() < 128) ? -1 : 1;
			int32_t newOctave = currentOctave + octaveChange;

			if (newOctave < -2) newOctave = -2;
			if (newOctave > 3) newOctave = 3;

			stages_[stageToChange].octave = newOctave;
		}
	}
}

} // namespace deluge::model::clip::sequencer::modes

// Register this mode with the manager
namespace {
	static auto registered_pulse_sequencer_mode = []() {
		deluge::model::clip::sequencer::SequencerModeManager::instance().registerMode<deluge::model::clip::sequencer::modes::PulseSequencerMode>("pulse_seq");
		return true;
	}();
}
