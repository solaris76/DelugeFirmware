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

// Bipolar around mid-velocity (64): soft lowers dest dial, hard raises it (~±48 at extremes).
inline constexpr int32_t kMachineVelDepth = 48;

inline void applyVelToU8(uint8_t& field, int32_t velocitySourceValue) {
	int32_t delta = static_cast<int32_t>((static_cast<int64_t>(velocitySourceValue) * kMachineVelDepth) / 2147483647);
	int32_t v = static_cast<int32_t>(field) + delta;
	if (v < 0) {
		v = 0;
	}
	else if (v > 127) {
		v = 127;
	}
	field = static_cast<uint8_t>(v);
}

inline void applyVelocityToFmDrum(FmDrumPatch& p, int32_t velSrc) {
	uint8_t* f = nullptr;
	switch (p.velDest) {
	case 1:
		f = &p.tune;
		break;
	case 2:
		f = &p.sweep;
		break;
	case 3:
		f = &p.mod;
		break;
	case 4:
		f = &p.fold;
		break;
	case 5:
		f = &p.decay;
		break;
	case 6:
		f = &p.noise;
		break;
	default:
		return;
	}
	applyVelToU8(*f, velSrc);
}

inline void applyVelocityToWaveTone(WaveTonePatch& p, int32_t velSrc) {
	uint8_t* f = nullptr;
	switch (p.velDest) {
	case 1:
		f = &p.osc1Wave;
		break;
	case 2:
		f = &p.osc1PhaseDist;
		break;
	case 3:
		f = &p.osc1Level;
		break;
	case 4:
		f = &p.osc2Wave;
		break;
	case 5:
		f = &p.osc2PhaseDist;
		break;
	case 6:
		f = &p.osc2Level;
		break;
	case 7:
		f = &p.oscDrift;
		break;
	case 8:
		f = &p.noiseLevel;
		break;
	case 9:
		f = &p.noiseCharacter;
		break;
	default:
		return;
	}
	applyVelToU8(*f, velSrc);
}

inline void applyVelocityToPerc(PercPatch& p, int32_t velSrc) {
	uint8_t* f = nullptr;
	switch (p.velDest) {
	case 1:
		f = &p.pitch;
		break;
	case 2:
		f = &p.color;
		break;
	case 3:
		f = &p.noise;
		break;
	case 4:
		f = &p.decay;
		break;
	case 5:
		f = &p.crunch;
		break;
	default:
		return;
	}
	applyVelToU8(*f, velSrc);
}

inline void applyVelocityToSkin(SkinPatch& p, int32_t velSrc) {
	uint8_t* f = nullptr;
	switch (p.velDest) {
	case 1:
		f = &p.pitch;
		break;
	case 2:
		f = &p.harm;
		break;
	case 3:
		f = &p.morph;
		break;
	case 4:
		f = &p.fold;
		break;
	case 5:
		f = &p.decay;
		break;
	case 6:
		f = &p.noise;
		break;
	default:
		return;
	}
	applyVelToU8(*f, velSrc);
}

inline void applyVelocityToResonator(ResonatorPatch& p, int32_t velSrc) {
	uint8_t* f = nullptr;
	switch (p.velDest) {
	case 1:
		f = &p.structure;
		break;
	case 2:
		f = &p.brightness;
		break;
	case 3:
		f = &p.damping;
		break;
	case 4:
		f = &p.position;
		break;
	case 5:
		f = &p.excite;
		break;
	default:
		return;
	}
	applyVelToU8(*f, velSrc);
}

inline void applyVelocityToSyOsc(SyOscPatch& p, int32_t velSrc) {
	uint8_t* f = nullptr;
	switch (p.velDest) {
	case 1:
		f = &p.pitch;
		break;
	case 2:
		f = &p.sweep;
		break;
	case 3:
		f = &p.ratio;
		break;
	case 4:
		f = &p.color;
		break;
	case 5:
		f = &p.noise;
		break;
	case 6:
		f = &p.decay;
		break;
	default:
		return;
	}
	applyVelToU8(*f, velSrc);
}

} // namespace deluge::dsp::machine
