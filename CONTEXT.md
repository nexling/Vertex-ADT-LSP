# Vertex ADT

Language tooling for Vertex ADT programs, so an editor can analyze and navigate `.prg`, `.adt`, `.p`, `.vxt`, `.3d`, and `.2d` files.

## Language

**Vertex ADT**:
The programming language of Vertex CAD automation, stored in those file extensions.
_Avoid_: ADT, Vertex (when the language is meant)

**Language server**:
The long-running process that speaks LSP and is the source of truth for language intelligence.
_Avoid_: LSP, the plugin, the extension (when the process is meant)

**Language-server executable**:
The win32-x64 process image at `server/vertex-adt-ls.exe` that the extension host launches over stdio when the language-server implementation setting is `native`.
_Avoid_: server.js, node server, lsp.exe, the binary (when this path is meant)

**Node language server**:
The process at `server/out/server.js` that the extension host launches over IPC when the language-server implementation setting is `node`.
_Avoid_: old implementation, server.js (when the process is meant)

**Language-server implementation setting**:
The window-scoped `vertexadt.languageServer.implementation` setting (`node` | `native`, default `node`) that chooses which process the extension host starts.
_Avoid_: flag, C++ rewrite switch, old/new implementation

**Extension host**:
The VS Code client that starts the language server and owns editor chrome.
_Avoid_: client, plugin (when the VS Code process is meant)

**Language intelligence**:
What the user sees while editing: diagnostics, completions, hover, document links, definition, references, semantic tokens, signature help, CodeLens, code actions, workspace exports, and newline cleanup.
_Avoid_: LSP features, language support (when this set is meant)

**Impact CodeLens**:
The CodeLens at the start of a Vertex ADT document that shows incoming commons-gated-call and include counts and blast-radius size when the incoming total meets the threshold. Blast-radius programs are incoming-caller ultimate-root closures, not declaration-conflict scopes.
_Avoid_: declaration-conflict scope (when the blast-radius walk is meant), CSV `call_count` (when that ungated count is meant)

**Document link**:
An LSP target for a `# include` / `call` path, a debug-dump FILE/SECTION header, or a `debug "./…"` path under User. The `#L<n>` fragment is a 1-based line in the target file. A named-section suffix (quoted, or after the path on `call` or a line-start `# include`) sets `#L<n>` to the first `#NAME#` line of the target section; the file is opened at the top when the section is missing.
_Avoid_: named-section suffix (when `#L<n>` is meant), definition (when click-open is meant)

**Newline cleanup**:
The previous-line rewrite after Enter in `vertexadt`, only for `set_obj_param`, `set_djparam`, and `set_joint_param` (argument quoting and spacing).
_Avoid_: format document, cleanupLine, on-type format, newline command-argument cleanup (when this rewrite is meant)

**Indentation formatter**:
The native Format Document / Format Selection rewrite of leading whitespace only, as spaces, from `if`/`else`/`endif` and `loop`/`endloop` nesting; it refuses the whole document on any block imbalance.
_Avoid_: newline cleanup, prettier, beautifier (when this rewrite is meant)

**Program move**:
In the native server, moving or renaming Vertex ADT files (the `vertexadt.moveProgram` command, or an explorer drag through `workspace/willRenameFiles`) and rewriting, in their own style, the `call(` / `# include` / `##included_from` paths that reach them, plus document-relative paths inside the moved files. System files are never edited. Sites it can't rewrite are reported, not blocking. See `docs/adr/0017-native-program-move.md`.
_Avoid_: refactor (when the whole rename-plus-rewrite is meant), rename symbol, Fix Missing Call Parameters

**After-paren argument**:
A command argument that sits after the closing `)`, not inside the parentheses.
_Avoid_: afterParams, outside parameter (when this slot is meant)

**Empty-comma slot**:
A command argument that is the empty string because of a leading, doubled, or trailing comma.
_Avoid_: missing parameter (when the empty string from a comma is meant), omitted cell

**String expression**:
A quoted string, string variable, `ascii(...)`, or `&` concatenation of those.
_Avoid_: string variable (when quotes, ascii, or concat are included), string literal

**dbcreate logical line**:
The space-joined text of a parenthesized `dbcreate(` and its backslash continuation lines.
_Avoid_: continuation line (when the joined start-line text is meant), dbcreate field completion (when that picker's backward `;!` walk is meant)

**Command catalog**:
The Vertex ADT commands that have syntax structure, a table validator or a custom validator, and optional strict-mode slots and cleanup rules.
_Avoid_: keywords (when command syntax is meant), commandValidations

**Keyword catalog**:
The ordered completion and hover entries (label, detail, documentation, insertText), including words that are not commands.
_Avoid_: command catalog, keywords (when this catalog is meant)

**Keyword-variable completion**:
The cheap `textDocument/completion` list: keyword-catalog items (plus snippet duplicates and if/loop structure snippets when expanded syntax is on) and unique live-buffer then include-neighborhood variable and string-variable names.
_Avoid_: completions (when include/call path and section completion or other specialized families are included)

**Include/call path and section completion**:
The `textDocument/completion` items for a `# include`, `##included_from`, or `call` path or named-section-suffix slot: `$DIR_*` keyword-catalog labels on an empty path, Custom-then-System directory entries on a non-empty path, and section-name items from the linked-file snapshot.
_Avoid_: unknown-section check, keyword-variable completion, remaining-param completion (when this picker is meant)

**Remaining-param completion**:
The `textDocument/completion` items for leftover callee parameter names after a trailing comma on a parenthesized `call(` path. Labels are the callee’s `name = get($IDENT)` left-hand names, from a buffer-wins linked-file snapshot.
_Avoid_: call-param count check, signature help, include/call path and section completion, `$CALLPARAMn` (when these LHS names are meant)

**Signature help**:
Ordinary `textDocument/signatureHelp` for the active catalog command or parenthesized `call(`. Catalog labels are inside vs after-paren structure; `call(` uses `path` plus remaining-param left-hand names from the buffer-wins extract. Null when parens are closed and there are no after-paren params.
_Avoid_: remaining-param completion (when the comma list is meant), keyword-variable completion (when catalog `insertText` is meant)

**Cross-call type**:
An object type that a `name = get($CALLPARAMn)` read takes from the nth argument at every resolved `call(` site (workspace index callers, buffer-wins), written into the callee's type timeline. It beats the `[type]` comment. Callers that disagree leave it untyped. Native only; see `docs/adr/0009-native-cross-call-types.md`.
_Avoid_: comment type (when the inferred type is meant), parameter type (the catalog sense), call-param count

**Return-field type**:
The object type of the variable joined after `"KEY="` on a callee format line, carried to the caller's `Y = X$` / `Y = strfield(R$,"KEY")` receiver of that field.
_Avoid_: call return-value completion (the snippet, not the type), return type (a command-type write map entry)

**Call return-value completion**:
The `textDocument/completion` snippets on the line after an immediately previous `\w+$ = call(...)` assignment. Labels are `return:ret{field}$` from the callee’s whole-file buffer-wins snapshot.
_Avoid_: remaining-param completion, keyword-variable completion, `found=call` (when the site is meant)

**Form-name completion**:
The `textDocument/completion` items for top-level Custom-then-System `forms/` file names on `edit_obj_data`, `edit_jnt_data`, `dbedit` (2nd inside argument), and `set_macro_form` (after-paren argument). Custom wins duplicates; origin is `custom/forms` / `system/forms`.
_Avoid_: include/call path and section completion, workspace index (when this `forms/` scan is meant)

**Object-parameter completion**:
The `textDocument/completion` items for ten `get_*`/`set_*` sites: 2nd-inside comma on the seven `get_*` commands, after-paren `)` on `set_obj_param` / `set_joint_param` / `set_djparam`. Type comes from the type timeline (empty-timeline `[type]` fallback), not cheap hover. XML is the parameter list for `macro`/`joint` only.
_Avoid_: keyword-variable completion, cheap hover (when this type lookup is meant), macro library/code completion (when the XML list is meant)

**Generic-parameter fallback**:
When object-parameter completion finds no parameters for the name's object type (or its macro/joint record has none), it offers the command's `generic` parameters instead (themselves falling through to `any`).
_Avoid_: cheap hover's forced `generic` (a different thing)

**Primary database**:
A Vertex `d_*` database file in a Custom `complibs/{library}` folder that is not a `c`/`l` companion: a name ending `c`/`l` is a companion only when the same name without that letter is also present. Only primary databases get a sidecar XML.
_Avoid_: companion, sidecar (when the database itself is meant)

**Macro library/code completion**:
The `textDocument/completion` items for quoted 1st then 2nd inside arguments of lowercase glued `new_macro(` / `new_djoint(` / `new_sheet(`. Libraries are Custom `complibs/` directories with a `d_*.xml` sidecar; codes are unique `CODE`/`SHTCODE` cells from that XML. With **code-first mode** on, the 1st argument lists pairs instead.
_Avoid_: form-name completion, object-parameter completion (when XML parameter fields are meant), System `complibs/` (when this Custom-only listing is meant), `$DIR_COMPLIBS` (when include/call path listing is meant)

**Code-first mode**:
In the native server with `vertexadt.completion.macroCodeFirst` on, the 1st argument of `new_macro(` / `new_djoint(` / `new_sheet(` (still being typed: empty, an unquoted word, or an unclosed quote) lists one `LIBRARY/CODE` **pair** per non-empty code across every Custom library. Typed text without `/` is a substring of the pair; with `/` its left part matches the library and its right part the code. Picking a pair writes both arguments. Exports run on a background pass and libraries appear as they finish. See `docs/adr/0008-native-macro-code-first-completion.md`.
_Avoid_: combined mode, code-only completion (the pair always carries its library), slot substring match (when the `/` split rule is meant)

**Piece set**:
A named `function_begin` block under vxsettings `piece.setup` (e.g. `FRAMING.WALL`, `MOLDING`), which the 6th argument of `new_piece` and the first after-paren argument of `set_piece_set` refer to (`set_piece_set` mode without bit 1 looks the set up by its description instead). Its NAME is the first word of the `function_begin` Data, read whole with dots. Native completion and hover read it from System `programs/vxsettings.xml` with the Custom file applied as a GUID overlay; see `docs/adr/0004-native-piece-setup-set-completion.md`.
_Avoid_: set fragment (a `set_begin` block pulled in by `include_set`), profile set, library (the 7th argument)

**Settings condition**:
A vxsettings DataItem's `Config` / `Application` / `Units` / `Language` attribute: names joined by `and` / `or` with optional `not`, evaluated left to right against the value in `system/programs/vxsettings_config.xml`. Blank, missing or unparsable conditions match.
_Avoid_: Optbit (ignored), config file (when the condition expression is meant)

**Completion family ranking**:
In the native server, when any slot completion family (dbcreate field, debug filename, include/call, remaining param, call return value, form name, object parameter, macro library/code, piece set) adds items, they sort above the cheap keyword-variable list. Family items get `sortText` `0`+family digit+match group+key, and cheap items get `1`+label. This deliberately diverges from Node; see `docs/adr/0001-native-completion-ranks-slot-items-first.md`.
_Avoid_: specials-only completion (keywords stay visible), Node parity (when completion `sortText` is meant)

**Slot substring match**:
In the native server, slot completion family items stay when their label contains the typed text anywhere, ignoring case (`cornermodel` finds `1cornermodel`). The server filters on every keystroke (`isIncomplete`), and re-requests reuse the slot's cached listing. See `docs/adr/0002-native-slot-completion-substring-match.md`.
_Avoid_: fuzzy match (VS Code's client filter, still used for keywords), prefix match

**Trigger-character completion**:
In the native server, typing a completion trigger character (space, `,`, `(`, `/`, `"`, `)`) opens a popup only where a completion family adds items. Otherwise the keyword-variable list waits for the first word character (VS Code quick suggestions). Ctrl+Space always gives the full list. See `docs/adr/0007-native-trigger-characters-open-family-sites-only.md`.
_Avoid_: suggestions on space (the old behaviour), specials-only popup (keywords still show at a family site)

**Vertex Dbase Converter helper**:
The spawned Windows executable `Vertex_Dbase_Converter.exe` that exports a Vertex `d_*` database to sidecar XML.
_Avoid_: in-process conversion, converter DLL (when this helper is meant)

**dbcreate field completion**:
The `textDocument/completion` items for after-paren `dbcreate(` field slots (name / type / length / index, cycling `n % 4` after the third comma).
_Avoid_: command-syntax check, dbcreate logical line (when this picker's backward `;!` walk is meant)

**Debug-filename completion**:
The `textDocument/completion` item after `debug ` whose insert is `./_<basename-with-extension>.dbg`, or `./_<basename>_<section>.dbg` when nearest previous/next `#NAME#` markers pair. `<section>` is that marker spelling, not a named-section body.
_Avoid_: debug dump, document link (when this filename picker is meant)

**Table validator**:
Command-catalog checks expressed as a closed row and interpreted by a generic engine.
_Avoid_: validation function (when the row is meant)

**Custom validator**:
Command-catalog logic compiled as named code because the command does not fit the closed row schema.
_Avoid_: validation function (when the escape hatch is meant)

**Slot kind**:
A closed table-validator predicate for one assignment or argument: normal variable, string variable, any variable, string expression, string-or-variable, number-or-variable, or digit literal or variable. String-or-variable has two encodings: any-variable-or-string-expression, or normal-variable-or-string-expression.
_Avoid_: type, object type, allowedTypes (when a catalog cell's predicate is meant)

**Command-syntax check**:
The diagnostic family that applies command-catalog structure, a table validator or custom validator, call-command syntax, and optional strict-mode type slots.
_Avoid_: validateCommandSyntax, command validation (when this family is meant), call-param count check

**Editor chrome**:
VS Code UI that is not language intelligence: TextMate grammar, themes, command-palette wiring, and file-header insertion.
_Avoid_: the extension (when only chrome is meant)

**Editor-visible parity**:
Every language-intelligence surface that works today still works, and the message contract holds, even if protocol and internals change.
_Avoid_: protocol parity, identical implementation

**Message contract**:
Diagnostic `message`/`source` and hover MarkupContent `value` are the current strings and templates, not a paraphrase.
_Avoid_: cleaned-up wording, equivalent messages

**Parity golden**:
A `test-fixtures/` program plus the editor-visible LSP observations the spec requires of it.
_Avoid_: acceptance fixture (when the V32 corpus latency files are meant), smoke test (when the spec case is meant)

**Acceptance fixture**:
A V32 corpus file used for interactive latency MUST ceilings (typical and outlier).
_Avoid_: parity golden, test-fixtures (when the V32 corpus pair is meant)

**Diagnostic envelope**:
The user-visible diagnostic shape: wrapped `{lineText} has an error: {inner}` with source `ADT-Syntax`, unwrapped command-syntax with source `Syntax exception - ADT Plugin`, or unwrapped `ADT-Syntax` for catalogued exceptions such as the call-param count check and the unknown-section check.
_Avoid_: error format, problem message (when the shape is meant)

**Diagnostic family**:
One named group of diagnostics in a full-document validate, with its own store reads and setting gate.
_Avoid_: collector, check function (when the spec group is meant)

**Inline if-then**:
A line whose trim matches `/if\s+.*\s+then\s+.*/i`: an `if` with a then-clause that still has characters after `then` plus whitespace. It does not push the if or indent stacks.
_Avoid_: single-line if, if-then pairing (when stack membership is meant)

**Pending if-header**:
This-pass flag that an `if` line is still waiting for `then` across continuation lines that start with `and` / `or` / `&&` / `||`.
_Avoid_: unmatched if (when leftover ifstack is meant), inline if-then

**If-header logical line**:
An `if` line ending in `\` joined with its continuation lines into one line for diagnostics. Each `\` plus the next line's indentation becomes one space, and a segment table maps positions back (native only, ADR 0014).
_Avoid_: pending if-header (when the and/or-leading form is meant), dbcreate logical line

**Full-document validate**:
The diagnostic list for one URI+version: every diagnostic family in spec order, then the `@nodebug` / verification-ignored / message+line post-filter.
_Avoid_: incremental validate, specificlineindex, validateTextDocument (when the spec pass is meant)

**Include neighborhood**:
The parent-context closure of a Vertex ADT document (transitive): its written `##included_from` parents plus its inferred parents, each cut at the include point, scoped per section, written parents unioned and inferred parents intersected. It does not include `# include` or `call` targets.
_Avoid_: related documents, include/call graph, execution tree, diagnostic neighbor, linked file (when `call` or `# include` are meant), include-neighborhood extract (when the closure is meant)

**Inferred parent**:
A file that `# include`s a document (or `call`s it with `common_indexes 1` / `common_points 1`) and so acts as an `##included_from` parent without a written line. Off above 10 parents, and in any scope a resolved written `##included_from` covers.
_Avoid_: caller (when the commons-gated include relation is meant), linked file, diagnostic neighbor

**Include point**:
A parent's earliest `# include` / `call` line of a child (the matching section's include first). The parent contributes only names assigned above it, in the same section or outside every section.
_Avoid_: call site (when the cut-off is meant), included_from line

**Include-neighborhood extract**:
The cached variable names, string-variable names, and first-win hover comments of a document's include neighborhood. Cheap hover, keyword-variable completion, used-before-declaration, and `[type]` fallbacks read it; unused-variable, unclosed-resource, call-param, and unknown-section do not.
_Avoid_: include neighborhood (when the cache is meant), includeLinks, linked-file snapshot, live-buffer extract

**Live buffer**:
The current unsaved text of one open document, identified by its URI and version.
_Avoid_: snapshot (when the worker copy is meant), disk text, live-buffer extract (when the text rather than extracted names is meant)

**Live-buffer extract**:
The variable names, string-variable names, and first-win hover comments of this open document's current buffer.
_Avoid_: extractVariables (when the Node function is meant), include-neighborhood extract (when this file's names are meant), index declarations

**Cheap hover**:
The 20 ms editor hover built from the keyword catalog, this document's live-buffer extract, and the last include-neighborhood extract, plus debug-dump values when the buffer is a debug dump, plus the native DIR path hover.
_Avoid_: type-timeline hover, getVariableObjectType, provideHover (when this path is meant), definition jump (when `/\w+\$?/` vs the definition word is meant)

**DIR token table**:
The native table in `server/src/dir_paths.cpp` mapping each case-correct `$DIR_*` token with a "returns" catalog detail to a folder: custom-then-system fallback, a pinned tree, the edited file's folder (`$DIR_THIS`), or the helper value `shared/projects/<actproj>`. It drives links, include/call resolution, completion and hover (ADR 0003).
_Avoid_: kDirTokens, convertVertexPath (when the table is meant), Vertex install paths (it maps workspace folders only)

**DIR path hover**:
The `→ <path>` line the native server adds when hovering a DIR token or a variable whose traced value holds one. It shows the path relative to the workspace root, such as `custom/macros/x.prg`, `none/<folder>` when it exists in neither tree, and `<name>`/`<expr>` for parts it can't trace.
_Avoid_: document link (when the hover is meant), resolved value (it is a workspace path, not the runtime value)

**Definition jump**:
The `textDocument/definition` Location for a Vertex ADT name: this-file nearest prior assignment, else one written `##included_from` hop (above its include point), else one Location per inferred parent declaring it above its include point. The word is `/[A-Za-z][A-Za-z0-9_-]*\$?/`, not cheap hover’s `/\w+\$?/`.
_Avoid_: document link, references, include neighborhood (when this hop is meant), Go to Definition (when the LSP method is meant)

**References**:
The `textDocument/references` locations for that same word in this buffer plus one-hop written `##included_from` files and inferred parents.
_Avoid_: unused-variable check (when those same-section uses are meant), Impact CodeLens (when incoming counts are meant), include neighborhood (when this hop is meant)

**Semantic tokens**:
The `textDocument/semanticTokens/full` coloring of typed identifiers in a Vertex ADT document, from the type timeline (and command-catalog keys as `validatedCommandVariable` when validation-command coloring is on). Generic and unknown object types are not pushed.
_Avoid_: TextMate / grammar highlighting (when this LSP method is meant), encoded `data` array (when the legend type is meant)

**Semantic-token legend**:
The advertised LSP `tokenTypes` list in fixed order, from `variable` through `validatedCommandVariable`. Index 0 is `variable` and is not pushed.
_Avoid_: theme colors, token modifiers (when the type list is meant)

**Type timeline**:
The ordered per-name type writes collected during one full-document validate of this buffer (this-file command and comment writes, plus one-level `##included_from` snapshot types). Vector-to-value diagnostics and semantic tokens read it.
_Avoid_: variableTypes, objectType (when this pass's writes are meant), live-buffer extract (when types rather than names/comments are meant)

**Object type**:
A name such as `value`, `pvector`, or `generic` recorded on a variable.
_Avoid_: slot kind (when a catalog predicate is meant), type timeline (when the name rather than this-pass writes is meant)

**Command-type write map**:
One of the four tables that record which object types a command writes (`returnTypeMap`, `inputParamTypeMap`, `paramTypeMap`, `multiParamTypeMap`).
_Avoid_: command catalog (when these type tables are meant), type timeline (when the tables rather than this-pass writes are meant)

**Vector type**:
One of `pvector`, `dvector`, `xvector`, `yvector`, `zvector`, `nvector`.
_Avoid_: vector (when this closed set is meant)

**Value type**:
The object type named `value`.
_Avoid_: generic, untyped, non-vector (when this name is meant)

**IGNORE TYPE marker**:
A case-sensitive `IGNORE TYPE` substring on a trimmed line that skips type-timeline recording for that line.
_Avoid_: hover IGNORE TYPE strip (when collection skip is meant), IGNORE CLOSE marker

**IGNORE CLOSE marker**:
A `;!IGNORE CLOSE` suffix that suppresses the unclosed-resource leftover on that open or `# include` line.
_Avoid_: IGNORE TYPE marker, stripValidationComment (when this suppress is meant)

**Synthetic name**:
A `T`/`L`/`H`/`M`/`P` plus digits name implied by a `find_*`, `point(`, `line(`, `circle(`, `hatch(`, or `get_hatch_line` form rather than written as an assignment.
_Avoid_: object type, keyword-catalog label, live-buffer extract (when ordinary assignments are meant)

**Diagnostic neighbor**:
An open Vertex ADT document that `# include`s, `##included_from`s, or `call`s a given file, or that shares a strict-mode declaration-conflict scope with it.
_Avoid_: relatedDocuments (the LSP payload), include neighborhood, visible tab

**Declaration-conflict scope**:
One compare group for strict-mode cross-file declaration numbers: the overlay identities in a single forward include / commons-gated-call closure from one namespace root.
_Avoid_: include neighborhood, diagnostic neighbor (when the compare group is meant), blast-radius scope (when Impact CodeLens is meant)

**Linked file**:
A `# include`, `##included_from`, or `call` target whose text is used while validating another document.
_Avoid_: include neighborhood (when `call` or `# include` are included), diagnostic neighbor, linked-file snapshot (when the cached text is meant)

**Linked-file snapshot**:
The cached text of a linked file used while validating another document. An open buffer wins over disk.
_Avoid_: include-neighborhood extract, index entry (when the text is meant), linked file (when the target identity is meant)

**Custom**:
The workspace library tree that wins when the same program, include, form, or macro also exists under System.
_Avoid_: user folder, project folder, User (when this library tree is meant)

**System**:
The vendor library tree used when Custom has no matching file.
_Avoid_: installation folder, standard library (when this tree is meant)

**User**:
The Vertex user/runtime tree opened beside Custom and System (V32 corpus: `usera`). It is not a library overlay and is not indexed.
_Avoid_: user folder (when Custom is meant)

**Library-relative path**:
The path of a program under Custom or System without that tree’s folder name. It is the overlay identity of one workspace-index slot.
_Avoid_: relative path, overlay key, abs path (when this identity is meant)

**Workspace index**:
The Custom-over-System map from library-relative path to one index entry per path.
_Avoid_: workspaceDeclarationIndex, declaration index (when this map is meant)

**Index entry**:
The workspace-index record for one library-relative path: winning file identity plus parsed declarations, include/call links, commons, callee requirements, local-slot manifest, and no-section expected call-param count.
_Avoid_: parseIndexEntry (when the spec record is meant)

**Named section**:
A `#NAME#` pair in a Vertex ADT file; the text between the matching markers is that section's body.
_Avoid_: declaration-number block, target section (when the enclosing body is meant)

**Joint-section rule**:
A closed per-name allow/deny of `new_macro` and `new_djoint` on a named section, with an optional required if-then-exit failsafe.
_Avoid_: joint section (when the named section itself is meant)

**If-then-exit failsafe**:
An earlier `if`/`then` block in the same named section that sets `RETURN` to `"0"` or `"1"` and then `exit`s, required before `new_macro` by some joint-section rules. Compact `if … then exit` is not this.
_Avoid_: then exit (when that compact form is meant)

**Declaration-number block**:
The `@declarationnumbercheckstart` … `@declarationnumbercheckend` span whose assignments fill an index entry's declarations.
_Avoid_: named section, declaration section

**Value-typed declaration**:
A declaration-number assignment whose stored type is the value type.
_Avoid_: value variable (when this skip is meant), value type (when the assignment row is meant)

**Declaration number**:
The positive integer on the right of a `name = N` assignment inside a declaration-number block.
_Avoid_: slot (when this integer is meant), local-slot name (when `lN…` is meant)

**Declaration-number duplicate**:
Two different names in the same named section of this document occupying the same declaration number.
_Avoid_: redeclaration, cross-file declaration conflict

**Redeclaration**:
The same name occupying a new declaration number later in the same declaration-number block of this document.
_Avoid_: declaration-number duplicate, cross-file declaration conflict

**Verification-ignored section**:
A named section named `EDIT_FORM`. Declaration, include/call-link, local-slot-manifest, and callee-common-requirement extracts skip it. The call-param count check, the unknown-section check, and the unclosed-resource check skip those bodies on the caller. Expected call-param count, include-neighborhood extract, and the callee unique-name list used by the unknown-section check do not.
_Avoid_: ignored section (when this body is meant)

**Bang comment**:
A line or semicolon fragment whose first non-space character is `!`.
_Avoid_: hover type comment (when `;!"…"` is meant)

**Local-slot name**:
A declaration name: lowercase `l`, digits, a letter, optional letters/digits/`_`/`-`, optional `$`.
_Avoid_: local variable, l-prefix (when this name is meant), local-slot prefix

**Local-slot prefix**:
The `l` plus digits at the start of a name (`l1` in `l1Area`). Workspace uniqueness and callee naming compare prefixes, not full local-slot names.
_Avoid_: local-slot name, slot number (when the `lN` string is meant)

**Target section**:
The named section a `# include` or `call` invokes. Empty means the whole file.
_Avoid_: source section, named-section suffix (when the invoked identity is meant)

**Named-section suffix**:
The ident after a `# include` or `call` path that names a target section (inside a quoted path, or the first ident after the path). Keywords `SECTION` / `SECTIONOFFILE` and an empty remainder are not suffixes.
_Avoid_: remainder, section argument (when this token is meant), target section (when the written token is meant), document-link `#L<n>` ident (when `SECTION` is still a fragment name)

**Source section**:
The named section that contains a `# include` or `call` line. Empty means the line sits outside any named section.
_Avoid_: target section

**Callee common requirement**:
An `@require_common_*` marker on a callee, with a directive and the named section that contains it (empty means whole-file).
_Avoid_: commonIndexes (when the marker is meant), commons-gated call

**Commons-gated call**:
A `call` whose source index entry has `commonIndexes` or `commonPoints`. Unused-variable check, declaration-conflict scope, and Impact walk only those `callLinks`.
_Avoid_: callee common requirement, common_dbases, any call (when the gate is meant)

**Callee common-requirements check**:
The diagnostic family that warns when a `call` site's calling section lacks `common_* 1` for a stored callee common requirement.
_Avoid_: require_common check, commonIndexes diagnostic (when this family is meant)

**Expected call-param count**:
The highest `$CALLPARAMn` index in a file, or in one named section of linked-file text.
_Avoid_: parameter count, arity (when `$CALLPARAM` slots are meant)

**Call-param count check**:
The diagnostic family that compares a parenthesized `call(` site's inside-argument count after the path to the linked file's expected call-param count.
_Avoid_: arity check, call-command syntax (when the count family is meant), unknown-section check

**Bracket balance check**:
The native-only diagnostic family that reports at most one unbalanced `(` `)` or `[` `]` per logical line (an unclosed opener, an extra closer, or a crossed pair), underlined at the problem rather than across the whole line. See `docs/adr/0006-native-bracket-balance-check.md`.
_Avoid_: paren check, mismatching-parentheses check (that was the old whole-line catalog message it replaced)

**Unknown-section check**:
The diagnostic family that warns when a named-section suffix is not an exact or prefix match of a named section in the linked file.
_Avoid_: section completion, unknown include (when this diagnostic is meant)

**Unclosed-resource check**:
The diagnostic family that reports unmatched `dbcreate`/`dbopen`/`open` in this file’s execution tree, leftover `# include` opens, `ALREADY_OPEN` aliases, and (native) call lines that leave a called section's handle open. See `docs/adr/0012-native-path-aware-unclosed-resources.md`.
_Avoid_: resource leak, unclosed database (when this family is meant), commons-gated call (when `common_dbases`/`common_files` share is meant), dbcreate logical line (when continuation tails are skipped, not joined)

**Execution tree**:
The open/close walk of a Vertex ADT file: `# include` is inlined, `call` is entered. In the native server it is path-aware and rooted per section: `if`/`else`/`loop` paths are followed, and `break`/`exit`/section end are where open handles leak.
_Avoid_: include neighborhood, include leftover (when the walk is meant), include/call graph (when this walk is meant)

**Caller chain**:
The callers of a section, then their callers, that a `$CALLPARAM`-bound handle still open at the section's end is checked against. Each caller closes it, fails, or passes it up when the handle is also its own `$CALLPARAM` and it has `common_dbases 1` / `common_files 1`.
_Avoid_: call graph (when this upward walk is meant), include root (that is the `# include` equivalent)

**Include leftover**:
An unmatched open that entered through a `# include` in this file, diagnosed on that include line.
_Avoid_: include-upward (when this file’s include line is meant), call leftover (there is none), include neighborhood

**Include root**:
A transitive `# include` parent of this file that itself has no incoming `# include` parent.
_Avoid_: include neighborhood, diagnostic neighbor (when this graph node is meant)

**Call-command syntax**:
The parenthesized `call(` path check inside the command-syntax check: the first inside argument must be a quoted string, a string variable, or an `&` concatenation of those.
_Avoid_: call-param count check, call catalog row, validateCallCommandSyntax (when the spec check is meant)

**Used-before-declaration check**:
The diagnostic family that reports a reference whose name is not in the live-buffer extract or include-neighborhood extract, and a non-string extract name longer than 16 characters.
_Avoid_: undeclared variable (when this family is meant), use-before-define (when source-order is meant)

**Unused-variable check**:
The diagnostic family that reports this-file assignment declarations with no same-section reference, no `# include` target use, and no commons-gated-call use of that name.
_Avoid_: unused check, dead variable, live-buffer extract (when this family is meant)

**Debug dump**:
A Vertex ADT debug trace whose text contains `START OF DEBUG in FILE` or `START OF DEBUG in SECTION`. It may be opened as `vertexadt`, `plaintext`, or `log`.
_Avoid_: debug file (when the dump document is meant, versus the `debug` command)

**Extension URI**:
The installed extension's `context.extensionUri`, passed to the language server at initialize as `initializationOptions.extensionUri`.
_Avoid_: extension path, install directory, extension root (when this initialize value is meant)

**Hover image**:
One of the seven `ADT-documentation/` PNGs prepended as `![Description]({uri})` on hover for `pnt_inside_poly`, `is_point_inside`, `vec_intersect`, `vxgeom_cross_pnts`, `is_in_vxgeom`, `point_to_vxgeom`, and `pnt_to_plane`.
_Avoid_: explainer, documentation image (when this hover surface is meant)
