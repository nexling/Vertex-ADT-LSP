#pragma once

#include "document_store.h"

#include <lsp/types.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Port of Node `CleanUpLineFunc(lineText)`. Returns the rebuilt line, or
// nullopt where Node returns null (no cleanupRules) or would throw (parse
// miss, unknown command, missing quoted after-paren slot).
[[nodiscard]] auto cleanUpLine(std::string_view lineText) -> std::optional<std::string>;

// Newline cleanup on textDocument/onTypeFormatting. Only `ch == "\n"` in a
// `vertexadt` buffer; rewrites the previous line (position.line - 1) as one
// whole-line TextEdit. Empty when there is no rewrite or it is an identity.
[[nodiscard]] auto newlineCleanupEdits(
  const Snapshot& snapshot,
  lsp::Position position,
  std::string_view ch) -> std::vector<lsp::TextEdit>;

} // namespace vertexadt
