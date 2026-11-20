# Cheap hover and keyword-variable completion

Parity goldens for `textDocument/hover` and cheap `textDocument/completion` on the 20 ms path: keyword catalog, live-buffer extract, last include-neighborhood extract.

`test-fixtures/include-call-sections/` and `test-fixtures/form-name-completion/` pin other completion families and are not this list.

## Smoke

```bash
npm run test:goldens
```

## Files

- `custom/main.prg` — live-buffer names, keyword sites, `##included_from macros/inc.prg`
- `custom/macros/inc.prg` — included `baz` / `qux$`

Workspace root is this folder so Custom overlay resolves `macros/inc.prg`.

## Hover

| Position | MarkupContent |
|---|---|
| `askdim` | keyword `{documentation}\n\n---\n{detail}` |
| `foo` | `Variable:` fence with trailing space on `vertexadt `, comment `comment` |
| `bar$` | `String Variable:` fence, comment `s` |
| `korsregel` | `[area]` / `IGNORE TYPE` stripped to `korsregel layer` (trailing space allowed) |
| `pnt_inside_poly` | `![Description]({extensionUri}/ADT-documentation/pnt_inside_poly explainer.png)` prepended to keyword body |
| `baz` after neighborhood refill | included `Variable:` comment `from include` |
| plaintext / log | `null` |

Empty `vertexadt` hover stays `null`.

## Completion

Empty `vertexadt`: full keyword catalog (1336), first item `askdim` kind Keyword `data` 1, no prefix filter.

`completionItem/resolve` on `askdim` replaces documentation with markdown `{documentation}\n\n---\n{detail}`.

On `main.prg` with default snippets off: unique `foo` (docs from live comment), `bar$` kind Text, `baz` after refill, `qux$` without docs. No `if-structure`.

With `vertexadt.snippets.useExpandedSyntax` true: `askdim (snippet)` immediately after `askdim`, then `if-structure` / `loop-structure` before variables.
