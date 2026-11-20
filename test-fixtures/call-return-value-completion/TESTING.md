# Call return-value completion

On the line right after a `Name$ = call(...)` assignment, `textDocument/completion` adds one `return:ret{field}$` snippet for each `KEY=` field that the callee packs into its return string. The rules are in [Specify call return-value completion](../../scratch/cpp-language-server-spec/issues/50-specify-call-return-value-completion.md). The golden is `scripts/goldens/38-call-return-value-completion.js`; run it with `npm run test:goldens`. The callees live under `custom/`. The caller (`custom/caller.prg`) only ever exists as a buffer.

Every row is checked twice. First, the native items must equal an oracle: the Node host's previous-line `callRegex` and item mapping (`client/out/extension.js`), run over a verbatim copy of `extractReturnValues` and fed the buffer-wins callee text. Second, each row pins its expected labels. Node itself reads the callee from disk only; the spec is buffer-wins. The field comments in the oracle's `COMMENTS` table were checked once against the live Node server's `custom/getCallReturnValues`.

## Site

The site is only the immediately previous line, trimmed. It must match `/(\w+\$)\s*=\s*call\(\s*("[^"]+"|'[^']+'|[^,)]+)(?:,\s*[^)]*)*\)/`. The current line's text and cursor are ignored, and line 0 is never a site.

| Previous line | Items (`return_fields.prg` = `W`, `H`, `DEPTH`, `WIDTH`) |
| --- | --- |
| `Ret$=call("…")` / `Ret$ = call("…")` / `Result$=call('…')` / `Ret$=call( "…")` / `Ret$=call(…)` | all four |
| `Ret$=call("…", a, b)` / `Ret$=call("…", a, (b)` | all four (the remaining arguments are ignored) |
| `Ret$=call(… MACROS)` / `Ret$=call("… MACROS")` / `Ret$=call("… SECTION")` | all four (the suffix is stripped from the path; the scan is still the whole file) |
| tab indent / `x = 1; Ret$=…` / `!Ret$=…` / trailing `! bang` / `_ret$` / `1x$` / `foo(Ret$=call("…"))` / `a$b$=` (`b$`) / NBSP around `=` | all four |
| `Ret$=call("…" )` | none (dirty path `return_fields.prg"` does not resolve, copied from Node) |
| `Ret=` / `found=` / bare `call(` / `$=` / `CALL(` / `Call(` / `call (` / space-form / `recall(` / `call_virtual(` / missing `)` / `Ret$=call(…,` | none |
| `call( )` / `call("")` / `missing.prg` / `empty.prg` / `no_fields.prg` / empty / `foo` | none |
| two calls on one line | the leftmost match wins, even when its callee is missing |

Cursor cases: completing on the call line itself, or two lines below it, gives none. A current line with text, a mid-line cursor, a CRLF caller, typed `(`, Invoked, and no context all still read the previous line. A remaining-param comma slot on the next line still gets the snippets.

## Fields

- **Exit name.** The return variable is named by the last trimmed line that starts with lowercase `exit` and matches `exit\s+(\w+\$)`. In `last_exit.prg` only `SECOND` appears. In `quirks.prg` these lines do not set the name: `! exit`, `exit "OK"`, `exitret$`, `EXIT`.
- **No exit.** A callee without an exit still parses its concat lines, because Node checks `includes('')`. In `no_exit.prg`, the left-hand side overwrites the name, which gives `ONE` and `TWO` but not `THREE`.
- **Format line.** A line counts only if it contains `=`, `"`, `&`, and the current name, and its left-hand side is a string variable. Each double-quoted part may give one uppercase `KEY=` or `|KEY=` key. Packed `"W=1|H=2"` with no `&` gives nothing; with `&`, it gives only the first key (`packed.prg` → `A`). Single-quoted keys, lowercase keys, and lines that do not contain the name give nothing.
- **Duplicates and sections.** Duplicate format lines repeat their fields; there is no dedup. `#EDIT_FORM#` bodies are scanned too (`quirks.prg` → `PLAIN`, `PLAIN`, `X`, `Z`, `BANG`, `FORM`). The whole file is scanned regardless of section (`sectional.prg MACROS` → `MAC`, `OTH`). A CRLF callee works the same way.

## Items

- `label` is `return:ret{lower}$ `, with a trailing space. `kind` is Snippet (15). `insertText` is a leading newline, then `ret{lower}$ = strfield({Caller$},"{KEY}") `, a newline, and `ret{lower} = ret{lower}$ `, each line with its trailing space. There is no `insertTextFormat`, `sortText`, `filterText`, `textEdit`, `detail`, `data`, or `commitCharacters`.
- `documentation` is markdown: a `vertexadt` fence holding `"Extract" {KEY} "from return string"`. When the callee has a comment for that key, the fence adds `!Variableinfo: {comment}`. The comment is the first non-empty one on a non-string variable named exactly `KEY` in the callee: `plate width`, `[number] height` (the tag is kept), and `second depth comment` (the first `DEPTH` has no comment). `WIDTH$` is a string variable, so it does not document `WIDTH`.

## Snapshot

- `buffer_wins.prg` on disk gives `DISK`. While the file is open as a buffer, the buffer text wins, including its comment and later edits. An empty buffer gives none. After `didClose`, the items come from disk again.
- `plaintext` buffers get no return-value items. The cheap keyword-variable list is always present.
- `completionItem/resolve` leaves these items unchanged.
