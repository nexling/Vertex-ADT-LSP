#pragma once

#include "document_store.h"

#include <lsp/types.h>

#include <vector>

namespace vertexadt {

// dbcreate field completion. Ordinary textDocument/completion adds these
// items on top of the cheap keyword-variable list. Local-only: this buffer
// from the continuation start through the UTF-16 cursor. Must not delay the
// cheap list. Empty when the cursor is not in a field slot.
[[nodiscard]] auto dbcreateFieldCompletionItems(
  const Snapshot& snapshot,
  lsp::Position position) -> std::vector<lsp::CompletionItem>;

} // namespace vertexadt
