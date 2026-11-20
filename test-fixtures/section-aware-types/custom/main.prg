! File-level writes. VecA is pvector; MyP matches /[PXYZN](\d+)?$/ so tokens skip it unless strict mode.
vec_to(VecA) 1, 2, 3
vec_to(MyP) 1, 2, 3 ;! [pvector]
DbOnly = 1 ;!"[database]"
Unknown = 1
Copied = Unknown
#MACROS#
vec_to(MyP) 1, 2, 3 ;! [pvector] macros
vec_to(VecA) 1, 2, 3 ;! macros
Other = MyP
UseP = MyP
UseA = VecA
#MACROS#
#MACROS2#
UseP2 = MyP
UseA2 = VecA
#MACROS2#
#JOINT#
##included_from lib/callee.prg
Ref = IncludedVar
DbRef = IncludedDb
#JOINT#
OutsideInc = IncludedVar
! bang VecA
# include skipped.prg
call skipped.prg
debug ./x.dbg
msg$ = "VecA"
if VecA then
Shown = vec_to(VecA)
Foo$ = "x"
foo-bar = 1
vec_to(KeepVec) 1, 2, 3
KeepVec = KeepVec ;!IGNORE TYPE
end ##
