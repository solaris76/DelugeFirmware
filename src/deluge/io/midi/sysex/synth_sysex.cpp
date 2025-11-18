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
#include "io/midi/sysex/sound_sysex_helpers.h"
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

	SoundSysex::writeSoundParameterSnapshot(jWriter, sound, paramManager);

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

// Check if there are any parameter subscribers
bool hasParameterSubscribers() {
	return parameterSubscribers.size() > 0;
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

// Notify subscribers of a non-parameter property change (e.g., polyphonic, mode, transpose, etc.)
// This is for properties that are not AutoParams but are still reported in getParameters
void notifyNonParamPropertyChanged(const char* paramName, int32_t value) {
	if (parameterSubscribers.size() == 0 || !paramName) {
		return;
	}

	parameterSubscribers.forEach([&](MIDICable& destination) {
		paramNotifyWriter.reset();
		paramNotifyWriter.setMemoryBased();
		smSysex::startDirect(paramNotifyWriter);
		paramNotifyWriter.writeOpeningTag("^parameterChanged", false, true);
		paramNotifyWriter.writeAttribute("kind", (int32_t)0); // Use 0 for non-parameter properties
		paramNotifyWriter.writeAttribute("id", (int32_t)0);   // Use 0 for non-parameter properties
		paramNotifyWriter.writeAttribute("name", paramName);
		paramNotifyWriter.writeAttribute("value", value);
		paramNotifyWriter.closeTag(true);
		smSysex::sendMsg(destination, paramNotifyWriter);
	});
}

} // namespace SynthSysex
