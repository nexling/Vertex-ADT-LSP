# Native string variables used before declaration

The generic used-before-declaration pass skipped every name followed by a `$` anywhere later on the line. String variables were never checked outside `call(...)` arguments, and neither were numeric names that came before one. `if makes3 = 1 and findstri(code$,"x95") then` passed while only `Code$` was declared. The native server now checks every `name$` use in the generic pass, and "used before declaration" is an Error.

- **What is a use.** A `name$` token: a letter first, then `[A-Za-z0-9_]`, then `$`. These are not uses: text in `"…"` or `'…'`, anything between the first and last `#` on the line, and a name right after a `$` (`$DIR_MACROS$`). Names containing `RETURN` and keyword-catalog labels (with or without the `$`) are skipped, the same as the numeric path. `stringUsageMatches` in `server/src/validate_used_before.cpp` finds them.
- **What is a declaration.** The live-buffer and include-neighborhood `stringVariablesSet`, compared case-sensitively. The extract's string matcher now also takes a string array element: `X$(i) =` and `X$() = 5470`. That mirrors what `X(i) =` already did for numeric names, and it records an assignment site on the same statement-start rule.
- **Case hint.** When the exact name is missing but a declared string name differs only in case, the message ends with `(did you mean Code$?)`. The `call(...)` argument path gets the same hint for `name$`. Numeric names get no hint.
- **Used before assignment.** A declared `name$` is also checked against `stringAssignmentSites`, behind `vertexadt.variables.warnUsedBeforeAssignment` (ADR 0009). The `X$ =` target is not a read of itself. `s$ = s$&"a"` with no earlier `s$ =` is flagged.
- **Unchanged.** Lines that skip the generic pass (`get(`, `call`, `dbcreate`, `dbalter`, `messagebox`, `charset`, `# include`, `function`) still skip it, so `X$ = get(code$)` is not checked. Numeric names before a `$` are still hidden. Dropping that rule is a separate change with its own corpus review.

## Severity

Every "used before declaration" is now an `ADT-Syntax` **Error**: generic numeric and string names, `call(...)` arguments, and the `dbcreate(id)` id. A name that doesn't exist is a bug. "Used before assignment" stays a Warning, because it is a heuristic with its own setting. "Keyword … exceeds maximum length of 16 characters" also stays a Warning. There is no setting to bring the Warning back.

## Corpus gate

Over C:\vxbd320, with the workspace root C:\vxbd320 (customlbab plus system, 442 `.prg` files) and the problem cap raised, the before/after diff removes nothing and adds 34 diagnostics. All of them were reviewed:

- 29 are strings that are never assigned in the file or its include neighborhood. For example, `set LOCATION, loc$` in two `*_adapt.prg` triggers, `set_macro_param(Macro) "HEIGHT",Hgt$,1`, `if cmp(code$,"S15")` with no `code$`/`Code$` anywhere, and `height$`, whose only assignment is commented out. `PR_DB_OPEN_ERROR$` / `PR_DBUSER_OPEN_ERROR$` in AccDoc/AccRight are among these. If they are built-in system texts, they need to go into the keyword catalog.
- 5 are "used before assignment": reads in a lookup loop before the append further down (`Dates$(d)`, `Mater$(j)`), and one error message that reads `Value$` before line 502.

With the custom tree as the root (the index only covers the first custom tree under C:\vxbd320), customukraine adds the same 19 hits as its customlbab copies. customvbc (233 files) adds 55:

- 33 never-assigned strings: `HLflrhngr1$`…`flrhngr5$`, `hdrout$`, `activehanger1$`…`activehanger5$` in spanspacing.prg, `Mat$`, `BaseLabel$`, `Offset1$`, `projname$`, `DirPrograms$`, and `DirHome$` / `taskResultFile$`, whose assignments are commented out.
- 1 is `PdfPenFileAcad$` in PDMfilegen.prg (the system copy is among the 29 above), which `system/sysprogs/PDMsetup.txt` assigns through `# include $DIR_SYSPROGS/PDMsetup.txt`. Names assigned in a `# include`d child are not part of the extract. That gap already existed for numeric names: the baseline already flags `PdfPIPenSet`, `SheetLimTol` and three more from the same file. It now shows as an error.
- 21 are "used before assignment": cross-section reads in designnotes.prg (assigned only in `#VARIABLECOLLECTION#`), and self-appends such as `errors$ = errors$&…` / `mes$ = mes$&…` with no earlier assignment.

The first run gave 645 hits. 611 came from string arrays (`Type$(R0)="…"`, `core_t$() = 5470`, `L$(i) = read(1)`) that the extract did not record. That led to the array-element rule above.

## Considered Options

- Error only for the new string hits. Rejected: the same typo would show up red or yellow depending on whether the name ends in `$`.
- A setting for the severity. Rejected for now. We come back to it if the numeric hits turn out to be too noisy as errors.
- Removing the later-`$` rule for numeric names in the same change. Deferred: it is a separate corpus review.

## Consequences

- Every existing "used before declaration" turns from yellow to red. `scripts/goldens/oracle-tapes/47.json` (a recording of the frozen Node server) was edited so its 21 such CSV rows read `error`, and its counts moved to match. Nothing else in the tape changed.
- Existing false positives in this family turn red as well. The known one is names assigned in a `# include`d child file. Fixing it means adding include children to the extract, which is a separate change.
- String arrays are now string variables in the extract, so they show up in hover, completion and the other consumers of `stringVariables`.
- Asserted by `scripts/goldens/16-used-before.js`.
