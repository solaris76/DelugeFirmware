#pragma once

#include "dsp/machine/patches.h"
#include <cstdint>

namespace deluge::dsp::machine {

// Digitone-ish harmonic ratio tables (approx)
float ratioFromIndexC(uint8_t idx);
float ratioFromIndexA(uint8_t idx);
void unpackRatioB(uint8_t dial, float& b1, float& b2);

void renderFmTone(FmTonePatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                  uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement);

void renderFmDrum(FmDrumPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                  uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement);

void renderWaveTone(WaveTonePatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                    uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement);

void renderPerc(PercPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement);

} // namespace deluge::dsp::machine
