#include "gui/ui/keyboard/modules/module_base.h"
#include "gui/ui/keyboard/keyboard_screen.h"

namespace deluge::gui::ui::keyboard::modules {

void KeyboardModule::requestRendering() {
	keyboardScreen.requestRendering();
}

} // namespace deluge::gui::ui::keyboard::modules
