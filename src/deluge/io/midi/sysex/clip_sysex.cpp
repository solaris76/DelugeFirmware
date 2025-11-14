/*
 * Copyright © 2024 Synthstrom Audible Limited
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

#include "io/midi/sysex/clip_sysex.h"
#include "gui/views/session_view.h"
#include "io/midi/sysex/sysex_common.h"
#include "model/action/action_logger.h"
#include "model/clip/clip.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/instrument.h"
#include "model/instrument/kit.h"
#include "model/model_stack.h"
#include "model/note/note.h"
#include "model/note/note_row.h"
#include "model/scale/preset_scales.h"
#include "model/song/song.h"
#include "modulation/params/param_manager.h"
#include "playback/mode/session.h"
#include "processing/sound/sound_drum.h"
#include "storage/smsysex.h"
#include "storage/storage_manager.h"
#include "util/d_string.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>

extern JsonSerializer jWriter;
extern Song* currentSong;
extern SessionView sessionView;
extern ActionLogger actionLogger;
extern bool sdRoutineLock;

namespace {

const char* clipTypeToString(OutputType type) {
	switch (type) {
	case OutputType::SYNTH:
		return "synth";
	case OutputType::KIT:
		return "kit";
	case OutputType::MIDI_OUT:
		return "midi";
	case OutputType::CV:
		return "cv";
	case OutputType::AUDIO:
		return "audio";
	default:
		return "unknown";
	}
}

OutputType parseClipType(const char* value) {
	if (!value) {
		return OutputType::NONE;
	}

	String typeString;
	typeString.set(value);

	if (typeString.equalsCaseIrrespective("synth")) {
		return OutputType::SYNTH;
	}
	if (typeString.equalsCaseIrrespective("kit")) {
		return OutputType::KIT;
	}
	if (typeString.equalsCaseIrrespective("midi") || typeString.equalsCaseIrrespective("midi_out")) {
		return OutputType::MIDI_OUT;
	}
	if (typeString.equalsCaseIrrespective("cv")) {
		return OutputType::CV;
	}
	if (typeString.equalsCaseIrrespective("audio")) {
		return OutputType::AUDIO;
	}

	return OutputType::NONE;
}

bool ensureSongAvailable() {
	return currentSong != nullptr;
}

int32_t clampIndex(int32_t value, int32_t minValue, int32_t maxValue) {
	return std::max(minValue, std::min(maxValue, value));
}

uint32_t getClipId(Clip* clip) {
	return (uint32_t)(uintptr_t)clip;
}

uint32_t getTrackId(Output* output) {
	return output ? (uint32_t)(uintptr_t)output : 0;
}

Clip* findClipById(uint32_t clipId) {
	if (!currentSong) {
		return nullptr;
	}
	for (int32_t i = 0; i < currentSong->sessionClips.getNumElements(); i++) {
		Clip* clip = currentSong->sessionClips.getClipAtIndex(i);
		if (clip && getClipId(clip) == clipId) {
			return clip;
		}
	}
	return nullptr;
}

Output* findTrackById(uint32_t trackId) {
	if (!currentSong || trackId == 0) {
		return nullptr;
	}
	for (int32_t i = 0; i < currentSong->sessionClips.getNumElements(); i++) {
		Clip* clip = currentSong->sessionClips.getClipAtIndex(i);
		if (clip && clip->output && getTrackId(clip->output) == trackId) {
			return clip->output;
		}
	}
	return nullptr;
}

struct TrackSummary {
	Output* output = nullptr;
	uint32_t trackId = 0;
	std::vector<uint32_t> clipIds;
	std::vector<int32_t> clipIndices;
	bool anyClipArmed = false;
	bool anyClipActive = false;
};

TrackSummary* getOrCreateTrackSummary(std::vector<TrackSummary>& tracks, Output* output) {
	for (auto& track : tracks) {
		if (track.output == output) {
			return &track;
		}
	}
	tracks.push_back({});
	tracks.back().output = output;
	tracks.back().trackId = getTrackId(output);
	return &tracks.back();
}

void writeClipSummary(JsonSerializer& writer, Clip* clip, int32_t index) {
	writer.writeAttribute("index", index);
	writer.writeAttribute("clipId", (int32_t)getClipId(clip));
	writer.writeAttribute("trackId", (int32_t)getTrackId(clip->output));
	writer.writeAttribute("type", clipTypeToString(clip->output->type));
	writer.writeAttribute("section", (int32_t)clip->section);
	writer.writeAttribute("length", clip->loopLength);
	writer.writeAttribute("colour", (int32_t)clip->colourOffset);
	writer.writeAttribute("trackColour", (int32_t)clip->output->colour);
	writer.writeAttribute("name", clip->name.isEmpty() ? "" : clip->name.get());
	writer.writeAttribute("trackName", clip->output->name.isEmpty() ? "" : clip->output->name.get());
}

void writeTrackSummary(JsonSerializer& writer, const TrackSummary& summary) {
	writer.writeAttribute("trackId", (int32_t)summary.trackId);
	writer.writeAttribute("type", clipTypeToString(summary.output->type));
	writer.writeAttribute("colour", (int32_t)summary.output->colour);
	writer.writeAttribute("name", summary.output->name.isEmpty() ? "" : summary.output->name.get());
	writer.writeAttribute("clipCount", (int32_t)summary.clipIds.size());
	writer.writeAttribute("armed", summary.anyClipArmed ? 1 : 0);
	writer.writeAttribute("playing", summary.anyClipActive ? 1 : 0);

	writer.writeArrayStart("clipIds");
	for (uint32_t clipId : summary.clipIds) {
		writer.writeTag("clipId", (int32_t)clipId, false);
	}
	writer.writeArrayEnding("clipIds");

	writer.writeArrayStart("clipIndices");
	for (int32_t clipIndex : summary.clipIndices) {
		writer.writeTag("index", clipIndex, false);
	}
	writer.writeArrayEnding("clipIndices");
}

void writeErrorAndSend(MIDICable& cable, const char* status, const char* message) {
	SysexCommon::writeStatus(jWriter, status, message);
	SysexCommon::sendResponse(cable, jWriter);
}

bool guardSongAndCard(MIDICable& cable) {
	if (!ensureSongAvailable()) {
		SysexCommon::writeStatus(jWriter, "error", "No song loaded");
		SysexCommon::sendResponse(cable, jWriter);
		return false;
	}

	if (sdRoutineLock) {
		SysexCommon::writeStatus(jWriter, "error", "SD busy");
		SysexCommon::sendResponse(cable, jWriter);
		return false;
	}

	return true;
}

const char* presetErrorToString(Error error) {
	switch (error) {
	case Error::FILE_NOT_FOUND:
		return "fileNotFound";
	case Error::FILE_UNREADABLE:
		return "fileUnreadable";
	case Error::FILE_UNSUPPORTED:
		return "fileUnsupported";
	case Error::FILE_CORRUPTED:
		return "fileCorrupted";
	case Error::INSUFFICIENT_RAM:
		return "insufficientRam";
	case Error::SD_CARD:
	case Error::SD_CARD_NOT_PRESENT:
	case Error::SD_CARD_FULL:
	case Error::SD_CARD_NO_FILESYSTEM:
		return "sdError";
	default:
		return "failed";
	}
}

bool splitPresetPath(const String& path, String& dirPath, String& presetName) {
	const char* pathChars = path.get();
	if (!pathChars || !pathChars[0]) {
		return false;
	}

	const char* fullPath = pathChars;
	const char* lastSlash = strrchr(fullPath, '/');

	if (lastSlash) {
		int32_t dirLen = lastSlash - fullPath;
		if (dirLen <= 0) {
			dirPath.set("/");
		}
		else {
			dirPath.set(fullPath, dirLen);
		}
		fullPath = lastSlash + 1;
	}
	else {
		dirPath.set("");
	}

	if (!*fullPath) {
		return false;
	}

	const char* dot = strrchr(fullPath, '.');
	if (dot && dot != fullPath) {
		presetName.set(fullPath, dot - fullPath);
	}
	else {
		presetName.set(fullPath);
	}

	return true;
}

Error loadInstrumentPresetForClip(InstrumentClip* clip, OutputType outputType, FilePointer* filePointer,
                                  String* dirPath, String* presetName) {
	if (!clip || !filePointer || !dirPath || !presetName) {
		return Error::UNSPECIFIED;
	}

	Instrument* newInstrument = nullptr;
	Error error = StorageManager::loadInstrumentFromFile(currentSong, clip, outputType, false, &newInstrument,
	                                                     filePointer, presetName, dirPath);
	if (error != Error::NONE) {
		return error;
	}

	newInstrument->loadAllAudioFiles(true);

	if (outputType == OutputType::MIDI_OUT) {
		return clip->setInstrument(newInstrument, currentSong, nullptr, nullptr);
	}

	error = clip->setAudioInstrument(newInstrument, currentSong, true, nullptr);
	if (error == Error::NONE && outputType == OutputType::KIT) {
		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStackWithTimelineCounter* modelStack =
		    setupModelStackWithSong(modelStackMemory, currentSong)->addTimelineCounter(clip);
		clip->setupAsNewKitClipIfNecessary(modelStack);
	}
	return error;
}

Error loadSynthPresetIntoKitDrum(InstrumentClip* clip, int32_t drumIndex, FilePointer* filePointer, String* dirPath,
                                 String* presetName) {
	if (!clip || !filePointer || !dirPath || !presetName || drumIndex < 0) {
		return Error::UNSPECIFIED;
	}

	if (!clip->output || clip->output->type != OutputType::KIT) {
		return Error::UNSPECIFIED;
	}

	Kit* kit = (Kit*)clip->output;
	Drum* drum = kit->getDrumFromIndex(drumIndex);
	if (!drum || drum->type != DrumType::SOUND) {
		return Error::FILE_UNSUPPORTED;
	}

	SoundDrum* soundDrum = (SoundDrum*)drum;
	int32_t noteRowIndex = -1;
	NoteRow* noteRow = clip->getNoteRowForDrum(soundDrum, &noteRowIndex);
	if (!noteRow) {
		return Error::UNSPECIFIED;
	}

	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack =
	    setupModelStackWithSong(modelStackMemory, currentSong)->addTimelineCounter(clip);
	ModelStackWithNoteRow* noteRowStack = modelStack->addNoteRow(noteRowIndex, noteRow);
	noteRow->stopCurrentlyPlayingNote(noteRowStack);

	kit->drumsWithRenderingActive.deleteAtKey((int32_t)(Drum*)soundDrum);
	kit->removeDrum(soundDrum);

	SoundDrum* drumHandle = soundDrum;
	Error error =
	    StorageManager::loadSynthToDrum(currentSong, clip, false, &drumHandle, filePointer, presetName, dirPath);
	if (error != Error::NONE) {
		kit->addDrum(soundDrum);
		return error;
	}

	drumHandle->loadAllSamples(true);
	drumHandle->name.set(presetName);
	drumHandle->path.set(dirPath);

	ParamManager* paramManager = currentSong->getBackedUpParamManagerPreferablyWithClip(drumHandle, clip);
	if (!paramManager) {
		kit->addDrum(drumHandle);
		return Error::FILE_CORRUPTED;
	}

	kit->addDrum(drumHandle);
	noteRow->setDrum(drumHandle, kit, noteRowStack, clip, paramManager, false);
	kit->selectedDrum = drumHandle;
	kit->beenEdited();

	return Error::NONE;
}

Error loadPresetForClip(Clip* clip, OutputType outputType, const String& presetPath, int32_t drumIndex,
                        bool* appliedToDrum) {
	const char* presetChars = presetPath.get();
	if (!clip || !presetChars || !presetChars[0]) {
		return Error::NONE;
	}

	if (clip->type != ClipType::INSTRUMENT) {
		return Error::FILE_UNSUPPORTED;
	}

	FilePointer filePointer{0};
	if (!StorageManager::fileExists(presetPath.get(), &filePointer)) {
		return Error::FILE_NOT_FOUND;
	}

	String dirPath;
	String presetName;
	if (!splitPresetPath(presetPath, dirPath, presetName)) {
		return Error::FILE_UNREADABLE;
	}

	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	if (outputType == OutputType::KIT && drumIndex >= 0) {
		if (appliedToDrum) {
			*appliedToDrum = true;
		}
		return loadSynthPresetIntoKitDrum(instrumentClip, drumIndex, &filePointer, &dirPath, &presetName);
	}

	return loadInstrumentPresetForClip(instrumentClip, outputType, &filePointer, &dirPath, &presetName);
}

bool outputsAreCompatible(Clip* clip, Output* targetOutput) {
	if (!clip || !targetOutput) {
		return false;
	}

	bool clipIsAudio = (clip->type == ClipType::AUDIO);
	bool outputIsAudio = (targetOutput->type == OutputType::AUDIO);
	return clipIsAudio == outputIsAudio;
}

} // namespace

namespace ClipSysex {

void getClips(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clips");

	if (!guardSongAndCard(cable)) {
		return;
	}

	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("count", (int32_t)currentSong->sessionClips.getNumElements());
	jWriter.writeArrayStart("clips");

	for (int32_t i = 0; i < currentSong->sessionClips.getNumElements(); ++i) {
		Clip* clip = currentSong->sessionClips.getClipAtIndex(i);
		if (!clip) {
			continue;
		}

		jWriter.writeOpeningTagBeginning(nullptr, true, true);
		writeClipSummary(jWriter, clip, i);
		jWriter.closeTag(false);
	}

	jWriter.writeArrayEnding("clips");
	SysexCommon::sendResponse(cable, jWriter);
}

void getTracks(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^tracks");

	if (!guardSongAndCard(cable)) {
		return;
	}

	std::vector<TrackSummary> tracks;
	int32_t numClips = currentSong->sessionClips.getNumElements();
	tracks.reserve(numClips);

	for (int32_t i = 0; i < numClips; ++i) {
		Clip* clip = currentSong->sessionClips.getClipAtIndex(i);
		if (!clip || !clip->output) {
			continue;
		}

		TrackSummary* summary = getOrCreateTrackSummary(tracks, clip->output);
		summary->clipIds.push_back(getClipId(clip));
		summary->clipIndices.push_back(i);
		if (clip->armState != ArmState::OFF) {
			summary->anyClipArmed = true;
		}
		if (clip->activeIfNoSolo) {
			summary->anyClipActive = true;
		}
	}

	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("count", (int32_t)tracks.size());
	jWriter.writeArrayStart("tracks");
	for (auto& track : tracks) {
		jWriter.writeOpeningTagBeginning(nullptr, true, true);
		writeTrackSummary(jWriter, track);
		jWriter.closeTag(false);
	}
	jWriter.writeArrayEnding("tracks");
	SysexCommon::sendResponse(cable, jWriter);
}

void createClip(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clipCreated");

	if (!guardSongAndCard(cable)) {
		return;
	}

	OutputType desiredType = OutputType::NONE;
	int32_t position = currentSong->sessionClips.getNumElements();
	bool colourProvided = false;
	int32_t colourValue = 0;
	String presetPath;
	bool presetRequested = false;
	int32_t drumIndex = -1;

	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "type")) {
			String typeString;
			reader.readTagOrAttributeValueString(&typeString);
			desiredType = parseClipType(typeString.get());
		}
		else if (!strcmp(tagName, "position")) {
			position = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "colour") || !strcmp(tagName, "color")) {
			colourValue = reader.readTagOrAttributeValueInt();
			colourProvided = true;
		}
		else if (!strcmp(tagName, "presetPath")) {
			reader.readTagOrAttributeValueString(&presetPath);
			presetRequested = !presetPath.isEmpty();
		}
		else if (!strcmp(tagName, "drumIndex")) {
			drumIndex = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}
	reader.match('}');

	if (desiredType == OutputType::NONE) {
		writeErrorAndSend(cable, "error", "Unknown clip type");
		return;
	}

	int32_t clampedPosition = clampIndex(position, 0, currentSong->sessionClips.getNumElements());
	Clip* newClip = sessionView.createClipAtIndex(desiredType, clampedPosition);
	if (!newClip) {
		writeErrorAndSend(cable, "error", "Failed to create clip");
		return;
	}

	// SysEx-created tracks should default to the first section (top row) for consistency
	newClip->section = 0;

	int32_t newIndex = currentSong->sessionClips.getIndexForClip(newClip);
	if (colourProvided) {
		sessionView.setClipColour(newIndex, colourValue);
	}

	bool presetApplied = false;
	const char* presetError = nullptr;
	if (presetRequested) {
		if (desiredType == OutputType::AUDIO) {
			presetError = "unsupported";
		}
		else {
			Error presetResult = loadPresetForClip(newClip, desiredType, presetPath, drumIndex, nullptr);
			if (presetResult == Error::NONE) {
				presetApplied = true;
			}
			else {
				presetError = presetErrorToString(presetResult);
			}
		}
	}

	SysexCommon::writeStatus(jWriter, "success");
	writeClipSummary(jWriter, newClip, newIndex);
	if (presetRequested) {
		jWriter.writeAttribute("presetPath", presetPath.get());
		jWriter.writeAttribute("presetLoaded", presetApplied ? 1 : 0);
		if (desiredType == OutputType::KIT && drumIndex >= 0) {
			jWriter.writeAttribute("drumIndex", drumIndex);
		}
		if (!presetApplied && presetError) {
			jWriter.writeAttribute("presetError", presetError);
		}
	}
	SysexCommon::sendResponse(cable, jWriter);
}

void setClipColour(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clipColour");

	if (!guardSongAndCard(cable)) {
		return;
	}

	int32_t index = -1;
	int32_t colourValue = 0;

	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			index = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "colour") || !strcmp(tagName, "color")) {
			colourValue = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}

	if (index < 0) {
		writeErrorAndSend(cable, "error", "Index required");
		return;
	}

	if (!sessionView.setClipColour(index, colourValue)) {
		writeErrorAndSend(cable, "error", "Invalid clip index");
		return;
	}

	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("index", index);
	jWriter.writeAttribute("colour", colourValue);
	SysexCommon::sendResponse(cable, jWriter);
}

void enterClip(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clipEntered");

	if (!guardSongAndCard(cable)) {
		return;
	}

	int32_t index = -1;
	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			index = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}

	if (index < 0) {
		writeErrorAndSend(cable, "error", "Index required");
		return;
	}

	if (!sessionView.enterClipAtIndex(index)) {
		writeErrorAndSend(cable, "error", "Invalid clip index");
		return;
	}

	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("index", index);
	SysexCommon::sendResponse(cable, jWriter);
}

void exitClip(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clipExited");

	if (!guardSongAndCard(cable)) {
		return;
	}

	Clip* activeClip = currentSong->getCurrentClip();
	if (!activeClip) {
		writeErrorAndSend(cable, "error", "No active clip");
		return;
	}

	sessionView.transitionToSessionView();

	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("index", currentSong->sessionClips.getIndexForClip(activeClip));
	SysexCommon::sendResponse(cable, jWriter);
}

void deleteClip(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clipDeleted");

	if (!guardSongAndCard(cable)) {
		return;
	}

	int32_t index = -1;
	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			index = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}

	if (index < 0) {
		writeErrorAndSend(cable, "error", "Index required");
		return;
	}

	if (!sessionView.deleteClipAtIndex(index)) {
		writeErrorAndSend(cable, "error", "Invalid clip index");
		return;
	}

	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("index", index);
	SysexCommon::sendResponse(cable, jWriter);
}

void duplicateClip(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clipDuplicated");

	if (!guardSongAndCard(cable)) {
		return;
	}

	uint32_t sourceClipId = 0;
	int32_t sourceIndex = -1;
	int32_t insertPosition = -1;
	int32_t sectionOverride = -1;
	uint32_t targetTrackId = 0;
	bool trackSpecified = false;

	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "sourceClipId") || !strcmp(tagName, "clipId")) {
			sourceClipId = (uint32_t)reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "sourceIndex")) {
			sourceIndex = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "position")) {
			insertPosition = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "section")) {
			sectionOverride = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "targetTrackId") || !strcmp(tagName, "trackId")) {
			targetTrackId = (uint32_t)reader.readTagOrAttributeValueInt();
			trackSpecified = true;
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}
	reader.match('}');

	Clip* sourceClip = nullptr;
	if (sourceClipId) {
		sourceClip = findClipById(sourceClipId);
	}
	if (!sourceClip && sourceIndex >= 0 && sourceIndex < currentSong->sessionClips.getNumElements()) {
		sourceClip = currentSong->sessionClips.getClipAtIndex(sourceIndex);
	}

	if (!sourceClip) {
		writeErrorAndSend(cable, "error", "Source clip not found");
		return;
	}

	Output* targetOutput = sourceClip->output;
	if (trackSpecified) {
		targetOutput = findTrackById(targetTrackId);
		if (!targetOutput) {
			writeErrorAndSend(cable, "error", "Target track not found");
			return;
		}
	}

	if (!outputsAreCompatible(sourceClip, targetOutput)) {
		writeErrorAndSend(cable, "error", "Incompatible target track");
		return;
	}

	int32_t numClips = currentSong->sessionClips.getNumElements();
	int32_t sourcePosition = currentSong->sessionClips.getIndexForClip(sourceClip);
	if (sourcePosition < 0) {
		writeErrorAndSend(cable, "error", "Source clip unavailable");
		return;
	}

	int32_t insertIndex = (insertPosition >= 0) ? insertPosition : sourcePosition + 1;
	if (insertIndex < 0) {
		insertIndex = 0;
	}
	else if (insertIndex > numClips) {
		insertIndex = numClips;
	}

	uint8_t targetSection = (uint8_t)((sourceClip->section + 1) % kMaxNumSections);
	if (sectionOverride >= 0) {
		int32_t newSection = sectionOverride;
		if (newSection < 0) {
			newSection = 0;
		}
		else if (newSection >= (int32_t)kMaxNumSections) {
			newSection = kMaxNumSections - 1;
		}
		targetSection = (uint8_t)newSection;
	}

	if (!currentSong->sessionClips.ensureEnoughSpaceAllocated(1)) {
		writeErrorAndSend(cable, "error", "Insufficient memory");
		return;
	}

	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack =
	    setupModelStackWithSong(modelStackMemory, currentSong)->addTimelineCounter(sourceClip);

	Error cloneError = sourceClip->clone(modelStack);
	if (cloneError != Error::NONE) {
		writeErrorAndSend(cable, "error", "Clone failed");
		return;
	}

	Clip* newClip = (Clip*)modelStack->getTimelineCounter();
	newClip->section = targetSection;
	newClip->output = targetOutput;
	newClip->armState = ArmState::OFF;
	newClip->activeIfNoSolo = false;
	sessionView.copyClipName(sourceClip, newClip, targetOutput);

	currentSong->sessionClips.insertClipAtIndex(newClip, insertIndex);
	sessionView.redrawClipsOnScreen();

	int32_t newIndex = currentSong->sessionClips.getIndexForClip(newClip);

	SysexCommon::writeStatus(jWriter, "success");
	writeClipSummary(jWriter, newClip, newIndex);
	jWriter.writeAttribute("sourceIndex", sourcePosition);
	jWriter.writeAttribute("clipId", (int32_t)getClipId(newClip));
	jWriter.writeAttribute("trackId", (int32_t)getTrackId(newClip->output));
	SysexCommon::sendResponse(cable, jWriter);
}

void setTrackColour(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^trackColour");

	if (!guardSongAndCard(cable)) {
		return;
	}

	int32_t index = -1;
	int32_t colourValue = -1;

	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			index = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "colour") || !strcmp(tagName, "color")) {
			colourValue = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}

	if (index < 0 || colourValue < 0) {
		writeErrorAndSend(cable, "error", "Index and colour required");
		return;
	}

	Clip* clip = sessionView.getClipByIndex(index);
	if (!clip) {
		writeErrorAndSend(cable, "error", "Invalid clip index");
		return;
	}

	int32_t hue = colourValue % 192;
	if (hue < 0) {
		hue += 192;
	}
	clip->output->colour = (int16_t)hue;
	sessionView.redrawClipsOnScreen();

	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("index", index);
	jWriter.writeAttribute("colour", hue);
	SysexCommon::sendResponse(cable, jWriter);
}

void moveClip(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clipMoved");

	if (!guardSongAndCard(cable)) {
		return;
	}

	uint32_t clipId = 0;
	int32_t clipIndex = -1;
	int32_t newPosition = -1;
	int32_t sectionOverride = -1;
	uint32_t targetTrackId = 0;
	bool trackSpecified = false;

	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "clipId")) {
			clipId = (uint32_t)reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "index")) {
			clipIndex = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "position")) {
			newPosition = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "section")) {
			sectionOverride = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "trackId") || !strcmp(tagName, "targetTrackId")) {
			targetTrackId = (uint32_t)reader.readTagOrAttributeValueInt();
			trackSpecified = true;
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}
	reader.match('}');

	Clip* clip = nullptr;
	if (clipId) {
		clip = findClipById(clipId);
	}
	if (!clip && clipIndex >= 0 && clipIndex < currentSong->sessionClips.getNumElements()) {
		clip = currentSong->sessionClips.getClipAtIndex(clipIndex);
	}

	if (!clip) {
		writeErrorAndSend(cable, "error", "Clip not found");
		return;
	}

	Output* targetOutput = clip->output;
	if (trackSpecified) {
		targetOutput = findTrackById(targetTrackId);
		if (!targetOutput) {
			writeErrorAndSend(cable, "error", "Target track not found");
			return;
		}
	}

	if (!outputsAreCompatible(clip, targetOutput)) {
		writeErrorAndSend(cable, "error", "Incompatible track");
		return;
	}

	int32_t currentIndex = currentSong->sessionClips.getIndexForClip(clip);
	if (currentIndex < 0) {
		writeErrorAndSend(cable, "error", "Clip unavailable");
		return;
	}

	if (newPosition >= 0) {
		int32_t maxIndex = currentSong->sessionClips.getNumElements() - 1;
		if (maxIndex < 0) {
			maxIndex = 0;
		}
		int32_t destination = newPosition;
		if (destination < 0) {
			destination = 0;
		}
		else if (destination > maxIndex) {
			destination = maxIndex;
		}
		if (destination != currentIndex) {
			currentSong->sessionClips.repositionElement(currentIndex, destination);
			currentIndex = destination;
		}
	}

	if (sectionOverride >= 0) {
		int32_t newSection = sectionOverride;
		if (newSection < 0) {
			newSection = 0;
		}
		else if (newSection >= (int32_t)kMaxNumSections) {
			newSection = kMaxNumSections - 1;
		}
		clip->section = (uint8_t)newSection;
	}

	if (trackSpecified) {
		clip->output = targetOutput;
	}

	sessionView.redrawClipsOnScreen();

	int32_t refreshedIndex = currentSong->sessionClips.getIndexForClip(clip);
	SysexCommon::writeStatus(jWriter, "success");
	writeClipSummary(jWriter, clip, refreshedIndex);
	SysexCommon::sendResponse(cable, jWriter);
}

void launchSection(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^sectionLaunched");

	if (!guardSongAndCard(cable)) {
		return;
	}

	int32_t section = -1;

	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "section") || !strcmp(tagName, "index")) {
			section = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}
	reader.match('}');

	if (section < 0 || section >= kMaxNumSections) {
		writeErrorAndSend(cable, "error", "Section out of range");
		return;
	}

	session.armSection((uint8_t)section, 0);

	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("section", section);
	SysexCommon::sendResponse(cable, jWriter);
}

void getNotes(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^notes");

	if (!guardSongAndCard(cable)) {
		return;
	}

	uint32_t clipId = 0;
	int32_t clipIndex = -1;

	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "clipId")) {
			clipId = (uint32_t)reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "clipIndex") || !strcmp(tagName, "index")) {
			clipIndex = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}
	reader.match('}');

	Clip* clip = nullptr;
	if (clipId) {
		clip = findClipById(clipId);
	}
	if (!clip && clipIndex >= 0 && clipIndex < currentSong->sessionClips.getNumElements()) {
		clip = currentSong->sessionClips.getClipAtIndex(clipIndex);
	}

	if (!clip) {
		writeErrorAndSend(cable, "error", "Clip not found");
		return;
	}

	if (clip->type != ClipType::INSTRUMENT) {
		writeErrorAndSend(cable, "error", "Clip is not an instrument clip");
		return;
	}

	InstrumentClip* instrumentClip = (InstrumentClip*)clip;

	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(modelStackMemory);
	modelStack->setTimelineCounter(clip);

	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("clipId", (int32_t)getClipId(clip));
	jWriter.writeAttribute("length", (int32_t)clip->loopLength);
	jWriter.writeAttribute("scaleMode", instrumentClip->isScaleModeClip() ? 1 : 0);

	if (instrumentClip->isScaleModeClip()) {
		jWriter.writeAttribute("scaleType", (int32_t)instrumentClip->getScaleType());
	}

	jWriter.writeArrayStart("notes");

	uint32_t totalNotes = 0;
	for (int32_t rowIndex = 0; rowIndex < instrumentClip->noteRows.getNumElements(); rowIndex++) {
		NoteRow* noteRow = instrumentClip->noteRows.getElement(rowIndex);
		if (!noteRow) {
			continue;
		}

		int32_t noteRowId = instrumentClip->getNoteRowId(noteRow, rowIndex);
		int32_t y = noteRow->y;

		// Iterate through all notes in this row
		for (int32_t noteIndex = 0; noteIndex < noteRow->notes.getNumElements(); noteIndex++) {
			Note* note = noteRow->notes.getElement(noteIndex);
			if (!note) {
				continue;
			}

			// Generate stable note ID: rowId + pos (notes can't overlap in same row)
			uint64_t noteId = ((uint64_t)noteRowId << 32) | (uint32_t)note->pos;

			jWriter.writeOpeningTagBeginning(nullptr, true, true);
			jWriter.writeAttribute("noteId", (int32_t)(noteId & 0xFFFFFFFF));
			jWriter.writeAttribute("noteIdHigh", (int32_t)(noteId >> 32));
			jWriter.writeAttribute("rowId", noteRowId);
			jWriter.writeAttribute("y", y);
			jWriter.writeAttribute("start", (int32_t)note->pos);
			jWriter.writeAttribute("length", (int32_t)note->getLength());
			jWriter.writeAttribute("velocity", (int32_t)note->getVelocity());
			jWriter.closeTag(false);
			totalNotes++;
		}
	}

	jWriter.writeArrayEnding("notes");
	jWriter.writeAttribute("count", (int32_t)totalNotes);
	SysexCommon::sendResponse(cable, jWriter);
}

void setNotes(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^notesSet");

	if (!guardSongAndCard(cable)) {
		return;
	}

	uint32_t clipId = 0;
	int32_t clipIndex = -1;

	reader.match('{');
	char const* tagName;
	bool parsedNotes = false;
	bool hasNoteOps = false;

	Clip* clip = nullptr;
	if (clipId) {
		clip = findClipById(clipId);
	}
	if (!clip && clipIndex >= 0 && clipIndex < currentSong->sessionClips.getNumElements()) {
		clip = currentSong->sessionClips.getClipAtIndex(clipIndex);
	}

	if (!clip) {
		writeErrorAndSend(cable, "error", "Clip not found");
		return;
	}

	if (clip->type != ClipType::INSTRUMENT) {
		writeErrorAndSend(cable, "error", "Clip is not an instrument clip");
		return;
	}

	InstrumentClip* instrumentClip = (InstrumentClip*)clip;

	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(modelStackMemory);
	modelStack->setTimelineCounter(clip);

	Action* action = actionLogger.getNewAction(ActionType::NOTE_EDIT, ActionAddition::ALLOWED);

	int32_t successCount = 0;
	int32_t errorCount = 0;

	// Parse incoming object
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "clipId")) {
			clipId = (uint32_t)reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "clipIndex") || !strcmp(tagName, "index")) {
			clipIndex = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "notes")) {
			hasNoteOps = true;
			reader.match('[');
			while (reader.match('{')) {
				char const* noteTagName;
				String opString;
				const char* op = nullptr;
				uint64_t noteId = 0;
				int32_t rowId = -1;
				int32_t y = -32768;
				int32_t start = -1;
				int32_t length = -1;
				int32_t velocity = -1;

				while (*(noteTagName = reader.readNextTagOrAttributeName())) {
					if (!strcmp(noteTagName, "op")) {
						reader.readTagOrAttributeValueString(&opString);
						op = opString.get();
					}
					else if (!strcmp(noteTagName, "noteId")) {
						uint32_t low = (uint32_t)reader.readTagOrAttributeValueInt();
						noteId = low;
					}
					else if (!strcmp(noteTagName, "noteIdHigh")) {
						uint32_t high = (uint32_t)reader.readTagOrAttributeValueInt();
						noteId |= ((uint64_t)high << 32);
					}
					else if (!strcmp(noteTagName, "rowId")) {
						rowId = reader.readTagOrAttributeValueInt();
					}
					else if (!strcmp(noteTagName, "y")) {
						y = reader.readTagOrAttributeValueInt();
					}
					else if (!strcmp(noteTagName, "start") || !strcmp(noteTagName, "pos")) {
						start = reader.readTagOrAttributeValueInt();
					}
					else if (!strcmp(noteTagName, "length")) {
						length = reader.readTagOrAttributeValueInt();
					}
					else if (!strcmp(noteTagName, "velocity")) {
						velocity = reader.readTagOrAttributeValueInt();
					}
					else {
						reader.readTagOrAttributeValue();
					}
				}
				reader.match('}');

				if (!op) {
					errorCount++;
					continue;
				}

				NoteRow* noteRow = nullptr;
				ModelStackWithNoteRow* modelStackWithNoteRow = nullptr;

				if (rowId >= 0) {
					noteRow = instrumentClip->getNoteRowFromId(rowId);
					if (noteRow) {
						modelStackWithNoteRow = modelStack->addNoteRow(rowId, noteRow);
						rowId = modelStackWithNoteRow->noteRowId;
					}
				}
				else if (y != -32768) {
					modelStackWithNoteRow = instrumentClip->getOrCreateNoteRowForYNote(y, modelStack, action);
					if (modelStackWithNoteRow) {
						noteRow = modelStackWithNoteRow->getNoteRowAllowNull();
						rowId = modelStackWithNoteRow->noteRowId;
					}
				}

				if (!noteRow || !modelStackWithNoteRow) {
					errorCount++;
					continue;
				}

				if (!strcmp(op, "add") || !strcmp(op, "create")) {
					if (start < 0 || length <= 0 || velocity < 1 || velocity > 127) {
						errorCount++;
						continue;
					}

					int32_t result = noteRow->attemptNoteAdd(
					    start, length, velocity, noteRow->getDefaultProbability(), noteRow->getDefaultIterance(),
					    noteRow->getDefaultFill(modelStackWithNoteRow), modelStackWithNoteRow, action);
					if (result > 0) {
						successCount++;
					}
					else {
						errorCount++;
					}
				}
				else if (!strcmp(op, "update") || !strcmp(op, "edit")) {
					uint32_t notePos = (uint32_t)(noteId & 0xFFFFFFFF);
					int32_t noteIndex = noteRow->notes.search(notePos, GREATER_OR_EQUAL);
					if (noteIndex < noteRow->notes.getNumElements()) {
						Note* note = noteRow->notes.getElement(noteIndex);
						if (note && note->pos == (int32_t)notePos) {
							if (length > 0) {
								note->setLength(length);
							}
							if (velocity >= 1 && velocity <= 127) {
								note->setVelocity(velocity);
							}
							if (action) {
								action->recordNoteChange(instrumentClip, rowId, note, note->getLength(),
								                         note->getVelocity(), note->getProbability());
							}
							successCount++;
						}
						else {
							errorCount++;
						}
					}
					else {
						errorCount++;
					}
				}
				else if (!strcmp(op, "delete") || !strcmp(op, "remove")) {
					uint32_t notePos = (uint32_t)(noteId & 0xFFFFFFFF);
					noteRow->deleteNoteByPos(modelStackWithNoteRow, notePos, action);
					successCount++;
				}
				else {
					errorCount++;
				}
			}
			reader.match(']');
			parsedNotes = true;
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}
	reader.match('}');

	if (hasNoteOps && !parsedNotes) {
		errorCount++;
	}

	instrumentClip->expectEvent();
	sessionView.redrawClipsOnScreen();

	SysexCommon::writeStatus(jWriter, successCount > 0 ? "success" : "error");
	jWriter.writeAttribute("successCount", successCount);
	jWriter.writeAttribute("errorCount", errorCount);
	SysexCommon::sendResponse(cable, jWriter);
}

} // namespace ClipSysex
