# Used-before-declaration check

Parity goldens for family 2f: used-before-declaration and 16-character keyword length.

Declared means the name is in this document’s live-buffer extract or the include-neighborhood extract, not source-order and not the index. Isolation pins a per-URI extract rebuild. Include-neighborhood names count as declared (Node merge). The Node slice-tests in this folder stay Node regression only; they use a simplified `call(arg)` split and do not pin native call-arg semantics.

Used-before-assignment (native only, [ADR 0009](../../docs/adr/0009-native-used-before-assignment.md)): a declared name whose assignments all come after the use, in another section, or (on the same line) are not the target gets `Variable X used before assignment (…)`. The golden also covers the `vertexadt.variables.warnUsedBeforeAssignment` toggle.

## Smoke

```bash
node scripts/goldens/16-used-before.js
```

## Files

- `custom/used_before_tab_a.prg` / `custom/used_before_tab_b.prg` — this-document extract isolation
- `custom/used_before_neighborhood.prg` + `custom/macros/used_before_inc.prg` — include-neighborhood names are declared
- Other cases are opened as unsaved `vertexadt` buffers in the golden

Workspace root is this folder so Custom overlay resolves `macros/used_before_inc.prg`.
