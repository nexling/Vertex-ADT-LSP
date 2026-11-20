semicolonindex = findstri(Line$,";!")
ok = findstri(Name$,"stack") ;! trailing comment
bad = findstri(Line$,";!"   ;! should still error — real comment, unclosed paren
