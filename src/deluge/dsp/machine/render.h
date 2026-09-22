#pragma once

#include "dsp/machine/patches.h"
#include <cstdint>

namespace deluge::dsp::machine {

void renderFmDrum(FmDrumPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                  uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale = 1.f);

void renderWaveTone(WaveTonePatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                    uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale = 1.f);

void renderPerc(PercPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale = 1.f);

void renderSkin(SkinPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale = 1.f);

void renderResonator(ResonatorPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                     uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale = 1.f);

void renderSyOsc(SyOscPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                 uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale = 1.f);

// Map Sound Env1 Decay (patched q31) onto machine envelope duration. Mid ≈ 1x; high ≈ much longer.
inline float envelopeTimeScaleFromDecayParam(int32_t decay_param) {
	float u = (static_cast<float>(decay_param) + 2147483648.f) * (1.f / 4294967296.f);
	if (u < 0.f) {
		u = 0.f;
	}
	else if (u > 1.f) {
		u = 1.f;
	}
	// ~0.35x at min, ~1x around mid, ~6x at max — so Mod Decay clearly lengthens hits
	return 0.35f + u * u * 5.65f;
}

} // namespace deluge::dsp::machine
