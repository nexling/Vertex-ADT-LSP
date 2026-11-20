# Native slot completion matches substrings

VS Code's fuzzy filter only accepts an item when the first typed character matches the start of a word in its `filterText`, so typing `cornermodel` never finds `1cornermodel`. There's no client setting that changes this. In the native server only, slot completion families now filter themselves. Their items stay when the label contains the typed text anywhere, ignoring case. Kept items take VS Code's typed text as their `filterText`, so the client keeps them. Every response with an active slot is `isIncomplete`, so VS Code asks again on each keystroke. Keywords keep VS Code's normal filtering.

- **Typed text**: what VS Code filters against, i.e. the item's textEdit range start to the cursor, else the `\w+\$?` word ending at the cursor. Leading spaces and quotes are ignored for matching.
- **Sites**: checked at the cursor first, then at the typed word's start (`new_macro(cor` is the `new_macro(` slot). dbcreate and debug filename try the word's start first, because dbcreate already offers the *next* field slot at the end of a word. textEdit ranges that end before the cursor are stretched to it.
- **Order**: `sortText` is `0` + family digit + match group + family key (see ADR 0001). The group is `0` for a start-of-label match or nothing typed, and `1` for a match elsewhere.
- **Cache**: the last raw listing per (document, family), keyed by line and the text before the typed word. Re-requests (`triggerKind` 3) reuse it. Any other request reads fresh, so a slot is re-read whenever you enter it.

This is a deliberate divergence from the frozen Node server. The contract lives in `server/src/slot_completion.h` and is asserted by `scripts/completion-ranking.js` and `scripts/goldens/48-slot-substring-completion.js`.

## Considered Options

- Static `filterText` holding every suffix of the label: no extra requests, but VS Code's fuzzy matching across the suffixes adds noise. Rejected.

## Consequences

- VS Code doesn't highlight the matched characters in slot items, because `filterText` is the typed text.
- Placeholder text typed in a slot (e.g. `SECTIONOFFILE`) filters every item out. VS Code already did this client-side.
- Call return-value covers the whole line after `Name$ = call(...)`, so any word typed on that line filters its snippets.
