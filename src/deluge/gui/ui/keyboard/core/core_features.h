#pragma once

#include "gui/l10n/l10n.h"
#include "hid/led/pad_leds.h"
#include "modulation/arpeggiator.h"
#include <cstdint>

namespace deluge::gui::ui::keyboard::core {

class CoreFeatures {
public:
	// Timing and Playback
	static bool isPlaying();
	static float getCurrentBPM();
	static uint32_t getBeatPosition();
	static uint32_t getCurrentTick();
	static uint32_t getTicksPerBeat();
	static uint32_t getTicksPerBar();

	// Audio Clip Information
	static bool isCurrentClipAudio();
	static bool isCurrentClipInstrument();
	static bool isCurrentClipKit();
	static bool isCurrentClipMidi();
	static bool isCurrentClipCV();

	// Waveform Rendering (simplified access)
	static void renderWaveform(RGB image[][kDisplayWidth + kSideBarWidth],
	                           uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], int32_t xOffset, int32_t yOffset,
	                           int32_t width, int32_t height);

	// Arpeggiator Access
	static bool isArpeggiatorEnabled();
	static void sendNoteToCurrentInstrument(int32_t noteCode, uint8_t velocity, bool isOn);

	// Arpeggiator Settings Access
	static bool getArpeggiatorMode();
	static void setArpeggiatorMode(bool enabled);
	static int32_t getArpeggiatorPreset();
	static void setArpeggiatorPreset(int32_t preset);
	static int32_t getArpeggiatorOctaveMode();
	static void setArpeggiatorOctaveMode(int32_t mode);
	static int32_t getArpeggiatorNoteMode();
	static void setArpeggiatorNoteMode(int32_t mode);
	static int32_t getArpeggiatorNumOctaves();
	static void setArpeggiatorNumOctaves(int32_t numOctaves);
	static int32_t getArpeggiatorSyncLevel();
	static void setArpeggiatorSyncLevel(int32_t syncLevel);
	static int32_t getArpeggiatorSyncType();
	static void setArpeggiatorSyncType(int32_t syncType);
	static int32_t getArpeggiatorStepRepeats();
	static void setArpeggiatorStepRepeats(int32_t stepRepeats);
	static bool getArpeggiatorRandomizerLock();
	static void setArpeggiatorRandomizerLock(bool enabled);

	// Arpeggiator Note Management
	static void addNoteToArpeggiator(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel = 0);
	static void removeNoteFromArpeggiator(int32_t noteCode);
	static void clearArpeggiatorNotes();
	static int32_t getArpeggiatorActiveNoteCount();
	static bool isNoteActiveInArpeggiator(int32_t noteCode);

	// Arpeggiator Playback Control
	static void resetArpeggiator();
	static void triggerArpeggiatorStep();
	static bool isArpeggiatorGateActive();
	static int32_t getArpeggiatorCurrentNote();
	static int32_t getArpeggiatorCurrentOctave();

	// Scale and Key Information
	static bool isScaleModeEnabled();
	static int32_t getRootNote();
	static int32_t getCurrentScale();
	static int32_t getSongRootNote();
	static int32_t getSongScale();

	// UI Feedback
	static void showPopup(const char* message);

	// Low-level access (use with caution)
	static uint32_t getAudioSampleTimer();

private:
	// Helper functions to get current clip/instrument
	static class InstrumentClip* getCurrentInstrumentClip();

	// Arpeggiator helper functions
	static ArpeggiatorSettings* getCurrentArpeggiatorSettings();
	static ArpeggiatorBase* getCurrentArpeggiator();

	// Internal helpers
	static uint32_t calculateBeatPosition(uint32_t tick);
};

} // namespace deluge::gui::ui::keyboard::core
