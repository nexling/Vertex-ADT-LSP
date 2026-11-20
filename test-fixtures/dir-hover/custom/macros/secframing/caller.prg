! $DIR_* path hover fixture. See test-fixtures/dir-hover/TESTING.md
call $DIR_MACROS/triggers/save_building BRACKETSECTION
Ret$=call("$DIR_MACROS/secframing/globalsizeminpmaxp.prg",checkmac)
text_edit "#$DIR_SETUP#/GEOMPROP.CUSTOM", "#$DIR_SETUP#/GEOMPROP.CUSTOM", 3, 3, 50, 80
pics$ = get($DIR_PICTS_SHARED)
base$ = "$DIR_MACROS"
sub$ = "triggers"
trig$ = base$ & "/#sub$#/save_building"
proj$ = "$DIR_ACTPROJ/" & name$
here$ = "$DIR_THIS/beam_web.prg"
sym$ = get($DIR_SYMBOLS)
lib$ = get($DIR_COMPLIBS)
low$ = "$dir_macros/x"
plus$ = "$DIR_MACROS" + "/x"
Dir$ = get($DIR_COMPLIBS)
Lib$ = "DOOR_CUSTOM"
Dir$ = Dir$&Lib$&"/"
Path$ = Dir$&"door_d.vxp"
dup$ = get($DIR_COMPLIBS) & "/DOOR_CUSTOM"
glued$ = "$DIR_MACROS" & Lib$
