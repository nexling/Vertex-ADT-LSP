# Program move

Golden: `scripts/goldens/61-program-move.js` ([ADR 0017](../../docs/adr/0017-native-program-move.md)). The workspace root is this folder. Nothing is written: the command and `willRenameFiles` only return edits.

## Files

- `custom/macros/secframing/beam.prg`: the file that moves. It does `# include frag.p` and `call("beam.prg INNER")` (both document-relative).
- `custom/macros/secframing/frag.p`: `##included_from beam.prg`.
- `custom/macros/secframing/sibling.prg`: `# include beam.prg`.
- `custom/macros/caller.prg`: two `$DIR_MACROS/…/beam.prg` calls (one `Secframing`, one with ` SETUP`), a string mention, a dynamic `path$ & "beam.prg"`. The golden opens it at version 7 with one extra call line.
- `custom/macros/shared.prg` overrides `system/macros/shared.prg`.
- `system/macros/syscaller.prg`: a System caller of beam.
- `system/complibs/taken/x.prg`: a System file a Custom target would shadow.

## Expected

| Check | Result |
|---|---|
| move beam → `$DIR_COMPLIBS/tools/beam2.prg` | 7 edits: caller ×3 (buffer line included, version 7), sibling, frag, beam's own `$DIR_MACROS/secframing/frag.p` and `beam2.prg INNER`; rename last; needs-confirmation annotation |
| skipped | System caller, string mention, dynamic path |
| target `$DIR_MACROS/caller.prg` | refusal: already exists |
| move shared.prg | confirmation naming `system/macros/shared.prg`; with `confirmShadow` an edit |
| drag `secframing` → `sf` | plain `changes`; caller → `$DIR_MACROS/sf/beam.prg`; sibling untouched; `vertexadt/programMoveReport` sent |
| drag a `.txt` | `null` |
| code action | `only: ['refactor']` and `triggerKind: 1` give it, automatic `triggerKind: 2` does not |
