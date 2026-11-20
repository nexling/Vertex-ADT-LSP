# Native macro code-first completion

Macro library/code completion asks for the library first and then the code. That's awkward when you remember a code but not which library it lives in. A new setting, `vertexadt.completion.macroCodeFirst` (default `false`), changes the 1st argument of `new_macro(` / `new_djoint(` / `new_sheet(` in the native server: it lists one `LIBRARY/CODE` pair per non-empty code across every Custom `complibs/` library, in place of the library names. Picking a pair writes both arguments. The same code can sit in several libraries and you still choose between them, because every pair carries its library.

- **Site.** A 1st argument that's still being typed: empty, an unquoted word, or an unclosed quote (VS Code auto-closes quotes, so `new_macro("cor|")` counts), with no comma after the command. Unlike library mode, a comma *before* the command no longer hides the slot. A closed quoted library (`new_macro("WALLS"|` or `new_macro("WALLS", |`) is still a code slot and lists that library's codes, setting or not.
- **Matching.** Typed text without `/` is a case-insensitive substring of `LIBRARY/CODE`. With `/`, the text left of the first `/` must be a substring of the library and the text right of it a substring of the code (`wal/corn`). This replaces ADR 0002's label-substring rule for this family only. Leading spaces and the opening quote are ignored.
- **Ranking.** Pairs where the code matched come first (match group `0`), then pairs where only the library matched (group `1`). Within a group, pairs sort by code, then by library, case-insensitively, so the same code from different libraries sits side by side. The key is a zero-padded rank under the usual family band (ADR 0001).
- **Edit.** One `TextEdit` from just after `(` through the 1st argument and, when a comma follows, the 2nd argument. It inserts `"LIB", "CODE"`. An empty 2nd argument (`new_macro(x|, , 5)`) is dropped together with its comma, so the result is `new_macro("LIB", "CODE", 5)`. Later arguments are kept. `filterText` is the text from `(` to the cursor, so VS Code keeps every pair the server returns.
- **Exports.** The listing is built from whatever `d_*.xml` each library holds right now, stale ones included. Libraries with a missing or stale export go to one background pass at a time, which runs the Vertex Dbase Converter for one database at a time, and the pass starts only when someone asks. A later keystroke (the list is always `isIncomplete`) picks up the new libraries. No slot listing cache is used, because exports land between keystrokes. Parsed codes are cached per library by `d_*.xml` names and write times. A database still stale after its export is recorded as failed at its write time, and it isn't retried until that time changes. Foreground and background exports share a per-database lock, so they never run the helper on the same database at once.
- **Nothing ready.** When there's no pair yet but exports are pending, the list holds one `Exporting N macro libraries…` item (kind Text). Its edit rewrites the typed text unchanged, so accepting it does nothing.

The setting is read through the existing `didChangeConfiguration` path (`completion.macroCodeFirst` under the `vertexadt` section, or flat `macroCodeFirst`), so it applies live. This is native only: the Node server is frozen and keeps library-then-code. The implementation is `macroCodeFirstCompletionItems` in `server/src/macro_library_completion.cpp`, and it is asserted by `scripts/goldens/54-macro-code-first-completion.js`.

## Considered Options

- Show library folders and pairs mixed together. Rejected: matching on the library part already covers "I know the library", and the mixed list is noisy.
- Export every library synchronously on the first request. Rejected: the first popup could block for seconds while the helper runs.
- Export eagerly at startup when the setting is on. Rejected: opening a workspace shouldn't spawn the helper for every library until someone actually asks.

## Consequences

- With the setting on, a new code-first request can open a popup even while library mode's synchronous export would still be running. The first pairs may leave out libraries whose export hasn't finished yet.
- The background pass logs through the server's stderr like the synchronous exports do. On shutdown it is cancelled (the helper is stopped) and joined.
