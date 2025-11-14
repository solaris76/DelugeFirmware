#include "io/midi/sysex/transport_sysex.h"
#include "model/action/action_logger.h"
#include "model/scale/preset_scales.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include "storage/smsysex.h"
#include <cstring>

extern JsonSerializer jWriter;
extern Song* currentSong;
extern PlaybackHandler playbackHandler;
extern ActionLogger actionLogger;
extern GlobalMIDICommand pendingGlobalMIDICommand;
extern int32_t pendingGlobalMIDICommandNumClustersWritten;

// Separate writer for async notifications to avoid reentrancy issues
JsonSerializer notifyWriter;

namespace TransportSysex {

const uint32_t MAX_SUBSCRIBERS = 4;
MIDICable* subscribers[MAX_SUBSCRIBERS] = {nullptr};
uint32_t numSubscribers = 0;

void transportControl(MIDICable& cable, JsonDeserializer& reader) {
	String command;
	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "command")) {
			reader.readTagOrAttributeValueString(&command);
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	const char* cmd = command.get();
	bool validCommand = true;

	// Basic transport
	if (!strcmp(cmd, "play")) {
		playbackHandler.playButtonPressed(0);
	}
	else if (!strcmp(cmd, "stop")) {
		playbackHandler.endPlayback();
	}
	else if (!strcmp(cmd, "record")) {
		playbackHandler.recordButtonPressed();
	}
	else if (!strcmp(cmd, "restart")) {
		if (currentSong && playbackHandler.recording != RecordingMode::ARRANGEMENT) {
			playbackHandler.forceResetPlayPos(currentSong, true);
		}
	}
	else if (!strcmp(cmd, "tap")) {
		playbackHandler.tempoEncoderAction(0, false, false);
	}
	// Loop commands
	else if (!strcmp(cmd, "loop")) {
		playbackHandler.tryLoopCommand(GlobalMIDICommand::LOOP);
	}
	else if (!strcmp(cmd, "loopLayering")) {
		playbackHandler.tryLoopCommand(GlobalMIDICommand::LOOP_CONTINUOUS_LAYERING);
	}
	// Edit commands (these are "pended" to avoid timing issues)
	else if (!strcmp(cmd, "undo")) {
		if (actionLogger.allowedToDoReversion()) {
			pendingGlobalMIDICommand = GlobalMIDICommand::UNDO;
			pendingGlobalMIDICommandNumClustersWritten = 0;
		}
	}
	else if (!strcmp(cmd, "redo")) {
		if (actionLogger.allowedToDoReversion()) {
			pendingGlobalMIDICommand = GlobalMIDICommand::REDO;
			pendingGlobalMIDICommandNumClustersWritten = 0;
		}
	}
	// Song commands
	else if (!strcmp(cmd, "fill")) {
		if (currentSong) {
			currentSong->changeFillMode(true);
		}
	}
	// Note: nextSong removed - unsafe from SysEx context (requires SD card access)
	else {
		validCommand = false;
	}

	if (!validCommand) {
		smSysex::startReply(jWriter, reader);
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", "Unknown transport command");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^transportCommand", false, true);
	jWriter.writeAttribute("command", cmd);
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void setTempo(MIDICable& cable, JsonDeserializer& reader) {
	int32_t bpm = 120;
	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "bpm")) {
			bpm = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (!currentSong) {
		smSysex::startReply(jWriter, reader);
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", "No song loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	if (bpm < 30)
		bpm = 30;
	if (bpm > 300)
		bpm = 300;

	currentSong->setBPM((float)bpm, false);
	notifyTempoChanged(bpm);

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^tempoSet", false, true);
	jWriter.writeAttribute("bpm", bpm);
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getTransportState(MIDICable& cable, JsonDeserializer& reader) {
	if (!currentSong) {
		smSysex::startReply(jWriter, reader);
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", "No song loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^transportState", false, true); // box = true for outer braces

	bool playing = (playbackHandler.playbackState & PLAYBACK_CLOCK_EITHER_ACTIVE) != 0;
	bool recording = (playbackHandler.recording != RecordingMode::OFF);
	int32_t bpm = (int32_t)currentSong->calculateBPM();

	jWriter.writeAttribute("playing", playing);
	jWriter.writeAttribute("recording", recording);
	jWriter.writeAttribute("bpm", bpm);
	jWriter.writeAttribute("swing", currentSong->swingAmount);
	jWriter.writeAttribute("metronomeOn", playbackHandler.metronomeOn);

	jWriter.closeTag(true); // box = true to close outer braces
	smSysex::sendMsg(cable, jWriter);
}

void setSwing(MIDICable& cable, JsonDeserializer& reader) {
	int32_t swing = 0;
	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "swing")) {
			swing = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (!currentSong) {
		smSysex::startReply(jWriter, reader);
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", "No song loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	if (swing < -50)
		swing = -50;
	if (swing > 50)
		swing = 50;

	currentSong->swingAmount = swing;
	notifyTransportChanged();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^swingSet", false, true);
	jWriter.writeAttribute("swing", swing);
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void setMetronome(MIDICable& cable, JsonDeserializer& reader) {
	int32_t enabled = 0;
	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "enabled") || !strcmp(tagName, "on")) {
			enabled = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	playbackHandler.metronomeOn = (enabled != 0);
	playbackHandler.setLedStates(); // Update tap tempo LED
	notifyTransportChanged();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^metronomeSet", false, true);
	jWriter.writeAttribute("enabled", enabled);
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void subscribeTransport(MIDICable& cable, JsonDeserializer& reader) {
	for (uint32_t i = 0; i < numSubscribers; i++) {
		if (subscribers[i] == &cable) {
			smSysex::startReply(jWriter, reader);
			jWriter.writeOpeningTag("^transportSubscribed", false, true);
			jWriter.writeAttribute("status", "already_subscribed");
			jWriter.closeTag(true);
			smSysex::sendMsg(cable, jWriter);
			return;
		}
	}

	if (numSubscribers < MAX_SUBSCRIBERS) {
		subscribers[numSubscribers++] = &cable;
		smSysex::startReply(jWriter, reader);
		jWriter.writeOpeningTag("^transportSubscribed", false, true);
		jWriter.writeAttribute("status", "success");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
	}
	else {
		smSysex::startReply(jWriter, reader);
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", "Max transport subscribers reached");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
	}
}

void unsubscribeTransport(MIDICable& cable, JsonDeserializer& reader) {
	for (uint32_t i = 0; i < numSubscribers; i++) {
		if (subscribers[i] == &cable) {
			for (uint32_t j = i; j < numSubscribers - 1; j++) {
				subscribers[j] = subscribers[j + 1];
			}
			subscribers[numSubscribers - 1] = nullptr;
			numSubscribers--;

			smSysex::startReply(jWriter, reader);
			jWriter.writeOpeningTag("^transportUnsubscribed", false, true);
			jWriter.writeAttribute("status", "success");
			jWriter.closeTag(true);
			smSysex::sendMsg(cable, jWriter);
			return;
		}
	}

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^transportUnsubscribed", false, true);
	jWriter.writeAttribute("status", "not_subscribed");
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void notifyTempoChanged(int32_t bpm) {
	if (numSubscribers == 0)
		return;

	for (uint32_t i = 0; i < numSubscribers; i++) {
		if (subscribers[i]) {
			notifyWriter.reset();
			notifyWriter.setMemoryBased();
			smSysex::startDirect(notifyWriter);
			notifyWriter.writeOpeningTag("^tempoChanged", false, true);
			notifyWriter.writeAttribute("bpm", bpm);
			notifyWriter.closeTag(true);
			smSysex::sendMsg(*subscribers[i], notifyWriter);
		}
	}
}

void notifyTransportChanged() {
	if (numSubscribers == 0 || !currentSong)
		return;

	for (uint32_t i = 0; i < numSubscribers; i++) {
		if (subscribers[i]) {
			notifyWriter.reset();
			notifyWriter.setMemoryBased();
			smSysex::startDirect(notifyWriter);
			notifyWriter.writeOpeningTag("^transportChanged", false, true); // box = true

			bool playing = (playbackHandler.playbackState & PLAYBACK_CLOCK_EITHER_ACTIVE) != 0;
			bool recording = (playbackHandler.recording != RecordingMode::OFF);
			int32_t bpm = (int32_t)currentSong->calculateBPM();

			notifyWriter.writeAttribute("playing", playing);
			notifyWriter.writeAttribute("recording", recording);
			notifyWriter.writeAttribute("bpm", bpm);
			notifyWriter.writeAttribute("swing", currentSong->swingAmount);
			notifyWriter.writeAttribute("metronomeOn", playbackHandler.metronomeOn);

			notifyWriter.closeTag(true); // box = true
			smSysex::sendMsg(*subscribers[i], notifyWriter);
		}
	}
}

void setSongScale(MIDICable& cable, JsonDeserializer& reader) {
	if (!currentSong) {
		smSysex::startReply(jWriter, reader);
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", "No song loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	int32_t rootNote = -1;
	int32_t scaleIndex = -1;
	String scaleName;
	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "rootNote") || !strcmp(tagName, "root")) {
			rootNote = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "scale") || !strcmp(tagName, "scaleIndex")) {
			scaleIndex = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "scaleName")) {
			reader.readTagOrAttributeValueString(&scaleName);
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	bool success = true;
	const char* errorMessage = nullptr;

	// Set root note if provided
	if (rootNote >= 0 && rootNote < 128) {
		currentSong->setRootNote(rootNote);
	}
	else if (rootNote >= 128) {
		success = false;
		errorMessage = "Root note must be 0-127";
	}

	// Set scale if provided
	if (success && scaleIndex >= 0) {
		if (scaleIndex >= 0 && scaleIndex < NUM_PRESET_SCALES) {
			Scale result = currentSong->setScale(static_cast<Scale>(scaleIndex));
			if (result == NO_SCALE) {
				success = false;
				errorMessage = "Failed to set scale: scale incompatible with existing notes";
			}
		}
		else if (scaleIndex == NUM_PRESET_SCALES) {
			// User scale
			if (!currentSong->hasUserScale()) {
				success = false;
				errorMessage = "No user scale available";
			}
			else {
				Scale result = currentSong->setScale(USER_SCALE);
				if (result == NO_SCALE) {
					success = false;
					errorMessage = "Failed to set user scale";
				}
			}
		}
		else {
			success = false;
			errorMessage = "Invalid scale index";
		}
	}
	else if (success && scaleName.get() && scaleName.get()[0]) {
		// Look up scale by name
		Scale foundScale = NO_SCALE;
		const char* scaleNameStr = scaleName.get();
		for (int32_t i = 0; i < NUM_PRESET_SCALES; i++) {
			const char* name = getScaleName(static_cast<Scale>(i));
			if (name && !strcmp(name, scaleNameStr)) {
				foundScale = static_cast<Scale>(i);
				break;
			}
		}
		// Check for USER_SCALE
		if (foundScale == NO_SCALE && !strcmp(scaleNameStr, "USER")) {
			foundScale = USER_SCALE;
		}

		if (foundScale != NO_SCALE) {
			if (foundScale == USER_SCALE && !currentSong->hasUserScale()) {
				success = false;
				errorMessage = "No user scale available";
			}
			else {
				Scale result = currentSong->setScale(foundScale);
				if (result == NO_SCALE) {
					success = false;
					errorMessage = "Failed to set scale: scale incompatible with existing notes";
				}
			}
		}
		else {
			success = false;
			errorMessage = "Scale name not found";
		}
	}

	smSysex::startReply(jWriter, reader);
	if (success) {
		jWriter.writeOpeningTag("^songScaleSet", false, true);
		jWriter.writeAttribute("status", "success");
		jWriter.writeAttribute("rootNote", currentSong->key.rootNote);
		Scale currentScale = currentSong->getCurrentScale();
		jWriter.writeAttribute("scale", (int32_t)currentScale);
		jWriter.writeAttribute("scaleName", getScaleName(currentScale));
		jWriter.closeTag(true);
	}
	else {
		const char* message = errorMessage ? errorMessage : "Unknown error";
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", message);
		jWriter.closeTag(true);
	}
	smSysex::sendMsg(cable, jWriter);
}

void getSongScale(MIDICable& cable, JsonDeserializer& reader) {
	if (!currentSong) {
		smSysex::startReply(jWriter, reader);
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", "No song loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^songScale", false, true);
	jWriter.writeAttribute("rootNote", currentSong->key.rootNote);
	Scale currentScale = currentSong->getCurrentScale();
	jWriter.writeAttribute("scale", (int32_t)currentScale);
	jWriter.writeAttribute("scaleName", getScaleName(currentScale));
	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

} // namespace TransportSysex
