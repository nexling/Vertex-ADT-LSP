# findstr* value expressions (native only)

`findstr`, `findstri`, `findstrr` and `findstrri` return a 1-based character position (0 when not found). Real code does arithmetic on that position (`end = findstrri(panellabel$,"-")-1`, `start = findstri(label$,"-")+1`) and uses it as a truth value (`ok = findstri(f$,"walls@") or findstri(f$,"deck@")`). Before this change the native server, like Node, reported `findstrri accepts at most 0 parameters after parentheses` on both `-1` and `+1`. It typed every findstr* result as `boolean`, so in strict mode `abs(p)` after `p = findstr(...)` was an error, and it had no catalog entry for `findstrr`. The Node server is frozen (since 2026-11-18), so all of this is a native/Node divergence.

## Rule

- The catalog field `structure.valueExpression` (native only) is set on the four findstr* commands. All four also have `paramsOutside: 0` and `maxParamsOutside: 0`. Before this change only `findstrri` had them, so `findstr`/`findstri` accepted any text after `)`.
- Command syntax (`validate_command_syntax.cpp`) normally treats text after the call's `)` as parameters after parentheses. For a `valueExpression` command it is accepted when both of these hold:
  - the line is an assignment (`name =` / `name$ =`, not `==`) and the call is to the right of the `=`;
  - the remainder continues the value (`catalog::isValueExpressionTail`). That means one of:
    - `+ - * /` followed by a math expression (`isValidMathExpressionWithCommands`), either alone or together with the value text before the call (`e = (findstr(a$,b$)-1)`, `e = 2*(findstrri(a$,b$)+1)`);
    - a comparison (`<`, `>`, `=`, `<>`, `<=`, `>=`) or a whole-word `and` / `or` (any case), with something after it;
    - a `)` that closes a `(` opened before the call (`oktest = (findstri(a$,"-M-"))`, `oktest = !(findstri(a$,"-M-") and x)`);
    - a lone `;` (`Val = findstr(Flag$,F$(i));`).
- Still flagged with `<name> accepts at most 0 parameters after parentheses`: a bare statement (`findstrri(a$,b$)-1`), and a remainder that starts with a name, number, string or comma (`e = findstr(a$,b$) b$`). A dangling operator or connector (`e = findstrri(a$,b$)-`, `ok = findstr(a$,b$) and`) and `andb$` (not a whole word) are flagged too. A stray `)` with no matching `(` is left to bracket balance (`Mismatching parentheses`).
- `if` conditions do not go through command syntax, so `if findstr(a$,b$) then` and `if findstri(a$,b$)-1 > 0 then` were not affected before and are not affected now.
- `^` is not a math operator in the validator, so `findstr(...)^2` is still flagged.

## Catalog

`server/catalog/*.json` is harvested from the frozen Node `server.js` (`npm run harvest-catalogs`). The native changes are applied on top of that harvest in `scripts/harvest-catalogs.js`, following the keyword overrides from ADR 0003:

- `NATIVE_COMMAND_ADDITIONS`: `findstrr` is cloned from `findstrri`, and every whole-word `findstrri` in its messages becomes `findstrr`. The result is optional assignment, exactly two string-expression parameters inside the parentheses, and strict input types string/string. Its hover text was already in `keywords.json`. The harvest fails if Node ever gains `findstrr`.
- `NATIVE_STRUCTURE_OVERRIDES`: `paramsOutside: 0`, `maxParamsOutside: 0`, `valueExpression: true` for all four.
- `NATIVE_RETURN_TYPE_OVERRIDES`: `findstr`, `findstri`, `findstrr`, `findstrri` return `value`, not `boolean`.

The catalog now has 245 commands and 240 table validators. The counts in `main.cpp` and `assertHarvest` were updated to match.

The type timeline takes the command's return type and ignores what follows `)`, so `e = findstrri(a$,b$)-1` gives `e` the type `value`. The timeline itself did not change.

## Node divergences

Checked with the Node oracle (`node server/out/server.js --stdio`, host settings shape) on the same probe file:

| Line | Node | Native |
|---|---|---|
| `end = findstrri(p$,"-")-1` / `+1` | `findstrri accepts at most 0 parameters after parentheses` | no diagnostic |
| `w = findstrri(p$,b$) or findstr(p$,b$)` | same error | no diagnostic |
| `s = findstr(p$,b$) b$` | no diagnostic | `findstr accepts at most 0 parameters after parentheses` |
| `t = findstrr(p$)` / `u = findstrr(p$,b$,b$)` | no diagnostic (no catalog entry) | `findstrr requires at least 2 …` / `accepts at most 2 … inside parentheses` |
| strict: `p = findstr(p$,b$)` then `abs(p)` | `abs: input value 'p' should be of type [value] but is 'boolean'` | no diagnostic |
| `findstrri(p$,b$)-1` (bare) | error | error (same) |

## Corpus gate

C:\vxbd320 (884 `.prg` files, 1920 lines calling findstr*), strict mode both off and on: no findstr* "after parentheses" diagnostics, and no type messages involving `boolean` or `value`. A first version accepted only math tails. It produced 16 false positives on `findstr`/`findstri` lines using `and`/`or`, wrapping parentheses, `!(` … `)` and trailing `;`. Those shapes are why the comparison, connector, `)` and `;` rules above exist.

## Tests

- `server/tests/findstr_value_expression_test.cpp` (target `findstr-value-expression-test`): arithmetic and logic tails (including the corpus lines), cases that are still flagged, `if` conditions, the `findstrr` entry, and strict `value` types.
- `scripts/goldens/57-findstr-value-expression.js` is a native-only golden with no Node oracle. It runs with strict mode off and on.
