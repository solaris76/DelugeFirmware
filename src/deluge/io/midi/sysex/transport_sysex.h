#pragma once

#include "definitions_cxx.hpp"

class MIDICable;
class JsonDeserializer;

namespace TransportSysex {

void transportControl(MIDICable& cable, JsonDeserializer& reader);
void setTempo(MIDICable& cable, JsonDeserializer& reader);
void getTransportState(MIDICable& cable, JsonDeserializer& reader);
void setSwing(MIDICable& cable, JsonDeserializer& reader);
void setMetronome(MIDICable& cable, JsonDeserializer& reader);
void subscribeTransport(MIDICable& cable, JsonDeserializer& reader);
void unsubscribeTransport(MIDICable& cable, JsonDeserializer& reader);
void setSongScale(MIDICable& cable, JsonDeserializer& reader);
void getSongScale(MIDICable& cable, JsonDeserializer& reader);
void notifyTempoChanged(int32_t bpm);
void notifyTransportChanged();

} // namespace TransportSysex
