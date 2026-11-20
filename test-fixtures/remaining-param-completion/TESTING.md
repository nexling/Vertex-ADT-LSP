# Remaining-param completion

After a trailing comma on parenthesized `call(`, `textDocument/completion` adds the callee's remaining `name = get($IDENT)` left-hand names, as described in [Specify remaining-param completion](../../scratch/cpp-language-server-spec/issues/49-specify-remaining-param-completion.md). The golden is `scripts/goldens/36-remaining-param-completion.js` (run it with `npm run test:goldens`). Callees live under `custom/`. The caller (`custom/caller.prg`) exists only as a buffer.

Every row is checked twice. The native items must equal an oracle built from the Node host's `callWithPathRegex` and item mapping (`client/out/extension.js`) over `extractFunctionParameters` / `extractCallParametersFromText`, fed buffer-wins callee text. Each row also pins its expected labels. Node itself reads the callee from disk only; the spec is buffer-wins.

## Site

The line prefix up to the UTF-16 cursor must match the host regex, so it ends with `,\s*` after a path argument. `paramPosition` is the naive comma count of the prefix minus one.

| Prefix | Items (`callee_three_params.prg` = `obj`, `Panel`, `iswall`) |
| --- | --- |
| `call("…",` / `call("…",  ` / `call("…" ,` / `call(…,` / `call('…',` | `obj`, `Panel`, `iswall` |
| `call("…", Area,` / `call("…",,` / `call("…", MACROS,` / `call("…", Area) extra,` | `Panel`, `iswall` |
| `call("…", Area, Sel,` / `call("…", "a,b",` | `iswall` |
| `call("…", Area, Sel, "1",` | none (past the last row) |
| `Ret$=call(` / `Ret$ = call(` / `Ret=call(` / `CALL(` / `Call(` / `recall(` / `my_call(` / mid-line `x = 1; call(` / `    call (` / `!call(` | `obj`, `Panel`, `iswall` |
| `call("… SECTION",` / `call("… SECTIONOFFILE",` | whole file (keyword, not a target) |
| `call("…", Area` / `call("…"` / `call("…", Area)` | none (no trailing comma) |
| `call_virtual(` / space-form `call …,` / `# include …,` / `call(,` / `call( ,` / `call("",` / `call("missing.prg",` / `call("empty_callee.prg",` / `call("file,…",` | none |
| `call("…", call("callee_commented.prg",` | first `call(` wins: `Panel`, `iswall` |

Cursor cases: a mid-line cursor right after a comma is a site, a cursor inside an argument is not, a CRLF caller line drops its `\r`, and Invoked / typed `,` / no context all behave the same.

## List

- Every `name = get($IDENT)` in source order, not CALLPARAM-filtered (`Other = get($FOO)` is listed). `Get(` is not a match. Two gets on one line are two rows. Bang lines count.
- A named target (`call("sectional_callee.prg MACROS",`) keeps that section's rows only. `EDIT_FORM` is not skipped. A section with no gets, or an unknown name, gives none. Whole file walks every line.
- Each row is its own item: `label` is the left-hand name, `kind` is Text (1) when it contains `$`, else Variable (6). `sortText` is `00`, `01`, … relative to the current comma. `documentation` is the plain string `Input Parameter {1-based extract index}: {comment}`. There is no `insertText`, `textEdit`, `filterText`, `data`, or `detail`.
- The comment is `split('"')[1]` after the line's first `;!` outside quotes: `;!"plate width"`, `;! "has spaces"`, and `;!"[pvector] keep tag"` (kept as is). `;! no quotes` and `;!""` fall back to `No comment given. See file!`. A CRLF callee's comment does not carry `\r`.

## Snapshot

- `buffer_wins_callee.prg` on disk gives `disk1`, `disk2`. With the file open as a buffer, its text wins (including edits). After `didClose` the list reads disk again.
- `plaintext` buffers get no remaining-param items.
- The cheap keyword-variable list is always present. No path/section picker items appear at a remaining-param comma.
- `completionItem/resolve` leaves these items unchanged.
