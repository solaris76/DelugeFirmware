/*
 * Copyright © 2024 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "io/midi/sysex/parameter_sysex.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/instrument.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include "modulation/params/param.h"
#include "modulation/params/param_collection.h"
#include "modulation/params/param_set.h"
#include "modulation/patch/patch_cable_set.h"
#include "processing/sound/sound.h"
#include "processing/sound/sound_instrument.h"
#include "storage/smsysex.h"
#include <cstring>

extern JsonSerializer jWriter;
extern Song* currentSong;

using namespace deluge::modulation::params;

namespace ParameterSysex {

// Track subscription state
static bool parametersSubscribed = false;

// Separate JsonSerializer for async notifications to avoid reentrancy
static JsonSerializer paramNotifyWriter;

// Helper to get the current Sound from the active clip
static Sound* getCurrentSound(ModelStackWithTimelineCounter** modelStackOut = nullptr) {
	if (!currentSong) {
		return nullptr;
	}

	Clip* clip = currentSong->getCurrentClip();
	if (!clip) {
		return nullptr;
	}

	Output* output = clip->output;
	if (!output || output->type != OutputType::SYNTH) {
		return nullptr;
	}

	Instrument* instrument = (Instrument*)output;
	if (instrument->type != OutputType::SYNTH) {
		return nullptr;
	}

	SoundInstrument* soundInstrument = (SoundInstrument*)instrument;

	if (modelStackOut) {
		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
		*modelStackOut = modelStack->addTimelineCounter(clip);
	}

	return (Sound*)soundInstrument;
}

// Helper to write oscillator type as string
static const char* getOscTypeString(OscType type) {
	switch (type) {
	case OscType::SQUARE:
		return "square";
	case OscType::SAW:
		return "saw";
	case OscType::SINE:
		return "sine";
	case OscType::TRIANGLE:
		return "triangle";
	case OscType::SAMPLE:
		return "sample";
	case OscType::WAVETABLE:
		return "wavetable";
	case OscType::INPUT_L:
		return "inputL";
	case OscType::INPUT_R:
		return "inputR";
	case OscType::INPUT_STEREO:
		return "inputStereo";
	default:
		return "square";
	}
}

// Helper to write LFO type as string
static const char* getLFOTypeString(LFOType type) {
	switch (type) {
	case LFOType::TRIANGLE:
		return "triangle";
	case LFOType::SINE:
		return "sine";
	case LFOType::SQUARE:
		return "square";
	case LFOType::SAW:
		return "saw";
	case LFOType::RANDOM_WALK:
		return "randomWalk";
	case LFOType::SAMPLE_AND_HOLD:
		return "sampleAndHold";
	default:
		return "triangle";
	}
}

// Helper to write filter mode as string
static const char* getFilterModeString(FilterMode mode) {
	switch (mode) {
	case FilterMode::TRANSISTOR_24DB:
		return "24dB";
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

// Helper to write ModFX type as string
static const char* getModFXTypeString(ModFXType type) {
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

// Get all parameters for the currently selected sound
void getParameters(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^parameters", false, true);

	Sound* sound = getCurrentSound();
	if (!sound) {
		jWriter.writeAttribute("error", "No synth sound selected");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	ModelStackWithTimelineCounter* modelStack = nullptr;
	getCurrentSound(&modelStack);

	if (!modelStack) {
		jWriter.writeAttribute("error", "Invalid model stack");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Check if this is an instrument clip
	InstrumentClip* clip = (InstrumentClip*)modelStack->getTimelineCounter();
	ParamManager* paramManager = &clip->paramManager;

	// === GENERAL SOUND ATTRIBUTES ===
	jWriter.writeOpeningTag("general", false, true);

	// Polyphonic mode
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
	jWriter.writeAttribute("polyphonic", polyMode);

	// Voice priority
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
	jWriter.writeAttribute("voicePriority", priority);

	// Synth mode
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
	jWriter.writeAttribute("mode", mode);

	jWriter.writeAttribute("transpose", sound->transpose);
	jWriter.writeAttribute("maxVoices", sound->maxVoiceCount);

	jWriter.closeTag(true);

	// === UNISON ===
	jWriter.writeOpeningTag("unison", false, true);
	jWriter.writeAttribute("num", sound->numUnison);
	jWriter.writeAttribute("detune", sound->unisonDetune);
	jWriter.writeAttribute("spread", sound->unisonStereoSpread);
	jWriter.closeTag(true);

	// === OSCILLATORS ===
	for (int32_t s = 0; s < kNumSources; s++) {
		const char* oscName = (s == 0) ? "osc1" : "osc2";
		jWriter.writeOpeningTag(oscName, false, true);

		Source* source = &sound->sources[s];

		jWriter.writeAttribute("type", getOscTypeString(source->oscType));
		jWriter.writeAttribute("transpose", sound->modulatorTranspose[s]);
		jWriter.writeAttribute("cents", sound->modulatorCents[s]);

		// Retrig phase (4294967295 = off)
		if (sound->oscRetriggerPhase[s] == 4294967295) {
			jWriter.writeAttribute("retrigPhase", -1);
		}
		else {
			jWriter.writeAttribute("retrigPhase", (int32_t)sound->oscRetriggerPhase[s]);
		}

		jWriter.closeTag(true);
	}

	// Osc sync
	jWriter.writeAttribute("osc2Sync", sound->oscillatorSync ? 1 : 0);

	// === PATCHED PARAMETERS (Local + Global) ===
	jWriter.writeOpeningTag("patched", false, true);

	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStack->addOtherTwoThingsButNoNoteRow(sound, paramManager);
	PatchedParamSet* patchedParams = (PatchedParamSet*)paramManager->getPatchedParamSet();

	// Loop through all patched parameters
	for (int32_t p = 0; p < kNumParams; p++) {
		AutoParam* param = &patchedParams->params[p];
		int32_t value = param->getCurrentValue();
		const char* paramName = paramNameForFile(Kind::PATCHED, p);
		if (paramName) {
			jWriter.writeAttribute(paramName, value);
		}
	}

	jWriter.closeTag(true);

	// === UNPATCHED PARAMETERS ===
	jWriter.writeOpeningTag("unpatched", false, true);

	UnpatchedParamSet* unpatchedParams = paramManager->getUnpatchedParamSet();

	for (int32_t p = 0; p < UNPATCHED_SOUND_MAX_NUM; p++) {
		int32_t value = unpatchedParams->getValue(p);
		const char* paramName = paramNameForFile(Kind::UNPATCHED_SOUND, p + UNPATCHED_START);
		if (paramName) {
			jWriter.writeAttribute(paramName, value);
		}
	}

	jWriter.closeTag(true);

	// === FILTER MODES ===
	jWriter.writeOpeningTag("filters", false, true);
	jWriter.writeAttribute("lpfMode", getFilterModeString(sound->lpfMode));
	jWriter.writeAttribute("hpfMode", getFilterModeString(sound->hpfMode));
	jWriter.closeTag(true);

	// === LFO CONFIGURATION ===
	jWriter.writeOpeningTag("lfos", false, true);
	for (int32_t i = 0; i < LFO_COUNT; i++) {
		char lfoName[8];
		sprintf(lfoName, "lfo%d", i + 1);
		jWriter.writeOpeningTag(lfoName, false, true);
		jWriter.writeAttribute("syncLevel", sound->lfoConfig[i].syncLevel);
		jWriter.writeAttribute("syncType", sound->lfoConfig[i].syncType);
		jWriter.closeTag(true);
	}
	jWriter.closeTag(true);

	// === MOD FX ===
	jWriter.writeOpeningTag("modFX", false, true);
	jWriter.writeAttribute("type", getModFXTypeString(sound->getModFXType()));
	jWriter.closeTag(true);

	// === SIDECHAIN ===
	jWriter.writeAttribute("sidechainSend", sound->sideChainSendLevel);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Set a single parameter value
void setParameter(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^parameterSet", false, true);

	// TODO: Implement parameter setting logic
	jWriter.writeAttribute("error", "Not yet implemented");

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Get a single parameter value
void getParameter(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^parameter", false, true);

	// TODO: Implement single parameter get
	jWriter.writeAttribute("error", "Not yet implemented");

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Get all patch cables (modulation routing)
void getPatchCables(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^patchCables", false, true);

	Sound* sound = getCurrentSound();
	if (!sound) {
		jWriter.writeAttribute("error", "No synth sound selected");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	ModelStackWithTimelineCounter* modelStack = nullptr;
	getCurrentSound(&modelStack);

	if (!modelStack) {
		jWriter.writeAttribute("error", "Invalid model stack");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Check if this is an instrument clip
	InstrumentClip* clip = (InstrumentClip*)modelStack->getTimelineCounter();
	ParamManager* paramManager = &clip->paramManager;

	// TODO: Implement patch cable reading
	jWriter.writeAttribute("error", "Patch cables not yet implemented");
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Set a patch cable (modulation routing)
void setPatchCable(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^patchCableSet", false, true);

	// TODO: Implement patch cable setting
	jWriter.writeAttribute("error", "Not yet implemented");

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Subscribe to parameter changes
void subscribeParameters(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	parametersSubscribed = true;

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^parametersSubscribed", false, true);
	jWriter.writeAttribute("subscribed", 1);
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Unsubscribe from parameter changes
void unsubscribeParameters(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	parametersSubscribed = false;

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^parametersUnsubscribed", false, true);
	jWriter.writeAttribute("subscribed", false);
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Notify subscribers of parameter change
void notifyParameterChanged(int32_t paramKind, int32_t paramId, int32_t value) {
	if (!parametersSubscribed) {
		return;
	}

	paramNotifyWriter.reset();
	paramNotifyWriter.setMemoryBased();

	paramNotifyWriter.writeOpeningTag("^parameterChanged", false, true);
	paramNotifyWriter.writeAttribute("kind", paramKind);
	paramNotifyWriter.writeAttribute("id", paramId);
	paramNotifyWriter.writeAttribute("value", value);
	paramNotifyWriter.closeTag(true);

	// Send via MIDI SysEx
	// TODO: Need to get cable reference - for now disabled
	// midiEngine.sendSysex(paramNotifyWriter.buffer, paramNotifyWriter.size);
}

} // namespace ParameterSysex
