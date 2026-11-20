# Semantic tokens

`textDocument/semanticTokens/full` for `vertexadt`, per [Specify semantic tokens](../../scratch/cpp-language-server-spec/issues/42-specify-semantic-tokens.md). Golden: `scripts/goldens/30-semantic-tokens.js` (`npm run test:goldens`). Workspace root is this folder (`custom/` tree).

Parity is the **legend type** of each named identifier, not the encoded `data` array or modifiers (always 0). `generic` and unknown object types are not pushed. Default settings: highlighting on, `debug.validationCommands` off, `strictMode.enabled` off.

Timeline writes reuse [Implement type timeline and vector-to-value transitions](../../scratch/cpp-language-server-impl/issues/13-implement-type-timeline-and-vector-to-value.md): `vec_to(Name)` types `Name`, not `Name = vec_to(...)`. The Node slice-test `run-type-lookup-test.js` stays Node lookup regression only (hardcoded timeline).

## `custom/main.prg`

| Identifier | Expect |
| --- | --- |
| File-level `VecA` on `vec_to(VecA) 1, 2, 3` | `pvectorVariable` |
| File-level `MyP` | no token (PXYZN skip; lookup is still `pvector`) |
| `DbOnly` | `databaseVariable` (`;!"[database]"`) |
| `Copied` | no token (`generic` copy of `Unknown`) |
| `UseA` / `VecA` on `UseA = VecA` inside `#MACROS#` | `pvectorVariable` (MACROS write, not file-level bleed) |
| `UseP` / `MyP` on `UseP = MyP` inside `#MACROS#` | no token (PXYZN) |
| `Other` on `Other = MyP` inside `#MACROS#` | `pvectorVariable` (copy of MACROS `MyP`) |
| `UseA2` / `VecA` on `UseA2 = VecA` inside `#MACROS2#` | no token (no bleed from MACROS) |
| `UseP2` / `MyP` inside `#MACROS2#` | no token |
| `IncludedVar` on `Ref = IncludedVar` inside `#JOINT#` | no token (`plane` is unknown) |
| `IncludedDb` on `DbRef = IncludedDb` inside `#JOINT#` | `databaseVariable` (directive-line write) |
| `IncludedVar` on `OutsideInc = IncludedVar` | no token (JOINT write does not apply) |
| `VecA` on `! bang VecA`, `# include`, `call `, `debug `, `"VecA"`, trailing `##` | no token |
| `if` | no token (keyword) |
| `vec_to` on `Shown = vec_to(VecA)` | no token (keyword); `validatedCommandVariable` when `debug.validationCommands` is on |
| `Foo$` | no token (lookahead rejects `$`) |
| `foo` / `bar` on `foo-bar = 1` | no token (no type) |
| `KeepVec` on the `;!IGNORE TYPE` line | `pvectorVariable` (collection skipped the line; earlier write still colors) |
| Same text as `plaintext` | `{ data: [] }` |
| Unopened document | `{ data: [] }` |
| `vertexadt.semanticHighlighting.enabled` off | `{ data: [] }` |
| `MyP` on MACROS `UseP = MyP` with strict mode on | `pvectorVariable` |

## `custom/dump.prg`

| Identifier | Expect |
| --- | --- |
| `KeepVec` on the first line | `pvectorVariable` |
| `KeepVec` / `Use` after `START OF DEBUG in FILE` | no token (scan stops) |

## `##included_from` parent buffer edit

The editor pull and tokens share one URI+version prep ([Reuse the validate pass's prep in semantic tokens](../../scratch/cpp-language-server-impl/issues/51-reuse-type-timeline-in-semantic-tokens.md)). Tokens must still follow a parent edit when `main.prg`'s own version does not change.

| Step | `IncludedDb` on `DbRef = IncludedDb` |
| --- | --- |
| Pull and tokens sent together, then tokens again | `databaseVariable` |
| Open `lib/callee.prg` and change its comment to `[pvector]` | `pvectorVariable` |
| Close `lib/callee.prg` (disk text wins again) | `databaseVariable` |

## `custom/vec_copy_child.prg`: cached `##included_from` parent and caller writes

The parent extraction is cached per parent text ([Cache the included-from parent extraction in the type timeline](../../scratch/cpp-language-server-impl/issues/52-cache-included-from-parent-extraction.md)). `lib/vec_copy_parent.prg` has `vec_copy(Copied) Src`, which reads the caller's earlier `Src` write, so an edit to the caller alone must change the copied type.

| Step | `Copied` on `Use = Copied` |
| --- | --- |
| Open (`vec_to(Src) 1, 2, 3`) | `pvectorVariable` |
| Edit line 1 to `vec_to(Src) 1, 0, 0` (parent text unchanged) | `xvectorVariable` |
| Restore | `pvectorVariable` |
