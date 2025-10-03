#pragma once

#include "gui/ui/keyboard/modules/module_base.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/instrument.h"
#include "modulation/arpeggiator.h"

namespace deluge::gui::ui::keyboard::modules {

/// @brief Generative sequencer module
/// @details Generates and sends notes using algorithmic patterns
class GenerativeSequencerModule : public KeyboardModule {
public:
	GenerativeSequencerModule();
	~GenerativeSequencerModule() override = default;

	// Module interface implementation
	bool init() override;
	void update(uint32_t deltaTime) override;
	void render(RGB image[][kDisplayWidth + kSideBarWidth],
	            uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override;
	bool handlePadPress(int32_t x, int32_t y, int32_t velocity) override;
	bool handleEncoder(int32_t offset, bool isVertical) override;

	// Module metadata
	const char* getName() const override { return "Generative Sequencer"; }
	const char* getAuthor() const override { return "Deluge Modules"; }
	const char* getVersion() const override { return "1.0.0"; }
	const char* getDescription() const override { return "Generates algorithmic note sequences"; }

private:
	// Generative parameters
	uint32_t lastNoteTime_ = 0;
	uint32_t noteInterval_ = 500; // ms between notes
	int32_t currentNote_ = 60;    // Middle C
	int32_t noteRange_ = 24;      // 2 octaves
	int32_t velocity_ = 100;

	// Pattern generation
	uint32_t patternStep_ = 0;
	uint32_t patternLength_ = 8;
	bool isPlaying_ = false;

	// Visual state
	uint32_t animationFrame_ = 0;
	RGB activeColor_;
	RGB inactiveColor_;

	// Note generation methods
	void generateNextNote();
	int32_t getRandomNote();
	int32_t getPatternNote();
	int32_t getScaleNote();

	// Rendering methods
	void renderSequencer(RGB image[][kDisplayWidth + kSideBarWidth]);
	void renderPattern(RGB image[][kDisplayWidth + kSideBarWidth]);
	void renderControls(RGB image[][kDisplayWidth + kSideBarWidth]);

	// Helper methods
	InstrumentClip* getCurrentInstrumentClip();
	void sendNoteToInstrument(int32_t noteCode, uint8_t velocity, bool isOn);
};

} // namespace deluge::gui::ui::keyboard::modules
