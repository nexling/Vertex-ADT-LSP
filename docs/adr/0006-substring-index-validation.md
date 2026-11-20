# Substring index validation (native only)

Vertex strings are 1-indexed. `A$[3,6]` returns characters 3–6, and a `0` end index means "to the end" (`P$=A$[P+11,0]`, adt.pdf chapter 2). `filename$[0,dotindex-1]` is a bug. It must be `filename$[1,dotindex-1]`. The same manual page says you may not assign into a substring (`A$[I,I]="T"`, `B$[5,8]="WELD"`). The native server now checks both. The Node language server is frozen (since 2026-11-18) and does not check substrings, so this is a deliberate native/Node divergence. Code is in `server/src/substring_index.cpp`.

## Rule

- A substring is `identifier$[a,b]` with exactly two top-level arguments. Nested `[]` / `()` and quotes inside the arguments are respected, and a substring nested in an argument (`A$[B$[1,1],0]`) is checked on its own. Brackets after a name without `$`, one or three arguments, and the manual's `[ ,Mode]` optional-argument notation do not match.
- Only literal integers (`[+-]?digits`) are checked. Variables and expressions (`A$[P+11,0]`, `A$[len(A$),len(A$)]`, `A$[I,I]`) are runtime values and are never flagged.
- Skipped: text inside `"…"` / `'…'`, anything after the first unquoted `!` (covers `;!` and inline comments), lines starting with `#` or `!`, `dbcreate(` continuation lines, `EDIT_FORM` sections and `@nodebugstart` … `@nodebugend` ranges. `debug` lines are checked.
- Always on, whatever `strictMode` is.

## Diagnostics

All use source `Syntax exception - ADT Plugin` and count toward `maxNumberOfProblems`. The family runs last, after concat whitespace.

| Code | Severity | When | Range | Message |
|---|---|---|---|---|
| `vertexadt/substring-start-index` | Error | Start is a literal below 1 (`0`, `-1`, `00`, `-0`) | start argument | `Substring start index must be 1 or higher (strings are 1-indexed); got <literal>.` |
| `vertexadt/substring-end-index` | Warning | End is a negative literal | end argument | `Substring end index must be 0 (to end) or a positive position; got <literal>.` |
| `vertexadt/substring-empty-range` | Warning | Both literal, start ≥ 1, end > 0, start > end | text between `[` and `]` | `Substring start <s> is after end <e>; result is empty.` |
| `vertexadt/substring-assignment` | Error | `name$[...]` at statement start (line start, after `;`, or after a whole-word `then`) followed by a single `=` | `name$[...]` | `Cannot assign to a substring (A$[i,j] = ...); build the new string with concatenation instead.` |

The post-filter normally keeps one diagnostic per message and line. For these codes the key also includes the start character, so two substrings with the same problem on one line both show.

## Code action

- **QuickFix** `Use 1-based start index` (preferred): one per `vertexadt/substring-start-index` diagnostic. It replaces the diagnostic's range with `1`. There is no fix-all, and the warnings and the assignment error get no fix.

## Tests

- `server/tests/substring_index_test.cpp` (target `substring-index-test`) covers the line scan, `fullDocumentValidate` and the code action.
- `scripts/goldens/51-substring-index.js` is a native-only golden with no Node oracle.
