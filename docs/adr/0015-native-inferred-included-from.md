# Native inferred `##included_from`

A file that is `# include`d (or `call`ed) from other programs used to see its parents' variables only through a hand-written `##included_from` line. Without one, everything the parent set up showed as `used before declaration`, and completion, hover and definition knew nothing about it. The native server now works out a file's parents from the workspace index and treats them as if the line were written. Nothing is written to the file.

## Decisions

- **Which links count.** Every indexed `# include` of the file. A `call` counts only when the caller sets `common_indexes 1` or `common_points 1`. A plain `call` passes data through `$CALLPARAMn`, which cross-call types already handles. A file is never its own parent.
- **What a parent contributes: what comes before the include point.** That means the names the parent assigns on lines above its *earliest* include of the child, inside the same `#SECTION#` as that include line (or outside every section). Assignments in the parent's other sections don't count. Branches and loops are ignored, the same as for used-before-assignment ([ADR 0009](0009-native-used-before-assignment.md)). The parent's own context at that point (its written or inferred parents, transitively, with cycle guards and at most 8 levels) is added too.
- **Context is per section.** `# include child.prg JOINT` feeds only the child's `JOINT` section. An include with no section feeds every section of the child. A child line outside every section sees the parents that include the whole file. A line in section `S` sees the parents that include the whole file or `S`.
- **Several parents: intersection.** A name counts as declared in a scope only when *every* parent reaching that scope declares it. In the type timeline, a name gets a type only when every reaching parent agrees on it. Otherwise it is declared but untyped (no semantic token, no vector-to-value transition). This doesn't apply to navigation: hover shows the first comment in parent-path order, definition returns one location per parent that declares the name above its include point, and references search every parent.
- **Cap.** With more than 10 distinct parents (after the `call` filter), nothing is inferred. The CodeLens says `N parents, too many to infer, pin one`.
- **Written lines win in their scope.** A resolved `##included_from` written outside every section switches inference off for the whole file. One written inside a section switches it off for that section only, and its context applies only there. Written parents are unioned, as before.
- **Written lines also stop at the include point (changed behavior).** The parent's earliest `# include` or `call` of this file is the cut-off. A plain `call` counts here because the author asserted the link. For a line written inside section `S`, an include of `S` or of the whole file wins over any other include. If the parent never references the child, the whole parent counts, as before.
- **Broken written lines.** A written `##included_from` whose path resolves to no file gets an `ADT-Syntax` Warning `##included_from file not found: <path>` (code `vertexadt/included-from-not-found`) on the path. The line counts as absent, so inference still runs. Paths with no `/` (for example `##included_from corner_pieces.prg`) stay unsupported: those directives are wrong, and the resolver was not widened for them.
- **Editor surfaces.**
  - **CodeLens** on line 1: `Included from: progs/a.prg`, `…a.prg, b.prg`, or `…a.prg +N`. Clicking it runs `vertexadt.openIncludedFromParents`, which opens the parent, or shows a quick pick when there are several. There is no lens when a written line covers the whole file or there are no candidates.
  - **Source actions** (only when the request asks for `source`, which is the Source Action… menu): `Pin ##included_from <path>` per candidate, plus `Pin all N inferred parents as ##included_from` when there are several. They insert the lines at the top of the file in the library-relative format that `findIncludedFile` resolves. Because written lines also stop at the include point, pinning doesn't widen the context.
- **Setting.** `vertexadt.inferIncludedFrom` (default `true`; the client sends it flat as `inferIncludedFrom`). Off: no inference, no lens and no pin actions. Written lines still use the include-point cut-off.

## Where

- `server/src/parent_context.{h,cpp}`: `resolveParentLinks` (written directives with cut-offs, inferred candidates, cap), `linesBeforeIncludePoint`, and the include-neighborhood builder `fillIncludeNeighborhoodExtract`. That builder now fills `VariableExtract::outsideSections` / `bySection` alongside the union sets.
- `WorkspaceIndex::incomingLinks` (a lazily rebuilt reverse map). `upsert` / `removeKey` return the link targets whose incoming links changed, so `WorkspaceState::dirtyInferredChildren` refills just those open children. The startup walk refills every open neighborhood when it finishes.
- Consumers:
  - used-before-declaration and used-before-assignment, via `scopeFor` (`validate_used_before.cpp`)
  - the type timeline (`pushInferredParentWrites`, plus written-line cut-offs in `handleIncludedFile`)
  - definition (`inferredDefinitionSites`) and references
  - cheap hover and completion (union of the scopes)
- Not covered: the unused-variable check and cross-call types keep reading written lines only, whole-file, as before.
- `server/src/validate_included_from.cpp` (warning), `server/src/included_from_ui.{h,cpp}` (lens and actions), `client/out/extension.js` (command, setting).
- Goldens: `scripts/goldens/60-inferred-included-from.js` on `test-fixtures/inferred-included-from/`. The advertised `codeActionKinds` gained `source` (goldens 43 and 44).

## Corpus gate

This compares C:\vxbd320 with the build from just before this change against the new build, pulling diagnostics for every indexed `.prg` with the problem cap raised (`scratch/inferred-included-from/corpus-gate.js` and `gate-diff.js`). The index covers one custom tree at a time, so there are three runs: root `C:\vxbd320` (customa + system, 440 files), root `customb` (233 files), and root `customc` (208 files).

- **No new errors in any tree.** The only additions are 65 `##included_from file not found` warnings in customb. All of them are real:
  - 56 slashless paths (`corner_pieces.prg` ×30, `walltypesorter.prg` ×12, `complete_batten`, `*_pre`, …)
  - the typos `@DIR_MACROS` / `@DIR_COMPLIS`
  - a `.vxt` path
  - `$DIR_COMPLIBS/…/walldetailing.prg` / `floordetailing.prg`, which do not exist in the customb or System trees
- **customb: used-before-declaration goes from 2795 to 1842** (915 removed in 22 files), plus 28 used-before-assignment and 38 other diagnostics removed. Most of these files carry a broken slashless directive. That line now counts as absent, so inference supplies the real parent. For example, `Platesplicer.prg` goes from 502 hits to 0 through `corner_pieces.prg`, which assigns every sampled name (`Area`, `Selemobj`, `OrigP`…, `TP3`) above its `# include` on line 828.
- **customa + system and customc: 2 removals each.** These trees already have resolving written lines on most included files. One removal is a vector-to-value transition. It came from a type `walldetailing.prg` assigned after its include of `beamnotching_sub.prg`, which the written line's new cut-off removes. The other is a counter's used-before-assignment, now satisfied by its inferred parent.

The startup walk also gained a per-file guard: one file failing `parseIndexEntry` is skipped (and logged to stderr), instead of the exception silently aborting the whole walk.

## Considered Options

- **Union over parents.** Rejected in favor of intersection: union hides real "only declared on one path" bugs.
- **Whole parent instead of the include point.** Rejected: it lets a parent's later assignments leak into the child, which is the main inaccuracy of hand-written lines.
- **Writing the line into the file automatically.** Rejected: files are shared and edited outside VS Code. Pinning stays an explicit source action.
- **Treating every `call` as a parent.** Rejected: a plain call has a separate scope, and counting it would pull up to 33 callers into the intersection (`pick_refpiece`).

## Consequences

- Native and Node now differ on every included child file. The Node server is frozen and keeps the old behavior. Parity diffs should leave out diagnostics on files with inferred parents.
- Existing written `##included_from` lines now stop at the parent's include point. Names the parent assigns after including the child no longer count.
- A file with a written line *inside* a section now gets that context only in that section. Before, it applied to the whole file.
