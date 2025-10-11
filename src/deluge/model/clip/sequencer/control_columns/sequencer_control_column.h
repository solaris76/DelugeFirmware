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

#include "definitions_cxx.hpp"
#include "gui/colour/rgb.h"
#include "io/midi/midi_device.h"
#include <array>
#include <cstdint>

class Serializer;
class Deserializer;

namespace deluge::model::clip::sequencer {

/**
 * Control types that can be assigned to sequencer control column pads.
 * Each type affects how the sequencer plays back notes.
 */
enum class ControlType : uint8_t {
	NONE = 0,          // Empty pad (no control assigned)
	CLOCK_DIVISION,    // Clock speed multiplier: 1/4, 1/2, 1x, 2x, 4x, 8x, 16x, 32x
	OCTAVE,            // Octave shift: -2, -1, 0, +1, +2, +3, +4, +5
	TRANSPOSE,         // Semitone transpose: -12 to +12
	PROBABILITY,       // Note probability: 0-100%
	VELOCITY,          // Velocity multiplier: 50-150%
	GATE_LENGTH,       // Gate length multiplier: 25%, 50%, 75%, 100%, 125%, 150%, 200%
	SWING,             // Swing amount: 50-75%
	MAX_TYPES
};

/**
 * Pad behavior mode - determines how pad activation works.
 */
enum class PadMode : uint8_t {
	TOGGLE = 0,        // Press to activate, press again to deactivate (latching)
	MOMENTARY = 1,     // Hold to activate, release to deactivate (momentary)
};

/**
 * Configuration for a single control column pad.
 * Stores the control type, value, mode, and activation state.
 */
struct PadConfig {
	ControlType type = ControlType::NONE;
	int32_t value = 0;              // Control-specific value
	PadMode mode = PadMode::TOGGLE;
	bool isActive = false;          // Current activation state (for toggle mode)

	// Get the color for rendering based on control type
	RGB getColor() const;
	
	// Get display name for this control type
	const char* getName() const;
	
	// Get value range for this control type
	int32_t getMinValue() const;
	int32_t getMaxValue() const;
	int32_t getDefaultValue() const;
	
	// Format value for OLED display (e.g., "2x", "+7", "50%")
	void formatValue(char* buffer, size_t bufferSize) const;

	// Cycle to next control type (for pad press cycling)
	void cycleControlType();

	// Serialization
	void writeToFile(Serializer& writer, int32_t padY) const;
	void readFromFile(Deserializer& reader);
};

/**
 * A single control column (left or right sidebar) with 8 configurable pads.
 * Each pad can be assigned a different control type, value, and mode.
 * 
 * This class manages:
 * - Per-pad configuration storage
 * - Rendering the column with color-coded pads
 * - Handling pad presses and encoder input
 * - Calculating active control values for playback
 */
class SequencerControlColumn {
public:
	SequencerControlColumn() = default;

	/**
	 * Render the control column on the grid.
	 * 
	 * @param image RGB array for LED output
	 * @param column Column index (16 or 17 for left/right sidebar)
	 * @param heldPad Y position of currently held pad (-1 if none)
	 */
	void renderColumn(RGB image[][kDisplayWidth + kSideBarWidth], 
	                  int32_t column, 
	                  int8_t heldPad) const;

	/**
	 * Handle pad press/release.
	 * - Press: cycle control type (if not configured) or activate (if configured)
	 * - Release: deactivate (if momentary) or do nothing (if toggle)
	 * - Shift+press: clear pad configuration
	 * 
	 * @param padY Y position of pad (0-7)
	 * @param pressed True if pressed, false if released
	 * @param shiftPressed True if SHIFT button is held
	 * @return True if state changed (needs redraw)
	 */
	bool handlePad(int8_t padY, bool pressed, bool shiftPressed);

	/**
	 * Handle encoder input while pad is held.
	 * - Turn encoder: adjust value
	 * - Press encoder: toggle between toggle/momentary mode
	 * 
	 * @param heldPad Y position of held pad (0-7)
	 * @param offset Encoder rotation amount (positive = clockwise)
	 * @param encoderPressed True if encoder button was pressed
	 * @return True if handled
	 */
	bool handleEncoder(int8_t heldPad, int32_t offset, bool encoderPressed);

	/**
	 * Get the accumulated effect of all active pads in this column.
	 * Used during playback to modify sequencer behavior.
	 */
	struct ActiveControls {
		float clockDivMultiplier{1.0f};   // Multiply clock speed (1.0 = normal)
		int32_t octaveShift{0};           // Octave offset (0 = no shift)
		int32_t transpose{0};             // Semitone offset (0 = no transpose)
		int32_t probability{100};         // Note probability 0-100 (100 = always play)
		float velocityMultiplier{1.0f};   // Velocity scale (1.0 = normal)
		float gateLengthMultiplier{1.0f}; // Gate length scale (1.0 = normal)
		float swing{0.5f};                // Swing amount (0.5 = no swing, 0.75 = max)
	};
	
	ActiveControls getActiveControls() const;

	/**
	 * Get pad configuration (for inspection/testing).
	 */
	const PadConfig& getPadConfig(int8_t padY) const {
		if (padY < 0 || padY >= kDisplayHeight) {
			static PadConfig empty;
			return empty;
		}
		return pads_[padY];
	}

	/**
	 * Set pad configuration (for testing/initialization).
	 */
	void setPadConfig(int8_t padY, const PadConfig& config) {
		if (padY >= 0 && padY < kDisplayHeight) {
			pads_[padY] = config;
		}
	}

	// Serialization
	void writeToFile(Serializer& writer, const char* tagName) const;
	void readFromFile(Deserializer& reader);

private:
	std::array<PadConfig, kDisplayHeight> pads_; // 8 pads (y0-y7)
	
	// Helper: Get brightness for pad rendering
	uint8_t getPadBrightness(int8_t padY, bool isHeld) const;
};

} // namespace deluge::model::clip::sequencer

