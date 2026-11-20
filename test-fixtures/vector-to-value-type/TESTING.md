# Vector-to-value type transitions

Parity goldens for family 1 of full-document validate: the this-pass type timeline compare. Node slice-test `run-type-transition-test.js` stays Node regression only.

Workspace root is this folder so Custom overlay resolves `macros/13-inc.prg`.

## Smoke

```bash
node scripts/goldens/13-type-timeline.js
```

## Compare (spec table)

| File | Warnings |
|---|---|
| `custom/13-pvector-then-value.prg` | one, on the `abs` value line |
| `custom/13-vector-only.prg` | none |
| `custom/13-value-then-vector.prg` | none |
| `custom/13-vector-vector-value.prg` | one; first vector is `pvector` in section `MACROS` |
| `custom/13-value-vector-value.prg` | one (later value still warns) |
| `custom/13-array-then-value.prg` | none |
| `custom/13-different-sections.prg` | none |
| `custom/13-same-name-both-sections.prg` | two, independent sections |

## Envelope

Wrapped Warning `ADT-Syntax`: `{lineText} has an error: Variable {varName} was previously typed as {vectorType}{sectionSuffix} and is now typed as value.` Related: `First typed as {vectorType} here`. No `code`. Range starts at character 1.

## Collection / gate

- `custom/13-ignore-type.prg` — `IGNORE TYPE` skips the vector write; no warning
- `custom/13-comment-type.prg` — `[pvector]` comment fallback then `value` warns
- `custom/13-edit-form.prg` — `#EDIT_FORM#` bodies skipped at compare
- `custom/13-include-main.prg` — one-level `##included_from macros/13-inc.prg` types on the directive line
- `vertexadt.variables.warnVectorToValueTransition` false skips the compare, not collection
