/*
 * Copyright © 2018-2023 Synthstrom Audible Limited
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

#include "model/drum/midi_drum.h"
#include "gui/ui/ui.h"
#include "gui/views/automation_view.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "io/midi/midi_device_helper.h"
#include "io/midi/midi_engine.h"
#include "model/clip/instrument_clip.h"
#include "model/clip/instrument_clip_minder.h"
#include "model/drum/non_audio_drum.h"
#include "model/instrument/kit.h"
#include "modulation/midi/midi_param_collection.h"
#include "modulation/params/param_set.h"
#include "storage/storage_manager.h"
#include "util/string.h"

#include <string.h>

MIDIDrum::MIDIDrum() : NonAudioDrum(DrumType::MIDI), modKnobCCAssignments() {
	channel = 0;
	note = 0;
	// Initialize all mod knob CC assignments to NONE
	modKnobCCAssignments.fill(CC_NUMBER_NONE);
}

void MIDIDrum::noteOn(ModelStackWithThreeMainThings* modelStack, uint8_t velocity, int16_t const* mpeValues,
                      int32_t fromMIDIChannel, uint32_t sampleSyncLength, int32_t ticksLate, uint32_t samplesLate) {
	ArpeggiatorSettings* arpSettings = getArpSettings();
	ArpReturnInstruction instruction;
	// Run everything by the Arp...
	arpeggiator.noteOn(arpSettings, note, velocity, &instruction, fromMIDIChannel, mpeValues);
	if (instruction.arpNoteOn != nullptr) {
		for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
			if (instruction.arpNoteOn->noteCodeOnPostArp[n] == ARP_NOTE_NONE) {
				break;
			}
			noteOnPostArp(instruction.arpNoteOn->noteCodeOnPostArp[n], instruction.arpNoteOn, n);
			instruction.arpNoteOn->noteStatus[n] = ArpNoteStatus::PLAYING;
		}
	}
}

void MIDIDrum::noteOff(ModelStackWithThreeMainThings* modelStack, int32_t velocity) {
	ArpeggiatorSettings* arpSettings = getArpSettings();
	ArpReturnInstruction instruction;
	// Run everything by the Arp...
	arpeggiator.noteOff(arpSettings, note, &instruction);
	for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
		if (instruction.glideNoteCodeOffPostArp[n] == ARP_NOTE_NONE) {
			break;
		}
		noteOffPostArp(instruction.glideNoteCodeOffPostArp[n]);
	}
	for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
		if (instruction.noteCodeOffPostArp[n] == ARP_NOTE_NONE) {
			break;
		}
		noteOffPostArp(instruction.noteCodeOffPostArp[n]);
	}
}

void MIDIDrum::noteOnPostArp(int32_t noteCodePostArp, ArpNote* arpNote, int32_t noteIndex) {
	NonAudioDrum::noteOnPostArp(noteCodePostArp, arpNote, noteIndex);
	lastVelocity = arpNote->velocity;

	midiEngine.sendNote(this, true, noteCodePostArp, arpNote->velocity, channel, kMIDIOutputFilterNoMPE, outputDevice);
}

void MIDIDrum::noteOffPostArp(int32_t noteCodePostArp) {
	NonAudioDrum::noteOffPostArp(noteCodePostArp);

	midiEngine.sendNote(this, false, noteCodePostArp, kDefaultNoteOffVelocity, channel, kMIDIOutputFilterNoMPE,
	                    outputDevice);
}

void MIDIDrum::killAllVoices() {
	if (hasActiveVoices()) {
		noteOff(nullptr);
	}
	arpeggiator.reset();
}

void MIDIDrum::writeToFile(Serializer& writer, bool savingSong, ParamManager* paramManager) {
	writer.writeOpeningTagBeginning("midiOutput", true);
	writeDrumTagsToFile(writer);
	writer.writeAttribute("channel", channel, false);
	writer.writeAttribute("note", note, false);

	// Save MIDI output device selection (index + name for reliable matching)
	deluge::io::midi::writeDeviceToFile(writer, outputDevice, outputDeviceName);

	writer.writeOpeningTagEnd();

	NonAudioDrum::writeArpeggiatorToFile(writer);

	// Write mod knob CC assignments if any are set
	writeModKnobAssignmentsToFile(writer);

	// Write device definition (CC labels)
	writeDeviceDefinitionFile(writer, true);

	if (savingSong) {
		Drum::writeMIDICommandsToFile(writer);
	}
	writer.writeClosingTag("midiOutput", true, true);
}

Error MIDIDrum::readFromFile(Deserializer& reader, Song* song, Clip* clip, int32_t readAutomationUpToPos) {
	char const* tagName;
	uint8_t savedOutputDevice = 0;
	String savedDeviceName;

	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "note")) {
			note = reader.readTagOrAttributeValueInt();
			reader.exitTag("note");
		}
		else if (!strcmp(tagName, "outputDeviceName")) {
			reader.readTagOrAttributeValueString(&savedDeviceName);
			outputDeviceName.set(&savedDeviceName);
			reader.exitTag("outputDeviceName");
		}
		else if (!strcmp(tagName, "outputDevice")) {
			savedOutputDevice = static_cast<uint8_t>(reader.readTagOrAttributeValueInt());
			outputDevice = savedOutputDevice;
			reader.exitTag("outputDevice");
		}
		else if (!strcmp(tagName, "modKnobs")) {
			Error error = readModKnobAssignmentsFromFile(reader, readAutomationUpToPos);
			if (error != Error::NONE) {
				return error;
			}
			reader.exitTag();
		}
		else if (!strcmp(tagName, "midiDevice")) {
			readDeviceDefinitionFile(reader, true);
			reader.exitTag();
		}
		else if (NonAudioDrum::readDrumTagFromFile(reader, tagName)) {}
		else {
			reader.exitTag(tagName);
		}
	}

	// Match device by name first (more reliable), fall back to index
	if (!savedDeviceName.isEmpty()) {
		outputDevice = deluge::io::midi::findDeviceIndexByName(savedDeviceName.get(), savedOutputDevice);
		outputDeviceName.set(&savedDeviceName);
	}
	else {
		outputDevice = savedOutputDevice;
	}

	return Error::NONE;
}

std::string MIDIDrum::getDrumName() {
	int32_t channelToDisplay = channel + 1;
	std::string buffer;

	if (display->haveOLED()) {
		if (!drumName.empty()) {
			buffer.append(drumName);
			buffer.append("\n");
		}
		buffer.append("MIDI CH");
		buffer.append(deluge::string::fromInt(channelToDisplay));
		buffer.append(": ");
		buffer.append(deluge::string::fromInt(note));
		buffer.append(" (");
		char noteLabel[5];
		noteCodeToString(note, noteLabel);
		buffer.append(noteLabel);
		buffer.append(")");
	}
	else {

		// Old 7-seg logic adapted to std::string.
		if (channelToDisplay < 10 && note < 100) {
			buffer = " ";
		}

		// If can't fit everything on display, show channel as hexadecimal
		if (channelToDisplay >= 10 && note >= 100) {
			buffer.push_back('A' + channelToDisplay - 10);
		}
		else {
			buffer.append(deluge::string::fromInt(channelToDisplay));
		}

		buffer.push_back('.');

		if (note < 10 && channelToDisplay < 100) {
			buffer.push_back(' ');
		}
		buffer.append(deluge::string::fromInt(note));
	}

	return buffer;
}

// NOTE: modEncoderAction override removed - gold knobs now always control MIDI CC automation
// via the normal View::modEncoderAction flow. Note, channel, and velocity are set via menu.

void MIDIDrum::expressionEvent(int32_t newValue, int32_t expressionDimension) {

	// Aftertouch only
	if (expressionDimension == 2) {
		int32_t value7 = newValue >> 24;
		// Note: use the note code currently on post-arp, because this drum supports "Chord Simulator" and "Octaves" and
		// the note code could be different
		midiEngine.sendPolyphonicAftertouch(this, channel, value7, arpeggiator.active_note.noteCodeOnPostArp[0],
		                                    kMIDIOutputFilterNoMPE);
	}
}

void MIDIDrum::polyphonicExpressionEventOnChannelOrNote(int32_t newValue, int32_t expressionDimension,
                                                        int32_t channelOrNoteNumber,
                                                        MIDICharacteristic whichCharacteristic) {
	// Because this is a Drum, we disregard the noteCode (which is what channelOrNoteNumber always is in our case - but
	// yeah, that's all irrelevant.
	expressionEvent(newValue, expressionDimension);
}

// Handle mod encoder button press for CC assignment (similar to MIDIInstrument)
bool MIDIDrum::modEncoderButtonAction(uint8_t whichModEncoder, bool on, ModelStackWithThreeMainThings* modelStack) {

	if (on) {
		// Allow CC assignment in normal mode (not when in other UI modes)
		// Gold knobs always control MIDI CC automation - press encoder to assign different CC
		if (currentUIMode == UI_MODE_NONE) {

			if (getCurrentUI()->toClipMinder()) {
				currentUIMode = UI_MODE_SELECTING_MIDI_CC;

				// Get current CC assignment for this knob
				int32_t cc = modKnobCCAssignments[this->modKnobMode * kNumPhysicalModKnobs + whichModEncoder];

				bool automationExists = doesAutomationExistOnMIDIParam(modelStack, cc);
				InstrumentClipMinder::editingMIDICCForWhichModKnob = whichModEncoder;
				InstrumentClipMinder::drawMIDIControlNumber(cc, automationExists);
				return true;
			}
			else {
				return false;
			}
		}
		else {
			return false;
		}
	}

	// De-press
	else {
		if (currentUIMode == UI_MODE_SELECTING_MIDI_CC) {
			currentUIMode = UI_MODE_NONE;
			if (display->haveOLED()) {
				deluge::hid::display::OLED::removePopup();
			}
			else {
				InstrumentClipMinder::redrawNumericDisplay();
			}
		}
		return false;
	}
}

// Get MIDI CC parameter for automation (similar to MIDIInstrument)
ModelStackWithAutoParam* MIDIDrum::getParamToControlFromInputMIDIChannel(int32_t cc,
                                                                         ModelStackWithThreeMainThings* modelStack) {

	if (!modelStack->paramManager) {
noParam:
		return modelStack->addParamCollectionAndId(nullptr, nullptr, 0)->addAutoParam(nullptr); // "No param"
	}

	ParamCollectionSummary* summary;
	int32_t paramId = cc;

	switch (cc) {
	case CC_NUMBER_PITCH_BEND:
		paramId = 0;
		goto expressionParam;
	case CC_NUMBER_Y_AXIS:
		paramId = 1;
		goto expressionParam;

	case CC_NUMBER_AFTERTOUCH:
		paramId = 2;
expressionParam:
		modelStack->paramManager->ensureExpressionParamSetExists(); // Allowed to fail
		summary = modelStack->paramManager->getExpressionParamSetSummary();
		if (!summary->paramCollection) {
			goto noParam;
		}
		break;

	case CC_NUMBER_NONE:
		goto noParam;

	default:
		// For MIDI CC parameters, ensure MIDIParamCollection exists
		// This is critical for kit rows where ParamManager may not have MIDI params yet
		if (!modelStack->paramManager->containsAnyParamCollectionsIncludingExpression()) {
			// Need to set up MIDI param collection for this drum's note row
			Error error = modelStack->paramManager->setupMIDI();
			if (error != Error::NONE) {
				goto noParam;
			}
		}
		summary = modelStack->paramManager->getMIDIParamCollectionSummary();
		break;
	}

	ModelStackWithParamId* modelStackWithParamId =
	    modelStack->addParamCollectionAndId(summary->paramCollection, summary, paramId);

	return summary->paramCollection->getAutoParamFromId(
	    modelStackWithParamId,
	    true); // Yes we do want to force creating it even if we're not recording - so the level indicator can update
	           // for the user
}

// Get param from mod encoder (for gold knob automation)
ModelStackWithAutoParam* MIDIDrum::getParamFromModEncoder(int32_t whichModEncoder,
                                                          ModelStackWithThreeMainThings* modelStack,
                                                          bool allowCreation) {

	if (!modelStack->paramManager) {
		return modelStack->addParamCollectionAndId(nullptr, nullptr, 0)->addAutoParam(nullptr); // "No param"
	}

	// NOTE: Gold knobs always control MIDI CC, whether auditioning or not
	// LED indicators should always show current CC values

	int32_t paramId = modKnobCCAssignments[this->modKnobMode * kNumPhysicalModKnobs + whichModEncoder];

	return getParamToControlFromInputMIDIChannel(paramId, modelStack);
}

// Check if automation exists on a MIDI CC parameter
bool MIDIDrum::doesAutomationExistOnMIDIParam(ModelStackWithThreeMainThings* modelStack, int32_t cc) {
	bool automationExists = false;

	ModelStackWithAutoParam* modelStackWithAutoParam = getParamToControlFromInputMIDIChannel(cc, modelStack);
	if (modelStackWithAutoParam->autoParam) {
		automationExists = modelStackWithAutoParam->autoParam->isAutomated();
	}

	return automationExists;
}

// Write mod knob CC assignments to file
void MIDIDrum::writeModKnobAssignmentsToFile(Serializer& writer) {
	// Each drum writes its own modKnobs block. Do not skip based on kit siblings — song save walks drums in
	// note-row order and removes them from the linked list as it goes, so sibling-based dedup only wrote modKnobs
	// on the last drum.
	bool hasAssignments = false;
	for (int32_t m = 0; m < kNumModButtons * kNumPhysicalModKnobs; m++) {
		if (modKnobCCAssignments[m] != CC_NUMBER_NONE) {
			hasAssignments = true;
			break;
		}
	}

	if (!hasAssignments) {
		return;
	}

	writer.writeOpeningTag("modKnobs");
	for (int32_t m = 0; m < kNumModButtons * kNumPhysicalModKnobs; m++) {
		int32_t cc = modKnobCCAssignments[m];

		writer.writeOpeningTagBeginning("modKnob");
		if (cc == CC_NUMBER_NONE) {
			writer.writeAttribute("cc", "none");
		}
		else if (cc == CC_NUMBER_PITCH_BEND) {
			writer.writeAttribute("cc", "bend");
		}
		else if (cc == CC_NUMBER_AFTERTOUCH) {
			writer.writeAttribute("cc", "aftertouch");
		}
		else if (cc == CC_NUMBER_Y_AXIS) {
			writer.writeAttribute("cc", CC_EXTERNAL_MOD_WHEEL); // Map internal Y axis back to mod wheel
		}
		else {
			writer.writeAttribute("cc", cc);
		}
		writer.closeTag(); // Self-closing tag since we don't write automation data here
	}
	writer.writeClosingTag("modKnobs");
}

// Read mod knob CC assignments from file
Error MIDIDrum::readModKnobAssignmentsFromFile(Deserializer& reader, int32_t readAutomationUpToPos) {
	(void)readAutomationUpToPos;
	char const* tagName;
	int32_t m = 0;

	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "modKnob")) {
			if (m >= kNumModButtons * kNumPhysicalModKnobs) {
				return Error::FILE_CORRUPTED;
			}

			char const* innerTag;
			while (*(innerTag = reader.readNextTagOrAttributeName())) {
				if (!strcmp(innerTag, "cc")) {
					char const* ccAttribute = reader.readTagOrAttributeValue();

					if (!ccAttribute || !strcasecmp(ccAttribute, "none")) {
						modKnobCCAssignments[m] = CC_NUMBER_NONE;
					}
					else if (!strcasecmp(ccAttribute, "bend")) {
						modKnobCCAssignments[m] = CC_NUMBER_PITCH_BEND;
					}
					else if (!strcasecmp(ccAttribute, "aftertouch")) {
						modKnobCCAssignments[m] = CC_NUMBER_AFTERTOUCH;
					}
					else {
						int32_t cc = stringToInt(ccAttribute);
						if (cc == CC_EXTERNAL_MOD_WHEEL) {
							cc = CC_NUMBER_Y_AXIS;
						}
						modKnobCCAssignments[m] = cc;
					}

					reader.exitTag("cc");
				}
				else if (!strcmp(innerTag, "value")) {
					// Kit modKnobs only store CC assignment, not automation — skip value if present.
					reader.exitTag("value");
				}
				else {
					reader.exitTag(innerTag);
				}
			}

			m++;
		}

		// Match MIDIInstrument::readModKnobAssignmentsFromFile — parent closes each modKnob.
		reader.exitTag();
		if (m >= kNumModButtons * kNumPhysicalModKnobs) {
			break;
		}
	}

	return Error::NONE;
}

// CC management and mod knob mode support

int32_t MIDIDrum::changeControlNumberForModKnob(int32_t offset, int32_t whichModEncoder, int32_t modKnobMode) {
	int8_t* cc = &modKnobCCAssignments[modKnobMode * kNumPhysicalModKnobs + whichModEncoder];

	int32_t newCC = *cc;

	newCC += offset;
	if (newCC < 0) {
		newCC += kNumCCNumbersIncludingFake;
	}
	else if (newCC >= kNumCCNumbersIncludingFake) {
		newCC -= kNumCCNumbersIncludingFake;
	}
	if (newCC == 1) {
		// mod wheel is actually CC_NUMBER_Y_AXIS (122) internally
		newCC += offset;
	}

	*cc = newCC;

	return newCC;
}

int32_t MIDIDrum::getKnobPosForNonExistentParam(int32_t whichModEncoder, ModelStackWithAutoParam* modelStack) {
	if (modelStack->autoParam
	    && (modelStack->paramId < kNumRealCCNumbers || modelStack->paramId == CC_NUMBER_PITCH_BEND)) {
		return 0; // MIDI CCs start at 0
	}
	// For non-MIDI params, return default behavior from base class
	return -64; // Default position
}

void MIDIDrum::modButtonAction(uint8_t whichModButton, bool on) {
	// If we're leaving CC selection mode, clean up
	if (currentUIMode == UI_MODE_SELECTING_MIDI_CC) {
		currentUIMode = UI_MODE_NONE;
		if (display->haveOLED()) {
			deluge::hid::display::OLED::removePopup();
		}
		else {
			InstrumentClipMinder::redrawNumericDisplay();
		}
	}
}

// Device definition file support methods

void MIDIDrum::writeDeviceDefinitionFile(Serializer& writer, bool writeFileNameToPresetOrSong) {
	writer.writeOpeningTagBeginning("midiDevice");
	writer.writeOpeningTagEnd();

	if (writeFileNameToPresetOrSong) {
		// Rows 2+ often have labels copied from row 1 but no path — fix before writing song/kit XML.
		ensureDeviceDefinitionFileNameFromKit();
		writeDeviceDefinitionFileNameToPresetOrSong(writer);
		// Reference the SD definition file; don't embed duplicate ccLabels in kit/song.
		if (deviceDefinitionFileName.isEmpty()) {
			writeCCLabelsToFile(writer);
		}
	}
	else {
		writeCCLabelsToFile(writer);
	}

	writer.writeClosingTag("midiDevice");
}

void MIDIDrum::writeDeviceDefinitionFileNameToPresetOrSong(Serializer& writer) {
	writer.writeOpeningTagBeginning("definitionFile");
	if (deviceDefinitionFileName.isEmpty()) {
		writer.writeAttribute("name", "");
	}
	else {
		writer.writeAttribute("name", deviceDefinitionFileName.get());
	}
	writer.closeTag();
}

void MIDIDrum::writeCCLabelsToFile(Serializer& writer) {
	writer.writeOpeningTagBeginning("ccLabels");
	for (int32_t i = 0; i < kNumRealCCNumbers; i++) {
		if (i == CC_EXTERNAL_MOD_WHEEL) {
			continue;
		}
		auto it = labels.find(i);
		if (it != labels.end() && !it->second.empty()) {
			char ccNumber[10];
			intToString(i, ccNumber, 1);
			writer.writeAttribute(ccNumber, it->second.data());
		}
	}
	writer.closeTag();
}

Error MIDIDrum::readDeviceDefinitionFile(Deserializer& reader, bool readFromPresetOrSong) {
	Error error = Error::FILE_UNREADABLE;
	loadDeviceDefinitionFile = false;
	bool hasDefinitionFileReference = false;

	char const* tagName;

	// step into any subtags
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "definitionFile")) {
			readDeviceDefinitionFileNameFromPresetOrSong(reader);
			hasDefinitionFileReference = !deviceDefinitionFileName.isEmpty();
			// only flag definition file for loading if we aren't reading from preset or song
			// and definition file name isn't blank
			if (hasDefinitionFileReference && !readFromPresetOrSong) {
				loadDeviceDefinitionFile = true;
			}
		}
		// if we aren't reading from device definition file later, then try to read
		// device info now
		else if (!strcmp(tagName, "ccLabels")) {
			if (!hasDefinitionFileReference) {
				MIDIDrum* labelSource = findKitMidiDrumWithLabels();
				if (labelSource) {
					labels = labelSource->labels;
					if (deviceDefinitionFileName.isEmpty() && !labelSource->deviceDefinitionFileName.isEmpty()) {
						deviceDefinitionFileName.set(labelSource->deviceDefinitionFileName.get());
					}
				}
				else {
					error = readCCLabelsFromFile(reader);
				}
			}
		}

		// Match MIDIInstrument::readDeviceDefinitionFile — close each child tag.
		reader.exitTag();
	}

	// Defer SD load until kit/song file is closed — opening another file mid-parse corrupts the read.
	if (readFromPresetOrSong && hasDefinitionFileReference && labels.empty()) {
		loadDeviceDefinitionFile = true;
	}

	return error;
}

void MIDIDrum::readDeviceDefinitionFileNameFromPresetOrSong(Deserializer& reader) {
	char const* tagName;

	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "name")) {
			reader.readTagOrAttributeValueString(&deviceDefinitionFileName);
		}
		reader.exitTag();
	}
}

Error MIDIDrum::readCCLabelsFromFile(Deserializer& reader) {
	Error error = Error::FILE_UNREADABLE;

	int32_t cc = 0;
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		char ccNumber[10];
		cc = stringToInt(tagName);

		if (cc < 0 || cc >= kNumRealCCNumbers) {
			reader.exitTag();
			continue;
		}

		char const* value = reader.readTagOrAttributeValue();
		if (value && value[0] != '\0') {
			labels[cc] = value;
		}

		error = Error::NONE;

		reader.exitTag();
	}

	return error;
}

MIDIDrum* MIDIDrum::findKitMidiDrumWithLabels() const {
	if (!kit) {
		return nullptr;
	}

	for (Drum* thisDrum = kit->firstDrum; thisDrum; thisDrum = thisDrum->next) {
		if (thisDrum == this || thisDrum->type != DrumType::MIDI) {
			continue;
		}

		MIDIDrum* midiDrum = static_cast<MIDIDrum*>(thisDrum);
		if (!midiDrum->labels.empty() && sharesMidiDeviceDefinitionWith(midiDrum)) {
			return midiDrum;
		}
	}

	return nullptr;
}

bool MIDIDrum::sharesMidiDeviceDefinitionWith(const MIDIDrum* other) const {
	if (!other) {
		return false;
	}

	// Primary key: same definition file (different paths = different instruments).
	return !strcmp(deviceDefinitionFileName.get(), other->deviceDefinitionFileName.get());
}

bool MIDIDrum::hasModKnobAssignments() const {
	for (int32_t m = 0; m < kNumModButtons * kNumPhysicalModKnobs; m++) {
		if (modKnobCCAssignments[m] != CC_NUMBER_NONE) {
			return true;
		}
	}
	return false;
}

MIDIDrum* MIDIDrum::findKitMidiDrumWithModKnobs() const {
	if (!kit) {
		return nullptr;
	}

	for (Drum* thisDrum = kit->firstDrum; thisDrum; thisDrum = thisDrum->next) {
		if (thisDrum == this || thisDrum->type != DrumType::MIDI) {
			continue;
		}

		MIDIDrum* midiDrum = static_cast<MIDIDrum*>(thisDrum);
		if (midiDrum->hasModKnobAssignments()) {
			return midiDrum;
		}
	}

	return nullptr;
}

bool MIDIDrum::modKnobAssignmentsMatch(MIDIDrum const* other) const {
	if (!other) {
		return false;
	}

	return modKnobCCAssignments == other->modKnobCCAssignments;
}

void MIDIDrum::ensureDeviceDefinitionFileNameFromKit() {
	if (!deviceDefinitionFileName.isEmpty() || !kit) {
		return;
	}

	for (Drum* thisDrum = kit->firstDrum; thisDrum; thisDrum = thisDrum->next) {
		if (thisDrum == this || thisDrum->type != DrumType::MIDI) {
			continue;
		}

		MIDIDrum* midiDrum = static_cast<MIDIDrum*>(thisDrum);
		if (!midiDrum->deviceDefinitionFileName.isEmpty()) {
			deviceDefinitionFileName.set(midiDrum->deviceDefinitionFileName.get());
			return;
		}
	}
}

std::string_view MIDIDrum::getNameFromCC(int32_t cc) {
	if (cc < 0 || cc >= kNumRealCCNumbers) {
		// out of range
		return std::string_view{};
	}

	auto it = labels.find(cc);

	// found
	if (it != labels.end()) {
		return it->second;
	}

	// not found
	return std::string_view{};
}

void MIDIDrum::setNameForCC(int32_t cc, std::string_view name) {
	if (cc >= 0 && cc < kNumRealCCNumbers) {
		labels[cc] = name;
	}
}

void MIDIDrum::copyLabelsFrom(MIDIDrum const* other) {
	if (!other) {
		return;
	}

	labels = other->labels;
	loadDeviceDefinitionFile = false;

	if (deviceDefinitionFileName.isEmpty() && other->deviceDefinitionFileName.get()[0] != '\0') {
		deviceDefinitionFileName.set(other->deviceDefinitionFileName.get());
	}
}

bool MIDIDrum::hasCCLabels() const {
	return !labels.empty();
}

void MIDIDrum::propagateSharedSettingsAcrossKit(Kit* kit) {
	if (!kit) {
		return;
	}

	MIDIDrum* modKnobTemplate = nullptr;
	MIDIDrum* definitionTemplate = nullptr;

	for (Drum* thisDrum = kit->firstDrum; thisDrum; thisDrum = thisDrum->next) {
		if (thisDrum->type != DrumType::MIDI) {
			continue;
		}

		MIDIDrum* midiDrum = static_cast<MIDIDrum*>(thisDrum);

		if (!modKnobTemplate && midiDrum->hasModKnobAssignments()) {
			modKnobTemplate = midiDrum;
		}

		if (!definitionTemplate && midiDrum->deviceDefinitionFileName.get()[0] != '\0') {
			definitionTemplate = midiDrum;
		}
	}

	for (Drum* thisDrum = kit->firstDrum; thisDrum; thisDrum = thisDrum->next) {
		if (thisDrum->type != DrumType::MIDI) {
			continue;
		}

		MIDIDrum* midiDrum = static_cast<MIDIDrum*>(thisDrum);

		if (modKnobTemplate && !midiDrum->hasModKnobAssignments()) {
			midiDrum->modKnobCCAssignments = modKnobTemplate->modKnobCCAssignments;
		}

		if (definitionTemplate && midiDrum->deviceDefinitionFileName.isEmpty()) {
			midiDrum->deviceDefinitionFileName.set(definitionTemplate->deviceDefinitionFileName.get());
		}

		if (modKnobTemplate && modKnobTemplate->hasCCLabels() && !midiDrum->hasCCLabels()) {
			midiDrum->copyLabelsFrom(modKnobTemplate);
		}
	}
}
