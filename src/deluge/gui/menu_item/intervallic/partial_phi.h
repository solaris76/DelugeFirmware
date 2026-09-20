#pragma once

#include "gui/menu_item/integer.h"
#include "gui/menu_item/intervallic/helpers.h"
#include "gui/menu_item/zone_based.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "util/cfunctions.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace deluge::gui::menu_item::intervallic {

inline constexpr int32_t kPhiNumZones = 8;
inline constexpr int32_t kPhiZoneResolution = 1024;

class PartialPhiZone final : public ZoneBasedMenuItem<kPhiNumZones, kPhiZoneResolution> {
public:
	PartialPhiZone(l10n::String name, uint8_t partial_id, uint8_t zone_id)
	    : ZoneBasedMenuItem(name), partialId_{partial_id}, zoneId_{zone_id} {}

	void readCurrentValue() override { setValue(static_cast<int32_t>(zoneField())); }
	void writeCurrentValue() override { zoneField() = static_cast<uint16_t>(getValue()); }

	[[nodiscard]] bool supportsAutoWrap() const override { return true; }
	[[nodiscard]] float getPhaseOffset() const override { return phaseField(); }
	void setPhaseOffset(float offset) override { phaseField() = offset; }

	[[nodiscard]] const char* getZoneName(int32_t zoneIndex) const override {
		static const char* const kNames[8] = {"Ember", "Coral", "Prism", "Jade", "Azure", "Ivory", "Slate", "Onyx"};
		if (zoneIndex < 0 || zoneIndex >= 8) {
			return "?";
		}
		return kNames[zoneIndex];
	}

	void selectEncoderAction(int32_t offset) override {
		if (Buttons::isButtonPressed(hid::button::SELECT_ENC)) {
			Buttons::selectButtonPressUsedUp = true;
			float& phase = phaseField();
			phase = std::max(0.0f, phase + static_cast<float>(offset));
			char buffer[16];
			intToString(static_cast<int32_t>(std::floor(phase)), buffer, 1);
			display->displayPopup(buffer);
			return;
		}
		ZoneBasedMenuItem::selectEncoderAction(offset);
	}

	bool isRelevant(ModControllableAudio*, int32_t) const override {
		auto* p = currentPartial(partialId_);
		return p != nullptr && p->isPhiFamily();
	}

private:
	uint8_t partialId_;
	uint8_t zoneId_;

	uint16_t& zoneField() const {
		auto* p = currentPartial(partialId_);
		return (zoneId_ == 0) ? p->phiZoneA : p->phiZoneB;
	}
	float& phaseField() const {
		auto* p = currentPartial(partialId_);
		return (zoneId_ == 0) ? p->phiPhaseA : p->phiPhaseB;
	}
};

class PartialWaveIndex final : public IntegerContinuous {
public:
	PartialWaveIndex(l10n::String name, uint8_t partial_id) : IntegerContinuous(name), partialId_{partial_id} {}

	void readCurrentValue() override {
		auto* p = currentPartial(partialId_);
		// Map ±(1<<30) roughly to 0..50 for menu
		int32_t v = p != nullptr ? ((p->waveIndex >> 25) + 25) : 25;
		setValue(std::clamp(v, int32_t{0}, int32_t{50}));
	}

	void writeCurrentValue() override {
		auto* p = currentPartial(partialId_);
		if (p == nullptr) {
			return;
		}
		p->waveIndex = (getValue() - 25) << 25;
	}

	[[nodiscard]] int32_t getMaxValue() const override { return 50; }
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return SLIDER; }

	void selectEncoderAction(int32_t offset) override {
		auto* p = currentPartial(partialId_);
		if (p != nullptr && p->isPhiFamily() && Buttons::isButtonPressed(hid::button::SELECT_ENC)) {
			Buttons::selectButtonPressUsedUp = true;
			p->gamma = std::max(0.0f, p->gamma + static_cast<float>(offset));
			char buffer[16];
			strcpy(buffer, "G:");
			intToString(static_cast<int32_t>(p->gamma), buffer + 2, 1);
			display->displayPopup(buffer);
			return;
		}
		IntegerContinuous::selectEncoderAction(offset);
	}

	bool isRelevant(ModControllableAudio*, int32_t) const override {
		auto* p = currentPartial(partialId_);
		if (p == nullptr) {
			return false;
		}
		if (p->isPhiFamily()) {
			return true;
		}
		return p->wave == OscType::WAVETABLE;
	}

private:
	uint8_t partialId_;
};

} // namespace deluge::gui::menu_item::intervallic
