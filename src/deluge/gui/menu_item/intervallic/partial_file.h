#pragma once

#include "gui/menu_item/intervallic/helpers.h"
#include "gui/menu_item/menu_item.h"
#include "gui/ui/browser/sample_browser.h"
#include "gui/ui/sound_editor.h"
#include "gui/ui_timer_manager.h"
#include "processing/sound/sound.h"

namespace deluge::gui::menu_item::intervallic {

/// Opens sample browser to load a wavetable into this partial's WaveTableHolder.
class PartialFile final : public MenuItem {
public:
	PartialFile(l10n::String name, uint8_t partial_id) : MenuItem(name), partialId_{partial_id} {}

	bool isRelevant(ModControllableAudio*, int32_t) const override {
		auto* p = currentPartial(partialId_);
		return p != nullptr && p->wave == OscType::WAVETABLE;
	}

	void beginSession(MenuItem* navigatedBackwardFrom) override {
		soundEditor.shouldGoUpOneLevelOnBegin = true;
		soundEditor.intervallicWtPartial = static_cast<int8_t>(partialId_);
		if (auto* patch = currentPatch()) {
			patch->ensureWaveTableHolder(partialId_);
		}
		if (!openUI(&sampleBrowser)) {
			uiTimerManager.unsetTimer(TimerName::SHORTCUT_BLINK);
			soundEditor.intervallicWtPartial = -1;
		}
	}

	MenuItem* selectButtonPress() override {
		beginSession(nullptr);
		return NO_NAVIGATION;
	}

	MenuPermission checkPermissionToBeginSession(ModControllableAudio*, int32_t, MultiRange**) override {
		return MenuPermission::YES;
	}

private:
	uint8_t partialId_;
};

} // namespace deluge::gui::menu_item::intervallic
