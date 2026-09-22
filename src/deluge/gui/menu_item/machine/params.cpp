#include "gui/menu_item/machine/params.h"
#include "definitions_cxx.hpp"
#include "gui/menu_item/value_scaling.h"
#include "gui/ui/sound_editor.h"
#include "model/clip/instrument_clip.h"
#include "model/drum/drum.h"
#include "model/instrument/kit.h"
#include "model/song/song.h"
#include "processing/sound/sound_drum.h"
#include <algorithm>

namespace deluge::gui::menu_item::machine {

void Dial::readCurrentValue() {
	auto* pps = soundEditor.currentParamManager->getPatchedParamSet();
	int32_t dial = computeCurrentValueForMachineDial(pps->getValue(getP()));
	if (dial > maxValue_) {
		dial = maxValue_;
	}
	setValue(dial);
}

int32_t Dial::getFinalValue() {
	return computeFinalValueForMachineDial(std::clamp(getValue(), int32_t{0}, maxValue_));
}

void Dial::mirrorDialOntoPatch(Sound& sound, uint8_t dial) const {
	Source& src = sound.sources[0];
	if (src.oscType != requiredType_) {
		return;
	}
	ensurePatch(src);
	uint8_t* f = getter_(src);
	if (f) {
		*f = dial;
	}
}

void Dial::writeCurrentValue() {
	Integer::writeCurrentValue();

	uint8_t dial = static_cast<uint8_t>(std::clamp(getValue(), int32_t{0}, maxValue_));
	mirrorDialOntoPatch(*soundEditor.currentSound, dial);

	if (currentUIMode == UI_MODE_HOLDING_AFFECT_ENTIRE_IN_SOUND_EDITOR && soundEditor.editingKitRow()) {
		Kit* kit = getCurrentKit();
		for (Drum* thisDrum = kit->firstDrum; thisDrum != nullptr; thisDrum = thisDrum->next) {
			if (thisDrum->type == DrumType::SOUND) {
				mirrorDialOntoPatch(*static_cast<SoundDrum*>(thisDrum), dial);
			}
		}
	}
}

} // namespace deluge::gui::menu_item::machine
