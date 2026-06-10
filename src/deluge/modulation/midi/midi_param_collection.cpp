/*
 * Copyright © 2017-2023 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
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

#include "modulation/midi/midi_param_collection.h"
#include "definitions_cxx.hpp"
#include "gui/views/automation_view.h"
#include "io/midi/midi_engine.h"
#include "model/action/action_logger.h"
#include "model/clip/instrument_clip.h"
#include "model/drum/midi_drum.h"
#include "model/instrument/instrument.h"
#include "model/instrument/midi_instrument.h"
#include "model/model_stack.h"
#include "model/settings/runtime_feature_settings.h"
#include "model/song/song.h"
#include "modulation/automation/auto_param.h"
#include "modulation/midi/midi_param.h"
#include "playback/playback_handler.h"
#include "processing/engines/audio_engine.h"
#include "storage/storage_manager.h"
#include "util/functions.h"

namespace {

static constexpr uint32_t kGeneratorCcThrottleSamples = kSampleRate / 30;

MIDIParam* getMidiParamFromModelStack(ModelStackWithAutoParam const* modelStack) {
	if (modelStack->paramCollection == nullptr) {
		return nullptr;
	}

	auto* collection = static_cast<MIDIParamCollection*>(modelStack->paramCollection);
	int32_t paramIndex = collection->params.searchExact(modelStack->paramId);
	if (paramIndex < 0) {
		return nullptr;
	}

	return collection->params.getElement(paramIndex);
}

bool shouldSendGeneratorCc(MIDIParam* midiParam, AutoParam const* param, ModelStackWithAutoParam const* modelStack,
                           int32_t newValue, bool forceSend) {
	if (forceSend || param == nullptr || !param->isGeneratorLane()) {
		return true;
	}

	auto mode = static_cast<RuntimeFeatureStateGeneratorCcOutput>(
	    runtimeFeatureSettings.get(RuntimeFeatureSettingType::GeneratorCcOutput));
	if (mode == RuntimeFeatureStateGeneratorCcOutput::Realtime) {
		return true;
	}

	if (midiParam == nullptr) {
		return true;
	}

	if (mode == RuntimeFeatureStateGeneratorCcOutput::Throttled) {
		uint32_t now = AudioEngine::audioSampleTimer;
		if (now - midiParam->lastGeneratorCcSendSample >= kGeneratorCcThrottleSamples) {
			midiParam->lastGeneratorCcSendSample = now;
			midiParam->lastGeneratorCcValueSent = MIDIParamCollection::autoparamValueToCC(newValue);
			return true;
		}
		return false;
	}

	uint32_t clipPos = modelStack->timelineCounterIsSet() ? modelStack->getLastProcessedPos() : 0;
	uint32_t readPos =
	    param->shouldUseAutomationClockRate(modelStack) ? param->getLaneReadPos(clipPos, modelStack) : clipPos;
	uint32_t stepLen = modelStack->song != nullptr ? modelStack->song->getSixteenthNoteLength() : 1;
	if (stepLen == 0) {
		stepLen = 1;
	}
	uint32_t stepIndex = readPos / stepLen;
	if (stepIndex != midiParam->lastGeneratorCcStepIndex) {
		midiParam->lastGeneratorCcStepIndex = stepIndex;
		return true;
	}
	return false;
}

} // namespace

MIDIParamCollection::MIDIParamCollection(ParamCollectionSummary* summary)
    : ParamCollection(sizeof(MIDIParamCollection), summary) {

	// Just to indicate there could be some automation, cos we don't actually use this variable properly.
	// TODO: at least make this go to 0 when no MIDIParams present.
	summary->whichParamsAreAutomated[0] = 1;
	summary->whichParamsAreInterpolating[0] = 0;
}

MIDIParamCollection::~MIDIParamCollection() {
	deleteAllParams(NULL, false);
}

void MIDIParamCollection::deleteAllParams(Action* action, bool deleteStorageToo) {
	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);

		if (action) {
			midiParam->param.deleteAutomationBasicForSetup();
		}
		midiParam->~MIDIParam();
	}

	if (deleteStorageToo) {
		params.empty();
	}
}

void MIDIParamCollection::tickTicks(int32_t numTicks, ModelStackWithParamCollection* modelStack) {
	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);
		AutoParam* param = &midiParam->param;

		if (param->hasInterpolationIncrement()) {
			int32_t oldValue = param->getCurrentValue();
			bool shouldNotify = param->tickTicks(numTicks);
			if (shouldNotify) { // Should always actually be true...
				ModelStackWithAutoParam* modelStackWithAutoParam = modelStack->addAutoParam(midiParam->cc, param);
				notifyParamModifiedInSomeWay(modelStackWithAutoParam, oldValue, false, true, true);
			}
		}
	}
}

void MIDIParamCollection::beenCloned(bool copyAutomation, int32_t reverseDirectionWithLength) {
	params.beenCloned(); // Copies memory for all of params

	// And now, copy the memory for the automation data that each member of params references
	for (int32_t i = 0; i < params.getNumElements(); i++) {
		params.getElement(i)->param.beenCloned(copyAutomation, reverseDirectionWithLength);
	}
}

void MIDIParamCollection::setPlayPos(uint32_t pos, ModelStackWithParamCollection* modelStack, bool reversed) {

	// Bend param is the only one which is actually gonna maybe want to set up some interpolation -
	// but for the other ones we still need to initialize them and crucially make sure automation overriding is
	// switched off
	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);
		AutoParam* param = &midiParam->param;
		ModelStackWithAutoParam* modelStackWithAutoParam = modelStack->addAutoParam(midiParam->cc, param);

		param->setPlayPos(pos, modelStackWithAutoParam, reversed);
	}

	ParamCollection::setPlayPos(pos, modelStack, reversed);
}

void MIDIParamCollection::generateRepeats(ModelStackWithParamCollection* modelStack, uint32_t oldLength,
                                          uint32_t newLength, bool shouldPingpong) {
	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);
		midiParam->param.generateRepeats(oldLength, newLength, shouldPingpong);
	}
}

void MIDIParamCollection::appendParamCollection(ModelStackWithParamCollection* modelStack,
                                                ModelStackWithParamCollection* otherModelStack, int32_t oldLength,
                                                int32_t reverseThisRepeatWithLength, bool pingpongingGenerally) {

	MIDIParamCollection* otherMIDIParamCollection = (MIDIParamCollection*)otherModelStack->paramCollection;
	for (int32_t i = 0; i < otherMIDIParamCollection->params.getNumElements(); i++) {
		MIDIParam* otherMidiParam = otherMIDIParamCollection->params.getElement(i);

		// Find param in *this* collection for the same CC. There should be one
		int32_t j = params.searchExact(otherMidiParam->cc);

		if (j != -1) {
			MIDIParam* midiParam = params.getElement(j);
			midiParam->param.appendParam(&otherMidiParam->param, oldLength, reverseThisRepeatWithLength,
			                             pingpongingGenerally);
		}
	}

	ticksTilNextEvent = 0;
}

void MIDIParamCollection::trimToLength(uint32_t newLength, ModelStackWithParamCollection* modelStack, Action* action,
                                       bool maySetupPatching) {
	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);
		AutoParam* param = &midiParam->param;
		ModelStackWithAutoParam* modelStackWithAutoParam = modelStack->addAutoParam(midiParam->cc, param);

		param->trimToLength(newLength, action, modelStackWithAutoParam);
	}
	ticksTilNextEvent = 0;
}

void MIDIParamCollection::shiftHorizontally(ModelStackWithParamCollection* modelStack, int32_t amount,
                                            int32_t effectiveLength) {
	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);
		midiParam->param.shiftHorizontally(amount, effectiveLength);
	}
}

void MIDIParamCollection::processCurrentPos(ModelStackWithParamCollection* modelStack, int32_t ticksSkipped,
                                            bool reversed, bool didPingpong, bool mayInterpolate) {

	ticksTilNextEvent -= ticksSkipped;
	if (ticksTilNextEvent <= 0) {
		bool interpolating = false;

		ticksTilNextEvent = 2147483647;

		for (int32_t i = 0; i < params.getNumElements(); i++) {
			MIDIParam* midiParam = params.getElement(i);
			AutoParam* param = &midiParam->param;
			ModelStackWithAutoParam* modelStackWithAutoParam = modelStack->addAutoParam(midiParam->cc, param);

			int32_t ticksTilNextEventThisParam = param->processCurrentPos(modelStackWithAutoParam, reversed,
			                                                              didPingpong, false, true); // No interpolating
			ticksTilNextEvent = std::min(ticksTilNextEvent, ticksTilNextEventThisParam);
			if (param->hasInterpolationIncrement()) {
				interpolating = true;
			}
		}
		modelStack->summary->whichParamsAreInterpolating[0] = static_cast<uint32_t>(interpolating);
	}
}

void MIDIParamCollection::remotelySwapParamState(AutoParamState* state, ModelStackWithParamId* modelStack) {

	MIDIParam* midiParam = params.getOrCreateParamFromCC(modelStack->paramId);
	if (!midiParam) {
		return;
	}

	AutoParam* param = &midiParam->param;

	ModelStackWithAutoParam* modelStackWithParam = modelStack->addAutoParam(param);

	param->swapState(state, modelStackWithParam);
}

void MIDIParamCollection::deleteAllAutomation(Action* action, ModelStackWithParamCollection* modelStack) {

	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);

		if (midiParam->param.isAutomated()) {
			ModelStackWithAutoParam* modelStackWithParam = modelStack->addAutoParam(midiParam->cc, &midiParam->param);
			midiParam->param.deleteAutomation(action, modelStackWithParam, false);
		}
	}
}

ModelStackWithAutoParam* MIDIParamCollection::getAutoParamFromId(ModelStackWithParamId* modelStack,
                                                                 bool allowCreation) {
	int32_t defaultValue = 0;
	MIDIParam* midiParam = params.getOrCreateParamFromCC(modelStack->paramId, defaultValue, allowCreation);
	AutoParam* param = NULL;
	if (midiParam) {
		param = &midiParam->param;
	}
	return modelStack->addAutoParam(param);
}

int32_t MIDIParamCollection::autoparamValueToCC(int32_t newValue) {
	int32_t rShift = 25;
	int32_t roundingAmountToAdd = 1 << (rShift - 1);
	int32_t maxValue = 2147483647 - roundingAmountToAdd;

	if (newValue > maxValue) {
		newValue = maxValue;
	}
	return (newValue + roundingAmountToAdd) >> rShift;
}
void MIDIParamCollection::sendMIDI(MIDISource source, int32_t masterChannel, int32_t cc, int32_t newValue,
                                   int32_t midiOutputFilter, uint8_t deviceFilter) {
	int32_t newValueSmall = autoparamValueToCC(newValue);

	midiEngine.sendCC(source, masterChannel, cc, newValueSmall + 64, midiOutputFilter, deviceFilter);
}

// For MIDI CCs, which prior to V2.0 did interpolation
// Returns error code
Error MIDIParamCollection::makeInterpolatedCCsGoodAgain(int32_t clipLength) {

	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);

		if (midiParam->cc >= 120) {
			return Error::NONE;
		}
		Error error = midiParam->param.makeInterpolationGoodAgain(clipLength, 25);
		if (error != Error::NONE) {
			return error;
		}
	}

	return Error::NONE;
}

void MIDIParamCollection::grabValuesFromPos(uint32_t pos, ModelStackWithParamCollection* modelStack) {
	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);

		AutoParam* param = &midiParam->param;

		// With MIDI, we only want to send these out if the param is actually automated and the value is
		// actually different
		if (param->isAutomated()) {

			int32_t oldValue = param->getCurrentValue();
			ModelStackWithAutoParam* modelStackWithAutoParam = modelStack->addAutoParam(midiParam->cc, param);
			bool shouldSend = param->grabValueFromPos(pos, modelStackWithAutoParam);

			if (shouldSend) {
				// Generator lanes at rest would spam CC on every UI/automation audition; send during playback only.
				bool sendExternal = playbackHandler.isEitherClockActive() || !param->isGeneratorLane();
				if (sendExternal) {
					notifyParamModifiedInSomeWay(modelStackWithAutoParam, oldValue, false, true, true);
				}
			}
		}
	}
}

void MIDIParamCollection::nudgeNonInterpolatingNodesAtPos(int32_t pos, int32_t offset, int32_t lengthBeforeLoop,
                                                          Action* action, ModelStackWithParamCollection* modelStack) {

	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);
		AutoParam* param = &midiParam->param;
		ModelStackWithAutoParam* modelStackWithAutoParam = modelStack->addAutoParam(midiParam->cc, param);

		param->nudgeNonInterpolatingNodesAtPos(pos, offset, lengthBeforeLoop, action, modelStackWithAutoParam);
	}
}

void MIDIParamCollection::notifyParamModifiedInSomeWay(ModelStackWithAutoParam const* modelStack, int32_t oldValue,
                                                       bool automationChanged, bool automatedBefore,
                                                       bool automatedNow) {

	ParamCollection::notifyParamModifiedInSomeWay(modelStack, oldValue, automationChanged, automatedBefore,
	                                              automatedNow);

	auto new_v = modelStack->autoParam->getCurrentValue();
	bool current_value_changed =
	    modelStack->modControllable->valueChangedEnoughToMatter(oldValue, new_v, getParamKind(), modelStack->paramId);

	if (!current_value_changed && !(automationChanged && automatedNow)) {
		return;
	}

	bool forceSend = automationChanged && automatedNow;
	MIDIParam* midiParam = getMidiParamFromModelStack(modelStack);
	if (!shouldSendGeneratorCc(midiParam, modelStack->autoParam, modelStack, new_v, forceSend)) {
		return;
	}

	// Detect if this is a kit row (MIDI drum) vs a MIDI instrument track
	// Kit rows have a noteRow in the modelStack, instrument tracks don't
	bool isKitRow = (modelStack->getNoteRowAllowNull() != nullptr);

	if (isKitRow) {
		// MIDI drum (kit row) - use drum's channel and device
		MIDIDrum* midiDrum = (MIDIDrum*)modelStack->modControllable;
		int32_t masterChannel = midiDrum->channel;
		int32_t midiOutputFilter = midiDrum->channel;
		uint8_t deviceFilter = midiDrum->outputDevice;

		// Send CC with device filter
		int32_t newValueSmall = autoparamValueToCC(new_v);
		midiEngine.sendCC(midiDrum, masterChannel, modelStack->paramId, newValueSmall + 64, midiOutputFilter,
		                  deviceFilter);
	}
	else {
		// MIDI instrument track
		if (modelStack->song->isOutputActiveInArrangement((MIDIInstrument*)modelStack->modControllable)) {
			MIDIInstrument* instrument = (MIDIInstrument*)modelStack->modControllable;
			int32_t midiOutputFilter = instrument->getChannel();
			int32_t masterChannel = instrument->getOutputMasterChannel();
			sendMIDI(instrument, masterChannel, modelStack->paramId, new_v, midiOutputFilter, instrument->outputDevice);
		}
	}
}

bool MIDIParamCollection::mayParamInterpolate(int32_t paramId) {
	return false;
}

int32_t MIDIParamCollection::knobPosToParamValue(int32_t knobPos, ModelStackWithAutoParam* modelStack) {
	return ParamCollection::knobPosToParamValue(knobPos, modelStack);
}

void MIDIParamCollection::notifyPingpongOccurred(ModelStackWithParamCollection* modelStack) {
	ParamCollection::notifyPingpongOccurred(modelStack);

	for (int32_t i = 0; i < params.getNumElements(); i++) {
		MIDIParam* midiParam = params.getElement(i);
		midiParam->param.notifyPingpongOccurred();
	}
}

void MIDIParamCollection::writeToFile(Serializer& writer) {
	if (params.getNumElements()) {

		writer.writeOpeningTag("midiParams");

		for (int32_t i = 0; i < params.getNumElements(); i++) {
			MIDIParam* midiParam = params.getElement(i);
			int32_t cc = midiParam->cc;

			writer.writeOpeningTag("param");
			if (cc == CC_NUMBER_NONE) { // Why would I have put this in here?
				writer.writeTag("cc", "none");
			}
			else {
				writer.writeTag("cc", cc);
			}

			writer.writeOpeningTag("value", false);
			midiParam->param.writeToFile(writer, true);
			writer.writeClosingTag("value", false);

			if (midiParam->param.getAutomationClockDivider() != 1) {
				writer.writeTag("clockDivider", midiParam->param.getAutomationClockDivider());
			}

			if (midiParam->param.getAutomationLaneType() != AutomationLaneType::MANUAL) {
				writer.writeTag("laneType", automationLaneTypeToString(midiParam->param.getAutomationLaneType()));
			}

			if (midiParam->param.getShapeAmount() != 64) {
				writer.writeTag("shapeAmount", midiParam->param.getShapeAmount());
			}

			if (midiParam->param.getShapeOffset() != 0) {
				writer.writeTag("shapeOffset", midiParam->param.getShapeOffset());
			}

			writer.writeClosingTag("param");
		}

		writer.writeClosingTag("midiParams");
	}
}

/*
    for (int32_t i = 0; i < params.getNumElements(); i++) {
        MIDIParam* midiParam = params.getElement(i);

    }
*/
