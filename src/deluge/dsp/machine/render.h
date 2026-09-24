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

// Bipolar offset onto a dial. Vel uses ±48; Random uses ±36 for musical per-hit jitter.
inline void applySrcToU8(uint8_t& field, int32_t sourceValue, int32_t depth) {
	int32_t delta = static_cast<int32_t>((static_cast<int64_t>(sourceValue) * depth) / 2147483647);
	int32_t v = static_cast<int32_t>(field) + delta;
	if (v < 0) {
		v = 0;
	}
	else if (v > 127) {
		v = 127;
	}
	field = static_cast<uint8_t>(v);
}

inline constexpr int32_t kMachineVelDepth = 48;
// Max bipolar dial offset at randAmt=127 (~±56).
inline constexpr int32_t kMachineRandMaxDepth = 56;

inline int32_t randomDepthFromAmt(uint8_t randAmt) {
	return (static_cast<int32_t>(randAmt) * kMachineRandMaxDepth) / 127;
}

inline uint8_t* fmDrumDestField(FmDrumPatch& p, uint8_t dest) {
	switch (dest) {
	case 1:
		return &p.tune;
	case 2:
		return &p.sweep;
	case 3:
		return &p.mod;
	case 4:
		return &p.fold;
	case 5:
		return &p.decay;
	case 6:
		return &p.noise;
	default:
		return nullptr;
	}
}

inline uint8_t* waveToneDestField(WaveTonePatch& p, uint8_t dest) {
	switch (dest) {
	case 1:
		return &p.osc1Wave;
	case 2:
		return &p.osc1PhaseDist;
	case 3:
		return &p.osc1Level;
	case 4:
		return &p.osc2Wave;
	case 5:
		return &p.osc2PhaseDist;
	case 6:
		return &p.osc2Level;
	case 7:
		return &p.oscDrift;
	case 8:
		return &p.noiseLevel;
	case 9:
		return &p.noiseCharacter;
	default:
		return nullptr;
	}
}

inline uint8_t* percDestField(PercPatch& p, uint8_t dest) {
	switch (dest) {
	case 1:
		return &p.pitch;
	case 2:
		return &p.color;
	case 3:
		return &p.noise;
	case 4:
		return &p.decay;
	case 5:
		return &p.crunch;
	default:
		return nullptr;
	}
}

inline uint8_t* skinDestField(SkinPatch& p, uint8_t dest) {
	switch (dest) {
	case 1:
		return &p.pitch;
	case 2:
		return &p.harm;
	case 3:
		return &p.morph;
	case 4:
		return &p.fold;
	case 5:
		return &p.decay;
	case 6:
		return &p.noise;
	default:
		return nullptr;
	}
}

inline uint8_t* resonatorDestField(ResonatorPatch& p, uint8_t dest) {
	switch (dest) {
	case 1:
		return &p.structure;
	case 2:
		return &p.brightness;
	case 3:
		return &p.damping;
	case 4:
		return &p.position;
	case 5:
		return &p.excite;
	default:
		return nullptr;
	}
}

inline uint8_t* syOscDestField(SyOscPatch& p, uint8_t dest) {
	switch (dest) {
	case 1:
		return &p.pitch;
	case 2:
		return &p.sweep;
	case 3:
		return &p.ratio;
	case 4:
		return &p.color;
	case 5:
		return &p.noise;
	case 6:
		return &p.decay;
	default:
		return nullptr;
	}
}

inline void applyVelocityToFmDrum(FmDrumPatch& p, int32_t velSrc) {
	if (uint8_t* f = fmDrumDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToFmDrum(FmDrumPatch& p, int32_t randSrc) {
	if (uint8_t* f = fmDrumDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToWaveTone(WaveTonePatch& p, int32_t velSrc) {
	if (uint8_t* f = waveToneDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToWaveTone(WaveTonePatch& p, int32_t randSrc) {
	if (uint8_t* f = waveToneDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToPerc(PercPatch& p, int32_t velSrc) {
	if (uint8_t* f = percDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToPerc(PercPatch& p, int32_t randSrc) {
	if (uint8_t* f = percDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToSkin(SkinPatch& p, int32_t velSrc) {
	if (uint8_t* f = skinDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToSkin(SkinPatch& p, int32_t randSrc) {
	if (uint8_t* f = skinDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToResonator(ResonatorPatch& p, int32_t velSrc) {
	if (uint8_t* f = resonatorDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToResonator(ResonatorPatch& p, int32_t randSrc) {
	if (uint8_t* f = resonatorDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToSyOsc(SyOscPatch& p, int32_t velSrc) {
	if (uint8_t* f = syOscDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToSyOsc(SyOscPatch& p, int32_t randSrc) {
	if (uint8_t* f = syOscDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

} // namespace deluge::dsp::machine
