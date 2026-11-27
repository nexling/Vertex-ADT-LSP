# Over-indent warnings in if/loop blocks

The validator warned only when a block line was indented *too little*: a body line at or left of its `if`/`loop`, or an `endif`/`else`/`endloop` left of its opener. A body line pushed any distance right, or an `endif` sitting deeper than its `if`, passed silently. The native server now also warns about too much indentation. The Node server has been frozen since 2026-11-18, so this is a native-only divergence. The code is `familyIndentLine` in `server/src/validate_if_loop_indent.cpp`.

## Rules

- **One level is the file's own step.** It is the most common positive gap, in raw characters (a tab counts as 1), between an opener and its block's first body line across the file. A tie goes to the smaller gap. A 2-space file gets 2, a 4-space file 4, a tab file 1. The smallest gap was tried first: one `<TAB>  ` line under a 2-space `if` (gap 1) made every normal block in the file warn. `editor.tabSize` is not read, so a file is never flagged just for its style. A file that mixes tabs and spaces gets odd results, as it already did.
- **First body line.** The first checked body line of a block sets the block's body level. When it is more than one step deeper than the opener: `This line is indented more than one level deeper than the matching 'if'.` (or `'loop'`).
- **Siblings.** A later direct body line deeper than the body level: `This line is indented more than the other lines in this 'if' block.` A shallower line that is still right of the opener is not flagged. At or left of the opener it keeps the existing `This line should be more indented than the matching 'if' or 'loop'.`
- **`else` shares the level.** Lines after `else` must match the body level set before it. An empty `if` branch lets the `else` branch set it.
- **Nested openers are body lines.** An `if`/`loop` line inside a block is checked like any body line before it opens its own block. Before, it was not checked at all, so an `if` at its parent's column passed.
- **Closers.** `endif` and `else` must sit at their opener's column in both directions: `This line should line up with the matching 'if'.` (or `'loop'`). `endloop` keeps the old "less indented" message, because a deeper `endloop` is already reported as `endloop at different indentation level`.
- **Exempt.** Whole-line comments (`!…`, `;!…`). Continuation lines of any `\` group (only the group's first line is checked; `if` header continuations were already skipped). `# include` lines. Top-level lines outside every block.
- All new diagnostics are Warnings with no quick fix. Format Document (ADR 0013) fixes them.

Whole-line `;!` comments inside a block used to get `This line should be more indented…` whatever their indent, because a line with nothing left after `;!` stripping has indentation -1. They are now skipped, as `!` comments always were.

## Auto-indent while typing

`language-configuration.json` drove auto-indent with `increaseIndentPattern: ^\s*(if\b.*\bthen|loop)$`. `loop 10`, `then ` with a trailing space, `then ;! note` and `then;` did not match, and `else` was only an unindented line, so Enter left the next line at the opener's column and the new line was then flagged. The rules are now:

- increase after `if … then` (`then` last, before optional whitespace or a `;`/`;!` comment), after `loop` with or without arguments, and after `else`;
- decrease on `else`, `endif` and `endloop`.

`if a then b = 1` does not indent. An inline `!` is logical NOT, not a comment, so `then !x` is a statement. `if … then` is matched up to the first `;`, so a `then` inside a trailing comment does not count. Keywords are case-sensitive (ADR 0010). An `if` header continued with `\` gets no special handling: the line after `if a or \` keeps its indent, and the body after the `… then` line does not auto-indent.

## Considered Options

- **One level = `editor.tabSize` (or a fixed 2).** Rejected: every 4-space and tab file would warn on every block.
- **Consistency only.** Rejected: `if` at 0 with the whole body at 8 would pass.
- **Re-indent the new line on the server via on-type formatting.** It would match the formatter exactly, but editing the new line's whitespace on Enter risks moving the cursor. The regex runs in the client instantly.

## Tests

- `scripts/goldens/14-if-loop-indent.js` (native-only rows at the end). Its `maxNumberOfProblems` cap fixture now indents its nested `if`s, which would otherwise use up the cap with the new nested-opener warning.

## Corpus

On 2026-11-27, `scratch/inferred-included-from/corpus-gate.js` over C:\vxbd320 (442 `.prg` files in customlbab and system), before vs after, indent messages only:

- 183 new warnings in 59 files: 57 + 5 one level too deep (`if` / `loop`), 64 + 9 deeper than siblings, 23 `endif`/`else` not lined up, 25 nested openers at or left of their parent.
- 5 old "should be more indented" warnings on `endif`/`else` lines are now the "line up" message.
- Spot checks are real: mixed tab/space lines, a stray extra space, 4 spaces in a 2-space file, misaligned `endif`. One cascade: after `if !Ret exit` (an `if` without `then`, already an error that opens a block), the next column-0 `if` is now also flagged as a nested opener.
- With the smallest-gap step the same run gave 457, 204 of them in `struct_tools/BEAM.prg`.
