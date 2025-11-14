## SysEx Roadmap

This document tracks outstanding work for the JSON SysEx API so that new
modules follow a consistent structure and the firmware/UI remain in sync.

### Implement Pending Kit Commands

- `createKit`, `loadKit`, `saveKit` currently return `status: "error"`.
- Implementation requires:
  - Safe construction of kits outside song load (needs empty clip + NoteRow wiring).
  - Access to kit preset browser / SD file-system helpers.
  - Persisting edits by reusing the same serialization that the UI employs.
- When adding these commands:
  - Use the shared `SysexCommon` helpers for replies.
  - Emit `^kitChanged` so the UI refreshes after load/create/save.

### Clip & Note Management API

Upcoming work will add `clip_sysex` and `note_sysex` modules. Recommended plan:

1. **Clip discovery**
   - `getClips` → list clip ids, track types, bar counts, flashing/playing state.
   - `getClipDetails` → length, tempo override, follow actions.
2. **Clip mutation (Session View aware)**
   - `createClip` with explicit `type` (`synth`, `kit`, `midi`, `cv`, `audio`), target row/column, and default length; should mirror the hardware flow of selecting a slot in Session View and pressing `NEW`.
   - `setClipColour` to adjust the clip’s internal hue (`colourOffset`) so rows mode / note & automation editors tint correctly (72-step wheel, matches Shift+encoder).
   - `setTrackColour` to drive `Output::colour` so Session View pads match remote controllers; hue range 0‑191 (hardware wheel).
   - `getTracks` to group clips by instrument/output and expose row metadata (name, colour, clip ids) for remote launchers.
   - `duplicateClip` / `moveClip` to copy or reposition clips without relying on “pressed pad” UI state; must keep section + track assignments consistent.
   - `launchSection` (scene trigger) to fire every clip in a given section/column, matching the hardware section pads.
   - `enterClip` to focus a clip (equivalent to pressing the clip’s pad) so subsequent parameter/note requests operate on it; should trigger the same rendering as a hardware enter.
   - `exitClip` to collapse back to Session View after editing so remote controllers can mirror the user’s navigation state.
   - `deleteClip` with safety checks for armed/playing clips; should broadcast `^clipRemoved` so subscribers can refresh slot grids.
   - ✅ Initial SysEx commands `getClips`, `getTracks`, `createClip`, `setClipColour`, `setTrackColour`, `duplicateClip`, `moveClip`, `launchSection`, `enterClip`, `exitClip`, `deleteClip` now exist (UI + firmware). Next iteration: add `^clipChanged` subscription + clip metadata (follow actions, armed state) and direct note editing.
3. **Note editing**
   - `getNotes` (paged) for the focused clip.
   - `addNote`, `updateNote`, `removeNote`.
   - Optional `subscribeNotes` using the same streaming pattern as params.
4. **Automation**
   - Reuse `SysexParamStream` to stream envelope/automation lane edits.

### Song Scale & Root

- ✅ Added `getSongScale` / `setSongScale` commands (Transport namespace) so hosts can mirror Deluge’s song-level key.
- `setSongScale` accepts `rootNote` (0–127) and a preset `scale` index (or `scaleName`, including the special `USER` scale); replies include `status`, `rootNote`, `scale`, and `scaleName`.
- Both commands share the existing `SysexCommon` helpers and mirror the behaviour of `Song::setRootNote` / `Song::setScale`, keeping hardware and remote controllers in sync.

### Shared Conventions

- Use `SysexCommon::startResponse`, `writeStatus`, `sendResponse` for every reply.
- Represent booleans as `0/1` integers in JSON attributes to avoid ambiguity.
- Keep push notifications (`^eventName`) as flat objects; reuse
  `SubscriberList` for any new streaming surface (clips, transport, arranger).

Keeping this checklist fresh while we expand into clips/notes ensures the JSON
schema stays coherent and the host UI can trust every response/notification
format.

