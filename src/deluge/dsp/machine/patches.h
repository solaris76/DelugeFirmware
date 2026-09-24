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
	       || t == OscType::RESONATOR || t == OscType::SY_OSC;
}

// FM Drum — Algo + 6 dials
// velDest: 0=Off, 1=Tune, 2=Sweep, 3=Mod, 4=Fold, 5=Decay, 6=Noise
struct FmDrumPatch {
	uint8_t algorithm{0}; // 0–6 flavour
	uint8_t tune{22};
	uint8_t sweep{60}; // depth + time coupled
	uint8_t mod{70};   // mod index + ratios
	uint8_t fold{10};  // fold + mild feedback
	uint8_t decay{70}; // body length
	uint8_t noise{35}; // noise + transient
	uint8_t velDest{0};
	uint8_t randDest{0}; // same dial indices as velDest
	uint8_t randAmt{40}; // 0–127 scales random depth

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
	// velDest/randDest: 0=Off, 1=Wave1, 2=PD1, 3=Lev1, 4=Wave2, 5=PD2, 6=Lev2, 7=Drift, 8=Noise, 9=NChar
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

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

// velDest: 0=Off, 1=Pitch, 2=Color, 3=Noise, 4=Decay, 5=Crunch
struct PercPatch {
	uint8_t role{0};
	uint8_t pitch{72};
	uint8_t color{70};  // inharmonicity + hardness
	uint8_t noise{40};  // level + tail
	uint8_t decay{50};  // body length
	uint8_t crunch{40}; // click + drive
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

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

// velDest: 0=Off, 1=Pitch, 2=Harm, 3=Morph, 4=Fold, 5=Decay, 6=Noise
struct SkinPatch {
	uint8_t mode{0};
	uint8_t pitch{36}; // register (bass→treble across dial)
	uint8_t harm{90};  // partial count + spread
	uint8_t morph{20}; // sine→square
	uint8_t fold{25};
	uint8_t decay{72}; // length; attack character derived
	uint8_t noise{35}; // noise + transient (0 = clean)
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

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

// velDest: 0=Off, 1=Structure, 2=Bright, 3=Damping, 4=Position, 5=Excite
struct ResonatorPatch {
	uint8_t model{0};
	uint8_t structure{64};  // partial spacing / detune / dispersion
	uint8_t brightness{70}; // high partials / brightness
	uint8_t damping{55};    // sustain (high = longer)
	uint8_t position{40};   // strike / pickup position
	uint8_t excite{70};     // impulse / noise hit amount
	uint8_t velDest{0};
	uint8_t randDest{0};
	uint8_t randAmt{40};

	void initDefaults();
	void applyModelDefaults();
	void loadModelDefaults(uint8_t newModel);
};

// Syncussion SY-1 / SY0.5–inspired one channel: dual VCO + noise, Mode + 6 dials.
enum class SyOscMode : uint8_t {
	Dual = 0, // A+B mix
	Sync,     // B hard-syncs to A (classic sweep)
	FM,       // A phase-mods B
	Ring,     // A×B
	Noise,    // noise-forward + tone
	Sweep,    // strong pitch env on both (laser / zap)
	COUNT,
};

// velDest: 0=Off, 1=Pitch, 2=Sweep, 3=Ratio, 4=Color, 5=Noise, 6=Decay
struct SyOscPatch {
	uint8_t mode{1}; // default Sync — the signature sound
	uint8_t pitch{70};
	uint8_t sweep{75}; // depth + time coupled
	uint8_t ratio{80}; // B vs A
	uint8_t color{55}; // tone / LP brightness
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
	// Resonator: small comb delay (KS) — keep tiny for voice RAM / cache
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
