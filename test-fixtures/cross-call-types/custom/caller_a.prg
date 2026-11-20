dbopen(db,"studs.dbf")
pc = get_actual_piece()
get_piece_dir(d,pc)
r$ = call("callee_db.prg",db,pc,d,db)
f$ = strfield(r$,"DB")
back = f$ ;!"returned studs[profile]"
pp = strfield(r$,"PCE")
exit
