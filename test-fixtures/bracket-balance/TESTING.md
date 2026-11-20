# Unbalanced parentheses and square brackets (native only)

See `docs/adr/0006-native-bracket-balance-check.md`.

## Automated

- Unit: `cmake --build server/build --config Release --target bracket-balance-test`, then `server\build\Release\bracket-balance-test.exe`.
- Golden: `scripts/goldens/52-bracket-balance.js` (part of `npm run test:goldens`).

## Manual check in the extension

Open `bracket_balance.prg`. Each line below gets exactly one red underline from the bracket check:

| Line | Message | Underlined |
|---|---|---|
| 2 `ret$=call(…,Panel_Label$` | `Mismatching parentheses for 'call'` | from `(` to the end of the line |
| 5 `x=(a+b` | `Mismatching parentheses` | `(a+b` |
| 6 `y=b)+1` | `Mismatching parentheses` | only the `)` |
| 7 `ret$=call("x",f(a,b` | `Mismatching parentheses for 'f'` | `(a,b` |
| 8 `z=layer[a` | `Mismatching brackets for 'layer'` | `[a` |
| 9 `w=abs(a[1)]` | `Expected ']' but found ')'` | `[1)` (no extra `abs` parameter error) |
| 12 `if (a=1 then` | `Mismatching parentheses` | `(a=1 then` |

Lines 10, 11 and 14–15 get no bracket error. The brackets on line 10 sit in a string and after a real `;!`, line 11's are inside strings, and the `dbcreate(` on 14 closes on its `\` continuation line 15.
