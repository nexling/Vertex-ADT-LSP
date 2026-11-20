# Vertex Dbase Converter helper

`custom/complibs/macro_sys/` is a real Custom macro library database copied from a Vertex V32 install (`d_MACRO_SYS`, its `c`/`l` companions, and the sidecar XML the helper wrote on 2026-10-06). Object-parameter completion and macro library/code completion reuse it for their goldens.

## Spawn plumbing

```
cmake --build server/build --config Release --target vertex-dbase-converter-test
server\build\Release\vertex-dbase-converter-test.exe
```

The test target is `EXCLUDE_FROM_ALL` and is never shipped. `fake-dbase-converter.exe` stands in for the helper. It is copied as `Vertex_Dbase_Converter.exe` into an extension root whose path contains a space and `ü`, and the first word of each database file picks its behaviour.

| Area | Assertion |
| --- | --- |
| Lookup | `extensionUri` (percent-encoded, UTF-8) joins to `Vertex_Dbase_Converter.exe` at the extension root. An empty or invalid URI, or a missing file, is `HelperMissing`, gets logged, and writes nothing. |
| Spawn | argv is exactly `[databasePath, --all, --xml, --export, xmlPath]`, cwd is the helper's directory, there is no console window, and exit 0 is `Exported` with nothing logged. |
| Failure | A non-zero exit is `Failed`, and the log reads `Error exporting macro XML for <db>: exit <n>: <stdout+stderr>`. |
| Cancel | A cancel while the child runs returns promptly, kills the child, and deletes the half-written sidecar. A waiter canceled before it gets the slot never spawns. |
| One child | Two concurrent exports both complete, and their child lifetimes never overlap. |
| Lifetime | When the parent exits without cleanup, the Job object kills the helper child. |
| Real helper | The repo-root `Vertex_Dbase_Converter.exe` exports a temp copy of `d_MACRO_SYS` to XML starting with `<?xml` and containing `<Table Name="MACRO_SYS"`. A missing database is `Failed` with `exit 1: Error: Path does not exist`. |
