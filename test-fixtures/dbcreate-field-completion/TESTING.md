# dbcreate field completion

`textDocument/completion` adds this family on top of the cheap keyword-variable list, per [Specify dbcreate field completion](../../scratch/cpp-language-server-spec/issues/47-specify-dbcreate-field-completion.md). Golden: `scripts/goldens/33-dbcreate-field-completion.js` (`npm run test:goldens`). Local-only: no Custom/System listing. Spec cites no fixture directory; assertions are inline.

Every response for `vertexadt` is a `CompletionList` with `isIncomplete: false` that still contains the full cheap list. Family items are the ones whose `detail` is a field-slot string (cheap items never set those). Non-`vertexadt` is `null`. Trigger kind is ignored. No `textEdit` / `filterText` / `sortText` / `data` / `documentation`. `completionItem/resolve` leaves family items unchanged.

Site: backward `\` walk, start line must `/\bdbcreate\s*\(/i` after `;!` split (not quote-aware), joined prefix through the UTF-16 cursor, first `)`, then ≥ 3 unquoted after-`)` commas. Slot is empty tokens → 0, else `n % 4` (next slot while typing). Trailing space does not change `n`.

## Slots (cursor at end of `dbcreate(1) "p",1,0,` plus the suffix)

| Suffix | Labels | Kind | `detail` |
| --- | --- | --- | --- |
| `` / ` ` / `   ` | `FIELDNAME` | EnumMember | `wanted fielname - case sensitive, usually uppercase!` |
| ` NAME` / ` NAME ` | `A`, `N`, `B`, `T`, `M`, `U` | EnumMember | `Alpha/Numeric/Binary/Text/Memo/Unicode field type` |
| ` NAME A` / ` NAME A ` | `1`, `8`, `16`, `32`, `64`, `128`, `255` | Value | `Field length` |
| ` NAME A 64` / ` NAME A 64 ` | `x`, `IND`, `UNIQUE` | EnumMember | `No index` / `Indexed field` / `Unique indexed field` |
| ` NAME A 64 x` / ` NAME A 64 x ` | `FIELDNAME` | EnumMember | (next field) |

`insertText` is the label. Send the whole slot; no prefix-filter.

## Site vs not

| Text (cursor at end unless noted) | Expect |
| --- | --- |
| `DBCREATE (` / indent / `x = 1; dbcreate(` / `x = dbcreate(` / `dbcreate()` | slot 0 |
| `dbcreate(1` / `dbcreate(1)` / `"p",` / `Dbase$,0,DBCopyId` / `xdbcreate(` / `_dbcreate(` / `dbcreate 1,` / unclosed `"p,1,0,` | nothing |
| `"a,b"` / `'a,b'` quoted path commas | slot 0 |
| unquoted `path,with,comma,1,0, FIELD` | slot 2 (`['1,0,', 'FIELD']`) |
| `dbcreate(foo(1))` / `dbcreate(foo(1,2))` | slot 0 |
| `dbcreate(foo(1) "p",1` | nothing |
| cursor inside `(…)` or before the 3rd comma | nothing |
| glued `FIELD` after 3rd comma | slot 1 |
| `A\B` mid-line backslash | tokens `A`, `B` → slot 2 |
| `;!` after fields still a site; quoted `;!` in path not a site; bang `!` not stripped | — |
| unrelated `foo \` / `bar \` above `dbcreate(` | nothing |
| `dbcreate(` in the middle of someone else’s `\` chain | nothing |
| continuation: start `…, \` plus empty next line col 0 | slot 0 |
| continuation after a complete four-token group, next line col 0 | slot 0 |
| `;!` before trailing `\` breaks the walk | nothing on the next line |
