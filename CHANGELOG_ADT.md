# Changelog

## 2026-11-27
- **Five more commands are validated.** `file_exists(path$)`, `dircreate path$` (with or without parentheses), `map_store(map$)`, `show_error msg$[,hdr$[,time]]` and `edit_obj_data(obj,form$,mode)` now get red errors for a wrong number of parameters, a number where a string belongs, or an assignment on `dircreate`/`show_error`. Strict mode checks the string and value types. The assignment is optional on `file_exists`, `map_store` and `edit_obj_data`. A time right after the message is an error: `show_error "msg",5000` shows `Header parameter must be a valid string in show_error`, so write `show_error "msg","Header",5000`.
- **Too much indentation is flagged.** Inside `if`/`loop` blocks you now get a yellow warning when a line is indented too far, not only too little. The first line of a block may be at most one level deeper than its `if`/`loop` (`This line is indented more than one level deeper than the matching 'if'.`). The other lines must not go deeper than that first one (`…more than the other lines in this 'if' block.`), and lines after `else` count as the same block. One level is whatever the file mostly uses, so 2-space, 4-space and tab files are all fine. `endif` and `else` must line up exactly with their `if` (`This line should line up with the matching 'if'.`). A nested `if`/`loop` at the same column as its parent is now flagged as well. Comment lines, the continuation lines of a `\` split, and `# include` lines are not checked. A whole-line `;!` comment inside a block no longer shows "should be more indented". Format Document fixes all of these.
- **Enter indents after `if … then`, `loop` and `else`.** The new line was left at the same column after `loop 10`, after `then` followed by a space or a `;!` comment, and after `else`, so it was then flagged. It now moves in one level. `endif`, `endloop` and `else` move back out as you type them. An inline `if a then b = 1` does not indent the next line.
- **Infinite loops are flagged.** A bare `loop` (no count) with no `exitif`, `break`, `exit` or `quit` inside it now gets a yellow warning on `loop`: `Infinite loop: no exitif/break/exit inside loop ... endloop.` An `exitif` inside an inner loop only leaves that inner loop, so it does not count for the outer one. `return` does not count either. When the loop does have an `exitif` but nothing in the body can change its condition, the `exitif` gets a blue note, e.g. ``exitif condition can never become true inside this loop: `i` is never changed in the loop body.`` A variable counts as changed when it is assigned, passed as an output (`out_…`) argument of a command, or passed to a command the extension doesn't know. `# include`d code is followed. A `call` in the loop, or a condition that calls a command (`exitif !dbfind(…)`), silences the note. `loop 10` is never checked. Turn both off with `vertexadt.diagnostics.warnInfiniteLoops`.

## 2026-11-26
- **Fast coloring in debug traces.** `.dbg` files now open as Vertex ADT and are colored the moment they appear, even at 20–50 MB (above 20 MB VS Code turns off its own coloring, so this is the only coloring there). Keywords, commands, strings, numbers, `;!` comments and variables are colored. The `<value>` column is dimmed, and markers like `_if_begin_` and the `START OF DEBUG` / `END of FILE` lines are styled as comments. You can restyle these with the `debugValue` and `debugMarker` token types. Variables assigned anywhere in the trace get their color a moment after opening. Hovering a variable shows its most recent value, as before, but now instantly anywhere in the file. Debug files no longer get diagnostics, completions, formatting, code lenses or include/call lookups. VS Code never passes files over 50 MB to extensions, so traces larger than that (like a 340 MB run) stay uncolored.
- **String variables used before declaration.** `if makes3 = 1 and findstri(code$,"x95") then` now shows `Variable code$ used before declaration (did you mean Code$?)` when only `Code$` is declared. Before, a `name$` was only checked inside `call(...)` arguments. Names are case-sensitive, and the hint appears when a declared name differs only in case. Text in quotes, between `#`s and `$DIR_…` tokens is not checked, and lines with `get(`, `call`, `dbcreate` and the other skipped commands are still skipped. A string read before its first `X$ =` now gets `used before assignment` too (`vertexadt.variables.warnUsedBeforeAssignment`).
- **"Used before declaration" is now an error.** It was a yellow warning. This applies to numeric and string names, in ordinary lines, `call(...)` arguments and `dbcreate(id)`. "Used before assignment" and "exceeds maximum length of 16 characters" stay warnings.

## 2026-11-25
- **More commands are checked.** `get_pnt_param`, `set_piece_inst`, `set_edge_shape`, `get_edge_dir`, `proj_vxgeom_dir`, `notewinopen`, `notewinclose`, `note`, `dwgsetlevels`, `function`, `folder_files`, `map_set`, `system`, `bgsystem`, `run_open`, `get_actual_piece` and `randomnumber` now show errors for wrong parameters or a missing assignment. With strict mode on, input types are checked too. `get_edge_dir`'s first output is now a point (`pvector`).

## 2026-11-23
- **Signature box when moving through arguments.** Click or move the cursor into the arguments of `call(` or any other command, and the box with its parameter names comes back, with the current one highlighted. For `call`, it also shows the input's comment from the called program. Escape keeps it closed until you leave that command's arguments or type `(`, `,` or a space. Also while typing: in nested commands such as `call("p.prg",len(a$),` the box shows the command you are inside, commas in single-quoted strings and brackets no longer count as arguments, and commands split with `\` work. No box appears in comments, or for a `call` whose program can't be found or has no inputs. It follows `editor.parameterHints.enabled`.
- **Suggestions come back after a typo.** In a slot with its own suggestions (`call(` and include paths, sections, form names, macro codes and so on), the list reopens as soon as what you typed matches again. You no longer have to go back to `(` or `/`. It stays closed while nothing matches, and when the only match is exactly what you typed. In paths, `.` and `-` keep filtering, accepting replaces the whole folder or file name the cursor is in, and accepting a folder lists the next level. Sections with `-` in the name are listed too. It follows `editor.suggestOnTriggerCharacters`.

## 2026-11-20
- **One language server.** The extension now always starts the native language server. The old Node language server and the `vertexadt.languageServer.implementation` setting are gone. If you had it set to `node`, delete it from your settings; it no longer does anything. **Move Callable Program** no longer needs that setting. The extension supports Windows x64 only. On other platforms it shows an error and no language features start.
- **Move a program and keep its callers working (native server).** Right-click a `.prg` in the explorer → **Vertex ADT: Move Callable Program…**, run it from the Command Palette, or pick **Move callable program…** from the Refactor menu or Ctrl+. in the file. Type the new path, pre-filled as `$DIR_MACROS/secframing/beam.prg`, end it with `/` to keep the name, and get a preview of every change. One Undo puts it all back. `call(` paths, `# include` lines and `##included_from` lines that reach the file are rewritten in the same style: `$DIR_MACROS/secframing/beam.prg` becomes `$DIR_COMPLIBS/tools/beam.prg`, a relative include next to the file stays relative, and named-section suffixes like ` SETUP` are kept. Relative paths inside the moved file are fixed too. Dragging or renaming files and folders in the explorer does the same. Files in `system` are never edited, and a System file can't be moved from the command. Moving a Custom file that overrides a System file asks first. References that can't be rewritten (System callers, dynamic paths like `path$ & "beam.prg"`) are listed under **Show details** in the `Vertex ADT: Program Move` output.
- **Included files know their parents automatically (native server).** A file that is `# include`d by other programs no longer needs a `##included_from` line to see their variables. It gets what each parent sets *before* its include of the file, in that same section. A `call` counts too when the caller sets `common_indexes 1` or `common_points 1`. `# include child.prg JOINT` only feeds the child's `JOINT` section. With several parents, only variables that *every* parent sets count, so "set on one path only" is still reported. Hover shows the first parent's comment, Go to Definition lists every parent, and Find References searches them. A CodeLens on line 1 shows `Included from: progs/a.prg +2` and opens them. Source Action… → `Pin ##included_from …` writes the line for you. Above 10 parents nothing is inferred and the lens says so. A written `##included_from` still wins: written outside sections it covers the whole file, written inside a section only that section. It now also stops at the parent's include point, so variables the parent sets after including the file no longer count. A written line whose file does not exist shows the warning `##included_from file not found: …`. Setting: `vertexadt.inferIncludedFrom` (default on).
- **`if` conditions split with `\` (native server).** An `if` condition continued on the next line with a trailing `\` is now checked as one line. `if findstr(A$,"1x") or \` followed by `   findstr(A$,"4x") then` no longer shows "If statement must be followed by a then.", and the `findstr` calls in the condition no longer get "accepts at most 0 parameters after parentheses". A split inline if (`if A \` then `B then X = 1`) no longer leaves an `if without matching endif`. Mixing `and` and `or` across the lines is now reported, on the line with the operator that mixes them. A continuation line like `B = 2 then` is a comparison, not an assignment. A `\` followed by a blank line, a `!` comment or the end of the file now shows `Line continuation '\' must be followed by a code line.` on the `\`. Format Document also refuses that case.
- **Format Document re-indents blocks (native server).** Format Document and Format Selection now re-indent `if`/`else`/`endif` and `loop`/`endloop` blocks with spaces, 2 per level by default (`editor.tabSize`). Tabs in indentation become spaces. `else` lines up with its `if`. The continuation lines of an `if a or \` condition sit 2 levels deeper than the `if`. Other `\` continuation lines, such as `dbcreate(` arguments, keep their alignment. Comments follow the block depth, whitespace-only lines are emptied, and lines starting with `#` stay at column 0. Nothing after the indentation is changed. If the file has an unmatched `else`/`endif`/`endloop`, a block left open (at the end of the file or a `#SECTION#`), crossed `if`/`loop` nesting, or an `if` without `then`, nothing is changed and a warning names the line, for example `Vertex ADT: not formatted: unmatched endif at line 31.` The extension now defaults `vertexadt` files to 2-space indentation with spaces and indentation detection off. Setting: `vertexadt.format.enable` (default on).

## 2026-11-19
- **Unclosed databases follow paths and callers (native server).** A section that opens a database for its caller no longer warns when every caller closes it. `dbopen(dbid)` with `dbid = get($CALLPARAM2)` is matched to the caller's 2nd argument (`l45timedb`), and the check goes up the call chain until a section closes it. If a caller does not, the open line says `not closed on every caller path` and links to that caller's `call` line, and the `call` line itself warns. Each `#SECTION#` is now checked on its own, so a `dbclose` in another section does not count. `if`/`else` branches and loops are followed: a database closed on only one branch, or still open at an `exit` or a `break` (`still open at break (line 20)`), is flagged. After `ok = dbvalid(h)`, the `if !ok then … break` branch counts `h` as not open. The duty to close only passes to a caller with `common_dbases 1`. `;!IGNORE CLOSE` also works on `call`, `break` and `exit` lines.
- **Arithmetic on `findstr` results (native server).** `end = findstrri(panellabel$,"-")-1` and `+1` no longer show `findstrri accepts at most 0 parameters after parentheses`. In an assignment, `+ - * /` with an expression, a comparison, `and`/`or`, a closing `)` or a trailing `;` after the call are accepted. This applies to `findstr`, `findstri`, `findstrr` and `findstrri`. Text that looks like extra parameters (`e = findstr(a$,b$) b$`) and a bare `findstrri(a$,b$)-1` line are still errors, now for all four commands. `findstrr` is now checked like the others: 2 strings inside the parentheses. With strict mode on, the result of a findstr command is a number (`value`), not a `boolean`, so `abs(p)` after `p = findstr(...)` is no longer an error.
- **Types follow `call` parameters and return values (native server).** If `db` is a database in the caller, `studs = get($CALLPARAM1)` in the called program is now a database too, with its colour and its object-parameter suggestions. It works through chains of calls, through `call("file.prg" SECTION, …)` into that section (also within the same file), and into `##included_from` fragments of the called program. Returned values work the other way: after `r$ = call(...)`, `f$ = strfield(r$,"DB")` and `back = f$`, `back` gets the type the called program packed after `"DB="`. The type callers really pass wins over a `;!"…[type]"` comment. With strict mode on, mismatches are now reported: red on a `call` argument whose type differs from the callee's `[type]` comment, red on a callee comment that contradicts what every caller passes, red on a receiver comment that contradicts the returned type, and yellow when callers pass different types or a return field is packed with different types. Vector types (`pvector`, `dvector`, …) all count as the same. Lines with `IGNORE TYPE` are left out. Setting: `vertexadt.strictMode.crossCallTypeChecks` (default on, only with strict mode).
- **Variables used before they are assigned (native server).** `z = later` followed by `later = 1` now shows the yellow warning `Variable later used before assignment (first assigned on line 2)`. Before, any assignment anywhere in the file was enough. The order is plain line order, so `X = X + 1` with no earlier `X =` is flagged, and so is a read inside a `loop` when the variable is only assigned further down. Add `X = 0` before it. Inside `#SECTION#` blocks, only assignments in the same section (or above all sections) count. `if X = 1 then` is a comparison, not an assignment. Names from `##included_from` files count as assigned. Names that are never assigned keep `used before declaration`. Setting: `vertexadt.variables.warnUsedBeforeAssignment` (default on).
- **Pick a macro code without knowing its library (native server).** New setting `vertexadt.completion.macroCodeFirst` (off by default). When it is on, the first argument of `new_macro(` / `new_djoint(` / `new_sheet(` suggests `LIBRARY/CODE` pairs from every Custom `complibs` library instead of library names. Type any part of the code or the library, or both separated by `/` (`wal/corner`). Codes that match come before pairs that only match on the library, and the same code from different libraries is listed side by side. Picking a pair fills in both arguments, `"WALLS", "1cornermodel"`, and replaces a 2nd argument that is already there. Libraries whose database needs exporting are added in the background as they finish. If nothing is ready yet, the list shows "Exporting N macro libraries…". With a library already typed, the 2nd argument still lists that library's codes.
- **Unbalanced parentheses and brackets (native server).** `ret$=call("$DIR_MACROS/addtimerforactpanel.prg AFTERGEN",Panel_Label$` now shows `Mismatching parentheses for 'call'`. Every line is checked for a missing `)` or `]`, an extra one, or a crossed pair (`f(a[1)]` gives `Expected ']' but found ')'`). The red underline starts at the bracket that is wrong and runs to where its closer was expected, instead of covering the whole line. Each line shows at most one such error. Brackets inside strings and after `;!` are ignored, and a line ending in `\` can close its brackets on the next line.
- **Links to named sections in `call` paths (native server).** `ret$=call("$DIR_MACROS/addtimerforactpanel.prg AFTERMPRFLOOR",panellabel$)` is now a link that opens the file at `#AFTERMPRFLOOR#`. The same works for `call path SECTION` and `# include "path SECTION"`. The tooltip names the section. If the section is not in the file, the link opens the top of the file.
- **Substring index checks (native server).** Strings are 1-indexed, so `filename$[0,dotindex-1]` now shows a red error on the `0`. A quick fix changes it to `1`. Any literal start below 1 (`0`, `-1`, `00`) is flagged. A `0` end still means "to the end" (`A$[P+11,0]`). A negative literal end (`A$[1,-1]`) and a start past the end (`A$[5,3]`) show yellow warnings. Assigning into a substring (`A$[I,I]="T"`) is a red error. Variables and expressions inside the brackets are not checked.
- **No spaces around `&` (native server).** String concatenation must not have spaces or tabs next to `&`. `date$&" "&time$` is fine. `date$ &" " &time$` now shows a red error on each spaced `&`. A quick fix removes the spaces for one `&`, and "Remove spaces around '&' in file" fixes them all. `&` inside strings, `&&` in `if` conditions and comments are not affected.
- **piece.setup set suggestions (native server).** Inside the quoted set argument of `new_piece` (6th argument, e.g. `"FRAMING.WALL"`), suggestions list the piece sets from `vxsettings.xml` that apply to your configuration in `system/programs/vxsettings_config.xml`. Custom `programs/vxsettings.xml` changes are applied on top of the System file. Hovering the set string shows its description, `include_set` lines and `param` lines, with custom lines marked.
- **`set_piece_set` set suggestions, and empty slots (native server).** The same set suggestions now appear for `set_piece_set(Pce) "TRUEFEATS",2`, in the first argument after `)`. For both commands they also appear in an empty slot, where the set name is inserted with its quotes. Hover on a `set_piece_set` string follows the mode: without 1 (e.g. `,2`) the string is looked up as the set's description, with 1 (e.g. `,1+2`) as its name.

## 2026-11-18
- **Slot suggestions first (native server).** In a slot with its own suggestions (macro libraries and codes after `new_macro(` / `new_djoint(` / `new_sheet(`, form names, object parameters, include/call paths and sections, remaining `call` parameters, call return values, `dbcreate` fields, `debug` filename), those suggestions now appear at the top of the list. Keywords are still listed below them.
- **Find slot suggestions by any part of the name (native server).** In those same slots, typing any part of a name finds it: `cornermodel` finds `1cornermodel`, and `sys` after `new_macro(` finds `MACRO_SYS`. Names that start with what you typed come first. While you type, a slot's list is read once and then reused. A new completion request (for example Ctrl+Space) reads it again.

## 2026-11-12
- **Empty comma slots.** Leading, doubled, and trailing commas now keep empty parameter slots (`vec_times_scalar(joinstartP),shtlaytopN,jointholestart` is three after-paren args with the first blank). `()` is still zero params. Structure checks report every empty in-arity slot (`First/Second/Third/Parameter N … is missing in {command}`) and the existing at-most/at-least count errors, then stop. Extra blanks past the command’s max are only the too-many error.

## 2026-11-07
- **Form name completion.** `edit_obj_data`, `edit_jnt_data`, `dbedit` (2nd argument inside `()`), and `set_macro_form` (string after `)`) suggest files that sit directly in `custom/forms` and `system/forms`. Custom wins on duplicate names. Inserts the filename with extension; replaces an existing quoted token.

## 2026-11-05
- **Section-scoped `@require_common_*`.** A no-suffix `call("foo.prg")` uses **every** `@require_common_*` in the callee (file-level and all sections). `call("foo.prg", JOINT_TEST)` uses file-level markers plus `#JOINT_TEST#` only — it does not inherit the caller’s section. Caller `common_* 1` must be in the same section as the `call()` (file-level directive for a file-level call; a directive in another section does not count).

## 2026-10-06
- **Unclosed databases and files.** Warns on `dbcreate` / `dbopen` / `open` when the handle is not closed later in the same execution tree (`# include` inlined, `call` entered). `dbopen ... "ALREADY_OPEN"` is a blue information. Suppress a line with `;!IGNORE CLOSE`. Setting: `vertexadt.diagnostics.warnUnclosedResources` (default on).
- **debug-then-assignment.** now handles if … then VAR = assignments.

## 2026-09-23
- **change_sheet.** new description with limitations of command.

## 2026-09-21

- **get_panel.** now accepts 'area' as input object. (error fixed)

## 2026-09-15

### Fixed

- **String-variable slices.** `name$[start,end]` is kept as one argument and treated as the string variable `name$`. Valid: `vsflag = cmpi(filename$[1,3],"VS-")`.

## 2026-09-02

### Added

- **`change_sheet`.** Autocomplete, syntax highlighting, and validation for `Ok = change_sheet(Sheet,"Library","Code",Thickness)`. Changes a sheet to another from the library while keeping its existing shape. Works on normal, advanced, and insulation sheets. Thickness is optional (own thickness up to the library value, insulation sheets only). Available from Vertex 32.0.9 onwards.

## 2026-08-28

### Changed

- **`new_macro` if-then-exit failsafe.** `set RETURN "0"` is now accepted as well as `set RETURN "1"` before `exit` in `UPDATE_JOINT`, `JOINT_DETAILING`, and `JOINT_ADAPT`.

### Fixed

- **`IGNORE TYPE` in variable hover.** The `IGNORE TYPE` marker is hidden from variable and string-variable hover the same way `[area]` type tags already are. Example: `;!"korsregel layer[area] IGNORE TYPE"` hovers as `korsregel layer`.

## 2026-08-24

### Added

- **`pnt_to_plane` overlap warning.** The first and second arguments must not be the same as the third (plane point). Invalid calls get a yellow warning; in-place projection (`pnt_to_plane(D2, D2, D3, X, Y)`) is still allowed.
  - Valid: `ok = pnt_to_plane(D2, D, D3, X, Y)`
  - Warns: `ok = pnt_to_plane(D2, D, D2, X, Y)` (1st === 3rd)
  - Warns: `ok = pnt_to_plane(D2, D, D, X, Y)` (2nd === 3rd)
- **Undeclared `dbcreate` output ids.** `dbcreate(vertstudsDB)` and similar start lines now report `Variable … used before declaration` when the output id was never assigned. Field-definition continuation lines (`INDEX`, `XPOS`, …) are still ignored. Numeric ids such as `dbcreate(1)` are not flagged.

### Fixed

- **`;!"[type]"` semantic highlighting.** Type comments such as `allstudsDB = 54 ;!"[database]"` are read again before `;!` is stripped, so variables get the intended semantic color (including in included files). Bracket names are matched case-insensitively (`[database]` / `[Database]`).


# Vertex ADT — TLDR PRE 2026-08-24-------------------------------------------------

## New commands
- **Export Workspace Reports** — CSV inventories (program usage, declaration numbers, local slots)
- **Export Workspace Diagnostics** — full-workspace validation → `vertexadt-workspace-diagnostics.csv`
- **Fix Missing Call Parameters** — insert `""` for under-filled `call()` sites workspace-wide
- **Assign Local Slot Names** — rename callee locals to `lN` + original name
- **Show Impact Analysis** — callers/includers & blast radius (from CodeLens)
- **Vertex ADT: Create File Header** (`vertexadt.createFileHeader`) — runs on the active vertexadt file from the command palette.

## Headline features
- **Strict mode** — cross-file declaration-number conflicts (blue), scoped per program, not across unrelated callers
- **Section-aware declarations** — `#SECTION#` scoping for dupes & include/call section suffixes
- **Local slot checks** — `lN` (underscore L) naming, workspace uniqueness, redeclaration warnings, `@local_slots` manifest
- **Call param validation** — `$CALLPARAMn` count vs arguments + quick fixes across workspace
- **Callee `@require_common_*`** — callees declare required `common_* 1` on callers (no-suffix call uses all markers; explicit section is scoped; caller directive is call-site scoped)
- **Impact CodeLens** — direct references + blast radius on heavily used files
- **Unclosed databases/files** — `dbcreate`/`dbopen`/`open` must be closed in the execution tree (`# include` + `call`); `;!IGNORE CLOSE` to suppress
- **Unused variables** — section-aware, includes `if` conditions & linked files
- **If/or/and checks** — dangling operators before `then` and just after `if`
- **Joint section rules** — per-section `new_macro` / `new_djoint` / failsafe checks
- **Vector→value warning** — re-typing vector vars as `value`
- **Section-aware semantic colors** — types don’t bleed across sections
- **Stable cross-tab diagnostics** — no false errors when switching open files
- **No false errors in EDIT_FORM sections** — LSP now ignores them for verification

## Settings worth knowing
| Setting | Default |
|---------|---------|
| `strictMode.enabled` | off |
| `diagnostics.exportCustomOnly` | on (export custom files only) |
| `calls.validateParameterCount` | on |
| `diagnostics.warnUnclosedResources` | on |
| `variables.noteUnusedVariables` | off |
| `impactAnalysis.enabled` / `minIncomingCount` | on / 2 |

--- full release notes below

# Vertex ADT — User Update Log

This log summarizes validation, workspace analysis, and editor features added in recent development cycles. It focuses on **what you can do** and **which settings control it**, not implementation details.

---

## New commands

| Command | What it does |
|--------|----------------|
| **Vertex ADT: Export Workspace Reports** | Writes CSV inventory files to the workspace root for codebase cleanup and analysis. Also available as a VS Code task. |
| **Vertex ADT: Export Workspace Diagnostics** | Runs the same validation rules as the editor across the whole workspace (respecting your current settings) and writes a sortable CSV of all issues. Also available as a task. |
| **Vertex ADT: Fix Missing Call Parameters** | Finds `call()` sites workspace-wide that are missing arguments and inserts empty `""` placeholders to match callee expectations. |
| **Vertex ADT: Assign Local Slot Names** | Renames local declarations in the active callee file to the next available `lN` + original-name pattern (e.g. `SelemP` → `l2SelemP`), file-wide. |
| **Vertex ADT: Show Impact Analysis** | Opens a picker of direct callers/includers and blast-radius program scopes (usually triggered from CodeLens). |
| **Vertex ADT: Open Settings** | Jumps to Vertex ADT settings. |

---

## Workspace reports (CSV)

Run **Export Workspace Reports** to generate:

| File | Contents |
|------|----------|
| `vertexadt-program-usage.csv` | Programs referenced by at least one `call()` or `# include`, with separate `call_count` and `include_count`. |
| `vertexadt-declaration-numbers.csv` | Each `(variable_name, declaration_number)` pair and how many files declare it. |
| `vertexadt-local-slots.csv` | Local slot variables (`lN…` names) per program and declaration number. |

Counts are global across the indexed workspace. **Custom files always win over system** duplicates with the same relative path.

---

## Workspace diagnostics export (CSV)

Run **Export Workspace Diagnostics** to generate:

**`vertexadt-workspace-diagnostics.csv`**

Columns: `program`, `line`, `column`, `severity`, `code`, `source`, `message`, `related_program`

- Uses your **active settings** (strict mode, unused variables, call checks, etc.) — same rules as when you open a file manually.
- Resolves variables from **`##included_from`** includes during validation.
- Processes large workspaces in batches and writes results incrementally.
- Summary notification shows file count and issue counts by severity.

---

## Strict mode — workspace-wide declaration checking

**Setting:** `vertexadt.strictMode.enabled` (default: off)

When enabled, the extension builds a workspace index of programs and their `# include` / `call()` relationships (custom over system). It then reports **cross-file declaration-number conflicts**:

- **Blue (Information):** Two *different* variable names share the same declaration number in a linked namespace.
- **Related information** links to the conflicting declaration in the other file.
- **Scoped correctly:** Shared callees/includes (e.g. a global macro used by many programs) do **not** merge unrelated callers into one conflict group.
- **Intentional alignment allowed:** Same variable name **and** same number across linked files (e.g. `extravar = 105` in caller and callee) is **not** flagged.
- **`call()` linking** only applies when the caller uses `common_indexes 1` or `common_points 1`.
- **Value-typed declarations** (`;!"[value]"`) are excluded from cross-file conflict checks.

Within-file duplicates remain **yellow warnings** as before.

---

## Section-aware declaration numbers

Declaration-number checks now respect `#SECTIONNAME#` regions:

- The same number in **different sections** of one file is allowed (e.g. `var1 = 1` in `#MACROS#` and `varP = 1` in `#MACROS2#`).
- Duplicates **within the same section** are warned, including across multiple `@declarationnumbercheck` blocks in that section.
- `# include callee.prg MACROS` / `call("callee.prg MACROS")` only brings in declarations from the callee’s matching section, checked as if pasted into the caller’s section context.

---

## Local slot variables (called programs)

For programs that are **call targets**, the extension helps manage `lN`-prefixed local declaration slots:

| Feature | What it does |
|--------|----------------|
| **Redeclaration warning** | Warns when the same variable is assigned a *new* declaration number later in the same section (`vertexadt/declaration-redeclared`). |
| **Workspace slot uniqueness** | In strict mode, reports when multiple callee programs use the same `lN` prefix workspace-wide. |
| **Callee naming convention** | Warns when a called program uses non-`lN` names inside declaration blocks. |
| **`@local_slots` manifest** | Optional line listing expected local slots; validated against actual declarations. |
| **Assign Local Slot Names** | Command + quick fix to rename locals to `lN` + original name across the file. |
| **Local slots CSV** | Included in workspace reports export. |

**Settings:**

| Setting | Default | Purpose |
|---------|---------|---------|
| `vertexadt.locals.validateUniqueNames` | on | Workspace-wide `lN` prefix conflicts among callees (strict mode). |
| `vertexadt.locals.requireLNamesInCallees` | on | Callees should use `lN` names in declaration blocks. |
| `vertexadt.locals.warnOnRedeclaration` | on | Same variable, new declaration number in a section. |

---

## Call parameter validation

Validates `call()` argument counts against callee `$CALLPARAM1`, `$CALLPARAM2`, … declarations.

- **Missing arguments** → error + quick fix to insert `""` placeholders.
- **Extra arguments** → warning.
- **Workspace fix command** applies missing-argument fixes across the codebase.

**Setting:** `vertexadt.calls.validateParameterCount` (default: on)

---

## Callee-driven `common_*` requirements

Callees can declare requirements with markers in source:

`@require_common_indexes`, `@require_common_dbases`, `@require_common_elements`, `@require_common_files`, `@require_common_points`

If a callee requires e.g. `@require_common_indexes`, every `call()` to that program errors unless the caller contains **`common_indexes 1`**. A no-suffix `call("foo.prg")` (including `SECTION` / `SECTIONOFFILE`) uses **every** marker in the callee. An explicit target (`call("foo.prg", JOINT_TEST)` or `call("foo.prg JOINT_TEST")`) uses file-level markers plus that section only; a missing/unknown target name uses file-level markers only. The caller’s matching `common_* 1` must be in the **calling section** (or at file level for a file-level call) — a directive in another section or at the wrong level does not count. Requirements are per resolved callee file (includes inside the callee are not followed).

*(Replaces an earlier global “require common_indexes on all calls” setting.)*

---

## Impact & blast radius CodeLens

At the top of files referenced from multiple places, a **CodeLens** shows:

- Direct impact: call/include counts.
- Blast radius: how many files could be affected per isolated program scope.

Click to open a picker of callers, includers, and blast-radius file lists.

**Settings:**

| Setting | Default | Purpose |
|---------|---------|---------|
| `vertexadt.impactAnalysis.enabled` | on | Show Impact / Blast Radius CodeLens. |
| `vertexadt.impactAnalysis.minIncomingCount` | 2 | Minimum direct references before CodeLens appears. |

Requires **Editor: Code Lens** to be enabled in VS Code.

---

## Syntax & variable validation improvements

### If-statement checks
- **Dangling `and`/`or`:** Errors when a logical operator appears immediately before `then` without a complete second condition (e.g. `if x = 5 and then`).
- Valid multi-line and combined conditions (`if A=1 and B=2 then`) still pass.

### Joint section rules
Per `#SECTION#` validation for joint programs:
- Section-specific allow/deny for `new_macro` and `new_djoint`.
- Requires an `if … then exit` failsafe before `new_macro` where the section rules demand it.

### Vector → value warning
Warns when a variable is first typed as a vector (`pvector`, `dvector`, `xvector`, `yvector`, `zvector`, `nvector`) and later typed as `value`.

**Setting:** `vertexadt.variables.warnVectorToValueTransition` (default: on)

### Unused variables
Reports declared variables that are never referenced, including:
- References in **`if` conditions** (`if I==0 then`, multi-line `if` headers).
- Usage across **linked include/call files** (section-aware).

**Setting:** `vertexadt.variables.noteUnusedVariables` (default: off)

### Section-aware semantic highlighting
Variable type colors now respect `#SECTION#` scope — a type declared in `#MACROS#` no longer bleeds into `#MACROS2#`.

### Cross-tab diagnostic stability
Switching between open `.prg` tabs no longer shows spurious errors on inactive files from stale shared validation state.

---

## Settings reference (new or noteworthy)

| Setting | Default | What it controls |
|---------|---------|------------------|
| `vertexadt.strictMode.enabled` | off | Cross-file declaration conflicts, local-slot workspace checks, stricter type rules. |
| `vertexadt.diagnostics.maxNumberOfProblems` | 100 | Max diagnostics per file while editing. |
| `vertexadt.diagnostics.exportCustomOnly` | on | Workspace diagnostics export: only custom-folder files (skip system-only). |
| `vertexadt.variables.noteUnusedVariables` | off | Blue unused-variable hints. |
| `vertexadt.variables.warnVectorToValueTransition` | on | Vector → value re-type warnings. |
| `vertexadt.calls.validateParameterCount` | on | `call()` argument count vs `$CALLPARAMn`. |
| `vertexadt.locals.validateUniqueNames` | on | Workspace `lN` prefix conflicts (strict mode). |
| `vertexadt.locals.requireLNamesInCallees` | on | Callees must use `lN` local names. |
| `vertexadt.locals.warnOnRedeclaration` | on | Same var, new declaration number in section. |
| `vertexadt.impactAnalysis.enabled` | on | Impact / blast radius CodeLens. |
| `vertexadt.impactAnalysis.minIncomingCount` | 2 | CodeLens threshold. |
| `vertexadt.semanticHighlighting.enabled` | on | Semantic token highlighting. |
| `vertexadt.snippets.useExpandedSyntax` | off | Keyword snippets with parameter placeholders. |
| `vertexadt.debug.validationCommands` | off | Color validated vs unvalidated commands (slower). |

---

## Diagnostic severity cheat sheet

| Color / level | Typical use |
|---------------|-------------|
| **Error (red)** | Syntax, missing call args, missing required `common_*`, joint section violations |
| **Warning (yellow)** | In-file duplicate declaration numbers, redeclarations, vector→value, extra call args, callee naming |
| **Information (blue)** | Cross-file declaration conflicts, unused variables, local-slot workspace conflicts |

---

## Quick fixes & snippets

- **Insert missing `call()` parameters** — per-line quick fix and workspace command.
- **Rename to local slot name** — quick fix on callee declaration blocks.
- **`@local_slots`** — snippet for declaring expected local slot manifest line.

---

## Custom vs system files

Throughout indexing, validation, and reports:

- Files under **custom** are indexed first.
- **System** files with the same relative path are skipped when a custom copy exists.
- Call/include path resolution prefers **custom**, then **system**.

---

## Still on the roadmap (not in this release)

From the project todo list — not yet implemented:

- File header creator with configurable templates  
- Ignore `#EDIT_FORM#` section for verification  

---

## Recommended workflow for a large custom workspace

1. Enable **`vertexadt.strictMode.enabled`** when cleaning up declaration numbers.  
2. Run **Export Workspace Reports** for usage and declaration-number inventories.  
3. Run **Export Workspace Diagnostics** with **`exportCustomOnly`** on to audit custom programs.  
4. Use **Fix Missing Call Parameters** after callee signature changes.  
5. Use **Assign Local Slot Names** on callee programs before enabling local-slot strict checks.  
6. Watch **Impact CodeLens** on shared macros/callees before editing widely used files.

---

*For manual test scenarios and fixture-based verification, see `test-fixtures/workspace-declaration-index/TESTING.md` and related fixture `TESTING.md` files in the repository.*

---

## Header Creator

### Command
**Vertex ADT: Create File Header** (`vertexadt.createFileHeader`) — runs on the active vertexadt file from the command palette.

### Setting
**`vertexadt.header.username`** — author name used in the header’s third field (default: empty).

### Behavior
- **Replace** an existing star-banner block at the top when:
  - First non-blank line is a `!` line with ≥10 `*`
  - A matching closing star line exists
  - All non-empty lines between them start with `!`
- **Prepend** the new 4-line header otherwise (including when the middle has invalid content).

New header format:
```
!***************************************************************
! {FILENAME} / {YYYY-MM-DD} / {username}
! 
!***************************************************************