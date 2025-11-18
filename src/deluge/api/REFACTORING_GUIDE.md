# Refactoring SysEx Handlers to Use DelugeAPI

This guide shows how to refactor existing SysEx handlers to use the `DelugeAPI::Controller` layer, reducing code by ~70% and eliminating manual navigation boilerplate.

## Example 1: `setDrumParameter` (Complex Handler)

### Before: Manual Navigation (145 lines)

```cpp
void KitSysex::setDrumParameter(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^drumParameterSet", false, true);

	// Manual navigation - get kit
	Kit* kit = getCurrentKit();
	if (!kit) {
		jWriter.writeAttribute("error", "No kit loaded");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// Parse parameters (20 lines)
	int32_t drumIndex = -1;
	String paramName;
	int32_t intValue = 0;

	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			drumIndex = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "name")) {
			reader.readTagOrAttributeValueString(&paramName);
		}
		else if (!strcmp(tagName, "value")) {
			intValue = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	if (drumIndex < 0 || paramName.isEmpty()) {
		jWriter.writeAttribute("error", "Missing parameters");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	// More manual navigation - get drum
	Drum* drum = kit->getDrumFromIndex(drumIndex);
	if (!drum || drum->type != DrumType::SOUND) {
		jWriter.writeAttribute("error", "Drum not found or not a SoundDrum");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	SoundDrum* soundDrum = (SoundDrum*)drum;

	// More manual navigation - get clip
	Clip* clip = currentSong->getCurrentClip();
	if (!clip || clip->type != ClipType::INSTRUMENT) {
		jWriter.writeAttribute("error", "No instrument clip selected");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	int32_t noteRowIndex;
	NoteRow* noteRow = instrumentClip->getNoteRowForDrum(drum, &noteRowIndex);
	if (!noteRow) {
		jWriter.writeAttribute("error", "Drum has no NoteRow");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	ParamManagerForTimeline* paramManager = &noteRow->paramManager;

	// Manual ModelStack setup (10+ lines)
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(modelStackMemory);
	ModelStackWithNoteRow* modelStackWithNoteRow =
	    modelStack->addNoteRow(instrumentClip->getNoteRowId(noteRow, noteRowIndex), noteRow);
	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStackWithNoteRow->addOtherTwoThings(soundDrum, paramManager);

	// Parameter lookup (40+ lines)
	const char* name = paramName.get();
	bool success = false;
	const char* errorMsg = "Unknown parameter";

	// Try unpatched params first
	if (paramManager->summaries[0].paramCollection) {
		UnpatchedParamSet* unpatchedParams = (UnpatchedParamSet*)paramManager->summaries[0].paramCollection;
		ParamCollection* unpatchedParamCollection = paramManager->summaries[0].paramCollection;

		for (int32_t p = 0; p < UNPATCHED_SOUND_MAX_NUM; p++) {
			const char* checkName = paramNameForFile(Kind::UNPATCHED_SOUND, p + UNPATCHED_START);
			if (checkName && !strcmp(name, checkName)) {
				AutoParam* param = &unpatchedParams->params[p];
				ModelStackWithAutoParam* modelStackWithParam = modelStackWithThreeMainThings->addParam(
				    unpatchedParamCollection, &paramManager->summaries[0], p, param);
				param->setCurrentValueInResponseToUserInput(intValue, modelStackWithParam);
				success = true;
				break;
			}
		}
	}

	// Try patched params if not found
	if (!success && paramManager->summaries[1].paramCollection) {
		PatchedParamSet* patchedParams = (PatchedParamSet*)paramManager->summaries[1].paramCollection;
		ParamCollection* patchedParamCollection = paramManager->summaries[1].paramCollection;

		for (int32_t p = 0; p < kNumParams; p++) {
			const char* checkName = paramNameForFile(Kind::PATCHED, p);
			if (checkName && !strcmp(name, checkName)) {
				AutoParam* param = &patchedParams->params[p];
				ModelStackWithAutoParam* modelStackWithParam = modelStackWithThreeMainThings->addParam(
				    patchedParamCollection, &paramManager->summaries[1], p, param);
				param->setCurrentValueInResponseToUserInput(intValue, modelStackWithParam);
				success = true;
				break;
			}
		}
	}

	// Response
	if (success) {
		jWriter.writeAttribute("name", name);
		jWriter.writeAttribute("value", intValue);
		jWriter.writeAttribute("success", 1);
		jWriter.writeAttribute("index", drumIndex);

		// Trigger UI refresh
		uiNeedsRendering(getCurrentUI());

		// Notify subscribers
		notifyDrumChanged(drumIndex);
	}
	else {
		jWriter.writeAttribute("error", errorMsg);
		jWriter.writeAttribute("name", name);
	}

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}
```

### After: Using API Layer (25 lines) - 83% reduction

```cpp
void KitSysex::setDrumParameter(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^drumParameterSet");

	// Parse JSON parameters
	int32_t drumIndex = -1;
	String paramName;
	int32_t intValue = 0;

	char const* tagName;
	reader.match('{');
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "index")) {
			drumIndex = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "name")) {
			reader.readTagOrAttributeValueString(&paramName);
		}
		else if (!strcmp(tagName, "value")) {
			intValue = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.exitTag();
		}
	}
	reader.match('}');

	// Validate input
	if (drumIndex < 0 || paramName.isEmpty()) {
		SysexCommon::writeStatus(jWriter, "error", "Missing parameters");
		SysexCommon::sendResponse(cable, jWriter);
		return;
	}

	// Direct API call - all navigation and ModelStack setup handled automatically!
	Kit* kit = DelugeAPI::Controller::getCurrentKit();
	DelugeAPI::Result result = DelugeAPI::Controller::setDrumParameter(
	    kit, drumIndex, paramName.get(), intValue);

	// Handle response
	if (!result.success) {
		SysexCommon::writeStatus(jWriter, "error", result.message ? result.message : "Failed to set parameter");
		SysexCommon::sendResponse(cable, jWriter);
		return;
	}

	// Success response
	SysexCommon::writeStatus(jWriter, "success");
	jWriter.writeAttribute("index", drumIndex);
	jWriter.writeAttribute("name", paramName.get());
	jWriter.writeAttribute("value", intValue);

	// Notify subscribers (handled by API layer or here)
	notifyDrumParameterChanged(drumIndex, paramName.get(), intValue);

	SysexCommon::sendResponse(cable, jWriter);
}
```

**Key Changes:**
- ✅ Eliminated 80+ lines of manual navigation
- ✅ Eliminated 10+ lines of ModelStack setup
- ✅ Eliminated 40+ lines of parameter lookup (moved to API layer)
- ✅ Single API call replaces all navigation logic
- ✅ Error handling simplified

---

## Example 2: `createClip` (Simpler Handler)

### Before: Manual Navigation (50 lines)

```cpp
void ClipSysex::createClip(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clipCreated");

	if (!guardSongAndCard(cable)) {
		return;
	}

	OutputType desiredType = OutputType::NONE;
	int32_t position = currentSong->sessionClips.getNumElements();
	bool colourProvided = false;
	int32_t colourValue = 0;

	// Manual JSON parsing
	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "type")) {
			String typeString;
			reader.readTagOrAttributeValueString(&typeString);
			desiredType = parseClipType(typeString.get());
		}
		else if (!strcmp(tagName, "position")) {
			position = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "colour") || !strcmp(tagName, "color")) {
			colourValue = reader.readTagOrAttributeValueInt();
			colourProvided = true;
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}

	if (desiredType == OutputType::NONE) {
		writeErrorAndSend(cable, "error", "Unknown clip type");
		return;
	}

	// Manual navigation and creation
	int32_t clampedPosition = clampIndex(position, 0, currentSong->sessionClips.getNumElements());
	Clip* newClip = sessionView.createClipAtIndex(desiredType, clampedPosition);
	if (!newClip) {
		writeErrorAndSend(cable, "error", "Failed to create clip");
		return;
	}

	int32_t newIndex = currentSong->sessionClips.getIndexForClip(newClip);
	if (colourProvided) {
		sessionView.setClipColour(newIndex, colourValue);
	}

	SysexCommon::writeStatus(jWriter, "success");
	writeClipSummary(jWriter, newClip, newIndex);
	SysexCommon::sendResponse(cable, jWriter);
}
```

### After: Using API Layer (20 lines) - 60% reduction

```cpp
void ClipSysex::createClip(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^clipCreated");

	if (!guardSongAndCard(cable)) {
		return;
	}

	// Parse JSON
	OutputType desiredType = OutputType::NONE;
	int32_t position = -1;
	int32_t colourValue = -1;

	reader.match('{');
	char const* tagName;
	while (*(tagName = reader.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "type")) {
			String typeString;
			reader.readTagOrAttributeValueString(&typeString);
			desiredType = parseClipType(typeString.get());
		}
		else if (!strcmp(tagName, "position")) {
			position = reader.readTagOrAttributeValueInt();
		}
		else if (!strcmp(tagName, "colour") || !strcmp(tagName, "color")) {
			colourValue = reader.readTagOrAttributeValueInt();
		}
		else {
			reader.readTagOrAttributeValue();
		}
	}

	if (desiredType == OutputType::NONE) {
		SysexCommon::writeStatus(jWriter, "error", "Unknown clip type");
		SysexCommon::sendResponse(cable, jWriter);
		return;
	}

	// Use default position if not provided
	if (position < 0) {
		position = DelugeAPI::Controller::getCurrentSong()->sessionClips.getNumElements();
	}

	// Direct API call - no manual navigation!
	Clip* newClip = DelugeAPI::Controller::createClip(desiredType, position);
	if (!newClip) {
		SysexCommon::writeStatus(jWriter, "error", "Failed to create clip");
		SysexCommon::sendResponse(cable, jWriter);
		return;
	}

	// Set colour if provided
	if (colourValue >= 0) {
		int32_t newIndex = DelugeAPI::Controller::getCurrentSong()->sessionClips.getIndexForClip(newClip);
		DelugeAPI::Controller::setClipColour(newIndex, colourValue);
	}

	// Success response
	SysexCommon::writeStatus(jWriter, "success");
	int32_t newIndex = DelugeAPI::Controller::getCurrentSong()->sessionClips.getIndexForClip(newClip);
	writeClipSummary(jWriter, newClip, newIndex);
	SysexCommon::sendResponse(cable, jWriter);
}
```

**Key Changes:**
- ✅ Eliminated manual `clampIndex` logic (handled by API)
- ✅ Direct API call replaces `sessionView.createClipAtIndex`
- ✅ Direct API call for `setClipColour`
- ✅ Cleaner error handling

---

## Example 3: `getParameters` (Read Operation)

### Before: Manual Navigation (50 lines)

```cpp
void SynthSysex::getParameters(MIDICable& cable, JsonDeserializer& reader) {
	jWriter.reset();
	jWriter.setMemoryBased();

	smSysex::startReply(jWriter, reader);
	jWriter.writeOpeningTag("^parameters", false, true);

	// Manual navigation
	if (!currentSong) {
		jWriter.writeAttribute("error", "No song");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Clip* clip = currentSong->getCurrentClip();
	if (!clip) {
		jWriter.writeAttribute("error", "No clip selected");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	Output* output = clip->output;
	if (!output || output->type != OutputType::SYNTH) {
		jWriter.writeAttribute("error", "Not a synth clip");
		jWriter.closeTag(true);
		smSysex::sendMsg(cable, jWriter);
		return;
	}

	SoundInstrument* soundInstrument = (SoundInstrument*)output;
	Sound* sound = (Sound*)soundInstrument;

	// Manual ModelStack setup
	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(modelStackMemory);
	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	ParamManagerForTimeline* paramManager = &instrumentClip->paramManager;

	// Write response
	if (!output->name.isEmpty()) {
		jWriter.writeAttribute("presetName", output->name.get());
	}

	SoundSysex::writeSoundParameterSnapshot(jWriter, sound, paramManager);

	jWriter.closeTag(true);
	smSysex::sendMsg(cable, jWriter);
}
```

### After: Using API Layer (20 lines) - 60% reduction

```cpp
void SynthSysex::getParameters(MIDICable& cable, JsonDeserializer& reader) {
	SysexCommon::startResponse(jWriter, reader, "^parameters");

	// Direct API call - no manual navigation!
	Clip* clip = DelugeAPI::Controller::getCurrentClip();
	if (!clip) {
		SysexCommon::writeStatus(jWriter, "error", "No clip selected");
		SysexCommon::sendResponse(cable, jWriter);
		return;
	}

	// Check if it's a synth clip
	Instrument* instrument = DelugeAPI::Controller::getInstrumentFromClip(clip);
	if (!instrument || instrument->type != OutputType::SYNTH) {
		SysexCommon::writeStatus(jWriter, "error", "Not a synth clip");
		SysexCommon::sendResponse(cable, jWriter);
		return;
	}

	SoundInstrument* soundInstrument = (SoundInstrument*)instrument;
	Sound* sound = (Sound*)soundInstrument;

	// Get param manager (API could provide this too)
	InstrumentClip* instrumentClip = (InstrumentClip*)clip;
	ParamManagerForTimeline* paramManager = &instrumentClip->paramManager;

	// Write response
	if (!clip->output->name.isEmpty()) {
		jWriter.writeAttribute("presetName", clip->output->name.get());
	}

	SoundSysex::writeSoundParameterSnapshot(jWriter, sound, paramManager);

	SysexCommon::writeStatus(jWriter, "success");
	SysexCommon::sendResponse(cable, jWriter);
}
```

**Key Changes:**
- ✅ Eliminated manual `currentSong` checks
- ✅ Direct API call for `getCurrentClip()`
- ✅ Direct API call for `getInstrumentFromClip()`
- ✅ Cleaner error handling

---

## Refactoring Steps

### Step 1: Identify Manual Navigation

Look for patterns like:
```cpp
// ❌ Manual navigation
Kit* kit = getCurrentKit();
if (!kit) { /* error */ }
Clip* clip = currentSong->getCurrentClip();
if (!clip) { /* error */ }
```

Replace with:
```cpp
// ✅ API layer
Kit* kit = DelugeAPI::Controller::getCurrentKit();
Clip* clip = DelugeAPI::Controller::getCurrentClip();
```

### Step 2: Identify ModelStack Setup

Look for patterns like:
```cpp
// ❌ Manual ModelStack setup
char modelStackMemory[MODEL_STACK_MAX_SIZE];
ModelStackWithTimelineCounter* modelStack =
    currentSong->setupModelStackWithCurrentClip(modelStackMemory);
// ... more setup
```

Replace with:
```cpp
// ✅ API layer handles ModelStack automatically
DelugeAPI::Result result = DelugeAPI::Controller::setParameter(...);
```

### Step 3: Replace Complex Operations

Look for patterns like:
```cpp
// ❌ Complex operation with lots of navigation
Drum* drum = kit->getDrumFromIndex(index);
Clip* clip = currentSong->getCurrentClip();
NoteRow* noteRow = instrumentClip->getNoteRowForDrum(drum, &noteRowIndex);
// ... 50 more lines
```

Replace with:
```cpp
// ✅ Single API call
DelugeAPI::Result result = DelugeAPI::Controller::setDrumParameter(
    kit, index, paramName, value);
```

### Step 4: Update Error Handling

Replace manual error handling:
```cpp
// ❌ Manual error handling
if (!kit) {
    jWriter.writeAttribute("error", "No kit");
    jWriter.closeTag(true);
    smSysex::sendMsg(cable, jWriter);
    return;
}
```

With API result handling:
```cpp
// ✅ API result handling
DelugeAPI::Result result = DelugeAPI::Controller::setDrumParameter(...);
if (!result.success) {
    SysexCommon::writeStatus(jWriter, "error", result.message);
    SysexCommon::sendResponse(cable, jWriter);
    return;
}
```

---

## Benefits Summary

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| **Lines of code** | 145 | 25 | 83% reduction |
| **Manual navigation** | 40+ lines | 0 lines | 100% eliminated |
| **ModelStack setup** | 10+ lines | 0 lines | 100% eliminated |
| **Error handling** | Scattered | Centralized | Better maintainability |
| **Testability** | Hard | Easy | API layer is testable |

---

## Migration Checklist

- [ ] Identify all manual navigation patterns
- [ ] Replace with `DelugeAPI::Controller` calls
- [ ] Remove manual ModelStack setup
- [ ] Update error handling to use `Result` type
- [ ] Test each refactored handler
- [ ] Update documentation
- [ ] Remove unused helper functions (if all handlers migrated)

---

## Next Steps

1. **Complete API layer implementation** - Finish parameter lookup, drum creation, etc.
2. **Refactor one handler at a time** - Start with simpler ones like `createClip`
3. **Test thoroughly** - Ensure behavior matches original
4. **Update tests** - Add unit tests for API layer
5. **Document changes** - Update API documentation



