# Native snippet and catalog corrections (native only)

The `get_obj_param` completion snippet used the `set_obj_param` shape, `get_obj_param(${1:in_OBJ}) ${2:in_Param},${3:[in_Mode]},${4:[in_Obj2]}`. The real syntax is `out$ = get_obj_param(Obj,"FIELD"[,MODE[,Obj2]])`. An audit compared every keyword snippet with its command's catalog structure (parentheses, arguments inside and after them), for the 104 commands that have both. Usage in C:\vxbd320 settled each mismatch. It found two more broken snippets and two wrong catalog structures. The Node server is frozen (since 2026-11-18), so every change here is a native/Node divergence.

## Snippets

A snippet lists the required parameters only, quotes its string slots, and leaves the assignment (`out$ =`) to the user, as the other `get_*` snippets do.

| Keyword | Before | After |
|---|---|---|
| `get_obj_param` | `get_obj_param(${1:in_OBJ}) ${2:in_Param},${3:[in_Mode]},${4:[in_Obj2]}` | `get_obj_param(${1:in_Obj},"${2:FIELD}")` |
| `set_djmember` | `set_djmember(${1:in_Joint}) ${2:in_Object}` (the required role was missing) | `set_djmember(${1:in_Joint}) ${2:in_Object},"${3:ROLE}"` |
| `new_feature` | `new_feature(${1:in_Type},${2:in_FeatureCustomType)0}, ${3:in_FeatureHandler/0})` (the `)0}` typo broke the placeholders) | `new_feature("${1:TYPE}","${2:0}","${3:0}")` (`"0","0"` is the common corpus form) |

## Command structure

- **`dbdelete`** as a statement is written without parentheses. Its argument is either a path (`dbdelete "./"&rawDB` in customa) or a database-id variable (`dbdelete DBID1`, `dbdelete id2` in customb). Node's catalog said `hasParentheses: true`, so a statement never matched and `dbdelete` was never validated: even a bare `dbdelete` passed. It is now a no-parentheses command with one `string-or-variable` (any variable) parameter after the name, the slot `unset` uses.
  - A missing parameter reports `dbdelete requires at least 1 parameters after parentheses`, and a second one reports `dbdelete accepts at most 1 parameters after parentheses`. These are the generic structure messages, which fire before the table validator.
  - `dbdelete("./x")` reports `Invalid syntax for 'dbdelete'`.
  - The slot passes a bare number (`dbdelete 5`), as it does for `unset` and `exit`.
  - A first version used a string-expression slot. The corpus gate showed 106 false positives on customb's id-variable form.
- **Not covered: `Ok = dbdelete(Db$)`.** Vertex's own `system/sysprogs/dbdemo/dbdemo01.prg` uses this function form, with parentheses and an assignment. The catalog can describe only one form per command. Native reports `Command 'dbdelete' should not have an assignment variable` there both before and after this change.
- **`dbgetactrow`** is written with parentheses (`Row_id=dbgetactrow("PROREG")`). Node's catalog said `hasParentheses: false` while also listing one parameter inside. The no-parentheses parser then reported `Invalid syntax for 'dbgetactrow'` on every use. Now it has `hasParentheses: true`. The rest of the entry (optional assignment, one string inside, nothing after) was already right.

## Catalog

All of it is applied on top of the frozen Node harvest in `scripts/harvest-catalogs.js`. Hand edits to `server/catalog/*.json` would be lost on the next harvest.

- `NATIVE_KEYWORD_OVERRIDES` (ADR 0003) now also takes `insertText`. The harvest fails if an `insertText` override names a keyword that Node gives no snippet.
- `NATIVE_STRUCTURE_OVERRIDES` (ADR 0011) gains `dbdelete` (`hasParentheses: false`, inside 0, outside 1–1) and `dbgetactrow` (`hasParentheses: true`).
- `NATIVE_TABLE_VALIDATOR_OVERRIDES` is new. It replaces a harvested table validator wholesale and is used for `dbdelete`, whose slot moves from `inside` to `after` and becomes `string-or-variable`. The harvest fails if the command has no harvested table validator.

The command and table-validator counts (245/240) are unchanged.

## Node divergences

| Line | Node | Native |
|---|---|---|
| `dbgetactrow("PICREG")` / `r=dbgetactrow("PROREG")` | `Invalid syntax for 'dbgetactrow'` | no diagnostic |
| `dbdelete` (no argument) | no diagnostic | `dbdelete requires at least 1 parameters after parentheses` |
| `dbdelete "./"&Db$` / `dbdelete DBID1` | no diagnostic | no diagnostic |
| `get_obj_param` / `set_djmember` / `new_feature` completion | old snippets | snippets above |

## Out of scope

- The 141 commands without both a snippet and a structure entry could not be audited this way.
- `working folder for completions/allkeywords.txt` is a historical working file and is left as is.

## Tests

- `server/tests/snippet_catalog_corrections_test.cpp` (target `snippet-catalog-corrections-test`) checks:
  - the three snippets' `insertText`;
  - that each snippet, with its placeholders filled in, validates cleanly;
  - valid and invalid `dbdelete` and `dbgetactrow` lines, including the corpus shapes.
- Goldens: all pass unchanged.

## Corpus gate

The standard gate opens only `.prg`/`.vxt` files under the first custom tree. It never reaches the `dbgetactrow` callers (extensionless files in `system/sysprogs`) or customb. So the gate was run on every C:\vxbd320 source file that mentions `dbdelete` or `dbgetactrow`: 82 files across customa, customc, customb and system, excluding `user*` debug logs. The baseline was the build before this change. Result: 7 diagnostics removed (the `dbgetactrow` false positives in `system/sysprogs`), 0 added. The full `.prg`/`.vxt` gate on customa (497 files) showed no change.
