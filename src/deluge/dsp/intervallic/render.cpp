#include "dsp/intervallic/render.h"
#include "definitions_cxx.hpp"
#include "dsp/oscillators/sine_osc.h"
#include "dsp/phi_gendy.hpp"
#include "dsp/phi_morph.hpp"
#include "dsp/phi_stair.hpp"
#include "dsp/phi_swarm.hpp"
#include "dsp/phi_vox.hpp"
#include "dsp/phi_weave.hpp"
#include "storage/wave_table/wave_table.h"
#include "storage/wave_table/wave_table_holder.h"
#include "util/functions.h"
#include <algorithm>
#include <cstring>

namespace deluge::dsp::intervallic {

namespace {

uint32_t applyInterval(uint32_t rootPhaseInc, int32_t semitones, int32_t cents) {
	while (semitones >= 12) {
		rootPhaseInc <<= 1;
		semitones -= 12;
		if (rootPhaseInc & 0x80000000u) {
			break;
		}
	}
	while (semitones <= -12) {
		rootPhaseInc >>= 1;
		semitones += 12;
	}
	static constexpr uint32_t kSemitoneQ16[12] = {
	    65536, 69433, 73562, 77936, 82570, 87480, 92681, 98193, 104032, 110218, 116772, 123715,
	};
	if (semitones > 0 && semitones < 12) {
		rootPhaseInc = static_cast<uint32_t>((static_cast<uint64_t>(rootPhaseInc) * kSemitoneQ16[semitones]) >> 16);
	}
	else if (semitones < 0 && semitones > -12) {
		rootPhaseInc = static_cast<uint32_t>((static_cast<uint64_t>(rootPhaseInc) << 16) / kSemitoneQ16[-semitones]);
	}
	if (cents != 0) {
		int32_t adj = 65536 + (cents * 38);
		if (adj < 1) {
			adj = 1;
		}
		rootPhaseInc = static_cast<uint32_t>((static_cast<uint64_t>(rootPhaseInc) * static_cast<uint32_t>(adj)) >> 16);
	}
	return rootPhaseInc;
}

void advanceLocalLfos(const Patch& patch, VoiceState& state, std::array<int32_t, 4>& out) {
	for (int32_t i = 0; i < 4; i++) {
		uint32_t rate = 1u + (static_cast<uint32_t>(patch.localLfos[i].rate) << 8);
		state.lfoPhase[i] += rate;
		out[i] = SineOsc::doFMNew(state.lfoPhase[i], 0) >> 16;
	}
}

int32_t sampleBasicWave(OscType wave, uint32_t phase, uint32_t fmMod) {
	switch (wave) {
	case OscType::TRIANGLE: {
		uint32_t p = phase + (fmMod << 8);
		int32_t t = static_cast<int32_t>(p);
		t ^= t >> 31;
		return (t - 0x40000000) << 1;
	}
	case OscType::SAW:
	case OscType::ANALOG_SAW_2:
		return static_cast<int32_t>(phase + (fmMod << 8));
	case OscType::SQUARE:
	case OscType::ANALOG_SQUARE: {
		uint32_t p = phase + (fmMod << 8);
		return (p < 0x80000000u) ? 0x3FFFFFFF : -0x40000000;
	}
	case OscType::SINE:
	default:
		return SineOsc::doFMNew(phase, fmMod);
	}
}

void smoothCrossfade(q31_t& smoothed, q31_t target) {
	if (smoothed == INT32_MIN) {
		smoothed = target;
	}
	else if (smoothed != target) {
		q31_t diff = target - smoothed;
		smoothed += (std::abs(diff) < 256) ? diff : (diff >> 2);
	}
}

template <typename Cache, typename BuildFn>
void updatePhiBanks(Cache& cache, Partial& part, BuildFn build) {
	float effOffA = part.phiPhaseA + part.gamma;
	float effOffB = part.phiPhaseB + part.gamma;
	if (cache.needsUpdate(part.phiZoneA, part.phiZoneB, effOffA, effOffB)) {
		cache.bankA = build(part.phiZoneA, effOffA);
		cache.bankB = build(part.phiZoneB, effOffB);
		cache.prevZoneA = part.phiZoneA;
		cache.prevZoneB = part.phiZoneB;
		cache.prevPhaseOffsetA = effOffA;
		cache.prevPhaseOffsetB = effOffB;
		if constexpr (requires { cache.prevCrossfade = INT32_MIN; }) {
			cache.prevCrossfade = INT32_MIN;
		}
	}
}

} // namespace

void render(Patch& patch, VoiceState& state, int32_t* oscBuffer, int32_t numSamples, uint32_t rootPhaseInc,
            int32_t amplitude, int32_t amplitudeIncrement) {
	if (rootPhaseInc == 0 || numSamples <= 0) {
		return;
	}

	std::array<uint32_t, kNumPartials> phaseInc{};
	std::array<int32_t, kNumPartials> baseLevels{};
	std::array<bool, kNumPartials> active{};
	std::array<bool, kNumPartials> isBasic{};
	int32_t numActive = 0;

	for (int32_t p = 0; p < kNumPartials; p++) {
		auto& part = patch.partials[p];
		active[p] = part.enabled && part.level > 0;
		if (!active[p]) {
			continue;
		}
		numActive++;
		baseLevels[p] = part.level;
		phaseInc[p] = applyInterval(rootPhaseInc, patch.effectiveInterval(p), part.detuneCents);
		isBasic[p] = !(part.isPhiFamily() || part.wave == OscType::WAVETABLE);
	}
	if (numActive == 0) {
		return;
	}

	const int32_t levelScale = std::max(int32_t{1}, numActive);
	static int32_t tempBuf[SSI_TX_BUFFER_NUM_SAMPLES] __attribute__((aligned(CACHE_LINE_SIZE)));
	static int32_t mixBuf[SSI_TX_BUFFER_NUM_SAMPLES] __attribute__((aligned(CACHE_LINE_SIZE)));
	memset(mixBuf, 0, numSamples * sizeof(int32_t));

	// PHI / WT into mix (buffer paths)
	for (int32_t p = 0; p < kNumPartials; p++) {
		if (!active[p] || isBasic[p]) {
			continue;
		}
		auto& part = patch.partials[p];
		if (part.wave == OscType::WAVETABLE) {
			auto* holder = part.waveTableHolder;
			auto* wt = holder != nullptr ? static_cast<WaveTable*>(holder->audioFile) : nullptr;
			if (wt == nullptr) {
				continue;
			}
			memset(tempBuf, 0, numSamples * sizeof(int32_t));
			int32_t waveIndex = part.waveIndex + 1073741824;
			state.phase[p] =
			    wt->render(tempBuf, numSamples, phaseInc[p], state.phase[p], false, 0, 0, 0, 0xFFFFFFFF, waveIndex, 0);
		}
		else if (part.isPhiFamily()) {
			// Ensure correct cache and render
			patch.ensurePhiCache(p);
			memset(tempBuf, 0, numSamples * sizeof(int32_t));
			int32_t* end = tempBuf + numSamples;
			q31_t targetXf = part.waveIndex;

			if (part.wave == OscType::PHI_MORPH && part.phiMorphCache) {
				auto& cache = *part.phiMorphCache;
				updatePhiBanks(cache, part, buildPhiMorphWavetable);
				smoothCrossfade(cache.smoothedCrossfade, targetXf);
				renderPhiMorph(cache, tempBuf, end, numSamples, phaseInc[p], &state.phase[p], 0xFFFFFFFF, 0, 0, false,
				               cache.smoothedCrossfade, 0);
			}
			else if (part.wave == OscType::PHI_WEAVE && part.phiWeaveCache) {
				auto& cache = *part.phiWeaveCache;
				updatePhiBanks(cache, part, buildPhiWeaveParams);
				smoothCrossfade(cache.smoothedCrossfade, targetXf);
				renderPhiWeave(cache, tempBuf, end, numSamples, phaseInc[p], &state.phase[p], 0xFFFFFFFF, 0, 0, false,
				               cache.smoothedCrossfade, 0);
			}
			else if (part.wave == OscType::PHI_VOX && part.phiVoxCache) {
				auto& cache = *part.phiVoxCache;
				updatePhiBanks(cache, part, buildPhiVoxParams);
				smoothCrossfade(cache.smoothedCrossfade, targetXf);
				renderPhiVox(cache, tempBuf, end, numSamples, phaseInc[p], &state.phase[p], &state.swarmSlavePhases[p],
				             0xFFFFFFFF, 0, 0, false, cache.smoothedCrossfade, 0, 0);
			}
			else if (part.wave == OscType::PHI_SWARM && part.phiSwarmCache) {
				auto& cache = *part.phiSwarmCache;
				updatePhiBanks(cache, part, buildPhiSwarmParams);
				smoothCrossfade(cache.smoothedCrossfade, targetXf);
				renderPhiSwarm(cache, tempBuf, end, numSamples, phaseInc[p], &state.phase[p],
				               &state.swarmSlavePhases[p], 0xFFFFFFFF, 0, 0, false, cache.smoothedCrossfade, 0);
			}
			else if (part.wave == OscType::PHI_GENDY && part.phiGendyCache) {
				auto& cache = *part.phiGendyCache;
				updatePhiBanks(cache, part, buildPhiGendyParams);
				smoothCrossfade(cache.smoothedCrossfade, targetXf);
				renderPhiGendy(cache, tempBuf, end, numSamples, phaseInc[p], &state.phase[p], 0xFFFFFFFF, 0, 0, false,
				               cache.smoothedCrossfade, 0);
			}
			else if (part.wave == OscType::PHI_STAIR && part.phiStairCache) {
				auto& cache = *part.phiStairCache;
				updatePhiBanks(cache, part, buildPhiStairParams);
				smoothCrossfade(cache.smoothedCrossfade, targetXf);
				renderPhiStair(cache, tempBuf, end, numSamples, phaseInc[p], &state.phase[p], 0xFFFFFFFF, 0, 0, false,
				               cache.smoothedCrossfade, 0);
			}
		}

		int32_t scaledLevel = (baseLevels[p] << 22) / levelScale;
		for (int32_t i = 0; i < numSamples; i++) {
			mixBuf[i] += multiply_32x32_rshift32(tempBuf[i], scaledLevel);
		}
	}

	// Basic waves sample loop + soft pair FM between basics
	int32_t amp = amplitude;
	std::array<int32_t, 4> lfoVals{};
	for (int32_t i = 0; i < numSamples; i++) {
		advanceLocalLfos(patch, state, lfoVals);
		int32_t mix = mixBuf[i];
		int32_t prevSample = 0;
		for (int32_t p = 0; p < kNumPartials; p++) {
			if (!active[p] || !isBasic[p]) {
				continue;
			}
			auto& part = patch.partials[p];
			int32_t level = baseLevels[p];
			int32_t cents = part.detuneCents;
			if (part.lfoDepthLevel || part.lfoDepthDetune) {
				int32_t lfo = lfoVals[part.lfoIndex & 3];
				if (part.lfoDepthLevel) {
					level += (lfo * part.lfoDepthLevel) >> 16;
					level = std::clamp(level, int32_t{0}, int32_t{255});
				}
				if (part.lfoDepthDetune) {
					cents += (lfo * part.lfoDepthDetune) >> 18;
					cents = std::clamp(cents, int32_t{-100}, int32_t{100});
					phaseInc[p] = applyInterval(rootPhaseInc, patch.effectiveInterval(p), cents);
				}
			}
			uint32_t mod = 0;
			if (patch.pairFmAmount > 0 && p > 0) {
				mod = static_cast<uint32_t>((std::abs(prevSample) >> 16) * patch.pairFmAmount);
			}
			state.phase[p] += phaseInc[p];
			int32_t s = sampleBasicWave(part.wave, state.phase[p], mod);
			int32_t scaledLevel = (level << 22) / levelScale;
			s = multiply_32x32_rshift32(s, scaledLevel);
			mix += s;
			prevSample = s;
		}
		amp += amplitudeIncrement;
		oscBuffer[i] += multiply_32x32_rshift32(mix, amp) << 1;
	}
}

} // namespace deluge::dsp::intervallic
