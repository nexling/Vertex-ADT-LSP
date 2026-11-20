# Drawing, model and file command entries

Eighteen commands had keyword docs but no `commands.json` entry, so nothing validated them: `dwgopen_bg`, `dwgactivate`, `dwgempty`, `group_updating`, `groupf`, `groupv`, `dwgsetscale`, `linewidth`, `text_type`, `dwgredraw`, `dwgupdatetext`, `dwgsetlimits`, `dwgstore`, `dwgclose`, `modelopen`, `fileremove`, `write`, `modelstore`. They now have entries. Each shape was set from how C:\vxbd320 uses the command. The counts are now 276 commands and 271 table validators (generator and `main.cpp`).

## Entries

| Command | Form | Parameters |
|---|---|---|
| `dwgopen_bg` | `[x =] dwgopen_bg(...)` | window string, file string, 4 numbers |
| `dwgactivate` | `[x =] dwgactivate("W")` or `[x =] dwgactivate "W"` | window string |
| `dwgempty`, `dwgredraw`, `dwgupdatetext` | statement | none |
| `group_updating`, `dwgsetscale`, `linewidth` | statement | 1 number |
| `text_type` | statement | 0–1 number (`text_type` alone occurs in joint_custom) |
| `dwgsetlimits` | statement | 0–4 numbers |
| `dwgclose` | statement | window string |
| `groupf` | `groupf(n) File$` | index; 1 string after |
| `groupv` | `groupv(n) File$[, name, value]...` | index; string, then up to 98 strings or variables |
| `dwgstore` | `[x =] dwgstore(File$[,Mode[,Setup$]])` | string, number, string |
| `modelstore` | `[x =] modelstore(File$[,Mode[,n]])` | string, number, number |
| `modelopen` | `modelopen W$[,] model=File$` | 1–2 strings (`model=File$` passes as a string expression) |
| `fileremove` | `fileremove P$` or `fileremove(P$)` | path string |
| `write` | `write(n) text` | file number; 1 string or variable after |

Not covered: `groupf File$` / `groupv File$,...` without an index. These are `hasParentheses` commands, so a statement without `(` is not validated.

## `structure.parenthesesOptional` (new)

`dwgactivate` appears as `Ok=dwgactivate("x")`, `dwgactivate "x"` and `Ok=dwgactivate "x"`. `fileremove` appears both with and without parentheses. Before this change, the catalog could describe only one of those forms (ADR 0016's `dbdelete` gap). A `hasParentheses` command with `parenthesesOptional: true` now also accepts the statement form. Its parameters are validated as the inside parameters. Strict-mode columns are not computed for the statement form.

## Command-syntax changes that affect every command

The corpus gate surfaced these. Each change only removes diagnostics.

- **`;` separates statements.** A no-parentheses command is validated on its own `;` statement, not the whole line. Before, `linewidth W; level L` passed `W; level L` as one parameter, and in `sort 3; linewidth 0.15` the parser took `sort` as the function name.
- **`; !` starts a comment.** A `;` statement that begins with `!` comments out the rest of the line. `;!` already did that.
- **Lines continued with `\` are skipped.** Command syntax is not checked on a line that still ends in `\` after the dbcreate and if-header joining (ADR 0014). That line holds only part of the statement, and the `\` was counted as a parameter.
- **A name assigned on the line is a variable.** If a line starts with `name =` or `name(i) =`, `name` is not treated as a command on that line (`LineWidth(1) = ...`, `Exit=0`).
- **`.25` and `-.5` are numbers.** `matchNumeric` no longer requires a digit before the decimal point (`linewidth .25` in win_custom).

## Tests

- `server/tests/drawing_file_commands_test.cpp` (target `drawing-file-commands-test`) covers the requested block, the corpus forms, the `;`, `; !`, `\` and assigned-name rules, and the error message for each invalid shape.
- The findstr, snippet-catalog and line-continuation unit tests pass, and so do all goldens.

## Corpus gate

The gate opened 2,487 C:\vxbd320 files: every `.prg/.adt/.p/.vxt/.3d/.2d` file, plus extensionless files under `sysprogs/` and `*_custom/`. The baseline was a build of the working tree from just before this change. The baseline and new runs were done one after the other, because parallel runs gave different used-before-declaration results.

- **168 removed**, all false positives:
  - 110 `tan requires an assignment variable` on `\`-continued lines;
  - 44 `vec_copy` with a `; ! comment`;
  - 12 `set` lines with several `;` statements;
  - 2 `Exit=0`.
- **6 added**, all `write`:
  - 2 real bugs: `AUTODRWN.prg:290` is missing an `&` before `getdate()`.
  - 4 come from the shared string-expression check (`pdfcontrol2mark`, `&sheet(Ind1)&`). In `&` concatenation that check accepts a plain numeric name but not an indexed one. Left as is, because the check is shared by every command.
