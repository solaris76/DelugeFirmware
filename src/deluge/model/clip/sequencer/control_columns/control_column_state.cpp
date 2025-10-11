/*
 * Copyright © 2024 Synthstrom Audible Deluge Firmware.
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

#include "model/clip/sequencer/control_columns/control_column_state.h"
#include "storage/storage_manager.h"
#include <cstring>

namespace deluge::model::clip::sequencer {

void ControlColumnState::readFromFile(Deserializer& reader) {
	char const* tagName;
	reader.match('{');

	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "leftControlColumn")) {
			leftColumn.readFromFile(reader);
		}
		else if (!strcmp(tagName, "rightControlColumn")) {
			rightColumn.readFromFile(reader);
		}
		else {
			reader.exitTag(tagName);
		}
	}

	reader.match('}');
}

} // namespace deluge::model::clip::sequencer

