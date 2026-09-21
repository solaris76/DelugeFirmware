#include "dsp/machine/patches.h"
#include <algorithm>

namespace deluge::dsp::machine {

void FmTonePatch::initDefaults() {
	*this = FmTonePatch{};
}

void FmDrumPatch::initDefaults() {
	*this = FmDrumPatch{};
}

void WaveTonePatch::initDefaults() {
	*this = WaveTonePatch{};
}

void PercPatch::initDefaults() {
	*this = PercPatch{};
	applyRoleDefaults();
}

void PercPatch::loadRoleDefaults(uint8_t newRole) {
	*this = PercPatch{};
	role = static_cast<uint8_t>(std::min<uint8_t>(newRole, static_cast<uint8_t>(PercRole::COUNT) - 1));
	applyRoleDefaults();
}

void PercPatch::applyRoleDefaults() {
	switch (static_cast<PercRole>(role)) {
	case PercRole::Metal:
		pitch = 88;
		color = 75;
		noise = 25;
		decay = 45;
		crunch = 50;
		break;
	case PercRole::Bell:
		pitch = 82;
		color = 40;
		noise = 8;
		decay = 58;
		crunch = 45;
		break;
	case PercRole::Hat808:
		pitch = 100;
		color = 55;
		noise = 90;
		decay = 28;
		crunch = 55;
		break;
	case PercRole::FM:
		pitch = 78;
		color = 90;
		noise = 18;
		decay = 40;
		crunch = 48;
		break;
	case PercRole::XOR:
		pitch = 92;
		color = 80;
		noise = 30;
		decay = 36;
		crunch = 60;
		break;
	case PercRole::Grains:
		pitch = 70;
		color = 55;
		noise = 118;
		decay = 52;
		crunch = 25;
		break;
	default:
		break;
	}
}

void SkinPatch::initDefaults() {
	*this = SkinPatch{};
	applyModeDefaults();
}

void SkinPatch::loadModeDefaults(uint8_t newMode) {
	*this = SkinPatch{};
	mode = static_cast<uint8_t>(std::min<uint8_t>(newMode, static_cast<uint8_t>(SkinMode::COUNT) - 1));
	applyModeDefaults();
}

void SkinPatch::applyModeDefaults() {
	switch (static_cast<SkinMode>(mode)) {
	case SkinMode::Skin:
		pitch = 28;
		harm = 95;
		morph = 15;
		fold = 18;
		decay = 78;
		break;
	case SkinMode::Liquid:
		pitch = 22;
		harm = 105;
		morph = 10;
		fold = 32;
		decay = 70;
		break;
	case SkinMode::Metal:
		pitch = 72;
		harm = 115;
		morph = 55;
		fold = 48;
		decay = 55;
		break;
	default:
		break;
	}
}

void ResonatorPatch::initDefaults() {
	*this = ResonatorPatch{};
	applyModelDefaults();
}

void ResonatorPatch::loadModelDefaults(uint8_t newModel) {
	*this = ResonatorPatch{};
	model = static_cast<uint8_t>(std::min<uint8_t>(newModel, static_cast<uint8_t>(ResonatorModel::COUNT) - 1));
	applyModelDefaults();
}

void ResonatorPatch::applyModelDefaults() {
	switch (static_cast<ResonatorModel>(model)) {
	case ResonatorModel::Modal:
		structure = 55;
		brightness = 75;
		damping = 60;
		position = 35;
		excite = 80;
		break;
	case ResonatorModel::Strings:
		structure = 70;
		brightness = 55;
		damping = 85;
		position = 50;
		excite = 65;
		break;
	case ResonatorModel::Wire:
		structure = 90;
		brightness = 60;
		damping = 45;
		position = 40;
		excite = 90;
		break;
	default:
		break;
	}
}

void MachineVoiceState::reset() {
	for (auto& p : phase) {
		p = 0;
	}
	for (auto& e : oscEnv) {
		e = 0.f;
	}
	for (auto& z : resZ1) {
		z = 0.f;
	}
	for (auto& z : resZ2) {
		z = 0.f;
	}
	for (auto& c : combBuf) {
		c = 0.f;
	}
	combPos = 0;
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
