/*
 * Copyright © 2024 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Deluge Firmware is free software: you can redistribute it and/or modify it under the
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

#include "io/midi/sysex/kit_sysex.h"
#include "gui/ui/ui.h"
#include "io/midi/sysex/sound_sysex_helpers.h"
#include "io/midi/sysex/sysex_common.h"
#include "memory/general_memory_allocator.h"
#include "model/clip/instrument_clip.h"
#include "model/drum/drum.h"
#include "model/drum/gate_drum.h"
#include "model/drum/midi_drum.h"
#include "model/instrument/kit.h"
#include "model/model_stack.h"
#include "model/note/note_row.h"
#include "model/sample/sample.h"
#include "model/song/song.h"
#include "modulation/params/param.h"
#include "modulation/params/param_collection.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/sound/sound.h"
#include "processing/sound/sound_drum.h"
#include "storage/audio/audio_file.h"
#include "storage/audio/audio_file_holder.h"
#include "storage/multi_range/multi_range.h"
#include "storage/smsysex.h"
#include <algorithm>
#include <cstring>
#include <new>

using namespace deluge::modulation::params;

extern JsonSerializer jWriter;
extern Song* currentSong;

namespace KitSysex {

// Track subscription state
static SysexCommon::SubscriberList kitSubscribers;

// Separate JsonSerializer for async notifications to avoid reentrancy
static JsonSerializer kitNotifyWriter;
static JsonSerializer drumParamNotifyWriter;

// Drum parameter subscription tracking
static SysexCommon::SubscriberList drumParamSubscribers;

// Helper to get the current kit from the active clip
static Kit* getCurrentKit() {
	if (!currentSong) {
		return nullptr;
	}

	Clip* clip = currentSong->getCurrentClip();
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		return nullptr;
	}

	Output* output = clip->output;
	if (!output || output->type != OutputType::KIT) {
		return nullptr;
	}

	return (Kit*)output;
}

// Helper to get drum type as string (distinguishes sample vs synth for SoundDrums)
static const char* getDrumTypeString(Drum* drum) {
	switch (drum->type) {
	case DrumType::SOUND: {
		// Check if it's sample-based or synth-based
		SoundDrum* soundDrum = (SoundDrum*)drum;
		// If the first oscillator is a sample type, it's an audio drum
		if (soundDrum->sources[0].oscType == OscType::SAMPLE) {
			return "audio";
		}
		else {
			return "synth";
		}
	}
	case DrumType::MIDI:
		return "midi";
	case DrumType::GATE:
		return "gate";
	default:
		return "unknown";
	}
}

// Get information about the current kit
void getKitInfo(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^kitInfo", false, true);

	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Kit name
	if (!kit->name.isEmpty()) {
		jWriter.writeAttribute("name", kit->name.get());
	}

	// Kit directory path
	if (!kit->dirPath.isEmpty()) {
		jWriter.writeAttribute("path", kit->dirPath.get());
	}

	// Count total drums
	int32_t drumCount = 0;
	for (Drum* drum = kit->firstDrum; drum; drum = drum->next) {
		drumCount++;
	}
	jWriter.writeAttribute("drumCount", drumCount);

	// Selected drum index
	if (kit->selectedDrum) {
		int32_t selectedIndex = kit->getDrumIndex(kit->selectedDrum);
		jWriter.writeAttribute("selectedDrumIndex", selectedIndex);
	}

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Get list of all drums in the kit
void getKitDrums(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^kitDrums", false, true);

	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Write array of drums
	jWriter.writeArrayStart("drums");

	int32_t index = 0;
	for (Drum* drum = kit->firstDrum; drum; drum = drum->next) {
		// Write anonymous object in array (no tag name)
		jWriter.writeOpeningTagBeginning(nullptr, true, true);

		jWriter.writeAttribute("index", index);
		jWriter.writeAttribute("type", getDrumTypeString(drum));

		// Get drum name
		char nameBuffer[8];
		nameBuffer[0] = '\0'; // Initialize
		drum->getName(nameBuffer);
		nameBuffer[7] = '\0'; // Ensure null termination
		jWriter.writeAttribute("name", nameBuffer);

		// MIDI input (note and channel)
		if (drum->midiInput.noteOrCC != 255) {
			jWriter.writeAttribute("midiNote", (int32_t)drum->midiInput.noteOrCC);
			jWriter.writeAttribute("midiChannel", (int32_t)drum->midiInput.channelOrZone);
		}

		// For SoundDrums, get sample info
		if (drum->type == DrumType::SOUND) {
			SoundDrum* soundDrum = (SoundDrum*)drum;
			// TODO: Add sample path info when we can access it safely
		}

		jWriter.closeTag(false); // box=false since writeOpeningTagBeginning already handled the opening brace
		index++;
	}

	jWriter.writeArrayEnding("drums");
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Get detailed information about a specific drum by index
void getDrumInfo(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^drumInfo", false, true);

	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Parse drum index from request
	int32_t drumIndex = -1;
	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			drumIndex = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (drumIndex < 0) {
		jWriter.writeAttribute("error", "Invalid drum index");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Drum* drum = kit->getDrumFromIndex(drumIndex);
	if (!drum) {
		jWriter.writeAttribute("error", "Drum not found");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	jWriter.writeAttribute("index", drumIndex);
	jWriter.writeAttribute("type", getDrumTypeString(drum));

	// Get drum name
	char nameBuffer[8];
	nameBuffer[0] = '\0'; // Initialize
	drum->getName(nameBuffer);
	nameBuffer[7] = '\0'; // Ensure null termination
	jWriter.writeAttribute("name", nameBuffer);

	// MIDI input
	if (drum->midiInput.noteOrCC != 255) {
		jWriter.writeAttribute("midiNote", (int32_t)drum->midiInput.noteOrCC);
		jWriter.writeAttribute("midiChannel", (int32_t)drum->midiInput.channelOrZone);
	}

	// MIDI mute command
	if (drum->muteMIDICommand.noteOrCC != 255) {
		jWriter.writeAttribute("muteNote", (int32_t)drum->muteMIDICommand.noteOrCC);
		jWriter.writeAttribute("muteChannel", (int32_t)drum->muteMIDICommand.channelOrZone);
	}

	// Arpeggiator settings
	jWriter.writeAttribute("arpMode", (int32_t)drum->arpSettings.mode);
	jWriter.writeAttribute("arpOctaves", drum->arpSettings.numOctaves);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Add a new drum to the kit
void addDrum(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^drumAdded", false, true);

	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Parse drum type from request
	String drumTypeStr;
	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "type")) {
			reader.readTagOrAttributeValueString(&drumTypeStr);
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	// Determine drum type
	DrumType drumType = DrumType::SOUND;
	const char* typeStr = drumTypeStr.get();
	if (!strcmp(typeStr, "midi")) {
		drumType = DrumType::MIDI;
	}
	else if (!strcmp(typeStr, "gate")) {
		drumType = DrumType::GATE;
	}
	else if (!strcmp(typeStr, "sound")) {
		drumType = DrumType::SOUND;
	}
	else {
		jWriter.writeAttribute("error", "Invalid drum type");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Create new drum
	Drum* newDrum = nullptr;
	switch (drumType) {
	case DrumType::SOUND: {
		void* memory = GeneralMemoryAllocator::get().allocMaxSpeed(sizeof(SoundDrum));
		if (!memory) {
			jWriter.writeAttribute("error", "Insufficient RAM");
			jWriter.closeTag(true);
			smSysex::sendMsg(cable, jWriter);
			return;
		}
		newDrum = new (memory) SoundDrum();
		break;
	}
	case DrumType::MIDI: {
		void* memory = GeneralMemoryAllocator::get().allocMaxSpeed(sizeof(MIDIDrum));
		if (!memory) {
			jWriter.writeAttribute("error", "Insufficient RAM");
			jWriter.closeTag(true);
			smSysex::sendMsg(cable, jWriter);
			return;
		}
		newDrum = new (memory) MIDIDrum();
		break;
	}
	case DrumType::GATE: {
		void* memory = GeneralMemoryAllocator::get().allocMaxSpeed(sizeof(GateDrum));
		if (!memory) {
			jWriter.writeAttribute("error", "Insufficient RAM");
			jWriter.closeTag(true);
			smSysex::sendMsg(cable, jWriter);
			return;
		}
		newDrum = new (memory) GateDrum();
		break;
	}
	}

	if (!newDrum) {
		jWriter.writeAttribute("error", "Failed to create drum");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Add drum to kit
	kit->addDrum(newDrum);

	// Setup ParamManager for SoundDrums
	ParamManagerForTimeline initialSoundParamManager;
	ParamManager* paramManagerForNewNoteRow = nullptr;

	if (drumType == DrumType::SOUND) {
		Error error = initialSoundParamManager.setupWithPatching();
		if (error == Error::NONE) {
			Sound::initParams(&initialSoundParamManager);
			SoundDrum* soundDrum = (SoundDrum*)newDrum;
			soundDrum->setupAsBlankSynth(&initialSoundParamManager);
			currentSong->backUpParamManager(soundDrum, currentSong->getCurrentClip(), &initialSoundParamManager, true);
			paramManagerForNewNoteRow = &initialSoundParamManager;
		}
	}

	// Get the current clip
	Clip* clip = currentSong->getCurrentClip();
	if (clip && clip->type == ClipType::INSTRUMENT) {
		InstrumentClip* instrumentClip = (InstrumentClip*)clip;

		// Create a new NoteRow for this drum at the end
		int32_t newNoteRowIndex = instrumentClip->noteRows.getNumElements();
		NoteRow* newNoteRow = instrumentClip->noteRows.insertNoteRowAtIndex(newNoteRowIndex);

		if (newNoteRow) {
			// Setup model stack
			char modelStackMemory[MODEL_STACK_MAX_SIZE];
			ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(modelStackMemory);
			ModelStackWithNoteRow* modelStackWithNoteRow = modelStack->addNoteRow(newNoteRowIndex, newNoteRow);

			// Associate drum with note row (using backed up param manager for SoundDrums)
			ParamManager* startingParamManager =
			    (paramManagerForNewNoteRow && paramManagerForNewNoteRow->containsAnyMainParamCollections())
			        ? paramManagerForNewNoteRow
			        : nullptr;
			newNoteRow->setDrum(newDrum, kit, modelStackWithNoteRow, nullptr, startingParamManager);
		}
	}

	// Mark kit as edited
	kit->beenEdited();

	// Get the index of the newly added drum
	int32_t newIndex = kit->getDrumIndex(newDrum);

	jWriter.writeAttribute("success", 1);
	jWriter.writeAttribute("index", newIndex);
	jWriter.writeAttribute("type", getDrumTypeString(newDrum));

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);

	// Trigger sidebar refresh to show the new drum row
	renderingNeededRegardlessOfUI(0, 0xFFFFFFFF);

	// Notify subscribers
	notifyDrumAdded(newIndex);
}

// Remove a drum from the kit by index
void removeDrum(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^drumRemoved", false, true);

	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Parse drum index from request
	int32_t drumIndex = -1;
	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			drumIndex = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (drumIndex < 0) {
		jWriter.writeAttribute("error", "Invalid drum index");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Drum* drum = kit->getDrumFromIndex(drumIndex);
	if (!drum) {
		jWriter.writeAttribute("error", "Drum not found");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Remove the drum
	kit->removeDrum(drum);

	// Delete the drum
	void* toDealloc = dynamic_cast<void*>(drum);
	drum->~Drum();
	delugeDealloc(toDealloc);

	// Mark kit as edited
	kit->beenEdited();

	jWriter.writeAttribute("success", 1);
	jWriter.writeAttribute("index", drumIndex);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);

	// Trigger sidebar refresh to show the drum removal
	renderingNeededRegardlessOfUI(0, 0xFFFFFFFF);

	// Notify subscribers
	notifyDrumRemoved(drumIndex);
}

// Set drum properties
void setDrumProperty(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^drumPropertySet", false, true);

	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Parse parameters
	int32_t drumIndex = -1;
	String propertyName;
	int32_t intValue = 0;
	bool hasIntValue = false;

	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			drumIndex = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "property")) {
			reader.readTagOrAttributeValueString(&propertyName);
		}
		else if (!strcmp(tagName, "value")) {
			intValue = reader.readTagOrAttributeValueInt();
			hasIntValue = true;
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (drumIndex < 0 || propertyName.isEmpty()) {
		jWriter.writeAttribute("error", "Missing parameters");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Drum* drum = kit->getDrumFromIndex(drumIndex);
	if (!drum) {
		jWriter.writeAttribute("error", "Drum not found");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	const char* propName = propertyName.get();
	bool success = false;

	// Handle different properties
	if (!strcmp(propName, "midiNote") && hasIntValue) {
		drum->midiInput.noteOrCC = (uint8_t)intValue;
		success = true;
	}
	else if (!strcmp(propName, "midiChannel") && hasIntValue) {
		drum->midiInput.channelOrZone = (uint8_t)intValue;
		success = true;
	}
	else if (!strcmp(propName, "muteNote") && hasIntValue) {
		drum->muteMIDICommand.noteOrCC = (uint8_t)intValue;
		success = true;
	}
	else if (!strcmp(propName, "muteChannel") && hasIntValue) {
		drum->muteMIDICommand.channelOrZone = (uint8_t)intValue;
		success = true;
	}
	else if (!strcmp(propName, "arpMode") && hasIntValue) {
		drum->arpSettings.mode = static_cast<ArpMode>(intValue);
		success = true;
	}
	else if (!strcmp(propName, "arpOctaves") && hasIntValue) {
		drum->arpSettings.numOctaves = intValue;
		success = true;
	}

	if (success) {
		// Mark kit as edited
		kit->beenEdited();

		jWriter.writeAttribute("success", 1);
		jWriter.writeAttribute("index", drumIndex);
		jWriter.writeAttribute("property", propName);
		jWriter.writeAttribute("value", intValue);

		// Trigger sidebar refresh to show the change
		renderingNeededRegardlessOfUI(0, 0xFFFFFFFF);

		// Notify subscribers
		notifyDrumChanged(drumIndex);
	}
	else {
		jWriter.writeAttribute("error", "Unknown property or invalid value");
	}

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Set sample file for a SoundDrum
void setDrumSample(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^drumSampleSet", false, true);

	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Parse parameters
	int32_t drumIndex = -1;
	String filePath;

	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			drumIndex = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "path")) {
			reader.readTagOrAttributeValueString(&filePath);
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (drumIndex < 0 || filePath.isEmpty()) {
		jWriter.writeAttribute("error", "Missing parameters");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Drum* drum = kit->getDrumFromIndex(drumIndex);
	if (!drum || drum->type != DrumType::SOUND) {
		jWriter.writeAttribute("error", "Drum not found or not a SoundDrum");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	SoundDrum* soundDrum = (SoundDrum*)drum;

	// Get the current clip to find the NoteRow
	Clip* clip = currentSong->getCurrentClip();
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		jWriter.writeAttribute("error", "No instrument clip selected");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	NoteRow* noteRow = instrumentClip->getNoteRowForDrum(drum);
	if (!noteRow) {
		jWriter.writeAttribute("error", "Drum has no NoteRow");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Setup as sample if not already
	if (soundDrum->sources[0].oscType != OscType::SAMPLE) {
		ParamManagerForTimeline* paramManager = &noteRow->paramManager;
		if (!paramManager->containsAnyMainParamCollections()) {
			Error error = paramManager->setupWithPatching();
			if (error != Error::NONE) {
				jWriter.writeAttribute("error", "Failed to setup ParamManager");
				jWriter.closeTag(true);
				smSysex::sendMsg(cable, jWriter);
				return;
			}
			Sound::initParams(paramManager);
		}
		soundDrum->setupAsSample(paramManager);
		currentSong->backUpParamManager(soundDrum, clip, paramManager, true);
	}

	// Get source and range
	Source* source = &soundDrum->sources[0];
	MultiRange* range = source->getOrCreateFirstRange();
	if (!range) {
		jWriter.writeAttribute("error", "Failed to create range");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Set the file path and load
	AudioFileHolder* holder = range->getAudioFileHolder();
	holder->setAudioFile(nullptr);
	holder->filePath.set(&filePath);

	Error loadError = holder->loadFile(false, true, true, CLUSTER_ENQUEUE, nullptr, false);
	if (loadError != Error::NONE) {
		jWriter.writeAttribute("error", "Failed to load sample file");
		jWriter.writeAttribute("loadError", (int32_t)loadError);
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Update drum name from filename
	const char* pathStr = filePath.get();
	const char* fileName = strrchr(pathStr, '/');
	if (!fileName) {
		fileName = pathStr;
	}
	else {
		fileName++; // Skip the '/'
	}

	String newName;
	newName.set(fileName);
	// Remove extension
	char* dot = strrchr(newName.get(), '.');
	if (dot) {
		*dot = '\0';
		newName.shorten((uint32_t)dot - (uint32_t)newName.get());
	}

	if (!newName.isEmpty()) {
		// Make name unique if needed
		if (kit->getDrumFromName(newName.get())) {
			kit->makeDrumNameUnique(&newName, 2);
		}
		soundDrum->name.set(&newName);
	}

	// Set repeat mode based on sample length
	if (holder->audioFile) {
		Sample* sample = (Sample*)holder->audioFile;
		source->repeatMode = (sample->getLengthInMSec() < 2002) ? SampleRepeatMode::ONCE : SampleRepeatMode::CUT;
	}

	kit->beenEdited();

	jWriter.writeAttribute("success", 1);
	jWriter.writeAttribute("index", drumIndex);
	jWriter.writeAttribute("path", filePath.get());
	if (!newName.isEmpty()) {
		jWriter.writeAttribute("name", newName.get());
	}

	// Trigger UI refresh
	renderingNeededRegardlessOfUI(0, 0xFFFFFFFF);

	// Notify subscribers
	notifyDrumChanged(drumIndex);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Get all parameters for a drum
void getDrumParameters(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^drumParameters", false, true);

	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Parse drum index from request
	int32_t drumIndex = -1;
	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			drumIndex = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (drumIndex < 0) {
		jWriter.writeAttribute("error", "Invalid drum index");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Drum* drum = kit->getDrumFromIndex(drumIndex);
	if (!drum || drum->type != DrumType::SOUND) {
		jWriter.writeAttribute("error", "Drum not found or not a SoundDrum");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	SoundDrum* soundDrum = (SoundDrum*)drum;

	// Get the NoteRow for this drum to access its ParamManager
	Clip* clip = currentSong->getCurrentClip();
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		jWriter.writeAttribute("error", "No instrument clip selected");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	int32_t noteRowIndex;
	NoteRow* noteRow = instrumentClip->getNoteRowForDrum(drum, &noteRowIndex);
	if (!noteRow) {
		jWriter.writeAttribute("error", "Drum has no NoteRow");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	ParamManagerForTimeline* paramManager = &noteRow->paramManager;

	// Setup model stack
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(modelStackMemory);
	ModelStackWithNoteRow* modelStackWithNoteRow =
	    modelStack->addNoteRow(instrumentClip->getNoteRowId(noteRow, noteRowIndex), noteRow);
	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStackWithNoteRow->addOtherTwoThings(soundDrum, paramManager);

	jWriter.writeAttribute("index", drumIndex);
	if (!soundDrum->name.isEmpty()) {
		jWriter.writeAttribute("name", soundDrum->name.get());
	}

	SoundSysex::writeSoundParameterSnapshot(jWriter, soundDrum, paramManager);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Set a parameter for a drum
void setDrumParameter(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^drumParameterSet", false, true);

	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Parse parameters
	int32_t drumIndex = -1;
	String paramName;
	int32_t intValue = 0;

	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			drumIndex = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "name")) {
			reader.readTagOrAttributeValueString(&paramName);
		}
		else if (!strcmp(tagName, "value")) {
			intValue = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (drumIndex < 0 || paramName.isEmpty()) {
		jWriter.writeAttribute("error", "Missing parameters");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Drum* drum = kit->getDrumFromIndex(drumIndex);
	if (!drum || drum->type != DrumType::SOUND) {
		jWriter.writeAttribute("error", "Drum not found or not a SoundDrum");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	SoundDrum* soundDrum = (SoundDrum*)drum;

	// Get the NoteRow for this drum
	Clip* clip = currentSong->getCurrentClip();
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		jWriter.writeAttribute("error", "No instrument clip selected");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	int32_t noteRowIndex;
	NoteRow* noteRow = instrumentClip->getNoteRowForDrum(drum, &noteRowIndex);
	if (!noteRow) {
		jWriter.writeAttribute("error", "Drum has no NoteRow");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	ParamManagerForTimeline* paramManager = &noteRow->paramManager;

	// Setup model stack
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(modelStackMemory);
	ModelStackWithNoteRow* modelStackWithNoteRow =
	    modelStack->addNoteRow(instrumentClip->getNoteRowId(noteRow, noteRowIndex), noteRow);
	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStackWithNoteRow->addOtherTwoThings(soundDrum, paramManager);

	const char* name = paramName.get();
	bool success = false;
	const char* errorMsg = "Unknown parameter";

	// Try unpatched params first
	if (paramManager->summaries[0].paramCollection) {
		UnpatchedParamSet* unpatchedParams = (UnpatchedParamSet*)paramManager->summaries[0].paramCollection;
		ParamCollection* unpatchedParamCollection = paramManager->summaries[0].paramCollection;

		for (int32_t p = 0; p < UNPATCHED_SOUND_MAX_NUM; p++) {
			const char* checkName = paramNameForFile(Kind::UNPATCHED_SOUND, p + UNPATCHED_START);
			if (checkName && !strcmp(name, checkName)) {
				AutoParam* param = &unpatchedParams->params[p];
				ModelStackWithAutoParam* modelStackWithParam = modelStackWithThreeMainThings->addParam(
				    unpatchedParamCollection, &paramManager->summaries[0], p, param);
				param->setCurrentValueInResponseToUserInput(intValue, modelStackWithParam);
				success = true;
				break;
			}
		}
	}

	// Try patched params if not found
	if (!success && paramManager->summaries[1].paramCollection) {
		PatchedParamSet* patchedParams = (PatchedParamSet*)paramManager->summaries[1].paramCollection;
		ParamCollection* patchedParamCollection = paramManager->summaries[1].paramCollection;

		for (int32_t p = 0; p < kNumParams; p++) {
			const char* checkName = paramNameForFile(Kind::PATCHED, p);
			if (checkName && !strcmp(name, checkName)) {
				AutoParam* param = &patchedParams->params[p];
				ModelStackWithAutoParam* modelStackWithParam = modelStackWithThreeMainThings->addParam(
				    patchedParamCollection, &paramManager->summaries[1], p, param);
				param->setCurrentValueInResponseToUserInput(intValue, modelStackWithParam);
				success = true;
				break;
			}
		}
	}

	if (success) {
		jWriter.writeAttribute("name", name);
		jWriter.writeAttribute("value", intValue);
		jWriter.writeAttribute("success", 1);
		jWriter.writeAttribute("index", drumIndex);

		// Trigger UI refresh
		uiNeedsRendering(getCurrentUI());

		// Notify subscribers
		notifyDrumChanged(drumIndex);
	}
	else {
		jWriter.writeAttribute("error", errorMsg);
		jWriter.writeAttribute("name", name);
	}

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

// Create a new empty kit
void createKit(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^kitCreated");
	SysexCommon::writeStatus(jWriter, "error", "Kit creation via SysEx not yet supported");
	jWriter.writeAttribute("supported", (int32_t)0);
	SysexCommon::sendResponse(cable, jWriter);
}

// Load an existing kit by name
void loadKit(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^kitLoaded");
	SysexCommon::writeStatus(jWriter, "error", "Kit loading via SysEx not yet supported");
	jWriter.writeAttribute("supported", (int32_t)0);
	SysexCommon::sendResponse(cable, jWriter);
}

// Save the current kit
void saveKit(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^kitSaved");
	SysexCommon::writeStatus(jWriter, "error", "Kit saving via SysEx not yet supported");
	jWriter.writeAttribute("supported", (int32_t)0);
	SysexCommon::sendResponse(cable, jWriter);
}

// Subscribe to kit changes
void subscribeKit(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	SysexCommon::startResponse(jWriter, reader, "^kitSubscribed");

	switch (kitSubscribers.add(&cable)) {
	case SysexCommon::SubscriberList::AddResult::ALREADY_PRESENT:
		SysexCommon::writeStatus(jWriter, "already");
		break;
	case SysexCommon::SubscriberList::AddResult::ADDED:
		SysexCommon::writeStatus(jWriter, "success");
		jWriter.writeAttribute("count", (int32_t)kitSubscribers.size());
		break;
	case SysexCommon::SubscriberList::AddResult::FULL:
		SysexCommon::writeStatus(jWriter, "error", "Max subscribers reached");
		break;
	}

	jWriter.writeAttribute("subscribed", (int32_t)(kitSubscribers.contains(&cable) ? 1 : 0));
	SysexCommon::sendResponse(cable, jWriter);
}

// Unsubscribe from kit changes
void unsubscribeKit(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	SysexCommon::startResponse(jWriter, reader, "^kitUnsubscribed");

	if (kitSubscribers.remove(&cable)) {
		SysexCommon::writeStatus(jWriter, "success");
	}
	else {
		SysexCommon::writeStatus(jWriter, "not_subscribed");
	}

	jWriter.writeAttribute("subscribed", (int32_t)0);
	SysexCommon::sendResponse(cable, jWriter);
}

// Subscribe to drum parameter changes
void subscribeDrumParameters(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^drumParametersSubscribed");

	switch (drumParamSubscribers.add(&cable)) {
	case SysexCommon::SubscriberList::AddResult::ALREADY_PRESENT:
		SysexCommon::writeStatus(jWriter, "already");
		jWriter.writeAttribute("subscribed", (int32_t)1);
		break;
	case SysexCommon::SubscriberList::AddResult::ADDED:
		SysexCommon::writeStatus(jWriter, "success");
		jWriter.writeAttribute("subscribed", (int32_t)1);
		jWriter.writeAttribute("count", (int32_t)drumParamSubscribers.size());
		break;
	case SysexCommon::SubscriberList::AddResult::FULL:
		SysexCommon::writeStatus(jWriter, "error", "Max subscribers reached");
		jWriter.writeAttribute("subscribed", (int32_t)0);
		SysexCommon::sendResponse(cable, jWriter);
		return;
	}

	SysexCommon::sendResponse(cable, jWriter);
}

// Unsubscribe from drum parameter changes
void unsubscribeDrumParameters(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^drumParametersUnsubscribed");

	if (drumParamSubscribers.remove(&cable)) {
		SysexCommon::writeStatus(jWriter, "success");
	}
	else {
		SysexCommon::writeStatus(jWriter, "not_subscribed");
	}

	jWriter.writeAttribute("subscribed", (int32_t)0);
	SysexCommon::sendResponse(cable, jWriter);
}

// Notify subscribers when a drum is added
void notifyDrumAdded(int32_t drumIndex) {
	if (kitSubscribers.size() == 0) {
		return;
	}

	kitSubscribers.forEach([&](MIDICable& destination) {
		kitNotifyWriter.reset();
		kitNotifyWriter.setMemoryBased();
		smSysex::startDirect(kitNotifyWriter);
		kitNotifyWriter.writeOpeningTag("^drumAdded", false, true);
		kitNotifyWriter.writeAttribute("index", drumIndex);
		kitNotifyWriter.closeTag(true);
		smSysex::sendMsg(destination, kitNotifyWriter);
	});
}

// Notify subscribers when a drum is removed
void notifyDrumRemoved(int32_t drumIndex) {
	if (kitSubscribers.size() == 0) {
		return;
	}

	kitSubscribers.forEach([&](MIDICable& destination) {
		kitNotifyWriter.reset();
		kitNotifyWriter.setMemoryBased();
		smSysex::startDirect(kitNotifyWriter);
		kitNotifyWriter.writeOpeningTag("^drumRemoved", false, true);
		kitNotifyWriter.writeAttribute("index", drumIndex);
		kitNotifyWriter.closeTag(true);
		smSysex::sendMsg(destination, kitNotifyWriter);
	});
}

// Notify subscribers when a drum property changes
void notifyDrumChanged(int32_t drumIndex) {
	if (kitSubscribers.size() == 0) {
		return;
	}

	kitSubscribers.forEach([&](MIDICable& destination) {
		kitNotifyWriter.reset();
		kitNotifyWriter.setMemoryBased();
		smSysex::startDirect(kitNotifyWriter);
		kitNotifyWriter.writeOpeningTag("^drumChanged", false, true);
		kitNotifyWriter.writeAttribute("index", drumIndex);
		kitNotifyWriter.closeTag(true);
		smSysex::sendMsg(destination, kitNotifyWriter);
	});
}

// Notify subscribers when the kit itself changes (load/create)
void notifyKitChanged() {
	if (kitSubscribers.size() == 0) {
		return;
	}

	kitSubscribers.forEach([&](MIDICable& destination) {
		kitNotifyWriter.reset();
		kitNotifyWriter.setMemoryBased();
		smSysex::startDirect(kitNotifyWriter);
		kitNotifyWriter.writeOpeningTag("^kitChanged", false, true);
		kitNotifyWriter.closeTag(true);
		smSysex::sendMsg(destination, kitNotifyWriter);
	});
}

void notifyDrumParameterChanged(int32_t drumIndex, char const* paramName, int32_t value) {
	if (drumParamSubscribers.size() == 0 || !paramName) {
		return;
	}

	drumParamSubscribers.forEach([&](MIDICable& destination) {
		drumParamNotifyWriter.reset();
		drumParamNotifyWriter.setMemoryBased();
		smSysex::startDirect(drumParamNotifyWriter);
		drumParamNotifyWriter.writeOpeningTag("^drumParameterChanged", false, true);
		drumParamNotifyWriter.writeAttribute("index", drumIndex);
		drumParamNotifyWriter.writeAttribute("name", paramName);
		drumParamNotifyWriter.writeAttribute("value", value);
		drumParamNotifyWriter.closeTag(true);
		smSysex::sendMsg(destination, drumParamNotifyWriter);
	});
}

} // namespace KitSysex
