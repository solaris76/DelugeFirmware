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

#include "io/midi/sysex/sysex_common.h"
#include "storage/smsysex.h"

namespace SysexCommon {

SubscriberList::SubscriberList() : count(0) {
	for (uint32_t i = 0; i < MAX_SYSEX_SUBSCRIBERS; ++i) {
		slots[i] = nullptr;
	}
}

SubscriberList::AddResult SubscriberList::add(MIDICable* cable) {
	if (!cable) {
		return AddResult::FULL;
	}

	for (uint32_t i = 0; i < count; ++i) {
		if (slots[i] == cable) {
			return AddResult::ALREADY_PRESENT;
		}
	}

	if (count >= MAX_SYSEX_SUBSCRIBERS) {
		return AddResult::FULL;
	}

	slots[count++] = cable;
	return AddResult::ADDED;
}

bool SubscriberList::remove(MIDICable* cable) {
	for (uint32_t i = 0; i < count; ++i) {
		if (slots[i] == cable) {
			for (uint32_t j = i; j < count - 1; ++j) {
				slots[j] = slots[j + 1];
			}
			slots[count - 1] = nullptr;
			count--;
			return true;
		}
	}
	return false;
}

bool SubscriberList::contains(MIDICable const* cable) const {
	for (uint32_t i = 0; i < count; ++i) {
		if (slots[i] == cable) {
			return true;
		}
	}
	return false;
}

void startResponse(JsonSerializer& writer, JsonDeserializer& reader, const char* tagName) {
	writer.reset();
	writer.setMemoryBased();

	smSysex::startReply(writer, reader);
	writer.writeOpeningTag(tagName, false, true);
}

void sendResponse(MIDICable& cable, JsonSerializer& writer) {
	writer.closeTag(true);
	smSysex::sendMsg(cable, writer);
}

void writeStatus(JsonSerializer& writer, const char* status, const char* message) {
	if (status) {
		writer.writeAttribute("status", status);
	}
	if (message) {
		writer.writeAttribute("message", message);
	}
}

} // namespace SysexCommon
