/*
 * Copyright (c) 2024 Synthstrom Audible Limited
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

#include "dsp/zone_param.hpp" // For ZoneBasedParam
#include "gui/menu_item/automation/automation.h"
#include "gui/menu_item/decimal.h"
#include "gui/menu_item/menu_item_with_cc_learning.h"
#include "gui/menu_item/source_selection/regular.h"
#include "gui/menu_item/velocity_encoder.h" // VelocityEncoder and zone render helpers
#include "gui/ui/sound_editor.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "model/model_stack.h"
#include "modulation/params/param.h"
#include "modulation/params/param_descriptor.h"
#include "modulation/params/param_set.h"
#include <cstdint>

namespace params = deluge::modulation::params;

namespace deluge::gui::menu_item {

// Re-export ZoneBasedParam from dsp namespace for convenience
using dsp::ZoneBasedParam;

/// Compute bit shift for resolution (31 - log2(resolution))
/// Only valid for power-of-2 resolutions
constexpr int32_t resolutionToShift(int32_t resolution) {
	// Count trailing zeros to get log2
	int32_t shift = 31;
	while (resolution > 1) {
		resolution >>= 1;
		shift--;
	}
	return shift;
}

/// Convert q31 param value to menu value with given resolution
/// Uses proper scaling to avoid drift between menu positions and zone boundaries
template <int32_t RESOLUTION>
inline int32_t paramToMenuValue(q31_t value) {
	// Scale q31 [0, ONE_Q31] to menu [0, RESOLUTION]
	// Use quotient + remainder to avoid 64-bit: value / (ONE_Q31/RES) with correction
	constexpr int32_t kOneQ31 = 2147483647;
	constexpr int32_t kQuotient = kOneQ31 / RESOLUTION; // Floor division
	// Simple division by quotient, then clamp (slight over-estimate is OK for display)
	int32_t result = value / kQuotient;
	return std::min(result, RESOLUTION);
}

/// Convert menu value to q31 param value with given resolution
/// Uses proper scaling to align menu positions with zone boundaries
template <int32_t RESOLUTION>
inline q31_t menuValueToParam(int32_t menuValue) {
	if (menuValue >= RESOLUTION) {
		return 2147483647; // ONE_Q31
	}
	// Exact 32-bit formula: menuValue * quotient + (menuValue * remainder) / RESOLUTION
	// where quotient = ONE_Q31 / RESOLUTION, remainder = ONE_Q31 % RESOLUTION
	// This gives exact results without 64-bit math
	constexpr int32_t kOneQ31 = 2147483647;
	constexpr int32_t kQuotient = kOneQ31 / RESOLUTION;
	constexpr int32_t kRemainder = kOneQ31 % RESOLUTION;
	return menuValue * kQuotient + (menuValue * kRemainder) / RESOLUTION;
}

// Legacy aliases for 1024-step resolution (used by existing code)
constexpr int32_t kZoneHighResSteps = 1024;
inline int32_t zoneParamToMenuValue(q31_t value) {
	return paramToMenuValue<1024>(value);
}
inline q31_t zoneMenuValueToParam(int32_t menuValue) {
	return menuValueToParam<1024>(menuValue);
}

/**
 * Base class for zone-based high-resolution menu items
 *
 * Provides:
 * - Configurable resolution with velocity-sensitive encoder
 * - Zone name rendering (OLED and horizontal menu)
 * - Common display value scaling (0-50)
 * - Optional auto-wrap mode: when turning past zone boundaries, wraps and adjusts phase offset
 *
 * Derived classes must implement:
 * - readCurrentValue() / writeCurrentValue() for storage
 * - getZoneName(int32_t) for zone labels
 * - isRelevant() if gating is needed
 *
 * For auto-wrap support, derived classes should also implement:
 * - supportsAutoWrap() returning true
 * - getPhaseOffset() / setPhaseOffset() for per-knob phase offset
 *
 * @tparam NUM_ZONES Number of zones (typically 8)
 * @tparam RESOLUTION Encoder steps (typically 1024 for zone params)
 */
template <int32_t NUM_ZONES = 8, int32_t RESOLUTION = 1024>
class ZoneBasedMenuItem : public DecimalWithoutScrolling {
public:
	using DecimalWithoutScrolling::DecimalWithoutScrolling;

	/// Override to provide zone name for each index (0 to NUM_ZONES-1)
	[[nodiscard]] virtual const char* getZoneName(int32_t zoneIndex) const = 0;

	/// Override to provide 2-char abbreviation for 7-segment display (default: first 2 chars of zone name)
	[[nodiscard]] virtual const char* getShortZoneName(int32_t zoneIndex) const { return getZoneName(zoneIndex); }

	/// Override to enable auto-wrap mode for this zone param
	/// When true, turning past boundaries wraps and adjusts per-knob phase offset
	[[nodiscard]] virtual bool supportsAutoWrap() const { return false; }

	/// Override to get per-knob phase offset (used by auto-wrap)
	/// Scale: 1.0 = one full zone cycle (RESOLUTION steps)
	[[nodiscard]] virtual float getPhaseOffset() const { return 0.0f; }

	/// Override to set per-knob phase offset (used by auto-wrap)
	virtual void setPhaseOffset([[maybe_unused]] float offset) {}

	[[nodiscard]] int32_t getMaxValue() const override { return RESOLUTION; }
	[[nodiscard]] int32_t getNumDecimalPlaces() const override { return 0; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return KNOB; }

	// Scale to 0-50 for display (matches gold knob popup range)
	[[nodiscard]] float getDisplayValue() override { return (this->getValue() * 50.0f) / RESOLUTION; }

	void selectEncoderAction(int32_t offset) override {
		if (supportsAutoWrap()) {
			// Auto-wrap mode: wraps at boundaries and auto-adjusts per-knob phase offset
			int32_t scaledOffset = velocity_.getScaledOffset(offset);
			int32_t newValue = this->getValue() + scaledOffset;
			float phaseOffset = getPhaseOffset();

			if (newValue > RESOLUTION) {
				// Wrap past max: go to start and increment phase offset
				this->setValue(newValue - RESOLUTION);
				setPhaseOffset(phaseOffset + 1.0f);
			}
			else if (newValue < 0) {
				// Wrap past min: go to end and decrement phase offset
				this->setValue(newValue + RESOLUTION);
				setPhaseOffset(phaseOffset - 1.0f);
			}
			else {
				this->setValue(newValue);
			}
			this->writeCurrentValue();
			if (display->haveOLED()) {
				renderUIsForOled();
			}
			else {
				this->drawValue();
			}
		}
		else {
			// Standard clamped mode
			DecimalWithoutScrolling::selectEncoderAction(velocity_.getScaledOffset(offset));
		}
	}

	void renderInHorizontalMenu(const SlotPosition& slot) override {
		// Capture 'this' to call virtual getZoneName
		renderZoneInHorizontalMenu(slot, this->getValue(), RESOLUTION, NUM_ZONES,
		                           [this](int32_t z) { return this->getZoneName(z); });
	}

protected:
	void drawPixelsForOled() override {
		drawZoneForOled(this->getValue(), RESOLUTION, NUM_ZONES, [this](int32_t z) { return this->getZoneName(z); });
	}

	// 7-segment: show zone abbreviation + position (e.g., "SY50" for Sync at 50%)
	void drawActualValue(bool justDidHorizontalScroll = false) override {
		int32_t zoneWidth = RESOLUTION / NUM_ZONES;
		int32_t zoneIndex = std::min(this->getValue() / zoneWidth, NUM_ZONES - 1);
		int32_t posInZone = this->getValue() - (zoneIndex * zoneWidth);
		int32_t posPercent = (posInZone * 99) / zoneWidth; // 0-99

		const char* abbrev = getShortZoneName(zoneIndex);
		char buffer[5];
		// Take first 2 chars of abbreviation
		buffer[0] = abbrev[0];
		buffer[1] = (abbrev[1] != '\0') ? abbrev[1] : ' ';
		// Add 2-digit position
		buffer[2] = '0' + (posPercent / 10);
		buffer[3] = '0' + (posPercent % 10);
		buffer[4] = '\0';
		display->setText(buffer);
	}

	mutable VelocityEncoder velocity_;
};

/**
 * Zone-based menu item backed by a field on ModControllableAudio
 *
 * No CC learning by default. Derived class implements read/write
 * to access the specific field.
 *
 * @tparam NUM_ZONES Number of zones (typically 8)
 */
template <int32_t NUM_ZONES = 8>
class ZoneBasedFieldItem : public ZoneBasedMenuItem<NUM_ZONES> {
public:
	using ZoneBasedMenuItem<NUM_ZONES>::ZoneBasedMenuItem;
	// Derived class must implement readCurrentValue/writeCurrentValue
};

} // namespace deluge::gui::menu_item
