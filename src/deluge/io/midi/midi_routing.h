/*
 * Copyright © 2024 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 */

#pragma once

#include <cstdint>

namespace deluge::io::midi {

// Clip/song matching: omit outputDevice from XML → legacy channel+suffix match only.
static constexpr uint8_t kMIDIOutputDeviceMatchUnspecified = 255;

// Output device index used by MIDIInstrument / MIDIDrum / MidiEngine:
// 0 = all connected outputs (legacy behaviour)
// 1 = DIN
// 2 = USB device-mode cable 1 (computer)
// 3 = USB device-mode cable 2
// 4+ = USB host-mode cables (hostedMIDIDevices[i] at index i + 4).
// Multi-port boxes (H4MIDI, MRCC 880, …) appear as one menu item per virtual cable.

} // namespace deluge::io::midi
