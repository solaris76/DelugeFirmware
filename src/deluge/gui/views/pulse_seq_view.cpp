#include "gui/views/pulse_seq_view.h"
#include "definitions_cxx.hpp"
#include "gui/colour/rgb.h"
#include "gui/ui/ui.h"
#include "gui/views/automation_view.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "hid/led/pad_leds.h"
#include "io/debug/print.h"
#include "model/clip/clip_minder.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/melodic_instrument.h"
#include "model/model_stack.h"
#include "model/note/note_row.h"
#include "model/scale/note_set.h"
#include "model/scale/preset_scales.h"
#include "model/song/song.h"
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <random>

namespace deluge::gui::views {

// Global instance
PulseSeqView pulseSeqView{};

// Static variable definition
bool PulseSeqView::cameFromPulseSeqView = false;

void PulseSeqView::openUI(InstrumentClip* clip) {
	currentClip_ = clip;
	currentInstrument_ = (Instrument*)clip->output;
	selectedParameter_ = 0; // Start with column 0 selected (red line at column 0)
	viewOffset_ = 0;        // Start with gate line at GL=3 (default position)
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
	// Stop the Pulse Sequencer timing
	stopPulseSeq();

	currentClip_ = nullptr;
	currentInstrument_ = nullptr;
}

void PulseSeqView::notifyPlaybackBegun() {
	// Start the Pulse Sequencer when playback begins
	display->displayNotification("PulseSeq", "Started");
	Debug::println("Playback Begun!");
	startPulseSeq();
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
	    RGB{0, 0, 200},  // Single: Blue
	    RGB{0, 255, 0},  // Multiple: Green
	    RGB{128, 0, 255} // Hold: Purple
	};

	// Scale note navigation colors (above red line) - avoid gate type colors
	RGB scaleNoteColors[4] = {
	    RGB{255, 100, 0}, // Pitch down: Orange
	    RGB{255, 200, 0}, // Pitch up: Yellow-orange
	    RGB{255, 255, 0}, // Octave down: Yellow
	    RGB{200, 255, 0}  // Octave up: Yellow-green
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
		int32_t scaleNote = getScaleNoteValue(x);   // 0-11 for scale note degree
		int32_t octave = getOctaveValue(x);         // -3 to +3 for octave offset
		int32_t pulseCount = getPulseCountValue(x); // 0-8 for pulse count (1 to 8 pulses)
		bool isSelected = (x == selectedParameter_);

		// Clear the column first
		for (int32_t y = 0; y < 8; y++) {
			if (y < kDisplayHeight && x < kDisplayWidth) {
				image[y][x] = RGB{0, 0, 0}; // Black (off)
				occupancyMask[y][x] = 0;    // No occupancy
			}
		}

		// Calculate where the gate line should appear based on viewOffset
		int32_t gateLineY = 3 + viewOffset_; // 3 is center, viewOffset shifts it

		// Draw the gate type color on the gate line
		if (gateLineY >= 0 && gateLineY < 8 && x < kDisplayWidth) {
			RGB gateColor = gateColors[gateType];
			bool isCurrentStage =
			    (x == getCurrentStage() && getCurrentInstrumentClip() && getCurrentInstrumentClip()->pulseSeqIsActive_);

			if (isCurrentStage) {
				// Current stage: bright red highlight
				image[gateLineY][x] = RGB{255, 0, 0}; // Bright red
			}
			else if (isSelected) {
				// Selected stage: flash the gate color subtly
				// Use a brighter version for flashing effect
				image[gateLineY][x] =
				    RGB{static_cast<uint8_t>(gateColor.r * 1.5), static_cast<uint8_t>(gateColor.g * 1.5),
				        static_cast<uint8_t>(gateColor.b * 1.5)};
			}
			else {
				// Non-selected: normal gate color
				image[gateLineY][x] = gateColor;
			}
			occupancyMask[gateLineY][x] = 64; // Full occupancy
		}

		// Draw pulse count pads above gate line (if pulseCount > 0)
		if (pulseCount > 0) {
			for (int32_t i = 0; i < pulseCount && i < 8; i++) {
				int32_t y = gateLineY - 1 - i; // Go up from gate line
				if (y >= 0 && y < kDisplayHeight && x < kDisplayWidth) {
					image[y][x] = pulseColors[i];
					occupancyMask[y][x] = 64;
				}
			}
		}

		// Draw scale note navigation pads below gate line (4 pads)
		for (int32_t i = 0; i < 4; i++) {
			int32_t y = gateLineY + 1 + i; // Go down from gate line
			if (y >= 0 && y < kDisplayHeight && x < kDisplayWidth) {
				image[y][x] = scaleNoteColors[i];
				occupancyMask[y][x] = 64;
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

	// Delegate all other buttons (including PLAY) to parent view
	else {
		ActionResult result = ClipMinder::buttonAction(b, on);
		if (result == ActionResult::NOT_DEALT_WITH) {
			// Let the global view system handle it (including PLAY button)
			return ActionResult::NOT_DEALT_WITH;
		}
		return result;
	}

	return ActionResult::DEALT_WITH;
}

void PulseSeqView::graphicsRoutine() {
	// Disable the progress bar (tick squares) in Pulse Sequencer view
	// We don't want the white progress bar from clip views
	// The Pulse Sequencer has its own visual feedback with the red stage highlighting

	// Set flash cursor to OFF to disable tick squares (like instrument clips do)
	if (PadLEDs::flashCursor != FLASH_CURSOR_OFF) {
		uint8_t tickSquares[kDisplayHeight];
		memset(tickSquares, 255, kDisplayHeight); // 255 = disabled

		uint8_t colours[kDisplayHeight];
		memset(colours, 0, kDisplayHeight);

		PadLEDs::setTickSquares(tickSquares, colours);
	}

	// Force UI refresh during Pulse Sequencer playback to show red pad movement
	if (getCurrentInstrumentClip() && getCurrentInstrumentClip()->pulseSeqIsActive_) {
		uiNeedsRendering(this);
	}
}

ActionResult PulseSeqView::padAction(int32_t x, int32_t y, int32_t velocity) {
	if (velocity > 0) {
		// Each column represents a sequencer stage
		if (x >= 0 && x < 8) {
			// Update selected stage to the column we're pressing
			selectedParameter_ = x;

			// Calculate where the gate line currently is
			int32_t gateLineY = 3 + viewOffset_;

			if (y == gateLineY) {
				// Tapping the gate line cycles through gate types for this sequencer stage
				int32_t currentGateType = getParameterValue(x);
				int32_t nextGateType = (currentGateType + 1) % 4; // Cycle 0->1->2->3->0
				setParameterValue(x, nextGateType);
				showParameterValuePopup(0, nextGateType); // 0 = Gate type
			}
			else if (y == gateLineY - 1) {
				// Pulse count 1: set pulse count to 1
				setPulseCountValue(x, 1);
				showParameterValuePopup(2, 1); // 2 = Pulse count
			}
			else if (y == gateLineY - 2) {
				// Pulse count 2: set pulse count to 2
				setPulseCountValue(x, 2);
				showParameterValuePopup(2, 2); // 2 = Pulse count
			}
			else if (y == gateLineY - 3) {
				// Pulse count 3: set pulse count to 3
				setPulseCountValue(x, 3);
				showParameterValuePopup(2, 3); // 2 = Pulse count
			}
			else if (y == gateLineY - 4) {
				// Pulse count 4: set pulse count to 4
				setPulseCountValue(x, 4);
				showParameterValuePopup(2, 4); // 2 = Pulse count
			}
			else if (y == gateLineY - 5) {
				// Pulse count 5: set pulse count to 5
				setPulseCountValue(x, 5);
				showParameterValuePopup(2, 5); // 2 = Pulse count
			}
			else if (y == gateLineY - 6) {
				// Pulse count 6: set pulse count to 6
				setPulseCountValue(x, 6);
				showParameterValuePopup(2, 6); // 2 = Pulse count
			}
			else if (y == gateLineY - 7) {
				// Pulse count 7: set pulse count to 7
				setPulseCountValue(x, 7);
				showParameterValuePopup(2, 7); // 2 = Pulse count
			}
			else if (y == gateLineY - 8) {
				// Pulse count 8: set pulse count to 8
				setPulseCountValue(x, 8);
				showParameterValuePopup(2, 8); // 2 = Pulse count
			}
			else if (y == gateLineY + 1) {
				// Pitch up: increase scale note degree
				int32_t currentScaleNote = getScaleNoteValue(x);
				int32_t newScaleNote = (currentScaleNote + 1) % 12; // Wrap around
				setScaleNoteValue(x, newScaleNote);

				char noteName[10];
				getNoteName(noteName, sizeof(noteName), x);
				display->displayNotification("Note", noteName);
			}
			else if (y == gateLineY + 2) {
				// Pitch down: decrease scale note degree
				int32_t currentScaleNote = getScaleNoteValue(x);
				int32_t newScaleNote = (currentScaleNote - 1 + 12) % 12; // Wrap around
				setScaleNoteValue(x, newScaleNote);

				char noteName[10];
				getNoteName(noteName, sizeof(noteName), x);
				display->displayNotification("Note", noteName);
			}
			else if (y == gateLineY + 3) {
				// Octave up: increase octave
				int32_t currentOctave = getOctaveValue(x);
				int32_t newOctave = std::min(static_cast<int32_t>(3), currentOctave + 1);
				setOctaveValue(x, newOctave);

				char noteName[10];
				getNoteName(noteName, sizeof(noteName), x);
				display->displayNotification("Note", noteName);
			}
			else if (y == gateLineY + 4) {
				// Octave down: decrease octave
				int32_t currentOctave = getOctaveValue(x);
				int32_t newOctave = std::max(static_cast<int32_t>(-3), currentOctave - 1);
				setOctaveValue(x, newOctave);

				char noteName[10];
				getNoteName(noteName, sizeof(noteName), x);
				display->displayNotification("Note", noteName);
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
	// Scroll the view up/down to show different parts of the range
	// viewOffset_ controls which part of the range is visible
	// We want GL to range from 3 to 7 (5 positions)
	// Default GL=3, CW increases GL, CCW decreases GL

	int32_t oldViewOffset = viewOffset_;
	int32_t oldGateLineY = 3 + viewOffset_; // GL = 3 + VO

	viewOffset_ += offset; // Clockwise moves gate line up (higher Y values)

	// Clamp viewOffset to keep GL between 3 and 7 (not below 3)
	// GL = 3 + VO, so:
	// GL = 3 means VO = 0
	// GL = 7 means VO = 4
	// Therefore: VO should be clamped to 0 to 4
	viewOffset_ = std::max(static_cast<int32_t>(0), std::min(static_cast<int32_t>(4), viewOffset_));

	int32_t newGateLineY = 3 + viewOffset_;

	// Debug logging removed - gate line positioning working correctly

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

void PulseSeqView::setScaleNoteValue(int32_t column, int32_t value) {
	if (!currentClip_)
		return;

	// Clamp scale note to 0-11 range (scale degree within octave)
	value = std::max(0, std::min(11, (int)value));
	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;

	// Each column is a sequencer stage - scale note only
	switch (column) {
	case 0:
		instrumentClip->scaleNote0_ = value; // Stage 0 scale note
		break;
	case 1:
		instrumentClip->scaleNote1_ = value; // Stage 1 scale note
		break;
	case 2:
		instrumentClip->scaleNote2_ = value; // Stage 2 scale note
		break;
	case 3:
		instrumentClip->scaleNote3_ = value; // Stage 3 scale note
		break;
	case 4:
		instrumentClip->scaleNote4_ = value; // Stage 4 scale note
		break;
	case 5:
		instrumentClip->scaleNote5_ = value; // Stage 5 scale note
		break;
	case 6:
		instrumentClip->scaleNote6_ = value; // Stage 6 scale note
		break;
	case 7:
		instrumentClip->scaleNote7_ = value; // Stage 7 scale note
		break;
	}
}

int32_t PulseSeqView::getScaleNoteValue(int32_t column) {
	if (!currentClip_)
		return 0;

	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;
	// Each column is a sequencer stage - scale note only
	switch (column) {
	case 0:
		return instrumentClip->scaleNote0_; // Stage 0 scale note
	case 1:
		return instrumentClip->scaleNote1_; // Stage 1 scale note
	case 2:
		return instrumentClip->scaleNote2_; // Stage 2 scale note
	case 3:
		return instrumentClip->scaleNote3_; // Stage 3 scale note
	case 4:
		return instrumentClip->scaleNote4_; // Stage 4 scale note
	case 5:
		return instrumentClip->scaleNote5_; // Stage 5 scale note
	case 6:
		return instrumentClip->scaleNote6_; // Stage 6 scale note
	case 7:
		return instrumentClip->scaleNote7_; // Stage 7 scale note
	default:
		return 0;
	}
}

void PulseSeqView::setOctaveValue(int32_t column, int32_t value) {
	if (!currentClip_)
		return;

	// Clamp octave to -3 to +3 range
	value = std::max(-3, std::min(3, (int)value));
	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;

	// Each column is a sequencer stage - octave only
	switch (column) {
	case 0:
		instrumentClip->octave0_ = value; // Stage 0 octave
		break;
	case 1:
		instrumentClip->octave1_ = value; // Stage 1 octave
		break;
	case 2:
		instrumentClip->octave2_ = value; // Stage 2 octave
		break;
	case 3:
		instrumentClip->octave3_ = value; // Stage 3 octave
		break;
	case 4:
		instrumentClip->octave4_ = value; // Stage 4 octave
		break;
	case 5:
		instrumentClip->octave5_ = value; // Stage 5 octave
		break;
	case 6:
		instrumentClip->octave6_ = value; // Stage 6 octave
		break;
	case 7:
		instrumentClip->octave7_ = value; // Stage 7 octave
		break;
	}
}

int32_t PulseSeqView::getOctaveValue(int32_t column) {
	if (!currentClip_)
		return 0;

	InstrumentClip* instrumentClip = (InstrumentClip*)currentClip_;
	// Each column is a sequencer stage - octave only
	switch (column) {
	case 0:
		return instrumentClip->octave0_; // Stage 0 octave
	case 1:
		return instrumentClip->octave1_; // Stage 1 octave
	case 2:
		return instrumentClip->octave2_; // Stage 2 octave
	case 3:
		return instrumentClip->octave3_; // Stage 3 octave
	case 4:
		return instrumentClip->octave4_; // Stage 4 octave
	case 5:
		return instrumentClip->octave5_; // Stage 5 octave
	case 6:
		return instrumentClip->octave6_; // Stage 6 octave
	case 7:
		return instrumentClip->octave7_; // Stage 7 octave
	default:
		return 0;
	}
}

void PulseSeqView::setPulseCountValue(int32_t column, int32_t value) {
	if (!currentClip_)
		return;

	// Clamp pulse count to 1-8 range (1-8 = 1 to 8 pulses, never 0)
	value = std::max(1, std::min(8, (int)value));
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

int32_t PulseSeqView::getPulseCountValue(int32_t column) {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (!currentClip)
		return 1; // Default to 1 pulse instead of 0

	// Each column is a sequencer stage - pulse count only
	switch (column) {
	case 0:
		return currentClip->pulse0_; // Stage 0 pulse count
	case 1:
		return currentClip->pulse1_; // Stage 1 pulse count
	case 2:
		return currentClip->pulse2_; // Stage 2 pulse count
	case 3:
		return currentClip->pulse3_; // Stage 3 pulse count
	case 4:
		return currentClip->pulse4_; // Stage 4 pulse count
	case 5:
		return currentClip->pulse5_; // Stage 5 pulse count
	case 6:
		return currentClip->pulse6_; // Stage 6 pulse count
	case 7:
		return currentClip->pulse7_; // Stage 7 pulse count
	default:
		return 1; // Default to 1 pulse
	}
}

// Helper functions for scale note calculation
NoteSet PulseSeqView::getCurrentScaleNotes() {
	if (!currentClip_ || !currentClip_->inScaleMode) {
		// Return chromatic scale (all 12 notes) if no scale selected
		NoteSet chromatic;
		chromatic.fill(); // Sets all 12 semitones
		return chromatic;
	}

	// Get the current song's scale notes
	return currentSong->key.modeNotes;
}

int32_t PulseSeqView::getActualNoteValue(int32_t column) {
	int32_t scaleNote = getScaleNoteValue(column);
	int32_t octave = getOctaveValue(column);

	NoteSet scaleNotes = getCurrentScaleNotes();

	// Get the note at the specified scale degree
	int32_t noteInOctave = scaleNotes[scaleNote];

	if (noteInOctave == -1) {
		// Scale degree doesn't exist, fallback to chromatic
		noteInOctave = scaleNote;
	}

	// Add root note offset and octave
	int32_t rootNote = currentSong->key.rootNote;

	return rootNote + noteInOctave + (octave * 12);
}

void PulseSeqView::getNoteName(char* buffer, int32_t bufferSize, int32_t column) {
	int32_t actualNote = getActualNoteValue(column);

	// Convert MIDI note number to note name
	const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

	int32_t noteInOctave = actualNote % 12;
	int32_t octave = (actualNote / 12) - 1; // MIDI note 60 = C4

	snprintf(buffer, bufferSize, "%s%d", noteNames[noteInOctave], octave);
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

// Pulse Sequencer timing methods
void PulseSeqView::processPulseSeqTick() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (!currentClip || !currentClip->pulseSeqIsActive_) {
		return;
	}

	// Debug: Show current state
	char debugMsg[64];
	snprintf(debugMsg, sizeof(debugMsg), "Tick: Stage=%d PulsesLeft=%d", currentClip->currentPulseSeqStage_,
	         currentClip->pulsesRemainingInStage_);
	Debug::println(debugMsg);

	// Decrement pulses remaining in current stage
	currentClip->pulsesRemainingInStage_--;

	// If we've used up all pulses in this stage, move to next stage
	if (currentClip->pulsesRemainingInStage_ <= 0) {
		// Move to next stage
		currentClip->currentPulseSeqStage_++;
		if (currentClip->currentPulseSeqStage_ >= 8) {
			currentClip->currentPulseSeqStage_ = 0; // Loop back to stage 0
		}

		// Set pulses remaining for new stage
		int32_t pulseCount = getPulseCountValue(currentClip->currentPulseSeqStage_);
		currentClip->pulsesRemainingInStage_ = pulseCount;

		// Debug: Show stage advancement
		snprintf(debugMsg, sizeof(debugMsg), "Advanced to Stage=%d PulseCount=%d", currentClip->currentPulseSeqStage_,
		         pulseCount);
		Debug::println(debugMsg);
	}

	// Generate note for current stage if gate type is not OFF
	int32_t currentStage = currentClip->currentPulseSeqStage_;
	int32_t gateType = getParameterValue(currentStage);

	if (gateType != 0) { // Not OFF
		// Get the note to play based on scale note and octave
		int32_t note = getActualNoteValue(currentStage);

		// Get velocity (use default velocity for now)
		int32_t velocity = 100; // Default velocity

		// Get instrument
		Instrument* instrument = (Instrument*)currentClip->output;

		// Create model stack for note generation
		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);

		// Generate the note
		if (instrument->type == OutputType::KIT) {
			// For kits, we'd need to handle drums differently
			// For now, skip kit support
		}
		else {
			// For melodic instruments (synth, MIDI, CV)
			((MelodicInstrument*)instrument)
			    ->beginAuditioningForNote(modelStack, note, velocity, zeroMPEValues, MIDI_CHANNEL_NONE, 0);

			// Debug: Show note being played
			char noteMsg[32];
			snprintf(noteMsg, sizeof(noteMsg), "Note:%d", note);
			Debug::println(noteMsg);
		}
	}

	// Request UI update to show current stage
	uiNeedsRendering(this);
}

// Arpeggiator-style timing integration
int32_t PulseSeqView::doTickForward(uint32_t clipCurrentPos, bool currentlyPlayingReversed) {
	Debug::println("doTickForward called!");

	// Get current clip using global function like other views
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (!currentClip) {
		Debug::println("doTickForward: getCurrentInstrumentClip() returned NULL!");
		return 2147483647; // No next event
	}
	if (!currentClip->pulseSeqIsActive_) {
		Debug::println("doTickForward: pulseSeqIsActive_ is FALSE!");
		return 2147483647; // No next event
	}

	// Use 16th note sync level (same as arpeggiator)
	const uint32_t syncLevel = 5; // SYNC_LEVEL_16TH
	const uint32_t syncType = 0;  // SYNC_TYPE_EVEN

	// Calculate ticks per period (same formula as arpeggiator)
	uint32_t ticksPerPeriod = 3 << (9 - syncLevel);
	if (syncType == 1) { // SYNC_TYPE_TRIPLET
		ticksPerPeriod = ticksPerPeriod * 2 / 3;
	}
	else if (syncType == 2) { // SYNC_TYPE_DOTTED
		ticksPerPeriod = ticksPerPeriod * 3 / 2;
	}

	// Check if we're at the start of a new period
	int32_t howFarIntoPeriod = clipCurrentPos % ticksPerPeriod;

	// Debug: Show timing calculation (console only)
	char debugMsg[32];
	snprintf(debugMsg, sizeof(debugMsg), "HFP:%d", howFarIntoPeriod);
	Debug::println(debugMsg);

	if (!howFarIntoPeriod) {
		// Time for a new pulse sequencer step
		processPulseSeqTick();
		howFarIntoPeriod = ticksPerPeriod;

		// Debug: Show current stage (only when stage advances)
		char stageMsg[32];
		snprintf(stageMsg, sizeof(stageMsg), "Stage:%d", currentClip->currentPulseSeqStage_);
		display->displayPopup((char*)stageMsg);
	}
	else {
		if (!currentlyPlayingReversed) {
			howFarIntoPeriod = ticksPerPeriod - howFarIntoPeriod;
		}
	}

	return howFarIntoPeriod;
}

void PulseSeqView::startPulseSeq() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (!currentClip) {
		display->displayNotification("PulseSeq", "No Clip!");
		return;
	}

	display->displayNotification("PulseSeq", "Active");
	Debug::println("PulseSeq Started!");
	currentClip->pulseSeqIsActive_ = true;
	currentClip->currentPulseSeqStage_ = 0;
	currentClip->pulsesRemainingInStage_ = getPulseCountValue(0);

	// Request UI update
	uiNeedsRendering(this);
}

void PulseSeqView::stopPulseSeq() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (!currentClip) {
		return;
	}

	currentClip->pulseSeqIsActive_ = false;

	// Request UI update
	uiNeedsRendering(this);
}

void PulseSeqView::resetPulseSeq() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (!currentClip) {
		return;
	}

	currentClip->currentPulseSeqStage_ = 0;
	currentClip->pulsesRemainingInStage_ = getPulseCountValue(0);

	// Request UI update
	uiNeedsRendering(this);
}

int32_t PulseSeqView::getCurrentStage() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (!currentClip) {
		return 0;
	}
	return currentClip->currentPulseSeqStage_;
}

int32_t PulseSeqView::getPulsesRemainingInStage() {
	InstrumentClip* currentClip = getCurrentInstrumentClip();
	if (!currentClip) {
		return 1;
	}
	return currentClip->pulsesRemainingInStage_;
}

} // namespace deluge::gui::views

// Global instance
deluge::gui::views::PulseSeqView pulseSeqView;
