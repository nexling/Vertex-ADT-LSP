# Typed iterator items

`Item = next_item(It)` used to be untyped. Its type now comes from the call that set up the iterator: `It = new_iterator("TYPE", Obj, "TASK", "PARAM")` or `init_iterator(It) "TYPE", Obj, "TASK", "PARAM"`. The parsing lives in `server/src/iterator_types.cpp`. The type timeline (`validate_type_timeline.cpp`) uses it to type items, and `validate_iterator_types.cpp` reports diagnostics. The mapping is data in `server/catalog/iterator-object-types.json`, generated into `server/src/generated/iterator_object_types_data.cpp` by `npm run generate-catalogs`.

## Rules

- **Constructors.** `new_iterator` (with an assignment) and `init_iterator` record what kind of item the iterator returns. `init_iterator` replaces the kind from its line on. `new_ci_iterator` is not covered.
- **CONNECTIONS wins.** A TASK (3rd argument) literal of exactly `CONNECTIONS` or `CONNECTIONS_HC` makes the item `joint`, whatever the first value is, including a variable.
- **Otherwise the first value decides**, as an exact, case-sensitive literal:

  | First value | Item type |
  |---|---|
  | `PROFOBJ` | profile |
  | `AREAOBJ` | area |
  | `WALLOBJ` | wall |
  | `MACROBJ` | macro |
  | `SHEETOBJ` | sheet |
  | `PROFEND` | profileend |
  | `EDGEOBJ` | edge |
  | `SLOPEOBJ`, `TRUSSOBJ` | slope |
  | `SELEMOBJ` | selemobj |
  | `NOTCHOBJ`, `LNOTCHOBJ`, `ANOTCHOBJ`, `CUTFEATOBJ`, `DIMPLEOBJ`, `FEATURE`, `FLANGECUT` | macro (as `new_feature` / `new_notch_feature` return) |

  `""`, a variable, a concatenation, or any other literal leaves the item untyped.
- **Scope.** The item takes the latest iterator write in the same file and section on an earlier line, by the timeline's usual linear rule (branches are not merged). An iterator set up in a caller or an included file gives no type. The item write is an ordinary timeline write, so hover, completion, semantic tokens and strict-mode type checks all see it. A `[type]` comment or a cross-call type keeps its existing precedence.

## Diagnostics

Both are always on (no setting), fire for `new_iterator` and `init_iterator`, and underline the first-argument literal including its quotes.

- **Error** (`vertexadt/iterator-connections-type`): a CONNECTIONS / CONNECTIONS_HC iterator whose first value is a literal other than `""`. These iterators can only return joints. The message is `CONNECTIONS iterators return joints; first value "PROFOBJ" returns profile`, or `... first value must be "", not "FOO"` for an unknown value. The item is still typed `joint`.
- **Information** (`vertexadt/iterator-unknown-type`): a first-value literal that is neither `""` nor in the table: `Unknown iterator object type "X"; will return all objects`. The engine treats it as no filter. ` (did you mean "Y"?)` is added for the listed typos (`MACROOBJ`, `MACOBJ` → `MACROBJ`) and for a value that matches a known one apart from case (`profobj` → `PROFOBJ`).
- `""` with any other task is silent: that is the normal way to ask for everything.

## Corpus (C:\vxbd320, 2026-11-27)

Across all ADT files, about 18,900 `CONNECTIONS` and 750 `CONNECTIONS_HC` iterators use `""`. 14 use `"MACROBJ"` with `CONNECTIONS` and now get the error. `MACROOBJ` (77 uses) and `MACOBJ` (57) get the information with a hint.

Strict-mode gate over the `.prg` files, base build vs this change, three roots (root, customvbc, customukraine; 883 files):

- New: 4 CONNECTIONS errors, 76 unknown-type notes, and 4 cross-call parameter conflicts where callers now pass typed items.
- Gone: 28 strict errors on items that used to fall back to `value` (`Platesplicer.prg` `splicer`, `coloring_profiles.prg` `cmppce`).
- New strict errors: 5.
  - `TRUSSFROMPANEL.prg` passes a `TRUSSOBJ` item (slope) to `disconnect`, whose catalog list has no slope.
  - `TRUSSTOPANEL_pre.prg` passes an `AREAOBJ` item to `connect(Area)`, whose first argument must be a joint.
  - `areaksyms.prg` (2): its `FindEdgeProfilesOfPanel.prg` return fields are now typed `profile`, so its timeline is rebuilt instead of reusing the cross-call parameters-only pass. In the rebuilt timeline, `pce = rimprofiles(k)` gets no `[profile]` comment type and falls back to `value`. This difference between the two passes existed before this change.
