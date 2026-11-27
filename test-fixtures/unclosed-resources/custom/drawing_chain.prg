#OPEN#
Ok=dwgopen_bg("planlay",File$,1,1,20,108)
#OPEN#

#GOOD#
call("drawing_chain.prg OPEN")
dwgclose "planlay"
#GOOD#

#BAD#
call("drawing_chain.prg OPEN")
#BAD#

#OPENV#
Win$ = get($CALLPARAM1)
Ok=dwgopen_bg(Win$,File$,1,1,20,108)
#OPENV#

#VARGOOD#
call("drawing_chain.prg OPENV",Port$)
dwgclose Port$
#VARGOOD#

#LITGOOD#
call("drawing_chain.prg OPENV","tmp")
dwgclose "tmp"
#LITGOOD#
