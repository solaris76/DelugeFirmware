/*
 * Copyright © 2016-2023 Synthstrom Audible Limited
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
#include "gui/ui/keyboard/chords.h"
#include "gui/ui/keyboard/layout/column_control_state.h"
#include "storage/flash_storage.h"

namespace deluge::gui::ui::keyboard {

constexpr int32_t kDefaultIsometricRowInterval = 5;
struct KeyboardStateIsomorphic {
	int32_t scrollOffset = (60 - (kDisplayHeight >> 2) * kDefaultIsometricRowInterval);
	int32_t rowInterval = kDefaultIsometricRowInterval;
};

struct KeyboardStateDrums {
	int32_t scroll_offset = 0;
	int32_t zoom_level = 8;
};

constexpr int32_t kDefaultInKeyRowInterval = 3;
struct KeyboardStateInKey {
	// Init scales have 7 elements, multipled by three octaves gives us C1 as first pad
	int32_t scrollOffset = (7 * 3);
	int32_t rowInterval = kDefaultInKeyRowInterval;
};

struct KeyboardStatePiano {
	// default octave = 1 (0 = -2oct), use a vertical scroll to change it
	int32_t scrollOffset = 3;
	// default note=0 (C)
	int32_t noteOffset = 0;
};

struct KeyboardStateChordLibrary {
	int32_t rowInterval = kOctaveSize;
	int32_t scrollOffset = 0;
	int32_t noteOffset = (rowInterval * 4);
	int32_t rowColorMultiplier = 5;
	ChordList chordList{};
};

struct KeyboardStateChord {
	int32_t noteOffset = (kOctaveSize * 4);
	int32_t modOffset = 0;
	int32_t scaleOffset = 0;
	bool autoVoiceLeading = false;
};

struct KeyboardStatePulseSeq {
	// Gate line position (0-4, where 4 is default showing all pitch pads above)
	int32_t gateLineOffset = 0; // 0 = gate line at Y=4, -1 = Y=3, etc.

	// Pulse Sequencer timing state
	int32_t currentStage = 0;           // Current stage (0-7)
	int32_t pulsesRemainingInStage = 1; // Pulses left in current stage
	bool isActive = false;              // Whether Pulse Sequencer is running

	// Stage parameters (8 stages, each with gate type, scale note, octave, pulse count)
	int32_t gateType[8] = {0, 0, 0, 0, 0, 0, 0, 0};   // 0=Off, 1=Single, 2=Multiple, 3=Hold
	int32_t scaleNote[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // Scale degree (0-6)
	int32_t octave[8] = {0, 0, 0, 0, 0, 0, 0, 0};     // Octave offset
	int32_t pulseCount[8] = {1, 1, 1, 1, 1, 1, 1, 1}; // Pulse count (1-8)
};
/// Please note that saving and restoring currently needs to be added manually in instrument_clip.cpp and all layouts
/// share one struct for storage
struct KeyboardState {
	KeyboardLayoutType currentLayout = FlashStorage::defaultKeyboardLayout;

	KeyboardStateIsomorphic isomorphic;
	KeyboardStateDrums drums;
	KeyboardStateInKey inKey;
	KeyboardStatePiano piano;
	KeyboardStateChord chord;
	KeyboardStateChordLibrary chordLibrary;
	KeyboardStatePulseSeq pulseSeq;

	layout::ColumnControlState columnControl;
};

}; // namespace deluge::gui::ui::keyboard
