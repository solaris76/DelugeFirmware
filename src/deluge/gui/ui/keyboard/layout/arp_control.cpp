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

#include "gui/ui/keyboard/layout/arp_control.h"
#include "gui/colour/colour.h"
#include "gui/ui/keyboard/keyboard_screen.h"
#include "gui/ui/sound_editor.h"
#include "model/model_stack.h"
#include "modulation/params/param.h"
#include "gui/menu_item/value_scaling.h"
#include "hid/display/display.h"
#include "hid/led/pad_leds.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/song/song.h"
#include "modulation/arpeggiator.h"
#include "modulation/arpeggiator_rhythms.h"
#include "util/d_string.h"
#include "model/sync.h"
#include "gui/menu_item/sync_level.h"

namespace deluge::gui::ui::keyboard::layout {

void KeyboardLayoutArpControl::evaluatePads(PressedPad presses[kMaxNumKeyboardPadPresses]) {
	// Clear current notes state
	currentNotesState = NotesState{};

	// Get arp settings once
	ArpeggiatorSettings* settings = getArpSettings();
	if (!settings) return;

	// Process all pad presses - simple multi-touch support
	for (int32_t i = 0; i < kMaxNumKeyboardPadPresses; i++) {
		if (presses[i].active) {
			int32_t x = presses[i].x;
			int32_t y = presses[i].y;
			uint8_t velocity = 127; // Default velocity

			// Handle control pads first
			if (y == 0) {
				// Top row: Arp mode, octaves, rhythm
				if (x >= 0 && x < 3) {
					handleArpMode(x, settings);
				}
				else if (x >= 4 && x < 12) {
					handleOctaves(x, settings);
				}
				else if (x >= 12 && x < 15) {
					handleRhythm(x, settings);
				}
			}
			else if (y == 1) {
				// Row 1: Sequence length
				handleSequenceLength(x, settings);
			}
			else if (y == 2) {
				// Row 2: Velocity spread
				handleVelocitySpread(x, settings);
			}
			else if (y == 3) {
				// Row 3: Gate and transpose
				if (x >= 0 && x < 8) {
					handleGate(x, settings);
				}
				else if (x >= 14 && x < 16) {
					handleTranspose(x);
				}
			}
			else if (y >= 4 && y < 8) {
				// Rows 4-7: Keyboard
				handleKeyboard(x, y, velocity);
			}
		}
	}

	// Update display
			if (display->haveOLED()) {
				renderUIsForOled();
			}
			keyboardScreen.requestMainPadsRendering();
		}

void KeyboardLayoutArpControl::handleArpMode(int32_t x, ArpeggiatorSettings* settings) {
	// Cycle through all arp presets
	int32_t currentPreset = static_cast<int32_t>(settings->preset);
	int32_t newPreset = currentPreset + 1;

	// Wrap around to OFF if we go past CUSTOM
	if (newPreset > static_cast<int32_t>(ArpPreset::CUSTOM)) {
		newPreset = static_cast<int32_t>(ArpPreset::OFF);
	}

	settings->preset = static_cast<ArpPreset>(newPreset);

	// Update all settings from the new preset
	settings->updateSettingsFromCurrentPreset();
	// Force arpeggiator to restart with new mode
	settings->flagForceArpRestart = true;

	// Show mode name in popup
	const char* modeName = getArpPresetDisplayName(settings->preset);
	display->displayPopup(modeName);

	// Force UI update
	keyboardScreen.requestMainPadsRendering();
}

void KeyboardLayoutArpControl::handleOctaves(int32_t x, ArpeggiatorSettings* settings) {
	// Direct octave control (1-8)
	int32_t newOctaves = x - 4 + 1;
	settings->numOctaves = newOctaves;
	// Force arpeggiator to restart with new octave count
	settings->flagForceArpRestart = false;
	display->displayPopup(("Octaves: " + std::to_string(newOctaves)).c_str());

	// Force UI update
	keyboardScreen.requestMainPadsRendering();
}

void KeyboardLayoutArpControl::handleRhythm(int32_t x, ArpeggiatorSettings* settings) {
	// Simple rhythm toggle
	if (displayState.appliedRhythm == 0) {
		displayState.appliedRhythm = displayState.currentRhythm;
		display->displayPopup("Rhythm ON");
	} else {
		displayState.appliedRhythm = 0;
		display->displayPopup("Rhythm OFF");
	}

	// Force UI update
	keyboardScreen.requestMainPadsRendering();
}

void KeyboardLayoutArpControl::handleSequenceLength(int32_t x, ArpeggiatorSettings* settings) {
	// Track the last touched sequence length pad for LED feedback
	lastTouchedSequenceLengthPad = x;

	// Direct sequence length control - each pad has its own value
	int32_t newLength = sequenceLengthValues[x];

	// Write to unpatched param system like the menu does
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (clip) {
		UI* originalUI = getCurrentUI();

		// Set up sound editor context like the official menu
		if (soundEditor.setup(clip, nullptr, 0)) {
			// Now we're in sound editor context - use the official approach
			char modelStackMemory[MODEL_STACK_MAX_SIZE];
			ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);
			ModelStackWithAutoParam* modelStackWithParam = modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_ARP_SEQUENCE_LENGTH);

			if (modelStackWithParam && modelStackWithParam->autoParam) {
				// Use signed scaling like the menu system
				int32_t finalValue = computeFinalValueForStandardMenuItem(newLength);
				modelStackWithParam->autoParam->setCurrentValueInResponseToUserInput(finalValue, modelStackWithParam);
			}

			// Exit sound editor context back to original UI
			originalUI->focusRegained();
		}
	}

	// Display "OFF" for value 0, otherwise show the value
	if (newLength == 0) {
		display->displayPopup("Seq Length: OFF");
	} else {
		display->displayPopup(("Seq Length: " + std::to_string(newLength)).c_str());
	}

	// Force UI update
	keyboardScreen.requestMainPadsRendering();
}

void KeyboardLayoutArpControl::handleVelocitySpread(int32_t x, ArpeggiatorSettings* settings) {
	// Track the last touched velocity pad for LED feedback
	lastTouchedVelocityPad = x;

	// Direct velocity spread control - each pad has its own value
	int32_t newVelocity = velocitySpreadValues[x];

	// Write to unpatched param system like the menu does
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (clip) {
		UI* originalUI = getCurrentUI();

		// Set up sound editor context like the official menu
		if (soundEditor.setup(clip, nullptr, 0)) {
			// Now we're in sound editor context - use the official approach
			char modelStackMemory[MODEL_STACK_MAX_SIZE];
			ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);
			ModelStackWithAutoParam* modelStackWithParam = modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_SPREAD_VELOCITY);

			if (modelStackWithParam && modelStackWithParam->autoParam) {
				// Use signed scaling like the menu system
				int32_t finalValue = computeFinalValueForStandardMenuItem(newVelocity);
				modelStackWithParam->autoParam->setCurrentValueInResponseToUserInput(finalValue, modelStackWithParam);
			}

			// Exit sound editor context back to original UI
			originalUI->focusRegained();
		}
	}

	// Display "OFF" for value 0, otherwise show the value
	if (newVelocity == 0) {
		display->displayPopup("Velocity: OFF");
	} else {
		display->displayPopup(("Velocity: " + std::to_string(newVelocity)).c_str());
	}

	// Force UI update
	keyboardScreen.requestMainPadsRendering();
}

void KeyboardLayoutArpControl::handleGate(int32_t x, ArpeggiatorSettings* settings) {
	// Track the last touched gate pad for LED feedback
	lastTouchedGatePad = x;

	// Direct gate control - each pad has its own value
	int32_t newGate = gateValues[x];

	// Write to unpatched param system like the menu does
	InstrumentClip* clip = getCurrentInstrumentClip();
		if (clip) {
		UI* originalUI = getCurrentUI();

		// Set up sound editor context like the official menu
		if (soundEditor.setup(clip, nullptr, 0)) {
			// Now we're in sound editor context - use the official approach
			char modelStackMemory[MODEL_STACK_MAX_SIZE];
			ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);
			ModelStackWithAutoParam* modelStackWithParam = modelStack->getUnpatchedAutoParamFromId(modulation::params::UNPATCHED_ARP_GATE);

			if (modelStackWithParam && modelStackWithParam->autoParam) {
				// Use absolute value (0-50) without scaling
				int32_t finalValue = computeFinalValueForStandardMenuItem(newGate);
				modelStackWithParam->autoParam->setCurrentValueInResponseToUserInput(finalValue, modelStackWithParam);
			}

			// Exit sound editor context back to original UI
			originalUI->focusRegained();
		}
	}

	display->displayPopup(("Gate: " + std::to_string(newGate)).c_str());

	// Force UI update
	keyboardScreen.requestMainPadsRendering();
}

void KeyboardLayoutArpControl::handleTranspose(int32_t x) {
	if (x == 14) {
		keyboardScrollOffset -= 12; // Down one octave
		display->displayPopup("Keyboard -1 Oct");
	} else if (x == 15) {
		keyboardScrollOffset += 12; // Up one octave
		display->displayPopup("Keyboard +1 Oct");
	}
}

void KeyboardLayoutArpControl::handleKeyboard(int32_t x, int32_t y, uint8_t velocity) {
	uint16_t note = noteFromCoords(x, y) + keyboardScrollOffset;
	if (note < 128) {
		enableNote(note, velocity);
	}
}

void KeyboardLayoutArpControl::renderPads(RGB image[][kDisplayWidth + kSideBarWidth]) {
	ArpeggiatorSettings* settings = getArpSettings();
	if (!settings) return;

	// Top row: Arp mode, octaves, rhythm
	for (int32_t x = 0; x < kDisplayWidth; x++) {
		if (x >= 0 && x < 3) {
			// Arp mode display
			image[0][x] = getArpModeColor(settings);
		}
		else if (x >= 4 && x < 12) {
			// Octave display
			image[0][x] = getOctaveColor(x - 4, settings->numOctaves);
		}
		else if (x >= 12 && x < 15) {
			// Rhythm display
			image[0][x] = getRhythmColor(x - 12, displayState.appliedRhythm);
	}
		else {
			// Unused pads are black
			image[0][x] = colours::black;
		}
	}

	// Row 1: Sequence length (8 pads wide)
	for (int32_t x = 0; x < 8; x++) {
		image[1][x] = getSequenceLengthColor(x);
	}
	for (int32_t x = 8; x < kDisplayWidth; x++) {
		// Unused pads are black
		image[1][x] = colours::black;
	}

	// Row 2: Velocity spread
	for (int32_t x = 0; x < 8; x++) {
		image[2][x] = getVelocitySpreadColor(x);
	}
	for (int32_t x = 8; x < kDisplayWidth; x++) {
		// Unused pads are black
		image[2][x] = colours::black;
	}

	// Row 3: Gate and rhythm visualization
	for (int32_t x = 0; x < 8; x++) {
		image[3][x] = getGateColor(x);
	}
	
	// Rhythm pattern visualization on pads x8-x13
	for (int32_t x = 8; x < 14; x++) {
		image[3][x] = getRhythmPatternColor(x - 8);
	}
	
	image[3][14] = colours::red; // Down transpose
	image[3][15] = colours::purple; // Up transpose

	// Rows 4-7: Keyboard (stop at y=7, leave y=8-15 for control columns)
	for (int32_t y = 4; y < 8; y++) {
		for (int32_t x = 0; x < 15; x++) {
			image[y][x] = getKeyboardColor(x, y);
		}
	}

	// Rows 8-15: Control columns (y16, y17 when 0-indexed)
	for (int32_t y = 8; y < 16; y++) {
		for (int32_t x = 0; x < kDisplayWidth; x++) {
			// Control columns - will be handled by holding y16, y17
			image[y][x] = colours::black;
		}
	}
}

// Color functions
RGB KeyboardLayoutArpControl::getArpModeColor(ArpeggiatorSettings* settings) {
	switch (settings->preset) {
		case ArpPreset::OFF: return colours::red;
		case ArpPreset::UP: return colours::pink;
		case ArpPreset::DOWN: return colours::pink;
		case ArpPreset::BOTH: return colours::pink;
		case ArpPreset::RANDOM: return colours::pink;
		case ArpPreset::WALK: return colours::magenta;
		case ArpPreset::CUSTOM: return colours::white;
		default: return colours::red;
	}
}

RGB KeyboardLayoutArpControl::getOctaveColor(int32_t octave, int32_t currentOctaves) {
	return (octave < currentOctaves) ? colours::blue : RGB(0, 0, 40);
}

RGB KeyboardLayoutArpControl::getRhythmColor(int32_t rhythm, int32_t currentRhythm) {
	return (rhythm == currentRhythm) ? colours::yellow : colours::black;
}

RGB KeyboardLayoutArpControl::getSequenceLengthColor(int32_t length) {
	// Highlight the last touched sequence length pad, dim all others
	if (lastTouchedSequenceLengthPad == length) {
		return colours::orange; // Bright orange for last touched pad
	} else {
		return colours::orange.adjust(32, 3); // Dim orange for other pads
	}
}

RGB KeyboardLayoutArpControl::getVelocitySpreadColor(int32_t spread) {
	// Highlight the last touched velocity pad, dim all others
	if (lastTouchedVelocityPad == spread) {
		return colours::cyan; // Bright cyan for last touched pad
	} else {
		return colours::cyan.adjust(32, 3); // Dim cyan for other pads
	}
}

RGB KeyboardLayoutArpControl::getGateColor(int32_t gate) {
	// Highlight the last touched gate pad, dim all others
	if (lastTouchedGatePad == gate) {
		return colours::green; // Bright green for last touched pad
	} else {
		return colours::green.adjust(32, 3); // Dim green for other pads
	}
}

RGB KeyboardLayoutArpControl::getRhythmPatternColor(int32_t step) {
	// Show the currently selected rhythm pattern (not necessarily applied)
	if (displayState.currentRhythm == 0) {
		// Pattern 0 (all notes) - show all steps as dim white
		return RGB::monochrome(32);
	}
	
	// Clamp rhythm to valid range
	int32_t rhythmIndex = std::clamp(displayState.currentRhythm, static_cast<int32_t>(0), static_cast<int32_t>(kMaxPresetArpRhythm));
	
	// Get the rhythm pattern
	const ArpRhythm& pattern = arpRhythmPatterns[rhythmIndex];
	
	// Check if this step should be active
	if (step < pattern.length && pattern.steps[step]) {
		// Bright white for active steps, dimmer if not applied
		if (displayState.appliedRhythm == displayState.currentRhythm) {
			return RGB::monochrome(255); // Bright white when applied
		} else {
			return RGB::monochrome(128); // Dimmer white when not applied
		}
	} else {
		return RGB::monochrome(32); // Dim white for inactive steps
	}
}

void KeyboardLayoutArpControl::applyRhythmToArpSettings() {
	ArpeggiatorSettings* settings = getArpSettings();
	if (!settings) return;
	
	// Apply the rhythm setting to arp settings
	settings->rhythm = displayState.appliedRhythm;
	
	// Force arpeggiator to restart with new rhythm
	settings->flagForceArpRestart = true;
}


RGB KeyboardLayoutArpControl::getKeyboardColor(int32_t x, int32_t y) {
	// Get the note for this pad position
	auto note = noteFromCoords(x, y);

	// Calculate note within octave relative to root
	int32_t noteWithinOctave = (uint16_t)((note + kOctaveSize) - getRootNote()) % kOctaveSize;

	// Check if this note is currently pressed
	bool isPressed = false;
	for (int32_t i = 0; i < currentNotesState.count; i++) {
		if (currentNotesState.notes[i].note == note) {
			isPressed = true;
			break;
		}
	}

	// Get color source using getNoteColour function
	RGB colourSource = getNoteColour(note);

	// Full brightness and colour for active root note
	if (noteWithinOctave == 0 && isPressed) {
		return colourSource.adjust(255, 1);
	}
	// Full colour but less brightness for inactive root note
	else if (noteWithinOctave == 0) {
		return colourSource.adjust(255, 2);
	}
	// Toned down colour but high brightness for active scale note
	else if (isPressed) {
		return colourSource.adjust(127, 3);
	}
	// Dimly white for inactive scale notes
	else {
		return RGB::monochrome(1);
	}
}

// Essential functions
ArpeggiatorSettings* KeyboardLayoutArpControl::getArpSettings() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip) return nullptr;
	return &clip->arpSettings;
}

void KeyboardLayoutArpControl::handleVerticalEncoder(int32_t offset) {
	// Scroll through rhythm patterns (but don't apply until encoder is pressed)
	if (offset > 0) {
		displayState.currentRhythm++;
		if (displayState.currentRhythm > 50) displayState.currentRhythm = 1;
	} else {
		displayState.currentRhythm--;
		if (displayState.currentRhythm < 1) displayState.currentRhythm = 50;
	}

	// Display rhythm pattern name and status
	DEF_STACK_STRING_BUF(buffer, 30);
	if (displayState.currentRhythm == 0) {
		buffer.append("Rhythm: None");
	} else {
		buffer.append("Rhythm: ");
		buffer.append(arpRhythmPatternNames[displayState.currentRhythm]);
		if (displayState.appliedRhythm == 0) {
			buffer.append(" (OFF)");
		} else {
			buffer.append(" (ON)");
		}
	}
	display->displayPopup(buffer.c_str());
}

void KeyboardLayoutArpControl::handleHorizontalEncoder(int32_t offset, bool shiftEnabled, PressedPad presses[kMaxNumKeyboardPadPresses], bool encoderPressed) {
	// Arp rate control
	ArpeggiatorSettings* settings = getArpSettings();
	if (!settings) return;

	// Get current sync value from arp settings
	deluge::gui::menu_item::SyncLevel syncLevel;
	int32_t currentSyncValue = syncLevel.syncTypeAndLevelToMenuOption(settings->syncType, settings->syncLevel);
	int32_t newSyncValue = currentSyncValue + offset;

	// Clamp to valid sync range (0 to NUM_SYNC_VALUES-1)
	if (newSyncValue < 0) newSyncValue = 0;
	if (newSyncValue >= NUM_SYNC_VALUES) newSyncValue = NUM_SYNC_VALUES - 1;

	// Update arp settings with new sync values
	settings->syncType = syncValueToSyncType(newSyncValue);
	settings->syncLevel = syncValueToSyncLevel(newSyncValue);

	// Force arpeggiator to restart with new rate
	settings->flagForceArpRestart = true;

	// Display proper note name using sync value
	DEF_STACK_STRING_BUF(buffer, 20);
	syncValueToString(newSyncValue, buffer, currentSong->getInputTickMagnitude());
	display->displayPopup(buffer.c_str());
}

void KeyboardLayoutArpControl::precalculate() {
	// No precalculation needed
}

void KeyboardLayoutArpControl::updateAnimation() {
	// Simple animation update
}

void KeyboardLayoutArpControl::updateDisplay() {
	// Simple display update
}

void KeyboardLayoutArpControl::updatePadLEDsDirect() {
	// Simple pad LED update
}

void KeyboardLayoutArpControl::updatePlaybackProgressBar() {
	// Simple progress bar update
}

char const* KeyboardLayoutArpControl::getArpPresetDisplayName(ArpPreset preset) {
	switch (preset) {
		case ArpPreset::OFF: return "OFF";
		case ArpPreset::UP: return "UP";
		case ArpPreset::DOWN: return "DOWN";
		case ArpPreset::BOTH: return "BOTH";
		case ArpPreset::RANDOM: return "RANDOM";
		case ArpPreset::WALK: return "WALK";
		case ArpPreset::CUSTOM: return "CUSTOM";
		default: return "UNKNOWN";
	}
}

} // namespace deluge::gui::ui::keyboard::layout
