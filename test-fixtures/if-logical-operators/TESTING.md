# If Logical Operators — Manual Test Guide

Open `test-fixtures/if-logical-operators` files in the Vertex ADT extension.

Placement errors use **Error** severity from **ADT-Syntax**.

## Scenario 1: Same-line dangling `and`

1. Open `dangling_same_line.prg`.
2. **Expected:** Error on `if x = 5 and then` — logical operator directly before `then` without a right-hand condition.

## Scenario 2: Same-line dangling `or`

1. Open `dangling_or.prg`.
2. **Expected:** Error on `if A or then`.

## Scenario 3: Logical operator directly after `if`

1. Open `leading_operator_same_line.prg`.
2. **Expected:** Error on `if and var > 1000 then` — no condition before `and`.

## Scenario 4: Valid combined same-line condition

1. Open `valid_combined.prg`.
2. **Expected:** No placement error (`if A = 1 and B = 2 then` is valid).

## Scenario 5: Valid multi-line condition

1. Open `valid_multiline.prg`.
2. **Expected:** No placement error for `if` / `and B = 2` / `then` continuation.

## Scenario 6: Multi-line dangling continuation

1. Open `dangling_multiline.prg`.
2. **Expected:** Error on the `and then` continuation line (line 2).

## Scenario 7: Valid parenthesized and/or mix

1. Open `valid_parenthesized_mix.prg`.
2. **Expected:** No operator-precedence error on either long `if … then` line (and/or in separate parenthesized groups is valid).

## Scenario 8: Invalid root-level and/or mix

1. Open `invalid_root_mix.prg`.
2. **Expected:** Error — `and` and `or` at the same level without grouping (`if a and b or c then`).

## Scenario 9: Invalid inner and/or mix

1. Open `invalid_inner_mix.prg`.
2. **Expected:** Error — `and` and `or` inside the same parentheses (`if (a and b or c) then`).

## Scenario 10: Invalid parenthesized terms with root-level mix

1. Open `invalid_paren_terms_mix.prg`.
2. **Expected:** Error — `if (Geom) and (Area) or (Geom) then` is equivalent to an unparenthesized root mix.

## Automated smoke test

From the repository root:

```bash
node test-fixtures/if-logical-operators/run-if-logical-operators-test.js
```
