# Point, edge, note, map and system command entries

Seventeen commands had keyword docs but no `commands.json` entry, so nothing validated them. They now have entries. The counts are now 293 commands and 288 table validators (generator and `main.cpp`).

## Entries

| Command | Form | Strict-mode types |
|---|---|---|
| `get_pnt_param` | `get_pnt_param(P) name$,obj[,mode]` | obj: the same list as `set_pnt_param` |
| `set_piece_inst` | `set_piece_inst(pce) prop$,value$` | `profile` |
| `set_edge_shape` | `x = set_edge_shape(edge,geom,mode$[,angle])` | `edge`, `vxgeom`, angle `value` |
| `get_edge_dir` | `get_edge_dir(edge) P,Dir` | `edge` |
| `proj_vxgeom_dir` | `proj_vxgeom_dir(out) geom,P,N,Dir` | `vxgeom`, `pvector`, direction, direction |
| `notewinopen` | `notewinopen(n) header$,rows,cols` | |
| `notewinclose` | `notewinclose(n)` | |
| `note` | `note(n) message$[,number]` | |
| `dwgsetlevels` | `dwgsetlevels settings$` | |
| `function` | `function NAME` or `function "NAME"` | |
| `folder_files` | `x = folder_files(path$[,filter$[,map$[,mode]]])` | |
| `map_set` | `x = map_set(map$,key,value)` | |
| `system` | `x = system(command$[,number])` | |
| `bgsystem`, `run_open` | `x = cmd(string$)` | |
| `get_actual_piece` | `x = get_actual_piece(end)` | `profileend` |
| `randomnumber` | `x = randomnumber(low,high)` | `value`, `value` |

Decisions:

- Forms written `x =` require the assignment.
- `map_set` key and value accept a string or any variable (54 working corpus lines pass objects or numbers).
- `set_piece_inst` takes only a string value, and its property names are not limited to the documented ones.
- `function` checks only that one name or string follows. The list of C functions is not known.
- `note` without an index (3 corpus lines) is not validated, the same gap as `groupf` in ADR 0020.
- Return types are unchanged (`system`, `bgsystem` and `map_set` stay `boolean`).
- `get_edge_dir` output 1 is now typed `pvector`. The generator asserted that it was omitted, for parity with the removed Node server; it now asserts the type.

## Shared parsing fixes

The corpus gate surfaced these. They affect every command.

- A word followed by `$` is a string variable, not a command: `Function$ = get(...)` in export_env_trg.prg.
- A call whose string holds `) =` is not read as an array-element assignment: `R = map_set(Map$,"SG2BRIX","... (Brix) = ...")` in dbdemo03.prg.

## Tests and corpus gate

`server/tests/note_map_system_commands_test.cpp` (target `note-map-system-commands-test`) covers the requested lines, the corpus forms, the invalid shapes and the strict types. The `drawing-file-commands-test` unit test and all goldens pass.

The gate opened 2,548 C:\vxbd320 files with strict mode on, baseline first and then the new build.

- **10 removed**, all `Invalid syntax for 'set'` on a string variable `Set$`.
- **35 added**:
  - 16 `get_pnt_param(P) holeticker,...` (CYKCLT_puncher, VSCYK_puncher in customlbab and customukraine): a number as the name. The matching `set_pnt_param` uses `holeticker$`.
  - 11 `note(1) "...",1!I` in `system/sysprogs/ansi_*`: an inline `!` is NOT, so `1!I` is not a number.
  - 6 `set_piece_inst` strict-type errors in coloring_profiles.prg, where `Pce` was already typed `value` and the other commands on it are flagged too.
  - 2 `function` in the binary file d_JOINT_CUSTOM, which is not a program.
