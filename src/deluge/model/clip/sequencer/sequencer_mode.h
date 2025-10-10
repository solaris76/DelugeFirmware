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

#include "definitions_cxx.hpp"
#include "gui/l10n/l10n.h"
#include "hid/led/pad_leds.h"

namespace deluge::model::clip::sequencer {

/**
 * Base class for alternative sequencer modes that can replace linear clip playback
 * with pattern-based, algorithmic, or other non-linear sequencing approaches.
 *
 * This is the foundation for step sequencers, euclidean sequencers, granular modes,
 * generative sequencers, and other creative sequencing paradigms.
 */
class SequencerMode {
public:
	virtual ~SequencerMode() = default;

	// Core identification
	virtual l10n::String name() = 0;

	// Minimal interface - will be expanded in future steps
	virtual void initialize() {}
	virtual void cleanup() {}

	// Rendering - allow sequencer modes to override pad display
	// Returns true if the mode handled rendering, false to use default linear rendering
	virtual bool renderPads(uint32_t whichRows, RGB* image, uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], 
	                       int32_t xScroll, uint32_t xZoom, int32_t renderWidth, int32_t imageWidth) { return false; }
	
	// Pad input - handle user interaction with pads
	// Returns true if the mode handled the pad press, false to use default behavior
	// x, y: pad coordinates (0-15, 0-7)
	// velocity: press velocity (0 = release, 1-127 = press)
	virtual bool handlePadPress(int32_t x, int32_t y, int32_t velocity) { return false; }
	
	// Playback - called during clip playback to generate notes
	// Return value: ticks until this mode needs to be called again
	// modelStack: ModelStackWithTimelineCounter* for note triggering  
	// absolutePlaybackPos: playbackHandler.lastSwungTickActioned - NEVER resets, always incrementing
	virtual int32_t processPlayback(void* modelStack, int32_t absolutePlaybackPos) { return 2147483647; } // Max int = never
	
	// Simple callback when a musical division boundary is crossed
	// Override this for easy timing - base class handles the modulo math
	// syncLevel: 7=16th, 6=8th, 8=32nd (same as arpeggiator)
	virtual void onMusicalDivision(void* modelStack) {}

protected:
	// ========== SIMPLE HELPERS FOR SEQUENCER MODES ==========
	
	// TIMING HELPERS - Musical divisions
	// Use song->getSixteenthNoteLength(), song->getQuarterNoteLength(), song->getBarLength() 
	// to get tick periods that automatically handle tempo and resolution
	
	static bool atDivisionBoundary(int32_t absolutePos, int32_t ticksPerPeriod) {
		return (absolutePos % ticksPerPeriod) == 0;
	}
	
	static int32_t ticksUntilNextDivision(int32_t absolutePos, int32_t ticksPerPeriod) {
		int32_t howFarIntoPeriod = absolutePos % ticksPerPeriod;
		return howFarIntoPeriod == 0 ? ticksPerPeriod : (ticksPerPeriod - howFarIntoPeriod);
	}
	
	// SCALE HELPERS - Get notes in current scale
	// Fills an array with all scale notes across specified octave range
	// Returns the number of notes filled
	// maxNotes: size of the noteArray buffer
	// octaveRange: how many octaves to span
	// baseOctave: starting octave (0 = root octave, 1 = one octave up, etc.)
	static int32_t getScaleNotes(void* modelStackPtr, int32_t* noteArray, int32_t maxNotes, 
	                             int32_t octaveRange = 2, int32_t baseOctave = 0);
	
	// NOTE HELPERS - Easy note triggering
	// Sends a note on/off to the instrument
	static void playNote(void* modelStackPtr, int32_t noteCode, uint8_t velocity, int32_t length);
	static void stopNote(void* modelStackPtr, int32_t noteCode);

	// Track type compatibility (default: support all)
	virtual bool supportsInstrument() { return true; }
	virtual bool supportsKit() { return true; }
	virtual bool supportsMIDI() { return true; }
	virtual bool supportsCV() { return true; }
	virtual bool supportsAudio() { return false; } // Audio modes need special handling

protected:
	// Protected constructor - only concrete implementations can be instantiated
	SequencerMode() = default;
};

} // namespace deluge::model::clip::sequencer
