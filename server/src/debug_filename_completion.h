#pragma once

#include "document_store.h"

#include <lsp/types.h>

#include <vector>

namespace vertexadt {

// Debug-filename completion. Ordinary textDocument/completion adds one File
// item on top of the cheap keyword-variable list when the UTF-16 line prefix
// matches /^\s*debug\s+$/. Local-only: this buffer plus the document
// basename. Must not delay the cheap list. Empty when not a site.
[[nodiscard]] auto debugFilenameCompletionItems(
  const Snapshot& snapshot,
  lsp::Position position) -> std::vector<lsp::CompletionItem>;

} // namespace vertexadt
