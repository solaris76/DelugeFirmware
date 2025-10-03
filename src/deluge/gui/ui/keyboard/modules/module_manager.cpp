#include "gui/ui/keyboard/modules/module_manager.h"
#include "gui/ui/keyboard/modules/core_features_module.h"
#include "gui/ui/keyboard/modules/generative_sequencer_module.h"
#include "gui/ui/keyboard/modules/obvious_test_module.h"
#include "gui/ui/keyboard/modules/test_module.h"
#include "gui/ui/keyboard/modules/timing_module.h"
#include "util/functions.h"
#include <algorithm>

namespace deluge::gui::ui::keyboard::modules {

// Global module manager instance
ModuleManager moduleManager;

ModuleManager::ModuleManager() : initialized_(false) {
	// Constructor initializes modules_ vector
}

ModuleManager::~ModuleManager() {
	// Clean up all modules
	for (auto* module : modules_) {
		delete module;
	}
	modules_.clear();
}

bool ModuleManager::init() {
	if (initialized_) {
		return true;
	}

	// Initialize built-in modules
	registerModule(new TimingModule());              // Beat position and tempo
	registerModule(new TestModule());                // Animated checkerboard
	registerModule(new CoreFeaturesModule());        // Audio clip detection
	registerModule(new GenerativeSequencerModule()); // Generative sequencer
	// registerModule(new ObviousTestModule()); // Disabled - too obvious!

	// Load additional modules from SD card
	loadModulesFromSD();

	// Sort modules by priority
	sortModulesByPriority();

	initialized_ = true;
	return true;
}

void ModuleManager::update(uint32_t deltaTime) {
	if (!initialized_) {
		return;
	}

	// Update all enabled modules
	for (auto* module : modules_) {
		if (module->isEnabled()) {
			module->update(deltaTime);
		}
	}
}

void ModuleManager::render(RGB image[][kDisplayWidth + kSideBarWidth],
                           uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	if (!initialized_) {
		return;
	}

	// Render all enabled modules in priority order
	for (auto* module : modules_) {
		if (module->isEnabled()) {
			module->render(image, occupancyMask);
		}
	}
}

void ModuleManager::renderModuleControls(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render module selection controls in control columns (y16, y17)
	// Left column (x = kDisplayWidth) - Module selection
	// Right column (x = kDisplayWidth + 1) - Module status

	// Left column: Module selection buttons
	for (int y = 0; y < kDisplayHeight; y++) {
		if (y < modules_.size()) {
			// Active module selection
			if (modules_[y]->isEnabled()) {
				image[y][kDisplayWidth] = RGB(0, 255, 0); // Green for active
			}
			else {
				image[y][kDisplayWidth] = RGB(64, 64, 64); // Gray for inactive
			}
		}
		else {
			image[y][kDisplayWidth] = RGB(16, 16, 16); // Dark for unused
		}
	}

	// Right column: Module status indicators
	for (int y = 0; y < kDisplayHeight; y++) {
		if (y < modules_.size()) {
			// Show module priority as brightness
			int priority = modules_[y]->getPriority();
			int brightness = (priority * 255) / 100; // Scale to 0-255
			image[y][kDisplayWidth + 1] = RGB(brightness, brightness, brightness);
		}
		else {
			image[y][kDisplayWidth + 1] = RGB(16, 16, 16); // Dark for unused
		}
	}

	// Highlight the control pads (y16, y17) for module cycling
	image[16][kDisplayWidth] = RGB(255, 255, 0); // Bright yellow
	image[17][kDisplayWidth] = RGB(255, 255, 0); // Bright yellow
}

bool ModuleManager::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	if (!initialized_) {
		return false;
	}

	// Pass event to modules in priority order (highest first)
	for (auto* module : modules_) {
		if (module->isEnabled() && module->handlePadPress(x, y, velocity)) {
			return true; // Event was handled
		}
	}

	return false; // No module handled the event
}

bool ModuleManager::handleEncoder(int32_t offset, bool isVertical) {
	if (!initialized_) {
		return false;
	}

	// Pass event to modules in priority order (highest first)
	for (auto* module : modules_) {
		if (module->isEnabled() && module->handleEncoder(offset, isVertical)) {
			return true; // Event was handled
		}
	}

	return false; // No module handled the event
}

bool ModuleManager::registerModule(KeyboardModule* module) {
	if (!module) {
		return false;
	}

	// Check if module already exists
	for (auto* existingModule : modules_) {
		if (existingModule == module) {
			return false; // Module already registered
		}
	}

	// Initialize the module
	if (!module->init()) {
		return false; // Module initialization failed
	}

	// Add to modules list
	modules_.push_back(module);

	// Sort by priority
	sortModulesByPriority();

	return true;
}

void ModuleManager::unregisterModule(KeyboardModule* module) {
	if (!module) {
		return;
	}

	// Find and remove module
	for (auto it = modules_.begin(); it != modules_.end(); ++it) {
		if (*it == module) {
			modules_.erase(it);
			delete module;
			break;
		}
	}
}

void ModuleManager::setModuleEnabled(const char* moduleName, bool enabled) {
	if (!moduleName) {
		return;
	}

	KeyboardModule* module = getModule(moduleName);
	if (module) {
		module->setEnabled(enabled);
	}
}

KeyboardModule* ModuleManager::getModule(const char* moduleName) {
	if (!moduleName) {
		return nullptr;
	}

	for (auto* module : modules_) {
		if (strcmp(module->getName(), moduleName) == 0) {
			return module;
		}
	}

	return nullptr;
}

int32_t ModuleManager::loadModulesFromSD() {
	// TODO: Implement SD card module loading
	// This would scan for .json module files and load them
	// For now, we only have built-in modules

	return 0; // Number of modules loaded
}

void ModuleManager::saveModuleConfiguration() {
	// TODO: Implement SD card configuration saving
	// This would save module settings to a .json file
}

void ModuleManager::sortModulesByPriority() {
	// Sort modules by priority (highest first)
	std::sort(modules_.begin(), modules_.end(),
	          [](const KeyboardModule* a, const KeyboardModule* b) { return a->getPriority() > b->getPriority(); });
}

} // namespace deluge::gui::ui::keyboard::modules
