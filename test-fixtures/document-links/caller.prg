# include "target.prg"
#include target.prg
# INCLUDE target.prg
# include sectioned.prg JOINT
  # include sectioned.prg SECTION
x = 1; # include target.prg JOINT
##included_from target.prg
! # include target.prg
Ret$=call("target.prg")
CALL("target.prg")
recall("target.prg")
call_virtual("target.prg")
call target.prg
call ("target.prg")
# include "$DIR_MACROS/from_dollar.prg"
# include "$DIR_EXPORT/nope.prg"
# include missing_not_a_file.prg
debug "./_caller.dbg"
debug macros/from_dollar.prg
  debug './_caller.dbg'
x debug "./_caller.dbg"
debug "./../outside.prg"
Ret$=call("sectioned.prg JOINT",x$)
call sectioned.prg JOINT
# include "sectioned.prg JOINT"
Ret$=call("sectioned.prg NOSUCH",x$)
