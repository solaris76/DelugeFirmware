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
#include "model/clip/instrument_clip.h"
#include "model/clip/sequencer/sequencer_mode_manager.h"
#include "model/song/song.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"

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
	// Get current clip and check its sequencer mode
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (clip && clip->hasSequencerMode()) {
		// Check which mode is active
		const std::string& modeName = clip->getSequencerModeName();
		if (modeName == "generative_test") {
			this->setValue(1); // GENERATIVE 1
		} else {
			this->setValue(0); // Default to PIANO ROLL
		}
	} else {
		this->setValue(0); // PIANO ROLL (default/linear mode)
	}
}

void ClipTypeSelection::writeCurrentValue() {
	// Apply the selected mode to the current clip
	int32_t selectedMode = this->getValue();
	InstrumentClip* clip = getCurrentInstrumentClip();

	if (clip) {
		if (selectedMode == 0) {
			// PIANO ROLL - clear sequencer mode (back to linear)
			clip->clearSequencerMode();
		} else if (selectedMode == 1) {
			// GENERATIVE 1 - set test sequencer mode
			clip->setSequencerMode("generative_test");
		}

		// Recalculate colours for instrument clip view (needed when going back to normal mode)
		instrumentClipView.recalculateColours();

		// Trigger UI refresh to show the new sequencer mode visuals
		uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0);
	}
}

} // namespace deluge::gui::menu_item::clip
