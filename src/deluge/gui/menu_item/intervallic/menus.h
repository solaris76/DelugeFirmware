#pragma once

#include "gui/menu_item/horizontal_menu.h"
#include "gui/menu_item/horizontal_menu_group.h"
#include "gui/menu_item/intervallic/lattice_params.h"
#include "gui/menu_item/intervallic/partial_file.h"
#include "gui/menu_item/intervallic/partial_param.h"
#include "gui/menu_item/intervallic/partial_phi.h"
#include "util/cfunctions.h"
#include <cstring>

namespace deluge::gui::menu_item::intervallic {

class PartialMenu final : public HorizontalMenu {
public:
	PartialMenu(std::initializer_list<MenuItem*> items, uint8_t partial_id)
	    : HorizontalMenu(l10n::String::STRING_FOR_INTERVAL, items), partialId_{partial_id} {}

	[[nodiscard]] std::string_view getTitle() const override {
		static char buf[4];
		buf[0] = 'P';
		buf[1] = static_cast<char>('1' + partialId_);
		buf[2] = 0;
		return buf;
	}

	uint8_t partialId() const { return partialId_; }

private:
	uint8_t partialId_;
};

class LatticeMenu final : public HorizontalMenu {
public:
	using HorizontalMenu::HorizontalMenu;
	[[nodiscard]] std::string_view getTitle() const override { return "LATTICE"; }
};

// Focus helpers used by Shift+pad entry
MenuItem* partialChildForRow(uint8_t partialId, int32_t row);
MenuItem* latticeChildForRow(int32_t row);
HorizontalMenu* partialMenu(uint8_t partialId);
HorizontalMenu* latticeMenu();
HorizontalMenuGroup* menuGroup();

extern HorizontalMenuGroup intervallicMenuGroup;

} // namespace deluge::gui::menu_item::intervallic
