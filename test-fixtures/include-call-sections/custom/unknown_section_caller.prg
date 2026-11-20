# include unknown_section_callee.prg MACROX
# include unknown_section_callee.prg MACROS
# include unknown_section_callee.prg MAC
# include unknown_section_callee.prg JOINTS
# include "unknown_section_callee.prg" JOINT
# include "unknown_section_callee.prg BAD"
Ret$=call("unknown_section_callee.prg MACROX",Area)
call("unknown_section_callee.prg", MACROS, Area)
call unknown_section_callee.prg JOINT
call unknown_section_callee.prg JOINTS
# INCLUDE unknown_section_callee.prg MACROX
##included_from unknown_section_callee.prg MACROX
# include unknown_section_callee.prg SECTION
# include unknown_section_callee.prg SECTIONOFFILE
!# include unknown_section_callee.prg MACROX
# include missing_no_file.prg MACROX
# include unknown_section_plain.prg MACROS
# include unknown_section_empty.prg FOO
call("unknown_section_callee.prg MACROS")
s$ = 'call("unknown_section_callee.prg BADNAME")'
##call("unknown_section_callee.prg BADNAME")
debug call("unknown_section_callee.prg BADNAME")
messagebox call("unknown_section_callee.prg BADNAME")
# include unknown_section_dir MACROX
call "unknown_section_callee.prg JOINTS"
# include "unknown_section_callee.prg JOINTS"
call("a_missing.prg SEC1"); call("unknown_section_callee.prg SEC2")
 # include 'unknown_section_callee.prg' macros
# include "$DIR_MACROS/unknown_section_foo.prg MACROX"
call callee.prg SECTION
x=1;!call("unknown_section_callee.prg MACROX")
call("unknown_section_callee.prg MACROX");!bang
