/*
 * Intervallic drone engine — multi-partial interval lattice inside one OscType.
 */
#pragma once

#include "definitions_cxx.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

class WaveTableHolder;

namespace deluge::dsp {
struct PhiMorphCache;
struct PhiWeaveCache;
struct PhiVoxCache;
struct PhiSwarmCache;
struct PhiGendyCache;
struct PhiStairCache;
} // namespace deluge::dsp

namespace deluge::dsp::intervallic {

constexpr int32_t kNumPartials = 8;
constexpr int32_t kDefaultActivePartials = 6;

enum class PlayMode : uint8_t {
	Latch = 0,
	Env = 1,
};

struct Partial {
	int8_t intervalSemitones{0};
	int8_t detuneCents{0};
	uint8_t level{200};
	int8_t pan{0};
	OscType wave{OscType::SINE};
	bool enabled{true};

	uint8_t lfoIndex{0};
	uint8_t lfoPhaseOffset{0};
	uint8_t lfoDepthLevel{0};
	uint8_t lfoDepthDetune{0};
	uint8_t lfoDepthPan{0};

	// PHI (unified fields; interpreted by current wave type)
	uint16_t phiZoneA{0};
	uint16_t phiZoneB{0};
	float phiPhaseA{0.0f};
	float phiPhaseB{0.0f};
	float gamma{0.0f};
	int32_t waveIndex{0}; // morph / WT position (patched-param style, ~±1<<30)

	WaveTableHolder* waveTableHolder{nullptr};

	// Lazy PHI caches (only one non-null matching `wave`)
	deluge::dsp::PhiMorphCache* phiMorphCache{nullptr};
	deluge::dsp::PhiWeaveCache* phiWeaveCache{nullptr};
	deluge::dsp::PhiVoxCache* phiVoxCache{nullptr};
	deluge::dsp::PhiSwarmCache* phiSwarmCache{nullptr};
	deluge::dsp::PhiGendyCache* phiGendyCache{nullptr};
	deluge::dsp::PhiStairCache* phiStairCache{nullptr};

	[[nodiscard]] bool isPhiFamily() const {
		return wave == OscType::PHI_MORPH || wave == OscType::PHI_STAIR || wave == OscType::PHI_WEAVE
		       || wave == OscType::PHI_VOX || wave == OscType::PHI_SWARM || wave == OscType::PHI_GENDY;
	}

	void freeCaches();
};

struct LocalLfo {
	uint8_t rate{64};
	uint8_t shape{0};
};

struct Patch {
	std::array<Partial, kNumPartials> partials{};
	std::array<LocalLfo, 4> localLfos{};
	uint8_t latticePreset{0};
	int8_t inversion{0};
	uint8_t voiceSpread{0};
	uint8_t pairFmAmount{0};
	uint8_t stereoSpread{0};
	PlayMode playMode{PlayMode::Latch};

	Patch();
	~Patch();

	void applyIntervals(const int8_t* intervals, int32_t activeCount);
	void initDefaultM9();
	void applyLatticePreset(int32_t presetIndex);

	WaveTableHolder* ensureWaveTableHolder(int32_t partialIndex);
	void ensurePhiCache(int32_t partialIndex);

	[[nodiscard]] int32_t effectiveInterval(int32_t partialIndex) const;
};

} // namespace deluge::dsp::intervallic
