# Object-parameter completion

Golden: `scripts/goldens/41-object-parameter-completion.js`. It runs as part of `npm run test:goldens`, or alone with `node scripts/goldens/41-object-parameter-completion.js`. Set `GOLDEN_VERBOSE=1` to print each case's object type and items.

**Oracle.** The live Node language server (`node server/out/server.js --stdio`) answers `custom/getVariableObjectType` and `custom/getObjectParameters`. Those answers go through a verbatim copy of the host's `extractGetCommandInfo` and its item mapping from `client/out/extension.js`. Every row must equal what the language-server executable returns from ordinary `textDocument/completion`. The comparison covers the family's items only: kind Property, with `sortText` starting `0`.

**Workspaces.** Each server gets its own temp copy of the libraries, with the workspace folder set to that copy's `custom/`:

| Library | Source | State |
| --- | --- | --- |
| `macro_sys` | `test-fixtures/vertex-dbase-converter/custom/complibs/macro_sys/` (real V32 database plus `c`/`l` companions) | The database is touched newer than its XML, so it is stale and the repo-root helper exports it. |
| `fresh_lib` | `custom/complibs/fresh_lib/` here | The database is older than its XML, so it is never exported. It has synthetic records: entities and numeric references, CDATA, `PARAMETERS` `A=1\|B\|C=2=3\|`, excluded and exact-case `Category` cells, unnamed and empty-name cells, a self-closing cell, an SHTCODE-only record, and a second case-insensitive CODE match that never wins. |
| `frm_lib` | `custom/complibs/frm_lib/` here | It has only `d_FRM_SHEET.xml`, so the `d_{library}.xml` fallback is exercised. |

| Area | Assertion |
| --- | --- |
| Sites | These are sites: the ten lowercase commands, the 2nd-inside comma for `get_*`, the after-`)` slot for the three `set_*`, inner spaces, a substring like `xget_obj_param(`, a bang line, mid-line, tab indent, and a cursor before trailing text. These are not sites: uppercase, a 3rd-arg comma, typing the field, `get_macro_param`, after-paren `get_pnt_param`, `set_pnt_param`, and an unclosed `set_obj_param(`. |
| Object type | The type timeline of this buffer, with the latest write at or above the cursor line in the same named section. Name lookup is case-sensitive. `P+2` / `P[1]` reduce to `P`. A quoted name is `generic`. A write in another section, or file-level with the cursor in a section, gives `generic`. The comment `[type]` fallback applies only to a name with no timeline rows (a PXYZN name, skipped when strict mode is off). The bracket text is trimmed and lowercased. CRLF works. |
| Static lists | profile / sheet / selemobj rows. A present-but-empty list wins (`area`). `get_material_param`, `get_area_param` and `get_section_param` use their `any` list. `get_area_param` has the `CODE` / `THICKNESS` extra-argument `insertText`. `get_pnt_param`, `get_sheet_param`, `get_joint_param` without an identity, and the `set_*` commands add nothing. |
| XML lists | Macro identity uses `new_macro`, joint identity uses `new_djoint`. Arguments resolve from literals (either quote) or from a plain `Name = "…"` above. A name ending `$` never resolves, which is copied from Node. An identity below the cursor is not used. An unresolved code falls back to the static table. Macros match CODE or SHTCODE and joints match CODE only, both case-insensitively, and the first record wins. Field items are `"{Name}"` / `Value in database: …` / `Macro field (…)` or `Joint field (…)`. `PARAMETERS` rows are labelled `PARAMETER RECORD, standard value: …` with detail `{command} - undefined`, and a trailing `\|` adds an empty-label row. An identity whose record has no fields shows the generic list. `get_material_param` ignores the XML. |
| Export | Static cases never spawn the helper. The stale `macro_sys` XML is re-exported, and the result is real helper output. Companions and the fresh XML are not exported. |
| Spec over Node | A missing `complibs/{library}` folder makes Node's `custom/getObjectParameters` throw (an uncaught `readFileSync`). The executable instead returns the generic list (empty for `get_obj_param`). |
| Missing helper | The extension root has no `Vertex_Dbase_Converter.exe`. The stale database is logged (`Vertex database converter not found at …`) and skipped, the sidecar is left untouched, and the existing XML is read. |
| Other | Keywords and variables stay in the list, and `isIncomplete` is false. The items carry no `textEdit`, `filterText`, `data` or `insertTextFormat`. A plaintext document adds nothing. `completionItem/resolve` returns the items unchanged. |

Cancel while the helper runs (kill, delete the half-written sidecar, a waiter that never spawns) is covered by the Vertex Dbase Converter helper test in `test-fixtures/vertex-dbase-converter/TESTING.md`. This family maps a canceled export to `RequestCancelled`.
