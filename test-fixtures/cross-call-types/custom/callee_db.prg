! callee: a stud database, a piece, a direction, a mixed slot
studs = get($CALLPARAM1) ;!"studs to search[profile]"
pce = get($CALLPARAM2)
dir = get($CALLPARAM3) ;!"direction[pvector]"
mixed = get($CALLPARAM4)
ret$ = "DB=" & studs & "|PCE=" & pce
exit ret$
