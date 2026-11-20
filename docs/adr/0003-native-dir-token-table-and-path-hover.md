# Native `$DIR_*` token table and path hover

`$DIR_*` tokens used to map to folders through a 23-row table, matched by substring and returning the first hit, so `$DIR_SYSPROGS_SHARED` resolved to `sysprogs` and `$DIR_PICTS_SHARED` to `picts`. In the native server only, one table in `server/src/dir_paths.cpp` now drives everything that reads a token: document links, include/call resolution, include/call completion and hover. It covers every keyword-catalog `$DIR_*` whose detail starts with "returns" (any case). Matching is exact and case-sensitive: `$dir_macros` is not a token, and the token is `$DIR_` plus the following `[A-Za-z0-9_]` run, so `$DIR_SYSTEM&` reads as `$DIR_SYSTEM` followed by `&`.

- **Fallback tokens** (MACROS, COMPLIBS, SYMBOLS, SYSBASE→`sysbase`, LIGHTWORKS→`lw`, PIPELIB→`pipelibrary`, …): `custom/<folder>` if the path exists there, else `system/<folder>`, else `none/<folder>`.
- **Pinned tokens**: `$DIR_SYMBOLS_SYSTEM` → `system/symbols`, `$DIR_PICTS_SHARED` → `shared/picts`, `$DIR_SYSPROGS_SHARED` → `shared/sysprogs`, `$DIR_CUSTOM`, `$DIR_SYSTEM` and `$DIR_SHARED` → that tree. They are shown even when the folder doesn't exist.
- **`$DIR_THIS`**: the folder of the document being edited. At runtime it is the running program's folder, which differs for included sections.
- **`$DIR_ACTPROJ`**: `shared/projects/<actproj>`. The `<>` marks a helper value, never a link.
- Tokens without a "returns" detail (`$DIR_HOME`, `$DIR_EXPORT`, …) stay unmapped.

## Path hover

- **Token**: hovering a token anywhere (code, strings, `#$DIR_X#`, `get(...)`, include/call paths) shows the catalog entry and `→ <path>`. The path is the text written after the token up to whitespace, a quote, `,`, `(`, `)`, `&` or `;`, with the token replaced by `<tree>/<folder>` and no extension added. The tree comes from where the whole path exists.
- **Variable**: a variable whose value holds a token gets `→ <path>` below any doc comment, or a hover of its own when it has none. The value is its last assignment at or above the hover line in the open file, ignoring branches. An occurrence right of `=` on its own assignment line (`Dir$ = Dir$&Lib$&"/"`) shows the value before that line. The trace follows `&` (the language's string concatenation; `+` is not), `#var#` interpolation, `get($DIR_X)` and copies, up to 8 variable lookups. Unknown parts show as `<name>` or `<expr>`.
- **Values show as built**: `get($DIR_X)` is the folder plus a trailing `/`, as Vertex returns it, while a token written in a string stays as written. Trailing and duplicate slashes are kept (`custom/complibs//x`), and a missing slash shows glued (`"$DIR_MACROS"&Lib$` → `none/macrosDOOR_CUSTOM`), so mistakes are visible. Only the existence check normalizes the path. Each value picks its tree on its own, so `Dir$ = get($DIR_COMPLIBS)` then `Path$ = Dir$&"DOOR_CUSTOM/door_d.vxp"` shows `custom/complibs/` and `custom/complibs/DOOR_CUSTOM/door_d.vxp` when those exist in custom.
- Paths display relative to the workspace root, with the `custom/`, `system/` or `shared/` part. They link to the file only when the target exists on disk and holds no placeholder.

## Catalog

`npm run harvest-catalogs` applies native overrides on top of the frozen Node harvest: `$DIR_SYSTEM&` is renamed `$DIR_SYSTEM` (the `&` was a typo) and `$DIR_SYMBOLS`'s detail becomes "-custom/system".

This is a deliberate divergence from the frozen Node server. The contract is asserted by `server/tests/dir_paths_test.cpp` (target `dir-paths-test`) and `scripts/goldens/49-dir-path-hover.js`.

## Considered Options

- Following assignments in the include neighborhood: rejected for now, because the neighborhood extract keeps names and comments, not assigned values.
- Ignoring case in tokens: rejected. Vertex tokens are case-correct.

## Consequences

- `$DIR_SYSPROGS_SHARED/x` and `$DIR_PICTS_SHARED/x` now resolve under `shared/` instead of `sysprogs/` and `picts/`.
- `$DIR_JOINTLIB`, `$DIR_PROGRAMS`, `$DIR_SHARED`, `$DIR_CUSTOM`, `$DIR_SYSTEM` and `$DIR_SYMBOLS_SYSTEM` now resolve in links and completion. `$DIR_THIS` and `$DIR_ACTPROJ` resolve only in hover.
- Include/call completion on an empty path offers `$DIR_SYSTEM` instead of `$DIR_SYSTEM&`.
