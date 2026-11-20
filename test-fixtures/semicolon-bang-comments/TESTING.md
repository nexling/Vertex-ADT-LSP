# String-aware `;!` comments

## Smoke test

```bash
node test-fixtures/semicolon-bang-comments/run-semicolon-bang-test.js
```

## Manual check in the extension

1. Open `test-fixtures/semicolon-bang-comments/findstri_in_string.prg`.
2. **Line 1** (`findstri(Line$,";!")`) — no `Mismatching parentheses for 'findstri'` error.
3. **Line 2** — trailing `;! comment` is ignored; `findstri` still validates normally.
4. **Line 3**: the real `;!` comment is stripped, and the unclosed `findstri(` gets `Mismatching parentheses for 'findstri'`. The underline runs from the `(` to the last code character before the `;!` (characters 14–25), not the whole line. See `docs/adr/0006-native-bracket-balance-check.md`.

Also verify declaration lines with `;!"[value]..."` style comments still parse correctly outside strings.
