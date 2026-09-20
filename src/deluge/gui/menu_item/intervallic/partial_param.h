#pragma once

#include "definitions_cxx.hpp"
#include "gui/menu_item/integer.h"
#include "gui/menu_item/intervallic/helpers.h"
#include "gui/menu_item/selection.h"
#include "hid/display/display.h"
#include "util/cfunctions.h"
#include <algorithm>
#include <cstring>

namespace deluge::gui::menu_item::intervallic {

enum class PartialField : uint8_t {
	Level,
	Interval,
	Detune,
	Pan,
	LfoDepthLevel,
	LfoDepthDetune,
	LfoIndex,
	LfoPhase,
};

class PartialParam final : public Integer {
public:
	PartialParam(l10n::String name, uint8_t partial_id, PartialField field)
	    : Integer(name), partialId_{partial_id}, field_{field} {}

	void readCurrentValue() override { setValue(getRaw()); }
	void writeCurrentValue() override { setRaw(getValue()); }

	[[nodiscard]] int32_t getMaxValue() const override {
		switch (field_) {
		case PartialField::Interval:
			return 48;
		case PartialField::Detune:
			return 200;
		case PartialField::Pan:
			return 128;
		case PartialField::LfoIndex:
			return 3;
		default:
			return 255;
		}
	}

	[[nodiscard]] int32_t getMinValue() const override { return 0; }

	[[nodiscard]] int32_t getDisplayValue() override {
		switch (field_) {
		case PartialField::Interval:
			return getValue() - 24;
		case PartialField::Detune:
			return getValue() - 100;
		case PartialField::Pan:
			return getValue() - 64;
		default:
			return getValue();
		}
	}

private:
	uint8_t partialId_;
	PartialField field_;

	int32_t getRaw() const {
		auto* p = currentPartial(partialId_);
		if (p == nullptr) {
			return 0;
		}
		switch (field_) {
		case PartialField::Level:
			return p->level;
		case PartialField::Interval:
			return p->intervalSemitones + 24;
		case PartialField::Detune:
			return p->detuneCents + 100;
		case PartialField::Pan:
			return p->pan + 64;
		case PartialField::LfoDepthLevel:
			return p->lfoDepthLevel;
		case PartialField::LfoDepthDetune:
			return p->lfoDepthDetune;
		case PartialField::LfoIndex:
			return p->lfoIndex;
		case PartialField::LfoPhase:
			return p->lfoPhaseOffset;
		}
		return 0;
	}

	void setRaw(int32_t val) {
		auto* p = currentPartial(partialId_);
		if (p == nullptr) {
			return;
		}
		val = std::clamp(val, getMinValue(), getMaxValue());
		switch (field_) {
		case PartialField::Level:
			p->level = static_cast<uint8_t>(val);
			p->enabled = val > 0;
			break;
		case PartialField::Interval:
			p->intervalSemitones = static_cast<int8_t>(val - 24);
			break;
		case PartialField::Detune:
			p->detuneCents = static_cast<int8_t>(val - 100);
			break;
		case PartialField::Pan:
			p->pan = static_cast<int8_t>(val - 64);
			break;
		case PartialField::LfoDepthLevel:
			p->lfoDepthLevel = static_cast<uint8_t>(val);
			break;
		case PartialField::LfoDepthDetune:
			p->lfoDepthDetune = static_cast<uint8_t>(val);
			break;
		case PartialField::LfoIndex:
			p->lfoIndex = static_cast<uint8_t>(val);
			break;
		case PartialField::LfoPhase:
			p->lfoPhaseOffset = static_cast<uint8_t>(val);
			break;
		}
	}
};

class PartialWave final : public Selection {
public:
	PartialWave(l10n::String name, uint8_t partial_id) : Selection(name), partialId_{partial_id} {}

	void readCurrentValue() override {
		auto* p = currentPartial(partialId_);
		setValue(p != nullptr ? waveToOption(p->wave) : 0);
	}

	void writeCurrentValue() override {
		auto* p = currentPartial(partialId_);
		if (p == nullptr) {
			return;
		}
		p->wave = optionToWave(getValue());
		if (auto* patch = currentPatch()) {
			patch->ensurePhiCache(partialId_);
		}
	}

	deluge::vector<std::string_view> getOptions(OptType) override {
		using enum l10n::String;
		return {
		    l10n::getView(STRING_FOR_SINE),      l10n::getView(STRING_FOR_TRIANGLE),
		    l10n::getView(STRING_FOR_SQUARE),    l10n::getView(STRING_FOR_SAW),
		    l10n::getView(STRING_FOR_WAVETABLE), l10n::getView(STRING_FOR_PHI_MORPH),
		    l10n::getView(STRING_FOR_PHI_WEAVE), l10n::getView(STRING_FOR_PHI_VOX),
		    l10n::getView(STRING_FOR_PHI_SWARM), l10n::getView(STRING_FOR_PHI_GENDY),
		    l10n::getView(STRING_FOR_PHI_STAIR),
		};
	}

private:
	uint8_t partialId_;

	static int32_t waveToOption(OscType w) {
		switch (w) {
		case OscType::TRIANGLE:
			return 1;
		case OscType::SQUARE:
		case OscType::ANALOG_SQUARE:
			return 2;
		case OscType::SAW:
		case OscType::ANALOG_SAW_2:
			return 3;
		case OscType::WAVETABLE:
			return 4;
		case OscType::PHI_MORPH:
			return 5;
		case OscType::PHI_WEAVE:
			return 6;
		case OscType::PHI_VOX:
			return 7;
		case OscType::PHI_SWARM:
			return 8;
		case OscType::PHI_GENDY:
			return 9;
		case OscType::PHI_STAIR:
			return 10;
		case OscType::SINE:
		default:
			return 0;
		}
	}

	static OscType optionToWave(int32_t opt) {
		switch (opt) {
		case 1:
			return OscType::TRIANGLE;
		case 2:
			return OscType::SQUARE;
		case 3:
			return OscType::SAW;
		case 4:
			return OscType::WAVETABLE;
		case 5:
			return OscType::PHI_MORPH;
		case 6:
			return OscType::PHI_WEAVE;
		case 7:
			return OscType::PHI_VOX;
		case 8:
			return OscType::PHI_SWARM;
		case 9:
			return OscType::PHI_GENDY;
		case 10:
			return OscType::PHI_STAIR;
		default:
			return OscType::SINE;
		}
	}
};

} // namespace deluge::gui::menu_item::intervallic
