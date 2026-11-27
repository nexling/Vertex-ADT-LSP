#pragma once

// Typed iterator items (docs/adr/0030-typed-iterator-items.md).
//
// `It = new_iterator("TYPE", Obj, "TASK", "PARAM")` and
// `init_iterator(It) "TYPE", Obj, "TASK", "PARAM"` decide what
// `Item = next_item(It)` returns:
//
// - TASK literal `CONNECTIONS` / `CONNECTIONS_HC` -> joint, whatever TYPE is.
//   A TYPE literal other than `""` there is an error.
// - Otherwise a TYPE literal from server/catalog/iterator-object-types.json
//   gives its type. `""`, a non-literal and an unknown literal give no type;
//   an unknown literal is information ("will return all objects").
//
// Matching is case-sensitive for TYPE and TASK. Columns are byte offsets into
// the line given (a code line after stripValidationComment).

#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

inline constexpr std::string_view kIteratorConnectionsTypeCode = "vertexadt/iterator-connections-type";
inline constexpr std::string_view kIteratorUnknownTypeCode = "vertexadt/iterator-unknown-type";

enum class IteratorProblem
{
  None,
  ConnectionsMismatch, // Error on the TYPE literal
  UnknownType          // Information on the TYPE literal
};

struct IteratorSite
{
  std::string iterator;
  int iteratorColumn = -1;
  // What next_item on this iterator returns; empty: untyped.
  std::string itemType;
  IteratorProblem problem = IteratorProblem::None;
  std::string message;
  // The TYPE literal, quotes included, when problem is set.
  int problemStart = -1;
  int problemEnd = -1;
};

struct NextItemSite
{
  std::string target;
  int targetColumn = -1;
  std::string iterator;
  int iteratorColumn = -1;
};

// Every `X = new_iterator(...)` and `init_iterator(X) ...` on the line.
[[nodiscard]] auto findIteratorSites(std::string_view line) -> std::vector<IteratorSite>;

// Every `X = next_item(It)` on the line.
[[nodiscard]] auto findNextItems(std::string_view line) -> std::vector<NextItemSite>;

} // namespace vertexadt
