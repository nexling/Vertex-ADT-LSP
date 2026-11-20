# Unused Variables — Manual Test Guide

Open `test-fixtures/unused-variables` as the VS Code workspace root.

Enable **Vertex ADT → Variables → Note Unused Variables** (`vertexadt.variables.noteUnusedVariables`).

Ensure **Editor: Code Lens** is not required; diagnostics use blue Information squiggles.

## Scenario 1: Local unused

1. Open `custom/local_unused.prg`.
2. **Expected:** Information diagnostic on `foo` and `bar` (`Variable … is declared but not used`).

## Scenario 2: Local used

1. Open `custom/local_used.prg`.
2. **Expected:** No unused diagnostic on `foo` (referenced in `bar = foo + 1`). `bar` is still unused.

## Scenario 3: Call argument used in callee

1. Open `custom/caller_passes_arg.prg` (ensure workspace index is warm — reload window if needed).
2. **Expected:** No unused diagnostic on `arg1` (used in `callee_uses_arg.prg`).

## Scenario 4: Call argument not used in callee

1. Open `custom/caller_unused_arg.prg`.
2. **Expected:** Unused diagnostic on `arg1`.

## Scenario 5: Include-linked usage

1. Open `custom/include_uses_var.prg`.
2. **Expected:** No unused diagnostic on `sharedVar` (referenced in `included_body.prg`).

## Scenario 6: Section-isolated declarations

1. Open `custom/section_isolated.prg`.
2. **Expected:** Unused diagnostic on `dup` inside `#MACROS2#` only; `dup` in `#MACROS#` is used via `used = dup + 1`. (`used` may also be flagged as unused if nothing else references it.)

## Scenario 7: Variable used only in if conditions

1. Open `custom/if_condition_use.prg`.
2. **Expected:** No unused diagnostic on `I` (`if I==0 then` inside a loop).

3. Open `custom/if_condition_spaced.prg`.
4. **Expected:** No unused diagnostic on `I` (`if I == 0 then`).

5. Open `custom/if_condition_multiline.prg`.
6. **Expected:** No unused diagnostic on `I` (multi-line `if` / `and I==0` / `then`).

7. Open `custom/if_condition_single_eq.prg`.
8. **Expected:** No unused diagnostic on `flatblock` (`if flatblock = 1 then` with single `=`).

## Automated smoke test

From the repository root:

```bash
node test-fixtures/unused-variables/run-unused-variables-test.js
```
