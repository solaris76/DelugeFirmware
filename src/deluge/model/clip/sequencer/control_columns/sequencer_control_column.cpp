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

#include "model/clip/sequencer/control_columns/sequencer_control_column.h"
#include "hid/display/display.h"
#include "storage/storage_manager.h"
#include "util/functions.h"

namespace deluge::model::clip::sequencer {

// ========== PadConfig Implementation ==========

RGB PadConfig::getColor() const {
	switch (type) {
	case ControlType::NONE:
		return RGB{16, 16, 16}; // Dim gray
	case ControlType::CLOCK_DIVISION:
		return RGB{255, 0, 0}; // Red
	case ControlType::OCTAVE:
		return RGB{0, 0, 255}; // Blue
	case ControlType::TRANSPOSE:
		return RGB{0, 255, 255}; // Cyan
	case ControlType::PROBABILITY:
		return RGB{255, 255, 0}; // Yellow
	case ControlType::VELOCITY:
		return RGB{255, 0, 255}; // Magenta
	case ControlType::GATE_LENGTH:
		return RGB{255, 128, 0}; // Orange
	case ControlType::SWING:
		return RGB{255, 255, 255}; // White
	default:
		return RGB{32, 32, 32}; // Gray
	}
}

const char* PadConfig::getName() const {
	switch (type) {
	case ControlType::NONE:
		return "EMPTY";
	case ControlType::CLOCK_DIVISION:
		return "CLOCK DIV";
	case ControlType::OCTAVE:
		return "OCTAVE";
	case ControlType::TRANSPOSE:
		return "TRANSPOSE";
	case ControlType::PROBABILITY:
		return "PROBABILITY";
	case ControlType::VELOCITY:
		return "VELOCITY";
	case ControlType::GATE_LENGTH:
		return "GATE LENGTH";
	case ControlType::SWING:
		return "SWING";
	default:
		return "UNKNOWN";
	}
}

int32_t PadConfig::getMinValue() const {
	switch (type) {
	case ControlType::CLOCK_DIVISION:
		return 0; // Index into clock division array
	case ControlType::OCTAVE:
		return -2;
	case ControlType::TRANSPOSE:
		return -12;
	case ControlType::PROBABILITY:
		return 0;
	case ControlType::VELOCITY:
		return 50;
	case ControlType::GATE_LENGTH:
		return 25;
	case ControlType::SWING:
		return 50;
	default:
		return 0;
	}
}

int32_t PadConfig::getMaxValue() const {
	switch (type) {
	case ControlType::CLOCK_DIVISION:
		return 7; // 8 clock divisions (0-7)
	case ControlType::OCTAVE:
		return 5;
	case ControlType::TRANSPOSE:
		return 12;
	case ControlType::PROBABILITY:
		return 100;
	case ControlType::VELOCITY:
		return 150;
	case ControlType::GATE_LENGTH:
		return 200;
	case ControlType::SWING:
		return 75;
	default:
		return 0;
	}
}

int32_t PadConfig::getDefaultValue() const {
	switch (type) {
	case ControlType::CLOCK_DIVISION:
		return 2; // 1x (no change)
	case ControlType::OCTAVE:
		return 0; // No shift
	case ControlType::TRANSPOSE:
		return 0; // No transpose
	case ControlType::PROBABILITY:
		return 100; // Always play
	case ControlType::VELOCITY:
		return 100; // 100%
	case ControlType::GATE_LENGTH:
		return 100; // 100%
	case ControlType::SWING:
		return 50; // No swing
	default:
		return 0;
	}
}

void PadConfig::formatValue(char* buffer, size_t bufferSize) const {
	// For display popup, just use simple strings
	// The OLED will show the value automatically
	buffer[0] = '\0'; // Empty string for now - values shown via intToString elsewhere
}

void PadConfig::cycleControlType() {
	// Cycle through control types
	int32_t nextType = static_cast<int32_t>(type) + 1;
	if (nextType >= static_cast<int32_t>(ControlType::MAX_TYPES)) {
		nextType = static_cast<int32_t>(ControlType::NONE);
	}
	type = static_cast<ControlType>(nextType);

	// Reset to default value when type changes
	value = getDefaultValue();
	isActive = false;
}

void PadConfig::writeToFile(Serializer& writer, int32_t padY) const {
	// Only write if pad is configured (not NONE)
	if (type == ControlType::NONE) {
		return;
	}

	writer.writeOpeningTagBeginning("pad", true);
	writer.writeAttribute("y", padY);

	// Write control type as string
	const char* typeName = "";
	switch (type) {
	case ControlType::CLOCK_DIVISION: typeName = "clock_div"; break;
	case ControlType::OCTAVE: typeName = "octave"; break;
	case ControlType::TRANSPOSE: typeName = "transpose"; break;
	case ControlType::PROBABILITY: typeName = "probability"; break;
	case ControlType::VELOCITY: typeName = "velocity"; break;
	case ControlType::GATE_LENGTH: typeName = "gate_length"; break;
	case ControlType::SWING: typeName = "swing"; break;
	default: typeName = "none"; break;
	}
	writer.writeAttribute("type", (char*)typeName);
	writer.writeAttribute("value", value);
	writer.writeAttribute("mode", mode == PadMode::TOGGLE ? "toggle" : "momentary");
	writer.writeAttribute("active", isActive ? "true" : "false");
	writer.closeTag(true);
}

void PadConfig::readFromFile(Deserializer& reader) {
	char const* tagName;
	reader.match('{');

	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "type")) {
			const char* typeStr = reader.readTagOrAttributeValue();
			if (!strcmp(typeStr, "clock_div")) type = ControlType::CLOCK_DIVISION;
			else if (!strcmp(typeStr, "octave")) type = ControlType::OCTAVE;
			else if (!strcmp(typeStr, "transpose")) type = ControlType::TRANSPOSE;
			else if (!strcmp(typeStr, "probability")) type = ControlType::PROBABILITY;
			else if (!strcmp(typeStr, "velocity")) type = ControlType::VELOCITY;
			else if (!strcmp(typeStr, "gate_length")) type = ControlType::GATE_LENGTH;
			else if (!strcmp(typeStr, "swing")) type = ControlType::SWING;
			else type = ControlType::NONE;
			reader.exitTag("type");
		}
		else if (!strcmp(tagName, "value")) {
			value = reader.readTagOrAttributeValueInt();
			reader.exitTag("value");
		}
		else if (!strcmp(tagName, "mode")) {
			const char* modeStr = reader.readTagOrAttributeValue();
			mode = (!strcmp(modeStr, "momentary")) ? PadMode::MOMENTARY : PadMode::TOGGLE;
			reader.exitTag("mode");
		}
		else if (!strcmp(tagName, "active")) {
			const char* activeStr = reader.readTagOrAttributeValue();
			isActive = (!strcmp(activeStr, "true"));
			reader.exitTag("active");
		}
		else if (!strcmp(tagName, "y")) {
			// Skip y attribute - we handle it externally
			reader.readTagOrAttributeValueInt();
			reader.exitTag("y");
		}
		else {
			reader.exitTag(tagName);
		}
	}

	reader.match('}');
}

// ========== SequencerControlColumn Implementation ==========

void SequencerControlColumn::renderColumn(RGB image[][kDisplayWidth + kSideBarWidth],
                                          int32_t column,
                                          int8_t heldPad) const {
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		const PadConfig& pad = pads_[y];
		RGB color = pad.getColor();

		// Adjust brightness based on state
		uint8_t brightness = getPadBrightness(y, y == heldPad);

		// Apply brightness scaling
		image[y][column] = RGB{
			static_cast<uint8_t>((color.r * brightness) / 255),
			static_cast<uint8_t>((color.g * brightness) / 255),
			static_cast<uint8_t>((color.b * brightness) / 255)
		};
	}
}

uint8_t SequencerControlColumn::getPadBrightness(int8_t padY, bool isHeld) const {
	const PadConfig& pad = pads_[padY];

	if (pad.type == ControlType::NONE) {
		return 16; // Dim for empty pads
	}

	if (isHeld) {
		return 255; // Full brightness when adjusting
	}

	if (pad.isActive) {
		if (pad.mode == PadMode::TOGGLE) {
			return 255; // Full brightness for active toggle
		} else {
			return 128; // Medium for active momentary (shouldn't normally be active when not held)
		}
	}

	// Configured but inactive
	if (pad.mode == PadMode::MOMENTARY) {
		return 64; // Dimmer for momentary (not currently held)
	}

	return 128; // Medium for inactive toggle
}

bool SequencerControlColumn::handlePad(int8_t padY, bool pressed, bool shiftPressed) {
	if (padY < 0 || padY >= kDisplayHeight) {
		return false;
	}

	PadConfig& pad = pads_[padY];

	// Shift + press = clear pad
	if (pressed && shiftPressed) {
		if (pad.type != ControlType::NONE) {
			pad.type = ControlType::NONE;
			pad.value = 0;
			pad.isActive = false;
			if (display) {
				display->displayPopup("CLEARED");
			}
			return true;
		}
		return false;
	}

	if (pressed) {
		// Short press: cycle to next control type (always cycles)
		pad.cycleControlType();

		// Display the new type
		if (display) {
			display->displayPopup(pad.getName());
		}
		return true;
	}
	else {
		// Release - handled by momentary logic in encoder
		return false;
	}

	return false;
}

bool SequencerControlColumn::handleEncoder(int8_t heldPad, int32_t offset, bool encoderPressed) {
	if (heldPad < 0 || heldPad >= kDisplayHeight) {
		return false;
	}

	PadConfig& pad = pads_[heldPad];

	// Can't adjust empty pads
	if (pad.type == ControlType::NONE) {
		return false;
	}

	// Encoder press (without turn): toggle between momentary/toggle mode
	if (encoderPressed && offset == 0) {
		if (pad.mode == PadMode::TOGGLE) {
			pad.mode = PadMode::MOMENTARY;
			if (display) {
				display->displayPopup("MOMENTARY");
			}
		}
		else {
			pad.mode = PadMode::TOGGLE;
			if (display) {
				display->displayPopup("TOGGLE");
			}
		}
		return true;
	}

	// Encoder turn: adjust value
	if (offset != 0) {
		int32_t newValue = pad.value + offset;

		// Clamp to valid range
		int32_t minVal = pad.getMinValue();
		int32_t maxVal = pad.getMaxValue();
		if (newValue < minVal) newValue = minVal;
		if (newValue > maxVal) newValue = maxVal;

		if (newValue != pad.value) {
			pad.value = newValue;

			// Activate the pad when value is set
			if (pad.mode == PadMode::TOGGLE) {
				pad.isActive = true; // Auto-activate on value change
			}

			// Display new value directly
			if (display) {
				display->displayPopup(pad.getName());
				display->setNextTransitionDirection(1);
				display->displayPopup(newValue);
			}
			return true;
		}
	}

	return false;
}

SequencerControlColumn::ActiveControls SequencerControlColumn::getActiveControls() const {
	ActiveControls controls;

	// Clock divisions: 1/4, 1/2, 1x, 2x, 4x, 8x, 16x, 32x
	static const float clockDivValues[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f};

	// Accumulate effects from all active pads
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		const PadConfig& pad = pads_[y];

		if (!pad.isActive || pad.type == ControlType::NONE) {
			continue;
		}

		switch (pad.type) {
		case ControlType::CLOCK_DIVISION: {
			int32_t idx = pad.value;
			if (idx >= 0 && idx < 8) {
				controls.clockDivMultiplier *= clockDivValues[idx];
			}
			break;
		}
		case ControlType::OCTAVE:
			controls.octaveShift += pad.value;
			break;
		case ControlType::TRANSPOSE:
			controls.transpose += pad.value;
			break;
		case ControlType::PROBABILITY:
			// Use most restrictive probability
			if (pad.value < controls.probability) {
				controls.probability = pad.value;
			}
			break;
		case ControlType::VELOCITY:
			controls.velocityMultiplier *= (pad.value / 100.0f);
			break;
		case ControlType::GATE_LENGTH:
			controls.gateLengthMultiplier *= (pad.value / 100.0f);
			break;
		case ControlType::SWING:
			// Average swing values (0.5 = no swing, 0.75 = max)
			controls.swing = (controls.swing + (pad.value / 100.0f)) / 2.0f;
			break;
		default:
			break;
		}
	}

	return controls;
}

void SequencerControlColumn::writeToFile(Serializer& writer, const char* tagName) const {
	// Count configured pads
	int32_t configuredPads = 0;
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		if (pads_[y].type != ControlType::NONE) {
			configuredPads++;
		}
	}

	// Don't write if no pads configured
	if (configuredPads == 0) {
		return;
	}

	writer.writeOpeningTagBeginning(tagName);
	writer.closeTag();

	// Write each configured pad
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		pads_[y].writeToFile(writer, y);
	}

	writer.writeClosingTag(tagName);
}

void SequencerControlColumn::readFromFile(Deserializer& reader) {
	// Clear all pads first
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		pads_[y] = PadConfig();
	}

	char const* tagName;
	reader.match('{');

	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "pad")) {
			// Read pad configuration
			PadConfig tempPad;
			int32_t padY = -1;

			reader.match('{');
			while (*(tagName = reader.readNextTagOrAttributeName())) {
				if (!strcmp(tagName, "y")) {
					padY = reader.readTagOrAttributeValueInt();
					reader.exitTag("y");
				}
				else {
					// Let PadConfig handle other attributes
					reader.exitTag(tagName);
				}
			}
			reader.match('}');

			// Re-read the pad to get all attributes
			// (This is a bit awkward, but matches the chord memory pattern)
			// TODO: Could be refactored to read in one pass

		}
		else {
			reader.exitTag(tagName);
		}
	}

	reader.match('}');
}

} // namespace deluge::model::clip::sequencer

