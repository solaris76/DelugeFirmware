/*
 * Copyright © 2026 Chris Griggs
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 */

#pragma once

#include "gui/l10n/l10n.h"
#include "model/clip/sequencer/sequencer_mode.h"
#include <array>

namespace deluge::model::clip::sequencer::modes {

/// Acid Seq — Sting / Push-style performance grid (not Step Seq).
///
///   y7 — Type 0–16 (left→right bar, tap column)
///   y6 — Density 0–16 (left→right bar, tap column; Shift+col15 = 16)
///   y5 — Random pattern (col 15)
///   y4 — Playhead / active steps (col = step 1–16)
///   y0–y3 — Root keyboard (cols 0–11 = C…B; y0 = C-1, y1 = C0, y2 = C1, y3 = C2)
///   Sidebar x17 y0–y7 — 8 sequence slots (scenes)
///   Encoders: vertical = density, horizontal = type (unless a sidebar pad is held — then <> configures that pad)
class AcidSequencerMode : public SequencerMode {
public:
	AcidSequencerMode() = default;
	~AcidSequencerMode() override = default;

	l10n::String name() override { return l10n::String::STRING_FOR_ACID_SEQ; }

	bool supportsInstrument() override { return true; }
	bool supportsKit() override { return false; }
	bool supportsMIDI() override { return true; }
	bool supportsCV() override { return true; }
	bool supportsAudio() override { return false; }
	bool supportsControlType(ControlType type) override { return true; }

	void initialize() override;
	void cleanup() override;

	bool renderPads(uint32_t whichRows, RGB* image, uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth],
	                int32_t xScroll, uint32_t xZoom, int32_t renderWidth, int32_t imageWidth) override;
	bool handlePadPress(int32_t x, int32_t y, int32_t velocity) override;

protected:
	bool handleModeSpecificVerticalEncoder(int32_t offset) override;
	bool handleHorizontalEncoder(int32_t offset, bool encoderPressed) override;

public:
	int32_t processPlayback(void* modelStack, int32_t absolutePlaybackPos) override;
	void stopAllNotes(void* modelStack) override;

	size_t captureScene(void* buffer, size_t maxSize) override;
	bool recallScene(const void* buffer, size_t size) override;

	void resetToInit() override;
	void randomizeAll(int32_t mutationRate = 100) override;
	void evolveNotes(int32_t mutationRate = 30) override;

	void writeToFile(Serializer& writer, bool includeScenes = true) override;
	Error readFromFile(Deserializer& reader) override;
	bool copyFrom(SequencerMode* other) override;

	static constexpr int32_t kNumSteps = 16;
	static constexpr int32_t kMaxUiLevel = 16;
	static constexpr int32_t kChromaticCols = 12;

	enum class GateType : int32_t { OFF = 0, ON = 1, SKIP = 2 };

	struct Step {
		GateType gateType{GateType::OFF};
		int8_t octave{0};
		uint8_t noteIndex{0};
		uint8_t gateLength{60};
		bool accent{false};
		bool slide{false};
	};

protected:
	static constexpr uint8_t kBaseVelocity = 80;
	static constexpr uint8_t kAccentVelocity = 120;

	std::array<Step, kNumSteps> steps_{};

	uint8_t type_ = 8;
	uint8_t density_ = 8;
	uint8_t rootSemitone_ = 0; // 0–11 (C–B)
	int8_t rootOctave_ = 1;    // scientific octave: y0 = -1 … y3 = +2 (default C1)

	bool initialized_ = false;
	int32_t ticksPerSixteenthNote_ = 0;
	uint8_t currentStep_ = 0;
	int32_t lastAbsolutePlaybackPos_ = 0;
	int8_t pingPongDirection_ = 1;
	int16_t activeNoteCode_ = -1;

	int32_t scaleNotes_[12];
	uint8_t numScaleNotes_ = 0;

	void updateScaleNotes(void* modelStackPtr);
	void clampGlobalsToUiRange();
	void clampRootAndStepState();
	void generatePattern();
	int32_t calculateNoteCode(const Step& step, const CombinedEffects& effects) const;
	uint8_t velocityForStep(const Step& step) const;
	void advanceStep(int32_t direction);
	void displayDensityPopup() const;
	void displayTypePopup() const;
	void displayRootPopup() const;
	void triggerRandomPattern();
	RGB colorForBarFill(int32_t column, int32_t level0to16) const;
	RGB colorForRootPad(int32_t semitone) const;
	bool isRootPadSelected(int32_t x, int32_t y) const;
	int32_t rootMidiNote() const;
};

} // namespace deluge::model::clip::sequencer::modes
