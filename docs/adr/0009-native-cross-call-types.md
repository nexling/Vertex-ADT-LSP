# Native cross-call types

A variable typed as a `database` in a caller used to arrive in the callee untyped. `allstudsDB = get($CALLPARAM1)` stayed generic unless someone wrote `;!"…[database]"` by hand, and nothing checked that the hand-written type matched what callers actually pass. The native server now carries object types across `call(` edges in both directions, and checks them.

- **Parameters (caller to callee).** The callee's `name = get($CALLPARAMn)` takes the type of the nth argument at every `call(` site that resolves to it. The argument's type is its last type-timeline write before the call line, in the same section. A number is `value`. Quoted strings, string variables, `&` concatenations, expressions and untyped names contribute nothing. An array element `arr(i)` uses `arr`'s own type, unless that type is `array`. The callers come from the workspace index's call links (the same graph as the Impact CodeLens), and their text is buffer-wins.
- **Return fields (callee to caller).** On a callee format line (`ret$ = "KEY=" & var & "|KEY2=" & var2`, the scan call return-value completion already uses), each field takes `var`'s type on that line. In the caller, `X$ = strfield(R$,"KEY")` after `R$ = call(...)`, then `Y = X$` (or `Y = strfield(R$,"KEY")` directly), gives `Y` that type. This is tracked by data flow: `R$` and `X$` keep their source until they are reassigned. It is not tied to the snippet's line layout.
- **Inferred beats commented.** An inferred type is written into the type timeline on the `get(` / receiver line and replaces the `[type]` comment fallback for that name in that section. Everything that reads the timeline (semantic tokens, object-parameter completion, vector-to-value) sees it. Cheap hover does not read the timeline and is unchanged.
- **Agreement.** Types match when they are equal, when both are vector types (`pvector`, `dvector`, `xvector`, `yvector`, `zvector`, `nvector` all count as one family), or when either side is untyped or `generic`. When callers disagree, the parameter gets no inferred type and the `[type]` comment, if any, still applies. A return field packed with disagreeing types gets none either.
- **Scope.** A named-section call (`call("x.prg" SECTION, …)`, or the suffix inside the quotes) types only the reads inside `#SECTION#`. A whole-file call types the reads outside every section. Same-file section calls work the same way, using one provisional pass of the file's own timeline. `get($CALLPARAMn)` reads in an `##included_from` fragment take the arguments of calls to its parent. Lines with the `IGNORE TYPE` marker contribute nothing on either side. `$CALLPARAMn` used directly, not through `get(`, is ignored. Return-field scans are whole-file, like the completion's.
- **Chains and cycles.** Types follow chains (A passes a database to B, B passes that parameter on to C). Resolution has two stages per file. The parameter stage builds a timeline with parameter types only, which is what callees read for arguments. The full stage adds receiver types, which is what callers read for return fields. A file whose stage is already running is skipped, so a cycle (A → B → A) terminates and contributes nothing through the loop. One walk goes at most 6 calls deep and visits at most 300 files. Past that a file counts as unresolved.

## Diagnostics

All of these require strict mode plus `vertexadt.strictMode.crossCallTypeChecks` (default `true`). Source is `ADT-Syntax`. Each carries `relatedInformation` that points at the contributing lines.

| Code | Where | Severity |
|---|---|---|
| `vertexadt/cross-call-argument-type` | caller argument whose type differs from the callee's `[type]` comment on that `get($CALLPARAMn)` | Error |
| `vertexadt/cross-call-comment-type` | callee `;!` comment whose `[type]` differs from the type all callers agree on | Error |
| `vertexadt/cross-call-param-conflict` | callee `get(` name when callers pass different types | Warning |
| `vertexadt/cross-call-return-conflict` | each callee format-line key packed with differing types | Warning |
| `vertexadt/cross-call-return-comment-type` | caller receiver comment whose `[type]` differs from the returned type | Error |

The two conflicts are warnings, because disagreeing callers aren't wrong code in themselves. The mismatches are errors. When callers agree on a type the comment contradicts, both sides are flagged: each caller argument and the callee comment.

## Freshness

Inferred typing is always on. Only the diagnostics are gated by strict mode. When the debounced index flush sees an edit, close or disk change in a file that mentions `call`, it drops the prep and diagnostic caches of every other open document that mentions `call`, and refreshes diagnostics and semantic tokens. Other files' timelines are cached in the resolver by path. A cached timeline is reused while its text hash, injected types, strict flag and the hashes of every file its build read (includes, `##included_from` parents) are unchanged. When the open file's parameters-only timeline already has the final injection, the prep reuses it instead of collecting again.

The implementation is `server/src/cross_call_types.cpp` (facts, walk, injection) and `server/src/validate_cross_call_types.cpp` (the family). It is asserted by `scripts/goldens/55-cross-call-types.js` over `test-fixtures/cross-call-types/`. Native only: the Node server is frozen and has none of this.

## Considered Options

- Take the comment over the inferred type. Rejected: the type callers really pass is the better guess, and a stale comment should be reported, not trusted.
- Use only open documents as callers. Rejected: a callee's colours and errors would depend on which tabs happen to be open.
- One-stage resolution with a plain cycle guard. Rejected: a file's own arguments were cut off when its callee's return fields depended on them, so `back = f$` after `call("callee", db, …)` lost its type.
- Return types placed by line position after the snippet. Rejected: it breaks as soon as the snippet's lines are moved or edited by hand.

## Consequences

- On V32 corpus with strict mode, `walldetailing.prg` (13k lines, many calls) costs about +10 ms on open and about +50 ms per edit over the same build without cross-call types. Typical files are unchanged.
- Editing any call-participating file re-validates the other open call participants after the 300 ms debounce, even when the edit doesn't change a type.
- A type that can only arrive through a cycle, or deeper than 6 calls, is not inferred.
