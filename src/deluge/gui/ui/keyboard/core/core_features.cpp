#include "gui/ui/keyboard/core/core_features.h"
#include "gui/ui/keyboard/keyboard_screen.h"
#include "gui/ui/ui.h"
#include "hid/display/display.h"
#include "model/clip/clip.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/kit.h"
#include "model/instrument/melodic_instrument.h"
#include "model/instrument/non_audio_instrument.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"

namespace deluge::gui::ui::keyboard::core {

// Timing and Playback
bool CoreFeatures::isPlaying() {
	return playbackHandler.isEitherClockActive();
}

float CoreFeatures::getCurrentBPM() {
	// Get BPM from current song - simplified for now
	// TODO: Find correct API for getting BPM
	return 120.0f; // Default BPM
}

uint32_t CoreFeatures::getBeatPosition() {
	uint32_t tick = playbackHandler.getActualSwungTickCount();
	return calculateBeatPosition(tick);
}

uint32_t CoreFeatures::getCurrentTick() {
	return playbackHandler.getActualSwungTickCount();
}

uint32_t CoreFeatures::getTicksPerBeat() {
	// Simplified for now - TODO: Find correct API
	return 96; // Default ticks per beat
}

uint32_t CoreFeatures::getTicksPerBar() {
	// Simplified for now - TODO: Find correct API
	return 384; // Default ticks per bar (4 beats)
}

// Audio Clip Information
bool CoreFeatures::isCurrentClipAudio() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	return clip && clip->output && clip->output->type == OutputType::AUDIO;
}

bool CoreFeatures::isCurrentClipInstrument() {
	// Simplified for now - TODO: Find correct API
	return true;
}

bool CoreFeatures::isCurrentClipKit() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	return clip && clip->output && clip->output->type == OutputType::KIT;
}

bool CoreFeatures::isCurrentClipMidi() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	return clip && clip->output && clip->output->type == OutputType::MIDI_OUT;
}

bool CoreFeatures::isCurrentClipCV() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	return clip && clip->output && clip->output->type == OutputType::CV;
}

// Waveform Rendering (simplified access)
void CoreFeatures::renderWaveform(RGB image[][kDisplayWidth + kSideBarWidth],
                                  uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], int32_t xOffset,
                                  int32_t yOffset, int32_t width, int32_t height) {
	// Simplified waveform rendering - just show a basic pattern
	// In a full implementation, this would use the actual waveform data
	for (int32_t y = yOffset; y < yOffset + height && y < kDisplayHeight; y++) {
		for (int32_t x = xOffset; x < xOffset + width && x < kDisplayWidth; x++) {
			// Simple sine wave pattern
			int32_t waveValue = (int32_t)(sin((x - xOffset) * 3.14159f / width) * height / 2);
			if (y == yOffset + height / 2 + waveValue) {
				image[y][x] = RGB(255, 255, 255); // White waveform line
				occupancyMask[y][x] = 255;
			}
		}
	}
}

// Arpeggiator Access
bool CoreFeatures::isArpeggiatorEnabled() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings && settings->mode != ArpMode::OFF;
}

void CoreFeatures::sendNoteToCurrentInstrument(int32_t noteCode, uint8_t velocity, bool isOn) {
	// This is a simplified implementation
	// In a full implementation, you'd need proper ModelStack setup
	char message[32];
	snprintf(message, sizeof(message), "Note %s: %d", isOn ? "On" : "Off", noteCode);
	showPopup(message);
}

// Arpeggiator Settings Access
bool CoreFeatures::getArpeggiatorMode() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings && settings->mode != ArpMode::OFF;
}

void CoreFeatures::setArpeggiatorMode(bool enabled) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	if (settings) {
		settings->mode = enabled ? ArpMode::ARP : ArpMode::OFF;
		settings->updatePresetFromCurrentSettings();
	}
}

int32_t CoreFeatures::getArpeggiatorPreset() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings ? (int32_t)settings->preset : 0;
}

void CoreFeatures::setArpeggiatorPreset(int32_t preset) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	if (settings) {
		settings->preset = (ArpPreset)preset;
		settings->updateSettingsFromCurrentPreset();
	}
}

int32_t CoreFeatures::getArpeggiatorOctaveMode() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings ? (int32_t)settings->octaveMode : 0;
}

void CoreFeatures::setArpeggiatorOctaveMode(int32_t mode) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	if (settings) {
		settings->octaveMode = (ArpOctaveMode)mode;
		settings->updatePresetFromCurrentSettings();
	}
}

int32_t CoreFeatures::getArpeggiatorNoteMode() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings ? (int32_t)settings->noteMode : 0;
}

void CoreFeatures::setArpeggiatorNoteMode(int32_t mode) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	if (settings) {
		settings->noteMode = (ArpNoteMode)mode;
		settings->updatePresetFromCurrentSettings();
	}
}

int32_t CoreFeatures::getArpeggiatorNumOctaves() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings ? settings->numOctaves : 2;
}

void CoreFeatures::setArpeggiatorNumOctaves(int32_t numOctaves) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	if (settings) {
		settings->numOctaves = (uint8_t)etl::clamp(numOctaves, (int32_t)1, (int32_t)4);
	}
}

int32_t CoreFeatures::getArpeggiatorSyncLevel() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings ? (int32_t)settings->syncLevel : 0;
}

void CoreFeatures::setArpeggiatorSyncLevel(int32_t syncLevel) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	if (settings) {
		settings->syncLevel = (SyncLevel)etl::clamp(syncLevel, (int32_t)0, (int32_t)15);
	}
}

int32_t CoreFeatures::getArpeggiatorSyncType() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings ? (int32_t)settings->syncType : 0;
}

void CoreFeatures::setArpeggiatorSyncType(int32_t syncType) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	if (settings) {
		settings->syncType = (SyncType)etl::clamp(syncType, (int32_t)0, (int32_t)3);
	}
}

int32_t CoreFeatures::getArpeggiatorStepRepeats() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings ? settings->numStepRepeats : 1;
}

void CoreFeatures::setArpeggiatorStepRepeats(int32_t stepRepeats) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	if (settings) {
		settings->numStepRepeats = (uint8_t)etl::clamp(stepRepeats, (int32_t)1, (int32_t)8);
	}
}

bool CoreFeatures::getArpeggiatorRandomizerLock() {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	return settings ? settings->randomizerLock : false;
}

void CoreFeatures::setArpeggiatorRandomizerLock(bool enabled) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	if (settings) {
		settings->randomizerLock = enabled;
	}
}

// Arpeggiator Note Management
void CoreFeatures::addNoteToArpeggiator(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	ArpeggiatorBase* arp = getCurrentArpeggiator();
	if (settings && arp) {
		ArpReturnInstruction instruction;
		arp->noteOn(settings, noteCode, velocity, &instruction, fromMIDIChannel, nullptr);
	}
}

void CoreFeatures::removeNoteFromArpeggiator(int32_t noteCode) {
	ArpeggiatorSettings* settings = getCurrentArpeggiatorSettings();
	ArpeggiatorBase* arp = getCurrentArpeggiator();
	if (settings && arp) {
		ArpReturnInstruction instruction;
		arp->noteOff(settings, noteCode, &instruction);
	}
}

void CoreFeatures::clearArpeggiatorNotes() {
	ArpeggiatorBase* arp = getCurrentArpeggiator();
	if (arp) {
		arp->reset();
	}
}

int32_t CoreFeatures::getArpeggiatorActiveNoteCount() {
	// Simplified for now - TODO: Find correct API to access arpeggiator notes
	return 0;
}

bool CoreFeatures::isNoteActiveInArpeggiator(int32_t noteCode) {
	// Simplified for now - TODO: Find correct API to access arpeggiator notes
	return false;
}

// Arpeggiator Playback Control
void CoreFeatures::resetArpeggiator() {
	ArpeggiatorBase* arp = getCurrentArpeggiator();
	if (arp) {
		arp->reset();
	}
}

void CoreFeatures::triggerArpeggiatorStep() {
	// Simplified for now - TODO: Find correct API to trigger arpeggiator step
	// The switchNoteOn method is protected, so we can't call it directly
}

bool CoreFeatures::isArpeggiatorGateActive() {
	ArpeggiatorBase* arp = getCurrentArpeggiator();
	return arp ? arp->gateCurrentlyActive : false;
}

int32_t CoreFeatures::getArpeggiatorCurrentNote() {
	ArpeggiatorBase* arp = getCurrentArpeggiator();
	return arp ? arp->whichNoteCurrentlyOnPostArp : -1;
}

int32_t CoreFeatures::getArpeggiatorCurrentOctave() {
	ArpeggiatorBase* arp = getCurrentArpeggiator();
	return arp ? arp->currentOctave : 0;
}

// Scale and Key Information
bool CoreFeatures::isScaleModeEnabled() {
	// Simplified for now - TODO: Find correct scale mode API
	return true;
}

int32_t CoreFeatures::getRootNote() {
	// Simplified for now - TODO: Find correct root note API
	return 60; // Middle C
}

int32_t CoreFeatures::getCurrentScale() {
	// Simplified for now - TODO: Find correct scale API
	return 0; // Major scale
}

int32_t CoreFeatures::getSongRootNote() {
	// Simplified for now - TODO: Find correct song root note API
	return 60; // Middle C
}

int32_t CoreFeatures::getSongScale() {
	// Simplified for now - TODO: Find correct scale API
	return 0; // Major scale
}

// UI Feedback
void CoreFeatures::showPopup(const char* message) {
	display->displayPopup(message);
}

// Low-level access
uint32_t CoreFeatures::getAudioSampleTimer() {
	// Simplified for now - TODO: Find correct audio timer API
	return 0;
}

// Private helper functions
InstrumentClip* CoreFeatures::getCurrentInstrumentClip() {
	// Simplified for now - TODO: Find correct API
	return nullptr;
}

ArpeggiatorSettings* CoreFeatures::getCurrentArpeggiatorSettings() {
	// Simplified for now - TODO: Find correct API to access arpeggiator settings
	// The getArpSettings methods are private in some instruments
	return nullptr;
}

ArpeggiatorBase* CoreFeatures::getCurrentArpeggiator() {
	// Simplified for now - TODO: Find correct API to access arpeggiator
	return nullptr;
}

uint32_t CoreFeatures::calculateBeatPosition(uint32_t tick) {
	uint32_t ticksPerBeat = getTicksPerBeat();
	return tick / ticksPerBeat;
}

} // namespace deluge::gui::ui::keyboard::core
