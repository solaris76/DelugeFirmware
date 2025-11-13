#include "io/midi/sysex/settings_sysex.h"
#include "gui/menu_item/colour.h"
#include "hid/led/pad_leds.h"
#include "io/midi/midi_engine.h"
#include "model/settings/runtime_feature_settings.h"
#include "playback/playback_handler.h"
#include "processing/engines/audio_engine.h"
#include "processing/engines/cv_engine.h"
#include "storage/flash_storage.h"
#include "storage/smsysex.h"
#include "version.h"
#include <cstdlib>
#include <cstring>

extern JsonSerializer jWriter;
extern MidiEngine midiEngine;
extern PlaybackHandler playbackHandler;
extern CVEngine cvEngine;

using namespace deluge::gui::menu_item;

namespace SettingsSysex {

// ============================================================================
// GROUPED SETTINGS GETTERS
// ============================================================================

void getSettingsCV(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settingsCV", false, true);

	jWriter.writeAttribute("cv1v", (int32_t)cvEngine.cvChannels[0].voltsPerOctave);
	jWriter.writeAttribute("cv2v", (int32_t)cvEngine.cvChannels[1].voltsPerOctave);
	jWriter.writeAttribute("cv1t", (int32_t)cvEngine.cvChannels[0].transpose);
	jWriter.writeAttribute("cv2t", (int32_t)cvEngine.cvChannels[1].transpose);
	jWriter.writeAttribute("cv1c", (int32_t)cvEngine.cvChannels[0].cents);
	jWriter.writeAttribute("cv2c", (int32_t)cvEngine.cvChannels[1].cents);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getSettingsGate(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settingsGate", false, true);

	jWriter.writeAttribute("g0", (int32_t)cvEngine.gateChannels[0].mode);
	jWriter.writeAttribute("g1", (int32_t)cvEngine.gateChannels[1].mode);
	jWriter.writeAttribute("g2", (int32_t)cvEngine.gateChannels[2].mode);
	jWriter.writeAttribute("g3", (int32_t)cvEngine.gateChannels[3].mode);
	jWriter.writeAttribute("goff", (int32_t)cvEngine.minGateOffTime);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getSettingsClock(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settingsClock", false, true);

	jWriter.writeAttribute("cas", playbackHandler.analogClockInputAutoStart);
	jWriter.writeAttribute("cip", (int32_t)playbackHandler.analogInTicksPPQN);
	jWriter.writeAttribute("cop", (int32_t)playbackHandler.analogOutTicksPPQN);
	jWriter.writeAttribute("mco", playbackHandler.midiOutClockEnabled);
	jWriter.writeAttribute("tmm", playbackHandler.tempoMagnitudeMatchingEnabled);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getSettingsMidi(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settingsMidi", false, true);

	// Basic MIDI settings
	jWriter.writeAttribute("mt", midiEngine.midiThru);
	jWriter.writeAttribute("mc", playbackHandler.midiInClockEnabled);
	jWriter.writeAttribute("mto", (int32_t)midiEngine.midiTakeover);
	jWriter.writeAttribute("mskr", midiEngine.midiSelectKitRow);

	// Global MIDI Commands (11 learnable functions)
	// gmc0-10: channel (c suffix), note (n suffix). 255 = not learned
	for (uint32_t i = 0; i < kNumGlobalMIDICommands; i++) {
		const LearnedMIDI& cmd = midiEngine.globalMIDICommands[i];
		char attrC[10], attrN[10];
		snprintf(attrC, sizeof(attrC), "gmc%dc", i);
		snprintf(attrN, sizeof(attrN), "gmc%dn", i);
		jWriter.writeAttribute(attrC, cmd.channelOrZone);
		jWriter.writeAttribute(attrN, cmd.noteOrCC);
	}

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getSettingsPads(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settingsPads", false, true);

	jWriter.writeAttribute("cact", (int32_t)activeColourMenu.value);
	jWriter.writeAttribute("cstp", (int32_t)stoppedColourMenu.value);
	jWriter.writeAttribute("cmut", (int32_t)mutedColourMenu.value);
	jWriter.writeAttribute("csol", (int32_t)soloColourMenu.value);
	jWriter.writeAttribute("cfil", (int32_t)fillColourMenu.value);
	jWriter.writeAttribute("conc", (int32_t)onceColourMenu.value);
	jWriter.writeAttribute("curs", (int32_t)PadLEDs::flashCursor);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getSettingsRecording(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settingsRecording", false, true);

	jWriter.writeAttribute("rq", (int32_t)FlashStorage::recordQuantizeLevel);
	jWriter.writeAttribute("acm", FlashStorage::audioClipRecordMargins);
	jWriter.writeAttribute("cib", (int32_t)playbackHandler.countInBars);
	jWriter.writeAttribute("mon", (int32_t)AudioEngine::inputMonitoringMode);
	jWriter.writeAttribute("trm", (int32_t)FlashStorage::defaultThresholdRecordingMode);
	jWriter.writeAttribute("lrc", (int32_t)FlashStorage::defaultLoopRecordingCommand);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getSettingsDefaults(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settingsDefaults", false, true);

	jWriter.writeAttribute("ds", (int32_t)FlashStorage::defaultScale);
	jWriter.writeAttribute("dv", (int32_t)FlashStorage::defaultVelocity);
	jWriter.writeAttribute("dm", (int32_t)FlashStorage::defaultMagnitude);
	jWriter.writeAttribute("br0", (int32_t)FlashStorage::defaultBendRange[0]);
	jWriter.writeAttribute("br1", (int32_t)FlashStorage::defaultBendRange[1]);
	jWriter.writeAttribute("mv", (int32_t)FlashStorage::defaultMetronomeVolume);
	jWriter.writeAttribute("si", (int32_t)FlashStorage::defaultSwingInterval);
	jWriter.writeAttribute("ssm", (int32_t)FlashStorage::defaultStartupSongMode);
	jWriter.writeAttribute("nct", (int32_t)FlashStorage::defaultNewClipType);
	jWriter.writeAttribute("ulct", FlashStorage::defaultUseLastClipType);
	jWriter.writeAttribute("pcp", (int32_t)FlashStorage::defaultPatchCablePolarity);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getSettingsCommunity(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settingsCommunity", false, true);

	// Automatically loop through ALL community features (future-proof!)
	// When new RuntimeFeatureSettingType is added, it automatically appears here
	for (uint32_t i = 0; i < RuntimeFeatureSettingType::MaxElement; i++) {
		RuntimeFeatureSettingType type = static_cast<RuntimeFeatureSettingType>(i);
		char attrName[8];
		snprintf(attrName, sizeof(attrName), "cf%d", i); // cf0, cf1, cf2...
		jWriter.writeAttribute(attrName, runtimeFeatureSettings.get(type));
	}

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getSettingsUI(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settingsUI", false, true);

	jWriter.writeAttribute("pb", (int32_t)FlashStorage::defaultPadBrightness);
	jWriter.writeAttribute("kl", (int32_t)FlashStorage::defaultKeyboardLayout);
	jWriter.writeAttribute("sl", (int32_t)FlashStorage::defaultSessionLayout);
	jWriter.writeAttribute("ht", (int32_t)FlashStorage::defaultHoldTime);
	jWriter.writeAttribute("sbp", (int32_t)FlashStorage::sampleBrowserPreviewMode);
	jWriter.writeAttribute("sm", (int32_t)FlashStorage::defaultSliceMode);
	jWriter.writeAttribute("gam", (int32_t)FlashStorage::defaultGridActiveMode);
	jWriter.writeAttribute("fav", (int32_t)FlashStorage::defaultFavouritesLayout);
	jWriter.writeAttribute("geu", FlashStorage::gridEmptyPadsUnarm);
	jWriter.writeAttribute("gecr", FlashStorage::gridEmptyPadsCreateRec);
	jWriter.writeAttribute("gags", FlashStorage::gridAllowGreenSelection);
	jWriter.writeAttribute("kvg", FlashStorage::keyboardFunctionsVelocityGlide);
	jWriter.writeAttribute("kmg", FlashStorage::keyboardFunctionsModwheelGlide);
	jWriter.writeAttribute("ac", FlashStorage::accessibilityShortcuts);
	jWriter.writeAttribute("amh", (int32_t)FlashStorage::accessibilityMenuHighlighting);
	jWriter.writeAttribute("cpu", FlashStorage::highCPUUsageIndicator);
	jWriter.writeAttribute("sharp", FlashStorage::defaultUseSharps);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

void getAllSettings(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^settings", false, true); // box = true for outer braces

	// MIDI Settings
	jWriter.writeAttribute("mt", midiEngine.midiThru);
	jWriter.writeAttribute("mc", playbackHandler.midiInClockEnabled);

	// Defaults
	jWriter.writeAttribute("ds", (int32_t)FlashStorage::defaultScale);
	jWriter.writeAttribute("dv", (int32_t)FlashStorage::defaultVelocity);
	jWriter.writeAttribute("dm", (int32_t)FlashStorage::defaultMagnitude);
	jWriter.writeAttribute("br0", (int32_t)FlashStorage::defaultBendRange[0]);
	jWriter.writeAttribute("br1", (int32_t)FlashStorage::defaultBendRange[1]);

	// UI/Display
	jWriter.writeAttribute("pb", (int32_t)FlashStorage::defaultPadBrightness);
	jWriter.writeAttribute("kl", (int32_t)FlashStorage::defaultKeyboardLayout);
	jWriter.writeAttribute("sl", (int32_t)FlashStorage::defaultSessionLayout);
	jWriter.writeAttribute("ht", (int32_t)FlashStorage::defaultHoldTime);

	// Recording
	jWriter.writeAttribute("rq", (int32_t)FlashStorage::recordQuantizeLevel);
	jWriter.writeAttribute("acm", FlashStorage::audioClipRecordMargins);
	jWriter.writeAttribute("sbp", (int32_t)FlashStorage::sampleBrowserPreviewMode);

	// Metronome/Swing
	jWriter.writeAttribute("mv", (int32_t)FlashStorage::defaultMetronomeVolume);
	jWriter.writeAttribute("si", (int32_t)FlashStorage::defaultSwingInterval);

	// Automation
	jWriter.writeAttribute("ai", FlashStorage::automationInterpolate);
	jWriter.writeAttribute("aclr", FlashStorage::automationClear);
	jWriter.writeAttribute("ash", FlashStorage::automationShift);
	jWriter.writeAttribute("ann", FlashStorage::automationNudgeNote);
	jWriter.writeAttribute("adps", FlashStorage::automationDisableAuditionPadShortcuts);

	// Grid
	jWriter.writeAttribute("geu", FlashStorage::gridEmptyPadsUnarm);
	jWriter.writeAttribute("gecr", FlashStorage::gridEmptyPadsCreateRec);
	jWriter.writeAttribute("gags", FlashStorage::gridAllowGreenSelection);

	// Keyboard
	jWriter.writeAttribute("kvg", FlashStorage::keyboardFunctionsVelocityGlide);
	jWriter.writeAttribute("kmg", FlashStorage::keyboardFunctionsModwheelGlide);

	// Misc
	jWriter.writeAttribute("ac", FlashStorage::accessibilityShortcuts);
	jWriter.writeAttribute("cpu", FlashStorage::highCPUUsageIndicator);
	jWriter.writeAttribute("sharp", FlashStorage::defaultUseSharps);

	// Additional Settings
	jWriter.writeAttribute("ssm", (int32_t)FlashStorage::defaultStartupSongMode);
	jWriter.writeAttribute("nct", (int32_t)FlashStorage::defaultNewClipType);
	jWriter.writeAttribute("ulct", FlashStorage::defaultUseLastClipType);
	jWriter.writeAttribute("sm", (int32_t)FlashStorage::defaultSliceMode);
	jWriter.writeAttribute("gam", (int32_t)FlashStorage::defaultGridActiveMode);

	jWriter.closeTag(true); // box = true to close outer braces
	smSysex::sendMsg(cable, jWriter);
}

void setSetting(MIDICable& cable, JsonDeserializer& reader) {
	String settingName;
	int32_t value = 0;

	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "name")) {
			reader.readTagOrAttributeValueString(&settingName);
		}
		else if (!strcmp(tagName, "value")) {
			value = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	const char* name = settingName.get();
	bool success = true;

	// MIDI
	if (!strcmp(name, "mt")) {
		midiEngine.midiThru = (value != 0);
	}
	else if (!strcmp(name, "mc")) {
		playbackHandler.setMidiInClockEnabled(value != 0);
	}
	// Defaults
	else if (!strcmp(name, "ds")) {
		FlashStorage::defaultScale = (uint8_t)value;
	}
	else if (!strcmp(name, "dv")) {
		FlashStorage::defaultVelocity = (uint8_t)value;
	}
	else if (!strcmp(name, "dm")) {
		FlashStorage::defaultMagnitude = (int8_t)value;
	}
	else if (!strcmp(name, "br0")) {
		FlashStorage::defaultBendRange[0] = (uint8_t)value;
	}
	else if (!strcmp(name, "br1")) {
		FlashStorage::defaultBendRange[1] = (uint8_t)value;
	}
	// UI/Display
	else if (!strcmp(name, "pb")) {
		FlashStorage::defaultPadBrightness = (uint8_t)value;
	}
	else if (!strcmp(name, "kl")) {
		FlashStorage::defaultKeyboardLayout = (KeyboardLayoutType)value;
	}
	else if (!strcmp(name, "sl")) {
		FlashStorage::defaultSessionLayout = (SessionLayoutType)value;
	}
	else if (!strcmp(name, "ht")) {
		FlashStorage::defaultHoldTime = (uint8_t)value;
	}
	// Recording
	else if (!strcmp(name, "rq")) {
		FlashStorage::recordQuantizeLevel = (uint8_t)value;
	}
	else if (!strcmp(name, "acm")) {
		FlashStorage::audioClipRecordMargins = (value != 0);
	}
	else if (!strcmp(name, "sbp")) {
		FlashStorage::sampleBrowserPreviewMode = (uint8_t)value;
	}
	// Metronome/Swing
	else if (!strcmp(name, "mv")) {
		FlashStorage::defaultMetronomeVolume = (uint8_t)value;
	}
	else if (!strcmp(name, "si")) {
		FlashStorage::defaultSwingInterval = (uint8_t)value;
	}
	// Automation
	else if (!strcmp(name, "ai")) {
		FlashStorage::automationInterpolate = (value != 0);
	}
	else if (!strcmp(name, "aclr")) {
		FlashStorage::automationClear = (value != 0);
	}
	else if (!strcmp(name, "ash")) {
		FlashStorage::automationShift = (value != 0);
	}
	else if (!strcmp(name, "ann")) {
		FlashStorage::automationNudgeNote = (value != 0);
	}
	else if (!strcmp(name, "adps")) {
		FlashStorage::automationDisableAuditionPadShortcuts = (value != 0);
	}
	// Grid
	else if (!strcmp(name, "geu")) {
		FlashStorage::gridEmptyPadsUnarm = (value != 0);
	}
	else if (!strcmp(name, "gecr")) {
		FlashStorage::gridEmptyPadsCreateRec = (value != 0);
	}
	else if (!strcmp(name, "gags")) {
		FlashStorage::gridAllowGreenSelection = (value != 0);
	}
	// Keyboard
	else if (!strcmp(name, "kvg")) {
		FlashStorage::keyboardFunctionsVelocityGlide = (value != 0);
	}
	else if (!strcmp(name, "kmg")) {
		FlashStorage::keyboardFunctionsModwheelGlide = (value != 0);
	}
	// Misc
	else if (!strcmp(name, "ac")) {
		FlashStorage::accessibilityShortcuts = (value != 0);
	}
	else if (!strcmp(name, "cpu")) {
		FlashStorage::highCPUUsageIndicator = (value != 0);
	}
	else if (!strcmp(name, "sharp")) {
		FlashStorage::defaultUseSharps = (value != 0);
	}
	// Extended MIDI
	else if (!strcmp(name, "mto")) {
		midiEngine.midiTakeover = static_cast<decltype(midiEngine.midiTakeover)>(value);
	}
	else if (!strcmp(name, "mskr")) {
		midiEngine.midiSelectKitRow = (value != 0);
	}
	// Global MIDI Commands
	else if (!strncmp(name, "gmc", 3) && strlen(name) >= 5) {
		// Parse gmc#c or gmc#n format
		int cmdIdx = atoi(name + 3); // Get number after "gmc"
		if (cmdIdx >= 0 && cmdIdx < kNumGlobalMIDICommands) {
			char suffix = name[strlen(name) - 1]; // Last char: 'c' or 'n'
			if (suffix == 'c') {
				midiEngine.globalMIDICommands[cmdIdx].channelOrZone = value;
			}
			else if (suffix == 'n') {
				midiEngine.globalMIDICommands[cmdIdx].noteOrCC = value;
			}
		}
	}
	// CV Settings
	else if (!strcmp(name, "cv1v")) {
		cvEngine.cvChannels[0].voltsPerOctave = value;
	}
	else if (!strcmp(name, "cv2v")) {
		cvEngine.cvChannels[1].voltsPerOctave = value;
	}
	else if (!strcmp(name, "cv1t")) {
		cvEngine.cvChannels[0].transpose = value;
	}
	else if (!strcmp(name, "cv2t")) {
		cvEngine.cvChannels[1].transpose = value;
	}
	else if (!strcmp(name, "cv1c")) {
		cvEngine.cvChannels[0].cents = value;
	}
	else if (!strcmp(name, "cv2c")) {
		cvEngine.cvChannels[1].cents = value;
	}
	// Gate Settings
	else if (!strcmp(name, "g0")) {
		cvEngine.gateChannels[0].mode = static_cast<GateType>(value);
	}
	else if (!strcmp(name, "g1")) {
		cvEngine.gateChannels[1].mode = static_cast<GateType>(value);
	}
	else if (!strcmp(name, "g2")) {
		cvEngine.gateChannels[2].mode = static_cast<GateType>(value);
	}
	else if (!strcmp(name, "g3")) {
		cvEngine.gateChannels[3].mode = static_cast<GateType>(value);
	}
	else if (!strcmp(name, "goff")) {
		cvEngine.minGateOffTime = value;
	}
	// Clock Settings
	else if (!strcmp(name, "cas")) {
		playbackHandler.analogClockInputAutoStart = (value != 0);
	}
	else if (!strcmp(name, "cip")) {
		playbackHandler.analogInTicksPPQN = static_cast<decltype(playbackHandler.analogInTicksPPQN)>(value);
	}
	else if (!strcmp(name, "cop")) {
		playbackHandler.analogOutTicksPPQN = static_cast<decltype(playbackHandler.analogOutTicksPPQN)>(value);
	}
	else if (!strcmp(name, "mco")) {
		playbackHandler.midiOutClockEnabled = (value != 0);
	}
	else if (!strcmp(name, "tmm")) {
		playbackHandler.tempoMagnitudeMatchingEnabled = (value != 0);
	}
	// Pad Colors
	else if (!strcmp(name, "cact")) {
		activeColourMenu.value = static_cast<decltype(activeColourMenu.value)>(value);
	}
	else if (!strcmp(name, "cstp")) {
		stoppedColourMenu.value = static_cast<decltype(stoppedColourMenu.value)>(value);
	}
	else if (!strcmp(name, "cmut")) {
		mutedColourMenu.value = static_cast<decltype(mutedColourMenu.value)>(value);
	}
	else if (!strcmp(name, "csol")) {
		soloColourMenu.value = static_cast<decltype(soloColourMenu.value)>(value);
	}
	else if (!strcmp(name, "cfil")) {
		fillColourMenu.value = static_cast<decltype(fillColourMenu.value)>(value);
	}
	else if (!strcmp(name, "conc")) {
		onceColourMenu.value = static_cast<decltype(onceColourMenu.value)>(value);
	}
	else if (!strcmp(name, "curs")) {
		PadLEDs::flashCursor = (value != 0);
	}
	// Extended Recording Settings
	else if (!strcmp(name, "cib")) {
		playbackHandler.countInBars = value;
	}
	else if (!strcmp(name, "mon")) {
		AudioEngine::inputMonitoringMode = static_cast<decltype(AudioEngine::inputMonitoringMode)>(value);
	}
	else if (!strcmp(name, "trm")) {
		FlashStorage::defaultThresholdRecordingMode =
		    static_cast<decltype(FlashStorage::defaultThresholdRecordingMode)>(value);
	}
	else if (!strcmp(name, "lrc")) {
		FlashStorage::defaultLoopRecordingCommand =
		    static_cast<decltype(FlashStorage::defaultLoopRecordingCommand)>(value);
	}
	// Extended Defaults
	else if (!strcmp(name, "ssm")) {
		FlashStorage::defaultStartupSongMode = static_cast<decltype(FlashStorage::defaultStartupSongMode)>(value);
	}
	else if (!strcmp(name, "nct")) {
		FlashStorage::defaultNewClipType = static_cast<decltype(FlashStorage::defaultNewClipType)>(value);
	}
	else if (!strcmp(name, "ulct")) {
		FlashStorage::defaultUseLastClipType = (value != 0);
	}
	else if (!strcmp(name, "pcp")) {
		FlashStorage::defaultPatchCablePolarity = static_cast<decltype(FlashStorage::defaultPatchCablePolarity)>(value);
	}
	// Extended UI Settings
	else if (!strcmp(name, "sm")) {
		FlashStorage::defaultSliceMode = static_cast<decltype(FlashStorage::defaultSliceMode)>(value);
	}
	else if (!strcmp(name, "gam")) {
		FlashStorage::defaultGridActiveMode = static_cast<decltype(FlashStorage::defaultGridActiveMode)>(value);
	}
	else if (!strcmp(name, "fav")) {
		FlashStorage::defaultFavouritesLayout = static_cast<decltype(FlashStorage::defaultFavouritesLayout)>(value);
	}
	else if (!strcmp(name, "amh")) {
		FlashStorage::accessibilityMenuHighlighting =
		    static_cast<decltype(FlashStorage::accessibilityMenuHighlighting)>(value);
	}
	// Community Features (cf0-cf21+)
	else if (!strncmp(name, "cf", 2) && strlen(name) >= 3) {
		int featureIdx = atoi(name + 2); // Get number after "cf"
		if (featureIdx >= 0 && featureIdx < RuntimeFeatureSettingType::MaxElement) {
			RuntimeFeatureSettingType type = static_cast<RuntimeFeatureSettingType>(featureIdx);
			runtimeFeatureSettings.set(type, (RuntimeFeatureStateToggle)value);
		}
	}
	else {
		success = false;
	}

	if (success) {
		FlashStorage::writeSettings();
	}

	smSysex::startReply(jWriter, reader);
	if (success) {
		jWriter.writeOpeningTag("^settingSet", false, true);
		jWriter.writeAttribute("name", name);
		jWriter.writeAttribute("value", value);
		jWriter.closeTag(true);
	}
	else {
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", "Unknown setting");
		jWriter.closeTag(true);
	}
	smSysex::sendMsg(cable, jWriter);
}

void getSetting(MIDICable& cable, JsonDeserializer& reader) {
	String settingName;

	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "name")) {
			reader.readTagOrAttributeValueString(&settingName);
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	const char* name = settingName.get();
	int32_t value = 0;
	bool found = true;

	// Match ALL settings from getAllSettings (use short names)
	// MIDI
	if (!strcmp(name, "mt")) {
		value = midiEngine.midiThru ? 1 : 0;
	}
	else if (!strcmp(name, "mc")) {
		value = playbackHandler.midiInClockEnabled ? 1 : 0;
	}
	// Defaults
	else if (!strcmp(name, "ds")) {
		value = FlashStorage::defaultScale;
	}
	else if (!strcmp(name, "dv")) {
		value = FlashStorage::defaultVelocity;
	}
	else if (!strcmp(name, "dm")) {
		value = FlashStorage::defaultMagnitude;
	}
	else if (!strcmp(name, "br0")) {
		value = FlashStorage::defaultBendRange[0];
	}
	else if (!strcmp(name, "br1")) {
		value = FlashStorage::defaultBendRange[1];
	}
	// UI/Display
	else if (!strcmp(name, "pb")) {
		value = FlashStorage::defaultPadBrightness;
	}
	else if (!strcmp(name, "kl")) {
		value = (int32_t)FlashStorage::defaultKeyboardLayout;
	}
	else if (!strcmp(name, "sl")) {
		value = (int32_t)FlashStorage::defaultSessionLayout;
	}
	else if (!strcmp(name, "ht")) {
		value = FlashStorage::defaultHoldTime;
	}
	// Recording
	else if (!strcmp(name, "rq")) {
		value = FlashStorage::recordQuantizeLevel;
	}
	else if (!strcmp(name, "acm")) {
		value = FlashStorage::audioClipRecordMargins ? 1 : 0;
	}
	else if (!strcmp(name, "sbp")) {
		value = FlashStorage::sampleBrowserPreviewMode;
	}
	// Metronome/Swing
	else if (!strcmp(name, "mv")) {
		value = FlashStorage::defaultMetronomeVolume;
	}
	else if (!strcmp(name, "si")) {
		value = FlashStorage::defaultSwingInterval;
	}
	// Automation
	else if (!strcmp(name, "ai")) {
		value = FlashStorage::automationInterpolate ? 1 : 0;
	}
	else if (!strcmp(name, "aclr")) {
		value = FlashStorage::automationClear ? 1 : 0;
	}
	else if (!strcmp(name, "ash")) {
		value = FlashStorage::automationShift ? 1 : 0;
	}
	else if (!strcmp(name, "ann")) {
		value = FlashStorage::automationNudgeNote ? 1 : 0;
	}
	else if (!strcmp(name, "adps")) {
		value = FlashStorage::automationDisableAuditionPadShortcuts ? 1 : 0;
	}
	// Grid
	else if (!strcmp(name, "geu")) {
		value = FlashStorage::gridEmptyPadsUnarm ? 1 : 0;
	}
	else if (!strcmp(name, "gecr")) {
		value = FlashStorage::gridEmptyPadsCreateRec ? 1 : 0;
	}
	else if (!strcmp(name, "gags")) {
		value = FlashStorage::gridAllowGreenSelection ? 1 : 0;
	}
	// Keyboard
	else if (!strcmp(name, "kvg")) {
		value = FlashStorage::keyboardFunctionsVelocityGlide ? 1 : 0;
	}
	else if (!strcmp(name, "kmg")) {
		value = FlashStorage::keyboardFunctionsModwheelGlide ? 1 : 0;
	}
	// Misc
	else if (!strcmp(name, "ac")) {
		value = FlashStorage::accessibilityShortcuts ? 1 : 0;
	}
	else if (!strcmp(name, "cpu")) {
		value = FlashStorage::highCPUUsageIndicator ? 1 : 0;
	}
	else if (!strcmp(name, "sharp")) {
		value = FlashStorage::defaultUseSharps ? 1 : 0;
	}
	else {
		found = false;
	}

	smSysex::startReply(jWriter, reader);
	if (found) {
		jWriter.writeOpeningTag("^setting", false, true);
		jWriter.writeAttribute("name", name);
		jWriter.writeAttribute("value", value);
		jWriter.closeTag(true);
	}
	else {
		jWriter.writeOpeningTag("^error", false, true);
		jWriter.writeAttribute("message", "Unknown setting");
		jWriter.closeTag(true);
	}
	smSysex::sendMsg(cable, jWriter);
}

void getFirmwareVersion(MIDICable& cable, JsonDeserializer& reader) {
	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^firmware", false, true);

	// Get version info
	const FirmwareVersion& version = FirmwareVersion::current();
	jWriter.writeAttribute("major", (int32_t)version.version().major);
	jWriter.writeAttribute("minor", (int32_t)version.version().minor);
	jWriter.writeAttribute("patch", (int32_t)version.version().patch);
	jWriter.writeAttribute("type", version.type() == FirmwareVersion::Type::COMMUNITY ? "community" : "official");

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}

} // namespace SettingsSysex
