/*
 * Intervallic lattice renderer — additive / pair-FM multi-wave stack.
 */
#pragma once

#include "dsp/intervallic/patch.h"
#include <array>
#include <cstdint>

namespace deluge::dsp::intervallic {

struct VoiceState {
	std::array<uint32_t, kNumPartials> phase{};
	std::array<uint32_t, 4> lfoPhase{};
	std::array<uint64_t, kNumPartials> swarmSlavePhases{};
};

void render(Patch& patch, VoiceState& state, int32_t* oscBuffer, int32_t numSamples, uint32_t rootPhaseInc,
            int32_t amplitude, int32_t amplitudeIncrement);

} // namespace deluge::dsp::intervallic
