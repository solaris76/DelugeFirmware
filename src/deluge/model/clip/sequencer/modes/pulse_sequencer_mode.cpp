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

namespace deluge::model::clip::sequencer::modes {

void PulseSequencerMode::initialize() {
	initialized_ = true;
	ticksPerSixteenthNote_ = 0;

	// Initialize all stages to defaults
	for (int32_t i = 0; i < kMaxStages; i++) {
		stages_[i].gateType = GateType::OFF;
		stages_[i].noteIndex = 0;
		stages_[i].octave = 0;
		stages_[i].pulseCount = 1;
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
	performanceControls_.clockDivider = 2;
	performanceControls_.numStages = 8;
	performanceControls_.playOrder = PlayOrder::FORWARDS;
	performanceControls_.currentStage = 0;
}

void PulseSequencerMode::cleanup() {
	// Send all notes off
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

	return true;
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

	// Calculate 16th note ticks
	if (ticksPerSixteenthNote_ == 0) {
		ticksPerSixteenthNote_ = modelStack->song->getSixteenthNoteLength();
	}

	// Apply clock divider to timing
	int32_t ticksPerPeriod = ticksPerSixteenthNote_ * performanceControls_.clockDivider;

	// Track note-offs for active notes
	int32_t gateLength = ticksPerPeriod / 4; // 25% gate by default

	for (int32_t n = 0; n < 16; n++) {
		if (sequencerState_.noteActive[n]) {
			sequencerState_.noteGatePos[n]++;

			int32_t noteGateLength = gateLength;
			int32_t sourceStage = sequencerState_.noteSourceStage[n];
			if (sourceStage >= 0 && sourceStage < 8) {
				if (stages_[sourceStage].gateType == GateType::HELD) {
					noteGateLength = ticksPerPeriod * stages_[sourceStage].pulseCount;
				}
			}

			if (sequencerState_.noteGatePos[n] >= noteGateLength) {
				switchNoteOff(modelStackPtr, n);
			}
		}
	}

	// Check if we're at a period boundary
	bool atBoundary = atDivisionBoundary(absolutePlaybackPos, ticksPerPeriod);

	if (atBoundary) {
		// Flash pad for visual feedback
		sequencerState_.gatePadFlashing = true;
		sequencerState_.flashStartTime = playbackHandler.getCurrentInternalTickCount();
		sequencerState_.lastPlayedStage = performanceControls_.currentStage;

		// Generate notes
		generateNotes(modelStackPtr);
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

	// Get all scale notes
	int32_t scaleNotes[32];
	int32_t numNotes = getScaleNotes(modelStackPtr, scaleNotes, 32, 6, 0); // 6 octaves for full range

	if (numNotes == 0) {
		return;
	}

	// Calculate note index with octave offset
	int32_t noteIndexInScale = stageData.noteIndex + performanceControls_.transpose;

	// Wrap to scale
	while (noteIndexInScale < 0) noteIndexInScale += numNotes;
	while (noteIndexInScale >= numNotes) noteIndexInScale -= numNotes;

	// Get base note
	int32_t note = scaleNotes[noteIndexInScale];

	// Apply octave offsets
	note += (stageData.octave * 12) + (performanceControls_.octave * 12);

	// Clamp to MIDI range
	if (note < 0) note = 0;
	if (note > 127) note = 127;

	// Find free note slot
	int32_t noteSlot = -1;
	for (int32_t n = 0; n < 16; n++) {
		if (!sequencerState_.noteActive[n]) {
			noteSlot = n;
			break;
		}
	}

	if (noteSlot == -1) {
		noteSlot = 0; // Reuse first slot
	}

	// Play the note
	uint8_t velocity = 100;
	int32_t noteLength = (ticksPerSixteenthNote_ * performanceControls_.clockDivider) / 4;

	playNote(modelStackPtr, note, velocity, noteLength);

	// Track this note
	sequencerState_.noteActive[noteSlot] = true;
	sequencerState_.noteGatePos[noteSlot] = 0;
	sequencerState_.noteCodeActive[noteSlot] = note;
	sequencerState_.noteSourceStage[noteSlot] = stage;
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
	default:
		break;
	}

	// Find next enabled stage
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

} // namespace deluge::model::clip::sequencer::modes

// Register this mode with the manager
namespace {
	static auto registered_pulse_sequencer_mode = []() {
		deluge::model::clip::sequencer::SequencerModeManager::instance().registerMode<deluge::model::clip::sequencer::modes::PulseSequencerMode>("pulse_seq");
		return true;
	}();
}
