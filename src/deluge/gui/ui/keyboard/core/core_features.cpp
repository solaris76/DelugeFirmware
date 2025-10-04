#include "core_features.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "hid/display/oled.h"
#include "hid/led/pad_leds.h"
#include "io/debug/print.h"
#include "io/midi/midi_device_manager.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/instrument.h"
#include "model/instrument/kit.h"
#include "model/instrument/melodic_instrument.h"
#include "model/instrument/non_audio_instrument.h"
#include "model/song/song.h"
#include "modulation/arpeggiator.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "playback/playback_handler.h"
#include "storage/storage_manager.h"
#include "util/functions.h"
#include <cstring>
#include <limits>

namespace deluge::gui::ui::keyboard::core {

using namespace deluge::modulation::params;

// ============================================================================
// CoreFeatures Implementation
// ============================================================================

std::unique_ptr<CoreFeatures> CoreFeatures::create() {
	return std::unique_ptr<CoreFeatures>(new CoreFeatures());
}

CoreFeatures::CoreFeatures()
    : timing_(std::make_unique<TimingController>()), display_(std::make_unique<DisplayController>()),
      audio_(std::make_unique<AudioController>()), arpeggiator_(std::make_unique<ArpeggiatorController>()),
      effects_(std::make_unique<EffectsController>()), scale_(std::make_unique<ScaleController>()) {
}

// ============================================================================
// TimingController Implementation
// ============================================================================

bool TimingController::isPlaying() const {
	return playbackHandler.playbackState & PLAYBACK_CLOCK_INTERNAL_ACTIVE;
}

int32_t TimingController::getCurrentTick() const {
	return (int32_t)playbackHandler.getCurrentInternalTickCount();
}

int32_t TimingController::getCurrentBPM() const {
	return (int32_t)playbackHandler.calculateBPMForDisplay();
}

void TimingController::setBPM(int32_t bpm) {
	currentSong->setBPM((float)bpm, true);
}

void TimingController::play() {
	playbackHandler.playButtonPressed(0);
}

void TimingController::pause() {
	// TODO: Find correct API for pause
}

void TimingController::stop() {
	// TODO: Find correct API for stop
}

void TimingController::doTickForward() {
	// TODO: Find correct API for tick forward
}

void TimingController::doTickBackward() {
	// TODO: Find correct API for tick backward
}

// ============================================================================
// DisplayController Implementation
// ============================================================================

void DisplayController::requestRendering() {
	uiNeedsRendering(getCurrentUI(), 0xFFFFFFFF, 0xFFFFFFFF);
}

void DisplayController::setPadLED(int32_t x, int32_t y, int32_t color) {
	PadLEDs::set({x, y}, RGB(color));
}

void DisplayController::setPadLEDBrightness(int32_t x, int32_t y, int32_t brightness) {
	// TODO: Implement brightness control
	// For now, just set the color
	PadLEDs::set({x, y}, RGB(brightness, brightness, brightness));
}

void DisplayController::clearAllPads() {
	PadLEDs::clearAllPadsWithoutSending();
}

void DisplayController::showPopup(const char* message) {
	display->displayPopup(message);
}

void DisplayController::showPopup(int32_t number) {
	display->displayPopup(number);
}

// ============================================================================
// Fader Implementation
// ============================================================================

void DisplayController::Fader::setHorizontalFader(int32_t startX, int32_t y, int32_t value, int32_t maxValue,
                                                  uint32_t dimmedColor, uint32_t litColor) {
	// Clamp value to valid range
	value = etl::clamp(value, (int32_t)0, maxValue);

	// Calculate how many pads should be lit
	int32_t totalPads = 16; // x0-15
	int32_t litPads = (value * totalPads) / maxValue;

	// Set all pads in the fader range
	for (int32_t x = startX; x < startX + totalPads && x < 16; x++) {
		if (x - startX < litPads) {
			// Lit portion
			PadLEDs::set({x, y}, RGB(litColor));
		}
		else {
			// Dimmed portion
			PadLEDs::set({x, y}, RGB(dimmedColor));
		}
	}
}

void DisplayController::Fader::setVerticalFader(int32_t x, int32_t startY, int32_t value, int32_t maxValue,
                                                uint32_t dimmedColor, uint32_t litColor) {
	// Clamp value to valid range
	value = etl::clamp(value, (int32_t)0, maxValue);

	// Calculate how many pads should be lit
	int32_t totalPads = 16; // y0-15
	int32_t litPads = (value * totalPads) / maxValue;

	// Set all pads in the fader range
	for (int32_t y = startY; y < startY + totalPads && y < 16; y++) {
		if (y - startY < litPads) {
			// Lit portion
			PadLEDs::set({x, y}, RGB(litColor));
		}
		else {
			// Dimmed portion
			PadLEDs::set({x, y}, RGB(dimmedColor));
		}
	}
}

void DisplayController::Fader::setShortHorizontalFader(int32_t startX, int32_t y, int32_t value, int32_t maxValue,
                                                       uint32_t dimmedColor, uint32_t litColor) {
	// Clamp value to valid range
	value = etl::clamp(value, (int32_t)0, maxValue);

	// Calculate how many pads should be lit
	int32_t totalPads = 8; // x0-7
	int32_t litPads = (value * totalPads) / maxValue;

	// Set all pads in the fader range
	for (int32_t x = startX; x < startX + totalPads && x < 8; x++) {
		if (x - startX < litPads) {
			// Lit portion
			PadLEDs::set({x, y}, RGB(litColor));
		}
		else {
			// Dimmed portion
			PadLEDs::set({x, y}, RGB(dimmedColor));
		}
	}
}

int32_t DisplayController::Fader::getValueFromHorizontalFader(int32_t startX, int32_t y, int32_t padX,
                                                              int32_t maxValue) {
	// Calculate relative position within fader
	int32_t relativeX = padX - startX;
	int32_t totalPads = 16; // x0-15

	// Clamp relative position
	relativeX = etl::clamp(relativeX, (int32_t)0, totalPads - 1);

	// Convert pad position to value
	return (relativeX * maxValue) / (totalPads - 1);
}

int32_t DisplayController::Fader::getValueFromVerticalFader(int32_t x, int32_t startY, int32_t padY, int32_t maxValue) {
	// Calculate relative position within fader
	int32_t relativeY = padY - startY;
	int32_t totalPads = 16; // y0-15

	// Clamp relative position
	relativeY = etl::clamp(relativeY, (int32_t)0, totalPads - 1);

	// Convert pad position to value
	return (relativeY * maxValue) / (totalPads - 1);
}

int32_t DisplayController::Fader::getValueFromShortHorizontalFader(int32_t startX, int32_t y, int32_t padX,
                                                                   int32_t maxValue) {
	// Calculate relative position within fader
	int32_t relativeX = padX - startX;
	int32_t totalPads = 8; // x0-7

	// Clamp relative position
	relativeX = etl::clamp(relativeX, (int32_t)0, totalPads - 1);

	// Convert pad position to value
	return (relativeX * maxValue) / (totalPads - 1);
}

// ============================================================================
// AudioController Implementation
// ============================================================================

bool AudioController::isCurrentClipInstrument() const {
	return getCurrentClipOutputType() == OutputType::SYNTH || getCurrentClipOutputType() == OutputType::KIT
	       || getCurrentClipOutputType() == OutputType::CV || getCurrentClipOutputType() == OutputType::MIDI_OUT;
}

bool AudioController::isCurrentClipAudio() const {
	return getCurrentClipOutputType() == OutputType::AUDIO;
}

void AudioController::sendNoteToCurrentInstrument(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel) {
	OutputType outputType = getCurrentClipOutputType();
	if (outputType == OutputType::NONE) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || !clip->output) {
		return;
	}

	Output* output = clip->output;
	if (outputType == OutputType::KIT) {
		// For kits, we need to use the unscrolledPadAudition method
		// This is a simplified approach - in practice you'd need more context
		Kit* kit = (Kit*)output;
		kit->receivedNoteForKit(nullptr, MIDIDeviceManager::root_din.cable, true, fromMIDIChannel, noteCode, velocity,
		                        false, nullptr, clip);
	}
	else if (outputType == OutputType::SYNTH || outputType == OutputType::CV) {
		MelodicInstrument* melodicInstrument = (MelodicInstrument*)output;
		melodicInstrument->beginAuditioningForNote(nullptr, noteCode, velocity, nullptr);
	}
	else if (outputType == OutputType::MIDI_OUT) {
		NonAudioInstrument* midiInstrument = (NonAudioInstrument*)output;
		midiInstrument->receivedNote(nullptr, MIDIDeviceManager::root_din.cable, true, fromMIDIChannel,
		                             MIDIMatchType::NO_MATCH, noteCode, velocity, false, nullptr);
	}
}

void AudioController::stopNoteOnCurrentInstrument(int32_t noteCode, int32_t fromMIDIChannel) {
	OutputType outputType = getCurrentClipOutputType();
	if (outputType == OutputType::NONE) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || !clip->output) {
		return;
	}

	Output* output = clip->output;
	if (outputType == OutputType::KIT) {
		Kit* kit = (Kit*)output;
		kit->receivedNoteForKit(nullptr, MIDIDeviceManager::root_din.cable, false, fromMIDIChannel, noteCode, 0, false,
		                        nullptr, clip);
	}
	else if (outputType == OutputType::SYNTH || outputType == OutputType::CV) {
		MelodicInstrument* melodicInstrument = (MelodicInstrument*)output;
		melodicInstrument->endAuditioningForNote(nullptr, noteCode);
	}
	else if (outputType == OutputType::MIDI_OUT) {
		NonAudioInstrument* midiInstrument = (NonAudioInstrument*)output;
		midiInstrument->receivedNote(nullptr, MIDIDeviceManager::root_din.cable, false, fromMIDIChannel,
		                             MIDIMatchType::NO_MATCH, noteCode, 0, false, nullptr);
	}
}

class InstrumentClip* AudioController::getCurrentInstrumentClip() const {
	return ::getCurrentInstrumentClip();
}

bool AudioController::hasEffectsSupport() const {
	OutputType outputType = getCurrentClipOutputType();
	return outputType == OutputType::SYNTH || outputType == OutputType::KIT || outputType == OutputType::AUDIO;
}

bool AudioController::isValidInstrumentClip() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	return clip != nullptr && clip->output != nullptr;
}

void AudioController::renderWaveform(RGB image[][kDisplayWidth + kSideBarWidth],
                                     uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], int32_t xScroll,
                                     int32_t xZoom, int32_t whichKernel, int32_t whichKernelStartedThis) {
	// TODO: Implement waveform rendering
	// This would integrate with the existing waveform rendering system
}

OutputType AudioController::getCurrentClipOutputType() const {
	Clip* currentClip = getCurrentClip();
	if (!currentClip || !currentClip->output) {
		return OutputType::NONE;
	}
	return currentClip->output->type;
}

// ============================================================================
// ArpeggiatorController Implementation
// ============================================================================

// Helper function to get current arpeggiator settings
ArpeggiatorSettings* ArpeggiatorController::getCurrentArpSettings() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || !clip->output) {
		return nullptr;
	}

	Output* output = clip->output;

	// Get arp settings from the instrument
	if (output->type == OutputType::SYNTH || output->type == OutputType::CV) {
		MelodicInstrument* melodicInstrument = (MelodicInstrument*)output;
		return melodicInstrument->getArpSettings(clip);
	}
	else if (output->type == OutputType::KIT) {
		Kit* kit = (Kit*)output;
		// Access arp settings directly from the clip since getArpSettings is private
		return &clip->arpSettings;
	}
	else if (output->type == OutputType::MIDI_OUT) {
		NonAudioInstrument* midiInstrument = (NonAudioInstrument*)output;
		return midiInstrument->getArpSettings(clip);
	}

	return nullptr;
}

// Helper function to get current arpeggiator instance
ArpeggiatorBase* ArpeggiatorController::getCurrentArpeggiator() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || !clip->output) {
		return nullptr;
	}

	Output* output = clip->output;

	// Get arpeggiator from the instrument
	if (output->type == OutputType::SYNTH || output->type == OutputType::CV) {
		MelodicInstrument* melodicInstrument = (MelodicInstrument*)output;
		return &melodicInstrument->arpeggiator;
	}
	else if (output->type == OutputType::KIT) {
		Kit* kit = (Kit*)output;
		return &kit->arpeggiator;
	}
	else if (output->type == OutputType::MIDI_OUT) {
		NonAudioInstrument* midiInstrument = (NonAudioInstrument*)output;
		return &midiInstrument->arpeggiator;
	}

	return nullptr;
}

void ArpeggiatorController::addNote(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel) {
	ArpeggiatorSettings* arpSettings = getCurrentArpSettings();
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpSettings || !arpeggiator) {
		return;
	}

	// Create instruction for note on
	ArpReturnInstruction instruction;
	instruction.sampleSyncLengthOn = 0;

	// Add note to arpeggiator
	arpeggiator->noteOn(arpSettings, noteCode, velocity, &instruction, fromMIDIChannel, nullptr);
}

void ArpeggiatorController::removeNote(int32_t noteCode) {
	ArpeggiatorSettings* arpSettings = getCurrentArpSettings();
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpSettings || !arpeggiator) {
		return;
	}

	// Create instruction for note off
	ArpReturnInstruction instruction;

	// Remove note from arpeggiator
	arpeggiator->noteOff(arpSettings, noteCode, &instruction);
}

void ArpeggiatorController::clearNotes() {
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpeggiator) {
		return;
	}

	// Reset the arpeggiator which clears all notes
	arpeggiator->reset();
}

void ArpeggiatorController::reset() {
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpeggiator) {
		return;
	}

	// Reset the arpeggiator
	arpeggiator->reset();
}

void ArpeggiatorController::triggerStep() {
	ArpeggiatorSettings* arpSettings = getCurrentArpSettings();
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpSettings || !arpeggiator) {
		return;
	}

	// Trigger a step by calling doTickForward
	ArpReturnInstruction instruction;
	arpeggiator->doTickForward(arpSettings, &instruction, 0, false);
}

int32_t ArpeggiatorController::getActiveNoteCount() const {
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpeggiator) {
		return 0;
	}

	// Check if any notes are active using the base class method
	return arpeggiator->hasAnyInputNotesActive() ? 1 : 0;
}

bool ArpeggiatorController::isNoteActive(int32_t noteCode) const {
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpeggiator) {
		return false;
	}

	// For now, use a simplified approach since we can't use dynamic_cast
	// Check if any notes are active and assume the note is active if arpeggiator is active
	return arpeggiator->hasAnyInputNotesActive();
}

bool ArpeggiatorController::isGateActive() const {
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpeggiator) {
		return false;
	}

	return arpeggiator->gateCurrentlyActive;
}

int32_t ArpeggiatorController::getCurrentNote() const {
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpeggiator) {
		return 0;
	}

	// Return the first currently playing note
	if (arpeggiator->noteCodeCurrentlyOnPostArp[0] != ARP_NOTE_NONE) {
		return arpeggiator->noteCodeCurrentlyOnPostArp[0];
	}

	return 0;
}

int32_t ArpeggiatorController::getCurrentOctave() const {
	ArpeggiatorBase* arpeggiator = getCurrentArpeggiator();

	if (!arpeggiator) {
		return 0;
	}

	return arpeggiator->currentOctave;
}

bool ArpeggiatorController::isEnabled() const {
	ArpeggiatorSettings* arpSettings = getCurrentArpSettings();
	if (!arpSettings) {
		return settings_.mode != ArpMode::OFF;
	}
	return arpSettings->mode != ArpMode::OFF;
}

void ArpeggiatorController::setEnabled(bool enabled) {
	ArpeggiatorSettings* arpSettings = getCurrentArpSettings();
	if (arpSettings) {
		arpSettings->mode = enabled ? ArpMode::ARP : ArpMode::OFF;
		arpSettings->updatePresetFromCurrentSettings();
	}
	else {
		settings_.mode = enabled ? ArpMode::ARP : ArpMode::OFF;
		settings_.updatePresetFromCurrentSettings();
	}
}

// ============================================================================
// EffectsController Implementation
// ============================================================================

bool EffectsController::isValidForEffects() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip) {
		return false;
	}

	OutputType outputType = clip->output->type;
	return outputType == OutputType::SYNTH || outputType == OutputType::KIT || outputType == OutputType::AUDIO;
}

// Static helper function for nested effect classes
static bool isValidForEffectsStatic() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip) {
		return false;
	}

	OutputType outputType = clip->output->type;
	return outputType == OutputType::SYNTH || outputType == OutputType::KIT || outputType == OutputType::AUDIO;
}

// Static helper function for parameter access
static ParamManagerForTimeline* getParamManagerStatic() {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip) {
		return nullptr;
	}
	return &clip->paramManager;
}

// Q31 conversion utilities
namespace Q31Utils {
// Convert percentage (0-100) to Q31 format
static int32_t percentageToQ31(int32_t percentage) {
	return (percentage * 4294967296) / 100 - 2147483648;
}

// Convert Q31 format to percentage (0-100)
static int32_t q31ToPercentage(int32_t q31Value) {
	return ((q31Value + 2147483648) * 100) / 4294967296;
}
} // namespace Q31Utils

// Parameter setting utilities
namespace ParamUtils {
// Set a parameter value safely
static void setParameter(UnpatchedParamSet* unpatchedParams, int32_t paramId, int32_t q31Value) {
	if (unpatchedParams) {
		unpatchedParams->params[paramId].setCurrentValueBasicForSetup(q31Value);
	}
}

// Set a parameter from percentage
static void setParameterFromPercentage(UnpatchedParamSet* unpatchedParams, int32_t paramId, int32_t percentage) {
	setParameter(unpatchedParams, paramId, Q31Utils::percentageToQ31(percentage));
}

// Get a parameter as percentage
static int32_t getParameterAsPercentage(UnpatchedParamSet* unpatchedParams, int32_t paramId) {
	if (!unpatchedParams) {
		return 0;
	}
	int32_t rawValue = unpatchedParams->getValue(paramId);
	return Q31Utils::q31ToPercentage(rawValue);
}
} // namespace ParamUtils

// ReverbEffect Implementation
bool EffectsController::ReverbEffect::isEnabled() const {
	// Use helper function to check if effects are supported
	if (!isValidForEffectsStatic()) {
		return false;
	}

	ParamManagerForTimeline* paramManager = getParamManagerStatic();
	if (!paramManager) {
		return false;
	}

	// Check if reverb send amount is greater than minimum (disabled)
	UnpatchedParamSet* unpatchedParams = paramManager->getUnpatchedParamSet();
	int32_t reverbAmount = unpatchedParams->getValue(UNPATCHED_REVERB_SEND_AMOUNT);
	return reverbAmount > std::numeric_limits<q31_t>::min();
}

void EffectsController::ReverbEffect::setEnabled(bool enabled) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	if (enabled) {
		// Set to a reasonable default value (50% send)
		unpatchedParams->params[UNPATCHED_REVERB_SEND_AMOUNT].setCurrentValueBasicForSetup(0);
	}
	else {
		// Set to minimum value (disabled)
		unpatchedParams->params[UNPATCHED_REVERB_SEND_AMOUNT].setCurrentValueBasicForSetup(
		    std::numeric_limits<q31_t>::min());
	}
}

int32_t EffectsController::ReverbEffect::getSendAmount() const {
	if (!isValidForEffectsStatic()) {
		return 0;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_REVERB_SEND_AMOUNT);

	// Convert from Q31 to percentage (0-100)
	if (rawValue == std::numeric_limits<q31_t>::min()) {
		return 0; // Disabled
	}

	// Convert Q31 to percentage
	return Q31Utils::q31ToPercentage(rawValue);
}

void EffectsController::ReverbEffect::setSendAmount(int32_t amount) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	if (amount <= 0) {
		// Disable reverb
		unpatchedParams->params[UNPATCHED_REVERB_SEND_AMOUNT].setCurrentValueBasicForSetup(
		    std::numeric_limits<q31_t>::min());
	}
	else {
		// Convert percentage to Q31
		ParamUtils::setParameterFromPercentage(unpatchedParams, UNPATCHED_REVERB_SEND_AMOUNT, amount);
	}
}

// DelayEffect Implementation
bool EffectsController::DelayEffect::isEnabled() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip) {
		return false;
	}

	// Effects are available on SYNTH, KIT, and AUDIO output types
	OutputType outputType = clip->output->type;
	if (outputType != OutputType::SYNTH && outputType != OutputType::KIT && outputType != OutputType::AUDIO) {
		return false;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t delayAmount = unpatchedParams->getValue(UNPATCHED_DELAY_AMOUNT);
	return delayAmount > std::numeric_limits<q31_t>::min();
}

void EffectsController::DelayEffect::setEnabled(bool enabled) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	if (enabled) {
		// Set to a reasonable default value (30% feedback)
		unpatchedParams->params[UNPATCHED_DELAY_AMOUNT].setCurrentValueBasicForSetup(-1073741824); // -25% in Q31
	}
	else {
		// Set to minimum value (disabled)
		unpatchedParams->params[UNPATCHED_DELAY_AMOUNT].setCurrentValueBasicForSetup(std::numeric_limits<q31_t>::min());
	}
}

int32_t EffectsController::DelayEffect::getSyncLevel() const {
	if (!isValidForEffectsStatic()) {
		return 0;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_DELAY_RATE);

	// Convert from Q31 to sync level (0-7)
	// This is a simplified conversion - actual implementation would need to decode the sync level
	return (rawValue + 2147483648) / (4294967296 / 8);
}

void EffectsController::DelayEffect::setSyncLevel(int32_t syncLevel) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert sync level to Q31 value
	int32_t q31Value = (syncLevel * 4294967296) / 8 - 2147483648;
	unpatchedParams->params[UNPATCHED_DELAY_RATE].setCurrentValueBasicForSetup(q31Value);
}

int32_t EffectsController::DelayEffect::getFeedbackAmount() const {
	if (!isValidForEffectsStatic()) {
		return 0;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_DELAY_AMOUNT);

	// Convert from Q31 to percentage (0-100)
	if (rawValue == std::numeric_limits<q31_t>::min()) {
		return 0; // Disabled
	}

	// Convert Q31 to percentage
	return Q31Utils::q31ToPercentage(rawValue);
}

void EffectsController::DelayEffect::setFeedbackAmount(int32_t amount) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	if (amount <= 0) {
		// Disable delay
		unpatchedParams->params[UNPATCHED_DELAY_AMOUNT].setCurrentValueBasicForSetup(std::numeric_limits<q31_t>::min());
	}
	else {
		// Convert percentage to Q31
		int32_t q31Value = Q31Utils::percentageToQ31(amount);
		unpatchedParams->params[UNPATCHED_DELAY_AMOUNT].setCurrentValueBasicForSetup(q31Value);
	}
}

// FilterEffect Implementation
bool EffectsController::FilterEffect::isEnabled() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return false;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Check if either LPF or HPF is active
	int32_t lpfFreq = unpatchedParams->getValue(UNPATCHED_LPF_FREQ);
	int32_t hpfFreq = unpatchedParams->getValue(UNPATCHED_HPF_FREQ);

	return (lpfFreq < ONE_Q31) || (hpfFreq > std::numeric_limits<q31_t>::min());
}

void EffectsController::FilterEffect::setEnabled(bool enabled) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	if (enabled) {
		// Set reasonable default values
		unpatchedParams->params[UNPATCHED_LPF_FREQ].setCurrentValueBasicForSetup(0); // Half way
		unpatchedParams->params[UNPATCHED_HPF_FREQ].setCurrentValueBasicForSetup(
		    std::numeric_limits<q31_t>::min()); // Disabled
	}
	else {
		// Disable both filters
		unpatchedParams->params[UNPATCHED_LPF_FREQ].setCurrentValueBasicForSetup(ONE_Q31); // Maximum (disabled)
		unpatchedParams->params[UNPATCHED_HPF_FREQ].setCurrentValueBasicForSetup(
		    std::numeric_limits<q31_t>::min()); // Disabled
	}
}

int32_t EffectsController::FilterEffect::getLPFFrequency() const {
	if (!isValidForEffectsStatic()) {
		return 0;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_LPF_FREQ);

	// Convert from Q31 to frequency (0-100)
	if (rawValue >= ONE_Q31) {
		return 100; // Maximum frequency (disabled)
	}

	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setLPFFrequency(int32_t frequency) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	if (frequency >= 100) {
		// Disable LPF
		unpatchedParams->params[UNPATCHED_LPF_FREQ].setCurrentValueBasicForSetup(ONE_Q31);
	}
	else {
		// Convert percentage to Q31
		int32_t q31Value = Q31Utils::percentageToQ31(frequency);
		unpatchedParams->params[UNPATCHED_LPF_FREQ].setCurrentValueBasicForSetup(q31Value);
	}
}

int32_t EffectsController::FilterEffect::getLPFResonance() const {
	if (!isValidForEffectsStatic()) {
		return 0;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_LPF_RES);

	// Convert from Q31 to percentage (0-100)
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setLPFResonance(int32_t resonance) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert percentage to Q31
	int32_t q31Value = Q31Utils::percentageToQ31(resonance);
	unpatchedParams->params[UNPATCHED_LPF_RES].setCurrentValueBasicForSetup(q31Value);
}

int32_t EffectsController::FilterEffect::getLPFMorph() const {
	if (!isValidForEffectsStatic()) {
		return 0;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_LPF_MORPH);

	// Convert from Q31 to percentage (0-100)
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setLPFMorph(int32_t morph) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert percentage to Q31
	int32_t q31Value = Q31Utils::percentageToQ31(morph);
	unpatchedParams->params[UNPATCHED_LPF_MORPH].setCurrentValueBasicForSetup(q31Value);
}

int32_t EffectsController::FilterEffect::getHPFFrequency() const {
	if (!isValidForEffectsStatic()) {
		return 0;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_HPF_FREQ);

	// Convert from Q31 to frequency (0-100)
	if (rawValue == std::numeric_limits<q31_t>::min()) {
		return 0; // Disabled
	}

	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setHPFFrequency(int32_t frequency) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	if (frequency <= 0) {
		// Disable HPF
		unpatchedParams->params[UNPATCHED_HPF_FREQ].setCurrentValueBasicForSetup(std::numeric_limits<q31_t>::min());
	}
	else {
		// Convert percentage to Q31
		int32_t q31Value = Q31Utils::percentageToQ31(frequency);
		unpatchedParams->params[UNPATCHED_HPF_FREQ].setCurrentValueBasicForSetup(q31Value);
	}
}

int32_t EffectsController::FilterEffect::getHPFResonance() const {
	if (!isValidForEffectsStatic()) {
		return 0;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_HPF_RES);

	// Convert from Q31 to percentage (0-100)
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setHPFResonance(int32_t resonance) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert percentage to Q31
	int32_t q31Value = Q31Utils::percentageToQ31(resonance);
	unpatchedParams->params[UNPATCHED_HPF_RES].setCurrentValueBasicForSetup(q31Value);
}

int32_t EffectsController::FilterEffect::getHPFMorph() const {
	if (!isValidForEffectsStatic()) {
		return 0;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_HPF_MORPH);

	// Convert from Q31 to percentage (0-100)
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setHPFMorph(int32_t morph) {
	if (!isValidForEffectsStatic()) {
		return;
	}

	InstrumentClip* clip = getCurrentInstrumentClip();

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert percentage to Q31
	int32_t q31Value = Q31Utils::percentageToQ31(morph);
	unpatchedParams->params[UNPATCHED_HPF_MORPH].setCurrentValueBasicForSetup(q31Value);
}

// ============================================================================
// ScaleController Implementation
// ============================================================================

bool ScaleController::isScaleModeEnabled() const {
	return scaleModeEnabled_;
}

int32_t ScaleController::getRootNote() const {
	return rootNote_;
}

int32_t ScaleController::getCurrentScale() const {
	return currentScale_;
}

int32_t ScaleController::getSongRootNote() const {
	return songRootNote_;
}

int32_t ScaleController::getSongScale() const {
	return songScale_;
}

} // namespace deluge::gui::ui::keyboard::core
