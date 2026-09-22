#pragma once

#include "definitions_cxx.hpp"
#include "gui/menu_item/integer.h"
#include "gui/menu_item/patched_param/integer.h"
#include "gui/menu_item/selection.h"
#include "gui/ui/sound_editor.h"
#include "hid/display/display.h"
#include "processing/sound/sound.h"
#include "processing/source.h"
#include <algorithm>
#include <cstdint>

namespace deluge::gui::menu_item::machine {

using FieldGetter = uint8_t* (*)(Source&);

inline void ensurePatch(Source& src) {
	src.ensureMachinePatchForType(src.oscType);
}

/// Unpatchable U8 (Algo / discrete fields).
class U8Param : public IntegerContinuous {
public:
	U8Param(l10n::String name, FieldGetter getter, OscType requiredType, int32_t maxValue = 127)
	    : IntegerContinuous(name), getter_(getter), requiredType_(requiredType), maxValue_(maxValue) {}

	void readCurrentValue() override {
		Source& src = soundEditor.currentSound->sources[0];
		ensurePatch(src);
		uint8_t* f = getter_(src);
		setValue(f ? *f : 0);
	}

	void writeCurrentValue() override {
		Source& src = soundEditor.currentSound->sources[0];
		ensurePatch(src);
		uint8_t* f = getter_(src);
		if (f) {
			*f = static_cast<uint8_t>(std::clamp(getValue(), int32_t{0}, maxValue_));
		}
	}

	[[nodiscard]] int32_t getMaxValue() const override { return maxValue_; }
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }

	bool isRelevant(ModControllableAudio*, int32_t) const override {
		return soundEditor.currentSound && soundEditor.currentSound->sources[0].oscType == requiredType_;
	}

private:
	FieldGetter getter_;
	OscType requiredType_;
	int32_t maxValue_;
};

/// Continuous machine dial: Autoparam + Select→mod map + mirror into hex patch field.
class Dial final : public patched_param::Integer {
public:
	Dial(l10n::String name, FieldGetter getter, OscType requiredType, int32_t dialIndex, int32_t maxValue = 127)
	    : Integer(name, deluge::modulation::params::LOCAL_MACHINE_0 + dialIndex), getter_(getter),
	      requiredType_(requiredType), maxValue_(maxValue) {}

	[[nodiscard]] int32_t getMaxValue() const override { return maxValue_; }
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }

	bool isRelevant(ModControllableAudio*, int32_t) const override {
		return soundEditor.currentSound && soundEditor.currentSound->sources[0].oscType == requiredType_;
	}

protected:
	void readCurrentValue() override;
	void writeCurrentValue() override;
	int32_t getFinalValue() override;

private:
	void mirrorDialOntoPatch(Sound& sound, uint8_t dial) const;

	FieldGetter getter_;
	OscType requiredType_;
	int32_t maxValue_;
};

class PercRole final : public Selection {
public:
	PercRole(l10n::String name) : Selection(name) {}
	void readCurrentValue() override { setValue(soundEditor.currentSound->sources[0].ensurePercPatch()->role); }
	void writeCurrentValue() override {
		soundEditor.currentSound->sources[0].ensurePercPatch()->role = static_cast<uint8_t>(getValue());
		soundEditor.currentSound->killAllVoices();
	}
	deluge::vector<std::string_view> getOptions(OptType) override {
		return {"Metal", "Bell", "808", "FM", "XOR", "Grains"};
	}
	bool isRelevant(ModControllableAudio*, int32_t) const override {
		return soundEditor.currentSound && soundEditor.currentSound->sources[0].oscType == OscType::PERC;
	}
};

class SkinMode final : public Selection {
public:
	SkinMode(l10n::String name) : Selection(name) {}
	void readCurrentValue() override { setValue(soundEditor.currentSound->sources[0].ensureSkinPatch()->mode); }
	void writeCurrentValue() override {
		soundEditor.currentSound->sources[0].ensureSkinPatch()->mode = static_cast<uint8_t>(getValue());
		soundEditor.currentSound->killAllVoices();
	}
	deluge::vector<std::string_view> getOptions(OptType) override { return {"Skin", "Liquid", "Metal"}; }
	bool isRelevant(ModControllableAudio*, int32_t) const override {
		return soundEditor.currentSound && soundEditor.currentSound->sources[0].oscType == OscType::SKIN;
	}
};

class ResonatorModel final : public Selection {
public:
	ResonatorModel(l10n::String name) : Selection(name) {}
	void readCurrentValue() override { setValue(soundEditor.currentSound->sources[0].ensureResonatorPatch()->model); }
	void writeCurrentValue() override {
		soundEditor.currentSound->sources[0].ensureResonatorPatch()->model = static_cast<uint8_t>(getValue());
		soundEditor.currentSound->killAllVoices();
	}
	deluge::vector<std::string_view> getOptions(OptType) override { return {"Modal", "Strings", "Wire"}; }
	bool isRelevant(ModControllableAudio*, int32_t) const override {
		return soundEditor.currentSound && soundEditor.currentSound->sources[0].oscType == OscType::RESONATOR;
	}
};

class SyOscMode final : public Selection {
public:
	SyOscMode(l10n::String name) : Selection(name) {}
	void readCurrentValue() override { setValue(soundEditor.currentSound->sources[0].ensureSyOscPatch()->mode); }
	void writeCurrentValue() override {
		soundEditor.currentSound->sources[0].ensureSyOscPatch()->mode = static_cast<uint8_t>(getValue());
		soundEditor.currentSound->killAllVoices();
	}
	deluge::vector<std::string_view> getOptions(OptType) override {
		return {"Dual", "Sync", "FM", "Ring", "Noise", "Sweep"};
	}
	bool isRelevant(ModControllableAudio*, int32_t) const override {
		return soundEditor.currentSound && soundEditor.currentSound->sources[0].oscType == OscType::SY_OSC;
	}
};

// --- FM Drum ---
inline uint8_t* fmDrumAlgo(Source& s) {
	return &s.ensureFmDrumPatch()->algorithm;
}
inline uint8_t* fmDrumTune(Source& s) {
	return &s.ensureFmDrumPatch()->tune;
}
inline uint8_t* fmDrumSweep(Source& s) {
	return &s.ensureFmDrumPatch()->sweep;
}
inline uint8_t* fmDrumMod(Source& s) {
	return &s.ensureFmDrumPatch()->mod;
}
inline uint8_t* fmDrumFold(Source& s) {
	return &s.ensureFmDrumPatch()->fold;
}
inline uint8_t* fmDrumDecay(Source& s) {
	return &s.ensureFmDrumPatch()->decay;
}
inline uint8_t* fmDrumNoise(Source& s) {
	return &s.ensureFmDrumPatch()->noise;
}

// --- Wavetone ---
inline uint8_t* wtOsc1Wave(Source& s) {
	return &s.ensureWaveTonePatch()->osc1Wave;
}
inline uint8_t* wtOsc1Pd(Source& s) {
	return &s.ensureWaveTonePatch()->osc1PhaseDist;
}
inline uint8_t* wtOsc1Lev(Source& s) {
	return &s.ensureWaveTonePatch()->osc1Level;
}
inline uint8_t* wtOsc2Wave(Source& s) {
	return &s.ensureWaveTonePatch()->osc2Wave;
}
inline uint8_t* wtOsc2Pd(Source& s) {
	return &s.ensureWaveTonePatch()->osc2PhaseDist;
}
inline uint8_t* wtOsc2Lev(Source& s) {
	return &s.ensureWaveTonePatch()->osc2Level;
}
inline uint8_t* wtOscMod(Source& s) {
	return &s.ensureWaveTonePatch()->oscMod;
}
inline uint8_t* wtDrift(Source& s) {
	return &s.ensureWaveTonePatch()->oscDrift;
}
inline uint8_t* wtNoiseLev(Source& s) {
	return &s.ensureWaveTonePatch()->noiseLevel;
}
inline uint8_t* wtNoiseType(Source& s) {
	return &s.ensureWaveTonePatch()->noiseType;
}
inline uint8_t* wtNoiseChar(Source& s) {
	return &s.ensureWaveTonePatch()->noiseCharacter;
}

// --- Perc ---
inline uint8_t* percPitch(Source& s) {
	return &s.ensurePercPatch()->pitch;
}
inline uint8_t* percColor(Source& s) {
	return &s.ensurePercPatch()->color;
}
inline uint8_t* percNoise(Source& s) {
	return &s.ensurePercPatch()->noise;
}
inline uint8_t* percDecay(Source& s) {
	return &s.ensurePercPatch()->decay;
}
inline uint8_t* percCrunch(Source& s) {
	return &s.ensurePercPatch()->crunch;
}

// --- Skin ---
inline uint8_t* skinPitch(Source& s) {
	return &s.ensureSkinPatch()->pitch;
}
inline uint8_t* skinHarm(Source& s) {
	return &s.ensureSkinPatch()->harm;
}
inline uint8_t* skinMorph(Source& s) {
	return &s.ensureSkinPatch()->morph;
}
inline uint8_t* skinFold(Source& s) {
	return &s.ensureSkinPatch()->fold;
}
inline uint8_t* skinDecay(Source& s) {
	return &s.ensureSkinPatch()->decay;
}

// --- Resonator ---
inline uint8_t* resStructure(Source& s) {
	return &s.ensureResonatorPatch()->structure;
}
inline uint8_t* resBright(Source& s) {
	return &s.ensureResonatorPatch()->brightness;
}
inline uint8_t* resDamping(Source& s) {
	return &s.ensureResonatorPatch()->damping;
}
inline uint8_t* resPosition(Source& s) {
	return &s.ensureResonatorPatch()->position;
}
inline uint8_t* resExcite(Source& s) {
	return &s.ensureResonatorPatch()->excite;
}

// --- SY Osc ---
inline uint8_t* syOscPitch(Source& s) {
	return &s.ensureSyOscPatch()->pitch;
}
inline uint8_t* syOscSweep(Source& s) {
	return &s.ensureSyOscPatch()->sweep;
}
inline uint8_t* syOscRatio(Source& s) {
	return &s.ensureSyOscPatch()->ratio;
}
inline uint8_t* syOscColor(Source& s) {
	return &s.ensureSyOscPatch()->color;
}
inline uint8_t* syOscNoise(Source& s) {
	return &s.ensureSyOscPatch()->noise;
}
inline uint8_t* syOscDecay(Source& s) {
	return &s.ensureSyOscPatch()->decay;
}

} // namespace deluge::gui::menu_item::machine
