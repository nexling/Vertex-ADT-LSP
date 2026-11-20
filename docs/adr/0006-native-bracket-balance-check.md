# Bracket balance check (native only)

`ret$=call("$DIR_MACROS/x.prg AFTERGEN",Panel_Label$` used to pass with no error. `call` is not a catalog command. Its only syntax check ran when the line contained a `)`, and even then it skipped a `call(` that had no match. The native server now checks every code line for unbalanced `(` `)` and `[` `]`. The Node language server is frozen (since 2026-11-18) and still misses these, so this is a deliberate native/Node divergence. Code is in `server/src/bracket_balance.cpp`.

## Rule

- Every code line is checked, including both sides of `then` on an `if` line, and `debug` and `messagebox` lines. Skipped: blank lines, whole-line `!` comments, `##` directive lines, `#NAME#` section markers and verification-ignored sections (`EDIT_FORM`). `@nodebug` ranges are dropped by the post-filter as usual.
- Ignored: brackets inside `"…"` or `'…'`, and anything after an unquoted `;!`. A bare `!` is code (logical NOT: `if (!a) then`, `!=`). An earlier draft treated it as a comment and gave hundreds of false positives on real programs.
- A line ending in `\` (before any `;!`) continues onto the next one, so the brackets of `dbcreate(` and any other continued statement can close on a later physical line. A skipped line ends the logical line.
- **At most one diagnostic per logical line.** Scanning left to right, the first extra or wrong closer wins. If there is none, the innermost opener still open at the end wins, whether it is `(` or `[`. Fixing it reveals the next one.

## Diagnostic

Severity Error, source `Syntax exception - ADT Plugin` (the same as other syntax errors), code `vertexadt/bracket-balance`.

| Case | Message | Range |
|---|---|---|
| Unclosed `(` right after a name | `Mismatching parentheses for '<name>'` | from the `(` to the last code character on its physical line (before `;!`, trailing blanks and `\`) |
| Unclosed bare `(` | `Mismatching parentheses` | same |
| Unclosed `[` right after a name | `Mismatching brackets for '<name>'` | same, from the `[` |
| Unclosed bare `[` | `Mismatching brackets` | same |
| Extra `)` / `]` | `Mismatching parentheses` / `Mismatching brackets` | just that character |
| Crossed pair, e.g. `f(a[1)]` | `Expected ']' but found ')'` | from the opener to the wrong closer; it can span lines across `\` |

`<name>` is the identifier directly before the opener: word characters, optionally ending in `$` (`a$[1`). If there is a space before it (`ascii (35`) or the word starts with a digit, it counts as bare.

## Replaced and suppressed

- The old catalog-command check (`Mismatching parentheses for '<cmd>'` on the whole line, in `validate_command_syntax.cpp`) is removed. An unclosed catalog command now produces no command-syntax issues, and this family reports it.
- Command syntax skips any line a bracket issue covers, so a crossed pair such as `x=abs(a[1)]` does not also give `Parameter must be a valid variable in abs`. Other families (used-before-declaration and so on) still run.
- The issues are computed once per validate pass (`bracketIssuesFor`) and shared by command syntax and this family. The family runs right after the per-line loop and counts toward `maxNumberOfProblems`.

## Real-code findings

A scan of the 882 `.prg` files under `C:\vxbd320` gives 21 diagnostics. All of them are real imbalances, for example `if (findstr(code$,"Garderob") then` and `set_djparam(Jnt) "PRE~WEBCOUNT",WebCount)` (a stray trailing `)`). Some are in Vertex's own `system/` programs, so the Vertex runtime evidently tolerates at least an unclosed `if (` and a trailing extra `)`. They are still reported as errors.

## Tests

- `server/tests/bracket_balance_test.cpp` (target `bracket-balance-test`) covers the document walk and `fullDocumentValidate`, including the suppressed command-syntax error.
- `scripts/goldens/52-bracket-balance.js` is a native-only golden on `test-fixtures/bracket-balance/bracket_balance.prg`, with no Node oracle. `scripts/goldens/15-command-syntax.js` now expects the narrow `findstri` range (14–25) instead of the whole line.
