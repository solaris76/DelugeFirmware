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

inline float envelopeTimeScaleFromDecayParam(int32_t decay_param) {
	float u = (static_cast<float>(decay_param) + 2147483648.f) * (1.f / 4294967296.f);
	if (u < 0.f) {
		u = 0.f;
	}
	else if (u > 1.f) {
		u = 1.f;
	}
	return 0.35f + u * u * 5.65f;
}

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
inline constexpr int32_t kMachineRandMaxDepth = 56;

inline int32_t randomDepthFromAmt(uint8_t randAmt) {
	return (static_cast<int32_t>(randAmt) * kMachineRandMaxDepth) / 127;
}

// Amount = chance (0–127) to pick a new Algo/Role/Mode this hit.
inline void applyRandomDiscrete(uint8_t& field, uint8_t count, int32_t randSrc, uint8_t amt) {
	if (amt == 0 || count < 2) {
		return;
	}
	uint32_t u = static_cast<uint32_t>(randSrc);
	if (static_cast<uint8_t>((u >> 24) & 0x7F) >= amt) {
		return;
	}
	field = static_cast<uint8_t>((u >> 1) % count);
}

inline void applyVelocityDiscrete(uint8_t& field, uint8_t count, int32_t velSrc) {
	if (count < 2) {
		return;
	}
	int32_t span = count - 1;
	int32_t delta = static_cast<int32_t>((static_cast<int64_t>(velSrc) * span) / 2147483647);
	int32_t v = static_cast<int32_t>(field) + delta;
	if (v < 0) {
		v = 0;
	}
	else if (v >= static_cast<int32_t>(count)) {
		v = count - 1;
	}
	field = static_cast<uint8_t>(v);
}

// --- continuous dial field lookup (dest 0 = Off handled by callers) ---

inline uint8_t* fmDrumDestField(FmDrumPatch& p, uint8_t dest) {
	switch (dest) {
	case 2:
		return &p.tune;
	case 3:
		return &p.wave;
	case 4:
		return &p.sweep;
	case 5:
		return &p.mod;
	case 6:
		return &p.fold;
	case 7:
		return &p.decay;
	case 8:
		return &p.noise;
	default:
		return nullptr;
	}
}

inline uint8_t* waveToneDestField(WaveTonePatch& p, uint8_t dest) {
	switch (dest) {
	case 1:
		return &p.pitch;
	case 2:
		return &p.osc1Wave;
	case 3:
		return &p.osc1PhaseDist;
	case 4:
		return &p.osc1Level;
	case 5:
		return &p.osc2Wave;
	case 6:
		return &p.osc2PhaseDist;
	case 7:
		return &p.osc2Level;
	case 9:
		return &p.oscDrift;
	case 10:
		return &p.noiseLevel;
	case 12:
		return &p.noiseCharacter;
	default:
		return nullptr;
	}
}

inline uint8_t* percDestField(PercPatch& p, uint8_t dest) {
	switch (dest) {
	case 2:
		return &p.pitch;
	case 3:
		return &p.color;
	case 4:
		return &p.noise;
	case 5:
		return &p.decay;
	case 6:
		return &p.crunch;
	default:
		return nullptr;
	}
}

inline uint8_t* skinDestField(SkinPatch& p, uint8_t dest) {
	switch (dest) {
	case 2:
		return &p.pitch;
	case 3:
		return &p.harm;
	case 4:
		return &p.morph;
	case 5:
		return &p.fold;
	case 6:
		return &p.decay;
	case 7:
		return &p.noise;
	default:
		return nullptr;
	}
}

inline uint8_t* resonatorDestField(ResonatorPatch& p, uint8_t dest) {
	switch (dest) {
	case 2:
		return &p.pitch;
	case 3:
		return &p.structure;
	case 4:
		return &p.brightness;
	case 5:
		return &p.damping;
	case 6:
		return &p.position;
	case 7:
		return &p.excite;
	default:
		return nullptr;
	}
}

inline uint8_t* syOscDestField(SyOscPatch& p, uint8_t dest) {
	switch (dest) {
	case 2:
		return &p.pitch;
	case 3:
		return &p.sweep;
	case 4:
		return &p.ratio;
	case 5:
		return &p.color;
	case 6:
		return &p.noise;
	case 7:
		return &p.decay;
	default:
		return nullptr;
	}
}

inline void applyVelocityToFmDrum(FmDrumPatch& p, int32_t velSrc) {
	if (p.velDest == 1) {
		applyVelocityDiscrete(p.algorithm, 7, velSrc);
		return;
	}
	if (uint8_t* f = fmDrumDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToFmDrum(FmDrumPatch& p, int32_t randSrc) {
	if (p.randDest == 1) {
		applyRandomDiscrete(p.algorithm, 7, randSrc, p.randAmt);
		return;
	}
	if (uint8_t* f = fmDrumDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToWaveTone(WaveTonePatch& p, int32_t velSrc) {
	if (p.velDest == 8) {
		applyVelocityDiscrete(p.oscMod, 4, velSrc);
		return;
	}
	if (p.velDest == 11) {
		applyVelocityDiscrete(p.noiseType, 3, velSrc);
		return;
	}
	if (uint8_t* f = waveToneDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToWaveTone(WaveTonePatch& p, int32_t randSrc) {
	if (p.randDest == 8) {
		applyRandomDiscrete(p.oscMod, 4, randSrc, p.randAmt);
		return;
	}
	if (p.randDest == 11) {
		applyRandomDiscrete(p.noiseType, 3, randSrc, p.randAmt);
		return;
	}
	if (uint8_t* f = waveToneDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToPerc(PercPatch& p, int32_t velSrc) {
	if (p.velDest == 1) {
		applyVelocityDiscrete(p.role, static_cast<uint8_t>(PercRole::COUNT), velSrc);
		return;
	}
	if (uint8_t* f = percDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToPerc(PercPatch& p, int32_t randSrc) {
	if (p.randDest == 1) {
		applyRandomDiscrete(p.role, static_cast<uint8_t>(PercRole::COUNT), randSrc, p.randAmt);
		return;
	}
	if (uint8_t* f = percDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToSkin(SkinPatch& p, int32_t velSrc) {
	if (p.velDest == 1) {
		applyVelocityDiscrete(p.mode, static_cast<uint8_t>(SkinMode::COUNT), velSrc);
		return;
	}
	if (uint8_t* f = skinDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToSkin(SkinPatch& p, int32_t randSrc) {
	if (p.randDest == 1) {
		applyRandomDiscrete(p.mode, static_cast<uint8_t>(SkinMode::COUNT), randSrc, p.randAmt);
		return;
	}
	if (uint8_t* f = skinDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToResonator(ResonatorPatch& p, int32_t velSrc) {
	if (p.velDest == 1) {
		applyVelocityDiscrete(p.model, static_cast<uint8_t>(ResonatorModel::COUNT), velSrc);
		return;
	}
	if (uint8_t* f = resonatorDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToResonator(ResonatorPatch& p, int32_t randSrc) {
	if (p.randDest == 1) {
		applyRandomDiscrete(p.model, static_cast<uint8_t>(ResonatorModel::COUNT), randSrc, p.randAmt);
		return;
	}
	if (uint8_t* f = resonatorDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

inline void applyVelocityToSyOsc(SyOscPatch& p, int32_t velSrc) {
	if (p.velDest == 1) {
		applyVelocityDiscrete(p.mode, static_cast<uint8_t>(SyOscMode::COUNT), velSrc);
		return;
	}
	if (uint8_t* f = syOscDestField(p, p.velDest)) {
		applySrcToU8(*f, velSrc, kMachineVelDepth);
	}
}
inline void applyRandomToSyOsc(SyOscPatch& p, int32_t randSrc) {
	if (p.randDest == 1) {
		applyRandomDiscrete(p.mode, static_cast<uint8_t>(SyOscMode::COUNT), randSrc, p.randAmt);
		return;
	}
	if (uint8_t* f = syOscDestField(p, p.randDest)) {
		applySrcToU8(*f, randSrc, randomDepthFromAmt(p.randAmt));
	}
}

} // namespace deluge::dsp::machine
