# Control-block pairing in hover and document highlight (native only)

Hovering `endif` used to show only the catalog line, and VS Code's built-in word highlight marked every `endif` in the file. Now the native server pairs `if`/`else`/`endif` and `loop`/`endloop`. It shows the partner in hover and highlights the whole block. The Node language server is frozen (since 2026-11-18) and has neither feature, so this is a deliberate native/Node divergence. The code is in `server/src/control_block_pairing.cpp`.

## Pairing rules

- One on-demand pass over the live buffer, current file only. Pairing does not follow `# include` or `##included_from`.
- Lines are skipped and stripped the same way as in full-document validate: the shared per-line skip (`isSharedPerLineSkip` in `text_util`, also used by `validate.cpp`), then `;!` comment stripping.
- Keywords are case-sensitive and must open the trimmed line. An inline if-then (`isInlineIfThen`, now shared with the validator) opens no block. `else` attaches to the innermost open `if`. The if and loop stacks are independent, as in the validator.
- **Divergence from the validator:** keywords must be whole words. `endiffoo`, `elsewhere = 1`, `loopn` and `if$` are not keywords. The validator uses prefix matching for its stacks, so in those rare lines the validator and the hover can disagree. Hover is the more useful of the two here, and changing the validator would move existing diagnostics.
- Unlike the validator, the pass ignores `maxNumberOfProblems` and pairs inside verification-ignored sections such as `#EDIT_FORM#`.

## Hover

The pairing section goes above the existing catalog keyword hover, separated by a `---` rule:

| On | Shows |
|---|---|
| `endif` / `endloop` | `**Closes `if` (line N):**` and a code block with the opener header: the `if` line plus its `and`/`or`/`&&`/`\|\|` continuation lines up to `then`, trimmed and with comments stripped (at most 20 lines). |
| `else` | `**Else of `if` (line N):**` with the header, then `Closed by `endif` at line M`. |
| `if` / `loop` | `Closed by `endif` at line M (`else` at line K)`. |
| unmatched | `No matching `if`` / `endif` / `loop` / `endloop`. |

`then` and other keywords keep their plain catalog hover.

## Document highlight

The server now advertises `documentHighlightProvider`. With a provider registered, VS Code drops its own textual word highlight for the language. So for any position that is not a block keyword, the server copies that behaviour: it finds the word under the caret using the language `wordPattern` `\b\w+\$?` (caret at the word's end counts), then returns every case-sensitive, whole-word match in the file. Word boundaries follow VS Code's default `editor.wordSeparators` plus whitespace. Matches inside strings and comments count. All highlights use kind `Text`. A user-customised `editor.wordSeparators` is not read.

## Tests

- `server/tests/control_block_pairing_test.cpp` (target `control-block-pairing-test`).
- `scripts/goldens/56-control-block-pairing.js`, a native-only golden with no Node oracle.
