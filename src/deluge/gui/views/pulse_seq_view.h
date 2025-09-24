#pragma once

#include "definitions_cxx.hpp"
#include "gui/colour/rgb.h"
#include "gui/ui/ui.h"
#include "hid/button.h"
#include "model/clip/clip_minder.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/instrument.h"

namespace deluge::gui::views {

class PulseSeqView : public UI, public ClipMinder {
public:
	PulseSeqView() = default;
	virtual ~PulseSeqView() = default;

	// Static flag to track if we came from pulse seq view
	static bool cameFromPulseSeqView;

	void openUI(InstrumentClip* clip);
	void closeUI();
	bool opened() override;
	void focusRegained() override;

	// UI overrides
	void render();
	void renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) override;
	void render7Seg();
	bool renderMainPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                    uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], bool drawUndefinedArea);

	// Button handling
	ActionResult buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) override;
	ActionResult padAction(int32_t x, int32_t y, int32_t velocity) override;
	ActionResult verticalEncoderAction(int32_t offset, bool inCardRoutine) override;
	ActionResult horizontalEncoderAction(int32_t offset) override;
	void modEncoderAction(int32_t whichModEncoder, int32_t offset) override;

	// Menu navigation
	void selectEncoderAction(int32_t offset);
	void backButtonAction();

	// Required pure virtual function
	UIType getUIType() override { return UIType::NONE; }

	// Pulse sequencer parameters
	void setParameterValue(int32_t column, int32_t value);
	int32_t getParameterValue(int32_t column);
	void setPitchValue(int32_t column, int32_t value);
	int32_t getPitchValue(int32_t column);
	void setPulseCountValue(int32_t column, int32_t value);
	int32_t getPulseCountValue(int32_t column);
	void generatePattern();

private:
	InstrumentClip* currentClip_ = nullptr;
	Instrument* currentInstrument_ = nullptr;

	// Pulse sequencer parameters (0-8 range for each)
	int32_t steps_ = 4;    // Column 0: Sequence length (1-16 steps)
	int32_t pulses_ = 4;   // Column 1: Number of pulses ÷ steps
	int32_t division_ = 4; // Column 2: Musical divisions (half speed, etc.)
	int32_t repeats_ = 4;  // Column 3: Probability of note repetition
	int32_t voicing_ = 4;  // Column 4: Preset musical voicings/intervals
	int32_t range_ = 4;    // Column 5: Note range (1 semitone to 5 octaves)
	int32_t groove_ = 4;   // Column 6: Velocity patterns

	// Additional parameters for columns 7-15
	int32_t scale_ = 4;       // Column 7: Scale type
	int32_t chord_ = 4;       // Column 8: Chord progression
	int32_t swing_ = 4;       // Column 9: Swing amount
	int32_t velocity_ = 4;    // Column 10: Base velocity
	int32_t octave_ = 4;      // Column 11: Octave offset
	int32_t transpose_ = 4;   // Column 12: Transpose amount
	int32_t probability_ = 4; // Column 13: Note probability
	int32_t length_ = 4;      // Column 14: Note length
	int32_t accent_ = 4;      // Column 15: Accent probability

	// UI state
	bool isGenerating_ = false;
	int32_t selectedParameter_ = 0;
	int32_t viewOffset_ = 0; // Added: controls which part of the -8 to +8 range is visible

	void updateDisplay();
	void renderParameterColumn(int32_t column, int32_t value);
	void renderSlider(int32_t x, int32_t y, int32_t value);
	void generateRandomPattern();
	void generateScalePattern();
	void generateChordPattern();
	void generateRhythmicPattern();
	int32_t getRandomNoteInScale(int32_t octave);
	int32_t getRandomVelocity();
	bool shouldPlaceNote();
	void showParameterValuePopup(int32_t parameterType, int32_t value);
};

extern PulseSeqView pulseSeqView;

} // namespace deluge::gui::views
