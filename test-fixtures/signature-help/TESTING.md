# Signature help

`textDocument/signatureHelp` for the active command-catalog command or parenthesized `call(` on the current line, as described in [Specify signature help](../../scratch/cpp-language-server-spec/issues/54-specify-signature-help.md). The golden is `scripts/goldens/37-signature-help.js` (run it with `npm run test:goldens`). Callees live under `custom/`. The caller (`custom/caller.prg`) exists only as a buffer.

Every row is checked twice. The native result must equal an oracle built from `server.js`'s winning `onSignatureHelp` (`getCommandContext` + the label assembler) with the spec deltas: lowercase catalog lookup, labels from catalog structure, and `call(` labelled `path` plus the callee's buffer-wins `name = get($IDENT)` extract. Most rows also pin `[label, activeParameter]` or `null`.

ADR 0022 overrides the oracle in the rows marked `NEW` in the golden: line-start `!` comments, single-quoted commas, unknown inner names, and a `call(` with no known inputs. Cursor rows for that ADR are in `scripts/goldens/64-signature-box-follows-the-cursor.js`.

## Capability

`signatureHelpProvider` is `{ triggerCharacters: ['(', ',', ' '], retriggerCharacters: [',', ' '] }`. `SignatureHelpContext` is ignored (a retrigger with a stale `activeSignatureHelp` recomputes).

## Active command

| Prefix | Help |
| --- | --- |
| `set_regenerable(` / `set_regenerable (` / `Ret$=set_regenerable(` / indented / mid-line | `set_regenerable(input object) regenerable value`, 0 |
| `SET_REGENERABLE(` | `SET_REGENERABLE(input object) regenerable value`, 0 (lowercase lookup, written ident) |
| `set_regenerable(Sheet,` / `!set_regenerable(` | null (past the one inside slot; comment) |
| `set_regenerable("a,b"` / `set_regenerable('a,b'` | 0 (quoted commas do not count) |
| `set_regenerable(Sheet)` | `set_regenerable regenerable value`, 0; parameters are the after-paren list only |
| `set_regenerable(Sheet) 0,` | null |
| `set_regenerable(map_size(` | `map_size(map name)`, 0 (inner catalog command wins) |
| `set_regenerable(foo(` | `set_regenerable(…)`, 0 (unknown inner name) |
| `recall(` / `my_call(…,` / `call_virtual(…,` | null |
| `map_size("n")` | null (closed, no after-paren labels) |
| `ask("q",` | `ask(param1, param2)`, 1 |
| `cube 1, 2` | `cube length x, length y, length z`, 1 |
| `cube 1, 2, 3,` / `x = cube 1, 2` / `cube(` | null (leftmost no-paren ident `x` is not a command; `cube` has 0 inside labels) |
| `cube() a, b` | `cube length x, length y, length z`, 1 |
| `set_regenerable(\nmap_size(` | `map_size(map name)` (current line only) |

## Unbounded maxima

The finite prefix is emitted, never keyword-catalog `insertText`: `add_aux_task(` is `add_aux_task(input object) param1`; `set_sheet_shape(s) a, b,` is `set_sheet_shape param1, param2, param3`, 2; `new_feature(a, b,` is `new_feature(param1, param2, param3)`, 2. One comma past that prefix is null.

Every catalog command is also swept as `name(`, `name(a,`, `name(a)`, `name(a) b,`, and `name a, b` against the oracle.

## `call(`

`callee_three_params.prg` = `obj`, `Panel`, `iswall`.

| Prefix | Help |
| --- | --- |
| `call(` / `call("missing.prg"` | null (no known inputs) |
| `call("…"` | `call(path, obj, Panel, iswall)`, 0 |
| `call("…",` / `call("…", Area` / `CALL(` / `call (` / `Ret$=call(` / mid-line / unquoted / single-quoted path | same, 1 (written ident) |
| `call("file,name.prg",` | 1 (quoted-path comma does not count) |
| `call("…", "a,b",` | 2 |
| `call("…", a, b,` | 3 |
| `call("…", a, b, c,` / `call("…", a, b)` / `call("missing.prg",` / `call("empty_callee.prg",` / space-form `call …,` | null |
| `call("sectional_callee.prg MACROS",` | `call(path, mac)`, 1 |
| `call("sectional_callee.prg", MACROS,` / `… SECTION",` | whole file `call(path, mac, form, whole)` |
| `call("crlf_callee.prg",` | `call(path, Name$, count)` (no `\r`) |
| `set_regenerable(call("…",` | `call(…)` wins; `call("…", map_size(` gives `map_size` |

A mid-line cursor and a CRLF caller line behave like the prefix up to the cursor.

## Snapshot

`buffer_wins_callee.prg` on disk gives `disk1`, `disk2`. With the file open as a buffer its text wins (including edits); after `didClose` disk returns. `plaintext` and never-opened documents are null.
