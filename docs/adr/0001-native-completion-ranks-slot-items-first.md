# Native completion ranks slot items above keywords

Slot completion families (dbcreate field, debug filename, include/call path and section, remaining param, call return value, form name, object parameter, macro library/code) used to be appended to the cheap keyword-variable list, so their items were sorted alphabetically among ~1,300 keywords. In the native server only, any response where a family adds items now ranks them first. Family items get `sortText` `0` + a family digit (the old append order) + their old sort key, or their label if they had none. ADR 0002 inserts a match-group character after the digit. Cheap items get `1` + their label. Responses with no family items are unchanged. Keywords stay visible because a slot may still need one. We considered "specials only" and rejected it: the server can't tell when the user has backed out of a slot, so hiding keywords would trap them.

This is a deliberate divergence from the Node server, which is frozen and still appends. Parity checks must not compare completion `sortText` with the Node oracle. The native contract lives in `server/src/completion_ranking.h` and is asserted by `scripts/completion-ranking.js`.

## Consequences

- VS Code ranks by fuzzy match first and uses `sortText` only to break ties. A keyword that matches the typed prefix clearly better can still rank above a special that matches poorly.
- Call return-value fires on the whole line after `Name$ = call(...)`, so its snippets lead anywhere on that line.
