# Unclosed databases and files

Enable **Vertex ADT → Diagnostics → Warn Unclosed Resources** (`vertexadt.diagnostics.warnUnclosedResources`, default on).

Opens: `dbcreate`, `dbopen`, `open`. Closes: `dbclose`, `dbclose_by_name`, `close`. A close must appear **after** the open in the execution tree (`# include` inlined, `call` entered). `call` closes of caller ids count only when this file has `common_dbases 1` / `common_files 1`.

The native server follows paths (`docs/adr/0012-native-path-aware-unclosed-resources.md`): each section is its own root, `if`/`else` and `loop` are followed, and handles still open at `break`, at the root's `exit` or at its end are reported. A failed `dbvalid` branch counts as not open. A `$CALLPARAM` handle opened for the caller is checked at each caller, up the chain.

- Yellow warning on unmatched opens (`… still open at break/exit (line N)`, `… not closed on every caller path`, `… no caller closes it`).
- Yellow warning on the `call` line of a caller that leaves the called code's handle open.
- No diagnostic on `dbopen(h) "ALREADY_OPEN",Cid` (or `Cid = dbopen(h,"ALREADY_OPEN",Cid)`): the source program that opened the database closes it.
- Yellow warning `vertexadt/close-already-open` on a `dbclose(h)` when, on some path, the latest `dbopen` of `h` was `ALREADY_OPEN` (`Database h was aliased with ALREADY_OPEN (line N); dbclose closes the source program's database`). Paths, includes and `call` (`common_dbases 1` / `$CALLPARAM`) are followed both ways. `dbclose_by_name` is never tied to an alias.
- `# include` also warns (one diagnostic per leftover handle) when included code opens something this file does not close.
- Suppress a line with `;!IGNORE CLOSE` (quick-fix available). It also works on `call`, `break`, `exit` and `dbclose` lines.

## Automated smoke test

From the repository root:

```bash
node scripts/goldens/20-unclosed-resources.js
node scripts/goldens/43-ignore-close-code-action.js
```

`runPathAwareGoldens` in the same golden is native-only (no Node oracle): call chain, branch-only close, break, IGNORE CLOSE on break, `dbvalid` guard, section scope, no caller, caller without `common_dbases`, loops, and the cross-file refresh. `runAliasCloseGoldens` is native-only too: `dbclose` of an `ALREADY_OPEN` alias (message, related link), the function form, latest open is real (silent), latest open is the alias (warns), alias on one branch only, IGNORE CLOSE on the `dbclose`, `dbclose_by_name`, and alias/close split across a `call` in both directions (`alias_caller.prg` → `alias_closer.prg`, `alias_reverse_caller.prg` → `alias_opener_callee.prg`).

Parity goldens (`scripts/goldens/20-unclosed-resources.js`) pull `textDocument/diagnostic` against `server/vertex-adt-ls-20.exe`. They pin unclosed `dbopen` Warning, matched close silence, `ALREADY_OPEN` silence, leftover on `# include`, `;!IGNORE CLOSE`, callee close with/without `common_dbases 1`, `dbclose_by_name` basename, section include, fragment own-open, and the setting gate. The IGNORE CLOSE QuickFix is pinned separately. Include-upward relatedInformation is implemented from index `includeLinks` when those entries exist.

IGNORE CLOSE QuickFix goldens (`scripts/goldens/43-ignore-close-code-action.js`) send `textDocument/codeAction` to `server/vertex-adt-ls.exe` and to the live Node language server with the same request diagnostics, and require the same actions. On `unclosed_dbopen.prg` and `include_opener.prg` (and, native-only, a `dbclose` of an alias) they apply the one QuickFix (`Ignore missing close (;!IGNORE CLOSE)`), check the line now ends ` ;!IGNORE CLOSE`, and revalidate to no unclosed-resource diagnostic. Synthetic rows cover ` IGNORE CLOSE` appended to an existing `;!`, lines that already suppress (including lowercase and NBSP between the words), `; !` not counting as a bang comment, quoted `;!`, trailing space, CRLF, UTF-16 columns, `data.line` vs range fallback, two leftovers on one line, other codes, and `only` filters (`source.fixAll` gets none).

## Manual checks

1. Open `custom/unclosed_dbopen.prg` → yellow on `dbopen`.
2. Open `custom/closed_dbopen.prg` → no unclosed-resource diagnostic.
3. Open `custom/already_open.prg` → no unclosed-resource diagnostic. Add `dbclose(1)` under it → yellow on `dbclose` (`… aliased with ALREADY_OPEN (line 1) …`).
4. Open `custom/include_opener.prg` → yellow on the `# include` line.
5. Add `;!IGNORE CLOSE` to a warned line → diagnostic disappears.
6. On a warned line, open the lightbulb → **Ignore missing close (;!IGNORE CLOSE)** appends the marker and the warning disappears.
7. Open `custom/chain_timer.prg` → yellow on `dbopen` (line 4) and `dbcreate` (line 7), each linking only to the `#BAD#` call line, and yellow on that call line (line 27). Add `dbclose(h)` under it → all three clear.
8. Open `custom/break_open.prg` → yellow on `dbopen`: `still open at break (line 3)`.
9. Open `custom/cross_opener.prg` and `custom/cross_caller.prg` → both warn. Add `dbclose(d)` to the caller → the warning in `cross_opener.prg` clears too.
10. Open `custom/alias_caller.prg` and `custom/alias_closer.prg` → yellow on `dbclose(1)` in `alias_closer.prg` (`… (alias_caller.prg line 2) …`), linking to the alias line. `alias_caller.prg` itself is clean.
