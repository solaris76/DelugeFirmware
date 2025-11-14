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

#include <cstdint>

class JsonDeserializer;
class JsonSerializer;
class MIDICable;

namespace SysexCommon {

constexpr uint32_t MAX_SYSEX_SUBSCRIBERS = 4;

class SubscriberList {
public:
	SubscriberList();

	enum class AddResult { ADDED, ALREADY_PRESENT, FULL };

	AddResult add(MIDICable* cable);
	bool remove(MIDICable* cable);
	bool contains(MIDICable const* cable) const;
	uint32_t size() const { return count; }

	template <typename Callback>
	void forEach(Callback&& callback) const {
		for (uint32_t i = 0; i < count; ++i) {
			MIDICable* cable = slots[i];
			if (cable) {
				callback(*cable);
			}
		}
	}

private:
	MIDICable* slots[MAX_SYSEX_SUBSCRIBERS];
	uint32_t count;
};

void startResponse(JsonSerializer& writer, JsonDeserializer& reader, const char* tagName);
void sendResponse(MIDICable& cable, JsonSerializer& writer);
void writeStatus(JsonSerializer& writer, const char* status, const char* message = nullptr);

} // namespace SysexCommon
