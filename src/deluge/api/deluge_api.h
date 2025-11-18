/*
 * Copyright © 2024 Synthstrom Audible Limited
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
#include "model/clip/clip.h"
#include "model/instrument/instrument.h"
#include "model/model_stack.h"
#include "modulation/params/param.h"

class Song;
class Clip;
class InstrumentClip;
class AudioClip;
class Kit;
class Drum;
class SoundDrum;
class NoteRow;
class ModelStackWithAutoParam;
class ParamManagerForTimeline;

namespace DelugeAPI {

/// Result structure for API operations
struct Result {
	bool success;
	Error error;
	const char* message;

	Result() : success(false), error(Error::NONE), message(nullptr) {}
	Result(bool s, Error e = Error::NONE, const char* msg = nullptr) : success(s), error(e), message(msg) {}

	static Result ok() { return Result(true); }
	static Result fail(Error e, const char* msg = nullptr) { return Result(false, e, msg); }
};

/// Direct model access controller - eliminates manual navigation boilerplate
class Controller {
public:
	// ===== Song & Clip Management =====

	/// Get current song (nullptr if none loaded)
	static Song* getCurrentSong();

	/// Get current clip (nullptr if none selected)
	static Clip* getCurrentClip();

	/// Get clip by index in session
	static Clip* getClipByIndex(int32_t index);

	/// Create a new clip at the specified index
	/// Returns nullptr on failure
	static Clip* createClip(OutputType type, int32_t insertIndex);

	/// Duplicate a clip from sourceIndex to targetIndex
	/// Returns nullptr on failure
	static Clip* duplicateClip(int32_t sourceIndex, int32_t targetIndex);

	/// Delete a clip by index
	static Result deleteClip(int32_t index);

	/// Set clip color
	static Result setClipColour(int32_t index, int32_t colourOffset);

	/// Enter (open) a clip by index
	static Result enterClip(int32_t index);

	// ===== Kit & Drum Management =====

	/// Get current kit from active clip (nullptr if not a kit)
	static Kit* getCurrentKit();

	/// Get kit from a specific clip
	static Kit* getKitFromClip(Clip* clip);

	/// Get drum by index from a kit
	static Drum* getDrumFromIndex(Kit* kit, int32_t index);

	/// Get drum index from a kit
	static int32_t getDrumIndex(Kit* kit, Drum* drum);

	/// Add a new drum to a kit
	/// Returns nullptr on failure
	static Drum* addDrum(Kit* kit, DrumType type);

	/// Remove a drum from a kit
	static Result removeDrum(Kit* kit, int32_t index);

	/// Set sample for a SoundDrum
	static Result setDrumSample(Kit* kit, int32_t index, const char* filePath);

	// ===== Parameter Management =====

	/// Get ModelStack for parameter access on current clip
	/// Automatically handles ModelStack setup based on clip type
	/// Returns nullptr on failure
	static ModelStackWithAutoParam* getParamStack(Clip* clip, const char* paramName);

	/// Get ModelStack for parameter access on a drum's NoteRow
	/// Automatically handles ModelStack setup
	/// Returns nullptr on failure
	static ModelStackWithAutoParam* getDrumParamStack(Kit* kit, int32_t drumIndex, const char* paramName);

	/// Set a parameter value on a clip
	/// Automatically handles ModelStack setup
	static Result setParameter(Clip* clip, const char* paramName, int32_t value);

	/// Set a parameter value on a drum
	/// Automatically handles ModelStack setup
	static Result setDrumParameter(Kit* kit, int32_t drumIndex, const char* paramName, int32_t value);

	/// Get a parameter value from a clip
	/// Returns 0 on failure (check result.success)
	static Result getParameter(Clip* clip, const char* paramName, int32_t& valueOut);

	/// Get a parameter value from a drum
	/// Returns 0 on failure (check result.success)
	static Result getDrumParameter(Kit* kit, int32_t drumIndex, const char* paramName, int32_t& valueOut);

	// ===== Instrument Access =====

	/// Get current instrument from active clip (nullptr if none)
	static Instrument* getCurrentInstrument();

	/// Get instrument from a clip
	static Instrument* getInstrumentFromClip(Clip* clip);

	// ===== NoteRow Access =====

	/// Get NoteRow for a drum in the current clip
	/// Returns nullptr if not found
	static NoteRow* getNoteRowForDrum(Kit* kit, Drum* drum);

	/// Get NoteRow index for a drum
	/// Returns -1 if not found
	static int32_t getNoteRowIndexForDrum(Kit* kit, Drum* drum);

private:
	// Internal helpers for ModelStack setup
	static ModelStackWithTimelineCounter* setupModelStackForClip(Clip* clip);
	static ModelStackWithNoteRow* setupModelStackForNoteRow(InstrumentClip* clip, NoteRow* noteRow,
	                                                        int32_t noteRowIndex);
};

} // namespace DelugeAPI
