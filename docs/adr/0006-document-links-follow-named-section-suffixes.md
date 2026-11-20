# Document links follow named-section suffixes (native only)

`ret$=call("$DIR_MACROS/addtimerforactpanel.prg AFTERMPRFLOOR",panellabel$)` gave no link: the whole quoted text was treated as the file path, and no file has that name. The native server now strips a named-section suffix before resolving the file and adds `#L<n>` pointing at the section. The Node language server is frozen (since 2026-11-18) and still gives no link here, so this is a deliberate native/Node divergence. Code is in `server/src/document_links.cpp`. The suffix parser is `splitIncludeCallTarget` in `server/src/text_util.cpp`.

## Rule

- Every form the glossary counts as a named-section suffix gets a section link:
  - `call("path SECTION", …)` and `x$=call("path SECTION")`: section inside the quotes.
  - `call path SECTION`: bare ident after the path (before this change it linked the file, not the section).
  - `# include "path SECTION"`: section inside the quotes.
  - `# include path SECTION` at line start: unchanged.
- The whole token is resolved first, so a link that resolved before never changes. Only when that fails is the path resolved again with the quoted suffix cut. A section written after the path wins over one inside the quotes.
- `SECTION` / `SECTIONOFFILE` inside quotes or after `call path` are not suffixes. `"path SECTION"` still links the file, with no fragment. After `# include path` at line start, any ident still counts (Node behaviour, pinned by golden 31), so `SECTION` there jumps to a `#SECTION#` marker.
- Matching is unchanged: the first line containing `#NAME#`, ignoring case. A prefix (`AFTERMPR` for `#AFTERMPRFLOOR#`) does not match, unlike the unknown-section check, because a prefix can match several sections.
- If the section is not found, the link still opens the file at the top. The unknown-section warning flags the bad name.
- The link range is the path token as captured today: the whole quoted string when the suffix is inside the quotes, just the path when the suffix sits after it.
- The tooltip is `Open <path> #NAME#` when the section is found, and `Open <path>` otherwise. Debug-dump header links keep `Open <path>`.

## Not done

- The validators (unknown-section, call-param, callee-common, unclosed-resources, workspace index) still keep their own copies of `parseIncludeCallPathAndSection`. Moving them onto `splitIncludeCallTarget` is a follow-up refactor.
- `##included_from` paths do not take a section.

## Tests

- `server/tests/document_links_test.cpp` (target `document-links-test`) covers the parser, all four forms, a missing section, the prefix case, the `SECTION` keyword and a missing file.
- `scripts/goldens/31-document-links.js` adds the quoted `call(`, `call` space and quoted `# include` forms and a missing section, and checks the new tooltip. These cases are native-only, with no Node oracle.
