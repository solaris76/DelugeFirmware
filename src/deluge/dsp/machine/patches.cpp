#include "dsp/machine/patches.h"

namespace deluge::dsp::machine {

void FmTonePatch::initDefaults() {
	*this = FmTonePatch{};
}

void FmDrumPatch::initDefaults() {
	*this = FmDrumPatch{};
	sweepTime = 28;
	sweepDepth = 90;
	bodyDecay = 50;
	noiseDecay = 28;
	noiseLevel = 30;
	transientLevel = 70;
}

void WaveTonePatch::initDefaults() {
	*this = WaveTonePatch{};
}

void PercPatch::initDefaults() {
	*this = PercPatch{};
	applyRoleDefaults();
}

void PercPatch::applyRoleDefaults() {
	switch (static_cast<PercRole>(role)) {
	case PercRole::Kick:
		pitch = 40;
		pitchEnv = 100;
		pitchEnvTime = 28; // short bend
		color = 30;
		tone = 20;
		click = 80;
		drive = 40;
		noiseLevel = 10;
		noiseDecay = 12;
		bodyDecay = 48; // ~mid of short-biased 1–50 range
		break;
	case PercRole::Snare:
		pitch = 70;
		pitchEnv = 50;
		pitchEnvTime = 16;
		color = 60;
		tone = 70;
		click = 60;
		drive = 20;
		noiseLevel = 90;
		noiseFilterType = 1;
		noiseDecay = 32;
		bodyDecay = 22;
		break;
	case PercRole::HH:
		pitch = 100;
		pitchEnv = 10;
		pitchEnvTime = 8;
		color = 90;
		tone = 80;
		click = 40;
		drive = 10;
		noiseLevel = 110;
		noiseFilterType = 1;
		noiseDecay = 18;
		bodyDecay = 8;
		break;
	case PercRole::Tom:
		pitch = 55;
		pitchEnv = 70;
		pitchEnvTime = 30;
		color = 50;
		tone = 40;
		click = 50;
		drive = 15;
		noiseLevel = 20;
		noiseDecay = 14;
		bodyDecay = 40;
		break;
	case PercRole::Clap:
		pitch = 80;
		pitchEnv = 20;
		pitchEnvTime = 12;
		color = 70;
		tone = 90;
		click = 20;
		drive = 25;
		noiseLevel = 100;
		noiseFilterType = 2;
		noiseDecay = 28;
		bodyDecay = 16;
		hold = 6;
		break;
	case PercRole::Cymbal:
		pitch = 90;
		pitchEnv = 15;
		pitchEnvTime = 40;
		color = 100;
		tone = 85;
		click = 30;
		drive = 10;
		noiseLevel = 100;
		noiseFilterType = 1;
		noiseDecay = 70; // longer noise wash uses upper dial
		bodyDecay = 55;
		break;
	default:
		break;
	}
}

void MachineVoiceState::reset() {
	for (auto& p : phase) {
		p = 0;
	}
	noiseState = 1;
	sampleCount = 0;
	clickSamplesLeft = 0;
	envA = 0.f;
	envB = 0.f;
	bodyEnv = 0.f;
	noiseEnv = 0.f;
	pitchEnv = 1.f;
	noteOn = true;
}

} // namespace deluge::dsp::machine
