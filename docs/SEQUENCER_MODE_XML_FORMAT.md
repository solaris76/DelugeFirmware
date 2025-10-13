# Sequencer Mode XML Format Specification

Based on analysis of real Deluge song files and the existing note storage format.

## Key Observations from Real Deluge Files

### 1. Hex Data Storage
Notes are stored as continuous hex strings prefixed with `0x`:
```xml
<noteRow
    y="36"
    noteDataWithLift="0x00000000000000186440140000006000000018644014000000C0000000186440140000012000000018644014" />
```

### 2. Attribute Format
- Attributes use tabs for indentation
- Hex values for parameters: `reverbAmount="0x80000000"`
- Simple values without quotes in some contexts

### 3. Firmware Versioning
```xml
<song
    firmwareVersion="c1.3.0"
    earliestCompatibleFirmware="c1.3.0">
```

---

## Existing Deluge Pattern Format (For Reference)

**Current Piano Roll Pattern:**
```xml
<pattern>
	<attributes
		patternVersion="0.0.1"
		screenWidth="384"
		scaleType="0"
		yNoteOfBottomRow="60" />
	<noteRows>
		<noteRow
			numNotes="5"
			yNote="60"
			yDisplay="0"
			noteDataWithSplitProb="0x000000000000001840401400000000000048..." />
		<noteRow
			numNotes="2"
			yNote="62"
			yDisplay="1"
			noteDataWithSplitProb="0x00000060000000184040140000000000012..." />
	</noteRows>
</pattern>
```

**Key characteristics:**
- Root tag: `<pattern>`
- Metadata in `<attributes>` tag
- Data stored as hex strings in `noteDataWithSplitProb`
- No XML declaration
- Tab-indented

**Our sequencer patterns follow the same simple structure!**

---

## Step Sequencer Pattern Format

### Pattern File (PATTERNS/SEQUENCER/STEP/PATTERN001.XML)

**Following the same simple structure as existing Deluge patterns:**

```xml
<pattern>
	<attributes
		patternVersion="0.0.1"
		sequencerMode="STEP"
		scaleType="0"
		numScaleNotes="7" />
	<stepSequencer
		numSteps="16"
		direction="0"
		pingPongDirection="1"
		pedalMaxReach="8"
		pedalGoingOutInt="1"
		growPhase="0"
		growMax="8"
		stepData="0x00000001010001020001040001050001070001080001000000010100010201000401000101" />
	<controlColumns
		padData="0x001003000101111101010017000601010604" />
	<scenes>
		<scene index="0" data="0x..." />
		<scene index="1" data="0x..." />
	</scenes>
</pattern>
```

**Key Points:**
- Root tag is `<pattern>` (matches existing Deluge patterns)
- Simple `<attributes>` section with metadata
- All data as hex strings (like `noteDataWithSplitProb`)
- No XML version declaration or firmware tags (patterns are simpler than songs)

### Step Data Encoding Breakdown
```
Step 0: noteIndex=0,  octave=0,  gate=ON     → 0x000001
Step 1: noteIndex=1,  octave=0,  gate=ON     → 0x010001
Step 2: noteIndex=2,  octave=0,  gate=ON     → 0x020001
Step 3: noteIndex=4,  octave=0,  gate=ON     → 0x040001
...
```

### Control Column Pad Encoding
```
Pad y=0, x=16, type=OCTAVE(3), valueIndex=0, mode=TOGGLE(0), active=1
→ 0x001003000001

Pad y=1, x=17, type=SCENE(8), valueIndex=1, mode=TOGGLE(0), active=0  
→ 0x011108010000
```

---

## Pulse Sequencer Pattern Format

### Pattern File (PATTERNS/SEQUENCER/PULSE/PATTERN001.XML)

**Following the same simple structure:**

```xml
<pattern>
	<attributes
		patternVersion="0.0.1"
		sequencerMode="PULSE"
		scaleType="0"
		numScaleNotes="7" />
	<pulseSequencer
		numPulses="8"
		period="192"
		pulseData="0x0000000000000018003C01000000300000000C003E010..."
		orderData="0x0001020304050607" />
	<controlColumns
		padData="0x..." />
	<scenes>
		<scene index="0" data="0x..." />
	</scenes>
</pattern>
```

**Key Points:**
- Same `<pattern>` root tag
- Pulse data and order data as hex attributes
- All hex encoding for efficiency

### Pulse Data Encoding Breakdown
```
Pulse 0: onset=0x00000000, duration=0x00000018, accent=0x00, note=0x3C, gate=0x01
→ 0x0000000000000018003C01

Pulse 1: onset=0x00000030, duration=0x0000000C, accent=0x00, note=0x3E, gate=0x01
→ 0x000000300000000C003E01
```

---

## Song File Integration

### In InstrumentClip (SONGS/SONGXXX.XML)

```xml
<instrumentClip
	inKeyMode="1"
	yScroll="37"
	keyboardLayout="1"
	yScrollKeyboard="50"
	instrumentPresetName="MySynth"
	instrumentPresetFolder="SYNTHS"
	isPlaying="1"
	isSoloing="0"
	isArmedForRecording="1"
	length="384"
	colourOffset="30"
	section="0">
	
	<!-- Standard clip parameters -->
	<soundParams
		arpeggiatorGate="0x00000000"
		portamento="0x80000000"
		volume="0x62000000"
		pan="0x00000000">
	</soundParams>
	
	<!-- Existing piano roll data (NoteRows) -->
	<noteRows>
		<noteRow
			y="57"
			noteDataWithLift="0x000000A80000002B644014..." />
	</noteRows>
	
	<!-- NEW: Sequencer mode states -->
	<sequencerModes>
		<!-- Step Sequencer state -->
		<stepSequencer
			numSteps="16"
			currentStep="0"
			direction="0"
			pingPongDirection="1"
			pedalMaxReach="8"
			pedalGoingOutInt="1"
			growPhase="0"
			growMax="8"
			stepData="0x..." />
		
		<!-- Pulse Sequencer state -->
		<pulseSequencer
			numPulses="8"
			period="192"
			currentPulse="0"
			currentPhase="0"
			pulseData="0x...">
			<playOrder orderData="0x0001020304050607" />
		</pulseSequencer>
		
		<!-- Control columns (shared across modes) -->
		<controlColumns padData="0x..." />
		
		<!-- Scenes (shared across modes) -->
		<scenes>
			<scene index="0" data="0x..." />
			<scene index="1" data="0x..." />
		</scenes>
	</sequencerModes>
</instrumentClip>
```

---

## Data Packing Details

### Step Sequencer Step Structure (3 bytes per step)
```c
struct StepData {
    uint8_t noteIndex;  // 0-31 (index into scale notes array)
    int8_t  octave;     // -3 to +3 (stored as signed)
    uint8_t gate;       // 0=OFF, 1=ON, 2=SKIP
};
```

### Pulse Sequencer Pulse Structure (12 bytes per pulse)
```c
struct PulseData {
    int32_t onset;      // Timing offset (0 to period-1)
    int32_t duration;   // Note length
    uint8_t accent;     // 0=off, 1=on
    uint8_t note;       // MIDI note (0-127)
    uint8_t gate;       // 0=OFF, 1=SINGLE, 2=MULTIPLE, 3=HELD
    uint8_t reserved;   // Padding for alignment
};
```

### Control Column Pad Structure (6 bytes per pad)
```c
struct ControlPadData {
    uint8_t  y;           // 0-7
    uint8_t  x;           // 16-17
    uint8_t  type;        // ControlType enum value
    uint8_t  valueIndex;  // Index into value array
    uint8_t  mode;        // 0=TOGGLE, 1=MOMENTARY
    uint8_t  active;      // 0=inactive, 1=active
};
```

---

## Implementation Guidelines

### Writing Hex Data
```cpp
void writeStepDataToFile(Serializer& writer, const std::array<Step, 16>& steps) {
    writer.write("\n\t\tstepData=\"0x");
    
    for (int i = 0; i < 16; i++) {
        char buffer[7];  // 6 hex chars + null
        
        // Write noteIndex (1 byte)
        intToHex(steps[i].noteIndex, buffer, 2);
        writer.write(buffer);
        
        // Write octave (1 byte, as unsigned for storage)
        intToHex(static_cast<uint8_t>(steps[i].octave + 3), buffer, 2);
        writer.write(buffer);
        
        // Write gate (1 byte)
        intToHex(static_cast<uint8_t>(steps[i].gateType), buffer, 2);
        writer.write(buffer);
    }
    
    writer.write("\"");
}
```

### Reading Hex Data
```cpp
Error readStepDataFromFile(Deserializer& reader, std::array<Step, 16>& steps) {
    char const* hexData;
    reader.readTagOrAttributeValue("stepData", &hexData);
    
    if (hexData[0] == '0' && hexData[1] == 'x') {
        hexData += 2;  // Skip "0x" prefix
    }
    
    for (int i = 0; i < 16; i++) {
        // Read 6 hex characters per step
        steps[i].noteIndex = hexToInt(&hexData[i * 6 + 0], 2);
        steps[i].octave = static_cast<int32_t>(hexToInt(&hexData[i * 6 + 2], 2)) - 3;
        steps[i].gateType = static_cast<GateType>(hexToInt(&hexData[i * 6 + 4], 2));
    }
    
    return Error::NONE;
}
```

---

## Advantages of This Format

1. **Compact**: All step data in one attribute, like existing Deluge note storage
2. **Fast**: Single hex string read/write, no XML traversal for each step
3. **Consistent**: Matches existing Deluge patterns (`noteDataWithLift`)
4. **Efficient**: Binary data, not text representation
5. **Extensible**: Can add new attributes without breaking existing readers

---

## File Size Estimates

**Step Sequencer Pattern:**
- Step data: 48 bytes → 96 hex chars
- Control columns: ~16 pads × 6 bytes = 96 bytes → 192 hex chars  
- 8 scenes × 512 bytes max = 4KB → 8192 hex chars
- **Total**: ~8.5KB typical

**Song with Both Modes:**
- Step data: 96 hex chars
- Pulse data: 192 hex chars
- Control columns: 192 hex chars
- Scenes: 8192 hex chars
- **Per clip**: ~8.5KB additional to existing data

This is comparable to existing NoteRow storage and acceptable for SD card performance.


