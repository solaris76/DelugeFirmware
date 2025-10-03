#pragma once

#include "gui/ui/keyboard/modules/module_base.h"

namespace deluge::gui::ui::keyboard::modules {

/// @brief Simple test module
/// @details Shows a simple pattern to demonstrate module functionality
class TestModule : public KeyboardModule {
public:
	TestModule();
	~TestModule() override = default;

	// Module interface implementation
	bool init() override;
	void update(uint32_t deltaTime) override;
	void render(RGB image[][kDisplayWidth + kSideBarWidth],
	            uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override;
	bool handlePadPress(int32_t x, int32_t y, int32_t velocity) override;
	bool handleEncoder(int32_t offset, bool isVertical) override;

	// Module metadata
	const char* getName() const override { return "Test Pattern"; }
	const char* getAuthor() const override { return "Deluge Modules"; }
	const char* getVersion() const override { return "1.0.0"; }
	const char* getDescription() const override { return "Shows a simple test pattern"; }

private:
	// Animation state
	uint32_t animationFrame_ = 0;
	uint32_t lastUpdate_ = 0;

	// Colors
	RGB patternColor_;
	RGB backgroundColor_;

	/// @brief Render a simple animated pattern
	void renderPattern(RGB image[][kDisplayWidth + kSideBarWidth]);
};

} // namespace deluge::gui::ui::keyboard::modules
