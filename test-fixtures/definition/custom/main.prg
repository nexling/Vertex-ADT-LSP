! Definition goldens: nearest prior declaration, filter, one-hop ##included_from
Alpha = 1
set Beta
Gamma(3) = 5
! Delta = 9
Visible = 1 ;! Hidden = 2
foo-bar = 4
Name$ = "x"
Point = 3
##included_from lib/included_lib.prg
  ##included_from syslib/sys_only.prg
##INCLUDED_FROM lib/upper_ignored.prg
Result = Alpha + Beta + Gamma + Delta + Hidden + foo-bar + Name$ + LibVar + SysVar + Shared + LastLineVar + UpperVar
Alpha = Alpha + 1
msg$ = "Alpha"
if Alpha > 0 then
endif
Self = Self
Other = Point + point + IF
Later = 1
