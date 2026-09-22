#include "dsp/machine/patches.h"
#include "gui/menu_item/value_scaling.h"
#include "modulation/params/param.h"
#include <algorithm>

namespace deluge::dsp::machine {

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

void SyOscPatch::initDefaults() {
	*this = SyOscPatch{};
	applyModeDefaults();
}

void SyOscPatch::loadModeDefaults(uint8_t newMode) {
	*this = SyOscPatch{};
	mode = static_cast<uint8_t>(std::min<uint8_t>(newMode, static_cast<uint8_t>(SyOscMode::COUNT) - 1));
	applyModeDefaults();
}

void SyOscPatch::applyModeDefaults() {
	switch (static_cast<SyOscMode>(mode)) {
	case SyOscMode::Dual:
		pitch = 72;
		sweep = 35;
		ratio = 64;
		color = 70;
		noise = 15;
		decay = 60;
		break;
	case SyOscMode::Sync:
		pitch = 70;
		sweep = 85;
		ratio = 95;
		color = 55;
		noise = 20;
		decay = 50;
		break;
	case SyOscMode::FM:
		pitch = 68;
		sweep = 45;
		ratio = 88;
		color = 60;
		noise = 12;
		decay = 55;
		break;
	case SyOscMode::Ring:
		pitch = 75;
		sweep = 40;
		ratio = 70;
		color = 65;
		noise = 25;
		decay = 48;
		break;
	case SyOscMode::Noise:
		pitch = 80;
		sweep = 30;
		ratio = 50;
		color = 75;
		noise = 100;
		decay = 35;
		break;
	case SyOscMode::Sweep:
		pitch = 65;
		sweep = 110;
		ratio = 100;
		color = 50;
		noise = 18;
		decay = 58;
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
	for (auto& c : combBuf) {
		c = 0.f;
	}
	combPos = 0;
	combLen = 64;
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

void collectMachineDials(OscType type, void const* patch, uint8_t out[kNumMachineDialParams]) {
	for (int i = 0; i < kNumMachineDialParams; i++) {
		out[i] = 0;
	}
	if (patch == nullptr) {
		return;
	}
	switch (type) {
	case OscType::FM_DRUM: {
		auto const& p = *static_cast<FmDrumPatch const*>(patch);
		out[0] = p.tune;
		out[1] = p.sweep;
		out[2] = p.mod;
		out[3] = p.fold;
		out[4] = p.decay;
		out[5] = p.noise;
		break;
	}
	case OscType::PERC: {
		auto const& p = *static_cast<PercPatch const*>(patch);
		out[0] = p.pitch;
		out[1] = p.color;
		out[2] = p.noise;
		out[3] = p.decay;
		out[4] = p.crunch;
		break;
	}
	case OscType::SKIN: {
		auto const& p = *static_cast<SkinPatch const*>(patch);
		out[0] = p.pitch;
		out[1] = p.harm;
		out[2] = p.morph;
		out[3] = p.fold;
		out[4] = p.decay;
		break;
	}
	case OscType::RESONATOR: {
		auto const& p = *static_cast<ResonatorPatch const*>(patch);
		out[0] = p.structure;
		out[1] = p.brightness;
		out[2] = p.damping;
		out[3] = p.position;
		out[4] = p.excite;
		break;
	}
	case OscType::SY_OSC: {
		auto const& p = *static_cast<SyOscPatch const*>(patch);
		out[0] = p.pitch;
		out[1] = p.sweep;
		out[2] = p.ratio;
		out[3] = p.color;
		out[4] = p.noise;
		out[5] = p.decay;
		break;
	}
	case OscType::WAVETONE: {
		auto const& p = *static_cast<WaveTonePatch const*>(patch);
		out[0] = p.osc1Wave;
		out[1] = p.osc1PhaseDist;
		out[2] = p.osc1Level;
		out[3] = p.osc2Wave;
		out[4] = p.osc2PhaseDist;
		out[5] = p.osc2Level;
		out[6] = p.oscDrift;
		out[7] = p.noiseLevel;
		out[8] = p.noiseCharacter;
		break;
	}
	default:
		break;
	}
}

void applyMachineDials(OscType type, void* patch, uint8_t const in[kNumMachineDialParams]) {
	if (patch == nullptr) {
		return;
	}
	switch (type) {
	case OscType::FM_DRUM: {
		auto& p = *static_cast<FmDrumPatch*>(patch);
		p.tune = in[0];
		p.sweep = in[1];
		p.mod = in[2];
		p.fold = in[3];
		p.decay = in[4];
		p.noise = in[5];
		break;
	}
	case OscType::PERC: {
		auto& p = *static_cast<PercPatch*>(patch);
		p.pitch = in[0];
		p.color = in[1];
		p.noise = in[2];
		p.decay = in[3];
		p.crunch = in[4];
		break;
	}
	case OscType::SKIN: {
		auto& p = *static_cast<SkinPatch*>(patch);
		p.pitch = in[0];
		p.harm = in[1];
		p.morph = in[2];
		p.fold = in[3];
		p.decay = in[4];
		break;
	}
	case OscType::RESONATOR: {
		auto& p = *static_cast<ResonatorPatch*>(patch);
		p.structure = in[0];
		p.brightness = in[1];
		p.damping = in[2];
		p.position = in[3];
		p.excite = in[4];
		break;
	}
	case OscType::SY_OSC: {
		auto& p = *static_cast<SyOscPatch*>(patch);
		p.pitch = in[0];
		p.sweep = in[1];
		p.ratio = in[2];
		p.color = in[3];
		p.noise = in[4];
		p.decay = in[5];
		break;
	}
	case OscType::WAVETONE: {
		auto& p = *static_cast<WaveTonePatch*>(patch);
		p.osc1Wave = in[0];
		p.osc1PhaseDist = in[1];
		p.osc1Level = in[2];
		p.osc2Wave = in[3];
		p.osc2PhaseDist = in[4];
		p.osc2Level = in[5];
		p.oscDrift = in[6];
		p.noiseLevel = in[7];
		p.noiseCharacter = in[8];
		break;
	}
	default:
		break;
	}
}

void dialsFromParamFinals(int32_t const* paramFinalValues, uint8_t out[kNumMachineDialParams]) {
	namespace params = deluge::modulation::params;
	for (int i = 0; i < kNumMachineDialParams; i++) {
		out[i] = static_cast<uint8_t>(
		    computeCurrentValueForMachineDialHybrid(paramFinalValues[params::LOCAL_MACHINE_0 + i]));
	}
}

} // namespace deluge::dsp::machine
