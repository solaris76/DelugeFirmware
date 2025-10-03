#pragma once

#include "gui/ui/keyboard/modules/module_base.h"
#include "util/containers.h"
#include <cstdint>

// Forward declarations
namespace deluge::gui::ui::keyboard::modules {
class TimingModule;
class TestModule;
class CoreFeaturesModule;
class ObviousTestModule;
class GenerativeSequencerModule;
} // namespace deluge::gui::ui::keyboard::modules

namespace deluge::gui::ui::keyboard::modules {

/// @brief Manages all keyboard modules
/// @details Handles loading, enabling/disabling, and coordinating modules
class ModuleManager {
public:
	ModuleManager();
	~ModuleManager();

	/// @brief Initialize the module manager
	/// @return true if successful
	bool init();

	/// @brief Update all active modules
	/// @param deltaTime Time since last update
	void update(uint32_t deltaTime);

	/// @brief Render module selection controls in control columns
	/// @param image LED color array
	void renderModuleControls(RGB image[][kDisplayWidth + kSideBarWidth]);

	/// @brief Render all active modules
	/// @param image LED color array
	/// @param occupancyMask Occupancy mask
	void render(RGB image[][kDisplayWidth + kSideBarWidth], uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]);

	/// @brief Handle pad press events
	/// @param x X coordinate
	/// @param y Y coordinate
	/// @param velocity Velocity
	/// @return true if any module handled the event
	bool handlePadPress(int32_t x, int32_t y, int32_t velocity);

	/// @brief Handle encoder events
	/// @param offset Encoder movement
	/// @param isVertical Vertical or horizontal encoder
	/// @return true if any module handled the event
	bool handleEncoder(int32_t offset, bool isVertical);

	/// @brief Register a module
	/// @param module Module to register
	/// @return true if successful
	bool registerModule(KeyboardModule* module);

	/// @brief Unregister a module
	/// @param module Module to unregister
	void unregisterModule(KeyboardModule* module);

	/// @brief Enable/disable a module
	/// @param moduleName Name of module to toggle
	/// @param enabled Enable or disable
	void setModuleEnabled(const char* moduleName, bool enabled);

	/// @brief Get module by name
	/// @param moduleName Name of module
	/// @return Pointer to module or nullptr
	KeyboardModule* getModule(const char* moduleName);

	/// @brief Get all registered modules
	/// @return Vector of all modules
	const deluge::vector<KeyboardModule*>& getAllModules() const { return modules_; }

	/// @brief Load modules from SD card
	/// @return Number of modules loaded
	int32_t loadModulesFromSD();

	/// @brief Save module configuration to SD card
	void saveModuleConfiguration();

private:
	deluge::vector<KeyboardModule*> modules_;
	bool initialized_ = false;

	/// @brief Sort modules by priority (highest first)
	void sortModulesByPriority();
};

/// @brief Global module manager instance
extern ModuleManager moduleManager;

} // namespace deluge::gui::ui::keyboard::modules
