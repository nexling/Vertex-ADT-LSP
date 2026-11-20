# Debug-dump hover after `if … then VAR =`

Parity goldens for cheap hover on a `vertexadt` debug dump. The language-server executable parses dump assignment lines (standalone `VAR =`, `if` / `else if … then VAR =`, function output-in-parens) and returns MarkupContent `**{word}** = `{value}`` for the most recent write at or before the hover line.

The Node slice-test `run-debug-then-assignment-test.js` stays Node regression only.

## Smoke

```bash
npm run test:goldens
```

## Cases

Open a `vertexadt` buffer whose first line is `START OF DEBUG in FILE`, then the dump line under test. Hover the written name on that line.

| Dump line | Hover word | MarkupContent `value` |
|---|---|---|
| `CTopZ = -1000000    <-1e+06>` | `CTopZ` | `**CTopZ** = `-1e+06`` |
| `if TZ > CTopZ then CTopZ = TZ    <5127.43>` | `CTopZ` (then-write) | `**CTopZ** = `5127.43`` |
| `if TZ > CTopZ then CTopZ = TZ    <true> _if_begin_` | `CTopZ` | `null` (bare true skipped) |
| `if TZ > CTopZ then CTopZ = TZ    <false>` | `CTopZ` | `null` |
| `if TZ > CTopZ then CTopZ = TZ    <true> _if_begin_ <5127.43>` | `CTopZ` | `**CTopZ** = `5127.43`` |
| `if A = B then CC = D    <100>` | `CC` | `**CC** = `100`` (dump hover needs word length ≥ 2) |
| `else CTopZ = TZ    <5127.43>` | `CTopZ` | `null` (`else VAR =` without `then`) |
| `else if TZ > CTopZ then CTopZ = TZ    <5127.43>` | `CTopZ` | `**CTopZ** = `5127.43`` |
| `Flag = TRUE    <true>` | `Flag` | `**Flag** = `true`` |
| `IF TZ > CTopZ THEN CTopZ = TZ    <5127.43>` | `CTopZ` | `**CTopZ** = `5127.43`` |
| `if X then Name$ = "A"    <"A">` | `Name$` | `**Name$** = `"A"`` |
| `vec_sub(NSP) SidPceSP,WP                     <-3300,254,2259.5>` | `NSP` | `**NSP** = `-3300,254,2259.5`` |
| `if a then if b then XX = Y    <9>` | `XX` | `**XX** = `9`` |

A later line that only names a previously written variable uses the other-line template `_Set on line {n}_`.
