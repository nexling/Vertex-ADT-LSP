# Workspace Declaration-Number Index — Manual Test Guide

Open `test-fixtures/workspace-declaration-index` as the VS Code workspace root (the folder that contains `custom/` and `system/`).

Enable strict mode: **Vertex ADT → Strict Mode → Enabled** (`vertexadt.strictMode.enabled`).

## Scenario 1: Include chain conflict (blue squiggle)

1. Open `custom/main_include.prg` and `custom/sub_included.prg`.
2. Both declare number `10` under different names (`VarA` / `VarB`).
3. **Expected:** Blue Information diagnostic on both files (cross-file conflict).
4. Within-file duplicate in a single file should still be **yellow** Warning.

## Scenario 2: Call without common_* (no cross-file conflict)

1. Open `custom/caller_no_common.prg` and `custom/callee_no_common.prg`.
2. Both use declaration number `5`.
3. **Expected:** No blue cross-file diagnostic (call namespace not shared).

## Scenario 3: Call with common_indexes 1 (cross-file conflict)

1. Open `custom/caller_with_common.prg` and `custom/callee_with_common.prg`.
2. Both use declaration number `20`.
3. **Expected:** Blue Information diagnostic on both files.

## Scenario 4: Custom wins over system

1. Open `custom/custom_override.prg` only.
2. The `system/custom_override.prg` copy declares `SystemOnly = 99` but must be ignored.
3. Rebuild index by reloading the window; no conflict should reference the system file.

## Scenario 5: strictMode off

1. Disable `vertexadt.strictMode.enabled`.
2. Re-open scenario 1 files.
3. **Expected:** No blue cross-file diagnostics. Per-file yellow warnings inside a single file still work.

## Scenario 6: Related information navigation

1. With strict mode on, open scenario 1.
2. Hover the blue squiggle on `VarA = 10`.
3. **Expected:** Related information links to `VarB = 10` in `sub_included.prg`.

## Scenario 7: Shared callee hub isolation

1. Open `custom/caller_a.prg`, `custom/caller_b.prg`, and `custom/shared_callee.prg`.
2. Both callers use `common_indexes 1` and call the same callee; each pair uses declaration number `10` with different variable names.
3. **Expected:** Conflicts only within each caller+callee pair. `caller_a` and `caller_b` must **not** conflict with each other.
4. Opening `shared_callee.prg` shows separate scoped diagnostics per caller (`[scope: caller_a.prg]`, etc.).

## Scenario 8: Shared include hub isolation

Same as scenario 7 using `include_caller_a.prg`, `include_caller_b.prg`, and `shared_include.prg`.

## Scenario 9: Value-typed declarations ignored

1. Open `custom/caller_value_skip.prg` and `custom/shared_callee_value.prg`.
2. Both use declaration number `30` for `ObjectSlot`/`CalleeObject` (non-value) and `ValueSlot` (value-typed via `;!"[value]..."`).
3. **Expected:** Blue Information diagnostic only for `ObjectSlot` vs `CalleeObject`. No conflict involving `ValueSlot`.

## Scenario 10: Export workspace CSV reports

1. Open `test-fixtures/workspace-declaration-index` as the workspace root.
2. Run **Vertex ADT: Export Workspace Reports** (Command Palette) or **Tasks: Run Task** → *Vertex ADT: Export Workspace Reports*.
3. **Expected:** Two CSV files appear in the workspace root and open in the editor:
   - `vertexadt-program-usage.csv` — `program,call_count,include_count`
   - `vertexadt-declaration-numbers.csv` — `variable_name,declaration_number,count`
4. Counts are global across the workspace (not scoped by call/include groups). Custom files win over system duplicates.

## Scenario 11: Section-scoped in-file declaration numbers

1. Open `custom/section_macros.prg`.
2. `var1 = 1` in `#MACROS#` and `varP = 1` in `#MACROS2#`.
3. **Expected:** No yellow warning (same number allowed in different `#SECTION#` bodies).

4. Open `custom/section_dup.prg`.
5. `var1 = 1` and `var2 = 1` both inside `#MACROS#`.
6. **Expected:** Yellow warning on the duplicate number within the same section.

## Scenario 12: Section-scoped cross-file via include

1. Open `custom/section_include_caller.prg` and `custom/section_include_callee.prg`.
2. Caller `#JOINT#` includes callee `MACROS` section; both use declaration number `10`.
3. **Expected:** Blue cross-file diagnostic (strict mode) for `CallerVar` vs `CalleeVar` in the `JOINT` scope.
4. `CallerOther = 10` in `#OTHER#` must **not** conflict with `CalleeVar` in callee `#MACROS#`.

## Scenario 13: Declaration numbers across multiple blocks in one file

1. Open `custom/section_multi_block_dup.prg`.
2. First `#SECTION_A#` block declares `D = 5`; a later `#SECTION_A#` block declares `Other = 5`.
3. **Expected:** Yellow warning on `Other = 5` (duplicate 5 within `SECTION_A`, across blocks).
4. `#SECTION_B#` block declares `PP = 5` then `D = 5`.
5. **Expected:** Yellow warning on `D = 5` (conflicts with `PP`), even though `D` was already used in `SECTION_A`.
6. `PP = 22` in `SECTION_A` and `PP = 5` in `SECTION_B` must **not** conflict (different sections).

## Scenario 14: Callee-driven `@require_common_*` on call()

1. Open `custom/callee_with_common.prg` — it contains `@require_common_indexes`.
2. Open `custom/caller_missing_common.prg` (calls that callee without `common_indexes 1`).
3. **Expected:** Error on the `call()` line requiring `common_indexes 1` in the caller.
4. Open `custom/caller_with_common.prg` (has `common_indexes 1`).
5. **Expected:** No error from this rule.
6. Open `custom/caller_no_common.prg` calling `callee_no_common.prg` (no callee marker).
7. **Expected:** No error from this rule; Scenario 2 cross-file behavior unchanged.
8. Section-scoped: a no-suffix `call("foo.prg")` from `#JOINT_TEST#` is a whole-file invocation — **every** callee `@require_common_*` applies (not an inherited `JOINT_TEST`). `call("foo.prg", JOINT_TEST)` uses file-level markers plus `#JOINT_TEST#` only. Caller `common_* 1` must be in the same section as the `call()`; file-level or `#JOINT_DETAILING#` does **not** satisfy a `JOINT_TEST` call, and a section directive does **not** satisfy a file-level call.

## Scenario 15: Aligned declaration slots across call chain

1. Open `custom/caller_same_slot.prg` and `custom/callee_same_slot.prg`.
2. Both declare `extravar = 105` with `common_indexes 1` linking the call namespace.
3. **Expected:** No blue cross-file diagnostic (same name and same number is intentional).
4. Contrast with Scenario 3 (`SharedA` / `SharedB` both `20`) — those must still conflict.

## Scenario 16: Cross-tab diagnostic isolation

1. Open two unrelated `.prg` files in separate tabs (e.g. one with `call()` argument checks, one without overlapping declarations).
2. Switch between tabs repeatedly.
3. **Expected:** Diagnostics on the inactive tab stay stable — no new spurious errors when another tab becomes active.
4. Edit the active file that uses `call()` linking — connected files may refresh, but unrelated open tabs must not gain false positives.

## Scenario 17: Impact and Blast Radius CodeLens

1. Ensure **Editor: Code Lens** is enabled (`editor.codeLens`).
2. Enable `vertexadt.impactAnalysis.enabled` (default on) and `vertexadt.impactAnalysis.minIncomingCount` = 2.
3. Open `custom/shared_callee.prg`.
4. **Expected:** CodeLens above line 1, e.g. `Impact: 2 calls, 0 includes | Blast radius: N files in 2 program(s)`.
5. Click the CodeLens → Quick Pick lists `caller_a.prg` and `caller_b.prg` at their `call()` lines.
6. Pick a blast-radius scope → second Quick Pick lists files in that program (e.g. `caller_a.prg`, `shared_callee.prg`).
7. Open `custom/sub_included.prg` (single include) → no CodeLens at default threshold.
8. Set `minIncomingCount` to 1 → CodeLens appears on `sub_included.prg`.

## Scenario 18: Export workspace diagnostics CSV

1. Open `test-fixtures/workspace-declaration-index` as the workspace root.
2. Enable `vertexadt.strictMode.enabled` (required for cross-file rows in this fixture).
3. Run **Vertex ADT: Export Workspace Diagnostics** (Command Palette) or **Tasks: Run Task** → *Vertex ADT: Export Workspace Diagnostics*.
4. **Expected:** `vertexadt-workspace-diagnostics.csv` appears in the workspace root and opens in the editor.
5. **Expected columns:** `program,line,column,severity,code,source,message,related_program`
6. Filter `severity` = `information` and confirm rows for Scenario 1 (`main_include.prg` / `sub_included.prg` cross-file conflicts) with `related_program` pointing at the other file.
7. Disable strict mode and re-run — cross-file `information` rows from Scenario 1 should disappear; per-file warnings inside a single file may remain.
8. Custom files win over system: no rows should reference `system/custom_override.prg` when a custom copy exists.

9. With `vertexadt.diagnostics.exportCustomOnly` enabled (default), only files under the custom folder are validated — system-only files are skipped. Disable the setting to include system-only files in the export.

## Automated smoke test

From the repository root:

```bash
node test-fixtures/workspace-declaration-index/run-index-test.js
node test-fixtures/workspace-declaration-index/run-decl-dup-test.js
node test-fixtures/workspace-declaration-index/run-call-common-test.js
node test-fixtures/workspace-declaration-index/run-impact-analysis-test.js
node test-fixtures/validation-isolation/run-tab-isolation-test.js
node test-fixtures/validation-isolation/run-refresh-heuristic-test.js
```
