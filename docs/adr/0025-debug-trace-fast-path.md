# Debug traces get their own fast path

Debug traces (`.dbg` execution dumps) were opened as `plaintext` or `log`. Hover and the debug-value lookup worked, but semantic tokens returned nothing. Opened as `vertexadt`, a trace went through the whole source-file pipeline first (section parse, live and neighborhood extracts, include and call resolution, type timeline) and then stopped at the `START OF DEBUG` line. A trace is one flattened execution with nothing to include or call, so all of that was wasted. Traces run to tens of megabytes (`_elcoupler_fix_pre_BUILD.dbg` is 20 MB, one `_floordetailing.dbg` run is 340 MB). The native server now treats a trace as its own kind of document, with a small pipeline that only reads the trace itself.

- **What is a trace.** A URI ending in `.dbg` (any case), or `START OF DEBUG in FILE|SECTION` within the first 4 KB. `DocumentStore` decides this on every open and change. `isDebugDump` now checks the head too instead of searching the whole text, which several callers did on every request. Over C:\vxbd320, no source file has the header past 4 KB, so nothing that was excluded before is validated now. `.dbg` is added to the `vertexadt` language.
- **One buffer.** The store keeps each buffer behind a `shared_ptr` (copy-on-write on edits). A trace hands that pointer to `DebugTraceStore`. Nothing else copies it: the workspace skips the linked-file copy, extracts, include neighborhood and indexing for traces, `openUris()` and `snapshotByNormalizedPath()` leave them out, and every handler checks `isDebugTrace` before taking a `Snapshot`. A 48.6 MiB trace sits at about 64 MB of server memory once open and indexed.
- **Line table, then index.** On open, `DebugTrace` builds a line-offset table with `memchr` (3 ms for 545 K lines). A single background worker then builds the assignment index: name → (line, value offset) in line order (40 ms for the same file). It parses each line with `parseLineAssignmentSpans`, a hand-written copy of the regex `parseLineAssignments`. `debug-trace-test` checks that the two give identical results on every line of the real traces. When the index lands, the server sends `workspace/semanticTokens/refresh`. A newer version cancels the old build.
- **Tokens.** `debugTraceSemanticTokens` lexes only the requested lines. `splitDebugLine` splits each one from the right: an optional `_word_` marker (kept only after a value), the last whitespace-preceded `<…>` value, then code. Code gets `keyword` (the grammar's control words), `function` (catalog commands and keyword labels), `string`, `number`, `comment` (`;!`, and a line-start `!`), and `variable` / `p|x|y|z|nvectorVariable` for `$` names and vector suffixes. Names in the index become `variable` once the index exists. Until then they are left uncolored, and range requests never wait for the index. `<value>` is `debugValue` and the marker is `debugMarker`. START/END banner lines are `debugMarker`, after skipping the BOM and the stray `0x7F` that real dumps start with. The seven new types are appended to the legend, so existing indices don't move. `package.json` declares `debugValue` and `debugMarker` as subtypes of `comment`.
- **Range tokens.** The server now advertises `semanticTokens/range`. For a trace, a range is answered on the message loop (well under 1 ms for a screen). For a source file, a range is the cached full result cut to those lines. VS Code stops asking for ranges once a full result arrives, even an empty one. The client middleware therefore throws `CancellationError` for full requests on traces over 2 M characters, which leaves the document without complete tokens, so it keeps requesting the visible range. Full requests on smaller traces are answered on the heavy pool.
- **Hover and definition.** `debugTraceHover` and `debugTraceDefinition` look the word up in the index (binary search on line). The rule and markdown are unchanged: names of 2+ characters that are not keyword labels, `**name** = \`value\``, plus `_Set on line N_`. A hover that comes before the index exists waits for it. A definition miss still falls through to the in-buffer declaration walk, but never to includes. Find References and document links keep their existing behavior on traces.
- **Everything else is off.** Diagnostics, completion, signature help, document highlight, code actions, code lenses and formatting answer empty for a trace without reading it.

## Measured

On a 48.6 MiB cut of the 340 MB trace (545 K lines), over LSP stdio with `scripts/lsp-golden-harness.js`:

- visible range, 120 lines: 0.2–0.7 ms (the first one, which includes the didOpen transfer, 113 ms)
- hover: 0.1 ms (39 ms for the first one, which waits for the index)
- definition and diagnostic pull: 0.1 ms
- full tokens, which the client blocks at this size: 1.1 s and 2.36 M tokens

## Considered Options

- **Optimize the shared slow path instead.** Rejected: most of it (includes, calls, type timeline) has no meaning for a trace, and changing it risks the source-file goldens.
- **Answer a declined full request with `null` or an empty result.** Rejected: VS Code marks the tokens complete and stops range requests.
- **Range-only tokens for every document.** Rejected: small traces and source files would lose whole-document tokens.
- **Traces over 50 MB.** VS Code does not sync files over 50 MB (`_MODEL_SYNC_LIMIT`) to extensions at all, so no request ever arrives for them. A command that opens part of a big trace as a virtual document under 50 MB is the likely next step. It is not part of this change.

## Consequences

- A buffer with the header in its head is a trace even when its extension is `.prg`. `test-fixtures/section-aware-types/custom/dump.prg` now gets trace tokens instead of stopping at the header (`scripts/goldens/30-semantic-tokens.js`).
- Plaintext or log buffers that are traces now get semantic tokens too.
- Asserted by `scripts/goldens/30-semantic-tokens.js` (legend, range capability, trace range tokens on `test-fixtures/debug-trace/sample.dbg`, source-file range = full cut to the range) and by `server/tests/debug_trace_test.cpp` (parser and hover parity, line splitting, timings on real traces passed as arguments).
