#pragma once

// Indentation formatter: textDocument/formatting and rangeFormatting
// (docs/adr/0013-native-indentation-formatter.md). Rewrites only leading
// whitespace, as spaces, from if/else/endif and loop/endloop nesting. Refuses
// the whole document (no edits) on any block imbalance.

#include <lsp/types.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

struct IndentRefusal
{
  // 0-based line the message names.
  int line = 0;
  // Without the "not formatted" prefix, e.g. "unmatched `endif` at line 7".
  std::string reason;
};

struct IndentPlan
{
  // Target leading-space count per line; empty when refused.
  std::vector<int> columns;
  std::optional<IndentRefusal> refusal;
};

struct IndentFormatResult
{
  std::vector<lsp::TextEdit> edits;
  std::optional<IndentRefusal> refusal;
};

// `indentSize` columns per level (clamped to >= 1). Tabs in leading
// whitespace count as `indentSize` columns.
[[nodiscard]] auto planIndentation(std::string_view text, int indentSize) -> IndentPlan;

// Edits for lines in [firstLine, lastLine] (inclusive) whose leading
// whitespace differs from the plan. Depth always comes from the whole text.
[[nodiscard]] auto indentationEdits(
  std::string_view text,
  int indentSize,
  int firstLine,
  int lastLine) -> IndentFormatResult;

// Whole document.
[[nodiscard]] auto indentationEdits(std::string_view text, int indentSize) -> IndentFormatResult;

// The full rewritten text (for tests and the corpus gate); nullopt when refused.
[[nodiscard]] auto formatIndentation(std::string_view text, int indentSize)
  -> std::optional<std::string>;

// "Not formatted: <reason>." for window/showMessage.
[[nodiscard]] auto indentRefusalMessage(const IndentRefusal& refusal) -> std::string;

} // namespace vertexadt
