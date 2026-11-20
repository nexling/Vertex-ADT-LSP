# Include/call path and section completion

`textDocument/completion` adds this family on top of the cheap keyword-variable list, per [Specify include/call path and section completion](../../scratch/cpp-language-server-spec/issues/39-specify-include-call-path-and-section-completion.md). Golden: `scripts/goldens/32-include-call-completion.js` (`npm run test:goldens`). Workspace root is this folder (Custom is `custom/`, System is `system/`). The unknown-section diagnostic on the same fixtures is `scripts/goldens/19-unknown-section.js`.

Every response for `vertexadt` is a `CompletionList` with `isIncomplete: false` that still contains the full cheap list. Family items are the ones with `sortText` (cheap items never set it). Non-`vertexadt` is `null`. Trigger characters: ` `, `,`, `(`, `/`, `"`, `)`.

`unknown_section_callee.prg` has names `JOINT`, `MACROS` (`SECTION` dropped).

## Modes

| Line (cursor at end unless noted) | Trigger | Expect |
| --- | --- | --- |
| `# include unknown_section_callee.prg ` | typed ` ` | sections, no leading space, empty replace at cursor |
| `# include unknown_section_callee.prg` | Invoked | sections, insert ` NAME`, empty replace at cursor |
| same | typed / no context / kind 3 | nothing |
| `… MA` / cursor inside `MACROS` | typed | sections, replace the whole ident |
| `… MACROS ` | typed | nothing (past suffix) |
| `… SECTIONOFFILE` | Invoked | sections, replace the keyword |
| `# include "unknown_section_callee.prg" ` | typed | sections |
| `# include "unknown_section_callee.prg"` | Invoked | nothing (glued to the quote) |
| `call("unknown_section_callee.prg ` / `call(unknown_section_callee.prg ` / `call unknown_section_callee.prg ` | typed | sections |
| `call("unknown_section_callee.prg")` cursor on `)` | Invoked | nothing |
| `Ret$=call("unknown_section_callee.prg` | Invoked | sections with leading space |
| `Ret$=call("…",` / `Ret$=call("… MACROS",` | typed `,` | nothing |
| `# include unknown_section_dir ` / `completion/both_target ` / `unknown_section_plain.prg ` / `unknown_section_empty.prg ` / `missing_no_file.prg SECTION` | typed | nothing |
| mid-line `# include`, `# INCLUDE`, `CALL`, `Ret$ = call("` | typed | nothing |
| `##included_from unknown_section_callee.prg ` | typed | paths (nothing listed) — never sections |

## Paths

| Line | Expect |
| --- | --- |
| `# include `, `call("`, `##included_from ` | only keyword-catalog labels starting with `$`, catalog order; Folder, `sortText` `0`+label, no `insertText` |
| `# include completion/` | `sub/`, `alpha.prg`, `both_target`, `both_target/`, `sys_only.prg` (Custom sorted, then System names not already listed; `D_skipped.prg` and `skipped.png` filtered). Folder `insertText` keeps `/`, `sortText` `0`+label; File `sortText` `1`+label. `alpha.prg` detail `2 sections` from the **Custom** text, documentation markdown ``- `ONE`\n- `TWO` ``; `both_target` `1 section`; no detail on nameless files. No `textEdit` on path items. |
| `##included_from completion/` | same labels, no attached names |
| `# include $DIR_MACROS/` | `unknown_section_foo.prg` (`1 section`) |

## Buffer-wins

With `unknown_section_callee.prg` open as `#FROM_BUFFER#…#FROM_BUFFER#`, `# include unknown_section_callee.prg ` offers only `FROM_BUFFER`.
