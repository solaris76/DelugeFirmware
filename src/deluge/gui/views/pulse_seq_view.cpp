#include "gui/views/pulse_seq_view.h"
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

PulseSeqView pulseSeqView{};

// Static variable definition
bool PulseSeqView::cameFromPulseSeqView = false;

void PulseSeqView::openUI(InstrumentClip* clip) {
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

void PulseSeqView::closeUI() {
	currentClip_ = nullptr;
	currentInstrument_ = nullptr;
}

bool PulseSeqView::opened() {
	focusRegained();
	return true;
}

void PulseSeqView::focusRegained() {
	// Force immediate pad LED rendering when view gains focus
	// This simulates what happens when a pad is pressed - calculate parameter values
	// and render the pads with the current parameter values

	// Initialize sequencer stage gate types to 0 (Off) if they haven't been set yet
	// Gate types start at 4 (default), so we only reset if they're still at default
	if (currentClip_) {
		// Check if any sequencer stage gate type has been modified from its default value
		bool hasBeenModified = false;
		for (int32_t x = 0; x < 8; x++) {
			if (getParameterValue(x) != 4) {
				hasBeenModified = true;
				break;
			}
		}

		// Only initialize to 0 (Off) if no sequencer stage gate types have been modified yet
		if (!hasBeenModified) {
			for (int32_t x = 0; x < 8; x++) {
				setParameterValue(x, 0); // 0 = Off gate type for this sequencer stage
			}
		}
	}

	// Trigger UI rendering to ensure pads are displayed
	uiNeedsRendering(this);
}

void PulseSeqView::render() {
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

void PulseSeqView::renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) {
	canvas.clear();

	// Top row: "Pulse Seq" (small text, like "Synth" in synth clips)
#if OLED_MAIN_HEIGHT_PIXELS == 64
	int32_t yPos = OLED_MAIN_TOPMOST_PIXEL + 12;
#else
	int32_t yPos = OLED_MAIN_TOPMOST_PIXEL + 3;
#endif
	canvas.drawStringCentred("Pulse Seq", yPos, kTextSpacingX, kTextSpacingY);

	// Center row: Stage names (each column is a sequencer stage)
	const char* stageNames[] = {
	    "Stage 1", // Column 0
	    "Stage 2", // Column 1
	    "Stage 3", // Column 2
	    "Stage 4", // Column 3
	    "Stage 5", // Column 4
	    "Stage 6", // Column 5
	    "Stage 7", // Column 6
	    "Stage 8"  // Column 7
	};

	// Gate type names for display
	const char* gateTypeNames[] = {"Off", "Single", "Multiple", "Hold"};

	const char* currentStageName = stageNames[selectedParameter_];

#if OLED_MAIN_HEIGHT_PIXELS == 64
	yPos = OLED_MAIN_TOPMOST_PIXEL + 30;
#else
	yPos = OLED_MAIN_TOPMOST_PIXEL + 17;
#endif

	int32_t stringLengthPixels = canvas.getStringWidthInPixels(currentStageName, kTextTitleSizeY);

	if (stringLengthPixels <= OLED_MAIN_WIDTH_PIXELS) {
		canvas.drawStringCentred(currentStageName, yPos, kTextTitleSpacingX, kTextTitleSizeY);
	}
	else {
		canvas.drawString(currentStageName, 0, yPos, kTextTitleSpacingX, kTextTitleSizeY);
		deluge::hid::display::OLED::setupSideScroller(0, currentStageName, 0, OLED_MAIN_WIDTH_PIXELS, yPos,
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

void PulseSeqView::render7Seg() {
	display->setText("GEN");
}

bool PulseSeqView::renderMainPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                                  uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], bool drawUndefinedArea) {
	if (!image) {
		return true;
	}

	// Gate type colors
	RGB gateColors[4] = {
	    RGB{32, 32, 32}, // Off: Dim gray
	    RGB{0, 0, 255},  // Single: Blue
	    RGB{0, 255, 0},  // Multiple: Green
	    RGB{128, 0, 255} // Hold: Purple
	};

	// Pitch colors (above red line) - avoid gate type colors
	RGB pitchColors[8] = {
	    RGB{255, 100, 0}, // +1: Orange
	    RGB{255, 150, 0}, // +2: Light orange
	    RGB{255, 200, 0}, // +3: Yellow-orange
	    RGB{255, 255, 0}, // +4: Yellow
	    RGB{200, 255, 0}, // +5: Yellow-green
	    RGB{150, 255, 0}, // +6: Light green
	    RGB{100, 255, 0}, // +7: Green
	    RGB{50, 255, 0}   // +8: Dark green
	};

	// Pulse count colors (below red line) - avoid gate type colors
	RGB pulseColors[8] = {
	    RGB{255, 0, 100},   // 1 pulse: Pink-red
	    RGB{255, 0, 150},   // 2 pulses: Pink
	    RGB{255, 0, 200},   // 3 pulses: Light pink
	    RGB{255, 100, 255}, // 4 pulses: Magenta
	    RGB{200, 100, 255}, // 5 pulses: Light purple
	    RGB{150, 100, 255}, // 6 pulses: Purple-blue
	    RGB{100, 100, 255}, // 7 pulses: Blue-purple
	    RGB{50, 50, 255}    // 8 pulses: Dark blue
	};

	// Render each column as a sequencer stage
	for (int32_t x = 0; x < 8; x++) {
		int32_t gateType = getParameterValue(x);    // 0-3 for gate types (Off, Single, Multiple, Hold)
		int32_t pitch = getPitchValue(x);           // 0-8 for pitch (+1 to +8 semitones)
		int32_t pulseCount = getPulseCountValue(x); // 0-8 for pulse count (1 to 8 pulses)
		bool isSelected = (x == selectedParameter_);

		// Clear the column first
		for (int32_t y = 0; y < 8; y++) {
			if (y < kDisplayHeight && x < kDisplayWidth) {
				image[y][x] = RGB{0, 0, 0}; // Black (off)
				occupancyMask[y][x] = 0;    // No occupancy
			}
		}

		// Calculate where the red line should appear based on viewOffset
		int32_t redLineY = 3 - viewOffset_; // 3 is center, viewOffset shifts it

		// Draw the gate type color on the red line
		if (redLineY >= 0 && redLineY < 8 && x < kDisplayWidth) {
			RGB gateColor = gateColors[gateType];

			if (isSelected) {
				// Selected stage: flash the gate color subtly
				// Use a brighter version for flashing effect
				image[redLineY][x] =
				    RGB{static_cast<uint8_t>(gateColor.r * 1.5), static_cast<uint8_t>(gateColor.g * 1.5),
				        static_cast<uint8_t>(gateColor.b * 1.5)};
			}
			else {
				// Non-selected: normal gate color
				image[redLineY][x] = gateColor;
			}
			occupancyMask[redLineY][x] = 64; // Full occupancy
		}

		// Draw pitch pads above red line (if pitch > 0)
		if (pitch > 0) {
			for (int32_t i = 0; i < pitch && i < 8; i++) {
				int32_t y = redLineY - 1 - i; // Go up from red line
				if (y >= 0 && y < kDisplayHeight && x < kDisplayWidth) {
					image[y][x] = pitchColors[i];
					occupancyMask[y][x] = 64;
				}
			}
		}

		// Draw pulse count pads below red line (if pulseCount > 0)
		if (pulseCount > 0) {
			for (int32_t i = 0; i < pulseCount && i < 8; i++) {
				int32_t y = redLineY + 1 + i; // Go down from red line
				if (y >= 0 && y < kDisplayHeight && x < kDisplayWidth) {
					image[y][x] = pulseColors[i];
					occupancyMask[y][x] = 64;
				}
			}
		}
	}

	return true;
}

ActionResult PulseSeqView::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
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
			cameFromPulseSeqView = true;
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

ActionResult PulseSeqView::padAction(int32_t x, int32_t y, int32_t velocity) {
	if (velocity > 0) {
		// Each column represents a sequencer stage
		if (x >= 0 && x < 16) {
			// Update selected stage to the column we're pressing
			selectedParameter_ = x;

			// Calculate where the red line currently is
			int32_t redLineY = 3 - viewOffset_;

			if (y == redLineY) {
				// Tapping the red line cycles through gate types for this sequencer stage
				int32_t currentGateType = getParameterValue(x);
				int32_t nextGateType = (currentGateType + 1) % 4; // Cycle 0->1->2->3->0
				setParameterValue(x, nextGateType);
				showParameterValuePopup(0, nextGateType); // 0 = Gate type
			}
			else if (y < redLineY) {
				// Tapping above red line sets pitch for this sequencer stage (+1 to +8)
				int32_t pitch = redLineY - y; // Distance from red line
				pitch = std::max(static_cast<int32_t>(1), std::min(static_cast<int32_t>(8), pitch)); // Clamp to 1-8
				setPitchValue(x, pitch);
				showParameterValuePopup(1, pitch); // 1 = Pitch
			}
			else if (y > redLineY) {
				// Tapping below red line sets pulse count for this sequencer stage (1 to 8)
				int32_t pulseCount = y - redLineY; // Distance from red line
				pulseCount =
				    std::max(static_cast<int32_t>(1), std::min(static_cast<int32_t>(8), pulseCount)); // Clamp to 1-8
				setPulseCountValue(x, pulseCount);
				showParameterValuePopup(2, pulseCount); // 2 = Pulse count
			}

			uiNeedsRendering(this);

			// Force OLED display update
			if (display->haveOLED()) {
				deluge::hid::display::oled_canvas::Canvas& canvas = deluge::hid::display::OLED::main;
				renderOLED(canvas);
				deluge::hid::display::OLED::markChanged();
			}
		}
	}

	return ActionResult::DEALT_WITH;
}

ActionResult PulseSeqView::verticalEncoderAction(int32_t offset, bool inCardRoutine) {
	// Scroll the view up/down to show different parts of the -8 to +8 range
	// viewOffset_ controls which part of the range is visible
	viewOffset_ += offset;

	// Clamp viewOffset to keep the red line visible
	// viewOffset: -4 to +3 (red line constrained to rows 0-7, covers full range)
	viewOffset_ = std::max(static_cast<int32_t>(-4), std::min(static_cast<int32_t>(3), viewOffset_));

	uiNeedsRendering(this);
	return ActionResult::DEALT_WITH;
}

ActionResult PulseSeqView::horizontalEncoderAction(int32_t offset) {
	// Navigate between parameters (columns)
	selectedParameter_ += offset;
	if (selectedParameter_ < 0)
		selectedParameter_ = 7;
	if (selectedParameter_ > 7)
		selectedParameter_ = 0;

	uiNeedsRendering(this);

	// Force OLED display update
	if (display->haveOLED()) {
		deluge::hid::display::oled_canvas::Canvas& canvas = deluge::hid::display::OLED::main;
		renderOLED(canvas);
		deluge::hid::display::OLED::markChanged();
	}

	return ActionResult::DEALT_WITH;
}

void PulseSeqView::modEncoderAction(int32_t whichModEncoder, int32_t offset) {
	// Gold knobs control actual synth parameters or MIDI CC values, just like in normal clip view
	// Delegate to the base UI modEncoderAction which handles synth/MIDI parameters properly
	UI::modEncoderAction(whichModEncoder, offset);
}

void PulseSeqView::selectEncoderAction(int32_t offset) {
	// Could use select encoder for parameter adjustment
}

void PulseSeqView::backButtonAction() {
	closeUI();
	changeRootUI(&instrumentClipView);
}

void PulseSeqView::setParameterValue(int32_t column, int32_t value) {
	if (!currentClip_)
		return;

	// Clamp gate type to 0-3 range (Off, Single, Multiple, Hold)
	value = std::max(0, std::min(3, (int)value));
	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;

	// Each column is a sequencer stage - gate type only
	switch (column) {
	case 0:
		instrumentClip->gateType0_ = value; // Stage 0 gate type
		break;
	case 1:
		instrumentClip->gateType1_ = value; // Stage 1 gate type
		break;
	case 2:
		instrumentClip->gateType2_ = value; // Stage 2 gate type
		break;
	case 3:
		instrumentClip->gateType3_ = value; // Stage 3 gate type
		break;
	case 4:
		instrumentClip->gateType4_ = value; // Stage 4 gate type
		break;
	case 5:
		instrumentClip->gateType5_ = value; // Stage 5 gate type
		break;
	case 6:
		instrumentClip->gateType6_ = value; // Stage 6 gate type
		break;
	case 7:
		instrumentClip->gateType7_ = value; // Stage 7 gate type
		break;
	}
}

void PulseSeqView::setPitchValue(int32_t column, int32_t value) {
	if (!currentClip_)
		return;

	// Clamp pitch to 0-8 range (0 = no pitch, 1-8 = +1 to +8 semitones)
	value = std::max(0, std::min(8, (int)value));
	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;

	// Each column is a sequencer stage - pitch only
	switch (column) {
	case 0:
		instrumentClip->pitch0_ = value; // Stage 0 pitch
		break;
	case 1:
		instrumentClip->pitch1_ = value; // Stage 1 pitch
		break;
	case 2:
		instrumentClip->pitch2_ = value; // Stage 2 pitch
		break;
	case 3:
		instrumentClip->pitch3_ = value; // Stage 3 pitch
		break;
	case 4:
		instrumentClip->pitch4_ = value; // Stage 4 pitch
		break;
	case 5:
		instrumentClip->pitch5_ = value; // Stage 5 pitch
		break;
	case 6:
		instrumentClip->pitch6_ = value; // Stage 6 pitch
		break;
	case 7:
		instrumentClip->pitch7_ = value; // Stage 7 pitch
		break;
	}
}

void PulseSeqView::setPulseCountValue(int32_t column, int32_t value) {
	if (!currentClip_)
		return;

	// Clamp pulse count to 0-8 range (0 = no pulses, 1-8 = 1 to 8 pulses)
	value = std::max(0, std::min(8, (int)value));
	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;

	// Each column is a sequencer stage - pulse count only
	switch (column) {
	case 0:
		instrumentClip->pulse0_ = value; // Stage 0 pulse count
		break;
	case 1:
		instrumentClip->pulse1_ = value; // Stage 1 pulse count
		break;
	case 2:
		instrumentClip->pulse2_ = value; // Stage 2 pulse count
		break;
	case 3:
		instrumentClip->pulse3_ = value; // Stage 3 pulse count
		break;
	case 4:
		instrumentClip->pulse4_ = value; // Stage 4 pulse count
		break;
	case 5:
		instrumentClip->pulse5_ = value; // Stage 5 pulse count
		break;
	case 6:
		instrumentClip->pulse6_ = value; // Stage 6 pulse count
		break;
	case 7:
		instrumentClip->pulse7_ = value; // Stage 7 pulse count
		break;
	}
}

int32_t PulseSeqView::getParameterValue(int32_t column) {
	if (!currentClip_)
		return 0;

	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;
	// Each column is a sequencer stage - gate type only
	switch (column) {
	case 0:
		return instrumentClip->gateType0_; // Stage 0 gate type
	case 1:
		return instrumentClip->gateType1_; // Stage 1 gate type
	case 2:
		return instrumentClip->gateType2_; // Stage 2 gate type
	case 3:
		return instrumentClip->gateType3_; // Stage 3 gate type
	case 4:
		return instrumentClip->gateType4_; // Stage 4 gate type
	case 5:
		return instrumentClip->gateType5_; // Stage 5 gate type
	case 6:
		return instrumentClip->gateType6_; // Stage 6 gate type
	case 7:
		return instrumentClip->gateType7_; // Stage 7 gate type
	default:
		return 0;
	}
}

int32_t PulseSeqView::getPitchValue(int32_t column) {
	if (!currentClip_)
		return 0;

	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;
	// Each column is a sequencer stage - pitch only
	switch (column) {
	case 0:
		return instrumentClip->pitch0_; // Stage 0 pitch
	case 1:
		return instrumentClip->pitch1_; // Stage 1 pitch
	case 2:
		return instrumentClip->pitch2_; // Stage 2 pitch
	case 3:
		return instrumentClip->pitch3_; // Stage 3 pitch
	case 4:
		return instrumentClip->pitch4_; // Stage 4 pitch
	case 5:
		return instrumentClip->pitch5_; // Stage 5 pitch
	case 6:
		return instrumentClip->pitch6_; // Stage 6 pitch
	case 7:
		return instrumentClip->pitch7_; // Stage 7 pitch
	default:
		return 0;
	}
}

int32_t PulseSeqView::getPulseCountValue(int32_t column) {
	if (!currentClip_)
		return 0;

	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;
	// Each column is a sequencer stage - pulse count only
	switch (column) {
	case 0:
		return instrumentClip->pulse0_; // Stage 0 pulse count
	case 1:
		return instrumentClip->pulse1_; // Stage 1 pulse count
	case 2:
		return instrumentClip->pulse2_; // Stage 2 pulse count
	case 3:
		return instrumentClip->pulse3_; // Stage 3 pulse count
	case 4:
		return instrumentClip->pulse4_; // Stage 4 pulse count
	case 5:
		return instrumentClip->pulse5_; // Stage 5 pulse count
	case 6:
		return instrumentClip->pulse6_; // Stage 6 pulse count
	case 7:
		return instrumentClip->pulse7_; // Stage 7 pulse count
	default:
		return 0;
	}
}

void PulseSeqView::generatePattern() {
	// Default to random pattern
	generateRandomPattern();
}

void PulseSeqView::updateDisplay() {
	// Request a redraw by calling render
	uiNeedsRendering(this);
}

void PulseSeqView::renderParameterColumn(int32_t column, int32_t value) {
	// Simplified for now
}

void PulseSeqView::renderSlider(int32_t x, int32_t y, int32_t value) {
	// Simplified for now
}

void PulseSeqView::generateRandomPattern() {
	if (!currentClip_)
		return;

	display->displayPopup((char*)"PATTERN GENERATED");
}

void PulseSeqView::generateScalePattern() {
	display->displayPopup((char*)"SCALE PATTERN");
}

void PulseSeqView::generateChordPattern() {
	display->displayPopup((char*)"CHORD PATTERN");
}

void PulseSeqView::generateRhythmicPattern() {
	display->displayPopup((char*)"RHYTHM PATTERN");
}

int32_t PulseSeqView::getRandomNoteInScale(int32_t octave) {
	return 60; // Middle C for now
}

int32_t PulseSeqView::getRandomVelocity() {
	return 100;
}

bool PulseSeqView::shouldPlaceNote() {
	return true;
}

void PulseSeqView::showParameterValuePopup(int32_t parameterType, int32_t value) {
	if (parameterType == 0) {
		// Gate type (0-3: Off, Single, Multiple, Hold)
		const char* gateTypeNames[] = {"Off", "Single", "Multiple", "Hold"};
		display->displayNotification("Gate", gateTypeNames[value]);
	}
	else if (parameterType == 1) {
		// Pitch (1-8: +1 to +8 semitones)
		char valueStr[10];
		snprintf(valueStr, sizeof(valueStr), "%d", value);
		display->displayNotification("Pitch", valueStr);
	}
	else if (parameterType == 2) {
		// Pulse count (1-8: 1 to 8 pulses)
		char valueStr[10];
		snprintf(valueStr, sizeof(valueStr), "%d", value);
		display->displayNotification("Pulse", valueStr);
	}
}

} // namespace deluge::gui::views
