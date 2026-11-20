pc = get_actual_piece()
r$ = call("callee_db.prg",db2,pc,5,pc)
r2$ = call("callee_db.prg",pc,pc,pc,pc) ;!IGNORE TYPE
exit
