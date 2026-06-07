/*
 * Copyright © 2026 Chris Griggs
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 */

#include "model/clip/sequencer/modes/acid_sequencer_mode.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/led/pad_leds.h"
#include "model/clip/instrument_clip.h"
#include "model/model_stack.h"
#include "model/scale/note_set.h"
#include "model/song/song.h"
#include "storage/storage_manager.h"
#include "util/functions.h"
#include <cstring>

extern void noteCodeToString(int32_t noteCode, char* buffer, int32_t* getLengthWithoutDot, bool appendOctaveNo);

namespace deluge::model::clip::sequencer::modes {

namespace {

constexpr uint32_t kUiAllRows = 0xFFFFFFFF;

constexpr int32_t kRootRowMin = 0;
constexpr int32_t kRootRowMax = 3;
constexpr int32_t kPlayheadRow = 4;
constexpr int32_t kRandomRow = 5;
constexpr int32_t kRandomCol = 15;
constexpr int32_t kDensityRow = 6;
constexpr int32_t kTypeRow = 7;
constexpr int32_t kSteps = AcidSequencerMode::kNumSteps;

constexpr RGB kRootPalette[12] = {{0, 0, 255},   {32, 0, 220},  {64, 0, 200},  {96, 0, 180},
                                  {128, 0, 160}, {160, 0, 140}, {192, 0, 120}, {220, 0, 100},
                                  {255, 0, 80},  {255, 40, 40}, {255, 100, 0}, {255, 180, 0}};

struct AcidSceneData {
	std::array<AcidSequencerMode::Step, kSteps> steps;
	uint8_t type;
	uint8_t density;
	uint8_t rootSemitone;
	int8_t rootOctave;
};

void writeStepArray(Serializer& writer, const char* tag, const std::array<AcidSequencerMode::Step, kSteps>& steps) {
	uint8_t stepData[kSteps * 6];
	for (int32_t i = 0; i < kSteps; ++i) {
		int32_t o = i * 6;
		stepData[o] = steps[i].noteIndex;
		stepData[o + 1] = static_cast<uint8_t>(steps[i].octave + 3);
		stepData[o + 2] = static_cast<uint8_t>(steps[i].gateType);
		stepData[o + 3] = steps[i].gateLength;
		stepData[o + 4] = steps[i].accent ? 1 : 0;
		stepData[o + 5] = steps[i].slide ? 1 : 0;
	}
	writer.writeAttributeHexBytes(tag, stepData, sizeof(stepData));
}

void readStepArray(const char* hexData, std::array<AcidSequencerMode::Step, kSteps>& steps) {
	if (hexData[0] == '0' && hexData[1] == 'x') {
		hexData += 2;
	}
	int32_t hexLen = strlen(hexData);
	int32_t bytesPerStep = hexLen / (kSteps * 2);
	if (bytesPerStep < 3) {
		bytesPerStep = 3;
	}

	int32_t requiredHexChars = kSteps * bytesPerStep * 2;
	if (hexLen < requiredHexChars) {
		return;
	}

	for (int32_t i = 0; i < kSteps; ++i) {
		int32_t offset = i * bytesPerStep * 2;
		steps[i].noteIndex = static_cast<uint8_t>(hexToIntFixedLength(&hexData[offset], 2));
		steps[i].octave = static_cast<int8_t>(hexToIntFixedLength(&hexData[offset + 2], 2) - 3);
		steps[i].gateType = static_cast<AcidSequencerMode::GateType>(hexToIntFixedLength(&hexData[offset + 4], 2));
		if (bytesPerStep >= 6) {
			steps[i].gateLength = static_cast<uint8_t>(hexToIntFixedLength(&hexData[offset + 6], 2));
			if (steps[i].gateLength < 1 || steps[i].gateLength > 100) {
				steps[i].gateLength = 60;
			}
			steps[i].accent = hexToIntFixedLength(&hexData[offset + 8], 2) != 0;
			steps[i].slide = hexToIntFixedLength(&hexData[offset + 10], 2) != 0;
		}
		else {
			steps[i].gateLength = 60;
			steps[i].accent = false;
			steps[i].slide = false;
		}
	}
}

} // namespace

void AcidSequencerMode::clampGlobalsToUiRange() {
	if (density_ > kMaxUiLevel) {
		density_ = static_cast<uint8_t>((static_cast<int32_t>(density_) * kMaxUiLevel) / 127);
	}
	if (type_ > kMaxUiLevel) {
		type_ = static_cast<uint8_t>((static_cast<int32_t>(type_) * kMaxUiLevel) / 127);
	}
}

void AcidSequencerMode::clampRootAndStepState() {
	if (currentStep_ >= kNumSteps) {
		currentStep_ = 0;
	}
	if (rootSemitone_ >= kChromaticCols) {
		rootSemitone_ = 0;
	}
	rootOctave_ = static_cast<int8_t>(clampValue(static_cast<int32_t>(rootOctave_), -1, 2));
}

void AcidSequencerMode::displayDensityPopup() const {
	if (!::display) {
		return;
	}
	char buf[16];
	strcpy(buf, "DENS ");
	intToString(density_, &buf[5]);
	::display->displayPopup(buf);
}

void AcidSequencerMode::displayTypePopup() const {
	if (!::display) {
		return;
	}
	char buf[16];
	strcpy(buf, "TYPE ");
	intToString(type_, &buf[5]);
	::display->displayPopup(buf);
}

void AcidSequencerMode::displayRootPopup() const {
	if (!::display || numScaleNotes_ == 0) {
		return;
	}
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
	CombinedEffects effects = getCombinedEffects();
	Step dummy{};
	dummy.noteIndex = 0;
	int32_t noteCode = calculateNoteCode(dummy, effects);
	char buffer[16];
	noteCodeToString(noteCode, buffer, nullptr, true);
	::display->displayPopup(buffer);
}

void AcidSequencerMode::triggerRandomPattern() {
	generatePattern();
	if (::display) {
		::display->displayPopup("RANDOM");
	}
	uiNeedsRendering(&instrumentClipView, kUiAllRows, 0);
}

RGB AcidSequencerMode::colorForBarFill(int32_t column, int32_t level0to16) const {
	if (level0to16 <= 0) {
		return RGB{16, 16, 20};
	}
	int32_t litThrough = (level0to16 >= kMaxUiLevel) ? kSteps : (level0to16 * kSteps) / kMaxUiLevel;
	if (column >= litThrough) {
		return RGB{16, 16, 20};
	}
	int32_t t = (column * 255) / (kSteps - 1);
	return RGB{static_cast<uint8_t>(t / 2), static_cast<uint8_t>(180 + t / 4), 32};
}

RGB AcidSequencerMode::colorForRootPad(int32_t semitone) const {
	return kRootPalette[semitone % kChromaticCols];
}

bool AcidSequencerMode::isRootPadSelected(int32_t x, int32_t y) const {
	return x == static_cast<int32_t>(rootSemitone_) && y == (static_cast<int32_t>(rootOctave_) + 1);
}

int32_t AcidSequencerMode::rootMidiNote() const {
	// Scientific octave on row: y0 = -1 (C-1), y3 = +2 (C2); x = semitone within octave
	return (static_cast<int32_t>(rootOctave_) + 1) * 12 + static_cast<int32_t>(rootSemitone_);
}

void AcidSequencerMode::initialize() {
	initialized_ = true;
	currentStep_ = 0;
	activeNoteCode_ = -1;
	ticksPerSixteenthNote_ = 0;
	lastAbsolutePlaybackPos_ = 0;
	pingPongDirection_ = 1;

	uint8_t tickSquares[kDisplayHeight];
	uint8_t colours[kDisplayHeight];
	memset(tickSquares, 255, kDisplayHeight);
	memset(colours, 0, kDisplayHeight);
	PadLEDs::setTickSquares(tickSquares, colours);

	if (controlColumnState_.hasGenericSequencerSidebar()) {
		controlColumnState_.initializeAcidSeqSidebar();
	}

	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
	updateScaleNotes(modelStack->addTimelineCounter(getCurrentClip()));

	bool anyOn = false;
	for (const auto& s : steps_) {
		if (s.gateType == GateType::ON) {
			anyOn = true;
			break;
		}
	}
	if (!anyOn) {
		generatePattern();
	}
	clampGlobalsToUiRange();
	clampRootAndStepState();
}

void AcidSequencerMode::cleanup() {
	if (activeNoteCode_ >= 0) {
		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
		stopNote(modelStack->addTimelineCounter(getCurrentClip()), activeNoteCode_);
	}
	initialized_ = false;
	activeNoteCode_ = -1;
	numScaleNotes_ = 0;
}

void AcidSequencerMode::updateScaleNotes(void* modelStackPtr) {
	auto* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);
	Song* song = modelStack->song;
	if (!song) {
		numScaleNotes_ = 0;
		return;
	}
	InstrumentClip* clip = static_cast<InstrumentClip*>(modelStack->getTimelineCounter());
	if (!clip || !clip->inScaleMode) {
		numScaleNotes_ = 12;
		for (int32_t i = 0; i < 12; i++) {
			scaleNotes_[i] = i;
		}
	}
	else {
		NoteSet modeNotes = song->key.modeNotes;
		numScaleNotes_ = 0;
		for (int32_t i = 0; i < 12; i++) {
			if (modeNotes.has(i)) {
				scaleNotes_[numScaleNotes_++] = i;
			}
		}
	}
	for (auto& step : steps_) {
		if (numScaleNotes_ > 0 && step.noteIndex >= numScaleNotes_) {
			step.noteIndex = 0;
		}
	}
}

int32_t AcidSequencerMode::calculateNoteCode(const Step& step, const CombinedEffects& effects) const {
	if (numScaleNotes_ == 0) {
		return rootMidiNote();
	}
	int32_t noteIndexInScale = static_cast<int32_t>(step.noteIndex) + effects.transpose;
	while (noteIndexInScale < 0) {
		noteIndexInScale += numScaleNotes_;
	}
	while (noteIndexInScale >= numScaleNotes_) {
		noteIndexInScale -= numScaleNotes_;
	}
	int32_t note = rootMidiNote() + scaleNotes_[noteIndexInScale] + (step.octave * 12) + (effects.octaveShift * 12);
	if (note < 0) {
		note = 0;
	}
	if (note > 127) {
		note = 127;
	}
	return note;
}

uint8_t AcidSequencerMode::velocityForStep(const Step& step) const {
	return step.accent ? kAccentVelocity : kBaseVelocity;
}

void AcidSequencerMode::generatePattern() {
	for (auto& s : steps_) {
		s = Step{};
	}
	if (numScaleNotes_ == 0) {
		return;
	}

	int32_t noteIdx = 0;
	int32_t octave = 0;
	int32_t lastNoteIdx = 0;

	for (int32_t i = 0; i < kNumSteps; i++) {
		if (density_ == 0) {
			continue;
		}
		if ((getRandom255() % kMaxUiLevel) >= density_) {
			continue;
		}
		steps_[i].gateType = GateType::ON;
		steps_[i].noteIndex = static_cast<uint8_t>(noteIdx);
		steps_[i].octave = static_cast<int8_t>(octave);

		if ((getRandom255() % 100) < 30) {
			steps_[i].accent = true;
		}
		if (i > 0 && noteIdx == lastNoteIdx && (getRandom255() % 100) < 40) {
			steps_[i].slide = true;
		}

		lastNoteIdx = noteIdx;
		int32_t maxJump = 1 + (static_cast<int32_t>(type_) * numScaleNotes_) / kMaxUiLevel;
		if (maxJump < 1) {
			maxJump = 1;
		}
		int32_t jump = 1 + (getRandom255() % maxJump);
		if (getRandom255() % 2) {
			noteIdx += jump;
		}
		else {
			noteIdx -= jump;
		}
		while (noteIdx < 0) {
			noteIdx += numScaleNotes_;
		}
		while (noteIdx >= numScaleNotes_) {
			noteIdx -= numScaleNotes_;
		}
		if (getRandom255() % 16 == 0) {
			octave =
			    static_cast<int8_t>(clampValue(static_cast<int32_t>(octave) + ((getRandom255() % 2) ? 1 : -1), -2, 2));
		}
	}
}

void AcidSequencerMode::advanceStep(int32_t direction) {
	switch (direction) {
	case 1: {
		int32_t n = static_cast<int32_t>(currentStep_) - 1;
		if (n < 0) {
			n = kNumSteps - 1;
		}
		currentStep_ = static_cast<uint8_t>(n);
		break;
	}
	case 2: {
		int32_t n = static_cast<int32_t>(currentStep_) + pingPongDirection_;
		if (n >= kNumSteps) {
			n = kNumSteps - 2;
			pingPongDirection_ = -1;
		}
		else if (n < 0) {
			n = 1;
			pingPongDirection_ = 1;
		}
		currentStep_ = static_cast<uint8_t>(n);
		break;
	}
	case 3:
		currentStep_ = static_cast<uint8_t>(getRandom255() % kNumSteps);
		break;
	default:
		currentStep_ = static_cast<uint8_t>((static_cast<int32_t>(currentStep_) + 1) % kNumSteps);
		break;
	}
}

bool AcidSequencerMode::handleModeSpecificVerticalEncoder(int32_t offset) {
	int32_t d = static_cast<int32_t>(density_) + offset;
	density_ = static_cast<uint8_t>(clampValue(d, 0, kMaxUiLevel));
	displayDensityPopup();
	uiNeedsRendering(&instrumentClipView, 1 << kDensityRow, 0);
	return true;
}

bool AcidSequencerMode::handleHorizontalEncoder(int32_t offset, bool encoderPressed) {
	// Sidebar pad held: <> assigns control type / value (same as other sequencer modes)
	if (heldControlColumnX_ >= 0 && heldControlColumnY_ >= 0) {
		return SequencerMode::handleHorizontalEncoder(offset, encoderPressed);
	}

	int32_t t = static_cast<int32_t>(type_) + offset;
	type_ = static_cast<uint8_t>(clampValue(t, 0, kMaxUiLevel));
	displayTypePopup();
	uiNeedsRendering(&instrumentClipView, 1 << kTypeRow, 0);
	return true;
}

bool AcidSequencerMode::renderPads(uint32_t whichRows, RGB* image,
                                   uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], int32_t /*xScroll*/,
                                   uint32_t /*xZoom*/, int32_t /*renderWidth*/, int32_t imageWidth) {
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
	updateScaleNotes(modelStack->addTimelineCounter(getCurrentClip()));

	for (int32_t x = 0; x < kDisplayWidth; x++) {
		for (int32_t y = 0; y < kDisplayHeight; y++) {
			if (!(whichRows & (1 << y))) {
				continue;
			}

			RGB color{0, 0, 0};
			const Step& step = (x < kSteps) ? steps_[x] : steps_[0];
			bool isCurrent = (x == static_cast<int32_t>(currentStep_));

			if (y >= kRootRowMin && y <= kRootRowMax && x < kChromaticCols) {
				color = colorForRootPad(x);
				if (isRootPadSelected(x, y)) {
					color = RGB{255, 255, 255};
				}
				else {
					color = RGB{static_cast<uint8_t>(color.r / 4), static_cast<uint8_t>(color.g / 4),
					            static_cast<uint8_t>(color.b / 4)};
				}
			}
			else if (y == kPlayheadRow && x < kSteps) {
				if (isCurrent) {
					color = RGB{255, 0, 0};
				}
				else if (step.gateType == GateType::ON) {
					color = RGB{0, 64, 32};
				}
				else if (step.gateType == GateType::SKIP) {
					color = RGB{48, 0, 48};
				}
			}
			else if (y == kRandomRow && x == kRandomCol) {
				color = RGB{255, 80, 255};
			}
			else if (y == kDensityRow && x < kSteps) {
				color = colorForBarFill(x, density_);
			}
			else if (y == kTypeRow && x < kSteps) {
				color = colorForBarFill(x, type_);
			}

			image[y * imageWidth + x] = color;
			if (occupancyMask) {
				occupancyMask[y][x] = (color.r || color.g || color.b) ? 64 : 0;
			}
		}
	}
	return true;
}

bool AcidSequencerMode::handlePadPress(int32_t x, int32_t y, int32_t velocity) {
	if (x >= kDisplayWidth) {
		return SequencerMode::handlePadPress(x, y, velocity);
	}
	if (velocity == 0) {
		return false;
	}

	if (y >= kRootRowMin && y <= kRootRowMax && x < kChromaticCols) {
		rootSemitone_ = static_cast<uint8_t>(x);
		rootOctave_ = static_cast<int8_t>(y - 1);
		displayRootPopup();
		uiNeedsRendering(&instrumentClipView,
		                 (1 << kRootRowMin) | (1 << (kRootRowMin + 1)) | (1 << (kRootRowMin + 2)) | (1 << kRootRowMax),
		                 0);
		return true;
	}

	if (x >= kSteps) {
		return false;
	}

	if (y == kDensityRow) {
		if (Buttons::isShiftButtonPressed() && x == kSteps - 1) {
			density_ = kMaxUiLevel;
		}
		else {
			density_ = static_cast<uint8_t>((x * kMaxUiLevel) / (kSteps - 1));
		}
		displayDensityPopup();
		uiNeedsRendering(&instrumentClipView, 1 << kDensityRow, 0);
		return true;
	}

	if (y == kTypeRow) {
		type_ = static_cast<uint8_t>((x * kMaxUiLevel) / (kSteps - 1));
		displayTypePopup();
		uiNeedsRendering(&instrumentClipView, 1 << kTypeRow, 0);
		return true;
	}

	if (y == kRandomRow && x == kRandomCol) {
		triggerRandomPattern();
		return true;
	}

	return false;
}

int32_t AcidSequencerMode::processPlayback(void* modelStackPtr, int32_t absolutePlaybackPos) {
	if (!initialized_) {
		return 2147483647;
	}
	auto* modelStack = static_cast<ModelStackWithTimelineCounter*>(modelStackPtr);
	CombinedEffects effects = getCombinedEffects();

	if (ticksPerSixteenthNote_ == 0) {
		ticksPerSixteenthNote_ = modelStack->song->getSixteenthNoteLength();
	}
	if (absolutePlaybackPos == 0) {
		currentStep_ = 0;
		pingPongDirection_ = 1;
	}

	int32_t adjustedTicks = ticksPerSixteenthNote_;
	if (effects.clockDivider > 1) {
		adjustedTicks *= effects.clockDivider;
	}
	else if (effects.clockDivider < -1) {
		adjustedTicks /= (-effects.clockDivider);
	}

	updateScaleNotes(modelStackPtr);
	if (!atDivisionBoundary(absolutePlaybackPos, adjustedTicks)) {
		return ticksUntilNextDivision(absolutePlaybackPos, adjustedTicks);
	}

	if (absolutePlaybackPos > lastAbsolutePlaybackPos_) {
		advanceStep(effects.direction);
	}

	if (activeNoteCode_ >= 0) {
		stopNote(modelStackPtr, activeNoteCode_);
		activeNoteCode_ = -1;
	}

	uiNeedsRendering(&instrumentClipView, 1 << kPlayheadRow, 0);

	int32_t checked = 0;
	while (checked < kNumSteps) {
		const Step& step = steps_[currentStep_];
		if (step.gateType == GateType::SKIP) {
			advanceStep(effects.direction);
			checked++;
			uiNeedsRendering(&instrumentClipView, 1 << kPlayheadRow, 0);
		}
		else {
			if (step.gateType == GateType::ON) {
				int32_t noteCode = calculateNoteCode(step, effects);
				if (noteCode >= 0 && noteCode <= 127) {
					int32_t noteLength = (adjustedTicks * static_cast<int32_t>(step.gateLength)) / 100;
					if (step.slide) {
						noteLength = (noteLength * 130) / 100;
					}
					playNote(modelStackPtr, noteCode, velocityForStep(step), noteLength);
					activeNoteCode_ = noteCode;
				}
			}
			break;
		}
	}

	lastAbsolutePlaybackPos_ = absolutePlaybackPos;
	return adjustedTicks;
}

void AcidSequencerMode::stopAllNotes(void* modelStackPtr) {
	if (activeNoteCode_ >= 0) {
		stopNote(modelStackPtr, activeNoteCode_);
		activeNoteCode_ = -1;
	}
	currentStep_ = 0;
	uiNeedsRendering(&instrumentClipView, 1 << kPlayheadRow, 0);
}

void AcidSequencerMode::resetToInit() {
	steps_ = {};
	type_ = 8;
	density_ = 8;
	rootSemitone_ = 0;
	rootOctave_ = 1;
	controlColumnState_.initializeAcidSeqSidebar();
	generatePattern();
	uiNeedsRendering(&instrumentClipView, kUiAllRows, 0xFFFFFFFF);
}

void AcidSequencerMode::randomizeAll(int32_t mutationRate) {
	density_ = static_cast<uint8_t>(clampValue((mutationRate * kMaxUiLevel) / 127, 0, kMaxUiLevel));
	type_ = static_cast<uint8_t>(clampValue(4 + (mutationRate * 12) / 127, 0, kMaxUiLevel));
	if (mutationRate >= 100) {
		generatePattern();
	}
	else {
		for (int32_t i = 0; i < kNumSteps; i++) {
			if (getRandom255() % 100 >= mutationRate) {
				continue;
			}
			if (numScaleNotes_ > 0) {
				steps_[i].noteIndex = static_cast<uint8_t>(getRandom255() % numScaleNotes_);
			}
			steps_[i].gateType = GateType::ON;
			steps_[i].accent = (getRandom255() % 100) < 35;
			steps_[i].slide = (getRandom255() % 100) < 25;
		}
	}
	uiNeedsRendering(&instrumentClipView, kUiAllRows, 0xFFFFFFFF);
}

void AcidSequencerMode::evolveNotes(int32_t mutationRate) {
	if (numScaleNotes_ == 0) {
		return;
	}
	for (int32_t i = 0; i < kNumSteps; i++) {
		if (getRandom255() % 100 >= mutationRate) {
			continue;
		}
		int32_t change = (mutationRate > 70) ? ((getRandom255() % 5) - 2) : ((getRandom255() % 3) - 1);
		int32_t newIdx = static_cast<int32_t>(steps_[i].noteIndex) + change;
		while (newIdx < 0) {
			newIdx += numScaleNotes_;
		}
		while (newIdx >= numScaleNotes_) {
			newIdx -= numScaleNotes_;
		}
		steps_[i].noteIndex = static_cast<uint8_t>(newIdx);
	}
	uiNeedsRendering(&instrumentClipView, kUiAllRows, 0);
}

size_t AcidSequencerMode::captureScene(void* buffer, size_t maxSize) {
	static_assert(sizeof(AcidSceneData) <= 512, "Acid scene must fit control-column scene buffer");
	if (maxSize < sizeof(AcidSceneData)) {
		return 0;
	}
	auto* scene = static_cast<AcidSceneData*>(buffer);
	scene->steps = steps_;
	scene->type = type_;
	scene->density = density_;
	scene->rootSemitone = rootSemitone_;
	scene->rootOctave = rootOctave_;
	return sizeof(AcidSceneData);
}

bool AcidSequencerMode::recallScene(const void* buffer, size_t size) {
	if (size < sizeof(AcidSceneData)) {
		return false;
	}
	auto* scene = static_cast<const AcidSceneData*>(buffer);
	steps_ = scene->steps;
	type_ = scene->type;
	density_ = scene->density;
	rootSemitone_ = scene->rootSemitone;
	rootOctave_ = scene->rootOctave;
	clampGlobalsToUiRange();
	clampRootAndStepState();
	uiNeedsRendering(&instrumentClipView, kUiAllRows, 0xFFFFFFFF);
	return true;
}

void AcidSequencerMode::writeToFile(Serializer& writer, bool includeScenes) {
	writer.writeOpeningTagBeginning("acidSequencer");
	writer.writeAttribute("numSteps", kNumSteps);
	writer.writeAttribute("currentStep", currentStep_);
	writer.writeAttribute("type", type_);
	writer.writeAttribute("density", density_);
	writer.writeAttribute("rootSemitone", rootSemitone_);
	writer.writeAttribute("rootOctave", rootOctave_);
	writeStepArray(writer, "patternData", steps_);
	writer.closeTag();
	controlColumnState_.writeToFile(writer, includeScenes);
}

Error AcidSequencerMode::readFromFile(Deserializer& reader) {
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "numSteps")) {
			if (reader.readTagOrAttributeValueInt() != kNumSteps) {
				return Error::FILE_CORRUPTED;
			}
		}
		else if (!strcmp(tagName, "currentStep")) {
			currentStep_ = static_cast<uint8_t>(reader.readTagOrAttributeValueInt());
			if (currentStep_ >= kNumSteps) {
				currentStep_ = 0;
			}
		}
		else if (!strcmp(tagName, "type")) {
			type_ = static_cast<uint8_t>(reader.readTagOrAttributeValueInt());
		}
		else if (!strcmp(tagName, "density")) {
			density_ = static_cast<uint8_t>(reader.readTagOrAttributeValueInt());
		}
		else if (!strcmp(tagName, "rootSemitone")) {
			rootSemitone_ = static_cast<uint8_t>(reader.readTagOrAttributeValueInt() % 12);
		}
		else if (!strcmp(tagName, "rootOctave")) {
			rootOctave_ = static_cast<int8_t>(reader.readTagOrAttributeValueInt());
		}
		else if (!strcmp(tagName, "patternData") || !strcmp(tagName, "patternAData")) {
			readStepArray(reader.readTagOrAttributeValue(), steps_);
		}
		else if (!strcmp(tagName, "patternBData")) {
			reader.readTagOrAttributeValue();
		}
		else {
			break;
		}
	}
	clampGlobalsToUiRange();
	clampRootAndStepState();
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack = setupModelStackWithSong(modelStackMemory, currentSong);
	updateScaleNotes(modelStack->addTimelineCounter(getCurrentClip()));
	return Error::NONE;
}

bool AcidSequencerMode::copyFrom(SequencerMode* other) {
	if (!other) {
		return false;
	}
	auto* otherAcid = static_cast<AcidSequencerMode*>(other);
	steps_ = otherAcid->steps_;
	type_ = otherAcid->type_;
	density_ = otherAcid->density_;
	rootSemitone_ = otherAcid->rootSemitone_;
	rootOctave_ = otherAcid->rootOctave_;
	controlColumnState_ = otherAcid->controlColumnState_;
	return true;
}

} // namespace deluge::model::clip::sequencer::modes
