#pragma once

#include "document_store.h"
#include "library_paths.h"

#include <lsp/types.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace vertexadt {

// Remaining-param completion. Ordinary textDocument/completion adds the
// callee's remaining `name = get($IDENT)` left-hand names after a trailing
// comma on parenthesized `call(`. Heavy: resolve plus linked-file snapshot.

struct RemainingParamSite
{
  // Host regex group 1, JS-trimmed (UTF-8).
  std::string firstArgument;
  // Naive comma count of the line prefix minus one.
  int paramPosition = 0;
};

// Cheap, on the message loop: vertexadt and the UTF-16 line prefix matches
// the host regex
// /(?:[A-Za-z][A-Za-z0-9_]*\$?\s*=\s*)?call\s*\(\s*((?:"[^"]+"|'[^']+'|[^,)]+))(?:,\s*[^,]*)*,\s*$/i
[[nodiscard]] auto remainingParamSite(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<RemainingParamSite>;

// The family's items (never the cheap list). Empty when the callee does not
// resolve, has no snapshot text, or the slot is past its last row.
[[nodiscard]] auto remainingParamCompletionItems(
  const RemainingParamSite& site,
  const LibraryLayout& layout,
  const std::function<std::optional<std::string>(const std::filesystem::path&)>& textForPath)
  -> std::vector<lsp::CompletionItem>;

} // namespace vertexadt
