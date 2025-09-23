#include "gui/views/generative_mode_view.h"
#include "definitions_cxx.hpp"
#include "gui/colour/rgb.h"
#include "gui/views/automation_view.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "hid/led/pad_leds.h"
#include "model/clip/clip_minder.h"
#include "model/clip/instrument_clip.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include <algorithm>
#include <random>

namespace deluge::gui::views {

GenerativeModeView generativeModeView{};

// Static variable definition
bool GenerativeModeView::cameFromGenerativeView = false;

void GenerativeModeView::openUI(InstrumentClip* clip) {
	currentClip_ = clip;
	currentInstrument_ = (Instrument*)clip->output;
	selectedParameter_ = 0; // Start with column 0 selected (red line at column 0)
	viewOffset_ = 0;        // Start with red line in center
	isGenerating_ = false;

	// Initialize with reasonable defaults (all at center/home = 0)
	// These will be overridden by actual clip settings if available
	steps_ = 0;       // Center
	pulses_ = 0;      // Center
	division_ = 0;    // Center
	repeats_ = 0;     // Center
	voicing_ = 0;     // Center
	range_ = 0;       // Center
	groove_ = 0;      // Center
	scale_ = 0;       // Center
	chord_ = 0;       // Center
	swing_ = 0;       // Center
	velocity_ = 0;    // Center
	octave_ = 0;      // Center
	transpose_ = 0;   // Center
	probability_ = 0; // Center
	length_ = 0;      // Center
	accent_ = 0;      // Center

	// TODO: Load actual parameter values from the clip
	// For now, we'll start with center values and let the user adjust

	// Tell the UI system that this view needs rendering
	uiNeedsRendering(this);
}

void GenerativeModeView::closeUI() {
	currentClip_ = nullptr;
	currentInstrument_ = nullptr;
}

bool GenerativeModeView::opened() {
	focusRegained();
	return true;
}

void GenerativeModeView::focusRegained() {
	// Force immediate pad LED rendering when view gains focus
	// This simulates what happens when a pad is pressed - calculate parameter values
	// and render the pads with the current parameter values

	// Only initialize parameter values if they haven't been set yet
	// Parameters start at 4 (center), so we only reset if they're still at default
	if (currentClip_) {
		// Check if any parameter has been modified from its default value
		bool hasBeenModified = false;
		for (int32_t x = 0; x < 16; x++) {
			if (getParameterValue(x) != 4) {
				hasBeenModified = true;
				break;
			}
		}

		// Only initialize to 0 if no parameters have been modified yet
		if (!hasBeenModified) {
			for (int32_t x = 0; x < 16; x++) {
				setParameterValue(x, 0);
			}
		}
	}

	// Trigger UI rendering to ensure pads are displayed
	uiNeedsRendering(this);
}

void GenerativeModeView::render() {
	if (display->haveOLED()) {
		deluge::hid::display::oled_canvas::Canvas& canvas = hid::display::OLED::main;
		renderOLED(canvas);
	}
	else {
		render7Seg();
	}

	// Render pad LEDs
	renderMainPads(0xFFFFFFFF, PadLEDs::image, PadLEDs::occupancyMask, true);
}

void GenerativeModeView::renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) {
	canvas.clear();

	// Top row: "Generative" (small text, like "Synth" in synth clips)
#if OLED_MAIN_HEIGHT_PIXELS == 64
	int32_t yPos = OLED_MAIN_TOPMOST_PIXEL + 12;
#else
	int32_t yPos = OLED_MAIN_TOPMOST_PIXEL + 3;
#endif
	canvas.drawStringCentred("Generative", yPos, kTextSpacingX, kTextSpacingY);

	// Center row: Parameter name (large text, like synth preset name)
	const char* parameterNames[] = {
	    "Steps",       // Column 0
	    "Pulses",      // Column 1
	    "Division",    // Column 2
	    "Repeats",     // Column 3
	    "Voicing",     // Column 4
	    "Range",       // Column 5
	    "Groove",      // Column 6
	    "Scale",       // Column 7
	    "Chord",       // Column 8
	    "Swing",       // Column 9
	    "Velocity",    // Column 10
	    "Octave",      // Column 11
	    "Transpose",   // Column 12
	    "Probability", // Column 13
	    "Length",      // Column 14
	    "Accent"       // Column 15
	};

	const char* currentParamName = parameterNames[selectedParameter_];

#if OLED_MAIN_HEIGHT_PIXELS == 64
	yPos = OLED_MAIN_TOPMOST_PIXEL + 30;
#else
	yPos = OLED_MAIN_TOPMOST_PIXEL + 17;
#endif

	int32_t stringLengthPixels = canvas.getStringWidthInPixels(currentParamName, kTextTitleSizeY);

	if (stringLengthPixels <= OLED_MAIN_WIDTH_PIXELS) {
		canvas.drawStringCentred(currentParamName, yPos, kTextTitleSpacingX, kTextTitleSizeY);
	}
	else {
		canvas.drawString(currentParamName, 0, yPos, kTextTitleSpacingX, kTextTitleSizeY);
		deluge::hid::display::OLED::setupSideScroller(0, currentParamName, 0, OLED_MAIN_WIDTH_PIXELS, yPos,
		                                              yPos + kTextTitleSizeY, kTextTitleSpacingX, kTextTitleSizeY,
		                                              false);
	}

	// Bottom row: Section info (like in synth clips)
	if (currentClip_) {
		DEF_STACK_STRING_BUF(info, currentClip_->name.getLength() + 10);
		if (currentClip_->name.isEmpty()) {
			info.append("Section ");
			info.appendInt(currentClip_->section + 1);
		}
		else {
			info.appendInt(currentClip_->section + 1);
			info.append(": ");
			info.append(currentClip_->name.get());
		}
		yPos = yPos + 14;
		canvas.drawStringCentred(info.data(), yPos, kTextSpacingX, kTextSpacingY);
		deluge::hid::display::OLED::setupSideScroller(1, info.data(), 0, OLED_MAIN_WIDTH_PIXELS, yPos,
		                                              yPos + kTextSpacingY, kTextSpacingX, kTextSpacingY, false);
	}
}

void GenerativeModeView::render7Seg() {
	display->setText("GEN");
}

bool GenerativeModeView::renderMainPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                                        uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
                                        bool drawUndefinedArea) {
	if (!image) {
		return true;
	}

	// Define blue-to-purple gradient colors (8 colors, symmetric around red center)
	RGB colors[8] = {
	    RGB{0, 0, 255},   // Blue (furthest from center)
	    RGB{32, 0, 223},  // Blue-purple
	    RGB{64, 0, 191},  // Blue-purple
	    RGB{96, 0, 159},  // Purple-blue
	    RGB{128, 0, 127}, // Purple-blue
	    RGB{160, 0, 95},  // Purple-red
	    RGB{191, 0, 64},  // Purple-red
	    RGB{255, 0, 0}    // Red (center/home)
	};

	// Render each column as a slider
	for (int32_t x = 0; x < 16; x++) {
		int32_t paramValue = getParameterValue(x);
		bool isSelected = (x == selectedParameter_);

		// Clear the column first
		for (int32_t y = 0; y < 8; y++) {
			if (y < kDisplayHeight && x < kDisplayWidth) {
				image[y][x] = RGB{0, 0, 0}; // Black (off)
				occupancyMask[y][x] = 0;    // No occupancy
			}
		}

		// Calculate where the red line should appear based on viewOffset
		// viewOffset: -3 to +3, red line position: 0 to 6
		int32_t redLineY = 3 - viewOffset_; // 3 is center, viewOffset shifts it

		// Draw the red center line at the calculated position
		if (redLineY >= 0 && redLineY < 8 && x < kDisplayWidth) {
			if (isSelected) {
				// Selected parameter: white highlight on red line
				image[redLineY][x] = RGB{255, 255, 255}; // White highlight
				occupancyMask[redLineY][x] = 64;         // Full occupancy
			}
			else {
				// Non-selected: normal red center line
				image[redLineY][x] = RGB{255, 0, 0}; // Red center line
				occupancyMask[redLineY][x] = 64;     // Full occupancy
			}
		}

		// Light up pads based on parameter value relative to current red line position
		// The fader should move with the red line as you scroll
		// paramValue = redLineY - y, so positive values are ABOVE red line, negative values are BELOW
		if (paramValue > 0) {
			// Positive values: light up pads above red line (y < redLineY)
			int32_t endY = std::max(static_cast<int32_t>(0), redLineY - paramValue);
			for (int32_t y = redLineY - 1; y >= endY; y--) {
				if (y >= 0 && y < kDisplayHeight && x < kDisplayWidth) {
					// Use symmetric gradient for positive values (reverse order from center)
					// Map distance from red line to color index (6-0, reversed)
					int32_t distance = redLineY - y - 1;
					int32_t colorIndex = 6 - std::min(static_cast<int32_t>(6), distance);
					if (colorIndex >= 0 && colorIndex < 8) {
						image[y][x] = colors[colorIndex];
						occupancyMask[y][x] = 64; // Full occupancy
					}
				}
			}
		}
		else if (paramValue < 0) {
			// Negative values: light up pads below red line (y > redLineY)
			int32_t endY = std::min(static_cast<int32_t>(7), redLineY + (-paramValue));
			for (int32_t y = redLineY + 1; y <= endY; y++) {
				if (y >= 0 && y < kDisplayHeight && x < kDisplayWidth) {
					// Use symmetric gradient for negative values (same order as positive)
					// Map distance from red line to color index (0-6, same as positive)
					int32_t distance = y - redLineY - 1;
					int32_t colorIndex = std::min(static_cast<int32_t>(6), distance);
					if (colorIndex >= 0 && colorIndex < 8) {
						image[y][x] = colors[colorIndex];
						occupancyMask[y][x] = 64; // Full occupancy
					}
				}
			}
		}
		// If paramValue == 0, only the red center line is lit
	}

	return true;
}

ActionResult GenerativeModeView::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
	using namespace deluge::hid::button;

	if (b == BACK && on) {
		backButtonAction();
		return ActionResult::DEALT_WITH;
	}

	// Song view button - same functionality as instrument clip view
	else if (b == SESSION_VIEW) {
		if (on) {
			if (inCardRoutine) {
				return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
			}
			// Now we can call this directly since we inherit from ClipMinder
			transitionToArrangerOrSession();
		}
	}

	// Clip view button - same functionality as instrument clip view
	else if (b == CLIP_VIEW) {
		if (on) {
			if (inCardRoutine) {
				return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
			}
			// Set flag to indicate we came from generative view
			cameFromGenerativeView = true;
			changeRootUI(&automationView);
		}
	}

	// Allow exiting via Synth/MIDI buttons
	else if ((b == SYNTH || b == MIDI) && on) {
		closeUI();
		// Return to instrument clip view
		changeRootUI(&instrumentClipView);
		return ActionResult::DEALT_WITH;
	}

	return ActionResult::DEALT_WITH;
}

ActionResult GenerativeModeView::padAction(int32_t x, int32_t y, int32_t velocity) {
	if (velocity > 0) {
		// Each column represents a different parameter
		// Y position represents the value based on current view offset
		if (x >= 0 && x < 16) {
			// Update selected parameter to the column we're pressing
			selectedParameter_ = x;

			// Calculate where the red line currently is
			int32_t redLineY = 3 - viewOffset_;

			// FINAL CORRECT calculation: value = redLineY - y
			// If red line is at y=0 and we tap y=7, we get 0-7 = -7
			// If red line is at y=7 and we tap y=1, we get 7-1 = +6
			int32_t paramValue = redLineY - y;

			// Clamp to valid range (-7 to +7)
			paramValue = std::max(static_cast<int32_t>(-7), std::min(static_cast<int32_t>(7), paramValue));

			setParameterValue(x, paramValue);
			uiNeedsRendering(this);

			// Force OLED display update
			if (display->haveOLED()) {
				deluge::hid::display::oled_canvas::Canvas& canvas = deluge::hid::display::OLED::main;
				renderOLED(canvas);
				deluge::hid::display::OLED::markChanged();
			}

			// Show parameter value popup
			showParameterValuePopup();
		}
	}

	return ActionResult::DEALT_WITH;
}

ActionResult GenerativeModeView::verticalEncoderAction(int32_t offset, bool inCardRoutine) {
	// Scroll the view up/down to show different parts of the -8 to +8 range
	// viewOffset_ controls which part of the range is visible
	viewOffset_ += offset;

	// Clamp viewOffset to keep the red line visible
	// viewOffset: -4 to +3 (red line constrained to rows 0-7, covers full range)
	viewOffset_ = std::max(static_cast<int32_t>(-4), std::min(static_cast<int32_t>(3), viewOffset_));

	uiNeedsRendering(this);
	return ActionResult::DEALT_WITH;
}

ActionResult GenerativeModeView::horizontalEncoderAction(int32_t offset) {
	// Navigate between parameters (columns)
	selectedParameter_ += offset;
	if (selectedParameter_ < 0)
		selectedParameter_ = 15;
	if (selectedParameter_ > 15)
		selectedParameter_ = 0;

	uiNeedsRendering(this);

	// Force OLED display update
	if (display->haveOLED()) {
		deluge::hid::display::oled_canvas::Canvas& canvas = deluge::hid::display::OLED::main;
		renderOLED(canvas);
		deluge::hid::display::OLED::markChanged();
	}

	// Show parameter value popup
	showParameterValuePopup();

	return ActionResult::DEALT_WITH;
}

void GenerativeModeView::modEncoderAction(int32_t whichModEncoder, int32_t offset) {
	// Gold knobs control actual synth parameters or MIDI CC values, just like in normal clip view
	// Delegate to the base UI modEncoderAction which handles synth/MIDI parameters properly
	UI::modEncoderAction(whichModEncoder, offset);
}

void GenerativeModeView::selectEncoderAction(int32_t offset) {
	// Could use select encoder for parameter adjustment
}

void GenerativeModeView::backButtonAction() {
	closeUI();
	changeRootUI(&instrumentClipView);
}

void GenerativeModeView::setParameterValue(int32_t column, int32_t value) {
	if (!currentClip_)
		return;

	// Clamp value to -7 to +7 range (full range)
	value = std::max(-7, std::min(7, (int)value));
	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;

	switch (column) {
	case 0:
		instrumentClip->generativeSteps_ = value;
		break;
	case 1:
		instrumentClip->generativePulses_ = value;
		break;
	case 2:
		instrumentClip->generativeDivision_ = value;
		break;
	case 3:
		instrumentClip->generativeRepeats_ = value;
		break;
	case 4:
		instrumentClip->generativeVoicing_ = value;
		break;
	case 5:
		instrumentClip->generativeRange_ = value;
		break;
	case 6:
		instrumentClip->generativeGroove_ = value;
		break;
	case 7:
		instrumentClip->generativeScale_ = value;
		break;
	case 8:
		instrumentClip->generativeChord_ = value;
		break;
	case 9:
		instrumentClip->generativeSwing_ = value;
		break;
	case 10:
		instrumentClip->generativeVelocity_ = value;
		break;
	case 11:
		instrumentClip->generativeOctave_ = value;
		break;
	case 12:
		instrumentClip->generativeTranspose_ = value;
		break;
	case 13:
		instrumentClip->generativeProbability_ = value;
		break;
	case 14:
		instrumentClip->generativeLength_ = value;
		break;
	case 15:
		instrumentClip->generativeAccent_ = value;
		break;
	}
}

int32_t GenerativeModeView::getParameterValue(int32_t column) {
	if (!currentClip_)
		return 0;

	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;
	switch (column) {
	case 0:
		return instrumentClip->generativeSteps_;
	case 1:
		return instrumentClip->generativePulses_;
	case 2:
		return instrumentClip->generativeDivision_;
	case 3:
		return instrumentClip->generativeRepeats_;
	case 4:
		return instrumentClip->generativeVoicing_;
	case 5:
		return instrumentClip->generativeRange_;
	case 6:
		return instrumentClip->generativeGroove_;
	case 7:
		return instrumentClip->generativeScale_;
	case 8:
		return instrumentClip->generativeChord_;
	case 9:
		return instrumentClip->generativeSwing_;
	case 10:
		return instrumentClip->generativeVelocity_;
	case 11:
		return instrumentClip->generativeOctave_;
	case 12:
		return instrumentClip->generativeTranspose_;
	case 13:
		return instrumentClip->generativeProbability_;
	case 14:
		return instrumentClip->generativeLength_;
	case 15:
		return instrumentClip->generativeAccent_;
	default:
		return 0;
	}
}

void GenerativeModeView::generatePattern() {
	// Default to random pattern
	generateRandomPattern();
}

void GenerativeModeView::updateDisplay() {
	// Request a redraw by calling render
	uiNeedsRendering(this);
}

void GenerativeModeView::renderParameterColumn(int32_t column, int32_t value) {
	// Simplified for now
}

void GenerativeModeView::renderSlider(int32_t x, int32_t y, int32_t value) {
	// Simplified for now
}

void GenerativeModeView::generateRandomPattern() {
	if (!currentClip_)
		return;

	display->displayPopup((char*)"PATTERN GENERATED");
}

void GenerativeModeView::generateScalePattern() {
	display->displayPopup((char*)"SCALE PATTERN");
}

void GenerativeModeView::generateChordPattern() {
	display->displayPopup((char*)"CHORD PATTERN");
}

void GenerativeModeView::generateRhythmicPattern() {
	display->displayPopup((char*)"RHYTHM PATTERN");
}

int32_t GenerativeModeView::getRandomNoteInScale(int32_t octave) {
	return 60; // Middle C for now
}

int32_t GenerativeModeView::getRandomVelocity() {
	return 100;
}

bool GenerativeModeView::shouldPlaceNote() {
	return true;
}

void GenerativeModeView::showParameterValuePopup() {
	// Get parameter names
	const char* parameterNames[] = {
	    "Steps",       // Column 0
	    "Pulses",      // Column 1
	    "Division",    // Column 2
	    "Repeats",     // Column 3
	    "Voicing",     // Column 4
	    "Range",       // Column 5
	    "Groove",      // Column 6
	    "Scale",       // Column 7
	    "Chord",       // Column 8
	    "Swing",       // Column 9
	    "Velocity",    // Column 10
	    "Octave",      // Column 11
	    "Transpose",   // Column 12
	    "Probability", // Column 13
	    "Length",      // Column 14
	    "Accent"       // Column 15
	};

	// Get current parameter value
	int32_t currentValue = getParameterValue(selectedParameter_);
	const char* paramName = parameterNames[selectedParameter_];

	// Invert the value for OLED display (since pads start at 0,0 top-left)
	int32_t displayValue = -currentValue;

	// Create popup text with parameter name and value
	char popupText[20];
	if (displayValue >= 0) {
		snprintf(popupText, sizeof(popupText), "%s +%d", paramName, displayValue);
	}
	else {
		snprintf(popupText, sizeof(popupText), "%s %d", paramName, displayValue);
	}

	// Show popup for 3 flashes (same as gold knob popups)
	display->displayPopup(popupText, 3, false, 255, 1, PopupType::NOTIFICATION);
}

} // namespace deluge::gui::views
