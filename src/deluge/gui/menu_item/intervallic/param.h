#pragma once

#include "gui/menu_item/menu_item.h"

namespace deluge::dsp::intervallic {
struct Patch;
}

namespace deluge::gui::menu_item {

/// Shift-pad editor for Intervallic partials (cols 0–7) + lattice globals on master pads.
class IntervallicParam final : public MenuItem {
public:
	using MenuItem::MenuItem;
	explicit IntervallicParam(l10n::String newName) : MenuItem(newName) {}

	void beginSession(MenuItem* navigatedBackwardFrom) override;
	void readValueAgain() final;
	void selectEncoderAction(int32_t offset) final;
	void drawPixelsForOled() override;
	void drawValue();
	[[nodiscard]] std::string_view getTitle() const override;

	bool potentialShortcutPadAction(int32_t x, int32_t y, bool on);
	void openAt(int32_t partial, int32_t row);

	/// Param addressing: partial 0–7 + row 0–7, or global rows on x>=8
	int32_t partialIndex{0};
	int32_t rowIndex{0}; // 0 level, 1 interval, 2 wave, 3 pw/detune, 4 phase stub, 5 detune, 6 wavepos stub, 7 pan
	/// 0 = editing a partial (partialIndex/rowIndex); 1+ = lattice globals (see param.cpp)
	int32_t globalParam{0};

	dsp::intervallic::Patch* patch{nullptr};

private:
	int32_t getValue() const;
	void setValue(int32_t val);
	int32_t upperLimit() const;
	void blinkPad();
	void formatValue(char* buffer) const;
};

extern IntervallicParam intervallicParam;

} // namespace deluge::gui::menu_item
