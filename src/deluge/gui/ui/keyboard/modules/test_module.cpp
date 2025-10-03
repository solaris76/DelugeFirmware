#include "gui/ui/keyboard/modules/test_module.h"

namespace deluge::gui::ui::keyboard::modules {

TestModule::TestModule() {
	// Set module priority (lower than timing module)
	setPriority(5);

	// Initialize colors
	patternColor_ = RGB(255, 0, 255);   // Magenta for pattern
	backgroundColor_ = RGB(16, 16, 16); // Dark gray for background
}

bool TestModule::init() {
	// Module initialization successful
	return true;
}

void TestModule::update(uint32_t deltaTime) {
	if (!isEnabled()) {
		return;
	}

	// Update animation frame
	lastUpdate_ += deltaTime;
	if (lastUpdate_ >= 100) { // Update every 100ms
		animationFrame_++;
		lastUpdate_ = 0;
		requestRendering(); // Request display update for animation
	}
}

void TestModule::render(RGB image[][kDisplayWidth + kSideBarWidth],
                        uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	if (!isEnabled()) {
		return;
	}

	renderPattern(image);
}

bool TestModule::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	if (!isEnabled()) {
		return false;
	}

	// Handle test module pad presses
	// For now, we don't handle any specific pad presses
	return false; // Let other modules handle the event
}

bool TestModule::handleEncoder(int32_t offset, bool isVertical) {
	if (!isEnabled()) {
		return false;
	}

	// Handle encoder events for test module
	// For now, we don't handle encoder events
	return false; // Let other modules handle the event
}

void TestModule::renderPattern(RGB image[][kDisplayWidth + kSideBarWidth]) {
	// Render a simple animated pattern on rows 4-7 - make it very obvious
	for (int y = 4; y < 8; y++) {
		for (int x = 0; x < kDisplayWidth; x++) {
			// Create a simple moving pattern
			int patternX = (x + animationFrame_) % kDisplayWidth;
			int patternY = (y + animationFrame_) % 4;

			// Create a checkerboard pattern that moves
			if ((patternX + patternY) % 2 == 0) {
				image[y][x] = RGB(255, 0, 255); // Bright magenta
			}
			else {
				image[y][x] = RGB(0, 255, 255); // Bright cyan
			}
		}
	}
}

} // namespace deluge::gui::ui::keyboard::modules
