#include "core_features.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "hid/display/oled.h"
#include "hid/led/pad_leds.h"
#include "io/debug/print.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/instrument.h"
#include "model/instrument/kit.h"
#include "model/instrument/melodic_instrument.h"
#include "model/instrument/non_audio_instrument.h"
#include "model/song/song.h"
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
	// TODO: Find correct API for getting current tick
	return 0;
}

int32_t TimingController::getCurrentBPM() const {
	// TODO: Find correct API for getting BPM
	return 120;
}

void TimingController::setBPM(int32_t bpm) {
	// TODO: Find correct API for setting BPM
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
// AudioController Implementation
// ============================================================================

bool AudioController::isCurrentClipInstrument() const {
	// TODO: Find correct API for accessing current clip
	return false;
}

bool AudioController::isCurrentClipAudio() const {
	// TODO: Find correct API for accessing current clip
	return false;
}

void AudioController::sendNoteToCurrentInstrument(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel) {
	// TODO: Find correct API for sending notes
}

void AudioController::stopNoteOnCurrentInstrument(int32_t noteCode, int32_t fromMIDIChannel) {
	// TODO: Find correct API for stopping notes
}

class InstrumentClip* AudioController::getCurrentInstrumentClip() {
	// TODO: Find correct API for getting current instrument clip
	return nullptr;
}

void AudioController::renderWaveform(RGB image[][kDisplayWidth + kSideBarWidth],
                                     uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], int32_t xScroll,
                                     int32_t xZoom, int32_t whichKernel, int32_t whichKernelStartedThis) {
	// TODO: Implement waveform rendering
	// This would integrate with the existing waveform rendering system
}

bool AudioController::isAudioClip() const {
	// TODO: Find correct API for checking audio clip
	return false;
}

// ============================================================================
// ArpeggiatorController Implementation
// ============================================================================

void ArpeggiatorController::addNote(int32_t noteCode, uint8_t velocity, int32_t fromMIDIChannel) {
	// TODO: Implement proper note addition to arpeggiator
	// This would integrate with the actual arpeggiator system
}

void ArpeggiatorController::removeNote(int32_t noteCode) {
	// TODO: Implement proper note removal from arpeggiator
}

void ArpeggiatorController::clearNotes() {
	// TODO: Implement proper note clearing
}

void ArpeggiatorController::reset() {
	// TODO: Implement proper arpeggiator reset
}

void ArpeggiatorController::triggerStep() {
	// TODO: Implement proper arpeggiator step triggering
}

int32_t ArpeggiatorController::getActiveNoteCount() const {
	// TODO: Implement proper note count retrieval
	return 0;
}

bool ArpeggiatorController::isNoteActive(int32_t noteCode) const {
	// TODO: Implement proper note active check
	return false;
}

bool ArpeggiatorController::isGateActive() const {
	// TODO: Implement proper gate active check
	return false;
}

int32_t ArpeggiatorController::getCurrentNote() const {
	// TODO: Implement proper current note retrieval
	return 0;
}

int32_t ArpeggiatorController::getCurrentOctave() const {
	// TODO: Implement proper current octave retrieval
	return 0;
}

bool ArpeggiatorController::isEnabled() const {
	return settings_.mode != ArpMode::OFF;
}

void ArpeggiatorController::setEnabled(bool enabled) {
	settings_.mode = enabled ? ArpMode::ARP : ArpMode::OFF;
	settings_.updatePresetFromCurrentSettings();
}

// ============================================================================
// EffectsController Implementation
// ============================================================================

// ReverbEffect Implementation
bool EffectsController::ReverbEffect::isEnabled() const {
	// Check if we have an audio output with effects
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return false;
	}

	// Get the param manager for the clip
	ParamManagerForTimeline& paramManager = clip->paramManager;

	// Check if reverb send amount is greater than minimum (disabled)
	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t reverbAmount = unpatchedParams->getValue(UNPATCHED_REVERB_SEND_AMOUNT);
	return reverbAmount > std::numeric_limits<q31_t>::min();
}

void EffectsController::ReverbEffect::setEnabled(bool enabled) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

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

float EffectsController::ReverbEffect::getRoomSize() const {
	// TODO: Implement room size access - this would require accessing reverb engine parameters
	return roomSize_;
}

void EffectsController::ReverbEffect::setRoomSize(float roomSize) {
	roomSize_ = roomSize;
	// TODO: Implement room size setting - this would require accessing reverb engine parameters
}

float EffectsController::ReverbEffect::getDamping() const {
	// TODO: Implement damping access - this would require accessing reverb engine parameters
	return damping_;
}

void EffectsController::ReverbEffect::setDamping(float damping) {
	damping_ = damping;
	// TODO: Implement damping setting - this would require accessing reverb engine parameters
}

float EffectsController::ReverbEffect::getWidth() const {
	// TODO: Implement width access - this would require accessing reverb engine parameters
	return width_;
}

void EffectsController::ReverbEffect::setWidth(float width) {
	width_ = width;
	// TODO: Implement width setting - this would require accessing reverb engine parameters
}

float EffectsController::ReverbEffect::getLPF() const {
	// TODO: Implement LPF access - this would require accessing reverb engine parameters
	return lpf_;
}

void EffectsController::ReverbEffect::setLPF(float lpf) {
	lpf_ = lpf;
	// TODO: Implement LPF setting - this would require accessing reverb engine parameters
}

float EffectsController::ReverbEffect::getHPF() const {
	// TODO: Implement HPF access - this would require accessing reverb engine parameters
	return hpf_;
}

void EffectsController::ReverbEffect::setHPF(float hpf) {
	hpf_ = hpf;
	// TODO: Implement HPF setting - this would require accessing reverb engine parameters
}

int32_t EffectsController::ReverbEffect::getSendAmount() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return 0;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_REVERB_SEND_AMOUNT);

	// Convert from Q31 to percentage (0-100)
	if (rawValue == std::numeric_limits<q31_t>::min()) {
		return 0; // Disabled
	}

	// Convert Q31 to percentage
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::ReverbEffect::setSendAmount(int32_t amount) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	if (amount <= 0) {
		// Disable reverb
		unpatchedParams->params[UNPATCHED_REVERB_SEND_AMOUNT].setCurrentValueBasicForSetup(
		    std::numeric_limits<q31_t>::min());
	}
	else {
		// Convert percentage to Q31
		int32_t q31Value = (amount * 4294967296) / 100 - 2147483648;
		unpatchedParams->params[UNPATCHED_REVERB_SEND_AMOUNT].setCurrentValueBasicForSetup(q31Value);
	}
}

// DelayEffect Implementation
bool EffectsController::DelayEffect::isEnabled() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return false;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t delayAmount = unpatchedParams->getValue(UNPATCHED_DELAY_AMOUNT);
	return delayAmount > std::numeric_limits<q31_t>::min();
}

void EffectsController::DelayEffect::setEnabled(bool enabled) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

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
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return 0;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_DELAY_RATE);

	// Convert from Q31 to sync level (0-7)
	// This is a simplified conversion - actual implementation would need to decode the sync level
	return (rawValue + 2147483648) / (4294967296 / 8);
}

void EffectsController::DelayEffect::setSyncLevel(int32_t syncLevel) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert sync level to Q31 value
	int32_t q31Value = (syncLevel * 4294967296) / 8 - 2147483648;
	unpatchedParams->params[UNPATCHED_DELAY_RATE].setCurrentValueBasicForSetup(q31Value);
}

int32_t EffectsController::DelayEffect::getSyncType() const {
	// TODO: Implement sync type access - this would require accessing delay engine parameters
	return syncType_;
}

void EffectsController::DelayEffect::setSyncType(int32_t syncType) {
	syncType_ = syncType;
	// TODO: Implement sync type setting - this would require accessing delay engine parameters
}

int32_t EffectsController::DelayEffect::getFeedbackAmount() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return 0;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_DELAY_AMOUNT);

	// Convert from Q31 to percentage (0-100)
	if (rawValue == std::numeric_limits<q31_t>::min()) {
		return 0; // Disabled
	}

	// Convert Q31 to percentage
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::DelayEffect::setFeedbackAmount(int32_t amount) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	if (amount <= 0) {
		// Disable delay
		unpatchedParams->params[UNPATCHED_DELAY_AMOUNT].setCurrentValueBasicForSetup(std::numeric_limits<q31_t>::min());
	}
	else {
		// Convert percentage to Q31
		int32_t q31Value = (amount * 4294967296) / 100 - 2147483648;
		unpatchedParams->params[UNPATCHED_DELAY_AMOUNT].setCurrentValueBasicForSetup(q31Value);
	}
}

bool EffectsController::DelayEffect::isPingPong() const {
	// TODO: Implement ping pong access - this would require accessing delay engine parameters
	return pingPong_;
}

void EffectsController::DelayEffect::setPingPong(bool pingPong) {
	pingPong_ = pingPong;
	// TODO: Implement ping pong setting - this would require accessing delay engine parameters
}

bool EffectsController::DelayEffect::isAnalog() const {
	// TODO: Implement analog access - this would require accessing delay engine parameters
	return analog_;
}

void EffectsController::DelayEffect::setAnalog(bool analog) {
	analog_ = analog;
	// TODO: Implement analog setting - this would require accessing delay engine parameters
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
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

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
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return 0;
	}

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
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	if (frequency >= 100) {
		// Disable LPF
		unpatchedParams->params[UNPATCHED_LPF_FREQ].setCurrentValueBasicForSetup(ONE_Q31);
	}
	else {
		// Convert percentage to Q31
		int32_t q31Value = (frequency * 4294967296) / 100 - 2147483648;
		unpatchedParams->params[UNPATCHED_LPF_FREQ].setCurrentValueBasicForSetup(q31Value);
	}
}

int32_t EffectsController::FilterEffect::getLPFResonance() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return 0;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_LPF_RES);

	// Convert from Q31 to percentage (0-100)
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setLPFResonance(int32_t resonance) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert percentage to Q31
	int32_t q31Value = (resonance * 4294967296) / 100 - 2147483648;
	unpatchedParams->params[UNPATCHED_LPF_RES].setCurrentValueBasicForSetup(q31Value);
}

int32_t EffectsController::FilterEffect::getLPFMode() const {
	// TODO: Implement LPF mode access - this would require accessing filter engine parameters
	return lpfMode_;
}

void EffectsController::FilterEffect::setLPFMode(int32_t mode) {
	lpfMode_ = mode;
	// TODO: Implement LPF mode setting - this would require accessing filter engine parameters
}

int32_t EffectsController::FilterEffect::getLPFMorph() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return 0;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_LPF_MORPH);

	// Convert from Q31 to percentage (0-100)
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setLPFMorph(int32_t morph) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert percentage to Q31
	int32_t q31Value = (morph * 4294967296) / 100 - 2147483648;
	unpatchedParams->params[UNPATCHED_LPF_MORPH].setCurrentValueBasicForSetup(q31Value);
}

int32_t EffectsController::FilterEffect::getHPFFrequency() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return 0;
	}

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
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	if (frequency <= 0) {
		// Disable HPF
		unpatchedParams->params[UNPATCHED_HPF_FREQ].setCurrentValueBasicForSetup(std::numeric_limits<q31_t>::min());
	}
	else {
		// Convert percentage to Q31
		int32_t q31Value = (frequency * 4294967296) / 100 - 2147483648;
		unpatchedParams->params[UNPATCHED_HPF_FREQ].setCurrentValueBasicForSetup(q31Value);
	}
}

int32_t EffectsController::FilterEffect::getHPFResonance() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return 0;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_HPF_RES);

	// Convert from Q31 to percentage (0-100)
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setHPFResonance(int32_t resonance) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert percentage to Q31
	int32_t q31Value = (resonance * 4294967296) / 100 - 2147483648;
	unpatchedParams->params[UNPATCHED_HPF_RES].setCurrentValueBasicForSetup(q31Value);
}

int32_t EffectsController::FilterEffect::getHPFMode() const {
	// TODO: Implement HPF mode access - this would require accessing filter engine parameters
	return hpfMode_;
}

void EffectsController::FilterEffect::setHPFMode(int32_t mode) {
	hpfMode_ = mode;
	// TODO: Implement HPF mode setting - this would require accessing filter engine parameters
}

int32_t EffectsController::FilterEffect::getHPFMorph() const {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return 0;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();
	int32_t rawValue = unpatchedParams->getValue(UNPATCHED_HPF_MORPH);

	// Convert from Q31 to percentage (0-100)
	return ((rawValue + 2147483648) * 100) / 4294967296;
}

void EffectsController::FilterEffect::setHPFMorph(int32_t morph) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (!clip || clip->output->type != OutputType::AUDIO) {
		return;
	}

	ParamManagerForTimeline& paramManager = clip->paramManager;

	UnpatchedParamSet* unpatchedParams = paramManager.getUnpatchedParamSet();

	// Convert percentage to Q31
	int32_t q31Value = (morph * 4294967296) / 100 - 2147483648;
	unpatchedParams->params[UNPATCHED_HPF_MORPH].setCurrentValueBasicForSetup(q31Value);
}

int32_t EffectsController::FilterEffect::getRouting() const {
	// TODO: Implement routing access - this would require accessing filter engine parameters
	return routing_;
}

void EffectsController::FilterEffect::setRouting(int32_t routing) {
	routing_ = routing;
	// TODO: Implement routing setting - this would require accessing filter engine parameters
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
