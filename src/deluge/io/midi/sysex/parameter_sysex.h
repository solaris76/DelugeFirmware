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

#include <cstdint>

class JsonDeserializer;
class MIDICable;

namespace ParameterSysex {

// Get all parameters for the currently selected sound/clip
void getParameters(MIDICable& cable, JsonDeserializer& reader);

// Set a single parameter value
void setParameter(MIDICable& cable, JsonDeserializer& reader);

// Get a single parameter value
void getParameter(MIDICable& cable, JsonDeserializer& reader);

// Get all patch cables (modulation routing)
void getPatchCables(MIDICable& cable, JsonDeserializer& reader);

// Set a patch cable (modulation routing)
void setPatchCable(MIDICable& cable, JsonDeserializer& reader);

// Subscribe to parameter changes for real-time updates
void subscribeParameters(MIDICable& cable, JsonDeserializer& reader);

// Unsubscribe from parameter changes
void unsubscribeParameters(MIDICable& cable, JsonDeserializer& reader);

// Notify subscribers of parameter change (called internally)
void notifyParameterChanged(int32_t paramKind, int32_t paramId, int32_t value);

} // namespace ParameterSysex
