# Definition

`textDocument/definition` for `vertexadt`, per [Specify definition](../../scratch/cpp-language-server-spec/issues/43-specify-definition.md). Golden: `scripts/goldens/28-definition.js` (`npm run test:goldens`). Workspace root is this folder (`custom/` and `system/` trees).

Every result is one `Location` (not an array, not a `LocationLink`) or `null`. Range starts at the first `\bName` on the target line (case-insensitive) and spans the definition word length.

## `custom/main.prg` (cursor on the `Result = …` line unless stated)

| Cursor on | Expect |
| --- | --- |
| `Alpha` | `Alpha = 1`, column 0, length 5 |
| `Beta` | `set Beta`, column 4 |
| `Gamma` | `Gamma(3) = 5` |
| `Delta` | null (`! Delta = 9` is a bang line) |
| `Hidden` | null (`;!` strips `Hidden = 2`) |
| `foo-bar` on `f`, `b`, and one past the end | `foo-bar = 4`, length 7 (hyphen is one word; inclusive end) |
| `Name$` | null (word is `Name`; `Name$ =` does not declare `Name`) |
| `LibVar` | `custom/lib/included_lib.prg` via `##included_from lib/included_lib.prg` |
| `SysVar` | `system/syslib/sys_only.prg` (indented `##included_from`, System fallback) |
| `Shared` | `system/syslib/sys_only.prg` (last include first) |
| `LastLineVar` | null (declared on the included file's last line, which is skipped) |
| `UpperVar` | null (`##INCLUDED_FROM` is not a site) |
| LHS of `Alpha = Alpha + 1` | `Alpha = 1` (the cursor line is never the target) |
| RHS of `Self = Self` | null |
| `Alpha` inside `"Alpha"` | null (quotes) |
| `if` | null (keyword) |
| `Point` / `point` / `IF` on `Other = …` | `Point = 3` / null (keyword, exact case) / null (passes filter, no declaration) |
| `LibVar` with `included_lib.prg` open and edited | the buffer line (buffer-wins); disk again after `didClose` |
| Same text opened as `plaintext` | null |
| Unopened document | null |

## `custom/dump.prg` (debug dump)

| Cursor on | Expect |
| --- | --- |
| `Total` on `Count = Total + NSP` | `Total = Total + 1` (most-recent assignment) |
| `NSP` on the same line | `vec_sub(NSP) …`, column 8 (paren output jumps in a dump) |
| LHS `Total` on `Total = 9` | that same line (dump search includes the cursor line) |
| `foo-bar` on `Count = foo-bar` | `foo-bar = 1` (dump parse misses hyphens; falls through to the declaration walk) |

Every cursor position in both files was swept against Node's own `onDefinition` helpers sliced from `server/out/server.js`: identical results.
