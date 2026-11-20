#OPEN#
dbasepath$ = get($CALLPARAM1)
dbid = get($CALLPARAM2)
dbopen(dbid) dbasepath$,1
ok = dbvalid(dbid)
if !ok then
    dbcreate(dbid) dbasepath$,1,0, \
    NAME  A  32  x
endif
ok = dbvalid(dbid)
if !ok then
    break
endif
exit "1"
#OPEN#

#GOOD#
common_dbases 1
h = 15842
ret$ = call("chain_timer.prg OPEN","TIMES",h)
dbclose(h)
#GOOD#

#BAD#
common_dbases 1
h = 15842
ret$ = call("chain_timer.prg OPEN","TIMES",h)
#BAD#
