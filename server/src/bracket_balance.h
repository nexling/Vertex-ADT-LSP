#pragma once

#include "text_util.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Unbalanced `(` `)` and `[` `]` on a code line. Native only; the Node
// language server is frozen and does not report them
// (docs/adr/0006-native-bracket-balance-check.md).

inline constexpr std::string_view kBracketBalanceCode = "vertexadt/bracket-balance";
inline constexpr std::string_view kBracketBalanceSource = "Syntax exception - ADT Plugin";

// One problem per logical line (physical lines joined by a trailing `\`).
// Positions are UTF-16 characters; the range may span lines only for a wrong
// closer whose opener sits on an earlier continued line.
struct BracketIssue
{
  int startLine = 0;
  std::size_t startCharacter = 0;
  int endLine = 0;
  std::size_t endCharacter = 0;
  std::string message;
};

// Every issue in a document. Skips blank lines, whole-line `!` comments, `##`
// directive lines, `#NAME#` section markers and verification-ignored sections.
// Ignores brackets inside `"…"` / `'…'` and after an unquoted `;!` comment. A
// bare `!` is logical NOT, so it is code. Callers skip validation-excluded
// documents themselves.
[[nodiscard]] auto findBracketIssues(
  const std::vector<std::string_view>& lines,
  const std::vector<NamedSection>& sections) -> std::vector<BracketIssue>;

} // namespace vertexadt
