/*
 * Digitone / Nord / Ableton-style machine patches for OscType engines.
 * Shared amp/filter/FX stay on Sound — these structs are SYN-only.
 *
 * Design: few dials, each with wide sonic range; Mode/Algo picks the character
 * (BIA / Digitone philosophy — not a wall of micro-params).
 */
#pragma once

#include "definitions_cxx.hpp"
#include <cstdint>

namespace deluge::dsp::machine {

inline constexpr bool isMachineOscType(OscType t) {
	return t == OscType::WAVETONE || t == OscType::FM_DRUM || t == OscType::PERC || t == OscType::SKIN
	       || t == OscType::RESONATOR;
}

// FM Drum — Algo + 6 dials
struct FmDrumPatch {
	uint8_t algorithm{0}; // 0–6 flavour
	uint8_t tune{22};
	uint8_t sweep{60}; // depth + time coupled
	uint8_t mod{70};   // mod index + ratios
	uint8_t fold{10};  // fold + mild feedback
	uint8_t decay{70}; // body length
	uint8_t noise{35}; // noise + transient

	void initDefaults();
};

// Wavetone — keep the full dual-osc palette (user favourite)
struct WaveTonePatch {
	uint8_t osc1Wave{0}; // sine
	uint8_t osc1PhaseDist{50};
	uint8_t osc1Level{110};
	uint8_t osc2Wave{20}; // slight tri blend
	uint8_t osc2PhaseDist{50};
	uint8_t osc2Level{70};

	uint8_t osc1LinOffset{64};
	uint8_t osc1WaveTable{0}; // 0 prim, 1 harm
	uint8_t oscMod{0};        // off / RM / RM fixed / hard sync
	uint8_t phaseReset{1};
	uint8_t osc2LinOffset{68}; // slight detune
	uint8_t osc2WaveTable{0};
	uint8_t oscDrift{12};

	uint8_t noiseAtk{0};
	uint8_t noiseHold{0};
	uint8_t noiseDec{40};
	uint8_t noiseLevel{0};
	uint8_t noiseFiltBase{40};
	uint8_t noiseFiltWidth{80};
	uint8_t noiseType{0}; // grain / tuned / s&w
	uint8_t noiseCharacter{0};

	void initDefaults();
};

// Perc — Role + 5 dials (Archer-style sources)
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
	uint8_t color{70};  // inharmonicity + hardness
	uint8_t noise{40};  // level + tail
	uint8_t decay{50};  // body length
	uint8_t crunch{40}; // click + drive

	void initDefaults();
	void applyRoleDefaults();
	void loadRoleDefaults(uint8_t newRole);
};

// Skin — Mode + 5 dials (Skin / Liquid / Metal)
enum class SkinMode : uint8_t {
	Skin = 0,
	Liquid,
	Metal,
	COUNT,
};

struct SkinPatch {
	uint8_t mode{0};
	uint8_t pitch{36}; // register (bass→treble across dial)
	uint8_t harm{90};  // partial count + spread
	uint8_t morph{20}; // sine→square
	uint8_t fold{25};
	uint8_t decay{72}; // length; attack character derived

	void initDefaults();
	void applyModeDefaults();
	void loadModeDefaults(uint8_t newMode);
};

// Rings-inspired resonator (MIT-licensed MI Rings ideas; internal exciter).
// Model + 5 dials — Structure / Brightness / Damping / Position / Excite.
enum class ResonatorModel : uint8_t {
	Modal = 0, // band-pass partials
	Strings,   // sympathetic combs
	Wire,      // nonlinear string / KS-ish
	COUNT,
};

struct ResonatorPatch {
	uint8_t model{0};
	uint8_t structure{64};  // partial spacing / detune / dispersion
	uint8_t brightness{70}; // high partials / brightness
	uint8_t damping{55};    // sustain (high = longer)
	uint8_t position{40};   // strike / pickup position
	uint8_t excite{70};     // impulse / noise hit amount

	void initDefaults();
	void applyModelDefaults();
	void loadModelDefaults(uint8_t newModel);
};

struct MachineVoiceState {
	uint32_t phase[6]{};
	float oscEnv[6]{};
	// Resonator modal / comb state (reused across machine types)
	float resZ1[16]{};
	float resZ2[16]{};
	float combBuf[256]{};
	uint16_t combPos{0};
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
