# References

`textDocument/references` for `vertexadt`, per [Specify references](../../scratch/cpp-language-server-spec/issues/44-specify-references.md). Golden: `scripts/goldens/29-references.js` (`npm run test:goldens`). Workspace root is this folder (`custom/` and `system/` trees).

Every result is `Location[]` (never `null`). Ranges are single-line and span the matched text. Order: this buffer, then the definition file (if it is a linked file), then each `##included_from` in source order, each file once.

## `custom/main.prg` (cursor on the `Result = …` line unless stated)

| Cursor on | Expect |
| --- | --- |
| `Alpha` | 10 sites in this buffer, then `lib/inc.prg`, `syslib/sys.prg`, `lib/late.prg` (`##included_from` after the cursor still counts; duplicate `inc.prg` is scanned once) |
| | Case-insensitive (`alpha`, `ALPHA`); plain `;` comment and `#EDIT_FORM#` lines kept; `! Alpha = 99`, `;! Alpha = 3`, `"Alpha"`, `Alpha$`, `Alpha2`, `MyAlpha`, `Alpha_b` skipped; `"a;!b" + Alpha` kept (`;!` inside quotes) |
| | Not followed: `# include lib/not_followed.prg`, `call "lib/called.prg"`, `##INCLUDED_FROM lib/upper.prg`, unresolved `lib/missing.prg` |
| `Alpha`, `includeDeclaration: false` | same minus `Alpha = 1` |
| `Alpha`, no `context` | same as `includeDeclaration: true` |
| `Alpha$` on `x = Alpha$ + …` | same as `Alpha` (the `$` drops from the word) |
| `Shared` | this buffer, then `sys.prg` (the definition file, last include first), then `inc.prg` |
| `Shared`, `includeDeclaration: false` | drops only `sys.prg` `Shared = 2` |
| `LibVar`, `includeDeclaration: false` | drops `inc.prg` `LibVar = 10`; keeps `late.prg` `LibVar = 12` |
| `Nowhere`, `includeDeclaration: false` | the one use (no locator, no filter) |
| `foo` on `y = foo + foo-bar + foo$` | `foo` inside `foo-bar = 2` and inside `foo-bar` (prefix hits), the `foo` use; not `foo$` |
| `foo-bar` | the two `foo-bar` sites |
| `Name$` on `q = Name$ + Name` | only the bare `Name` (searches `Name`, skips `Name$`) |
| `Name$x` on `v = Name$x + name$X` | both `Name$x` sites (word keeps `$`) |
| `if` / `"Alpha"` / between words | `[]` |
| `Alpha` with `lib/inc.prg` open and edited | the buffer line (buffer-wins); disk again after `didClose` |
| Same text opened as `plaintext` | `[]` |
| Unopened document | `[]` |

## `custom/dump.prg` (debug dump)

| Cursor on | Expect |
| --- | --- |
| `Total` on `Count = Total` | every `Total` (same regex scan as a normal file) |
| same, `includeDeclaration: false` | drops `Total = Total + 1` column 0 (most-recent assignment) |
| `foo-bar` on `Count = foo-bar`, `includeDeclaration: false` | both sites (dump parse misses hyphens; nothing declared above) |

Every cursor position in both files, with `includeDeclaration` true and false (1638 requests), matched the Node server (`server/out/server.js --stdio`). Node's `findFile` does not decode `%20`, so that sweep ran on a copy of this folder at a path without spaces.
