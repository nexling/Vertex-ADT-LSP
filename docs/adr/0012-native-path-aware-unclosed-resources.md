# Path-aware unclosed-resource check (native only)

The unclosed-resource check used to read a file as one flat stream: `# include` inlined, `call` entered, and an open was fine if any matching close came later. That missed leaks on `exit`/`break` paths and on `if` branches. It also gave false warnings for a section that opens a database for its caller. In `addtimerforactpanel.prg`, `#OPENORCREATEDB#` opens `dbid = get($CALLPARAM2)` and every calling section closes it as `l45timedb`. The old check never connected the two names, because it did not follow a `call` back into the same file. The native server now follows paths and hands the duty to close up the call chain. The Node language server is frozen (since 2026-11-18) and keeps the flat stream, so this is a deliberate native/Node divergence. The code is in `server/src/validate_unclosed_resources.cpp`.

## Rules

- **Each section is its own root.** Every named section is checked on its own, and so are the lines outside every section. A `dbclose` in another section no longer counts.
- **Paths, not text order.** `if`/`else`/`endif` (block and inline `if … then stmt`) are followed separately and joined: a handle closed on one branch only is still open after the `endif`. A `loop` body may run zero or more times. `exitif` leaves the loop; outside a loop it leaves the program level like `exit`. Conditions are not evaluated, with one exception: after `ok = dbvalid(h)`, the branch of `if !ok`, `if ok = 0` or `if !dbvalid(h)` treats `h` as not open (and the `else` of `if ok` does too).
- **Exits.** `break` stops the whole program, so any handle open there leaks, whichever frame it belongs to. `exit` or running off the end of the root section leaks the root's own handles. `exit` in a called section leaks only that section's local handles. The rest go back to the caller.
- **Same handle = same argument position.** `x = get($CALLPARAMn)` in a called section is bound to the caller's n-th argument (a plain name or number). The callee's open and close of `x` are then the caller's handle. Different names holding the same number are not matched. Two uses of the same literal number in one namespace are the same handle, as before.
- **The duty goes up the chain.** A handle bound to the root's own `$CALLPARAM` that is still open at the end is checked at every caller of that section, found through the index's `callLinks` and the files' own `call` lines. A caller either closes it, fails, or passes it further up if the handle is also its own `$CALLPARAM`. A caller whose handle is a local name is the top of that chain and must close it. The duty only passes to a caller with `common_dbases 1` (`common_files 1` for files). Otherwise the called section must close it itself.
- **Where warnings go.**
  - The open line gets `… is opened here and not closed on every caller path`, with a related link to each failing chain's `call` line. With no callers at all, it gets `… is opened here for the caller, but no caller closes it`. A leak at `break`/`exit` reads `… and still open at break (line N)` or `… at exit (line N)` (with the file name when the stop is in another file), with links to every stop. Otherwise the old message stays.
  - The top `call` line of a failing chain gets `… opened by the called code is not closed on every path after this call` in its own file, with links to the opens.
  - Include leftovers and include roots work as before.
- **Suppression.** `;!IGNORE CLOSE` on an open, `call`, `# include`, `break` or `exit` line suppresses that line's part. The existing quick fix works on the new warnings. Setting: `vertexadt.diagnostics.warnUnclosedResources`.
- **Refresh.** A called section's warning depends on its callers. Editing a file that mentions `call` already drops the cached diagnostics of every open file that mentions `call` (which `$CALLPARAM` matches). So the called file is re-checked with no new wiring.
- **Limits.** A root walk stops after 400 000 statements, or at 48 nested frames. A truncated root reports nothing. Loops get at most 4 passes. Correlated flags (`closeme = 1` … `if closeme then dbclose(h)`) are not tracked, so that pattern is flagged.

## Corpus gate (C:\vxbd320, 884 files, 2026-11-19)

64 → 131 unclosed-resource diagnostics. No line that warned before is silent now. 25 baseline warnings changed wording; 92 new ones:

- 56 handles still open at an early `exit`/`exitif` (error paths such as `if !Hole then … exit` in `walldetailing.prg`, `AUTODRWN.prg`, `link_part.prg`), and 12 at a `break`. 7 of the breaks are the one at line 20 of `addtimerforactpanel.prg #OPENORCREATEDB#`, reached from `floordetailing.prg` and `DWGCOPY.prg` while they hold their own databases and files.
- 8 opens never closed on some branch (for example `sel_prod.prg` closes only inside `if NRow > 0`).
- The `addtimerforactpanel.prg` result asked for: lines 7 and 11 list only `#AFTERDRWN#` (line 123) as failing, plus a warning on line 123.
- The timer database `opentimedb.prg` hands on through `ACTTIMEDB` and `closetimedb.prg` closes in a later program. Its 2 opens now read `not closed on every caller path`, and its callers (`WALL_MF_P1.prg`, `CEIL_MF_P1.prg`, …) get call-line warnings (10 at the end of the section, plus some of the exit/break ones above). That is intended long-lived state: mark those lines `;!IGNORE CLOSE`. In `customc` the same file has no callers that resolve to it, so it reads `no caller closes it`.
- `marktimedb.prg`: `closeme` flag correlation (see limits).

Slowest file 271 ms (`walldetailing.prg`), against 266 ms before.

## Tests

- `scripts/goldens/20-unclosed-resources.js` `runPathAwareGoldens`: native-only rows with no Node oracle (call chain, branch close, break, IGNORE CLOSE on break, `dbvalid` guard, section scope, no caller, no `common_dbases`, loops, cross-file refresh).
- Fixtures in `test-fixtures/unclosed-resources/custom/`: `chain_timer.prg`, `branch_close.prg`, `break_open.prg`, `break_ignore.prg`, `dbvalid_guard.prg`, `section_scope.prg`, `no_caller.prg`, `chain_nocommons.prg`, `loops.prg`, `cross_opener.prg`, `cross_caller.prg`.
