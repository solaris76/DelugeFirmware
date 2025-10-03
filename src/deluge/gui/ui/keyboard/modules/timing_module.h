#pragma once

#include "gui/ui/keyboard/modules/module_base.h"

namespace deluge::gui::ui::keyboard::modules {

/// @brief Timing display module
/// @details Shows beat position, tempo, and timing information on the keyboard grid
class TimingModule : public KeyboardModule {
public:
	TimingModule();
	~TimingModule() override = default;

	// Module interface implementation
	bool init() override;
	void update(uint32_t deltaTime) override;
	void render(RGB image[][kDisplayWidth + kSideBarWidth],
	            uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override;
	bool handlePadPress(int32_t x, int32_t y, int32_t velocity) override;
	bool handleEncoder(int32_t offset, bool isVertical) override;

	// Module metadata
	const char* getName() const override { return "Timing Display"; }
	const char* getAuthor() const override { return "Deluge Modules"; }
	const char* getVersion() const override { return "1.0.0"; }
	const char* getDescription() const override { return "Shows beat position and tempo visualization"; }

private:
	// Timing state
	uint32_t currentBeat_ = 0;
	uint32_t lastBeatUpdate_ = 0;
	float bpm_ = 120.0f;
	bool isPlaying_ = false;

	// Visual settings
	RGB beatColor_;
	RGB tempoColor_;
	RGB inactiveColor_;

	// Configuration
	bool showBeatPosition_ = true;
	bool showTempoVisualization_ = true;
	bool showPlaybackState_ = true;

	/// @brief Update timing information
	void updateTimingInfo();

	/// @brief Render beat position indicator
	void renderBeatPosition(RGB image[][kDisplayWidth + kSideBarWidth]);

	/// @brief Render tempo visualization
	void renderTempoVisualization(RGB image[][kDisplayWidth + kSideBarWidth]);

	/// @brief Render playback state
	void renderPlaybackState(RGB image[][kDisplayWidth + kSideBarWidth]);

	/// @brief Calculate color based on tempo intensity
	RGB calculateTempoColor(float intensity);
};

} // namespace deluge::gui::ui::keyboard::modules
