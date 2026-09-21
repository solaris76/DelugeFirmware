/*
 * Copyright (c) 2014-2023 Synthstrom Audible Limited
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
#pragma once
#include "definitions_cxx.hpp"
#include "dsp/machine/patches.h"
#include "gui/menu_item/formatted_title.h"
#include "gui/menu_item/selection.h"
#include "gui/menu_item/submenu.h"
#include "gui/ui/sound_editor.h"
#include "model/song/song.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound.h"
#include "processing/source.h"
#include "util/comparison.h"

#include <hid/display/oled.h>
#include <vector>

extern gui::menu_item::Submenu dxMenu;

namespace deluge::gui::menu_item::osc {

inline bool mayUseMachine(uint8_t sourceId) {
	return sourceId == 0;
}

class Type final : public Selection, public FormattedTitle {
public:
	Type(l10n::String name, l10n::String title_format_str, uint8_t source_id)
	    : Selection(name), FormattedTitle(title_format_str, source_id + 1), sourceId_{source_id} {};
	void beginSession(MenuItem* navigatedBackwardFrom) override { Selection::beginSession(navigatedBackwardFrom); }

	bool mayUseDx() const { return !soundEditor.editingKit() && sourceId_ == 0; }

	/// Build OscType list matching getOptions() order for current context.
	/// Phi family sits with basic/wavetable (same as on-main); machines are kit-friendly source-0 engines.
	void buildTypeList(std::vector<OscType>& out) const {
		out.clear();
		out.push_back(OscType::SINE);
		out.push_back(OscType::TRIANGLE);
		out.push_back(OscType::SQUARE);
		out.push_back(OscType::ANALOG_SQUARE);
		out.push_back(OscType::SAW);
		out.push_back(OscType::ANALOG_SAW_2);
		out.push_back(OscType::WAVETABLE);
		out.push_back(OscType::PHI_MORPH);
		out.push_back(OscType::PHI_STAIR);
		out.push_back(OscType::PHI_WEAVE);
		out.push_back(OscType::PHI_VOX);
		out.push_back(OscType::PHI_SWARM);
		out.push_back(OscType::PHI_GENDY);

		if (soundEditor.currentSound->getSynthMode() == SynthMode::RINGMOD) {
			return;
		}

		out.push_back(OscType::SAMPLE);

		if (mayUseMachine(sourceId_)) {
			out.push_back(OscType::WAVETONE);
			out.push_back(OscType::FM_DRUM);
			out.push_back(OscType::PERC);
			out.push_back(OscType::SKIN);
			out.push_back(OscType::RESONATOR);
		}

		if (mayUseDx()) {
			out.push_back(OscType::DX7);
		}

		if (AudioEngine::micPluggedIn || AudioEngine::lineInPluggedIn) {
			out.push_back(OscType::INPUT_L);
			out.push_back(OscType::INPUT_R);
			out.push_back(OscType::INPUT_STEREO);
		}
		else {
			out.push_back(OscType::INPUT_L); // shown as "Input"
		}
	}

	void readCurrentValue() override {
		std::vector<OscType> types;
		buildTypeList(types);
		OscType cur = soundEditor.currentSound->sources[sourceId_].oscType;
		int32_t idx = 0;
		for (size_t i = 0; i < types.size(); i++) {
			if (types[i] == cur) {
				idx = static_cast<int32_t>(i);
				break;
			}
		}
		setValue(idx);
	}

	void writeCurrentValue() override {
		OscType oldValue = soundEditor.currentSound->sources[sourceId_].oscType;
		std::vector<OscType> types;
		buildTypeList(types);
		int32_t idx = getValue();
		if (idx < 0 || idx >= static_cast<int32_t>(types.size())) {
			return;
		}
		OscType newValue = types[idx];

		const auto needs_unassignment = {
		    OscType::INPUT_L,   OscType::INPUT_R,   OscType::INPUT_STEREO, OscType::SAMPLE,    OscType::DX7,
		    OscType::WAVETABLE, OscType::PHI_MORPH, OscType::PHI_STAIR,    OscType::PHI_WEAVE, OscType::PHI_VOX,
		    OscType::PHI_SWARM, OscType::PHI_GENDY, OscType::WAVETONE,     OscType::FM_DRUM,   OscType::PERC,
		    OscType::SKIN,      OscType::RESONATOR,
		};

		if (util::one_of(oldValue, needs_unassignment) || util::one_of(newValue, needs_unassignment)) {
			soundEditor.currentSound->killAllVoices();
		}

		soundEditor.currentSound->sources[sourceId_].setOscType(newValue);

		if (oldValue == OscType::SQUARE || newValue == OscType::SQUARE) {
			soundEditor.currentSound->setupPatchingForAllParamManagers(currentSong);
		}
	}

	[[nodiscard]] std::string_view getTitle() const override { return FormattedTitle::title(); }

	deluge::vector<std::string_view> getOptions(OptType optType) override {
		(void)optType;
		using enum l10n::String;
		std::vector<OscType> types;
		buildTypeList(types);
		deluge::vector<std::string_view> options;
		options.reserve(types.size());
		for (OscType t : types) {
			switch (t) {
			case OscType::SINE:
				options.emplace_back(l10n::getView(STRING_FOR_SINE));
				break;
			case OscType::TRIANGLE:
				options.emplace_back(l10n::getView(STRING_FOR_TRIANGLE));
				break;
			case OscType::SQUARE:
				options.emplace_back(l10n::getView(STRING_FOR_SQUARE));
				break;
			case OscType::ANALOG_SQUARE:
				options.emplace_back(l10n::getView(STRING_FOR_ANALOG_SQUARE));
				break;
			case OscType::SAW:
				options.emplace_back(l10n::getView(STRING_FOR_SAW));
				break;
			case OscType::ANALOG_SAW_2:
				options.emplace_back(l10n::getView(STRING_FOR_ANALOG_SAW));
				break;
			case OscType::WAVETABLE:
				options.emplace_back(l10n::getView(STRING_FOR_WAVETABLE));
				break;
			case OscType::PHI_MORPH:
				options.emplace_back(l10n::getView(STRING_FOR_PHI_MORPH));
				break;
			case OscType::PHI_STAIR:
				options.emplace_back(l10n::getView(STRING_FOR_PHI_STAIR));
				break;
			case OscType::PHI_WEAVE:
				options.emplace_back(l10n::getView(STRING_FOR_PHI_WEAVE));
				break;
			case OscType::PHI_VOX:
				options.emplace_back(l10n::getView(STRING_FOR_PHI_VOX));
				break;
			case OscType::PHI_SWARM:
				options.emplace_back(l10n::getView(STRING_FOR_PHI_SWARM));
				break;
			case OscType::PHI_GENDY:
				options.emplace_back(l10n::getView(STRING_FOR_PHI_GENDY));
				break;
			case OscType::SAMPLE:
				options.emplace_back(l10n::getView(STRING_FOR_SAMPLE));
				break;
			case OscType::DX7:
				options.emplace_back(l10n::getView(STRING_FOR_DX7));
				break;
			case OscType::WAVETONE:
				options.emplace_back(l10n::getView(STRING_FOR_WAVETONE));
				break;
			case OscType::FM_DRUM:
				options.emplace_back(l10n::getView(STRING_FOR_FM_DRUM));
				break;
			case OscType::PERC:
				options.emplace_back(l10n::getView(STRING_FOR_PERC));
				break;
			case OscType::SKIN:
				options.emplace_back(l10n::getView(STRING_FOR_SKIN));
				break;
			case OscType::RESONATOR:
				options.emplace_back(l10n::getView(STRING_FOR_RESONATOR));
				break;
			case OscType::INPUT_L:
				if (!(AudioEngine::micPluggedIn || AudioEngine::lineInPluggedIn)) {
					options.emplace_back(l10n::getView(STRING_FOR_INPUT));
				}
				else {
					options.emplace_back(l10n::getView(STRING_FOR_INPUT_LEFT));
				}
				break;
			case OscType::INPUT_R:
				options.emplace_back(l10n::getView(STRING_FOR_INPUT_RIGHT));
				break;
			case OscType::INPUT_STEREO:
				options.emplace_back(l10n::getView(STRING_FOR_INPUT_STEREO));
				break;
			}
		}
		return options;
	}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		Sound* sound = static_cast<Sound*>(modControllable);
		return (sound->getSynthMode() != SynthMode::FM);
	}

	MenuItem* selectButtonPress() override {
		if (soundEditor.currentSound->sources[sourceId_].oscType != OscType::DX7) {
			return nullptr;
		}
		return &dxMenu;
	}

	[[nodiscard]] bool showColumnLabel() const override { return false; }

	void renderInHorizontalMenu(const SlotPosition& slot) override {
		oled_canvas::Canvas& image = OLED::main;

		const OscType osc_type = soundEditor.currentSound->sources[sourceId_].oscType;
		if (osc_type == OscType::DX7) {
			const auto option = getOptions(OptType::FULL)[getValue()].data();
			return image.drawStringCentered(option, slot.start_x, slot.start_y + kHorizontalMenuSlotYOffset + 5,
			                                kTextTitleSpacingX, kTextTitleSizeY, slot.width);
		}

		const Icon& icon = [&] {
			switch (osc_type) {
			case OscType::SINE:
				return OLED::sineIcon;
			case OscType::TRIANGLE:
				return OLED::triangleIcon;
			case OscType::SQUARE:
			case OscType::ANALOG_SQUARE:
				return OLED::squareIcon;
			case OscType::SAW:
			case OscType::ANALOG_SAW_2:
				return OLED::sawIcon;
			case OscType::SAMPLE:
				return OLED::sampleIcon;
			case OscType::INPUT_STEREO:
			case OscType::INPUT_L:
			case OscType::INPUT_R:
				return AudioEngine::lineInPluggedIn ? OLED::inputIcon : OLED::micIcon;
			case OscType::WAVETABLE:
				return OLED::wavetableIcon;
			case OscType::PHI_MORPH:
				return OLED::phiMorphIcon;
			case OscType::PHI_WEAVE:
				return OLED::phiWeaveIcon;
			case OscType::PHI_VOX:
				return OLED::phiVoxIcon;
			case OscType::PHI_SWARM:
				return OLED::phiSwarmIcon;
			case OscType::PHI_GENDY:
				return OLED::phiGendyIcon;
			case OscType::PHI_STAIR:
				return OLED::phiStairIcon;
			case OscType::WAVETONE:
				return OLED::waveToneIcon;
			case OscType::FM_DRUM:
				return OLED::fmDrumIcon;
			case OscType::PERC:
				return OLED::percIcon;
			case OscType::SKIN:
				return OLED::percIcon;
			case OscType::RESONATOR:
				return OLED::percIcon;
			default:
				return OLED::sineIcon;
			}
		}();

		image.drawIconCentered(icon, slot.start_x, slot.width, slot.start_y + kHorizontalMenuSlotYOffset + 2);

		if (osc_type == OscType::ANALOG_SQUARE || osc_type == OscType::ANALOG_SAW_2) {
			const int32_t x = slot.start_x + 4;
			constexpr int32_t y = OLED_MAIN_HEIGHT_PIXELS - kTextSpacingY - 8;
			image.clearAreaExact(x - 1, y - 1, x + kTextSpacingX + 1, y + kTextSpacingY + 1);
			image.drawChar('A', x, y, kTextSpacingX, kTextSpacingY);
		}
	}

	bool wrapAround() override {
		return parent != nullptr && parent->renderingStyle() == Submenu::RenderingStyle::HORIZONTAL;
	}

private:
	uint8_t sourceId_;
};

} // namespace deluge::gui::menu_item::osc
