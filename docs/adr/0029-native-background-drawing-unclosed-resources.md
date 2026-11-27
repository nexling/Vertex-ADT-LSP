# Background drawings in the unclosed-resource check (native only)

The path-aware unclosed-resource check (ADR 0012) already followed databases (`dbopen`/`dbcreate` → `dbclose`) and files (`open(n)` → `close(n)`). It now also follows background drawings: `dwgopen_bg(Port$, …)` opens a drawing that `dwgclose Port$` must close. A background drawing left open is invisible to the user and stays open after the program ends. The Node language server is frozen, so this is native only. The code is in `server/src/validate_unclosed_resources.cpp`.

## Rules

- **What is tracked.** Only `dwgopen_bg` opens and only `dwgclose` closes. A foreground `dwgopen` window may stay open for the user on purpose. `modelopen`, `dwgopen_old` and `fileopen` are not tracked. A `dwgclose` with no matching open stays silent, as for every kind.
- **The handle is the port expression.** For `dwgopen_bg` it is the first argument inside the parentheses. For `dwgclose` it is the rest of its `;` statement, with or without parentheses (`dwgclose "p"`, `dwgclose("p")`, `dwgclose "p"; x = 1`). Two ports match when their text is equal once spaces outside quotes are removed and case is ignored: `"PlanLay"` = `"planlay"`, `Win$` = `win$`, `"tmp"&Nr$` = `"tmp" & nr$`. Values are not tracked, so a literal never matches a variable that holds it.
- **Scope.** Ports are global for the session, so drawings have one namespace shared by every frame, with no `common_*` flag. A literal port is the same drawing in every frame. A root section that has callers hands an open literal port to them, the way a `$CALLPARAM` handle goes up (ADR 0012), so `#OPEN#` may open `"planlay"` and its caller close it. A root with no caller must close it itself. A variable port belongs to its frame unless it is bound through `x = get($CALLPARAMn)`. `call` arguments that are `$` names or string literals now bind, as plain names and numbers already did.
- **Failed opens.** After `ok = dwgopen_bg(port, …)`, the `if !ok` / `if ok = 0` branch treats `port` as not open, the same as the `dbvalid` guard.
- **Reopening** an open port is silent, and one `dwgclose` closes it.
- **Messages** read `Drawing "planlay" is opened here and …`, with the same endings as databases and files (`not closed in this execution tree`, `still open at exit (line N)`, `not closed on every caller path`, call-line and include-line warnings). The same setting (`vertexadt.diagnostics.warnUnclosedResources`), `;!IGNORE CLOSE` and quick fix apply. ALREADY_OPEN alias checks stay database only.

## Corpus gate (C:\vxbd320, 2026-11-27)

Every `.prg` file in customlbab and system, plus the 38 other files that mention `dwgopen_bg` or `dwgclose` (customukraine, customvbc, extensionless sysprogs and triggers, `.vxt`): 480 files. Baseline was HEAD (80b6c61), run before the new build, one after the other.

- 38 → 39 unclosed-resource diagnostics. None removed, so the new `call`-argument binding did not change a database or file result.
- The one added warning is a real leak. In `customukraine/complibs/commoninp_lindb/PNL_LABELnWNAME.vxt:235`, `ok = dwgopen_bg("tmp",…)` is still open at six `exit` error paths (lines 264, 272, 284, 328, 336, 348). In the customlbab copy, the open is commented out.
- Every other `dwgopen_bg` in the corpus (`DWGCOPY.prg`, `VXPCOPY.prg`, `previewtester.prg`, …) is closed on every path.

Slowest file 308 ms (`walldetailing.prg`), against 290 ms before.

## Tests

- `scripts/goldens/20-unclosed-resources.js` `runDrawingGoldens`: close and no close; case, space and composite matching; parentheses and `;`; literal against variable; untracked commands; `!Ok` and `Ok = 0` guards; exit; a close on one branch only; reopening; IGNORE CLOSE; and a cross-section chain.
- Fixture `test-fixtures/unclosed-resources/custom/drawing_chain.prg`: a literal port closed by one caller and not by another, plus a `$CALLPARAM` port bound to a variable and to a literal.
