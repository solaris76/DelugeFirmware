#pragma once

#include "dsp/intervallic/patch.h"
#include "gui/menu_item/integer.h"
#include "gui/menu_item/intervallic/helpers.h"
#include "gui/menu_item/selection.h"
#include <algorithm>

namespace deluge::gui::menu_item::intervallic {

class LatticePreset final : public Selection {
public:
	using Selection::Selection;
	void readCurrentValue() override {
		auto* patch = currentPatch();
		setValue(patch != nullptr ? patch->latticePreset : 0);
	}
	void writeCurrentValue() override {
		auto* patch = currentPatch();
		if (patch != nullptr) {
			patch->applyLatticePreset(getValue());
		}
	}
	deluge::vector<std::string_view> getOptions(OptType) override {
		return {"m9", "m13", "m11", "OPEN", "SUB", "CLSTR"};
	}
};

class LatticeInversion final : public Integer {
public:
	using Integer::Integer;
	void readCurrentValue() override {
		auto* patch = currentPatch();
		setValue(patch != nullptr ? patch->inversion : 0);
	}
	void writeCurrentValue() override {
		auto* patch = currentPatch();
		if (patch != nullptr) {
			patch->inversion = static_cast<int8_t>(std::clamp(getValue(), int32_t{0}, int32_t{7}));
		}
	}
	[[nodiscard]] int32_t getMaxValue() const override { return 7; }
};

class LatticeVoiceSpread final : public Integer {
public:
	using Integer::Integer;
	void readCurrentValue() override {
		auto* patch = currentPatch();
		setValue(patch != nullptr ? patch->voiceSpread : 0);
	}
	void writeCurrentValue() override {
		auto* patch = currentPatch();
		if (patch != nullptr) {
			patch->voiceSpread = static_cast<uint8_t>(std::clamp(getValue(), int32_t{0}, int32_t{255}));
		}
	}
	[[nodiscard]] int32_t getMaxValue() const override { return 255; }
};

class LatticePairFm final : public Integer {
public:
	using Integer::Integer;
	void readCurrentValue() override {
		auto* patch = currentPatch();
		setValue(patch != nullptr ? patch->pairFmAmount : 0);
	}
	void writeCurrentValue() override {
		auto* patch = currentPatch();
		if (patch != nullptr) {
			patch->pairFmAmount = static_cast<uint8_t>(std::clamp(getValue(), int32_t{0}, int32_t{255}));
		}
	}
	[[nodiscard]] int32_t getMaxValue() const override { return 255; }
};

class LatticePlayMode final : public Selection {
public:
	using Selection::Selection;
	void readCurrentValue() override {
		auto* patch = currentPatch();
		setValue(patch != nullptr ? static_cast<int32_t>(patch->playMode) : 0);
	}
	void writeCurrentValue() override {
		auto* patch = currentPatch();
		if (patch != nullptr) {
			patch->playMode = static_cast<dsp::intervallic::PlayMode>(getValue());
		}
	}
	deluge::vector<std::string_view> getOptions(OptType) override { return {"LATCH", "ENV"}; }
};

class LatticeLfoRate final : public Integer {
public:
	LatticeLfoRate(l10n::String name, uint8_t lfo_id) : Integer(name), lfoId_{lfo_id} {}
	void readCurrentValue() override {
		auto* patch = currentPatch();
		setValue(patch != nullptr ? patch->localLfos[lfoId_].rate : 0);
	}
	void writeCurrentValue() override {
		auto* patch = currentPatch();
		if (patch != nullptr) {
			patch->localLfos[lfoId_].rate = static_cast<uint8_t>(std::clamp(getValue(), int32_t{0}, int32_t{255}));
		}
	}
	[[nodiscard]] int32_t getMaxValue() const override { return 255; }

private:
	uint8_t lfoId_;
};

} // namespace deluge::gui::menu_item::intervallic
