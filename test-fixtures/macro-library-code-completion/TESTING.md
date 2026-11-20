# Macro library/code completion

Golden: `scripts/goldens/42-macro-library-code-completion.js`. It runs as part of `npm run test:goldens`, or alone with `node scripts/goldens/42-macro-library-code-completion.js`. Set `GOLDEN_VERBOSE=1` to print each case's slot and items.

**Oracle.** The answers the removed Node language server gave (replayed from `scripts/goldens/oracle-tapes/`, ADR 0018) to `custom/getMacroLibraries` and `custom/getMacroCodes`. Those answers go through a verbatim copy of the host's `libraryCodeCommands` site and its item mapping from `client/out/extension.js`. Every row must equal what the language-server executable returns from ordinary `textDocument/completion`. The comparison covers the family's items only: kind Folder or Value with no `data`.

**Workspace.** Each server gets its own temp copy of this folder. The workspace folders are `system/` then `customa/`, so the Custom name comes from `includes("custom")`.

| Library (`customa/complibs/`) | State |
| --- | --- |
| `codes_lib` | Its database and `c` companion are older than `d_CODES_LIB.xml`, so they are never exported. The XML records cover a duplicate CODE, padded and empty values, SHTCODE before CODE in one record, CODE before SHTCODE, lowercase and mixed-case `code`/`Code` names (skipped), `&amp;`, a `"` in a value, CDATA, non-ASCII text, and a record with no code. |
| `macro_sys` | Copied from `test-fixtures/vertex-dbase-converter/custom/complibs/macro_sys/` with the real V32 database and its `c`/`l` companions, but no XML. Only an export by the repo-root helper makes it a library. |
| `Mixed_Case` | Only `d_FRM_SHEET.xml`. The label is `MIXED_CASE`, and its codes come from the fallback file. |
| `other_xml` | Only `d_WALL_LAYERS.xml`. It is listed, but it gives no codes. |
| `empty_codes` | `d_EMPTY_CODES.xml` with no CODE/SHTCODE cell. It is listed, but it gives no codes. |
| `upper_ext` | `d_UPPER_EXT.XML`. The uppercase extension still counts. |
| `no_xml` | `notes.xml`, `D_UPPER.xml` and `d_DATA.dat`. It is not listed. |
| `broken_lib` | `d_BROKEN` is an empty file. The helper fails on it, no XML is written, and it is not listed. |
| `readme.txt` | A file, not a directory. It is not listed. |

`system/complibs/sys_lib` has XML but is never listed or read: System is not consulted.

| Area | Assertion |
| --- | --- |
| Sites | These are library slots: lowercase glued `new_macro(` / `new_djoint(` / `new_sheet(` with an optional space and no comma on the line, including assignment, indent, tab, mid-line, a bang line, `mynew_macro(`, text after the cursor, a later line, and CRLF. These are code slots: a closed quoted first argument, any comma, single quotes, a spaced comma, after a closed code, and a later occurrence on the line when an earlier one fails. These are not sites: an unclosed quote, a 3rd argument, after `)`, uppercase or capitalized commands, a space before `(`, `new_macros(`, unquoted names, `new_piece`, `set_macro_form`, and `change_sheet`. |
| Empty library | `new_macro("",`, `new_macro(,"CODE"` and `foo, new_macro(` are code slots with an empty library, so they add nothing. |
| Library list | `CODES_LIB`, `EMPTY_CODES`, `MACRO_SYS`, `MIXED_CASE`, `OTHER_XML`, `UPPER_EXT`. That is directory order, uppercased, with `insertText` `"LABEL"`. |
| Code list | `codes_lib` gives unique first-cell values in document order, untrimmed, with `insertText` `"code"` (not escaped). A library label in any case opens the folder (Windows). A missing folder, `A,B`, `other_xml`, `empty_codes` and `sys_lib` add nothing. |
| Export | Code slots for other libraries never touch `macro_sys`. The first library listing exports `macro_sys` through the real helper, and its codes then match Node. Companions, the fresh `codes_lib` database and the failing `broken_lib` database get no sidecar. |
| Missing helper | The extension root has no `Vertex_Dbase_Converter.exe`. The failure is logged (`Vertex database converter not found at …`). `macro_sys` is not listed and gives no codes, and nothing is exported. The other libraries are still listed. |
| Other | Keywords and variables stay in the list, and `isIncomplete` is false. The items carry no `textEdit`, `filterText`, `sortText`, `detail`, `documentation` or `insertTextFormat`. A plaintext document adds nothing. `completionItem/resolve` returns the items unchanged. |

Cancel while the helper runs is covered by `test-fixtures/vertex-dbase-converter/TESTING.md`. This family maps a canceled export to `RequestCancelled`.

## Code-first mode

Golden: `scripts/goldens/54-macro-code-first-completion.js`. It's native only, with no oracle. See `docs/adr/0008-native-macro-code-first-completion.md`. The workspace is this fixture plus `dup_lib`, which shares `BATHZONE` with `codes_lib` and has `DUPONLY`, and the stale `macro_sys` database with no XML. The setting is changed live through `didChangeConfiguration` `{ vertexadt: { completion: { macroCodeFirst } } }`.

| Area | Assertion |
| --- | --- |
| Off | `new_macro(` lists library folders, as above. Turning the setting off again restores them without a restart. |
| First request | Only Value pairs `LIBRARY/CODE`, sorted by code, with no `macro_sys` pairs yet. `CODES_LIB/BATHZONE` comes right before `DUP_LIB/BATHZONE`. The empty code is skipped. `MIXED_CASE` (fallback file) and `UPPER_EXT` are included. The edit range is `(`..cursor, `newText` is `"DUP_LIB", "BATHZONE"`, and there's no `filterText` when nothing is typed. |
| Background export | A later request lists `MACRO_SYS/…` pairs once `d_MACRO_SYS.xml` is written. `broken_lib` gets no XML and is not listed. |
| Matching | `zone` gives the two BATHZONE pairs, with `filterText` `zone`. `dup` gives `DUP_LIB/DUPONLY` (group 0) before `DUP_LIB/BATHZONE` (library-only, group 1). `"dup/bath` gives only `DUP_LIB/BATHZONE`. `mixed/` gives both MIXED_CASE codes. An unmatched word gives nothing. |
| Edit range | Typed after `(` up to the cursor. With an auto-closed quote it runs through the closing quote. `M = new_macro("bath", "OLD", 5)` (cursor after `bath`) covers both arguments. `new_macro(bath, , 5)` stops at the 3rd argument. With the cursor before `"codes_lib"`, the edit replaces it. |
| Sites | `new_djoint` and `new_sheet` work. A comma earlier on the line is fine. `new_piece` is not a site. A closed library (`new_macro("codes_lib", `) still lists plain codes. |
| Placeholder | A workspace with only the stale `macro_sys`: `new_macro("ma` gives one Text item, `Exporting 1 macro library…`, whose edit rewrites `"ma` unchanged. A later request gives `MACRO_SYS/…` pairs instead. |
