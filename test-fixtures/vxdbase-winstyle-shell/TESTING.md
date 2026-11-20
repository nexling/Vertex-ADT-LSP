# vxdbase_winstyle shell files

## Smoke test

```bash
node test-fixtures/vxdbase-winstyle-shell/run-vxdbase-shell-test.js
```

## Manual check in the extension

1. Open `shell_no_sections.prg` — no validation squiggles (even if code like `X = 1 and Y or Z` would normally error).
2. Open `with_sections.prg` — validation should still run (e.g. `and`/`or` mix at root may report precedence error).
3. Open `normal_no_marker.prg` — validation runs as usual.

Excluded shell files are also omitted from workspace index rebuild and workspace diagnostics export.
