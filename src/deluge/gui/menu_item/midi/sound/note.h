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

#include "gui/menu_item/integer.h"
#include "gui/ui/sound_editor.h"
#include "model/drum/midi_drum.h"
#include "model/instrument/kit.h"
#include "model/output.h"

namespace deluge::gui::menu_item::midi::sound {

class Note final : public Integer {
public:
	using Integer::Integer;

	void readCurrentValue() override {
		Output* output = getCurrentOutput();
		if (output && output->type == OutputType::KIT) {
			Kit* kit = (Kit*)output;
			if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
				MIDIDrum* midiDrum = (MIDIDrum*)kit->selectedDrum;
				this->setValue(midiDrum->note);
			}
		}
	}

	void writeCurrentValue() override {
		Output* output = getCurrentOutput();
		if (output && output->type == OutputType::KIT) {
			Kit* kit = (Kit*)output;
			if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
				MIDIDrum* midiDrum = (MIDIDrum*)kit->selectedDrum;
				midiDrum->note = this->getValue();
			}
		}
	}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) override {
		Output* output = getCurrentOutput();
		return (output && output->type == OutputType::KIT);
	}

	int32_t getMinValue() const override { return 0; }
	int32_t getMaxValue() const override { return 127; }
};

} // namespace deluge::gui::menu_item::midi::sound
