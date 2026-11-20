# Newline cleanup

`textDocument/onTypeFormatting` rewrites the previous line after Enter, as described in [Catalog newline cleanup rules](../../scratch/cpp-language-server-spec/issues/15-catalog-newline-cleanup-rules.md). The golden is `scripts/goldens/35-newline-cleanup.js` (run it with `npm run test:goldens`). The spec cites no fixture directory, so the assertions are inline. The probe files (`wall.prg`, `other.plaintext`, `other.log`) exist only as buffers.

Today's Node host watches for a typed `\n` / `\r\n` insert and calls `custom/cleanupLine` (`CleanUpLineFunc`). It applies the result only when the result is truthy and differs from the line. The native language server advertises `documentOnTypeFormattingProvider: { firstTriggerCharacter: "\n" }` instead. The extension contributes `"[vertexadt]": { "editor.formatOnType": true }`, so VS Code sends the request on Enter.

## Request → result

- The request only acts when `ch` is `"\n"` and the buffer's `languageId` is `vertexadt`. In every other case the result is `null`.
- The target is the line `position.line - 1`, with no line ending. On a mid-line Enter, that is the left fragment. When `position.line` is 0, the result is `null`.
- If there is a rewrite, the result is one `TextEdit`. Its range runs from `(line, 0)` to `(line, UTF-16 length)`, so a CRLF `\r` is excluded.
- If there is no rewrite, the result is `null`. No rewrite means one of these: Node returned null, the rebuilt line equals the current line, or Node would throw. A throw never becomes an LSP error.

## Rule

Only `set_obj_param`, `set_djparam`, and `set_joint_param` have `cleanupRules`. The lookup is case-insensitive and the original spelling is kept. The after-paren value slot (the 2nd slot) is quoted iff it matches `/^\d+(\.\d+)?$/`. On a mismatch the next rule moves to the next slot, so the value is left as typed. The line is rebuilt as `{name}({inside joined ", "}) {after joined ", "}{;!comment}`. The rebuild drops the indent, the assignment, and any spaces before `;!`. A `;!` inside quotes is not treated as a comment.

## Rows (all cross-checked against Node `custom/cleanupLine` over stdio)

| Line | After Enter |
| --- | --- |
| `set_obj_param(obj) NAME, 1` | `set_obj_param(obj) NAME, "1"` |
| `    set_obj_param(obj) NAME, 1` / `x = set_obj_param(obj) NAME, 1` / `set_obj_param( obj )  NAME,  1` | `set_obj_param(obj) NAME, "1"` |
| `<TAB>set_obj_param(obj) NAME, "1"` | `set_obj_param(obj) NAME, "1"` (indent only) |
| `set_obj_param(obj) NAME, 1  ;! keep` / `… "1" ;! keep` | `set_obj_param(obj) NAME, "1";! keep` |
| `set_obj_param(a,b ,  c) NAME, 1` | `set_obj_param(a, b, c) NAME, "1"` |
| `… NAME, 1.5` | `… NAME, "1.5"` |
| `… NAME, 1, 0, other[, extra]` | `… NAME, "1", 0, other[, extra]` (mode/Obj2/extras untouched) |
| `SET_OBJ_PARAM(obj) NAME, 1` | `SET_OBJ_PARAM(obj) NAME, "1"` |
| `… "NAME", 8` / `… 5, 8` / `… ";!x", 1` / `… , 1` | value quoted, name slot untouched |
| `set_djparam(jnt) P, 2` / `set_joint_param(j) P, 3[, 4]` | `… P, "2"` / `… P, "3"[, 4]` |
| `… NAME, "1"`, `VAL`, `"foo"`, `1.`, `.5`, `-1`, `1e3`, `abc, 1`, `"a" & b$`, `1 ; plain comment` | no edit (identity) |
| `get_obj_param(obj) NAME, 1`, `set_joint_punch(j) P, 1` | no edit (no `cleanupRules`) |
| empty, `if a then`, `set_obj_param(obj NAME, 1`, `! set_obj_param(…) …`, `frobnicate(obj) NAME, 1`, `set_obj_param(obj) NAME`, `set_obj_param(obj)`, `set_djparam(jnt) P` | no edit (Node throws) |

Other cases checked by the golden:

- Only the previous line is rewritten, with lines above it and an auto-indented new line present.
- A mid-line Enter rewrites the left fragment.
- In a CRLF buffer, the range excludes `\r`.
- An astral character in the comment gives a UTF-16 range end.
- `ch` `;` returns `null`.
- `plaintext` and `log` buffers return `null`.
