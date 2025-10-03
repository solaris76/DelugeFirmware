#pragma once

#include "gui/ui/keyboard/modules/module_manager.h"

namespace deluge::gui::ui::keyboard::modules {

/// @brief Module control system
/// @details Provides runtime control over module enable/disable
class ModuleControl {
public:
	/// @brief Toggle a module on/off
	/// @param moduleName Name of module to toggle
	static void toggleModule(const char* moduleName);

	/// @brief Enable a module
	/// @param moduleName Name of module to enable
	static void enableModule(const char* moduleName);

	/// @brief Disable a module
	/// @param moduleName Name of module to disable
	static void disableModule(const char* moduleName);

	/// @brief Get module status
	/// @param moduleName Name of module
	/// @return true if enabled, false if disabled
	static bool isModuleEnabled(const char* moduleName);

	/// @brief List all available modules
	static void listModules();

	/// @brief Cycle through modules (for testing)
	static void cycleModules();

	/// @brief Show module popup on OLED
	/// @param moduleName Name of module
	/// @param enabled Whether module is enabled
	static void showModulePopup(const char* moduleName, bool enabled);

private:
	static int currentModuleIndex_;
	static const char* moduleNames_[];
	static const int moduleCount_;
};

} // namespace deluge::gui::ui::keyboard::modules
