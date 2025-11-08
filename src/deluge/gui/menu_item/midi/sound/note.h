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
			auto* kit = static_cast<Kit*>(output);
			if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
				auto* midiDrum = static_cast<MIDIDrum*>(kit->selectedDrum);
				this->setValue(midiDrum->note);
			}
		}
	}

	void writeCurrentValue() override {
		Output* output = getCurrentOutput();
		if (output && output->type == OutputType::KIT) {
			auto* kit = static_cast<Kit*>(output);
			if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
				auto* midiDrum = static_cast<MIDIDrum*>(kit->selectedDrum);
				midiDrum->note = this->getValue();
			}
		}
	}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) override {
		Output* output = getCurrentOutput();
		return (output && output->type == OutputType::KIT);
	}

	// Override to show note name in horizontal menu
	void renderInHorizontalMenu(const HorizontalMenuSlotParams& slot) override {
		using namespace deluge::hid::display;
		oled_canvas::Canvas& image = OLED::main;

		int32_t noteValue = getValue();

		// Note names array (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
		constexpr const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

		// Calculate note name and octave
		int32_t noteName = noteValue % 12;
		int32_t octave = (noteValue / 12) - 2; // MIDI octave convention (C-2 to G9)

		// Build note string (e.g., "C3", "F#4")
		char noteStr[8];
		const char* note = noteNames[noteName];

		// Copy note name and add octave
		int idx = 0;
		while (note[idx] != '\0') {
			noteStr[idx] = note[idx];
			idx++;
		}

		// Add octave
		if (octave < 0) {
			noteStr[idx++] = '-';
			noteStr[idx++] = '0' + (-octave);
		}
		else if (octave < 10) {
			noteStr[idx++] = '0' + octave;
		}
		else {
			// For octave 10+, use two digits
			noteStr[idx++] = '1';
			noteStr[idx++] = '0' + (octave - 10);
		}
		noteStr[idx] = '\0';

		// Draw note name centered
		image.drawStringCentered(noteStr, slot.start_x, slot.start_y + kHorizontalMenuSlotYOffset, kTextTitleSpacingX,
		                         kTextTitleSizeY, slot.width);
	}

	int32_t getMinValue() const override { return 0; }
	int32_t getMaxValue() const override { return 127; }
};

} // namespace deluge::gui::menu_item::midi::sound
