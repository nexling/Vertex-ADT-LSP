# `$DIR_*` path hover

Hover for `$DIR_*` tokens and the variables that carry them, per [ADR 0003](../../docs/adr/0003-native-dir-token-table-and-path-hover.md). Native server only. Golden: `scripts/goldens/49-dir-path-hover.js` (`npm run test:goldens`). Unit tests: `server/tests/dir_paths_test.cpp` (target `dir-paths-test`).

Open this folder as the workspace root (it has `custom/` and `system/`, and deliberately no `shared/`), set the language-server implementation to `native`, and open `custom/macros/secframing/caller.prg`. Every path line starts with `→ `. "Link" means the path is clickable and opens the target.

## `caller.prg`

| Hover on | Expect |
| --- | --- |
| `$DIR_MACROS` in `call $DIR_MACROS/triggers/save_building` | catalog detail, then `custom/macros/triggers/save_building` (link, no `.prg` added) |
| `$DIR_MACROS` in `call("$DIR_MACROS/secframing/globalsizeminpmaxp.prg", …)` | `system/macros/secframing/globalsizeminpmaxp.prg` (link: only in system) |
| either `$DIR_SETUP` in `"#$DIR_SETUP#/GEOMPROP.CUSTOM"` | `none/setup/GEOMPROP.CUSTOM` (no link) |
| `$DIR_PICTS_SHARED` | `shared/picts` (no link: no `shared/` folder) |
| `$DIR_THIS` | `custom/macros/secframing/beam_web.prg` (no link: missing file) |
| `$DIR_SYMBOLS` | `system/symbols` (link) |
| `pics$` | String Variable, `shared/picts/` (`get()` ends with a slash) |
| `base$` | `custom/macros` (link) |
| `trig$` (`base$ & "/#sub$#/save_building"`) | `custom/macros/triggers/save_building` (link) |
| `proj$` (`"$DIR_ACTPROJ/" & name$`) | `shared/projects/<actproj>/<name$>` |
| `lib$` | `custom/complibs/` (link) |
| `Dir$` in `Dir$ = get($DIR_COMPLIBS)` | `custom/complibs/` (link) |
| left `Dir$` in `Dir$ = Dir$&Lib$&"/"` | `custom/complibs/DOOR_CUSTOM/` (link) |
| right `Dir$` in `Dir$ = Dir$&Lib$&"/"` | `custom/complibs/` (the value before this line) |
| `Path$` (and `Dir$` on its line) | `custom/complibs/DOOR_CUSTOM/door_d.vxp` (link) / `custom/complibs/DOOR_CUSTOM/` |
| `dup$` | `custom/complibs//DOOR_CUSTOM` (link; the duplicate slash is shown) |
| `glued$` (`"$DIR_MACROS" & Lib$`) | `none/macrosDOOR_CUSTOM` (missing slash shown glued) |
| `Lib$` | no path |
| `$dir_macros` / `low$` | no path (tokens are case-sensitive) |
| `plus$` (`"$DIR_MACROS" + "/x"`) | no path (`&` concatenates, `+` does not) |
| `sub$` | no hover (plain string) |
| `text_edit` | normal keyword hover, no path line |
