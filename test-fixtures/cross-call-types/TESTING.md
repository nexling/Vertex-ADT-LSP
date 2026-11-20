# Cross-call types (native only)

See `docs/adr/0009-native-cross-call-types.md`.

## Automated

- Golden: `scripts/goldens/55-cross-call-types.js` (part of `npm run test:goldens`).

## Manual check in the extension

Open this folder as the workspace, turn on `vertexadt.strictMode.enabled`, and open the files below.

| File | Expect |
|---|---|
| `callee_db.prg` line 2 | red on the comment: `Comment types studs as profile but callers pass database ($CALLPARAM1)` |
| `callee_db.prg` line 4 | yellow on `dir`: callers pass `dvector, value` |
| `callee_db.prg` line 5 | yellow on `mixed`: callers pass `database, profile` |
| `callee_db.prg` line 3 | `pce` is coloured as a piece (both callers pass a profile) |
| `caller_a.prg` line 4 | red on `db`: the callee expects `profile` |
| `caller_a.prg` line 6 | red on the comment: the callee returns `database` for `DB`. `back` is coloured as a database |
| `caller_b.prg` line 2 | red on `5`: the callee expects `pvector`. Line 3 (`IGNORE TYPE`) passes nothing |
| `chain_leaf.prg` line 1 | red on the comment: the database from `chain_top.prg` arrives through `chain_mid.prg` |
| `self_sections.prg` | red on `pc` (line 2) and on the `#WORKER#` comment (line 5) |
| `return_conflict.prg` lines 3–4 | yellow on `OBJ`: packed as `database, profile` |
| `frag_part.prg` line 2 | red on the comment: callers of `frag_parent.prg` pass a database |
| `cycle_a.prg`, `cycle_b.prg` | nothing (the cycle has no typed origin) |

Then set `vertexadt.strictMode.crossCallTypeChecks` to `false`: the red and yellow marks go away, and the colours stay.
