# Indentation formatter (native only)

Vertex ADT files mix tabs, 2-space, and 4-space indentation, and nested `if`/`loop` blocks often drift. The native server now answers `textDocument/formatting` and `textDocument/rangeFormatting` by re-indenting from block nesting. The Node language server is frozen (since 2026-11-18) and has no formatter, so this is a deliberate native/Node divergence. The code is in `server/src/indent_formatter.cpp`.

## What changes

- Only leading whitespace. Line content, tabs and spaces inside a line, trailing whitespace, line endings (CRLF/LF), and the final newline are kept.
- Output is always spaces. One level is the request's `tabSize`; `insertSpaces: false` is ignored. A tab in existing leading whitespace counts as `tabSize` columns.
- One `TextEdit` per line whose leading whitespace changes. Whitespace-only lines become empty.
- The extension sets `[vertexadt]` defaults `editor.tabSize: 2`, `editor.insertSpaces: true`, `editor.detectIndentation: false`. 2-space indentation is what most of the corpus uses, and without the last setting a tab-indented file would be formatted at a detected width.

## Depth rules

- Top level is column 0. Lines starting with `#` (`#SECTION#`, `##…`, `# include`, `# attributes on`) are always at column 0. At a `#NAME#` section marker the block stack must be empty.
- Keywords follow ADR 0010: case-sensitive, whole words (`$` continues the word), must open the line after `;!` stripping. One combined stack holds `if` and `loop`, so crossed nesting (`if … loop … endif`) is an imbalance. Pairing and the validator keep two separate stacks.
- `if` and `loop` lines sit at the current depth and open one level. `else` lines up with its `if`. `endif`/`endloop` close the level and sit at the new depth.
- Whole-line comments (`!…`, `;!…`) take the current depth. `debug` and `messagebox` lines are ordinary body lines.
- **if header:** an `if` line ending in `\` continues on the next line, and so on until a line without `\`. Continuation lines sit at the `if`'s depth plus 2 levels, so they don't line up with the body. Lines that only start with `and`/`or` but follow no `\` are not continuations (the corpus never writes that form).
- **Inline if:** the first whole-word, case-insensitive `then` outside quotes is found in the joined header. If a statement follows it, no block opens. `)then x = 1` is inline. This differs from the shared `isInlineIfThen` regex, which needs whitespace before `then`, so pairing and the validator treat `if (a = 0)then b = 1` as a block opener. `then;` and `then; !note` are an empty statement plus a comment, so they still open a block.
- **Other `\` groups** (for example `dbcreate(` argument lists) keep each continuation line's offset from the group's first line, and move only when the first line moves. Negative results clamp to column 0.

## Refusal

The formatter makes no edits at all when the document has any of the following. The server then sends `window/showMessage` (Warning) `Vertex ADT: not formatted: <reason>.` with a 1-based line:

- an unmatched `else`, `endif`, or `endloop`;
- crossed nesting (`` `endif` at line N is inside `loop` at line K ``);
- a block still open at the end of the file, or at a `#NAME#` section marker;
- an `if` with no `then` (`if Bearing`, `if !Ret exit`);
- an `if` header whose `\` runs into a blank line, a `!` comment, a `#` line, or the end of the file (the shared continuation rule, ADR 0014).

Format Selection works out depth from the whole document, edits only the selected lines (a selection ending at column 0 leaves out that line), and refuses on the same conditions anywhere in the document.

## Scope and switch

- Only open `vertexadt` documents. Debug-dump buffers (`START OF DEBUG in FILE/SECTION`) get no edits and no message.
- `vertexadt.format.enable` (default `true`). The providers are always advertised, because settings arrive after `initialize`. When the setting is off, both requests return `null`.
- There is no on-type re-indent. The language-configuration `indentationRules` already outdent `else`/`endif`/`endloop` while typing, and Newline cleanup is the only Enter-time rewrite.

## Tests

- `server/tests/indent_formatter_test.cpp` (target `indent-formatter-test`).
- `scripts/goldens/58-indentation-formatter.js`, a native-only golden with no Node oracle.
- Corpus gate: target `indent-formatter-corpus`, run as `indent-formatter-corpus.exe C:\vxbd320 [tabSize] [--list-refusals]`. Every formatted file must be idempotent and change only leading whitespace. Refusals are counted, not failures. Given a single file path, it prints that file formatted. On 2026-11-20: 1730 files, 923 already correct, 795 changed, 12 refused (all real imbalances or `if` without `then`), 0 failures.
