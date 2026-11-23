# The slot popup reopens after an edit

VS Code closes the suggestion popup when nothing matches, and never opens one on a deletion. After a typo in `call($DIR_MACROS/secfx`, deleting the `x` left no popup; you had to go back to `(` or `/`. The server can't push a popup, so the client (`client/out/slotSuggest.js`) reopens it.

- **When**: after a single-line edit at the only cursor (typing, deleting, undo/redo, paste), if VS Code didn't already ask for completions there. Cursor movement never opens it. Multi-cursor edits are ignored. It is off when `editor.suggestOnTriggerCharacters` is off.
- **Quiet check**: the client asks `vertexadt/slotCompletionProbe` (completion params) first. It runs the slot families without the keyword list, and returns one item when the popup should open, else none. Call return values never count, because their site is the whole line.
- **Exact match**: the probe stays quiet when the only match's label is the typed text, case-sensitively. If only the case differs, the popup opens so it can be corrected.
- **No match**: no popup. No fuzzy guesses.
- **Acceptance**: every item carries the `vertexadt.suggestionAccepted` command, so inserting it doesn't reopen the popup. A folder whose inserted text ends in `/` opens the next level.
- **Automatic requests**: VS Code sends quick suggestions as Invoked, which the server reads as Ctrl+Space. The client sends a request shortly after an edit at `triggerKind` 2 without a character instead (see ADR 0007), so typing a complete file name doesn't switch to its sections.
- **Path segments**: in an include/call path, the typed text is the segment the cursor is in, after the last `/`, including `.` and `-`. The directory before it is listed, without needing a cached listing. Accepting an item replaces the whole segment and keeps the rest, without doubling a `/`. A space still ends the path.
- **Sections**: `-` and `.` belong to a section name being typed, and `#BERWICK-HORIZONTAL#` style names are listed. Section diagnostics still only know `\w+` names.
- **Listing cache** (ADR 0002): on reuse, replace ranges that end right of the cursor move with the text. Ctrl+Space include/call listings have their own key.

Path items now have a `textEdit`. A path with no `/` yet lists only `$DIR_*`.

## Considered Options

- Keep the popup open with non-matching items. Rejected: Enter or Tab would insert something that wasn't asked for, and VS Code still closes it at a `/`.
- Trigger suggestions after every deletion without asking. Rejected: it would open keyword-only popups.

## Consequences

- Extensions can't see Esc, so the next edit in the slot can reopen a dismissed popup.
- Whether the popup is open is inferred from VS Code's own requests. The goldens cover the server half only (`scripts/goldens/63-slot-popup-reopens.js`).
