# Call Parameter Validation — Manual Test Guide

Enable **Vertex ADT → Calls → Validate Parameter Count** (`vertexadt.calls.validateParameterCount`, default on).

## Scenario 1: Missing parameters

1. Open `custom/caller_missing_one.prg` (ensure `custom/callee_three_params.prg` is in the workspace).
2. **Expected:** Error on the `call(...)` — expects 3 parameters but 2 were provided.
3. Lightbulb quick-fix: **Add 1 missing call parameter(s) as ""**.
4. **Expected result:** `ret$=call("callee_three_params.prg",Area,Selemobj,"")`

## Scenario 2: Valid call

1. Open `custom/caller_ok.prg`.
2. **Expected:** No call parameter count diagnostic.

## Scenario 3: Extra parameters

1. Open `custom/caller_extra_one.prg`.
2. **Expected:** Warning — expects 3 parameters but 4 were provided.

## Scenario 4: Workspace-wide fix

1. Add several under-parameterized `call()` sites (or use `caller_missing_one.prg` copies).
2. Run command palette: **Vertex ADT: Fix Missing Call Parameters**.
3. **Expected:** All missing slots filled with `""` across the workspace.

## Automated smoke test

From the repository root:

```bash
node test-fixtures/call-param-validation/run-call-param-validation-test.js
```
