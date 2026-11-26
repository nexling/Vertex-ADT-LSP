# Debug trace fixture

`sample.dbg` is 102 lines from a real `_floordetailing.dbg` run. It has the first 40 lines, then up to six lines from each kind: banners, if-then assignments, `<true>`/`<false>` ifs, multi-output commands, paren outputs, truncated `...` lines, loop counters, string assignments, block markers, `<` in code, lines without a value, and non-ASCII lines. Line 0 keeps the BOM and stray `0x7F` byte that real dumps start with. See [ADR 0025](../../docs/adr/0025-debug-trace-fast-path.md).

| Request | Expect |
| --- | --- |
| range 0–10 | the `START OF DEBUG` banner is `debugMarker` |
| `  timer = 1   <1>` | `timer` variable (once the index lands), `1` number, `<1>` debugValue |
| `endif   <> _if_end_` | `endif` keyword, `<>` debugValue, `_if_end_` debugMarker |
| `P = 1   <1>` | `P` pvectorVariable |
| range 50–end | identical to the full result's tokens on those lines |
| diagnostic pull | no items |

`server/tests/debug_trace_test.cpp` also runs on this file: parser parity with `parseLineAssignments` on every line, and the index against `findMostRecentDebugValue`.
