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

#include "sequencer_clip_view.h"
#include "gui/ui/sound_editor.h"
#include "gui/colour/colour.h"
#include "model/clip/sequencer_clip.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include <algorithm>
#include <array>

bool SequencerClipView::opened() {
	bool success = ClipView::opened();
	if (success) {
		needsRendering_ = true;
		lastRenderTime_ = 0;
	}
	return success;
}

void SequencerClipView::focusRegained() {
	ClipView::focusRegained();
	needsRendering_ = true;
}

bool SequencerClipView::renderMainPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                                       uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], bool drawUndefinedArea) {
	if (needsRendering_) {
		renderSequencerControls(image);
		renderPlayhead(image);
		needsRendering_ = false;
		lastRenderTime_ = AudioEngine::audioSampleTimer;
	}
	
	return true;
}

bool SequencerClipView::renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                                      uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	// Common sidebar - red to indicate sequencer clip
	SequencerClip* clip = getCurrentSequencerClip();
	if (clip && image) {
		RGB color = RGB{255, 0, 0}; // Red for sequencer
		for (int y = 0; y < kDisplayHeight; y++) {
			image[y][kDisplayWidth] = color;
		}
	}
	return true;
}

bool SequencerClipView::setupScroll(uint32_t oldScroll) {
	return ClipView::setupScroll(oldScroll);
}

void SequencerClipView::tellMatrixDriverWhichRowsContainSomethingZoomable() {
	// Sequencer clips don't have zoomable content
}

ActionResult SequencerClipView::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
	return ClipView::buttonAction(b, on, inCardRoutine);
}

ActionResult SequencerClipView::padAction(int32_t x, int32_t y, int32_t velocity) {
	if (velocity > 0) {
		handlePadPress(x, y, velocity);
		needsRendering_ = true;
		return ActionResult::DEALT_WITH;
	}
	
	return ClipView::padAction(x, y, velocity);
}

void SequencerClipView::graphicsRoutine() {
	if (playbackHandler.isEitherClockActive()) {
		uint32_t currentTime = AudioEngine::audioSampleTimer;
		if (currentTime - lastRenderTime_ > kSampleRate / 30) { // 30fps refresh
			needsRendering_ = true;
		}
	}
}

void SequencerClipView::playbackEnded() {
	ClipView::playbackEnded();
	needsRendering_ = true;
}

void SequencerClipView::clipNeedsReRendering(Clip* clip) {
	needsRendering_ = true;
}

void SequencerClipView::selectEncoderAction(int8_t offset) {
	handleEncoderTurn(offset);
	needsRendering_ = true;
}

ActionResult SequencerClipView::horizontalEncoderAction(int32_t offset) {
	// Cycle through SequencerType values
	SequencerClip* clip = getCurrentSequencerClip();
	if (clip) {
		SequencerType currentType = clip->getSequencerType();
		SequencerType newType = currentType;
		
		// Cycle through the enum values
		int32_t currentIndex = static_cast<int32_t>(currentType);
		int32_t maxIndex = static_cast<int32_t>(SequencerType::ARPEGGIATOR);
		
		if (offset > 0) {
			// Next type
			currentIndex = (currentIndex + 1) % (maxIndex + 1);
		} else {
			// Previous type
			currentIndex = (currentIndex - 1 + (maxIndex + 1)) % (maxIndex + 1);
		}
		
		newType = static_cast<SequencerType>(currentIndex);
		clip->setSequencerType(newType);
		
		needsRendering_ = true;
		
		// Display the new type
		const char* typeNames[] = {"RANDOM", "PULSE", "EUCLIDEAN", "ARP"};
		display->displayPopup(typeNames[currentIndex][0]);
		
		return ActionResult::DEALT_WITH;
	}
	
	return ActionResult::NOT_DEALT_WITH;
}

ActionResult SequencerClipView::verticalEncoderAction(int32_t offset, bool inCardRoutine) {
	return ClipView::verticalEncoderAction(offset, inCardRoutine);
}

ActionResult SequencerClipView::timerCallback() {
	return ClipView::timerCallback();
}

uint32_t SequencerClipView::getMaxLength() {
	return ClipView::getMaxLength();
}

uint32_t SequencerClipView::getMaxZoom() {
	return ClipView::getMaxZoom();
}

void SequencerClipView::renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) {
	SequencerClip* clip = getCurrentSequencerClip();
	if (clip) {
		canvas.drawString("SEQUENCER", 0, 0, kTextSpacingX, kTextSizeYUpdated);
		renderOLEDInfo(canvas);
	}
}

SequencerClip* SequencerClipView::getCurrentSequencerClip() {
	Clip* clip = getCurrentClip();
	if (clip && clip->type == ClipType::SEQUENCER) {
		return static_cast<SequencerClip*>(clip);
	}
	return nullptr;
}

void SequencerClipView::renderPlayhead(RGB image[][kDisplayWidth + kSideBarWidth]) {
	SequencerClip* clip = getCurrentSequencerClip();
	if (!clip || !playbackHandler.isEitherClockActive()) {
		return;
	}
	
	uint32_t currentStep = clip->getCurrentStep();
	uint32_t patternLength = clip->getPatternLength();
	
	if (patternLength > 0) {
		int32_t playheadX = (static_cast<int32_t>(currentStep) * kDisplayWidth) / static_cast<int32_t>(patternLength);
		playheadX = (playheadX < static_cast<int32_t>(kDisplayWidth - 1)) ? playheadX : static_cast<int32_t>(kDisplayWidth - 1);
		
		// Render playhead as bright green
		image[kDisplayHeight - 1][playheadX] = RGB{0, 255, 0};
	}
}

void SequencerClipView::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	// Base implementation - can be overridden by specific views
	// For now, just trigger a re-render
	needsRendering_ = true;
}