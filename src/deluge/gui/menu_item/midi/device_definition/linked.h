/*
 * Copyright (c) 2024 Sean Ditny
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

#include "gui/menu_item/toggle.h"
#include "gui/ui/load/load_midi_device_definition_ui.h"
#include "gui/ui/sound_editor.h"
#include "model/drum/midi_drum.h"
#include "model/instrument/kit.h"
#include "model/instrument/midi_instrument.h"
#include "model/output.h"
#include "model/song/song.h"

namespace deluge::gui::menu_item::midi::device_definition {

class Linked : public Toggle {
public:
	using Toggle::Toggle;

	void readCurrentValue() override {
		Output* output = getCurrentOutput();

		// Support both MIDI instruments and MIDI drum kit rows
		if (output->type == OutputType::MIDI_OUT) {
			MIDIInstrument* midiInstrument = (MIDIInstrument*)output;
			this->setValue(!midiInstrument->deviceDefinitionFileName.isEmpty());
		}
		else if (output->type == OutputType::KIT) {
			Kit* kit = (Kit*)output;
			if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
				MIDIDrum* midiDrum = (MIDIDrum*)kit->selectedDrum;
				this->setValue(!midiDrum->deviceDefinitionFileName.isEmpty());
			}
		}
	}

	void writeCurrentValue() override {
		t = this->getValue();
		Output* output = getCurrentOutput();

		// if you want to link a definition file, open the load definition file UI
		if (t) {
			openUI(&loadMidiDeviceDefinitionUI);
		}
		// if you want to unlink a definition file, just clear the definition file name
		else {
			if (output->type == OutputType::MIDI_OUT) {
				MIDIInstrument* midiInstrument = (MIDIInstrument*)output;
				midiInstrument->deviceDefinitionFileName.clear();
			}
			else if (output->type == OutputType::KIT) {
				Kit* kit = (Kit*)output;
				if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
					MIDIDrum* midiDrum = (MIDIDrum*)kit->selectedDrum;
					midiDrum->deviceDefinitionFileName.clear();
				}
			}
		}
	}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) {
		Output* output = getCurrentOutput();
		// Device definition is relevant for both MIDI instruments and MIDI drum kit rows
		if (output && output->type == OutputType::MIDI_OUT) {
			return true;
		}
		if (output && output->type == OutputType::KIT) {
			Kit* kit = (Kit*)output;
			return (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI);
		}
		return false;
	}

	void renderSubmenuItemTypeForOled(int32_t yPixel) final {
		deluge::hid::display::oled_canvas::Canvas& image = deluge::hid::display::OLED::main;

		int32_t startX = getSubmenuItemTypeRenderIconStart();

		if (getToggleValue()) {
			image.drawGraphicMultiLine(deluge::hid::display::OLED::checkedBoxIcon, startX, yPixel,
			                           kSubmenuIconSpacingX);

			// Get device definition file name from either MIDI instrument or MIDI drum
			char const* fullPath = nullptr;
			Output* output = getCurrentOutput();

			if (output->type == OutputType::MIDI_OUT) {
				MIDIInstrument* midiInstrument = (MIDIInstrument*)output;
				fullPath = midiInstrument->deviceDefinitionFileName.get();
			}
			else if (output->type == OutputType::KIT) {
				Kit* kit = (Kit*)output;
				if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
					MIDIDrum* midiDrum = (MIDIDrum*)kit->selectedDrum;
					fullPath = midiDrum->deviceDefinitionFileName.get();
				}
			}

			if (fullPath) {
				// locate last occurence of "/" in string
				char* fileName = strrchr((char*)fullPath, '/');
				if (fileName) {
					image.drawString(++fileName, kTextSpacingX, yPixel + (kTextSpacingY * 2), kTextSpacingX,
					                 kTextSpacingY);
				}
			}
		}
		else {
			image.drawGraphicMultiLine(deluge::hid::display::OLED::uncheckedBoxIcon, startX, yPixel,
			                           kSubmenuIconSpacingX);
		}
	}

	bool t;
};

} // namespace deluge::gui::menu_item::midi::device_definition
