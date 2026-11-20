pc = get_actual_piece()
r$ = call("self_sections.prg" WORKER,pc)
exit
#WORKER#
wp = get($CALLPARAM1) ;!"worker input[database]"
exit
#WORKER#
