#include "param.h"
#include "definitions_cxx.hpp"
#include "dsp/intervallic/patch.h"
#include "gui/ui/sound_editor.h"
#include "gui/ui/ui.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "processing/sound/sound.h"
#include "processing/source.h"
#include "util/cfunctions.h"
#include <algorithm>
#include <cstring>

using deluge::hid::display::OLED;

namespace deluge::gui::menu_item {

IntervallicParam intervallicParam{l10n::String::STRING_FOR_INTERVAL};

void IntervallicParam::beginSession(MenuItem* navigatedBackwardFrom) {
	readValueAgain();
}

void IntervallicParam::openAt(int32_t partial, int32_t row) {
	partialIndex = partial;
	rowIndex = row;
	globalParam = 0; // 0 = partial mode
	readValueAgain();
}

std::string_view IntervallicParam::getTitle() const {
	static char buffer[20];
	if (globalParam == 1) {
		return "LATTICE";
	}
	if (globalParam == 2) {
		return "INVERSION";
	}
	if (globalParam == 3) {
		return "VOIC SPREAD";
	}
	if (globalParam == 4) {
		return "PAIR FM";
	}
	if (globalParam == 5) {
		return "PLAY MODE";
	}
	if (globalParam >= 6 && globalParam <= 9) {
		strcpy(buffer, "LFO0 RATE");
		buffer[3] = static_cast<char>('0' + (globalParam - 6));
		return buffer;
	}

	// Partial params: "P1 LEVEL", "P2 INT", ...
	static constexpr char const* kRowNames[] = {
	    "LEVEL", "INT", "WAVE", "LVL LFO", "LFO PHS", "DETUNE", "DET LFO", "PAN",
	};
	strcpy(buffer, "P0 ");
	buffer[1] = static_cast<char>('1' + partialIndex); // display as 1–8
	int32_t row = std::clamp(rowIndex, int32_t{0}, int32_t{7});
	strcpy(buffer + 3, kRowNames[row]);
	return buffer;
}

int32_t IntervallicParam::upperLimit() const {
	if (globalParam == 5) {
		return 1; // play mode
	}
	if (globalParam == 1) {
		return 5; // lattice presets
	}
	if (globalParam == 2) {
		return 7; // inversion
	}
	if (globalParam >= 6 && globalParam <= 9) {
		return 255; // LFO rates
	}
	if (globalParam != 0) {
		return 255;
	}
	if (rowIndex == 2) {
		// Basic waves only for first test through SAW (includes SINE/TRI/SQUARE/ANALOG_SQUARE/SAW)
		return static_cast<int32_t>(OscType::SAW);
	}
	if (rowIndex == 1) {
		return 48;
	}
	if (rowIndex == 5) {
		return 200;
	}
	return 255;
}

int32_t IntervallicParam::getValue() const {
	if (patch == nullptr) {
		return 0;
	}
	if (globalParam == 1) {
		return patch->latticePreset;
	}
	if (globalParam == 2) {
		return patch->inversion;
	}
	if (globalParam == 3) {
		return patch->voiceSpread;
	}
	if (globalParam == 4) {
		return patch->pairFmAmount;
	}
	if (globalParam == 5) {
		return static_cast<int32_t>(patch->playMode);
	}
	if (globalParam >= 6 && globalParam <= 9) {
		return patch->localLfos[globalParam - 6].rate;
	}

	auto& p = patch->partials[partialIndex];
	switch (rowIndex) {
	case 0:
		return p.level;
	case 1:
		return p.intervalSemitones + 24;
	case 2:
		return static_cast<int32_t>(p.wave);
	case 3:
		return p.lfoDepthLevel;
	case 4:
		return p.lfoPhaseOffset;
	case 5:
		return p.detuneCents + 100;
	case 6:
		return p.lfoDepthDetune;
	case 7:
		return p.pan + 64;
	default:
		return 0;
	}
}

void IntervallicParam::setValue(int32_t val) {
	if (patch == nullptr) {
		return;
	}
	val = std::clamp(val, int32_t{0}, upperLimit());

	if (globalParam == 1) {
		patch->applyLatticePreset(val);
		return;
	}
	if (globalParam == 2) {
		patch->inversion = static_cast<int8_t>(val);
		return;
	}
	if (globalParam == 3) {
		patch->voiceSpread = static_cast<uint8_t>(val);
		return;
	}
	if (globalParam == 4) {
		patch->pairFmAmount = static_cast<uint8_t>(val);
		return;
	}
	if (globalParam == 5) {
		patch->playMode = static_cast<dsp::intervallic::PlayMode>(val);
		return;
	}
	if (globalParam >= 6 && globalParam <= 9) {
		patch->localLfos[globalParam - 6].rate = static_cast<uint8_t>(val);
		return;
	}

	auto& p = patch->partials[partialIndex];
	switch (rowIndex) {
	case 0:
		p.level = static_cast<uint8_t>(val);
		p.enabled = val > 0;
		break;
	case 1:
		p.intervalSemitones = static_cast<int8_t>(val - 24);
		break;
	case 2:
		p.wave = static_cast<OscType>(val);
		break;
	case 3:
		p.lfoDepthLevel = static_cast<uint8_t>(val);
		break;
	case 4:
		p.lfoPhaseOffset = static_cast<uint8_t>(val);
		break;
	case 5:
		p.detuneCents = static_cast<int8_t>(val - 100);
		break;
	case 6:
		p.lfoDepthDetune = static_cast<uint8_t>(val);
		break;
	case 7:
		p.pan = static_cast<int8_t>(val - 64);
		break;
	default:
		break;
	}
}

void IntervallicParam::readValueAgain() {
	patch = soundEditor.currentSound->sources[0].ensureIntervallicPatch();
	drawValue();
	blinkPad();
}

void IntervallicParam::selectEncoderAction(int32_t offset) {
	setValue(getValue() + offset);
	drawValue();
}

void IntervallicParam::drawValue() {
	if (display->haveOLED()) {
		renderUIsForOled();
		return;
	}

	char buffer[24];
	formatValue(buffer);
	display->setText(buffer);
}

void IntervallicParam::drawPixelsForOled() {
	char buffer[24];
	formatValue(buffer);
	OLED::main.drawStringCentred(buffer, 18 + OLED_MAIN_TOPMOST_PIXEL, kTextHugeSpacingX, kTextHugeSizeY);
}

void IntervallicParam::formatValue(char* buffer) const {
	if (globalParam == 5) {
		strcpy(buffer, getValue() == 0 ? "LATCH" : "ENV");
		return;
	}
	if (globalParam == 1) {
		static constexpr char const* kPresets[] = {"m9", "m13", "m11", "OPEN", "SUB", "CLSTR"};
		int32_t idx = std::clamp(getValue(), int32_t{0}, int32_t{5});
		strcpy(buffer, kPresets[idx]);
		return;
	}
	if (globalParam == 0 && rowIndex == 2) {
		switch (static_cast<OscType>(getValue())) {
		case OscType::TRIANGLE:
			strcpy(buffer, "TRI");
			break;
		case OscType::SQUARE:
		case OscType::ANALOG_SQUARE:
			strcpy(buffer, "SQUARE");
			break;
		case OscType::SAW:
		case OscType::ANALOG_SAW_2:
			strcpy(buffer, "SAW");
			break;
		case OscType::SINE:
		default:
			strcpy(buffer, "SINE");
			break;
		}
		return;
	}
	if (globalParam == 0 && rowIndex == 1) {
		int32_t semis = getValue() - 24;
		if (semis > 0) {
			buffer[0] = '+';
			intToString(semis, buffer + 1, 1);
		}
		else {
			intToString(semis, buffer, 1);
		}
		return;
	}
	if (globalParam == 0 && rowIndex == 5) {
		int32_t cents = getValue() - 100;
		if (cents > 0) {
			buffer[0] = '+';
			intToString(cents, buffer + 1, 1);
		}
		else {
			intToString(cents, buffer, 1);
		}
		return;
	}
	if (globalParam == 0 && rowIndex == 7) {
		int32_t pan = getValue() - 64;
		if (pan == 0) {
			strcpy(buffer, "C");
		}
		else if (pan < 0) {
			intToString(-pan, buffer, 1);
			strcat(buffer, "L");
		}
		else {
			intToString(pan, buffer, 1);
			strcat(buffer, "R");
		}
		return;
	}
	intToString(getValue(), buffer, 1);
}

void IntervallicParam::blinkPad() {
	if (globalParam != 0) {
		int32_t y = std::clamp(globalParam - 1, int32_t{0}, int32_t{7});
		soundEditor.setupShortcutBlink(8, y, 1);
	}
	else {
		soundEditor.setupShortcutBlink(partialIndex, rowIndex, 1);
	}
	soundEditor.blinkShortcut();
}

bool IntervallicParam::potentialShortcutPadAction(int32_t x, int32_t y, bool on) {
	if (!on) {
		return true;
	}
	if (x < 8) {
		openAt(x, y);
		return true;
	}
	if (x == 8) {
		// y0 lattice, y1 inv, y2 spread, y3 pairFm, y4 playMode, y5-7 LFO0-2 rates
		globalParam = 1 + y;
		if (globalParam > 9) {
			globalParam = 9;
		}
		partialIndex = 0;
		rowIndex = 0;
		readValueAgain();
		return true;
	}
	return false; // cols 9–15 → normal sound shortcuts
}

} // namespace deluge::gui::menu_item
