#pragma once

#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/sound/sound.h"
#include "storage/audio/audio_file_holder.h"
#include "storage/multi_range/multi_range.h"
#include "util/functions.h"
#include <cstdio>
#include <cstring>

class JsonSerializer;
class ParamManagerForTimeline;

namespace SoundSysex {

inline const char* filterModeToString(FilterMode mode) {
	switch (mode) {
	case FilterMode::TRANSISTOR_24DB:
		return "24dB";
	case FilterMode::TRANSISTOR_24DB_DRIVE:
		return "24dBDrive";
	case FilterMode::TRANSISTOR_12DB:
		return "12dB";
	case FilterMode::HPLADDER:
		return "HPLadder";
	case FilterMode::SVF_BAND:
		return "SVFBand";
	case FilterMode::SVF_NOTCH:
		return "SVFNotch";
	default:
		return "24dB";
	}
}

inline const char* modFXTypeToString(ModFXType type) {
	switch (type) {
	case ModFXType::NONE:
		return "none";
	case ModFXType::FLANGER:
		return "flanger";
	case ModFXType::CHORUS:
		return "chorus";
	case ModFXType::PHASER:
		return "phaser";
	case ModFXType::CHORUS_STEREO:
		return "chorusStereo";
	case ModFXType::WARBLE:
		return "warble";
	case ModFXType::GRAIN:
		return "grain";
	default:
		return "none";
	}
}

inline void writeSoundParameterSnapshot(JsonSerializer& writer, Sound* sound, ParamManagerForTimeline* paramManager) {
	using namespace deluge::modulation::params;

	if (!sound || !paramManager) {
		return;
	}

	const char* polyMode = "poly";
	switch (sound->polyphonic) {
	case PolyphonyMode::AUTO:
		polyMode = "auto";
		break;
	case PolyphonyMode::POLY:
		polyMode = "poly";
		break;
	case PolyphonyMode::MONO:
		polyMode = "mono";
		break;
	case PolyphonyMode::LEGATO:
		polyMode = "legato";
		break;
	case PolyphonyMode::CHOKE:
		polyMode = "choke";
		break;
	}
	writer.writeAttribute("polyphonic", polyMode);

	int32_t priority = 1;
	switch (sound->voicePriority) {
	case VoicePriority::LOW:
		priority = 0;
		break;
	case VoicePriority::MEDIUM:
		priority = 1;
		break;
	case VoicePriority::HIGH:
		priority = 2;
		break;
	}
	writer.writeAttribute("voicePriority", priority);

	const char* mode = "subtractive";
	switch (sound->synthMode) {
	case SynthMode::SUBTRACTIVE:
		mode = "subtractive";
		break;
	case SynthMode::FM:
		mode = "fm";
		break;
	case SynthMode::RINGMOD:
		mode = "ringmod";
		break;
	}
	writer.writeAttribute("mode", mode);

	writer.writeAttribute("transpose", sound->transpose);
	writer.writeAttribute("maxVoices", sound->maxVoiceCount);
	writer.writeAttribute("unisonNum", sound->numUnison);
	writer.writeAttribute("unisonDetune", sound->unisonDetune);
	writer.writeAttribute("unisonSpread", sound->unisonStereoSpread);

	for (int32_t s = 0; s < kNumSources; s++) {
		Source* source = &sound->sources[s];
		char prefix[10];
		::snprintf(prefix, sizeof(prefix), "osc%d", s + 1);

		char attrName[32];
		::snprintf(attrName, sizeof(attrName), "%sType", prefix);
		writer.writeAttribute(attrName, ::oscTypeToString(source->oscType));

		::snprintf(attrName, sizeof(attrName), "%sTranspose", prefix);
		writer.writeAttribute(attrName, sound->modulatorTranspose[s]);

		::snprintf(attrName, sizeof(attrName), "%sCents", prefix);
		writer.writeAttribute(attrName, sound->modulatorCents[s]);

		::snprintf(attrName, sizeof(attrName), "%sRetrigPhase", prefix);
		if (sound->oscRetriggerPhase[s] == 4294967295) {
			writer.writeAttribute(attrName, -1);
		}
		else {
			writer.writeAttribute(attrName, (int32_t)sound->oscRetriggerPhase[s]);
		}

		if (source->oscType == OscType::SAMPLE || source->oscType == OscType::WAVETABLE) {
			if (source->ranges.getNumElements() > 0) {
				MultiRange* range = source->ranges.getElement(0);
				if (range) {
					AudioFileHolder* holder = range->getAudioFileHolder();
					if (holder && holder->filePath.isEmpty() == false) {
						::snprintf(attrName, sizeof(attrName), "%sFile", prefix);
						writer.writeAttribute(attrName, holder->filePath.get());
					}
				}
			}
		}
	}

	writer.writeAttribute("osc2Sync", sound->oscillatorSync ? 1 : 0);

	if (paramManager->summaries[0].paramCollection) {
		auto* unpatchedParams = (UnpatchedParamSet*)paramManager->summaries[0].paramCollection;

		for (int32_t p = 0; p < UNPATCHED_SOUND_MAX_NUM; p++) {
			int32_t value = unpatchedParams->getValue(p);
			const char* paramName = paramNameForFile(Kind::UNPATCHED_SOUND, p + UNPATCHED_START);
			if (paramName && std::strcmp(paramName, "none") != 0) {
				writer.writeAttribute(paramName, value);
			}
		}
	}

	if (paramManager->summaries[1].paramCollection) {
		auto* patchedParams = (PatchedParamSet*)paramManager->summaries[1].paramCollection;

		for (int32_t p = 0; p < kNumParams; p++) {
			AutoParam* param = &patchedParams->params[p];
			const char* paramName = paramNameForFile(Kind::PATCHED, p);
			if (paramName && std::strcmp(paramName, "none") != 0) {
				writer.writeAttribute(paramName, param->getCurrentValue());
			}
		}
	}

	writer.writeAttribute("lpfMode", filterModeToString(sound->lpfMode));
	writer.writeAttribute("hpfMode", filterModeToString(sound->hpfMode));

	const char* routeName = "H2L";
	switch (sound->filterRoute) {
	case FilterRoute::HIGH_TO_LOW:
		routeName = "H2L";
		break;
	case FilterRoute::LOW_TO_HIGH:
		routeName = "L2H";
		break;
	case FilterRoute::PARALLEL:
		routeName = "parallel";
		break;
	}
	writer.writeAttribute("filterRoute", routeName);

	writer.writeAttribute("delayPingPong", sound->delay.pingPong ? 1 : 0);
	writer.writeAttribute("delayAnalog", sound->delay.analog ? 1 : 0);
	writer.writeAttribute("delaySyncLevel", (int32_t)sound->delay.syncLevel);
	writer.writeAttribute("delaySyncType", (int32_t)sound->delay.syncType);

	writer.writeAttribute("sidechainAttack", sound->sidechain.attack);
	writer.writeAttribute("sidechainRelease", sound->sidechain.release);
	writer.writeAttribute("sidechainSend", sound->sideChainSendLevel);

	writer.writeAttribute("clippingAmount", sound->clippingAmount);

	for (int32_t i = 0; i < LFO_COUNT; i++) {
		char attrName[32];
		::snprintf(attrName, sizeof(attrName), "lfo%dSyncLevel", i + 1);
		writer.writeAttribute(attrName, sound->lfoConfig[i].syncLevel);

		::snprintf(attrName, sizeof(attrName), "lfo%dSyncType", i + 1);
		writer.writeAttribute(attrName, sound->lfoConfig[i].syncType);
	}

	writer.writeAttribute("modFXType", modFXTypeToString(sound->getModFXType()));
}

} // namespace SoundSysex
