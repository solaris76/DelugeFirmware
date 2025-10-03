#include "gui/ui/keyboard/modules/module_control.h"
#include "gui/l10n/l10n.h"
#include "gui/ui/keyboard/keyboard_screen.h"
#include "gui/ui/keyboard/modules/module_manager.h"
#include "hid/display/display.h"

namespace deluge::gui::ui::keyboard::modules {

// Module names for easy reference
const char* ModuleControl::moduleNames_[] = {"Timing Module", "Test Module", "Core Features Module",
                                             "Obvious Test Module"};

const int ModuleControl::moduleCount_ = 4;
int ModuleControl::currentModuleIndex_ = 0;

void ModuleControl::showModulePopup(const char* moduleName, bool enabled) {
	// Create popup message
	char popupText[32];
	if (enabled) {
		snprintf(popupText, sizeof(popupText), "%s ON", moduleName);
	}
	else {
		snprintf(popupText, sizeof(popupText), "%s OFF", moduleName);
	}

	// Display popup on OLED
	display->displayPopup(popupText);
}

void ModuleControl::toggleModule(const char* moduleName) {
	KeyboardModule* module = moduleManager.getModule(moduleName);
	if (module) {
		bool newState = !module->isEnabled();
		module->setEnabled(newState);
		keyboardScreen.requestRendering();     // Update display
		showModulePopup(moduleName, newState); // Show popup
	}
}

void ModuleControl::enableModule(const char* moduleName) {
	KeyboardModule* module = moduleManager.getModule(moduleName);
	if (module) {
		module->setEnabled(true);
		keyboardScreen.requestRendering(); // Update display
		showModulePopup(moduleName, true); // Show popup
	}
}

void ModuleControl::disableModule(const char* moduleName) {
	KeyboardModule* module = moduleManager.getModule(moduleName);
	if (module) {
		module->setEnabled(false);
		keyboardScreen.requestRendering();  // Update display
		showModulePopup(moduleName, false); // Show popup
	}
}

bool ModuleControl::isModuleEnabled(const char* moduleName) {
	KeyboardModule* module = moduleManager.getModule(moduleName);
	return module ? module->isEnabled() : false;
}

void ModuleControl::listModules() {
	// This would print to debug output
	// For now, just cycle through them
	cycleModules();
}

void ModuleControl::cycleModules() {
	// Cycle through modules for testing
	currentModuleIndex_ = (currentModuleIndex_ + 1) % moduleCount_;

	// Enable only the current module
	for (int i = 0; i < moduleCount_; i++) {
		if (i == currentModuleIndex_) {
			enableModule(moduleNames_[i]);
		}
		else {
			disableModule(moduleNames_[i]);
		}
	}

	keyboardScreen.requestRendering(); // Update display after cycling

	// Show popup for the currently active module
	showModulePopup(moduleNames_[currentModuleIndex_], true);
}

} // namespace deluge::gui::ui::keyboard::modules
