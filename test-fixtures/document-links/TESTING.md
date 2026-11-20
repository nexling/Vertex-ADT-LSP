# Document links

`textDocument/documentLink` for `vertexadt`, `plaintext`, and `log`, per [Specify document links](../../scratch/cpp-language-server-spec/issues/46-specify-document-links.md). Golden: `scripts/goldens/31-document-links.js` (`npm run test:goldens`). Workspace root is this folder. The folder **name** contains `user` so `debug "./…"` resolves here; a second session with name `document-links` has no debug-command links.

Every link has `range` (raw path capture; quotes included except the debug-header FILE path, which is inside the quotes), `target` (file URI, optional `#Ln` 1-based fragment), and tooltip exactly `Open {fsPath}`. No `documentLink/resolve`. Empty / no matches → `[]`. Advertise `documentLinkProvider.resolveProvider = false`.

Concat order: include/call paths, then at most one debug-header link, then `debug "./…"`.

## `caller.prg`

| Site | Expect |
| --- | --- |
| `# include "target.prg"` | `target.prg`; range includes quotes; no fragment |
| `#include target.prg` | `target.prg`; no fragment |
| `# INCLUDE target.prg` | no link |
| `# include sectioned.prg JOINT` | `sectioned.prg#L2` (first disk `includes('#joint#')`) |
| indented `# include sectioned.prg SECTION` | `sectioned.prg#L5` (`SECTION` is a fragment ident) |
| mid-line `# include target.prg JOINT` | `target.prg`; **no** fragment |
| `##included_from target.prg` | `target.prg`; **no** fragment |
| bang `! # include target.prg` | `target.prg` |
| `Ret$=call("target.prg")` | **two** identical quoted-path links |
| `CALL(` / `recall(` / `call_virtual(` | no link |
| space-form `call target.prg` | `target.prg` |
| `call ("target.prg")` | no link (capture includes parens) |
| `$DIR_MACROS/from_dollar.prg` | `macros/from_dollar.prg` |
| `$DIR_EXPORT/nope.prg` | no link |
| `missing_not_a_file.prg` | no link |
| `debug "./_caller.dbg"` and indented `'./…'` | `_caller.dbg`; no fragment |
| `debug macros/…`, mid-line `debug "./…"`, `debug "./../outside.prg"` | no debug-command link |

Same include/call and debug-command links when this text is opened as `plaintext`.

## `dump.prg`

Concat: `# include target.prg`, then header `sectioned.prg#L2` (range is the FILE path inside the quotes), then `debug "./_caller.dbg"`. Same three families as `log`.

## `dump-missing.prg` / `dump-dollar-miss.prg`

Non-`$` missing FILE still links to that path. `$DIR_EXPORT` miss is no header link.

## Empty / no user folder

Empty `vertexadt` → `[]`. Workspace folder name without `user` keeps include/call links and drops debug-command links.
