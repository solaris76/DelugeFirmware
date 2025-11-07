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

class DrumChannel final : public Integer {
public:
	using Integer::Integer;

	void readCurrentValue() override {
		Output* output = getCurrentOutput();
		if (output && output->type == OutputType::KIT) {
			auto* kit = static_cast<Kit*>(output);
			if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
				auto* midiDrum = static_cast<MIDIDrum*>(kit->selectedDrum);
				// Display as 1-16 (stored internally as 0-15)
				this->setValue(midiDrum->channel + 1);
				return;
			}
		}
		this->setValue(1); // Default
	}

	void writeCurrentValue() override {
		Output* output = getCurrentOutput();
		if (output && output->type == OutputType::KIT) {
			auto* kit = static_cast<Kit*>(output);
			if (kit->selectedDrum && kit->selectedDrum->type == DrumType::MIDI) {
				auto* midiDrum = static_cast<MIDIDrum*>(kit->selectedDrum);
				// Convert from displayed 1-16 to internal 0-15
				int32_t value = this->getValue();
				if (value >= 1 && value <= 16) {
					midiDrum->channel = value - 1;
				}
			}
		}
	}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) override {
		Output* output = getCurrentOutput();
		return (output && output->type == OutputType::KIT);
	}

	// Override to show channel number in horizontal menu (same style as note name)
	void renderInHorizontalMenu(const HorizontalMenuSlotParams& slot) override {
		using namespace deluge::hid::display;
		oled_canvas::Canvas& image = OLED::main;

		int32_t channelValue = getValue();

		// Build channel string (e.g., "1", "16")
		char channelStr[8] = {0};
		intToString(channelValue, channelStr, 1);

		// Draw centered (aligned with note name)
		image.drawStringCentered(channelStr, slot.start_x, slot.start_y + kHorizontalMenuSlotYOffset,
		                         kTextTitleSpacingX, kTextTitleSizeY, slot.width);
	}

	int32_t getMinValue() const override { return 1; } // Display as 1-16
	int32_t getMaxValue() const override { return 16; }
};

} // namespace deluge::gui::menu_item::midi::sound
