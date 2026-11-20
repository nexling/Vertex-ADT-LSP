# Form-name completion

In the form slot of `edit_obj_data`, `edit_jnt_data`, `dbedit` (the 2nd argument inside the parentheses) and `set_macro_form` (the argument after the closing parenthesis), `textDocument/completion` adds the top-level file names from Custom `forms/` and System `forms/`. The rules are in [Specify form-name completion](../../scratch/cpp-language-server-spec/issues/51-specify-form-name-completion.md).

Two tests use this fixture:

- `run-form-name-completion-test.js` is the Node regression test for `server/out/formNameCompletion.js`. Run it with `node`.
- `scripts/goldens/39-form-name-completion.js` is the parity golden against `server/vertex-adt-ls.exe`. Run it with `npm run test:goldens`.

The golden checks every row twice. First, the native items must equal an oracle: `formNameCompletion.js` (`analyzeFormNameCompletion` and `collectWorkspaceFormNames`) run through the host's item mapping in `client/out/extension.js`. Second, each row pins its expected labels. The cheap keyword-variable list must still be in the response.

## Sites

The slot must be a string literal. If the slot is empty or holds only whitespace up to the cursor, the item inserts `"name"` at the cursor. If the cursor is inside quotes, the item replaces the quoted content and keeps the quotes.

| Line (`\|` = cursor, otherwise at the end) | Items |
| --- | --- |
| `edit_obj_data(Obj,` / `ok = edit_jnt_data(conn, ` / `dbedit(1,` / `set_macro_form(Mac)` / `set_macro_form(Mac) ` | all four, wrapped in quotes |
| `edit_obj_data(Obj,"form` / `"form\|.vxf",0)` / `"form.vxf\|",0)` (on the closing quote) / `set_macro_form(Mac) "form` / `dbedit(1,'form` | all four, replacing the content |
| `edit_obj_data(Obj,\|"form.vxf",0)` (on the opening quote) / `edit_obj_data(Obj, \|Form$` | all four, wrapped at the cursor (copied from Node) |
| nested `foo(a,b)` as the 1st argument / `EDIT_OBJ_DATA` / `Dbedit` / `! edit_obj_data` / `;` inside quotes / `edit_obj_data (` / tabs / `.edit_obj_data(` / `Ok=edit_obj_data(` / `edit_obj_data(Obj,\|)` / empty 1st argument / `set_macro_form()` / unclosed `"foo\bar` / two commands on one line (the last wins) / non-ASCII and a surrogate pair before the command | all four |
| still in the 1st argument / just after `(` / cursor on `(` / `Form$` / `F\|orm$` / `ascii(65)` / 3rd argument / after a closed string / after the closing `)` | none |
| `set_macro_form(Mac` / cursor on `)` / inside the parentheses / after a closed after-paren string | none |
| `dbeditfrm` / `dbledit` / `xedit_obj_data` / `new_macro` / space-form `dbedit 1,` / cursor in a `;` comment / `x = 1; edit_obj_data(…` / empty line | none |

The site also holds on the third line of a CRLF buffer, for typed `"` and `)`, with no completion context, and with a cursor past the end of the line, which is clamped. A `plaintext` document gets no form items. `completionItem/resolve` returns the item unchanged.

## Items

Each item has kind File, `label` and `filterText` set to the file name with its extension, `detail` set to `custom/forms` or `system/forms`, `sortText` set to the name lowercased, and a `textEdit` over the replace range on the cursor's line. `insertText` equals the `textEdit` text.

## Listing

- `custom/forms/`: `alpha.vxf`, `notes.txt`, `shared.vxf`, `nested/skip.vxf`. `system/forms/`: `shared.vxf`, `system_only.vxf`.
- The result is `alpha.vxf`, `notes.txt`, `shared.vxf` (from Custom, which wins), and `system_only.vxf`. The `nested` directory and its file are not offered. `notes.txt` is offered because there is no extension filter.
- The same four names come back when the workspace root is the fixture, when the workspace folders are `custom` and `system`, and when the only folder is `custom` (System is then its sibling). A workspace without `forms/` directories gets no form items.
- A temporary workspace with a `customa` Custom folder checks the rest. Custom `Shared.VXF` beats System `shared.vxf`, and the Custom casing is kept. `D_part.vxm` and the extensionless `noext` are kept. A subdirectory is skipped. The sort order matches Node's `localeCompare` for `_`, `-`, `.`, digits, and `Ä`/`Å`, and `sortText` lowercases `Ä`. A file added after the server starts appears on the next request, because the listing is not cached.
