#pragma once

#include "text_util.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace vertexadt {

// Vertex rejects whitespace touching a concatenation `&`: `a$&" "&b$` is valid,
// `a$ &" " &b$` is not. Native only; the Node language server still accepts it
// (docs/adr/0005-concat-ampersand-no-whitespace.md).

inline constexpr std::string_view kConcatWhitespaceCode = "vertexadt/concat-whitespace";
inline constexpr std::string_view kConcatWhitespaceMessage =
  "Remove spaces around '&': string concatenation must not contain whitespace";

// One offending `&` with the spaces/tabs directly before and after it.
// [start, end) is the span to replace with "&".
struct ConcatWhitespaceSpan
{
  std::size_t startByte = 0;
  std::size_t endByte = 0;
};

// Spans in one line's code. Skips `&` inside `"…"` / `'…'`, `&&`, and anything
// from the first unquoted `!` (covers `;!` and inline `!` comments).
[[nodiscard]] auto findConcatWhitespaceInLine(std::string_view line)
  -> std::vector<ConcatWhitespaceSpan>;

// LSP range of one span (UTF-16 characters).
struct ConcatWhitespaceIssue
{
  int line = 0;
  std::size_t startCharacter = 0;
  std::size_t endCharacter = 0;
};

// Every span in a document, skipping whole-line `!` comments, `##` directive
// lines, `#NAME#` section markers, `@nodebug` ranges and verification-ignored
// sections. Callers skip validation-excluded documents (debug dumps) themselves.
[[nodiscard]] auto findConcatWhitespaceIssues(
  const std::vector<std::string_view>& lines,
  const std::vector<NamedSection>& sections) -> std::vector<ConcatWhitespaceIssue>;

} // namespace vertexadt
