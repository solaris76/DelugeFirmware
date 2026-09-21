#pragma once

#include "dsp/machine/patches.h"
#include "gui/menu_item/integer.h"
#include "gui/menu_item/selection.h"
#include "gui/ui/sound_editor.h"
#include "processing/sound/sound.h"
#include "processing/source.h"
#include <cstdint>

namespace deluge::gui::menu_item::machine {

using FieldGetter = uint8_t* (*)(Source&);

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
			soundEditor.currentSound->killAllVoices();
		}
	}

	[[nodiscard]] int32_t getMaxValue() const override { return maxValue_; }
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }

	bool isRelevant(ModControllableAudio*, int32_t) override {
		return soundEditor.currentSound && soundEditor.currentSound->sources[0].oscType == requiredType_;
	}

private:
	FieldGetter getter_;
	OscType requiredType_;
	int32_t maxValue_;

	static void ensurePatch(Source& src) {
		switch (src.oscType) {
		case OscType::FM_TONE:
			src.ensureFmTonePatch();
			break;
		case OscType::FM_DRUM:
			src.ensureFmDrumPatch();
			break;
		case OscType::WAVETONE:
			src.ensureWaveTonePatch();
			break;
		case OscType::PERC:
			src.ensurePercPatch();
			break;
		default:
			break;
		}
	}
};

/// Perc Role: Kick / Snare / HH / Tom / Clap / Cymbal
class PercRole final : public Selection {
public:
	PercRole(l10n::String name) : Selection(name) {}

	void readCurrentValue() override { setValue(soundEditor.currentSound->sources[0].ensurePercPatch()->role); }

	void writeCurrentValue() override {
		auto* patch = soundEditor.currentSound->sources[0].ensurePercPatch();
		patch->role = static_cast<uint8_t>(getValue());
		patch->applyRoleDefaults();
		soundEditor.currentSound->killAllVoices();
	}

	deluge::vector<std::string_view> getOptions(OptType) override {
		return {"Kick", "Snare", "HH", "Tom", "Clap", "Cymbal"};
	}

	bool isRelevant(ModControllableAudio*, int32_t) override {
		return soundEditor.currentSound && soundEditor.currentSound->sources[0].oscType == OscType::PERC;
	}
};

// --- field accessors ---
inline uint8_t* fmToneAlgo(Source& s) {
	return &s.ensureFmTonePatch()->algorithm;
}
inline uint8_t* fmToneRatioC(Source& s) {
	return &s.ensureFmTonePatch()->ratioC;
}
inline uint8_t* fmToneRatioA(Source& s) {
	return &s.ensureFmTonePatch()->ratioA;
}
inline uint8_t* fmToneRatioB(Source& s) {
	return &s.ensureFmTonePatch()->ratioB;
}
inline uint8_t* fmToneHarm(Source& s) {
	return &s.ensureFmTonePatch()->harmonics;
}
inline uint8_t* fmToneDetune(Source& s) {
	return &s.ensureFmTonePatch()->detune;
}
inline uint8_t* fmToneFb(Source& s) {
	return &s.ensureFmTonePatch()->feedback;
}
inline uint8_t* fmToneMix(Source& s) {
	return &s.ensureFmTonePatch()->mix;
}
inline uint8_t* fmToneALev(Source& s) {
	return &s.ensureFmTonePatch()->aLevel;
}
inline uint8_t* fmToneAAtk(Source& s) {
	return &s.ensureFmTonePatch()->aEnvAtk;
}
inline uint8_t* fmToneADec(Source& s) {
	return &s.ensureFmTonePatch()->aEnvDec;
}
inline uint8_t* fmToneAEnd(Source& s) {
	return &s.ensureFmTonePatch()->aEnvEnd;
}
inline uint8_t* fmToneBLev(Source& s) {
	return &s.ensureFmTonePatch()->bLevel;
}
inline uint8_t* fmToneBAtk(Source& s) {
	return &s.ensureFmTonePatch()->bEnvAtk;
}
inline uint8_t* fmToneBDec(Source& s) {
	return &s.ensureFmTonePatch()->bEnvDec;
}
inline uint8_t* fmToneBEnd(Source& s) {
	return &s.ensureFmTonePatch()->bEnvEnd;
}

inline uint8_t* fmDrumTune(Source& s) {
	return &s.ensureFmDrumPatch()->tune;
}
inline uint8_t* fmDrumSweepT(Source& s) {
	return &s.ensureFmDrumPatch()->sweepTime;
}
inline uint8_t* fmDrumSweepD(Source& s) {
	return &s.ensureFmDrumPatch()->sweepDepth;
}
inline uint8_t* fmDrumAlgo(Source& s) {
	return &s.ensureFmDrumPatch()->algorithm;
}
inline uint8_t* fmDrumFb(Source& s) {
	return &s.ensureFmDrumPatch()->feedback;
}
inline uint8_t* fmDrumFold(Source& s) {
	return &s.ensureFmDrumPatch()->waveFold;
}
inline uint8_t* fmDrumBodyDec(Source& s) {
	return &s.ensureFmDrumPatch()->bodyDecay;
}
inline uint8_t* fmDrumBodyLev(Source& s) {
	return &s.ensureFmDrumPatch()->bodyLevel;
}
inline uint8_t* fmDrumNoiseLev(Source& s) {
	return &s.ensureFmDrumPatch()->noiseLevel;
}
inline uint8_t* fmDrumNoiseDec(Source& s) {
	return &s.ensureFmDrumPatch()->noiseDecay;
}
inline uint8_t* fmDrumTrans(Source& s) {
	return &s.ensureFmDrumPatch()->transientLevel;
}
inline uint8_t* fmDrumModA(Source& s) {
	return &s.ensureFmDrumPatch()->modA;
}
inline uint8_t* fmDrumModB(Source& s) {
	return &s.ensureFmDrumPatch()->modB;
}

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

inline uint8_t* percPitch(Source& s) {
	return &s.ensurePercPatch()->pitch;
}
inline uint8_t* percPitchEnv(Source& s) {
	return &s.ensurePercPatch()->pitchEnv;
}
inline uint8_t* percPitchEnvT(Source& s) {
	return &s.ensurePercPatch()->pitchEnvTime;
}
inline uint8_t* percColor(Source& s) {
	return &s.ensurePercPatch()->color;
}
inline uint8_t* percTone(Source& s) {
	return &s.ensurePercPatch()->tone;
}
inline uint8_t* percClick(Source& s) {
	return &s.ensurePercPatch()->click;
}
inline uint8_t* percDrive(Source& s) {
	return &s.ensurePercPatch()->drive;
}
inline uint8_t* percNoiseLev(Source& s) {
	return &s.ensurePercPatch()->noiseLevel;
}
inline uint8_t* percNoiseDec(Source& s) {
	return &s.ensurePercPatch()->noiseDecay;
}
inline uint8_t* percBodyDec(Source& s) {
	return &s.ensurePercPatch()->bodyDecay;
}
inline uint8_t* percHold(Source& s) {
	return &s.ensurePercPatch()->hold;
}

} // namespace deluge::gui::menu_item::machine
