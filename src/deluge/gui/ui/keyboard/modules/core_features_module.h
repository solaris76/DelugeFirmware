#pragma once

#include "gui/ui/keyboard/modules/module_base.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"

namespace deluge::gui::ui::keyboard::modules {

/// @brief Core features demonstration module
/// @details Shows how to access Deluge's core systems: timing, audio, waveforms
class CoreFeaturesModule : public KeyboardModule {
public:
	CoreFeaturesModule();
	~CoreFeaturesModule() override = default;

	// Module interface implementation
	bool init() override;
	void update(uint32_t deltaTime) override;
	void render(RGB image[][kDisplayWidth + kSideBarWidth],
	            uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override;
	bool handlePadPress(int32_t x, int32_t y, int32_t velocity) override;
	bool handleEncoder(int32_t offset, bool isVertical) override;

	// Module metadata
	const char* getName() const override { return "Core Features Demo"; }
	const char* getAuthor() const override { return "Deluge Modules"; }
	const char* getVersion() const override { return "1.0.0"; }
	const char* getDescription() const override { return "Demonstrates access to timing, audio, and waveform systems"; }

private:
	// Timing access
	uint32_t lastTickCount_ = 0;
	float currentBPM_ = 120.0f;
	bool isPlaying_ = false;

	// Visual state
	uint32_t animationFrame_ = 0;
	RGB timingColor_;
	RGB audioColor_;
	RGB waveformColor_;

	/// @brief Access timing information
	void updateTimingInfo();

	/// @brief Render timing information
	void renderTimingInfo(RGB image[][kDisplayWidth + kSideBarWidth]);

	/// @brief Render audio information
	void renderAudioInfo(RGB image[][kDisplayWidth + kSideBarWidth]);

	/// @brief Render waveform (simplified version)
	void renderWaveform(RGB image[][kDisplayWidth + kSideBarWidth]);

	/// @brief Get current BPM from playback handler
	float getCurrentBPM();

	/// @brief Get current tick count
	uint32_t getCurrentTickCount();

	/// @brief Check if playback is active
	bool isPlaybackActive();
};

} // namespace deluge::gui::ui::keyboard::modules
