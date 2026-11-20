#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Vertex strings are 1-indexed: `A$[3,6]` is characters 3–6, and a 0 end means
// "to the end" (`A$[P+11,0]`). A start below 1 is a bug, and assigning into a
// substring (`A$[I,I]="T"`) is not allowed. Only literal integers are checked;
// variables and expressions are runtime values. Native only; the Node language
// server does not check substrings (docs/adr/0006-substring-index-validation.md).

inline constexpr std::string_view kSubstringStartIndexCode = "vertexadt/substring-start-index";
inline constexpr std::string_view kSubstringEndIndexCode = "vertexadt/substring-end-index";
inline constexpr std::string_view kSubstringEmptyRangeCode = "vertexadt/substring-empty-range";
inline constexpr std::string_view kSubstringAssignmentCode = "vertexadt/substring-assignment";
inline constexpr std::string_view kSubstringSource = "Syntax exception - ADT Plugin";
inline constexpr std::string_view kSubstringStartFixTitle = "Use 1-based start index";

enum class SubstringIssueKind
{
  StartBelowOne, // Error, range: start argument
  NegativeEnd,   // Warning, range: end argument
  StartAfterEnd, // Warning, range: text between `[` and `]`
  Assignment,    // Error, range: `name$[...]`
};

// One problem in one line. [startByte, endByte) is the diagnostic range.
struct SubstringIssue
{
  SubstringIssueKind kind = SubstringIssueKind::StartBelowOne;
  std::size_t startByte = 0;
  std::size_t endByte = 0;
  std::string message;
};

// Issues in one line of code. Matches `identifier$[a,b]` with exactly two
// top-level arguments (nested `[]` / `()` and quotes respected), outside
// `"…"` / `'…'` and before the first unquoted `!`. Inner substrings inside an
// argument are checked on their own.
[[nodiscard]] auto findSubstringIssuesInLine(std::string_view line) -> std::vector<SubstringIssue>;

[[nodiscard]] bool isSubstringCode(std::string_view code);

} // namespace vertexadt
