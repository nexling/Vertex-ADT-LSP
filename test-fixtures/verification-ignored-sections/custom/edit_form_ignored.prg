if OUTSIDE and then
    X = 1
endif

#EDIT_FORM#
if BROKEN
    1BADVAR = 2
UNUSED = 99
@declarationnumbercheckstart
DUPA = 1
DUPB = 1
@declarationnumbercheckend
#EDIT_FORM#

if ALSO_OUTSIDE or then
    Y = 2
endif
