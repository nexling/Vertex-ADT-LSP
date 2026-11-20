# Remove the Node language server

The Node language server (`server/out/server.js` and its helper modules) has been frozen since 2026-11-18, and every change since then went into the native server only. On 2026-11-20 it was removed. The language-server executable is now the only language server. This supersedes the "frozen Node server" wording in ADRs 0001–0017. Those ADRs are left as written, as history.

## Decisions

- **No implementation setting.** `vertexadt.languageServer.implementation`, the `VERTEXADT_LS_IMPLEMENTATION` launch override, the reload prompt and the `vertexadt.nativeServer` context key are gone. The extension host always launches `server/vertex-adt-ls.exe` over stdio. Every `custom/*` provider that only ran for Node (hover, completions, document links, newline cleanup, `custom/updateSettings`, and so on) was deleted from `client/out/extension.js`, along with the helpers only they used. Host commands always go through `workspace/executeCommand`.
- **No fallback.** If the executable is missing, the extension shows an error that names the path and says only Windows x64 is supported. No language client starts. `npm run package` builds a `win32-x64` VSIX, so the extension isn't offered on other platforms.
- **Native is the source of truth.** Nothing compares against a live Node process anymore. Goldens that used Node as their oracle replay its recorded answers instead (`scripts/oracle-tape.js`, `scripts/goldens/oracle-tapes/`). All of them passed against native when the answers were recorded.
  - 41–47: the Node session's request results, in call order, one tape per oracle session. Paths are stored as anchor tokens and respelled for each run's temp directories. Files Node wrote (the 47 CSV exports) are stored too.
  - 36–39: results of the pure Node helpers (`targetSectionSupport`, `formNameCompletion`), keyed by function and normalized arguments.

  A golden that changes its oracle calls fails with a pointer to assert native output directly. The tapes can't be re-recorded.
- **Catalogs are hand-maintained JSON.** `server/catalog/*.json` is the source of truth. `npm run generate-catalogs` (`scripts/generate-catalog-cpp.js`) regenerates `server/src/generated/` from it. The native overrides that `harvest-catalogs.js` used to apply on top of the Node harvest (ADRs 0003, 0011, 0016) are already in the committed JSON, and regenerating reproduced the C++ exactly. Correct a catalog entry by editing the JSON.
- **Deleted tests.** The `test-fixtures/` runners that loaded or sliced Node code (ascii-string-expression, debug-then-assignment, empty-comma-slots, form-name-completion, include-call-sections, unclosed-resources) and the generic LSP-sample e2e suite (`client/out/test`, `client/testFixture`, `scripts/e2e.sh`). Their behaviour is covered by the native goldens.

## Consequences

- The native/Node parity effort (`scratch/native-node-parity-gaps/`) was dropped with its open diff tickets unrun, and the folder was deleted. To run Node again, check out the "Snapshot before Node removal" commit, the first commit in the repository.
- `npm test` runs the golden harness.
