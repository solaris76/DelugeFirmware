/*
 * Copyright © 2026 Synthstrom Audible Limited
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
#include "gui/l10n/l10n.h"
#include "gui/menu_item/menu_item.h"
#include "hid/display/display.h"
#include "io/midi/midi_engine.h"
#include "model/instrument/midi_instrument.h"
#include "model/output.h"
#include "model/song/song.h"

namespace deluge::gui::menu_item::song {
class AllNotesOff final : public MenuItem {
public:
	using MenuItem::MenuItem;

	MenuItem* selectButtonPress() override {
		if (currentSong) {
			currentSong->stopAllMIDIAndGateNotesPlaying();
			for (Output* output = currentSong->firstOutput; output; output = output->next) {
				if (output->type == OutputType::MIDI_OUT) {
					static_cast<MIDIInstrument*>(output)->allNotesOff();
				}
			}
		}
		midiEngine.sendAllNotesOffAllChannels();
		display->displayPopup(l10n::get(l10n::String::STRING_FOR_ALL_NOTES_OFF));
		return NO_NAVIGATION;
	}

	bool shouldEnterSubmenu() override { return false; }
};
} // namespace deluge::gui::menu_item::song
