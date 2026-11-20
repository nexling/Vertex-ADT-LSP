# Native trigger characters open family sites only

The native server registers ` `, `,`, `(`, `/`, `"` and `)` as completion trigger characters. These are needed for the slot families: `call ` lists sections, `,` lists remaining parameters, `"` lists form names, and so on. Every trigger used to return the full keyword-variable list as well, so a popup of keywords opened after every space you typed. In the native server only, a request made by a typed trigger character (`triggerKind` 2 with a `triggerCharacter`) now returns an empty, complete list unless a completion family adds items at the cursor. The keyword-variable list then opens on the first word character typed, through VS Code's quick suggestions (which are on by default and need no setting).

- **Family site with items**: unchanged. Family items rank above the keyword-variable list (ADR 0001), so typing on in the slot doesn't make the popup jump from family items only to everything.
- **Family site with no items** (an empty directory, all parameters already passed, an empty `complibs/`): no popup, the same as no site.
- **Unchanged**: Ctrl+Space (`triggerKind` 1), re-requests for an incomplete list (`triggerKind` 3) and requests without a context still get the full list.

This is a deliberate divergence from the frozen Node server. It is implemented in the completion handler in `server/src/main.cpp` and asserted by `scripts/goldens/53-trigger-character-family-only.js`.

## Considered Options

- Show only family items on a trigger and bring the keyword list back on the next keystroke (`isIncomplete`). Rejected: the popup would jump from a few items to the whole catalog after one letter.
- Drop ` ` from the trigger characters. Rejected: the space-started families (include/call sections, debug filename, dbcreate fields) would then need Ctrl+Space.

## Consequences

- A request with `triggerKind` 2 but no `triggerCharacter` isn't treated as a trigger character. VS Code never sends one, but older goldens use it to model typing.
