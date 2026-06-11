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

#pragma once

#include "definitions_cxx.hpp"
#include "model/drum/non_audio_drum.h"
#include "util/containers.h"
#include <array>
#include <string_view>

class ModelStackWithAutoParam;

class MIDIDrum final : public NonAudioDrum {
public:
	MIDIDrum();

	void noteOn(ModelStackWithThreeMainThings* modelStack, uint8_t velocity, int16_t const* mpeValues,
	            int32_t fromMIDIChannel = MIDI_CHANNEL_NONE, uint32_t sampleSyncLength = 0, int32_t ticksLate = 0,
	            uint32_t samplesLate = 0) override;
	void noteOff(ModelStackWithThreeMainThings* modelStack, int32_t velocity = kDefaultLiftValue) override;
	void noteOnPostArp(int32_t noteCodePostArp, ArpNote* arpNote, int32_t noteIndex) override;
	void noteOffPostArp(int32_t noteCodePostArp) override;
	void writeToFile(Serializer& writer, bool savingSong, ParamManager* paramManager) override;
	Error readFromFile(Deserializer& reader, Song* song, Clip* clip, int32_t readAutomationUpToPos) override;

	/// Read mod knob CC assignments from file
	Error readModKnobAssignmentsFromFile(Deserializer& reader, int32_t readAutomationUpToPos);
	/// Write mod knob CC assignments to file
	void writeModKnobAssignmentsToFile(Serializer& writer);
	void getName(char* buffer) override;
	int32_t getNumChannels() override { return 16; }
	void killAllVoices() override;

	// NOTE: modEncoderAction no longer overridden - uses default flow for MIDI CC automation
	bool modEncoderButtonAction(uint8_t whichModEncoder, bool on, ModelStackWithThreeMainThings* modelStack);
	void modButtonAction(uint8_t whichModButton, bool on);

	void expressionEvent(int32_t newValue, int32_t expressionDimension) override;

	void polyphonicExpressionEventOnChannelOrNote(int32_t newValue, int32_t expressionDimension,
	                                              int32_t channelOrNoteNumber,
	                                              MIDICharacteristic whichCharacteristic) override;

	/// Get MIDI CC parameter for automation (similar to MIDIInstrument)
	ModelStackWithAutoParam* getParamToControlFromInputMIDIChannel(int32_t cc,
	                                                               ModelStackWithThreeMainThings* modelStack);

	/// Get param from mod encoder (for gold knob automation)
	ModelStackWithAutoParam* getParamFromModEncoder(int32_t whichModEncoder, ModelStackWithThreeMainThings* modelStack,
	                                                bool allowCreation = true);

	/// Check if automation exists on a MIDI CC parameter
	bool doesAutomationExistOnMIDIParam(ModelStackWithThreeMainThings* modelStack, int32_t cc);

	// CC management (like MIDIInstrument)
	int32_t changeControlNumberForModKnob(int32_t offset, int32_t whichModEncoder, int32_t modKnobMode);
	int32_t getKnobPosForNonExistentParam(int32_t whichModEncoder, ModelStackWithAutoParam* modelStack);

	// Mod knob mode support
	uint8_t* getModKnobMode() { return &modKnobMode; }

	// Device definition file support (for custom CC labels)
	/// Read device definition file
	Error readDeviceDefinitionFile(Deserializer& reader, bool readFromPresetOrSong);
	void readDeviceDefinitionFileNameFromPresetOrSong(Deserializer& reader);
	Error readCCLabelsFromFile(Deserializer& reader);
	/// Write device definition file
	void writeDeviceDefinitionFile(Serializer& writer, bool writeFileNameToPresetOrSong);
	void writeDeviceDefinitionFileNameToPresetOrSong(Serializer& writer);
	void writeCCLabelsToFile(Serializer& writer);
	/// Get/set CC labels
	std::string_view getNameFromCC(int32_t cc);
	void setNameForCC(int32_t cc, std::string_view name);
	bool hasCCLabels() const;
	void copyLabelsFrom(MIDIDrum const* other);
	/// After kit load/save round-trip, row 0 may hold modKnobs/definition that rows 1+ omitted in XML.
	static void propagateSharedSettingsAcrossKit(class Kit* kit);

	uint8_t note;
	int8_t noteEncoderCurrentOffset;

	/// Default velocity for this drum (used when not auditioning)
	uint8_t defaultVelocity{64};

	/// Current mod knob mode (0 = upper, 1 = lower)
	uint8_t modKnobMode{0};

	/// Gold knob CC assignments (like MIDIInstrument)
	/// Stores which CC number is controlled by each gold knob (per mod mode)
	std::array<int8_t, kNumModButtons * kNumPhysicalModKnobs> modKnobCCAssignments;

	/// Device definition file name (for custom CC labels)
	String deviceDefinitionFileName;
	bool loadDeviceDefinitionFile{false};

	/// MIDI output device selection for this drum
	/// - 0: ALL devices (send to all connected MIDI outputs - default behavior)
	/// - 1: DIN MIDI port only
	/// - 2+: Specific USB MIDI device (2 = first USB device, 3 = second USB device, etc.)
	uint8_t outputDevice{0};

	/// Store the device name for reliable matching when devices are reconnected
	/// This ensures the correct device is selected even if USB devices are plugged in a different order
	String outputDeviceName;

private:
	MIDIDrum* findKitMidiDrumWithLabels() const;
	MIDIDrum* findKitMidiDrumWithModKnobs() const;
	bool hasModKnobAssignments() const;
	bool modKnobAssignmentsMatch(MIDIDrum const* other) const;
	/// Copy definition file path from an earlier kit row if this row only has shared labels.
	void ensureDeviceDefinitionFileNameFromKit();
	/// Same definition file path, or both inline-only (no definition file set).
	bool sharesMidiDeviceDefinitionWith(const MIDIDrum* other) const;

	/// Custom CC label names loaded from device definition file
	deluge::fast_map<uint8_t, std::string> labels;
};
