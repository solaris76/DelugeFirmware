/*
 * Digitone / Nord / Ableton-style machine patches for OscType engines.
 * Shared amp/filter/FX stay on Sound — these structs are SYN-only.
 *
 * Design: few dials, each with wide sonic range; Mode/Algo picks the character
 * (BIA / Digitone philosophy — not a wall of micro-params).
 *
 * velDest / randDest (shared indices per engine):
 *   FM:  0 Off, 1 Algo, 2 Pitch, 3 Wave, 4 Sweep, 5 Amount, 6 Fold, 7 Decay, 8 Noise
 *   WT:  0 Off, 1 Pitch, 2 Wave1, 3 PD1, 4 Lev1, 5 Wave2, 6 PD2, 7 Lev2, 8 Mod, 9 Drift, 10 Noise, 11 NType, 12 NChar
 *   Perc:0 Off, 1 Role, 2 Pitch, 3 Color, 4 Noise, 5 Decay, 6 Drive
 *   Skin:0 Off, 1 Mode, 2 Pitch, 3 Harm, 4 Wave, 5 Fold, 6 Decay, 7 Noise
 *   Res: 0 Off, 1 Model, 2 Pitch, 3 Harm, 4 Color, 5 Decay, 6 Mix, 7 Hit
 *   SY:  0 Off, 1 Mode, 2 Pitch, 3 Sweep, 4 Ratio, 5 Color, 6 Noise, 7 Decay
 */
#pragma once

#include "definitions_cxx.hpp"
#include <cstdint>

namespace deluge::dsp::machine {

inline constexpr bool isMachineOscType(OscType t) {
	return t == OscType::WAVETONE || t == OscType::FM_DRUM || t == OscType::PERC || t == OscType::SKIN
	       || t == OscType::RESONATOR || t == OscType::SY_OSC;
}

struct FmDrumPatch {
	uint8_t algorithm{0}; // 0–6 flavour
	uint8_t tune{22};
	uint8_t wave{0}; // carrier morph (0 = sine)
	uint8_t sweep{60};
	uint8_t mod{70};
	uint8_t fold{10};
	uint8_t decay{70};
	uint8_t noise{35};
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

	void initDefaults();
};

struct WaveTonePatch {
	uint8_t pitch{64}; // note transpose (64 = unity)
	uint8_t osc1Wave{0};
	uint8_t osc1PhaseDist{50};
	uint8_t osc1Level{110};
	uint8_t osc2Wave{20};
	uint8_t osc2PhaseDist{50};
	uint8_t osc2Level{70};

	uint8_t osc1LinOffset{64};
	uint8_t osc1WaveTable{0};
	uint8_t oscMod{0};
	uint8_t phaseReset{1};
	uint8_t osc2LinOffset{68};
	uint8_t osc2WaveTable{0};
	uint8_t oscDrift{12};

	uint8_t noiseAtk{0};
	uint8_t noiseHold{0};
	uint8_t noiseDec{40};
	uint8_t noiseLevel{0};
	uint8_t noiseFiltBase{40};
	uint8_t noiseFiltWidth{80};
	uint8_t noiseType{0};
	uint8_t noiseCharacter{0};
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

	void initDefaults();
};

enum class PercRole : uint8_t {
	Metal = 0,
	Bell,
	Hat808,
	FM,
	XOR,
	Grains,
	COUNT,
};

struct PercPatch {
	uint8_t role{0};
	uint8_t pitch{72};
	uint8_t color{70};
	uint8_t noise{40};
	uint8_t decay{50};
	uint8_t crunch{40};
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

	void initDefaults();
	void applyRoleDefaults();
	void loadRoleDefaults(uint8_t newRole);
};

enum class SkinMode : uint8_t {
	Skin = 0,
	Liquid,
	Metal,
	COUNT,
};

struct SkinPatch {
	uint8_t mode{0};
	uint8_t pitch{36};
	uint8_t harm{90};
	uint8_t morph{20};
	uint8_t fold{25};
	uint8_t decay{72};
	uint8_t noise{35};
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

	void initDefaults();
	void applyModeDefaults();
	void loadModeDefaults(uint8_t newMode);
};

enum class ResonatorModel : uint8_t {
	Modal = 0,
	Strings,
	Wire,
	COUNT,
};

struct ResonatorPatch {
	uint8_t model{0};
	uint8_t pitch{64}; // note transpose (64 = unity)
	uint8_t structure{64};
	uint8_t brightness{70};
	uint8_t damping{55};
	uint8_t position{40};
	uint8_t excite{70};
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

	void initDefaults();
	void applyModelDefaults();
	void loadModelDefaults(uint8_t newModel);
};

enum class SyOscMode : uint8_t {
	Dual = 0,
	Sync,
	FM,
	Ring,
	Noise,
	Sweep,
	COUNT,
};

struct SyOscPatch {
	uint8_t mode{1};
	uint8_t pitch{70};
	uint8_t sweep{75};
	uint8_t ratio{80};
	uint8_t color{55};
	uint8_t noise{30};
	uint8_t decay{55};
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

	void initDefaults();
	void applyModeDefaults();
	void loadModeDefaults(uint8_t newMode);
};

struct MachineVoiceState {
	uint32_t phase[6]{};
	float oscEnv[6]{};
	float combBuf[128]{};
	uint16_t combPos{0};
	uint16_t combLen{64};
	uint32_t noiseState{1};
	uint32_t sampleCount{0};
	uint32_t clickSamplesLeft{0};
	float envA{0.f};
	float envB{0.f};
	float bodyEnv{0.f};
	float noiseEnv{0.f};
	float pitchEnv{0.f};
	bool noteOn{false};

	void reset();
};

} // namespace deluge::dsp::machine
