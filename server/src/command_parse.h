#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Ports of Node's command-line parse helpers (`server/out/server.js`).
struct ParsedCommand
{
  std::string functionName;
  std::vector<std::string> insideParams;
  std::vector<std::string> afterParams;
};

// Index of the `)` matching `(` at `openParenIndex`, skipping `"…"`; -1 if none.
[[nodiscard]] int findMatchingParenIndex(std::string_view text, int openParenIndex);

// Node `parseParameters(paramString)` with `acceptSpacesAsSeparators` false.
[[nodiscard]] auto parseParameters(std::string_view paramString) -> std::vector<std::string>;

// Node `parseCommand(command, command, false)`; the assignment is parsed and dropped.
[[nodiscard]] auto parseCommand(std::string_view command) -> std::optional<ParsedCommand>;

} // namespace vertexadt
