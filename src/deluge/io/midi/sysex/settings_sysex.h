#pragma once

#include "definitions_cxx.hpp"

class MIDICable;
class JsonDeserializer;

namespace SettingsSysex {

// Grouped getters - organized by Deluge menu structure
void getSettingsCV(MIDICable& cable, JsonDeserializer& reader);
void getSettingsGate(MIDICable& cable, JsonDeserializer& reader);
void getSettingsClock(MIDICable& cable, JsonDeserializer& reader);
void getSettingsMidi(MIDICable& cable, JsonDeserializer& reader);
void getSettingsDefaults(MIDICable& cable, JsonDeserializer& reader);
void getSettingsPads(MIDICable& cable, JsonDeserializer& reader);
void getSettingsRecording(MIDICable& cable, JsonDeserializer& reader);
void getSettingsCommunity(MIDICable& cable, JsonDeserializer& reader);
void getSettingsUI(MIDICable& cable, JsonDeserializer& reader);
void getAllSettings(MIDICable& cable, JsonDeserializer& reader);

// Individual getters/setters
void setSetting(MIDICable& cable, JsonDeserializer& reader);
void getSetting(MIDICable& cable, JsonDeserializer& reader);

// Firmware info
void getFirmwareVersion(MIDICable& cable, JsonDeserializer& reader);

} // namespace SettingsSysex
