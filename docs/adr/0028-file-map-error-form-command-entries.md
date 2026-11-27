# File, map, error and form command entries

Five commands had keyword docs but no `commands.json` entry, so nothing validated them. They now have entries. The counts are now 298 commands and 293 table validators (generator and `main.cpp`).

## Entries

| Command | Form | Strict-mode types |
|---|---|---|
| `file_exists` | `[x =] file_exists(path$)` | path `string` |
| `dircreate` | `dircreate path$` or `dircreate(path$)` | path `string` |
| `map_store` | `[x =] map_store(map$)` | map `string` |
| `show_error` | `show_error msg$[,hdr$[,time]]` | `string`, `string`, `value` |
| `edit_obj_data` | `[x =] edit_obj_data(obj,form$,mode)` | form `string`, mode `value` |

Decisions:

- The assignment is optional for `file_exists`, `map_store` and `edit_obj_data`. Most corpus uses sit in an `if` condition (`if edit_obj_data(...) then`, `if (len(P$) && file_exists(P$)) then`), and the validator only checks the statement after `then`, so those calls are not validated there either way.
- `show_error` takes exactly three shapes: a message, a message and a header, or a message, a header and a time. A number in slot 2 (`show_error "msg",5000`) is an error: the header must come before the time. Separating params with a space is accepted (`show_error "a" "b"`).
- `dircreate` accepts parentheses or none (2 corpus lines use them) and never an assignment.
- The `edit_obj_data` object is any variable, with no type restriction (`Params`, `This`, `proxyc`, `Mac`, `ScrewPattern`). Mode literals other than 0 and 1 are not flagged.
- Return types are unchanged: `file_exists` and `edit_obj_data` stay `boolean`, `map_store` stays `value`.

## Tests and corpus gate

`server/tests/file_map_error_form_commands_test.cpp` (target `file-map-error-form-commands-test`) covers the requested lines, the corpus forms, the invalid shapes and the strict types. The `note-map-system-commands-test` and `drawing-file-commands-test` unit tests and all goldens pass.

The gate opened the 765 non-`.dbg` C:\vxbd320 files that mention any of the five commands (any extension, all custom trees) with strict mode on, baseline first and then the new build.

- **0 removed.**
- **8 added**, all `Header parameter must be a valid string in show_error` on a message followed by a time, as decided above:
  - `show_error "...",5000` in `joint_custom/elcoupler_pre` (customlbab and customukraine).
  - `show_error "...",0` three times in customvbc `commoninp_brc/SHB_201403A_PRG`.
  - `show_error Prompt$,3000` in `system/macros/triggers/voljaribolts` and twice in `voljarihgt`.
