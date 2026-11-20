# Self-reassignment types

A type write takes effect at its target token (`docs/adr/0019-native-write-target-token.md`). In `obj = get_panel_core(obj)` only the left `obj` is `area`; the right `obj` keeps the type it had before the line, and the next line sees `area`. An output slot that shares its name with an input on the same line works the same way.

## Smoke

```bash
node scripts/goldens/62-self-reassign-types.js
```

Strict mode is on for the whole golden.

## `custom/self_reassign.prg`

| Line | Expect |
|---|---|
| `obj = get_panel_core(obj)` | no strict error; left `obj` area, right `obj` selem |
| `use = get_panel_core(obj)` | `obj` area |
| `num = get_panel_core(num)` | strict error `input object 'num' ... but is 'value'`; left area, right value |
| `vec_add(dv) dv, pt` | output `dv` pvector, input `dv` dvector |
| `vec = vec_get_x(vec)` | no strict error; left value, right pvector; the vector-to-value warning stays |
| `fresh = get_panel_core(fresh)` | right `fresh` uncolored; used-before-assignment |
| `use = get_panel_core(other)` | strict error `... 'other' ... but is 'value'` (the write on the next line does not count) |

## Object-parameter completion

`P = new_piece(1)` then `P = get_obj_param(P, |`: the profile list (the right `P` is still a profile, not the line's `value`).
