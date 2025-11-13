# Parameter Control SysEx API

Comprehensive parameter access for Deluge synth sounds via SysEx.

## Status

✅ **Implemented:**
- `getParameters` - Retrieve all parameters from currently selected synth sound
- SysEx command dispatching 
- JSON serialization for all parameter types

🚧 **In Progress:**
- `setParameter` - Modify individual parameters
- `getPatchCables` - Read modulation routing
- `setPatchCable` - Set modulation routing
- `subscribeParameters` / `unsubscribeParameters` - Real-time parameter change notifications

## Commands

### Get All Parameters

```javascript
{getParameters: {}}
```

**Response:**
```javascript
{
  "^parameters": {
    "general": {
      "polyphonic": "poly|mono|legato|choke|auto",
      "voicePriority": 0-2,  // 0=LOW, 1=MEDIUM, 2=HIGH
      "mode": "subtractive|fm|ringmod",
      "transpose": -24 to +24,
      "maxVoices": 1-8
    },
    "unison": {
      "num": 1-8,
      "detune": 0-50,
      "spread": 0-127
    },
    "osc1": {
      "type": "square|saw|sine|triangle|sample|wavetable|inputL|inputR|inputStereo",
      "transpose": -48 to +48,
      "cents": -50 to +50,
      "retrigPhase": -1 (off) or 0-4294967295
    },
    "osc2": {
      ... // Same as osc1
    },
    "osc2Sync": 0|1,
    "patched": {
      // ALL PATCHED PARAMETERS (60+ params)
      // Local params (per-voice):
      "oscAVolume": int32,
      "oscBVolume": int32,
      "volume": int32,
      "noiseVolume": int32,
      "modulator0Volume": int32,
      "modulator1Volume": int32,
      "fold": int32,
      "modulator0Feedback": int32,
      "modulator1Feedback": int32,
      "carrier0Feedback": int32,
      "carrier1Feedback": int32,
      "lpfResonance": int32,
      "hpfResonance": int32,
      "env0Sustain": int32,
      "env1Sustain": int32,
      "env2Sustain": int32,
      "env3Sustain": int32,
      "lpfMorph": int32,
      "hpfMorph": int32,
      "oscAPhaseWidth": int32,
      "oscBPhaseWidth": int32,
      "oscAWaveIndex": int32,
      "oscBWaveIndex": int32,
      "pan": int32,
      "lpfFrequency": int32,
      "pitchAdjust": int32,
      "oscAPitchAdjust": int32,
      "oscBPitchAdjust": int32,
      "modulator0PitchAdjust": int32,
      "modulator1PitchAdjust": int32,
      "hpfFrequency": int32,
      "lfo1LocalFrequency": int32,
      "lfo2LocalFrequency": int32,
      "env0Attack": int32,
      "env1Attack": int32,
      "env2Attack": int32,
      "env3Attack": int32,
      "env0Decay": int32,
      "env1Decay": int32,
      "env2Decay": int32,
      "env3Decay": int32,
      "env0Release": int32,
      "env1Release": int32,
      "env2Release": int32,
      "env3Release": int32,
      
      // Global params (whole sound):
      "volumePostFX": int32,
      "volumePostReverbSend": int32,
      "reverbAmount": int32,
      "modFXDepth": int32,
      "delayFeedback": int32,
      "delayRate": int32,
      "modFXRate": int32,
      "lfoFrequency1": int32,
      "lfoFrequency2": int32,
      "arpRate": int32
    },
    "unpatched": {
      // ALL UNPATCHED PARAMETERS
      "portamento": int32,
      "stutterRate": int32,
      "bass": int32,
      "treble": int32,
      "bassFrequency": int32,
      "trebleFrequency": int32,
      "sampleRateReduction": int32,
      "bitcrushing": int32,
      "modFXOffset": int32,
      "modFXFeedback": int32,
      "sidechainShape": int32,
      "compressorThreshold": int32,
      "arpGate": int32,
      "arpRhythm": int32,
      "arpSequenceLength": int32,
      "arpChordPolyphony": int32,
      "arpRatchetAmount": int32,
      "noteProbability": int32,
      "reverseProbability": int32,
      "arpBassProbability": int32,
      "arpSwapProbability": int32,
      "arpGlideProbability": int32,
      "arpChordProbability": int32,
      "arpRatchetProbability": int32,
      "arpSpreadGate": int32,
      "arpSpreadOctave": int32,
      "spreadVelocity": int32
    },
    "filters": {
      "lpfMode": "24dB|12dB|HPLadder|SVFBand|SVFNotch",
      "hpfMode": "24dB|12dB|HPLadder|SVFBand|SVFNotch"
    },
    "lfos": {
      "lfo1": {
        "syncLevel": 0-15,
        "syncType": 0-7
      },
      "lfo2": { ... },
      "lfo3": { ... },
      "lfo4": { ... }
    },
    "modFX": {
      "type": "none|flanger|chorus|phaser|chorusStereo|warble|grain"
    },
    "sidechainSend": int32
  }
}
```

## Parameter Value Ranges

### Patched Parameters
All patched parameters use **32-bit signed integer** values:
- Range: `-2147483648` to `2147483647`
- Neutral value varies by parameter type:
  - Volume params: `0x7FFFFFFF` (max positive)
  - Pitch params: centered at `0x00000000`
  - Pan: centered at `0x00000000` (-50% to +50%)
  - Envelopes: exponential scale

### Oscillator Types
- `square`, `saw`, `sine`, `triangle` - Basic waveforms
- `sample` - Sample playback (requires file loaded)
- `wavetable` - Wavetable synthesis (requires file loaded)
- `inputL`, `inputR`, `inputStereo` - Audio input

### Filter Modes
- `24dB` - Transistor ladder 24dB/octave (default)
- `12dB` - Transistor ladder 12dB/octave
- `HPLadder` - High-pass ladder filter
- `SVFBand` - State variable band-pass
- `SVFNotch` - State variable notch

### ModFX Types
- `none` - No modulation FX
- `flanger` - Flanger effect
- `chorus` - Chorus effect
- `phaser` - Phaser effect
- `chorusStereo` - Stereo chorus
- `warble` - Warble effect (tape-like)
- `grain` - Granular synthesis

## Implementation Details

### Requirements
- **Active synth clip**: Commands only work when a synth (not kit/MIDI/CV) clip is selected
- **Port 3**: Use Deluge USB Port 3 for SysEx communication

### File Locations
- `src/deluge/io/midi/sysex/parameter_sysex.h` - API declarations
- `src/deluge/io/midi/sysex/parameter_sysex.cpp` - Implementation
- `src/deluge/storage/smsysex.cpp` - Command dispatcher integration

### Error Handling
If no synth sound is selected or the operation fails:
```javascript
{"^parameters": {"error": "No synth sound selected"}}
```

## Future Enhancements

1. **setParameter** - Modify individual parameters with validation
2. **getPatchCables** - Read modulation matrix (patch cables)
3. **setPatchCable** - Create/modify modulation routing
4. **Parameter subscriptions** - Real-time change notifications
5. **Batch parameter updates** - Set multiple parameters atomically
6. **Parameter presets** - Save/load complete parameter sets

## Usage Example

```javascript
// Connect to Deluge Port 3
const midiAccess = await navigator.requestMIDIAccess({sysex: true});
const delugeOut = Array.from(midiAccess.outputs.values()).find(o => o.name.includes('Deluge') && o.name.includes('3'));
const delugeIn = Array.from(midiAccess.inputs.values()).find(i => i.name.includes('Deluge') && i.name.includes('3'));

// Request all parameters
const cmd = {getParameters: {}};
const json = JSON.stringify(cmd);
const header = [0xF0, 0x00, 0x21, 0x7B, 0x01, 0x04, 0x01]; // SysEx header + msgID
const msg = [...header, ...Array.from(json).map(c => c.charCodeAt(0)), 0xF7];

delugeIn.onmidimessage = (e) => {
  const data = Array.from(e.data);
  if (data[0] === 0xF0 && data[5] === 0x05) { // JSON response
    const jsonBytes = data.slice(7, data.indexOf(0xF7));
    const response = JSON.parse(String.fromCharCode(...jsonBytes));
    console.log('Parameters:', response);
  }
};

delugeOut.send(msg);
```

## Notes

- All parameter values returned are the **final computed values** after modulation
- To get **neutral/base** values without modulation, would need a separate API call (not yet implemented)
- Parameter names use the same strings as in Deluge XML preset files
- Values can be modified in-place for preset creation/editing

---

**Status:** Alpha - Core retrieval working, write operations in development  
**Firmware Branch:** `sysex`  
**Last Updated:** 2025-11-13

