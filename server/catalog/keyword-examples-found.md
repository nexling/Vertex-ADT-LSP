# Keywords with "no example in system" that now have examples

Source: `server/catalog/keywords.json`, entries whose `documentation` is "no example in system" (case-insensitive). 397 entries were checked.

Scanned: `C:\vxbd320\system` and `C:\vxbd320\customlbab`. Skipped: `.vxm` and `.vxp` (binary), XML, images and other binary files. Comment lines (`!`, `;`, `//`, `<`) were ignored.

Not added to `keywords.json` yet, as requested.

## Found in code (call style `name(...)`)

| Keyword | File (under `C:\vxbd320\`) | Line | Example |
|---|---|---|---|
| `complib_value` | `system\macros\triggers\edit_wall_frm` | 12 | `Form$ = complib_value(FrmLibrary$,"SCREEN_FORM")` |
| `complib_value` | `system\macros\triggers\edit_wall_frm` | 13 | `SubPath$ = complib_value(FrmLibrary$,"PATH")` |
| `complib_value` | `system\sysprogs\claddings.prg` | 11 | `SubPath$ = complib_value(FrmLibrary$,"PATH")` |
| `add_save_vers` | `system\macros\pcecalc\copy_eng_output.prg` | 10 | `VersionedPath$ = add_save_vers(UnversionedPath$)` |
| `getdate` | `customlbab\sysprogs\AUTODRWN.prg` | 290 | `write(donefileid) "Auto drawing assignment completed successfully on "getdate()" at "&gettime()` |
| `gettime` | `customlbab\sysprogs\AUTODRWN.prg` | 290 | same line as `getdate` |
| `is_weak` | `system\complibs\joint_sys11\wstf_062014a` | 110 | `Weak = is_weak(This,End1)` |
| `is_weak` | `system\complibs\joint_sys11\wstf_062016a` | 122 | `Weak = is_weak(This,End1)` |
| `refresh_struct` | `customlbab\complibs\joint_custom\shacktsymbol_pre` | 241 | `refresh_struct(Shaftsym) Shaftsym` |

## Found in code (statement style, no parentheses)

| Keyword | File (under `C:\vxbd320\`) | Line | Example |
|---|---|---|---|
| `dbbtmenable` | `system\macros\triggers\handle_settings_btm.prg` | 15 | `dbbtmenable "FUNCTION=PSD_SETTINGS",0` |
| `dbbtmenable` | `system\macros\triggers\handle_settings_btm.prg` | 17 | `dbbtmenable "FUNCTION=PSD_SETTINGS",1` |

## Not examples

Word-only matches for these were strings, comments, data tables or other products, not calls: `constr`, `day`, `delete`, `dialog`, `filepath`, `groups`, `publish`, `round`, `distance`, `stretch`, `text_z`, `translate`, `timestamp`, `week`.

The other ~380 keywords had no call or statement usage in the scanned files.

## Caveat

Word-only results were capped at 30 per keyword, so a real statement use of one of the "not examples" words could be past that cap. Only the six call-style keywords and `dbbtmenable` are confirmed.
