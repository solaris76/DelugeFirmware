/*
 * Copyright © 2019-2023 Synthstrom Audible Limited
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

#include "gui/ui/rename/rename_midi_cc_ui.h"
#include "definitions_cxx.hpp"
#include "gui/l10n/l10n.h"
#include "gui/views/automation_view.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/led/pad_leds.h"
#include "model/drum/midi_drum.h"
#include "model/instrument/kit.h"
#include "model/instrument/midi_instrument.h"
#include "model/output.h"
#include "model/song/song.h"
#include <string_view>

RenameMidiCCUI renameMidiCCUI{"CC Name"};

bool RenameMidiCCUI::canRename() const {
	Clip* clip = getCurrentClip();
	int32_t cc = clip->lastSelectedParamID;
	// if we're not dealing with a real cc number
	// then don't allow user to edit the name
	return cc >= 0 && cc != CC_EXTERNAL_MOD_WHEEL && cc < kNumRealCCNumbers;
}

String RenameMidiCCUI::getName() const {
	Clip* clip = getCurrentClip();
	int32_t cc = clip->lastSelectedParamID;
	std::string_view name{};

	// Get name from MIDI instrument or MIDI drum
	if (clip->output->type == OutputType::MIDI_OUT) {
		MIDIInstrument* midiInstrument = (MIDIInstrument*)clip->output;
		name = midiInstrument->getNameFromCC(cc);
	}
	else if (clip->output->type == OutputType::KIT) {
		Kit* kit = (Kit*)clip->output;
		if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
			MIDIDrum* midiDrum = (MIDIDrum*)kit->selectedDrum;
			name = midiDrum->getNameFromCC(cc);
		}
	}

	String name_string;
	name_string.set(name.data(), name.length());
	return name_string;
}

bool RenameMidiCCUI::trySetName(String* name) {

	Clip* clip = getCurrentClip();
	int32_t cc = clip->lastSelectedParamID;

	// Set name for MIDI instrument or MIDI drum
	if (clip->output->type == OutputType::MIDI_OUT) {
		MIDIInstrument* midiInstrument = (MIDIInstrument*)clip->output;
		midiInstrument->setNameForCC(cc, enteredText.get());
		midiInstrument->editedByUser =
		    true; // need to set this to true so that the name gets saved with the song / preset
	}
	else if (clip->output->type == OutputType::KIT) {
		Kit* kit = (Kit*)clip->output;
		if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
			MIDIDrum* midiDrum = (MIDIDrum*)kit->selectedDrum;
			midiDrum->setNameForCC(cc, enteredText.get());
			// Note: For MIDI drums, names are saved per-drum (in device definition file)
			// No editedByUser flag needed - always saved with the drum
		}
	}

	return true;
}
