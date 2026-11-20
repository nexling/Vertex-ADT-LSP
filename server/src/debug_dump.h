#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vertexadt {

struct DebugAssignment
{
  std::string value;
  int line = 0;
};

[[nodiscard]] auto parseLineAssignments(std::string_view lineText)
  -> std::unordered_map<std::string, std::string>;

[[nodiscard]] auto findMostRecentDebugValue(
  std::string_view text,
  int hoverLine,
  std::string_view varName) -> std::optional<DebugAssignment>;

} // namespace vertexadt
