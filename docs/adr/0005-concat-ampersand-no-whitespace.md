# No whitespace around a concatenation `&` (native only)

Vertex rejects whitespace touching a string-concatenation `&`. `date$&" "&time$` is valid. `date$ &" " &time$` is not. The native server now reports every such `&` as an error and offers fixes. The Node language server is frozen (since 2026-11-18) and still accepts spaced `&`, so this is a deliberate native/Node divergence. Code is in `server/src/concat_whitespace.cpp`.

## Rule

- Any space or tab directly before or after a concatenation `&` is an error, on either side: `a$ &b$`, `a$& b$`, `a$ & b$`.
- These are not concatenations: `&` inside `"…"` or `'…'`, `&&` (logical AND in `if`), and anything after the first unquoted `!` (which covers `;!` and inline `!` comments).
- Leading indentation does not count, so a continuation line that starts with `&` is fine. Whitespace between `&` and a trailing `\` does count, so `a$ &\` is an error.
- It runs on every code line: assignments, `debug`, `messagebox`, command arguments, call parameters and `# include` / `call` paths. Skipped: whole-line `!` comments, `##` directive lines, `#NAME#` section markers, `EDIT_FORM` sections, `@nodebugstart` … `@nodebugend` ranges and validation-excluded documents (debug dumps, vxdbase winstyle shells).
- It is always on, whatever `strictMode` is, because the program fails in Vertex.

## Diagnostic

- One diagnostic per offending `&`. Its range is the `&` plus the whitespace on both sides. Severity Error, source `ADT-Syntax`, code `vertexadt/concat-whitespace`, message `Remove spaces around '&': string concatenation must not contain whitespace`.
- The post-filter normally keeps one diagnostic per message and line. For this code the key also includes the start character, so two spaced `&` on one line give two diagnostics.
- It counts toward `maxNumberOfProblems` like the other post-loop families and runs last.
- The other parsers still accept `\s*&\s*` (call params, used-before, unused variables, table validator, command parse). A spaced `&` produces only this diagnostic, with no knock-on errors.

## Code actions

- **QuickFix** `Remove spaces around '&'` (preferred): one per request diagnostic with this code. It replaces the diagnostic's range with `&`.
- **SourceFixAll** `Remove spaces around '&' in file (N)`: re-walks the buffer and replaces every span with `&`, bottom-up. It is offered when the request carries a concat-whitespace diagnostic, or when `only` explicitly asks for `source.fixAll` (fix on save). It is not offered when there is nothing to fix.

## Tests

- `server/tests/concat_whitespace_test.cpp` (target `concat-whitespace-test`) covers the span finder, the document walk, `fullDocumentValidate` and the code actions. It links `VERTEX_ADT_LS_CORE_SOURCES`, which is every server source except `main.cpp`.
- `scripts/goldens/50-concat-whitespace.js` is a native-only golden with no Node oracle.
