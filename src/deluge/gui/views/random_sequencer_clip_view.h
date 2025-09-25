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

#include "gui/views/sequencer_clip_view.h"
#include <cstdint>
#include <array>

class SequencerClip;

class RandomSequencerClipView final : public SequencerClipView {
public:
	RandomSequencerClipView() = default;

protected:
	// Implement virtual methods from SequencerClipView
	void renderSequencerControls(RGB image[][kDisplayWidth + kSideBarWidth]) override;
	void handleEncoderTurn(int32_t offset) override;
	void renderOLEDInfo(deluge::hid::display::oled_canvas::Canvas& canvas) override;

private:
	void renderDensityControl(RGB image[][kDisplayWidth + kSideBarWidth]);
	void handleDensityChange(int32_t offset);
};

extern RandomSequencerClipView randomSequencerClipView;
