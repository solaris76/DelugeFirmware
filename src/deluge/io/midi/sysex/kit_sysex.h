/*
 * Copyright © 2024 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Deluge Firmware is free software: you can redistribute it and/or modify it under the
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

#include "definitions_cxx.hpp"

class MIDICable;
class JsonDeserializer;

namespace KitSysex {

// Get information about the current kit
void getKitInfo(MIDICable& cable, JsonDeserializer& reader);

// Get list of all drums in the kit
void getKitDrums(MIDICable& cable, JsonDeserializer& reader);

// Get detailed information about a specific drum by index
void getDrumInfo(MIDICable& cable, JsonDeserializer& reader);

// Add a new drum to the kit
void addDrum(MIDICable& cable, JsonDeserializer& reader);

// Remove a drum from the kit by index
void removeDrum(MIDICable& cable, JsonDeserializer& reader);

// Set drum properties (name, MIDI input, etc.)
void setDrumProperty(MIDICable& cable, JsonDeserializer& reader);

// Set sample file for a SoundDrum
void setDrumSample(MIDICable& cable, JsonDeserializer& reader);

// Get all parameters for a drum
void getDrumParameters(MIDICable& cable, JsonDeserializer& reader);

// Set a parameter for a drum
void setDrumParameter(MIDICable& cable, JsonDeserializer& reader);

// Subscribe / unsubscribe to drum parameter changes
void subscribeDrumParameters(MIDICable& cable, JsonDeserializer& reader);
void unsubscribeDrumParameters(MIDICable& cable, JsonDeserializer& reader);

// Create a new empty kit
void createKit(MIDICable& cable, JsonDeserializer& reader);

// Load an existing kit by name
void loadKit(MIDICable& cable, JsonDeserializer& reader);

// Save the current kit
void saveKit(MIDICable& cable, JsonDeserializer& reader);

// Subscribe to kit changes
void subscribeKit(MIDICable& cable, JsonDeserializer& reader);

// Unsubscribe from kit changes
void unsubscribeKit(MIDICable& cable, JsonDeserializer& reader);

// Notification functions (called when kit changes)
void notifyDrumAdded(int32_t drumIndex);
void notifyDrumRemoved(int32_t drumIndex);
void notifyDrumChanged(int32_t drumIndex);
void notifyKitChanged();
void notifyDrumParameterChanged(int32_t drumIndex, char const* paramName, int32_t value);

} // namespace KitSysex
