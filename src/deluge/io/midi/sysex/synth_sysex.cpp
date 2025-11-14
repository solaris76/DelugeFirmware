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

#include "io/midi/sysex/synth_sysex.h"
#include "gui/ui/ui.h"
#include "io/midi/sysex/sysex_common.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/instrument.h"
#include "model/model_stack.h"
#include "model/sample/sample_holder.h"
#include "model/song/song.h"
#include "modulation/params/param.h"
#include "modulation/params/param_collection.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/patch/patch_cable_set.h"
#include "processing/sound/sound.h"
#include "processing/sound/sound_instrument.h"
#include "storage/audio/audio_file_holder.h"
#include "storage/multi_range/multi_range.h"
#include "storage/smsysex.h"
#include <algorithm>
#include <cstring>

extern JsonSerializer jWriter;
extern Song* currentSong;

using namespace deluge::modulation::params;

namespace SynthSysex {

// Track subscription state
static SysexCommon::SubscriberList parameterSubscribers;

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

	if (!currentSong) {
		jWriter.writeAttribute("error", "No song");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Clip* clip = currentSong->getCurrentClip();
	if (!clip) {
		jWriter.writeAttribute("error", "No clip selected");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Output* output = clip->output;
	if (!output || output->type != OutputType::SYNTH) {
		jWriter.writeAttribute("error", "Not a synth clip");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	SoundInstrument* soundInstrument = (SoundInstrument*)output;
	Sound* sound = (Sound*)soundInstrument;

	// Setup proper model stack using Song's method
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(modelStackMemory);
	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	ParamManagerForTimeline* paramManager = &instrumentClip->paramManager;

	// === PATCH NAME ===
	if (!output->name.isEmpty()) {
		jWriter.writeAttribute("presetName", output->name.get());
	}

	// === GENERAL SOUND ATTRIBUTES (flat structure for simplicity) ===
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

	// Unison
	jWriter.writeAttribute("unisonNum", sound->numUnison);
	jWriter.writeAttribute("unisonDetune", sound->unisonDetune);
	jWriter.writeAttribute("unisonSpread", sound->unisonStereoSpread);

	// Oscillators
	for (int32_t s = 0; s < kNumSources; s++) {
		Source* source = &sound->sources[s];
		char prefix[10];
		sprintf(prefix, "osc%d", s + 1);

		char attrName[32];
		sprintf(attrName, "%sType", prefix);
		jWriter.writeAttribute(attrName, getOscTypeString(source->oscType));

		sprintf(attrName, "%sTranspose", prefix);
		jWriter.writeAttribute(attrName, sound->modulatorTranspose[s]);

		sprintf(attrName, "%sCents", prefix);
		jWriter.writeAttribute(attrName, sound->modulatorCents[s]);

		sprintf(attrName, "%sRetrigPhase", prefix);
		if (sound->oscRetriggerPhase[s] == 4294967295) {
			jWriter.writeAttribute(attrName, -1);
		}
		else {
			jWriter.writeAttribute(attrName, (int32_t)sound->oscRetriggerPhase[s]);
		}

		// Get file path for sample/wavetable oscillators
		if (source->oscType == OscType::SAMPLE || source->oscType == OscType::WAVETABLE) {
			if (source->ranges.getNumElements() > 0) {
				MultiRange* range = source->ranges.getElement(0);
				if (range) {
					AudioFileHolder* holder = range->getAudioFileHolder();
					if (holder && holder->filePath.isEmpty() == false) {
						sprintf(attrName, "%sFile", prefix);
						jWriter.writeAttribute(attrName, holder->filePath.get());
					}
				}
			}
		}
	}

	// Osc sync
	jWriter.writeAttribute("osc2Sync", sound->oscillatorSync ? 1 : 0);

	// === UNPATCHED PARAMETERS ===
	// Try to safely access unpatched params
	if (paramManager->summaries[0].paramCollection) {
		UnpatchedParamSet* unpatchedParams = (UnpatchedParamSet*)paramManager->summaries[0].paramCollection;

		for (int32_t p = 0; p < UNPATCHED_SOUND_MAX_NUM; p++) {
			int32_t value = unpatchedParams->getValue(p);
			const char* paramName = paramNameForFile(Kind::UNPATCHED_SOUND, p + UNPATCHED_START);
			if (paramName) {
				jWriter.writeAttribute(paramName, value);
			}
		}
	}

	// === PATCHED PARAMETERS ===
	// Try to safely access patched params
	if (paramManager->summaries[1].paramCollection) {
		PatchedParamSet* patchedParams = (PatchedParamSet*)paramManager->summaries[1].paramCollection;

		for (int32_t p = 0; p < kNumParams; p++) {
			AutoParam* param = &patchedParams->params[p];
			int32_t value = param->getCurrentValue();
			const char* paramName = paramNameForFile(Kind::PATCHED, p);
			if (paramName) {
				jWriter.writeAttribute(paramName, value);
			}
		}
	}

	// === FILTER MODES & ROUTING ===
	jWriter.writeAttribute("lpfMode", getFilterModeString(sound->lpfMode));
	jWriter.writeAttribute("hpfMode", getFilterModeString(sound->hpfMode));

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
	jWriter.writeAttribute("filterRoute", routeName);

	// === DELAY CONFIGURATION ===
	jWriter.writeAttribute("delayPingPong", sound->delay.pingPong ? 1 : 0);
	jWriter.writeAttribute("delayAnalog", sound->delay.analog ? 1 : 0);
	jWriter.writeAttribute("delaySyncLevel", (int32_t)sound->delay.syncLevel);
	jWriter.writeAttribute("delaySyncType", (int32_t)sound->delay.syncType);

	// === SIDECHAIN (static params) ===
	jWriter.writeAttribute("sidechainAttack", sound->sidechain.attack);
	jWriter.writeAttribute("sidechainRelease", sound->sidechain.release);
	jWriter.writeAttribute("sidechainSend", sound->sideChainSendLevel);

	// === CLIPPING ===
	jWriter.writeAttribute("clippingAmount", sound->clippingAmount);

	// === LFO CONFIGURATION (flat structure) ===
	for (int32_t i = 0; i < LFO_COUNT; i++) {
		char attrName[32];
		sprintf(attrName, "lfo%dSyncLevel", i + 1);
		jWriter.writeAttribute(attrName, sound->lfoConfig[i].syncLevel);

		sprintf(attrName, "lfo%dSyncType", i + 1);
		jWriter.writeAttribute(attrName, sound->lfoConfig[i].syncType);
	}

	// === MOD FX ===
	jWriter.writeAttribute("modFXType", getModFXTypeString(sound->getModFXType()));

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Set a single parameter value
void setParameter(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^parameterSet", false, true);

	if (!currentSong) {
		jWriter.writeAttribute("error", "No song");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Clip* clip = currentSong->getCurrentClip();
	if (!clip) {
		jWriter.writeAttribute("error", "No clip selected");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Output* output = clip->output;
	if (!output || output->type != OutputType::SYNTH) {
		jWriter.writeAttribute("error", "Not a synth clip");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	SoundInstrument* soundInstrument = (SoundInstrument*)output;
	Sound* sound = (Sound*)soundInstrument;

	// Parse parameters from JSON
	String paramName;
	int32_t intValue = 0;
	String strValue;
	bool hasIntValue = false;
	bool hasStrValue = false;

	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "name")) {
			reader.readTagOrAttributeValueString(&paramName);
		}
		else if (!strcmp(tagName, "value")) {
			// Try reading as int first
			intValue = reader.readTagOrAttributeValueInt();
			hasIntValue = true;
		}
		else if (!strcmp(tagName, "strValue")) {
			reader.readTagOrAttributeValueString(&strValue);
			hasStrValue = true;
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (paramName.isEmpty()) {
		jWriter.writeAttribute("error", "Missing parameter name");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	const char* name = paramName.get();
	bool success = false;
	const char* errorMsg = "Unknown parameter";

	// === HANDLE DIFFERENT PARAMETER TYPES ===

	// General attributes
	if (!strcmp(name, "transpose")) {
		sound->transpose = intValue;
		success = true;
	}
	else if (!strcmp(name, "maxVoices")) {
		sound->maxVoiceCount = std::clamp<int32_t>(intValue, 1, 8);
		success = true;
	}
	else if (!strcmp(name, "unisonNum")) {
		sound->numUnison = std::clamp<int32_t>(intValue, 1, 8);
		success = true;
	}
	else if (!strcmp(name, "unisonDetune")) {
		sound->unisonDetune = std::clamp<int32_t>(intValue, 0, 50);
		success = true;
	}
	else if (!strcmp(name, "unisonSpread")) {
		sound->unisonStereoSpread = std::clamp<int32_t>(intValue, 0, 127);
		success = true;
	}
	else if (!strcmp(name, "osc2Sync")) {
		sound->oscillatorSync = (intValue != 0);
		success = true;
	}
	else if (!strcmp(name, "clippingAmount")) {
		sound->clippingAmount = std::clamp<int32_t>(intValue, 0, 255);
		success = true;
	}
	else if (!strcmp(name, "delayPingPong")) {
		sound->delay.pingPong = (intValue != 0);
		success = true;
	}
	else if (!strcmp(name, "delayAnalog")) {
		sound->delay.analog = (intValue != 0);
		success = true;
	}
	else if (!strcmp(name, "delaySyncLevel")) {
		sound->delay.syncLevel = (SyncLevel)std::clamp<int32_t>(intValue, 0, 9);
		success = true;
	}
	else if (!strcmp(name, "delaySyncType")) {
		sound->delay.syncType = (SyncType)std::clamp<int32_t>(intValue, 0, 2);
		success = true;
	}
	else if (!strcmp(name, "sidechainSend")) {
		sound->sideChainSendLevel = intValue;
		success = true;
	}
	else {
		// Try patched/unpatched params via ParamManager
		InstrumentClip* instrumentClip = (InstrumentClip*)clip;
		ParamManagerForTimeline* paramManager = &instrumentClip->paramManager;

		// Setup model stack for proper parameter updates
		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(modelStackMemory);
		ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
		    modelStack->addOtherTwoThingsButNoNoteRow(sound, paramManager);

		// Try unpatched params first
		if (paramManager->summaries[0].paramCollection) {
			UnpatchedParamSet* unpatchedParams = (UnpatchedParamSet*)paramManager->summaries[0].paramCollection;
			ParamCollection* unpatchedParamCollection = paramManager->summaries[0].paramCollection;

			for (int32_t p = 0; p < UNPATCHED_SOUND_MAX_NUM; p++) {
				const char* checkName = paramNameForFile(Kind::UNPATCHED_SOUND, p + UNPATCHED_START);
				if (checkName && !strcmp(name, checkName)) {
					AutoParam* param = &unpatchedParams->params[p];
					ModelStackWithAutoParam* modelStackWithParam = modelStackWithThreeMainThings->addParam(
					    unpatchedParamCollection, &paramManager->summaries[0], p, param);
					// Use setCurrentValueInResponseToUserInput to trigger recalculations
					param->setCurrentValueInResponseToUserInput(intValue, modelStackWithParam);
					success = true;
					break;
				}
			}
		}

		// Try patched params if not found
		if (!success && paramManager->summaries[1].paramCollection) {
			PatchedParamSet* patchedParams = (PatchedParamSet*)paramManager->summaries[1].paramCollection;
			ParamCollection* patchedParamCollection = paramManager->summaries[1].paramCollection;

			for (int32_t p = 0; p < kNumParams; p++) {
				const char* checkName = paramNameForFile(Kind::PATCHED, p);
				if (checkName && !strcmp(name, checkName)) {
					AutoParam* param = &patchedParams->params[p];
					ModelStackWithAutoParam* modelStackWithParam = modelStackWithThreeMainThings->addParam(
					    patchedParamCollection, &paramManager->summaries[1], p, param);
					// Use setCurrentValueInResponseToUserInput to trigger recalculations
					param->setCurrentValueInResponseToUserInput(intValue, modelStackWithParam);
					success = true;
					break;
				}
			}
		}
	}

	if (success) {
		jWriter.writeAttribute("name", name);
		jWriter.writeAttribute("value", intValue);
		jWriter.writeAttribute("success", 1);

		// Trigger UI refresh to show the new value
		uiNeedsRendering(getCurrentUI());
	}
	else {
		jWriter.writeAttribute("error", errorMsg);
		jWriter.writeAttribute("name", name);
	}

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
	SysexCommon::startResponse(jWriter, reader, "^parametersSubscribed");

	switch (parameterSubscribers.add(&cable)) {
	case SysexCommon::SubscriberList::AddResult::ALREADY_PRESENT:
		SysexCommon::writeStatus(jWriter, "already");
		jWriter.writeAttribute("subscribed", (int32_t)1);
		break;
	case SysexCommon::SubscriberList::AddResult::ADDED:
		SysexCommon::writeStatus(jWriter, "success");
		jWriter.writeAttribute("subscribed", (int32_t)1);
		jWriter.writeAttribute("count", (int32_t)parameterSubscribers.size());
		break;
	case SysexCommon::SubscriberList::AddResult::FULL:
		SysexCommon::writeStatus(jWriter, "error", "Max subscribers reached");
		jWriter.writeAttribute("subscribed", (int32_t)0);
		break;
	}

	SysexCommon::sendResponse(cable, jWriter);
}

// Unsubscribe from parameter changes
void unsubscribeParameters(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^parametersUnsubscribed");

	if (parameterSubscribers.remove(&cable)) {
		SysexCommon::writeStatus(jWriter, "success");
	}
	else {
		SysexCommon::writeStatus(jWriter, "not_subscribed");
	}

	jWriter.writeAttribute("subscribed", (int32_t)0);
	SysexCommon::sendResponse(cable, jWriter);
}

// Notify subscribers of parameter change
void notifyParameterChanged(int32_t paramKind, int32_t paramId, const char* paramName, int32_t value) {
	if (parameterSubscribers.size() == 0 || !paramName) {
		return;
	}

	parameterSubscribers.forEach([&](MIDICable& destination) {
		paramNotifyWriter.reset();
		paramNotifyWriter.setMemoryBased();
		smSysex::startDirect(paramNotifyWriter);
		paramNotifyWriter.writeOpeningTag("^parameterChanged", false, true);
		paramNotifyWriter.writeAttribute("kind", paramKind);
		paramNotifyWriter.writeAttribute("id", paramId);
		paramNotifyWriter.writeAttribute("name", paramName);
		paramNotifyWriter.writeAttribute("value", value);
		paramNotifyWriter.closeTag(true);
		smSysex::sendMsg(destination, paramNotifyWriter);
	});
}

} // namespace SynthSysex
