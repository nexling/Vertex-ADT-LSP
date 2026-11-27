# Infinite-loop warnings (native only)

A bare `loop` (no count) repeats until something leaves it. A missing `exitif`, or an `exitif` whose condition never changes inside the body, hangs Vertex. The native server now flags both. The code is in `server/src/infinite_loops.cpp` (the walk, unit-tested by `server/tests/infinite_loops_test.cpp`) and `server/src/validate_infinite_loops.cpp` (the family). The Node server is frozen and has no such check.

## Rules

- **Only bare `loop`.** `loop 10` / `loop N` is bounded and never checked. Loops pair like control-block pairing (`;!` comments stripped, case-sensitive whole-word keywords, the shared per-line skip). A file with an unpaired `loop` or `endloop` gets no diagnostics; the leftover-loop check already reports it.
- **Ways out.** `exitif`, `break`, `exit` and `quit`, on their own line, after `;`, or in an inline `if … then …` / `else …`. `return` is not a way out. An `exitif` belongs to the innermost loop, so one inside an inner loop does not leave the outer one. `break` / `exit` / `quit` leave every loop.
- **Warning** (`vertexadt/infinite-loop`) on the `loop` keyword when the body has no way out: `Infinite loop: no exitif/break/exit inside loop ... endloop.`
- **Information** (`vertexadt/exitif-never-changes`) on each `exitif` keyword when the loop has no `break` / `exit` / `quit` and no exitif condition variable can change in the body: ``exitif condition can never become true inside this loop: `i` is never changed in the loop body.`` A variable changes when, anywhere in the body (nested `if`s and inner loops included):
  - it is assigned (`X =`, `X$ =`, `X(i) =`, `set X`), also inside an inline `if … then`;
  - it is passed in an output slot of a cataloged command, by the keyword insert text's labels (`out_X` / `outX`). `in_X` / `inX` / `InX` slots only read. Unlabeled slots and slots past the template may write;
  - it is any argument of a statement-level command that is not in the catalog.

  Names compare case-insensitively. Inline `!` is NOT, not a comment, so `exitif !Pce` reads `Pce`. The note stays silent when a condition calls a cataloged command (`exitif !dbfind(…)`), reads no variable (`exitif 0`), or when one of several exitifs is plausible.
- **Includes and calls.** `# include` is inlined (its target section only) when it resolves through the library layout. An unresolved include, an include cycle, or more than 16 nested includes silences both diagnostics for the loops around it, since the unseen code may leave or change anything. An `exitif` that comes from included code counts as a way out but silences the note (it has no line in this file). A `call` is opaque: it is not a way out, and it silences the note.
- **Not reported** in verification-ignored sections. Setting: `vertexadt.diagnostics.warnInfiniteLoops` (default on) controls both.

## Corpus gate (C:\vxbd320, 2026-11-27)

883 `.prg` files over three runs (root = customlbab + system, then customvbc, then customukraine), 272 of them with a bare `loop`: **0 warnings, 0 notes.** A probe document opened under the same root and settings got both diagnostics, so the zero is real. Production code ends every bare loop. The check is for code being written. Not gated: `.vxt`, `.p` and extensionless ADT files.
