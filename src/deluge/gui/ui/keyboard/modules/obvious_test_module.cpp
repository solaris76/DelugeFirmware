#include "gui/ui/keyboard/modules/obvious_test_module.h"

namespace deluge::gui::ui::keyboard::modules {

ObviousTestModule::ObviousTestModule() {
	// Set highest priority to render on top of everything
	setPriority(100);
}

bool ObviousTestModule::init() {
	return true;
}

void ObviousTestModule::update(uint32_t deltaTime) {
	if (!isEnabled()) {
		return;
	}
	animationFrame_++;
	requestRendering(); // Request display update for animation
}

void ObviousTestModule::render(RGB image[][kDisplayWidth + kSideBarWidth],
                               uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	if (!isEnabled()) {
		return;
	}

	// Fill entire screen with bright colors - impossible to miss!
	for (int y = 0; y < kDisplayHeight; y++) {
		for (int x = 0; x < kDisplayWidth; x++) {
			// Create a bright animated pattern
			int colorIndex = (x + y + animationFrame_) % 3;
			switch (colorIndex) {
			case 0:
				image[y][x] = RGB(255, 0, 0);
				break; // Bright red
			case 1:
				image[y][x] = RGB(0, 255, 0);
				break; // Bright green
			case 2:
				image[y][x] = RGB(0, 0, 255);
				break; // Bright blue
			}
		}
	}
}

bool ObviousTestModule::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	if (!isEnabled()) {
		return false;
	}
	return false; // Let other modules handle
}

bool ObviousTestModule::handleEncoder(int32_t offset, bool isVertical) {
	if (!isEnabled()) {
		return false;
	}
	return false; // Let other modules handle
}

} // namespace deluge::gui::ui::keyboard::modules
