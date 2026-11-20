# Header Creator

## Smoke test

```bash
node test-fixtures/header-creator/run-header-creator-test.js
```

## Manual check in the extension

1. Set **Vertex ADT: Header: Username** (`vertexadt.header.username`) in settings (e.g. your name).
2. Open a `.prg` file with an old star-banner header (see `with_old_header.prg`).
3. Run **Vertex ADT: Create File Header** from the command palette.
4. Confirm the old header is replaced with the 4-line template:
   - Line 2: `! {FILENAME} / {YYYY-MM-DD} / {username}`
   - Line 3: `! "leave empty for user manual input"`
5. Open `with_invalid_middle.prg` and run the command — new header should be **prepended**, old block kept.
6. Open `no_header.prg` and run the command — header should be inserted at the top.
