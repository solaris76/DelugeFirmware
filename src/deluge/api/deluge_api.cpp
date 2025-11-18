/*
 * Copyright © 2024 Synthstrom Audible Limited
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

#include "api/deluge_api.h"
#include "gui/ui/ui.h"
#include "gui/views/session_view.h"
#include "model/clip/audio_clip.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/kit.h"
#include "model/model_stack.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/sound/sound.h"
#include "processing/sound/sound_drum.h"
#include "processing/sound/sound_instrument.h"
#include <cstring>

extern Song* currentSong;
extern SessionView sessionView;

using namespace deluge::modulation::params;

namespace DelugeAPI {

// ===== Song & Clip Management =====

Song* Controller::getCurrentSong() {
	return currentSong;
}

Clip* Controller::getCurrentClip() {
	if (!currentSong) {
		return nullptr;
	}
	return currentSong->getCurrentClip();
}

Clip* Controller::getClipByIndex(int32_t index) {
	if (!currentSong) {
		return nullptr;
	}
	if (index < 0 || index >= currentSong->sessionClips.getNumElements()) {
		return nullptr;
	}
	return currentSong->sessionClips.getClipAtIndex(index);
}

Clip* Controller::createClip(OutputType type, int32_t insertIndex) {
	if (!currentSong) {
		return nullptr;
	}
	return sessionView.createClipAtIndex(type, insertIndex);
}

Clip* Controller::duplicateClip(int32_t sourceIndex, int32_t targetIndex) {
	if (!currentSong) {
		return nullptr;
	}
	return sessionView.duplicateClipToIndex(sourceIndex, targetIndex);
}

Result Controller::deleteClip(int32_t index) {
	if (!currentSong) {
		return Result::fail(Error::NONE, "No song loaded");
	}
	if (!sessionView.deleteClipAtIndex(index)) {
		return Result::fail(Error::NONE, "Invalid clip index");
	}
	return Result::ok();
}

Result Controller::setClipColour(int32_t index, int32_t colourOffset) {
	if (!currentSong) {
		return Result::fail(Error::NONE, "No song loaded");
	}
	if (!sessionView.setClipColour(index, colourOffset)) {
		return Result::fail(Error::NONE, "Invalid clip index");
	}
	return Result::ok();
}

Result Controller::enterClip(int32_t index) {
	if (!currentSong) {
		return Result::fail(Error::NONE, "No song loaded");
	}
	if (!sessionView.enterClipAtIndex(index)) {
		return Result::fail(Error::NONE, "Invalid clip index");
	}
	return Result::ok();
}

// ===== Kit & Drum Management =====

Kit* Controller::getCurrentKit() {
	if (!currentSong) {
		return nullptr;
	}
	Clip* clip = currentSong->getCurrentClip();
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		return nullptr;
	}
	Output* output = clip->output;
	if (!output || output->type != OutputType::KIT) {
		return nullptr;
	}
	return (Kit*)output;
}

Kit* Controller::getKitFromClip(Clip* clip) {
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		return nullptr;
	}
	Output* output = clip->output;
	if (!output || output->type != OutputType::KIT) {
		return nullptr;
	}
	return (Kit*)output;
}

Drum* Controller::getDrumFromIndex(Kit* kit, int32_t index) {
	if (!kit) {
		return nullptr;
	}
	return kit->getDrumFromIndex(index);
}

int32_t Controller::getDrumIndex(Kit* kit, Drum* drum) {
	if (!kit || !drum) {
		return -1;
	}
	return kit->getDrumIndex(drum);
}

Drum* Controller::addDrum(Kit* kit, DrumType type) {
	// This would need to call the actual kit->addDrum implementation
	// For now, return nullptr - this needs integration with KitSysex::addDrum logic
	// TODO: Extract drum creation logic to be reusable
	return nullptr;
}

Result Controller::removeDrum(Kit* kit, int32_t index) {
	if (!kit) {
		return Result::fail(Error::NONE, "No kit");
	}
	Drum* drum = kit->getDrumFromIndex(index);
	if (!drum) {
		return Result::fail(Error::NONE, "Invalid drum index");
	}
	// This would need to call the actual removal logic
	// TODO: Extract drum removal logic to be reusable
	return Result::fail(Error::NONE, "Not yet implemented");
}

Result Controller::setDrumSample(Kit* kit, int32_t index, const char* filePath) {
	if (!kit) {
		return Result::fail(Error::NONE, "No kit");
	}
	// This would need to call the actual sample loading logic
	// TODO: Extract sample loading logic to be reusable
	return Result::fail(Error::NONE, "Not yet implemented");
}

// ===== Parameter Management =====

ModelStackWithTimelineCounter* Controller::setupModelStackForClip(Clip* clip) {
	if (!currentSong || !clip) {
		return nullptr;
	}
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
	return modelStack->addTimelineCounter(clip);
}

ModelStackWithNoteRow* Controller::setupModelStackForNoteRow(InstrumentClip* instrumentClip, NoteRow* noteRow,
                                                             int32_t noteRowIndex) {
	if (!currentSong || !instrumentClip || !noteRow) {
		return nullptr;
	}
	ModelStackWithTimelineCounter* modelStack = setupModelStackForClip((Clip*)instrumentClip);
	if (!modelStack) {
		return nullptr;
	}
	return modelStack->addNoteRow(instrumentClip->getNoteRowId(noteRow, noteRowIndex), noteRow);
}

ModelStackWithAutoParam* Controller::getParamStack(Clip* clip, const char* paramName) {
	if (!clip || !paramName || clip->type != ClipType::INSTRUMENT) {
		return nullptr;
	}

	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	ParamManagerForTimeline* paramManager = &instrumentClip->paramManager;

	// Get the parameter from the instrument
	Output* output = clip->output;
	if (!output || output->type != OutputType::SYNTH) {
		return nullptr;
	}

	SoundInstrument* soundInstrument = (SoundInstrument*)output;
	Sound* sound = (Sound*)soundInstrument;

	ModelStackWithTimelineCounter* modelStack = setupModelStackForClip(clip);
	if (!modelStack) {
		return nullptr;
	}

	// Sound inherits from ModControllableAudio, so we can cast it directly
	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStack->addOtherTwoThingsButNoNoteRow((ModControllable*)sound, paramManager);

	// Find parameter by name using fileStringToParam
	ParamType paramId = fileStringToParam(Kind::UNPATCHED_SOUND, paramName, true);
	if (paramId == GLOBAL_NONE) {
		return nullptr;
	}

	// Determine which collection the parameter belongs to
	Kind paramKind = Kind::NONE;
	int32_t paramIdWithOffset = paramId;
	ParamCollection* paramCollection = nullptr;
	ParamCollectionSummary* summary = nullptr;

	if (paramId >= UNPATCHED_START) {
		// Unpatched parameter
		paramKind = Kind::UNPATCHED_SOUND;
		paramIdWithOffset = paramId - UNPATCHED_START;
		if (paramManager->summaries[0].paramCollection) {
			paramCollection = paramManager->summaries[0].paramCollection;
			summary = &paramManager->summaries[0];
		}
	}
	else {
		// Patched parameter
		paramKind = Kind::PATCHED;
		paramIdWithOffset = paramId;
		if (paramManager->summaries[1].paramCollection) {
			paramCollection = paramManager->summaries[1].paramCollection;
			summary = &paramManager->summaries[1];
		}
	}

	if (!paramCollection || !summary) {
		return nullptr;
	}

	// Get the AutoParam
	AutoParam* autoParam = nullptr;
	if (paramKind == Kind::UNPATCHED_SOUND) {
		UnpatchedParamSet* unpatchedParams = (UnpatchedParamSet*)paramCollection;
		if (paramIdWithOffset >= 0 && paramIdWithOffset < UNPATCHED_SOUND_MAX_NUM) {
			autoParam = &unpatchedParams->params[paramIdWithOffset];
		}
	}
	else if (paramKind == Kind::PATCHED) {
		PatchedParamSet* patchedParams = (PatchedParamSet*)paramCollection;
		if (paramIdWithOffset >= 0 && paramIdWithOffset < kNumParams) {
			autoParam = &patchedParams->params[paramIdWithOffset];
		}
	}

	if (!autoParam) {
		return nullptr;
	}

	return modelStackWithThreeMainThings->addParam(paramCollection, summary, paramIdWithOffset, autoParam);
}

ModelStackWithAutoParam* Controller::getDrumParamStack(Kit* kit, int32_t drumIndex, const char* paramName) {
	if (!kit || !paramName) {
		return nullptr;
	}

	Drum* drum = kit->getDrumFromIndex(drumIndex);
	if (!drum || drum->type != DrumType::SOUND) {
		return nullptr;
	}

	SoundDrum* soundDrum = (SoundDrum*)drum;

	Clip* clip = currentSong->getCurrentClip();
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		return nullptr;
	}

	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	int32_t noteRowIndex;
	NoteRow* noteRow = instrumentClip->getNoteRowForDrum(drum, &noteRowIndex);
	if (!noteRow) {
		return nullptr;
	}

	ParamManagerForTimeline* paramManager = &noteRow->paramManager;
	ModelStackWithNoteRow* modelStackWithNoteRow = setupModelStackForNoteRow(instrumentClip, noteRow, noteRowIndex);
	if (!modelStackWithNoteRow) {
		return nullptr;
	}

	// SoundDrum inherits from Sound which inherits from ModControllableAudio
	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStackWithNoteRow->addOtherTwoThings((ModControllable*)soundDrum, paramManager);

	// Find parameter by name using fileStringToParam
	ParamType paramId = fileStringToParam(Kind::UNPATCHED_SOUND, paramName, true);
	if (paramId == GLOBAL_NONE) {
		return nullptr;
	}

	// Determine which collection the parameter belongs to
	Kind paramKind = Kind::NONE;
	int32_t paramIdWithOffset = paramId;
	ParamCollection* paramCollection = nullptr;
	ParamCollectionSummary* summary = nullptr;

	if (paramId >= UNPATCHED_START) {
		// Unpatched parameter
		paramKind = Kind::UNPATCHED_SOUND;
		paramIdWithOffset = paramId - UNPATCHED_START;
		if (paramManager->summaries[0].paramCollection) {
			paramCollection = paramManager->summaries[0].paramCollection;
			summary = &paramManager->summaries[0];
		}
	}
	else {
		// Patched parameter
		paramKind = Kind::PATCHED;
		paramIdWithOffset = paramId;
		if (paramManager->summaries[1].paramCollection) {
			paramCollection = paramManager->summaries[1].paramCollection;
			summary = &paramManager->summaries[1];
		}
	}

	if (!paramCollection || !summary) {
		return nullptr;
	}

	// Get the AutoParam
	AutoParam* autoParam = nullptr;
	if (paramKind == Kind::UNPATCHED_SOUND) {
		UnpatchedParamSet* unpatchedParams = (UnpatchedParamSet*)paramCollection;
		if (paramIdWithOffset >= 0 && paramIdWithOffset < UNPATCHED_SOUND_MAX_NUM) {
			autoParam = &unpatchedParams->params[paramIdWithOffset];
		}
	}
	else if (paramKind == Kind::PATCHED) {
		PatchedParamSet* patchedParams = (PatchedParamSet*)paramCollection;
		if (paramIdWithOffset >= 0 && paramIdWithOffset < kNumParams) {
			autoParam = &patchedParams->params[paramIdWithOffset];
		}
	}

	if (!autoParam) {
		return nullptr;
	}

	return modelStackWithThreeMainThings->addParam(paramCollection, summary, paramIdWithOffset, autoParam);
}

Result Controller::setParameter(Clip* clip, const char* paramName, int32_t value) {
	ModelStackWithAutoParam* modelStack = getParamStack(clip, paramName);
	if (!modelStack || !modelStack->autoParam) {
		return Result::fail(Error::NONE, "Parameter not found");
	}

	// Set parameter value
	modelStack->autoParam->setCurrentValueInResponseToUserInput(value, modelStack);

	// Trigger UI refresh
	uiNeedsRendering(getCurrentUI());

	return Result::ok();
}

Result Controller::setDrumParameter(Kit* kit, int32_t drumIndex, const char* paramName, int32_t value) {
	ModelStackWithAutoParam* modelStack = getDrumParamStack(kit, drumIndex, paramName);
	if (!modelStack || !modelStack->autoParam) {
		return Result::fail(Error::NONE, "Parameter not found");
	}

	// Set parameter value
	modelStack->autoParam->setCurrentValueInResponseToUserInput(value, modelStack);

	// Trigger UI refresh
	uiNeedsRendering(getCurrentUI());

	return Result::ok();
}

Result Controller::getParameter(Clip* clip, const char* paramName, int32_t& valueOut) {
	ModelStackWithAutoParam* modelStack = getParamStack(clip, paramName);
	if (!modelStack || !modelStack->autoParam) {
		return Result::fail(Error::NONE, "Parameter not found");
	}

	valueOut = modelStack->autoParam->getCurrentValue();
	return Result::ok();
}

Result Controller::getDrumParameter(Kit* kit, int32_t drumIndex, const char* paramName, int32_t& valueOut) {
	ModelStackWithAutoParam* modelStack = getDrumParamStack(kit, drumIndex, paramName);
	if (!modelStack || !modelStack->autoParam) {
		return Result::fail(Error::NONE, "Parameter not found");
	}

	valueOut = modelStack->autoParam->getCurrentValue();
	return Result::ok();
}

// ===== Instrument Access =====

Instrument* Controller::getCurrentInstrument() {
	Clip* clip = getCurrentClip();
	return getInstrumentFromClip(clip);
}

Instrument* Controller::getInstrumentFromClip(Clip* clip) {
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		return nullptr;
	}
	Output* output = clip->output;
	if (!output) {
		return nullptr;
	}
	return (Instrument*)output;
}

// ===== NoteRow Access =====

NoteRow* Controller::getNoteRowForDrum(Kit* kit, Drum* drum) {
	if (!kit || !drum || !currentSong) {
		return nullptr;
	}
	Clip* clip = currentSong->getCurrentClip();
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		return nullptr;
	}
	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	int32_t noteRowIndex;
	return instrumentClip->getNoteRowForDrum(drum, &noteRowIndex);
}

int32_t Controller::getNoteRowIndexForDrum(Kit* kit, Drum* drum) {
	if (!kit || !drum || !currentSong) {
		return -1;
	}
	Clip* clip = currentSong->getCurrentClip();
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		return -1;
	}
	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	int32_t noteRowIndex;
	NoteRow* noteRow = instrumentClip->getNoteRowForDrum(drum, &noteRowIndex);
	if (!noteRow) {
		return -1;
	}
	return noteRowIndex;
}

} // namespace DelugeAPI
