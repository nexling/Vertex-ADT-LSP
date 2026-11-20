# Debug-filename completion

`textDocument/completion` adds one File item on top of the cheap keyword-variable list, per [Specify debug-filename completion](../../scratch/cpp-language-server-spec/issues/48-specify-debug-filename-completion.md). Golden: `scripts/goldens/34-debug-filename-completion.js` (`npm run test:goldens`). Local-only (this buffer + document basename): no disk `.dbg` listing. The spec cites no fixture directory, so the assertions are inline and the probe files (`wall.prg`, `my wall.v.prg`) exist only as buffers.

Every `vertexadt` response is a `CompletionList` with `isIncomplete: false` that still contains the full cheap list, including the catalog `debug` keyword. Family items are the ones whose `detail` starts with `Debug output file for current document`. Non-`vertexadt` is `null`. Trigger kind is ignored. `completionItem/resolve` leaves the item unchanged.

## Item

| Enclosing section | `label` = `insertText` | `detail` |
| --- | --- | --- |
| none | `./_<base>.dbg` | `Debug output file for current document` |
| `<name>` | `./_<base>_<name>.dbg` | `Debug output file for current document and section <name>` |

Kind File (17), `sortText` `'0'`, no `textEdit` / `filterText` / `data` / `documentation`. `<base>` is `path.parse(fsPath).base`, extension kept and percent-decoded: `wall.prg` → `./_wall.prg.dbg`, `my wall.v.prg` → `./_my wall.v.prg.dbg`.

## Site: `/^\s*debug\s+$/` on the UTF-16 prefix (`|` = cursor)

| Prefix | Expect |
| --- | --- |
| `debug \|`, `debug   \|`, `debug<TAB>\|`, `    debug \|`, NBSP indent + ideographic space | item |
| `debug \|foo` (prefix end, not line end); character past line end (clamped) | item |
| `debug foo\|`, `debug\|`, `DEBUG \|`, `Debug \|`, `debugger \|`, `x = debug \|`, `debug ./\|`, `debug "./\|`, `debug(\|`, astral char before `debug` | nothing |

## Section (`getEnclosingSectionName`)

Nearest previous line with a `#([A-Za-z0-9_]+)#` match (last non-overlapping match on it) and nearest later line (first match). Both required, compared case-insensitively, previous spelling returned. Otherwise the item has no section. The current line is skipped. Checked against the `extension.js` helper.

| Buffer | Section |
| --- | --- |
| `#Macros#` / debug / `#MACROS#` | `Macros` |
| `#A# x #B#` / blank / debug / blank / `#b#` | `B` |
| `#A##B#` / debug / `#B#` | `B` |
| `#A#` / debug / `#A# #C#` | `A` |
| `#A#` / debug / `#C# #A#` | none |
| debug / `#A#`; `#A#` / debug | none |
| `#OUTER#` / `#INNER#` / debug / `#OUTER#` | none |
| `#my-sec#` / debug / `#my-sec#` | none |
| `#X#` / `debug \|#Y#` / `#X#` | `X` |
| `#Y#` / `debug \|#Y#` | none |
| `  !#1A# ;` / debug / `foo #1a# bar` | `1A` |
| `#EDIT_FORM#` / debug / `#EDIT_FORM#` | `EDIT_FORM` |
| CRLF `#S#` / debug / `#s#` | `S` |
| `#S#` / `;FILE x SECTION y` / debug / `#S#` | `S` |
