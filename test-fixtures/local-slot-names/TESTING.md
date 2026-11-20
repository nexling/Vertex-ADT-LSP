# Local Slot Names — Manual Test Guide

Open `test-fixtures/local-slot-names` as the VS Code workspace root.

Enable strict mode: **Vertex ADT → Strict Mode → Enabled** (`vertexadt.strictMode.enabled`).

## Scenario 1: Workspace-wide l1 collision

1. Open `custom/callee_a.prg` and `custom/callee_b.prg` (both are call targets).
2. Both use the `l1` prefix (`l1Area` / `l1Elem` vs `l1Other`).
3. **Expected:** Blue Information diagnostic on both files (`vertexadt/local-slot-name-collision`).

## Scenario 2: Unique l2 prefix is OK

1. Open `custom/callee_c.prg` (`l2Only`).
2. **Expected:** No prefix collision with `l1` used by other callees.

## Scenario 3: Redeclaration warning

1. Open `custom/redeclare.prg`.
2. **Expected:** Yellow warning on `A = 50` (`vertexadt/declaration-redeclared`).

## Scenario 4: Callee naming convention

1. Open `custom/callee_bad_names.prg` (called from `caller_bad.prg`).
2. **Expected:** Yellow warning on `foo` suggesting `l19foo` (next slot after highest used called prefix; gaps like uncalled `l17` are kept).

## Scenario 5: @local_slots manifest

1. Open `custom/callee_manifest_ok.prg` — no manifest errors.
2. Open `custom/callee_manifest_bad.prg` — errors for missing `l2` in declarations and `l3` missing from manifest.

## Scenario 6: Assign local slot names command

1. Open `custom/callee_bad_names.prg` (note `foo` is also used on the `set(foo, 1)` line).
2. Run **Vertex ADT: Assign Local Slot Names** or use the quick fix.
3. **Expected:** every `foo` in the file becomes `l19foo` when called programs already use up to `l18` (uncalled `l17` programs do not count).

## Scenario 7: Export workspace CSV

1. Run **Vertex ADT: Export Workspace Reports**.
2. **Expected:** `vertexadt-local-slots.csv` appears with callee local slot rows.

## Automated smoke test

```bash
node test-fixtures/local-slot-names/run-local-slot-test.js
```
