#OPEN#
h = get($CALLPARAM1)
dbopen(h) "A",1
#OPEN#
#USE#
d = 5
ret$ = call("chain_nocommons.prg OPEN",d)
dbclose(d)
#USE#
