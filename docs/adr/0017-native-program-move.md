# Native program move

Moving or renaming a Vertex ADT file used to break every `call(` and `# include` that reached it, and nothing found them. The native server now rewrites those paths when a file moves. One engine (`planProgramMove`) has two entry points:

- **Command path:** the `vertexadt.moveProgram` command. You can reach it from the `refactor.move` code action, the Command Palette, or the explorer context menu.
- **Drag path:** explorer drags and renames, through `workspace/willRenameFiles`.

## Decisions

- **Scope: whole files.** Single files, or folders on the drag path (expanded to the ADT files inside them). Moving `#SECTION#`s between files is out of scope.
- **What is rewritten.** The path part of `call(` (quoted, `$`-unquoted, and the `call "x"` space form), `# include`, and `##included_from`. Comment lines (`!` at line start, after `;!`) are skipped. So are `call` words inside strings and same-file section calls (`call SECTION X`). The named-section suffix, quotes and anything else on the line are never touched; the edit covers only the path.
- **Finding references: text scan, not index links.** The workspace index keeps one link per target and section, and it doesn't record `##included_from`. So the engine reads every library-tree file (buffer-wins) whose text contains a moved file's basename and resolves each reference itself. Because of this the index doesn't have to be built for a move to be complete, and a move never refuses with "index still building" (that was the plan before this was found).
- **Resolution.** A reference reaches a file when:
  - `$DIR_THIS/…` resolves against the referencing file's folder (document-relative)
  - another `$DIR_*` resolves through `LibraryLayout::resolveVertexFilePath`
  - an absolute path exists as written
  - a bare path exists next to the referencing file (document-relative, as document links resolve it); otherwise it goes through `resolveVertexFilePath` (library-relative, Custom then System).

  Bare `##included_from x.prg` therefore counts as document-relative here, although [ADR 0015](0015-native-inferred-included-from.md) leaves it unresolved. Rewriting it to a `$DIR_*` path makes it resolvable.
- **What needs an edit.** A reference whose target moves, or a document-relative reference inside a moved file (its folder changed). References between two files that move together, staying relative, end up unchanged and get no edit.
- **Spelling.** The rewrite keeps the reference's style:
  - **Document-relative / `$DIR_THIS`:** stays relative when the target is in or under the caller's new folder. Otherwise it becomes a token path.
  - **`$DIR_*`:** becomes the Fallback token whose folder is the target's first library segment. The old token wins a tie. With no such token, it becomes `$DIR_CUSTOM/…` (or `$DIR_SYSTEM/…`).
  - **Bare library-relative:** stays library-relative.
  - **Absolute:** stays absolute.

  Directory segments the old path already spelled, in any case, keep the caller's spelling. The file name always takes the new spelling, so a case-only rename updates callers. Backslash-only paths keep backslashes. If no spelling reaches the target, the site is skipped and reported.
- **Command path refuses up front:**
  - a source outside Custom, or a System source (Vertex upgrades overwrite it)
  - a target outside Custom
  - an existing target (a case-only rename of the same file is allowed)
  - a target that would shadow a System file

  When the source shadows a System file at the same library path, it asks first, naming the System file the old path will reach.
- **Drag path can't refuse.** `willRenameFiles` only adds edits, so the engine never rewrites a caller to reach a different file:
  - **A System file moves:** its callers keep the old path, where a reinstall puts the file back.
  - **A file leaves the Custom tree:** treated as delete + create, with no edits.
  - **A shadowing problem:** the callers are rewritten anyway.

  In every case a warning names the problem. Folder drags collect everything into one report.
- **Skipped, never blocking.** These sites are reported and never stop the move:
  - references in System files ("System caller", since System files are never edited)
  - code lines that mention a moved basename without a reference that resolves (dynamic paths like `path$ & "x.prg"`, or plain strings)
  - references no `$DIR_*` path can reach

  Dynamic paths that don't contain the basename can't be seen at all.
- **Edits.**
  - **Command path:** one WorkspaceEdit. `documentChanges` carries text edits on the old URIs (versioned for open buffers, null otherwise), then the `RenameFile` ops, all under one `needsConfirmation` change annotation, so VS Code opens Refactor Preview and one Undo reverts the move.
  - **Drag path:** plain `changes`, applied by VS Code's own file-operation participant flow.
- **Reporting.** The command result carries `report`. The drag path sends it as the `vertexadt/programMoveReport` notification. The host shows one toast ("N reference(s) updated, M skipped" plus the first warning) with **Show details**, which opens the `Vertex ADT: Program Move` output channel, one `file:line [reason] text` per site. A drag with nothing skipped and no warnings shows nothing.
- **Code action.** `refactor.move` "Move callable program…" runs the host command. It is offered when the request asks for `refactor`/`refactor.move`, or when it was invoked (Ctrl+., `triggerKind` 1) with no `only`, never on the automatic lightbulb. The automatic one keeps Node parity for the existing code-action goldens.
- **Native only.** The Node server is frozen and has none of this. The host sets the `vertexadt.nativeServer` context key; the palette and explorer entries are hidden without it, and the Node server advertises no `fileOperations`, so drags change nothing.

## Where

- `server/src/program_move.{h,cpp}`: reference scan, resolution, spelling, safety checks, plan, JSON.
- `server/src/program_move_commands.{h,cpp}`: the command modes (`default`, `validate`, `plan`), `willRenameFiles`, the `fileOperations` capability (`**/*`, filtered server side).
- `server/src/code_actions.cpp`: the `refactor.move` action. `server/src/main.cpp`: wiring.
- `client/out/extension.js`: the command (input box pre-filled with the `$DIR_*` path, validated live, shadow confirmation modal, applyEdit), the report toast and output channel, and the context key. `package.json`: command and menus.
- Tests: `server/tests/program_move_test.cpp` (`program-move-test`), `scripts/goldens/61-program-move.js` over `test-fixtures/program-move/`.

## Considered options

- **Index links as the caller list.** Rejected: one link per target and section misses repeated calls, and `##included_from` isn't indexed.
- **Refuse while the index builds.** Dropped once the text scan replaced index links.
- **Only document-relative or only library-relative bare paths.** Rejected: document links use the first and the index uses the second, and real files rely on both.
- **Always write `$DIR_*` paths.** Rejected: relative includes inside a moved folder would churn for no reason.

## Consequences

- A move reads every library-tree file once (cached by the linked-file store, which the startup walk has already filled), and fully parses only the files that mention a moved basename.
- Every rename in the workspace reaches the server. Non-ADT renames return null without a file walk.
