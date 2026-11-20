dbopen(1) "A",1
ok = dbvalid(1)
if !ok then
    break
endif
dbclose(1)
