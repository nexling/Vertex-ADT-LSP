# Typed iterator items

`next_item` takes its type from the iterator's `new_iterator` / `init_iterator` (`docs/adr/0030-typed-iterator-items.md`). A CONNECTIONS iterator with a first value other than `""` is an error. An unknown first value is information.

## Smoke

```bash
node scripts/goldens/66-typed-iterators.js
```

## `custom/typed_iterators.prg`

Item types are read from semantic tokens on the `next_item` assignment target.

| Iterator | Item | Expect |
|---|---|---|
| each of the 17 first values in `server/catalog/iterator-object-types.json` with a non-CONNECTIONS task | `Prof` … `Flange` | that type |
| `""` with `CONNECTIONS` (inside a loop) | `Joint` | joint, no diagnostic |
| `"PROFOBJ"` with `CONNECTIONS_HC` | `JointHc` | joint; error `CONNECTIONS_HC iterators return joints; first value "PROFOBJ" returns profile` |
| `"FOO"` with `CONNECTIONS` | `JointBad` | joint; error `CONNECTIONS iterators return joints; first value must be "", not "FOO"` |
| `Kind$` with `CONNECTIONS` | `JointKind` | joint, no diagnostic |
| `init_iterator(ItRe)` `"PROFOBJ"`, then `"WALLOBJ"` | `Re1`, `Re2` | profile, then wall |
| `"MACROOBJ"`, `"MACOBJ"` | `Typo` | untyped; information `... will return all objects (did you mean "MACROBJ"?)` |
| `"profobj"` | `Case1` | untyped; information `... (did you mean "PROFOBJ"?)` (case-sensitive) |
| `"PIECE"` | `Piece` | untyped; information without a hint |
| `""` with `PARTS` | `All` | untyped, no diagnostic |
| `Kind$` with `PARTS` | `Var1` | untyped, no diagnostic |
| `"PROFOBJ"` with `"connections"` | `Low` | profile (the lowercase task is not CONNECTIONS), no diagnostic |
| `init_iterator(ItInit) "",Obj,"CONNECTIONS",""` with a `;!` comment holding `"MACROOBJ"` | `JointInit` | joint; the comment is ignored |
| `init_iterator(ItInitBad) "AREAOBJ",Obj,"CONNECTIONS",""` | | error `... first value "AREAOBJ" returns area` |

Diagnostics underline the first-argument literal, quotes included, with source `ADT-Syntax`.
