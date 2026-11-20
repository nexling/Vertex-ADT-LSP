# Joint-section rules

Parity goldens for family 17: closed named sections `UPDATE_JOINT`, `JOINT_DETAILING`, `JOINT_ADAPT`, `JOINT_TEST`, and `EDIT_PROPERTIES`. All five ban `new_djoint`. `JOINT_TEST` also bans `new_macro`. The first three require a block if-then-exit failsafe (`set RETURN "0"` / `"1"` then a line that starts with `exit`, not compact `then exit`) before `new_macro`.

Wrapped `{lineText} has an error: {inner}` Warning, source `ADT-Syntax`, no `code`, on the command line.

`test-fixtures/section-aware-types/` is type lookup / hover, not this family.

## Smoke

From the repository root, after building `server/vertex-adt-ls-25.exe`:

```bash
node scripts/goldens/25-joint-section.js
```

## Files

- `table-rows.prg` — five table rows, other named section, text outside sections
- `failsafe.prg` — accept/reject matrix for the if-then-exit failsafe
- `skip-parse.prg` — bang / `;!` / `messagebox`, pairing, nested `JOINT_TEST` and `#EDIT_FORM#`
