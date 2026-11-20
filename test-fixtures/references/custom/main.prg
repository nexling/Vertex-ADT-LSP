! References goldens: this buffer plus one-hop ##included_from
Alpha = 1
set Beta
Beta = Alpha + alpha + ALPHA
x = Alpha$ + Alpha2 + MyAlpha + Alpha_b
foo-bar = 2
y = foo + foo-bar + foo$
! Alpha = 99
z = 1 ;! Alpha = 3
w = 1 ; Alpha in a plain comment
msg$ = "Alpha" + Alpha
k = "a;!b" + Alpha
Name$ = "n"
q = Name$ + Name
v = Name$x + name$X
# include lib/not_followed.prg
##included_from lib/inc.prg
##included_from lib/inc.prg
  ##included_from syslib/sys.prg
##INCLUDED_FROM lib/upper.prg
##included_from lib/missing.prg
call "lib/called.prg"
Result = Alpha + LibVar + SysVar + Shared + Nowhere
if Alpha > 0 then
endif
#EDIT_FORM# Alpha
Late = 1
##included_from lib/late.prg
