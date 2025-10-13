# Sequencer Mode Persistence Proposal

## Current Issue
When saving patterns (SAVE + horizontal encoder), the Deluge currently saves the **piano roll data** (NoteRows) regardless of which sequencer mode is active. This means:
- Step Sequencer pattern data is lost
- Pulse Sequencer pattern data is lost
- Control column configurations are lost
- Only the underlying NoteRows are preserved

## Goals
1. **Pattern saving**: Save ONLY the currently active view (Piano Roll, Step Seq, or Pulse Seq)
2. **Song saving**: Save ALL sequencer states plus piano roll data
3. **Think of patterns like scenes**: Each pattern is a snapshot of ONE mode, but may contain multiple scenes (up to 8)

---

## Proposed Solution

### UI Flow Summary

#### Pattern Saving (SAVE + Horizontal Encoder Press)
```
Active View              →  Saves To                           →  File Contains
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Piano Roll               →  PATTERNS/MELODIC/                  →  NoteRows only
Step Sequencer          →  PATTERNS/SEQUENCER/STEP/           →  Step pattern + control columns
Pulse Sequencer         →  PATTERNS/SEQUENCER/PULSE/          →  Pulse pattern + control columns
```

**Key Point**: Only ONE mode's data is saved - whatever view you're currently in.

#### Song Saving (Standard SAVE workflow)
```
What Gets Saved
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✓ Piano Roll data (NoteRows)
✓ Step Sequencer state (if used)
✓ Pulse Sequencer state (if used)
✓ Control column configurations (all modes)
✓ All scene data
✓ Standard clip/instrument settings
```

**Key Point**: EVERYTHING is saved - all modes, all states.

---

### 1. Pattern File Structure

#### Current Pattern File Location
```
SD CARD ROOT/
  PATTERNS/
    MELODIC/           (Synth, MIDI, CV patterns)
    RHYTHMIC/
      KIT/             (Full kit patterns)
      DRUM/            (Single drum patterns)
```

#### Proposed: Add Sequencer Mode Patterns
```
SD CARD ROOT/
  PATTERNS/
    MELODIC/
      SEQUENCER/       (New folder for sequencer mode patterns)
        STEP/
        PULSE/
    RHYTHMIC/
      SEQUENCER/       (New folder for kit sequencer patterns)
        STEP/
        PULSE/
```

#### Current Pattern File Structure (Piano Roll)
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pattern>
  <attributes 
    patternVersion="1"
    screenWidth="6144"
    scaleType="0"
    yNoteOfBottomRow="60">
  </attributes>
  <noteRows>
    <noteRow 
      numNotes="2"
      yNote="64"
      yDisplay="2"
      noteDataWithSplitProb="0x000000000C0040...">
    </noteRow>
  </noteRows>
</pattern>
```

#### Proposed: Sequencer Mode Pattern File Structure

**Step Sequencer Pattern:**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<sequencerPattern>
  <firmwareVersion>1.3.0-alpha+sequencerModes</firmwareVersion>
  <earliestCompatibleFirmware>1.3.0-alpha</earliestCompatibleFirmware>
  
  <attributes
    sequencerMode="STEP"
    scaleType="0"
    numScaleNotes="7">
  </attributes>
  
  <!-- Step Sequencer-specific data -->
  <stepSequencer 
    numSteps="16"
    currentStep="0">
    
    <steps>
      <!-- Each step: noteIndex, octave, gate -->
      <step index="0" noteIndex="0" octave="0" gate="ON" />
      <step index="1" noteIndex="2" octave="0" gate="ON" />
      <step index="2" noteIndex="4" octave="0" gate="OFF" />
      <!-- ... 13 more steps -->
    </steps>
    
    <!-- Playback direction state (for complex modes like Pedal, Grow) -->
    <playbackState
      direction="FORWARD"
      pingPongDirection="1"
      pedalMaxReach="8"
      pedalGoingOut="1"
      growPhase="0"
      growMax="8">
    </playbackState>
  </stepSequencer>
  
  <!-- Control column configuration -->
  <controlColumns>
    <pad y="0" x="16" type="OCTAVE" valueIndex="3" mode="TOGGLE" active="1" />
    <pad y="1" x="16" type="TRANSPOSE" valueIndex="17" mode="TOGGLE" active="0" />
    <pad y="0" x="17" type="SCENE" valueIndex="0" mode="TOGGLE" active="0" sceneValid="1" />
    <pad y="1" x="17" type="SCENE" valueIndex="1" mode="TOGGLE" active="0" sceneValid="0" />
    <pad y="7" x="17" type="RESET" valueIndex="0" mode="MOMENTARY" active="0" />
    <pad y="6" x="17" type="RANDOM" valueIndex="4" mode="MOMENTARY" active="0" />
    <!-- ... more pads -->
  </controlColumns>
  
  <!-- Scene data (up to 8 scenes) -->
  <scenes>
    <scene index="0" size="128">
      <!-- Binary data encoded as hex -->
      0xABCDEF123456...
    </scene>
  </scenes>
</sequencerPattern>
```

**Pulse Sequencer Pattern:**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<sequencerPattern>
  <firmwareVersion>1.3.0-alpha+sequencerModes</firmwareVersion>
  <earliestCompatibleFirmware>1.3.0-alpha</earliestCompatibleFirmware>
  
  <attributes
    sequencerMode="PULSE"
    scaleType="0"
    numScaleNotes="7">
  </attributes>
  
  <!-- Pulse Sequencer-specific data -->
  <pulseSequencer 
    numPulses="8"
    period="192"
    currentPulse="0"
    currentPhase="0">
    
    <pulses>
      <!-- Each pulse: onset, duration, accent, note, gate type -->
      <pulse index="0" onset="0" duration="12" accent="0" note="60" gate="SINGLE" />
      <pulse index="1" onset="24" duration="8" accent="1" note="64" gate="HELD" />
      <!-- ... 6 more pulses -->
    </pulses>
    
    <playOrder>
      <!-- Current play order indices -->
      <order>0,1,2,3,4,5,6,7</order>
    </playOrder>
  </pulseSequencer>
  
  <!-- Control column configuration (same as above) -->
  <controlColumns>
    <!-- ... -->
  </controlColumns>
  
  <!-- Scene data -->
  <scenes>
    <!-- ... -->
  </scenes>
</sequencerPattern>
```

---

### 2. Song File Integration

#### Current Song File Structure (Simplified)
```xml
<song>
  <instruments>
    <sound>...</sound>
    <kit>...</kit>
  </instruments>
  
  <sessionClips>
    <instrumentClip>
      <clipName>My Clip</clipName>
      <inKeyMode>1</inKeyMode>
      <!-- ... clip attributes ... -->
      
      <noteRows>
        <noteRow>...</noteRow>
      </noteRows>
    </instrumentClip>
  </sessionClips>
</song>
```

#### Proposed: Add Sequencer Mode State to Clips

```xml
<song>
  <instruments>
    <!-- ... -->
  </instruments>
  
  <sessionClips>
    <instrumentClip>
      <clipName>My Step Seq</clipName>
      <inKeyMode>1</inKeyMode>
      <!-- ... existing clip attributes ... -->
      
      <!-- NEW: Sequencer mode state -->
      <sequencerMode 
        type="STEP"
        active="1">
        
        <stepSequencer numSteps="16">
          <steps>
            <step index="0" noteIndex="0" octave="0" gate="ON" />
            <!-- ... -->
          </steps>
          <playbackState
            direction="FORWARD"
            pingPongDirection="1"
            pedalMaxReach="8"
            pedalGoingOut="1"
            growPhase="0"
            growMax="8">
          </playbackState>
        </stepSequencer>
        
        <controlColumns>
          <pad y="0" x="16" type="OCTAVE" valueIndex="3" mode="TOGGLE" active="1" />
          <!-- ... -->
        </controlColumns>
        
        <scenes>
          <scene index="0" size="128">0xABCDEF...</scene>
        </scenes>
      </sequencerMode>
      
      <!-- Existing NoteRows (for backward compatibility) -->
      <noteRows>
        <noteRow>...</noteRow>
      </noteRows>
    </instrumentClip>
  </sessionClips>
</song>
```

#### Multiple Sequencer States in One Clip
- A clip can have **both** Step and Pulse sequencer states saved
- Plus the underlying NoteRows (piano roll)
- User can switch between any view and see their patterns preserved
- Like having multiple "views" of the same musical material

---

## Implementation Plan

### Phase 1: Add Serialization Methods to Sequencer Modes

**File: `sequencer_mode.h`**
```cpp
class SequencerMode {
public:
    // ... existing methods ...
    
    // ========== PERSISTENCE ==========
    
    // Write sequencer mode state to XML
    virtual void writeToFile(Serializer& writer, bool includeScenes = true);
    
    // Read sequencer mode state from XML
    virtual Error readFromFile(Deserializer& reader);
    
    // Check if this mode can be saved as a pattern
    virtual bool canSaveAsPattern() { return true; }
};
```

**Files: `step_sequencer_mode.cpp`, `pulse_sequencer_mode.cpp`**
- Implement `writeToFile()` for each mode
- Implement `readFromFile()` for each mode
- Save/restore all mode-specific state

### Phase 2: Extend Control Column Serialization

**File: `sequencer_control_state.h`**
```cpp
class SequencerControlState {
public:
    // ... existing methods ...
    
    // ========== PERSISTENCE ==========
    
    void writeToFile(Serializer& writer, bool includeScenes = true);
    Error readFromFile(Deserializer& reader);
};
```

### Phase 3: Integrate into InstrumentClip Serialization

**File: `instrument_clip.cpp`**

Modify `InstrumentClip::writeDataToFile()`:
```cpp
void InstrumentClip::writeDataToFile(Serializer& writer, Song* song) {
    // ... existing attributes ...
    
    writer.writeOpeningTagEnd();
    
    // ... existing tags (midiCommands, params, etc.) ...
    
    // NEW: Write sequencer mode state
    SequencerMode* mode = getActiveSequencerMode();
    if (mode && mode->canSaveAsPattern()) {
        writer.writeOpeningTag("sequencerMode");
        writer.writeAttribute("type", mode->getName());
        writer.writeAttribute("active", 1);
        mode->writeToFile(writer, true); // Include scenes
        writer.writeClosingTag("sequencerMode");
    }
    
    // Existing NoteRows (for backward compatibility)
    if (noteRows.getNumElements()) {
        // ... existing noteRows writing ...
    }
}
```

Modify `InstrumentClip::readFromFile()`:
```cpp
Error InstrumentClip::readFromFile(Deserializer& reader, Song* song) {
    // ... existing reading logic ...
    
    // NEW: Check for sequencer mode data
    if (!strcmp(tagName, "sequencerMode")) {
        char const* modeType;
        reader.readTagOrAttributeValue("type", &modeType);
        
        SequencerMode* mode = SequencerModeManager::createMode(modeType);
        if (mode) {
            Error error = mode->readFromFile(reader);
            if (error != Error::NONE) {
                delete mode;
                return error;
            }
            setActiveSequencerMode(mode);
        }
        reader.exitTag("sequencerMode");
    }
    
    // ... continue with existing reading ...
}
```

### Phase 4: Modify Pattern Save Logic

**Modify: `instrument_clip_view.cpp` (SAVE + encoder handler)**
```cpp
// Check which view is currently active
if (isInStepSequencerView()) {
    // Save Step Sequencer pattern only
    openUI(&saveSequencerPatternUI);
    saveSequencerPatternUI.setMode(SequencerModeType::STEP);
    saveSequencerPatternUI.setFolder("PATTERNS/SEQUENCER/STEP/");
}
else if (isInPulseSequencerView()) {
    // Save Pulse Sequencer pattern only
    openUI(&saveSequencerPatternUI);
    saveSequencerPatternUI.setMode(SequencerModeType::PULSE);
    saveSequencerPatternUI.setFolder("PATTERNS/SEQUENCER/PULSE/");
}
else {
    // Piano Roll view - save NoteRows (existing behavior)
    openUI(&savePatternUI);
}
```

**New File: `save_sequencer_pattern_ui.h/cpp`**
- Inherits from `SaveUI`
- Saves ONLY the currently active sequencer mode
- Routes to appropriate folder based on mode type

### Phase 5: Create Sequencer Pattern Load UI

**New File: `load_sequencer_pattern_ui.h/cpp`**
- Inherits from `LoadUI`
- Browse sequencer pattern folders
- Load into active sequencer mode

---

## File Locations Summary

### On SD Card

**Pattern Files:**
```
PATTERNS/
  MELODIC/
    SEQUENCER/
      STEP/
        PATTERN001.XML
        PATTERN002.XML
      PULSE/
        PATTERN001.XML
  RHYTHMIC/
    SEQUENCER/
      STEP/
        PATTERN001.XML
```

**Song Files (unchanged location):**
```
SONGS/
  SONG001.XML
```

### In Codebase

**New Files:**
```
src/deluge/
  model/clip/sequencer/
    persistence/
      sequencer_pattern_serializer.h
      sequencer_pattern_serializer.cpp
  gui/ui/save/
    save_sequencer_pattern_ui.h
    save_sequencer_pattern_ui.cpp
  gui/ui/load/
    load_sequencer_pattern_ui.h
    load_sequencer_pattern_ui.cpp
```

**Modified Files:**
```
src/deluge/
  model/clip/sequencer/
    sequencer_mode.h                    (add writeToFile/readFromFile)
    sequencer_mode.cpp
    modes/
      step_sequencer_mode.h/cpp        (implement persistence)
      pulse_sequencer_mode.h/cpp       (implement persistence)
    control_columns/
      sequencer_control_state.h/cpp    (add writeToFile/readFromFile)
  model/clip/
    instrument_clip.h/cpp              (integrate sequencer mode serialization)
  gui/views/
    instrument_clip_view.cpp           (route to correct save UI)
```

---

## Benefits of This Approach

1. **Mode-Specific Patterns**: Each sequencer mode can have its own pattern library
2. **Complete Song State**: Songs preserve ALL sequencer states + piano roll data
3. **View Switching**: Switch between Step/Pulse/Piano Roll without losing work
4. **Organized**: Clear folder structure separates different pattern types
5. **Extensible**: Easy to add new sequencer modes in the future
6. **Scene Support**: Scenes are part of the pattern/song data
7. **Control Column Persistence**: Your pad configurations are saved with patterns

---

## Design Decisions (APPROVED)

1. **Save underlying NoteRows when saving sequencer patterns?**
   - ✅ **YES** - Save both sequencer mode data AND NoteRows
   - Allows fallback to piano roll view
   - User can switch between sequencer mode and piano roll seamlessly

2. **"Bake" sequencer pattern to NoteRows?**
   - ❌ **NOT FOR NOW** - Skip "Export to Piano Roll" function
   - Can add later if needed

3. **Scene data encoding in XML**
   - ✅ **HEX STRING** - Compact and efficient
   - Format: `0xABCDEF123456...`

4. **Backward compatibility**
   - ❌ **NOT REQUIRED** - Forward compatibility only
   - Simplifies implementation significantly
   - New firmware can use new format exclusively

---

## Implementation Summary

### What Happens When You Save a Pattern
```
User Action: SAVE + Press Horizontal Encoder
→ Check current view
→ If Piano Roll: Save NoteRows to PATTERNS/MELODIC/
→ If Step Seq:   Save Step pattern to PATTERNS/SEQUENCER/STEP/
→ If Pulse Seq:  Save Pulse pattern to PATTERNS/SEQUENCER/PULSE/
→ Include control column configuration
→ Include scene data
```

### What Happens When You Save a Song
```
User Action: Standard SAVE workflow
→ For each InstrumentClip:
   ✓ Save Piano Roll data (NoteRows)
   ✓ Save Step Sequencer state (if exists)
   ✓ Save Pulse Sequencer state (if exists)
   ✓ Save Control column configurations (all modes)
   ✓ Save Scene data (all 8 scenes)
→ Result: Complete state preservation
```

### What Happens When You Load
```
Pattern Load:
→ Loads into the CURRENT view only
→ Step pattern → only affects Step Sequencer
→ Pulse pattern → only affects Pulse Sequencer
→ Piano Roll pattern → only affects NoteRows

Song Load:
→ Restores ALL sequencer states
→ Restores piano roll data
→ User can switch between any view
```

---

## Next Steps

1. ✅ Review proposal (DONE)
2. ✅ Decide on design questions (DONE)
3. Implement Phase 1 (core serialization)
4. Implement Phase 2 (clip integration)
5. Implement Phase 3 (pattern save UI)
6. Implement Phase 4 (pattern load UI)
7. Test thoroughly
8. Update documentation


