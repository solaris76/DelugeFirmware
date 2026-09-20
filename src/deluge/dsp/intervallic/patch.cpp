#include "dsp/intervallic/patch.h"
#include "dsp/phi_gendy.hpp"
#include "dsp/phi_morph.hpp"
#include "dsp/phi_stair.hpp"
#include "dsp/phi_swarm.hpp"
#include "dsp/phi_vox.hpp"
#include "dsp/phi_weave.hpp"
#include "memory/memory_allocator_interface.h"
#include "storage/wave_table/wave_table_holder.h"

namespace deluge::dsp::intervallic {

void Partial::freeCaches() {
	delete phiMorphCache;
	phiMorphCache = nullptr;
	delete phiWeaveCache;
	phiWeaveCache = nullptr;
	delete phiVoxCache;
	phiVoxCache = nullptr;
	delete phiSwarmCache;
	phiSwarmCache = nullptr;
	delete phiGendyCache;
	phiGendyCache = nullptr;
	delete phiStairCache;
	phiStairCache = nullptr;
	if (waveTableHolder != nullptr) {
		waveTableHolder->~WaveTableHolder();
		delugeDealloc(waveTableHolder);
		waveTableHolder = nullptr;
	}
}

Patch::Patch() {
	initDefaultM9();
}

Patch::~Patch() {
	for (auto& p : partials) {
		p.freeCaches();
	}
}

void Patch::applyIntervals(const int8_t* intervals, int32_t activeCount) {
	for (int32_t i = 0; i < kNumPartials; i++) {
		// Preserve caches/holders across lattice preset changes — only reset musical fields
		auto* wt = partials[i].waveTableHolder;
		auto morph = partials[i].phiMorphCache;
		auto weave = partials[i].phiWeaveCache;
		auto vox = partials[i].phiVoxCache;
		auto swarm = partials[i].phiSwarmCache;
		auto gendy = partials[i].phiGendyCache;
		auto stair = partials[i].phiStairCache;

		partials[i] = {};
		partials[i].waveTableHolder = wt;
		partials[i].phiMorphCache = morph;
		partials[i].phiWeaveCache = weave;
		partials[i].phiVoxCache = vox;
		partials[i].phiSwarmCache = swarm;
		partials[i].phiGendyCache = gendy;
		partials[i].phiStairCache = stair;
		partials[i].wave = OscType::SINE;

		if (i < activeCount) {
			partials[i].intervalSemitones = intervals[i];
			partials[i].level = 100;
			partials[i].enabled = true;
			partials[i].pan = static_cast<int8_t>((i - 3) * 8);
		}
		else {
			partials[i].level = 0;
			partials[i].enabled = false;
		}
	}
}

void Patch::initDefaultM9() {
	static constexpr int8_t kTwo[] = {0, 3};
	applyIntervals(kTwo, 2);
	latticePreset = 0;
	inversion = 0;
	voiceSpread = 0;
	pairFmAmount = 0;
	stereoSpread = 32;
	playMode = PlayMode::Latch;
	for (int32_t i = 0; i < 4; i++) {
		localLfos[i] = {};
		localLfos[i].rate = static_cast<uint8_t>(40 + i * 20);
	}
}

void Patch::applyLatticePreset(int32_t presetIndex) {
	latticePreset = static_cast<uint8_t>(std::clamp(presetIndex, int32_t{0}, int32_t{5}));
	switch (latticePreset) {
	case 0: {
		static constexpr int8_t i[] = {0, 3, 7, 10, 14, 12};
		applyIntervals(i, 6);
		break;
	}
	case 1: {
		static constexpr int8_t i[] = {0, 3, 7, 10, 14, 21};
		applyIntervals(i, 6);
		break;
	}
	case 2: {
		static constexpr int8_t i[] = {0, 3, 7, 10, 14, 17};
		applyIntervals(i, 6);
		break;
	}
	case 3: {
		static constexpr int8_t i[] = {0, 7, 10, 15, 14, 12};
		applyIntervals(i, 6);
		break;
	}
	case 4: {
		static constexpr int8_t i[] = {0, -12, -5, 3, 7, 10};
		applyIntervals(i, 6);
		break;
	}
	case 5: {
		static constexpr int8_t i[] = {0, 2, 7, 9, 14, 12};
		applyIntervals(i, 6);
		break;
	}
	default:
		initDefaultM9();
		break;
	}
}

WaveTableHolder* Patch::ensureWaveTableHolder(int32_t partialIndex) {
	if (partialIndex < 0 || partialIndex >= kNumPartials) {
		return nullptr;
	}
	auto& p = partials[partialIndex];
	if (p.waveTableHolder == nullptr) {
		void* memory = allocMaxSpeed(sizeof(WaveTableHolder));
		if (memory == nullptr) {
			return nullptr;
		}
		p.waveTableHolder = new (memory) WaveTableHolder();
	}
	return p.waveTableHolder;
}

void Patch::ensurePhiCache(int32_t partialIndex) {
	if (partialIndex < 0 || partialIndex >= kNumPartials) {
		return;
	}
	auto& p = partials[partialIndex];
	switch (p.wave) {
	case OscType::PHI_MORPH:
		if (p.phiMorphCache == nullptr) {
			p.phiMorphCache = new PhiMorphCache{};
		}
		break;
	case OscType::PHI_WEAVE:
		if (p.phiWeaveCache == nullptr) {
			p.phiWeaveCache = new PhiWeaveCache{};
		}
		break;
	case OscType::PHI_VOX:
		if (p.phiVoxCache == nullptr) {
			p.phiVoxCache = new PhiVoxCache{};
		}
		break;
	case OscType::PHI_SWARM:
		if (p.phiSwarmCache == nullptr) {
			p.phiSwarmCache = new PhiSwarmCache{};
		}
		break;
	case OscType::PHI_GENDY:
		if (p.phiGendyCache == nullptr) {
			p.phiGendyCache = new PhiGendyCache{};
		}
		break;
	case OscType::PHI_STAIR:
		if (p.phiStairCache == nullptr) {
			p.phiStairCache = new PhiStairCache{};
		}
		break;
	default:
		break;
	}
}

int32_t Patch::effectiveInterval(int32_t partialIndex) const {
	if (partialIndex < 0 || partialIndex >= kNumPartials) {
		return 0;
	}
	std::array<int32_t, kNumPartials> sorted{};
	int32_t n = 0;
	for (int32_t i = 0; i < kNumPartials; i++) {
		if (partials[i].enabled && partials[i].level > 0) {
			sorted[n++] = partials[i].intervalSemitones;
		}
	}
	if (n == 0) {
		return partials[partialIndex].intervalSemitones;
	}
	std::sort(sorted.begin(), sorted.begin() + n);

	int32_t inv = inversion;
	while (inv < 0) {
		inv += n;
	}
	inv %= n;

	int32_t base = partials[partialIndex].intervalSemitones;
	int32_t rank = 0;
	for (int32_t i = 0; i < n; i++) {
		if (sorted[i] == base) {
			rank = i;
			break;
		}
		if (sorted[i] < base) {
			rank = i + 1;
		}
	}
	rank = (rank + inv) % n;
	int32_t out = sorted[rank];
	if (voiceSpread > 0 && rank >= n / 2) {
		out += 12 * ((voiceSpread + 63) / 64);
		if (voiceSpread > 160) {
			out += 12;
		}
	}
	return out;
}

} // namespace deluge::dsp::intervallic
