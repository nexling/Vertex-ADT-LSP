# The signature box follows the cursor

VS Code opens the signature help box only when `(`, `,` or a space is typed, and a click closes it. Going back over the arguments of a finished `call(` showed nothing. The client (`client/out/signatureFollow.js`) now reopens it.

- **When**: 50 ms after a single cursor moves (click or key), for every command that has a box while typing. Text selections, multiple cursors and moves caused by edits are ignored. It is off when `editor.parameterHints.enabled` is off. The suggestion popup is still never opened by cursor movement (ADR 0021): it takes over arrow keys, Enter and Tab, and the signature box doesn't.
- **Which command**: `server/src/signature_help.cpp` walks the brackets of the logical line (physical lines joined by a trailing `\`, by the bracket check's rules). The innermost command that has something to show for the cursor's argument wins, so after an inner `)` the outer command's box returns. Commas in either quote type and in nested parentheses or brackets aren't counted. This replaces the Node port's "last `name(` before the cursor".
- **Nothing to show**: no box past the last argument, in `;!` and line-start `!` comments, on an assignment to a name that is also a command (`cube = 1`), or for a `call(` whose callee is missing or has no inputs. Unnamed catalog parameters (`param1`) do show.
- **Path**: `call(` reads its path past the cursor, so the box also opens inside the path.
- **Comments**: each `call(` input carries the callee's comment as parameter documentation, in the words of remaining-param completion.
- **Escape**: a keybinding for `parameterHintsVisible` closes the box and remembers the command through `vertexadt/signatureHelpSites`, which returns the name positions of the enclosing commands. The box stays closed while that command still encloses the cursor, or until `(`, `,` or a space is typed. With the suggestion popup open, Escape closes the popup first.
