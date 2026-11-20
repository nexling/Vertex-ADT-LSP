# Inferred `##included_from`

Golden: `scripts/goldens/60-inferred-included-from.js` ([ADR 0015](../../docs/adr/0015-native-inferred-included-from.md)). The workspace root is this folder.

## Files

- `custom/macros/child.prg`: no directive. It is included by `progs/p1.prg` and `progs/p2.prg` (whole file) and by `progs/p3.prg` (`JOINT` only). `progs/p5.prg` calls it without common flags, so p5 is not a parent.
- `custom/macros/written.prg`: `##included_from progs/wp.prg`. `wp.prg` sets `W1` before its include and `W2` after it.
- `custom/macros/broken.prg`: `##included_from progs/nope.prg` (does not resolve).
- `custom/macros/capchild.prg`: included by `capped/cap01…cap11.prg`, which is 11 parents, over the cap.

## Expected

| Check | Result |
|---|---|
| child CodeLens | `Included from: progs/p1.prg +2`, parents p1, p2, p3 |
| child undeclared | `C` (p2 sets it after its include), `LATE` (set after both includes), `J2` (only p3; p1/p2 reach `JOINT` without it) |
| definition of `A` | two Locations, in p1.prg and p2.prg |
| hover `A` | `A from p1` (first parent in path order) |
| codeAction `only: ['source']` | three `Pin ##included_from progs/pN.prg` plus `Pin all 3 inferred parents as ##included_from`; without `only` none |
| written.prg | `W2` undeclared, no inferred lens |
| broken.prg | Warning `##included_from file not found: progs/nope.prg` at (0,16), code `vertexadt/included-from-not-found` |
| capchild.prg | lens `Included from: 11 parents, too many to infer, pin one`, `CAPVAR` undeclared |
| `inferIncludedFrom: false` | child: `A B C J1 J2 LATE` undeclared, no lens |
