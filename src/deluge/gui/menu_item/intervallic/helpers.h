#pragma once

#include "dsp/intervallic/patch.h"
#include "gui/ui/sound_editor.h"
#include "processing/sound/sound.h"
#include "processing/source.h"

namespace deluge::gui::menu_item::intervallic {

inline dsp::intervallic::Patch* currentPatch() {
	if (soundEditor.currentSound == nullptr) {
		return nullptr;
	}
	return soundEditor.currentSound->sources[0].ensureIntervallicPatch();
}

inline dsp::intervallic::Partial* currentPartial(uint8_t partialId) {
	auto* patch = currentPatch();
	if (patch == nullptr || partialId >= dsp::intervallic::kNumPartials) {
		return nullptr;
	}
	return &patch->partials[partialId];
}

} // namespace deluge::gui::menu_item::intervallic
