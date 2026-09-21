/*
 * Digitone / Nord / Ableton-style machine patches for OscType engines.
 * Shared amp/filter/FX stay on Sound — these structs are SYN-only.
 */
#pragma once

#include "definitions_cxx.hpp"
#include <cstdint>

namespace deluge::dsp::machine {

inline constexpr bool isMachineOscType(OscType t) {
	return t == OscType::FM_TONE || t == OscType::WAVETONE || t == OscType::FM_DRUM || t == OscType::PERC;
}

// Digitone FM Tone — 4 operators A, B1, B2, C
struct FmTonePatch {
	uint8_t algorithm{0}; // 0–7
	uint8_t ratioC{3};    // indexed harmonic ratios
	uint8_t ratioA{3};
	uint8_t ratioB{4}; // combined B1/B2 dial
	uint8_t harmonics{64};
	uint8_t detune{0};
	uint8_t feedback{0};
	uint8_t mix{64}; // X/Y crossfade

	uint8_t aEnvAtk{0};
	uint8_t aEnvDec{32};
	uint8_t aEnvEnd{127};
	uint8_t aLevel{64};
	uint8_t bEnvAtk{0};
	uint8_t bEnvDec{32};
	uint8_t bEnvEnd{127};
	uint8_t bLevel{64};

	uint8_t aDelay{0};
	uint8_t aTrig{1};
	uint8_t aEnvReset{1};
	uint8_t phaseReset{1}; // 0 off, 1 all, …
	uint8_t bDelay{0};
	uint8_t bTrig{1};
	uint8_t bEnvReset{1};

	uint8_t ratioCOffset{64};
	uint8_t ratioAOffset{64};
	uint8_t ratioB1Offset{64};
	uint8_t ratioB2Offset{64};
	uint8_t keyTrackA{0};
	uint8_t keyTrackB1{0};
	uint8_t keyTrackB2{0};

	void initDefaults();
};

struct FmDrumPatch {
	uint8_t tune{64};
	uint8_t sweepTime{40};
	uint8_t sweepDepth{80};
	uint8_t algorithm{0}; // 0–6
	uint8_t opCWave{0};
	uint8_t opABWave{0};
	uint8_t feedback{0};
	uint8_t waveFold{0};

	uint8_t ratioA{32};
	uint8_t decayA{40};
	uint8_t endA{64};
	uint8_t modA{80};
	uint8_t ratioB{48};
	uint8_t decayB{50};
	uint8_t endB{64};
	uint8_t modB{60};

	uint8_t bodyHold{0};
	uint8_t bodyDecay{80};
	uint8_t opCPhase{0};
	uint8_t bodyLevel{127};
	uint8_t noiseReset{0};
	uint8_t noiseRingMod{0};

	uint8_t noiseHold{0};
	uint8_t noiseDecay{50};
	uint8_t drumTransient{20};
	uint8_t transientLevel{64};
	uint8_t noiseBase{40};
	uint8_t noiseWidth{80};
	uint8_t noiseGrain{0};
	uint8_t noiseLevel{40};

	void initDefaults();
};

struct WaveTonePatch {
	uint8_t osc1Wave{0};
	uint8_t osc1PhaseDist{50};
	uint8_t osc1Level{100};
	uint8_t osc2Wave{0};
	uint8_t osc2PhaseDist{50};
	uint8_t osc2Level{100};

	uint8_t osc1LinOffset{64};
	uint8_t osc1WaveTable{0}; // 0 prim, 1 harm
	uint8_t oscMod{0};        // off / RM / RM fixed / hard sync
	uint8_t phaseReset{1};
	uint8_t osc2LinOffset{64};
	uint8_t osc2WaveTable{0};
	uint8_t oscDrift{0};

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

enum class PercRole : uint8_t {
	Kick = 0,
	Snare,
	HH,
	Tom,
	Clap,
	Cymbal,
	COUNT,
};

struct PercPatch {
	uint8_t role{0}; // PercRole
	uint8_t pitch{64};
	uint8_t pitchEnv{80}; // amount
	uint8_t pitchEnvTime{40};
	uint8_t color{64};
	uint8_t tone{64};
	uint8_t click{64};
	uint8_t drive{20};

	uint8_t noiseLevel{40};
	uint8_t noiseFilter{64};    // LP←→HP style
	uint8_t noiseFilterType{0}; // 0 LP 1 HP 2 BP
	uint8_t noiseDecay{40};
	uint8_t bodyDecay{70};
	uint8_t hold{0};

	void initDefaults();
	void applyRoleDefaults();
};

// Per-voice runtime for machine render
struct MachineVoiceState {
	uint32_t phase[4]{};
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
