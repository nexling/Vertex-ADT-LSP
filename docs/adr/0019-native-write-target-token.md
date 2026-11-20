# A type write takes effect at its target token

A variable can be both the input and the output of one statement: `obj = get_panel_core(obj)`, `dbfind(Id) Id,1,Sel$`, `vec_times_scalar(D) V,-D/2`. Before this change, each type-timeline write applied to its whole line, because every reader used `write.line <= line`. So the input `obj` was already read as the command's output type (`area`) instead of the type it had going in. That hid real strict errors and caused false ones, and it miscolored the input token and gave it the wrong object-parameter completion.

## Decisions

- **Rule.** A write on line N is visible to its own target token on line N (the assignment target or the output slot) and to every line after N in the same named section. Every other occurrence of that name on line N sees the type from before line N. One lookup, `typeWriteAt` (`server/src/validate.h`), applies this rule. It is used by strict command-syntax checks, semantic tokens and object-parameter completion. Cross-call argument reads already read strictly before the line.
- **Tokens, not names.** `TypeWrite::column` is the target token's byte column. A read passes the byte column of the token it is reading. In `vec_add(dv) dv, pt` the output `dv` is `pvector` and the input `dv` keeps `dvector`. Strict checks get parameter columns by splitting the call's text at top-level commas (`paramColumns`). If a piece doesn't match the parsed parameter, its column is -1, so it reads the type from before the line.
- **Whole-line writes stay whole-line.** `[type]` comment fallback, `##included_from` snapshot writes, inferred parent writes and cross-call injections have column -1 and are visible to their entire line, as before.
- **No earlier type means untyped.** In `fresh = f(fresh)` with no earlier write, the right-hand `fresh` falls back to `[type]` comments and extracts, then counts as unknown. It doesn't borrow the line's own write. Used-before-assignment reports it as before.
- **Vector-to-value is unchanged.** `v = vec_get_x(v)` with `v` a vector still warns. Only the right-hand read changed.
- **Strict reads use 0-based lines.** `getCurrentVariableType` used to compare 0-based write lines with a 1-based line number. That let a write on the *next* line type the current line. It now reads at the 0-based line.

## Consequences

- Corpus gate (`C:\vxbd320` root, customvbc and customukraine roots, strict mode on, problem cap raised): 12 strict false positives removed (`dbfind(X) X,…`, `dbopen(Id) "…",Id`, `vec_times_scalar(D) V,-D/2`) and none added.
- Golden: `scripts/goldens/62-self-reassign-types.js` (`test-fixtures/self-reassign-types/TESTING.md`).
- Hover does not read the timeline (it shows `;!` comments), so it is unaffected.
