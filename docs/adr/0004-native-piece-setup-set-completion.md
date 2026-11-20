# Native piece.setup set completion from vxsettings

Two commands name a piece set from Vertex's `piece.setup` settings: the 6th argument of `new_piece`, e.g. `new_piece(SP,5,EP,5,Dir,"FRAMING.WALL","WOOD",code$)`, and the first argument after the parentheses of `set_piece_set`, e.g. `set_piece_set(Pce) "TRUEFEATS",2`. In the native server only, ordinary completion in that slot now offers the piece sets that are active for the user's configuration, and hovering a quoted set string shows the set's resolved lines. Code is in `server/src/piece_setup_sets.cpp` and `server/src/vxsettings_xml.cpp`.

## Slot

- Inside the quotes, completion replaces the quoted content.
- In an empty slot (only spaces or tabs before the cursor), it inserts `"NAME"`, with a leading space when the cursor is right after `)` (`set_piece_set(p)|`). A typed bare word is replaced by the quoted NAME, because the site is retried at the word's start.
- A slot holding other text (`Set$ &`, `a+`), or a cursor right before an existing quoted string, is no site.
- The list pops up on the server's existing `,`, space and `)` triggers. Keywords and variables stay below the sets, because the slot can also hold a variable.
- `set_piece_set` needs its closing `)`. Its after-paren arguments end at a `!` comment.

## Sources and merge

- `system/programs/vxsettings.xml` is the base. The piece.setup tree is the SetupItem whose trimmed `<Summary>` is `piece.setup`.
- The Custom tree's `programs/vxsettings.xml` is an **overlay by GUID**, not a second list. Its piece.setup parent has the same GUID and no Summary. A SetupItem with a matching GUID merges into the System one, and a DataItem with a matching GUID replaces the System one, whatever its `Custom` value (`REPLACE`, `ADD` or anything newer). Everything else is added. An added DataItem goes before the item's trailing `function_end`. `OriginalData` is ignored. A Custom file without a System file gives no sets.
- `system/programs/vxsettings_config.xml` gives the active Application, Config, Units and Language.
- The folders are found like `forms/` (`resolveLibrarySubdirectory`).

## Which sets

- Every descendant SetupItem whose trimmed Summary starts with `piece.setup`. Only `function_begin` DataItems name sets: NAME is the first word of `Data`, read whole with dots (`FRAMING.WALL`), and the rest is DESCR. One pair of surrounding double quotes is dropped from DESCR, because multi-word descriptions are quoted (`FRAMING.WALL "WALL FRAMING"` has DESCR `WALL FRAMING`). `set_begin` fragments (`DEFSECT`, `PCETYPES`, …) are not offered.
- In one SetupItem the first active `function_begin` opens the set and later ones are ignored until `function_end`. The active `include_set` and `param` lines in between belong to it.
- Names dedupe case-sensitively, and the first one wins in merged file order.

## Settings conditions

A DataItem is active when its `Config`, `Application`, `Units` and `Language` conditions all match the active values. `Optbit`, `UILanguage`, `Final`, `Type` and `Custom` are ignored, because the server cannot know the option bits and showing more beats hiding sets.

- The grammar is names (which may contain `-`) joined by `and` / `or`, each optionally preceded by `not`, with no parentheses (none appear in the shipped files). Names compare case-insensitively.
- Evaluation is a **strict left-to-right fold**: `A or B and C` is `(A or B) and C`. For the two shipped strings that mix operators (`BD or BD-View and not InD`, `not ED-Lite and ED or HD`) this gives the same answer as standard precedence, because each attribute has a single active value.
- It fails open. A missing attribute, a blank expression, a missing config file or value, and an unparsable expression all match.

## Items and hover

- Completion always offers NAME for both commands. Label and detail are NAME, and NAME is the inserted text (quoted in an empty slot). The documentation shows `Description:` DESCR, the active `include_set` lines as written (not expanded), and the active `param` lines. Lines from Custom are marked *(custom)*, and a set added by Custom is tagged *(custom set)*.
- Items keep merged file order on ties and rank in completion family `8` (ADR 0001), with substring filtering (ADR 0002).
- Hover only applies to a quoted string. In a bare slot, the normal variable hover stays.
- `new_piece` hover matches NAME.
- `set_piece_set`'s mode decides the lookup the way Vertex does: bit 1 set means "read by piece.setup function", clear means "read by description". When the mode is a literal number or `+` sum (`2`, `1+2`), bit 1 picks NAME or DESCR. A missing or non-literal mode (`Mode`) tries NAME, then DESCR. The first match in file order wins.
- No diagnostics for unknown set names (`"WOOD.STD"` in the shipped macros is in neither file).

## Caching

The result is cached on the three files' path, size and modification time and rebuilt on the next request after any of them changes, disappears or appears. A first parse of the 2.8 MB System file takes about 20 ms. Hover runs on the message loop, so the first hover after a change pays that cost once.

This is a deliberate divergence from the frozen Node server, which has no counterpart. The contract is asserted by `server/tests/piece_setup_sets_test.cpp` (target `piece-setup-sets-test`; set `VXBD_ROOT` for an optional smoke run against a real install).

## Considered Options

- Custom wins by set name: rejected. The Custom file is a GUID overlay whose parent has no Summary, so it would find no sets and miss REPLACE edits to System sets.
- Standard `and`-before-`or` precedence: rejected for the simpler left-to-right fold, which gives the same results on the shipped data.
- Reusing the macro library XML scanner: rejected. It is specific to that format, so a small generic reader was added and the macro library reader was left alone.
- Invalidating through `didChangeWatchedFiles`: replaced by modification-time keys, which also catch files changed outside the watched globs.
