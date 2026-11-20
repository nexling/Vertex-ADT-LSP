#SKIPPED#
dbopen(1) "A",1
loop
    exitif x
    dbclose(1)
endloop
#SKIPPED#
#BALANCED#
loop 3
    dbopen(2) "B",1
    dbclose(2)
endloop
#BALANCED#
