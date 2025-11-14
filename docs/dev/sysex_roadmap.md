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
2. **Clip mutation**
   - `createClip` / `deleteClip` / `duplicateClip`.
   - `setClipProperty` (color, loop length, follow action, target track).
3. **Note editing**
   - `getNotes` (paged) for the focused clip.
   - `addNote`, `updateNote`, `removeNote`.
   - Optional `subscribeNotes` using the same streaming pattern as params.
4. **Automation**
   - Reuse `SysexParamStream` to stream envelope/automation lane edits.

### Shared Conventions

- Use `SysexCommon::startResponse`, `writeStatus`, `sendResponse` for every reply.
- Represent booleans as `0/1` integers in JSON attributes to avoid ambiguity.
- Keep push notifications (`^eventName`) as flat objects; reuse
  `SubscriberList` for any new streaming surface (clips, transport, arranger).

Keeping this checklist fresh while we expand into clips/notes ensures the JSON
schema stays coherent and the host UI can trust every response/notification
format.

