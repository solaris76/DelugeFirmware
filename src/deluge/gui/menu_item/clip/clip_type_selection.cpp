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

#include "gui/menu_item/clip/clip_type_selection.h"
#include "gui/ui/sound_editor.h"
#include "gui/l10n/l10n.h"

namespace deluge::gui::menu_item::clip {

deluge::vector<std::string_view> ClipTypeSelection::getOptions(OptType optType) {
	deluge::vector<std::string_view> options;

	// Add available clip types for Synth/MIDI/CV tracks
	for (const auto& stringId : clipTypeOptions) {
		options.push_back(l10n::getView(stringId));
	}

	return options;
}

void ClipTypeSelection::readCurrentValue() {
	// For now, always default to Piano Roll (index 0)
	// TODO: Read actual mode from clip when sequencer system is integrated
	this->setValue(0);
}

void ClipTypeSelection::writeCurrentValue() {
	// For now, just acknowledge the selection
	// TODO: Actually switch the clip's sequencer mode
	int32_t selectedMode = this->getValue();

	// Future implementation will connect to sequencer mode system
	// For now, the selection just shows which mode was chosen
}

} // namespace deluge::gui::menu_item::clip
