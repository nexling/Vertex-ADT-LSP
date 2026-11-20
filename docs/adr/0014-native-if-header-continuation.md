# `\`-continued if headers are validated as one line (native only)

An `if` condition can be split across lines with a trailing `\`:

```
      if findstr(Code$[1,2],"1x") or findstr(Code$[1,2],"2x") or \
         findstr(Code$[1,2],"4x") or findstr(Code$[1,2],"6x") then
```

The validator read each physical line on its own. So the first line got "If statement must be followed by a then." and the and/or mix check never saw the whole condition. An inline split (`if A \` / `B then x = 1`) left an `if` open with no `endif`. A continuation line shaped like `B = 2 then` was read as an assignment. The native server now joins such a header into one logical line for diagnostics. The Node server is frozen (since 2026-11-18) and still reads physical lines, so this is a deliberate native/Node divergence.

## Continuation rule

`server/src/line_continuation.cpp` holds the one rule. The indentation formatter (ADR 0013) and the validator both use it.

- A line continues when, after `;!` stripping and trailing whitespace (CR included), its last character is `\`. A `\` inside the `;!` comment doesn't count.
- Only a code line can continue or be continued. A code line is not blank, not a `!` comment, not a `#` line, and has something left after `;!` stripping.
- The chain ends at the first line that does not continue. When the line after a `\` is not a code line, or the file ends, the `\` is **dangling**.
- Joining: the first line keeps its indentation. Each `\`, its line ending and the next line's leading whitespace become one space. A segment table maps each joined byte back to its physical line and byte.
- No line limit.

## Scope: diagnostics for `if` headers only

A header is a line whose first word is `if` (case-sensitive, `$` continues the word, as in ADR 0010) and that continues. Lines that continue another statement (`dbcreate(`, `line`, assignments) are skipped, so their continuation lines never start a header. Hover, navigation, semantic tokens, extracts and the formatter's own output are unchanged.

The validator's families that use the joined header (`ifHeaderLinesFor` in `validate_if_loop_indent.cpp`):

- **Missing `then`**: checked on the joined text, reported on the `if` line.
- **Operator placement** and the **and/or mix** check run on the joined condition. A mix error is reported on the physical line that holds the operator that caused it, the first one of the second kind.
- **Inline vs. block**: decided on the joined text. That drives the if/endif stack and the indentation stack. Continuation lines are skipped by the if/endif matching and the indentation check.
- **Command syntax**: as for a one-line `if`, only the statement after `then` is checked, and it is reported on the line where that statement starts. The condition is not checked as a command, and continuation lines are skipped.
- **Unused variables / declarations**: a continuation line declares only in the text after `then`, so `B = 2 then` is a comparison.

The "pending if-header" model is unchanged and still applies after the join: an `if` without `then` followed by lines that start with `and`/`or`.

Diagnostic ranges never span lines. Header-level diagnostics keep their usual shape on the `if` line. The mix error keeps its whole-line shape, on the line of the operator.

## Dangling `\`

`Line continuation '\' must be followed by a code line.`: Error, source `ADT-Syntax`, range the `\` character. It replaces "must be followed by a then" for that header, which then opens a block as an `if` without `then` does. It is reported only for `if` headers. A `\` that leads into a `!` line inside a `dbcreate(` field list is in the corpus (`system/sysprogs/propcalc_main_cn.prg:254-256`), and the `dbcreate` join skips such lines.

The formatter's refusal (`the condition of 'if' at line N is not terminated`) now also covers a `\` that leads into a `!` comment. Before, it joined the comment and refused with "has no `then`".

## Corpus gate

C:\vxbd320 (1745 `.prg/.adt/.p/.vxt/.2d/.3d` files, root folder workspace, `maxNumberOfProblems` 100000), 2026-11-20, defaults and all opt-ins on. Totals went 6969 → 6962 and 17037 → 17030. Nothing was added, and the 7 that disappeared are the same in both modes:

- `system/macros/triggers/ModEHX.p:31` and `:44`: the false "must be followed by a then", plus 4 false `findstr accepts at most 0 parameters after parentheses` on the header lines. The `\` was being validated as a parameter after the call.
- `customa/complibs/commoninp_x/SYLOMERADD.vxt:63`: the hand-made test header `if SylP \` / `SylX then`.

The formatter corpus gate output is identical to before: 925 already correct, 793 changed, 12 refused, 0 failures.

## Tests

- `server/tests/line_continuation_test.cpp` (target `line-continuation-test`): code lines, `;!` comments and trailing whitespace, joining, the position map, each stop condition.
- `server/tests/indent_formatter_test.cpp`: a header that runs into a `!` comment, and `;!` after `\`.
- `scripts/goldens/59-if-header-continuation.js` is native only, with no Node oracle. It covers the `ModEHX.p` form, an inline split, a `B = 2 then` continuation line, an and/or mix across `\`, `;!` after `\`, and a `\` into a blank line, a comment and the end of the file.
- `npm run test:goldens` can run against another build with `VERTEX_ADT_LS_EXE=<path>`.
