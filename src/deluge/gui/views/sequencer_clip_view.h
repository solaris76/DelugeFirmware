/*
 * Copyright © 2019-2023 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "gui/views/clip_view.h"
#include "hid/button.h"
#include "model/clip/clip_minder.h"
#include <array>
#include <cstdint>

class SequencerClip;

class SequencerClipView : public ClipView, public ClipMinder {
public:
	SequencerClipView() = default;

	// Common sequencer view methods
	bool opened() override;
	void focusRegained() override;
	bool renderMainPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                    uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], bool drawUndefinedArea = true) override;
	bool renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                   uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override;
	bool setupScroll(uint32_t oldScroll) override;
	void tellMatrixDriverWhichRowsContainSomethingZoomable() override;
	[[nodiscard]] bool supportsTriplets() const override { return true; }
	ClipMinder* toClipMinder() override { return this; }

	ActionResult buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) override;
	ActionResult padAction(int32_t x, int32_t y, int32_t velocity) override;

	void graphicsRoutine() override;
	void playbackEnded() override;

	void clipNeedsReRendering(Clip* clip) override;
	void selectEncoderAction(int8_t offset) override;
	ActionResult horizontalEncoderAction(int32_t offset) override;
	ActionResult verticalEncoderAction(int32_t offset, bool inCardRoutine) override;
	ActionResult timerCallback() override;
	uint32_t getMaxLength() override;
	uint32_t getMaxZoom() override;

	void renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) override;

	// ui
	UIType getUIType() override { return UIType::INSTRUMENT_CLIP; }
	UIModControllableContext getUIModControllableContext() override { return UIModControllableContext::CLIP; }

protected:
	// Common helper methods for all sequencer views
	SequencerClip* getCurrentSequencerClip();   // Legacy - not used with new approach
	InstrumentClip* getCurrentInstrumentClip(); // New approach - work with InstrumentClip in generative mode
	void renderPlayhead(RGB image[][kDisplayWidth + kSideBarWidth]);
	void handlePadPress(int32_t x, int32_t y, int32_t velocity);

	uint32_t lastRenderTime_;
	bool needsRendering_;

	// Virtual methods to be implemented by specific sequencer views
	virtual void renderSequencerControls(RGB image[][kDisplayWidth + kSideBarWidth]) = 0;
	virtual void handleEncoderTurn(int32_t offset) = 0;
	virtual void renderOLEDInfo(deluge::hid::display::oled_canvas::Canvas& canvas) = 0;
};
