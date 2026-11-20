# Verification ignored sections (`#EDIT_FORM#`)

## Smoke test

```bash
node test-fixtures/verification-ignored-sections/run-edit-form-ignore-test.js
```

## Manual check in the extension

1. Open `test-fixtures/verification-ignored-sections/custom/edit_form_ignored.prg`.
2. Confirm diagnostics appear on lines **outside** `#EDIT_FORM#` (dangling `and` / `or` before `then`).
3. Confirm **no** diagnostics inside the `#EDIT_FORM#` body (bad `if`, numeric variable name, duplicate declaration numbers, unused variable).
4. Run **Export Workspace Reports** (or open the file with strict mode) and verify `DUPA` / `DUPB` from the form section are not listed in declaration-number or local-slot CSV output.
