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

#include "io/midi/sysex/param_stream.h"
#include "io/midi/sysex/kit_sysex.h"
#include "io/midi/sysex/synth_sysex.h"
#include "model/drum/drum.h"
#include "model/instrument/kit.h"
#include "model/model_stack.h"
#include "model/note/note_row.h"
#include "modulation/automation/auto_param.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include <cstring>

using namespace deluge::modulation::params;

namespace {

const char* getParamName(Kind kind, int32_t paramId) {
	const char* name = paramNameForFile(kind, paramId);
	if (!name || !name[0] || !strcmp(name, "none")) {
		return nullptr;
	}
	return name;
}

} // namespace

namespace SysexParamStream {

void handleParamChange(ModelStackWithAutoParam const* modelStack) {
	if (!modelStack || !modelStack->autoParam) {
		return;
	}

	ParamManager* paramManager = modelStack->paramManager;
	ParamCollectionSummary* summary = modelStack->summary;
	if (!paramManager || !summary || !summary->paramCollection) {
		return;
	}

	Kind paramKind = summary->paramCollection->getParamKind();
	int32_t paramIdWithOffset = modelStack->paramId;

	switch (paramKind) {
	case Kind::UNPATCHED_SOUND:
		paramIdWithOffset += UNPATCHED_START;
		break;
	case Kind::PATCHED:
	case Kind::MIDI:
		break;
	default:
		// Skip patch-cables, expression, etc. for now
		return;
	}

	const char* paramName = getParamName(paramKind, paramIdWithOffset);
	if (!paramName) {
		return;
	}

	int32_t value = modelStack->autoParam->getCurrentValue();

	// If this AutoParam belongs to a kit NoteRow, treat it as a drum parameter.
	NoteRow* noteRow = modelStack->getNoteRowAllowNull();
	if (noteRow && noteRow->drum && noteRow->drum->kit) {
		Kit* kit = noteRow->drum->kit;
		int32_t drumIndex = kit->getDrumIndex(noteRow->drum);
		if (drumIndex >= 0) {
			KitSysex::notifyDrumParameterChanged(drumIndex, paramName, value);
		}
		return;
	}

	// Otherwise treat as a synth / global parameter (including MIDI instruments).
	SynthSysex::notifyParameterChanged((int32_t)paramKind, paramIdWithOffset, paramName, value);
}

} // namespace SysexParamStream
