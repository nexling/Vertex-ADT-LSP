if (ccdist < 501) and (cmpi(dia$,prevdia$)) and (abs(prevxpos - xpos) < 1 or abs(prevypos - ypos) < 1) and (cmpi(prevcorners$,corners$)) and !(abs(prevxpos - xpos) < 1 and abs(prevypos - ypos) < 1) then
    X = 1
endif
if ccdist < 500 and (cmpi(dia$,prevdia$)) and (abs(prevxpos - xpos) < 1 or abs(prevypos - ypos) < 1) and !(abs(prevxpos - xpos) < 1 and abs(prevypos - ypos) < 1) then
    Y = 2
endif
