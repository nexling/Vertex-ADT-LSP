# Native used-before-assignment check

The used-before-declaration check asks whether a name is assigned *anywhere* in the document (or its include neighborhood). `z = later` followed by `later = 1` passes. The native server now also warns when the only assignments come after the use: `Variable later used before assignment (first assigned on line 2)`. It's an `ADT-Syntax` Warning on the use line, and its `relatedInformation` points at the assignment line. A name that is never assigned keeps the old `used before declaration` message. The new warning only applies to names the old check already treats as declared, so it never replaces that message and never shows up next to it.

- **Order is text order.** A use is fine if an assignment comes on an earlier line. `if` branches are not tracked, so an assignment in any earlier branch counts. `loop … endloop` gets no exception: `if oldSheet then … oldSheet = sht` inside a loop is flagged, because the first pass reads a value that was never assigned. Writing `X = 0` before the loop clears it.
- **Same line.** On the first-assignment line, only the assignment target counts. `x = x + 1` with no earlier `x =` is flagged. An assignment in an earlier `;` statement on the same line counts (`X= 6; Y= X`).
- **Sections.** An assignment inside a `#NAME# … #NAME#` block only counts for uses in a block with the same name (blocks with the same name are treated as one section). An assignment outside every section counts everywhere after it. When the only assignment is in another section, the message reads `(only assigned in section NAME, line N)`.
- **What counts as an assignment.** The extract's matchers are reused, with tighter limits:
  - `X =` / `X$ =` / `X(i) =` count only at the start of a statement: line start, after `;`, or after `then` / `else`. This means `if X = 1 then` is a read and `"where ACT = X"` is text.
  - `set X` counts, and so do the synthetic `T…`/`L…`/`H…`/`P…`/`M…` names from `find_*_param` and friends, on their own line.
  - `name(` (a call or an array read) never counts.
  - Numeric uses only look at numeric assignments, and `X$` uses (call arguments) only look at string assignments, so `dx$ = dx` doesn't assign `dx`.
  - A declared name with no assignment of its kind is left alone. That covers names that are only compared, only called, or only assigned as the other type. Checking those would be a new kind of "never assigned" check, and it's out of scope here.
- **Include neighborhood.** Names that come in through `##included_from` count as assigned before line 1. Callee variables from `# include file SECTION` were never part of the extract, so they don't count, the same as before.
- **Where it's checked.** In the same three places as used-before-declaration: generic identifiers, `call(...)` arguments, and the `dbcreate(id)` id. Call arguments and dbcreate ids are never assignment targets, so a same-line assignment doesn't count for them.

The setting is `vertexadt.variables.warnUsedBeforeAssignment` (default `true`, flat `warnUsedBeforeAssignment` also accepted). It is separate from `strictMode`. Native only: the Node server is frozen and keeps the old behavior. Assignment sites are recorded per line in the live-buffer extract (`VariableExtract::assignmentSites` / `stringAssignmentSites` in `server/src/extracts.cpp`). The check is `emitUsedBeforeAssignment` in `server/src/validate_used_before.cpp`, and it is asserted by `scripts/goldens/16-used-before.js`.

## Corpus gate

Over the 884 `.prg` files in C:\vxbd320 (workspace root C:\vxbd320, so `##included_from` resolves), the check reports 145 hits in 62 files. Every hit was reviewed, and each one reads a variable before its first assignment in the text:

- About 90 are loop-carried or first-pass reads that rely on the variable starting at 0 (`if First = 0 then First = …`, shift chains, arrays that grow inside a loop).
- 27 are counters or self-updates (`AllPticker = AllPticker + 1`, `if Dot < Min then Min= Dot`).
- 27 are cross-section reads (for example, an `#INPUT#` section that uses `inpY` / `inpZ`, which are only assigned in `#BUILD#`).

Exempting counters or loop reads was considered and rejected. These are exactly the "relies on implicit zero" cases the check is meant to surface.

The false positives found along the way were fixed in the extract's site rules above, before this count:

- comparisons in `if X = …` counted as assignments (155 hits in one file)
- `;`-separated statements didn't count
- `set` didn't count
- a string assignment counted for the numeric name

## Considered Options

- Loop-aware ordering (an assignment later in the same loop body counts). Rejected: the first pass still reads the unassigned value, so this would mostly hide real bugs.
- Flow-sensitive analysis across `if` branches. Rejected: a lot of work, and it would raise false positives without a matching gain.
- Reusing the `used before declaration` message. Rejected: "this name doesn't exist" and "move this line up" call for different fixes.

## Consequences

- Workspaces that rely on implicit zero get new yellow warnings (about 7% of files in the reference corpus). Turning off `vertexadt.variables.warnUsedBeforeAssignment` brings back the old behavior.
- Array reads are now checked by order too: `z = arr(1)` before any `arr(i) =` is flagged.
- Native and Node now differ on every file with a forward reference. Parity diffs should leave out `used before assignment` messages.
