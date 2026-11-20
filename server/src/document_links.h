#pragma once

#include "document_store.h"
#include "library_paths.h"

#include <lsp/types.h>

namespace vertexadt {

struct DocumentLinksRequest
{
  const Snapshot& snapshot;
  const LibraryLayout& layout;
};

// textDocument/documentLink. Ordinary LSP, complete targets, no resolve.
// Same three families on vertexadt, plaintext, and log. Missing document is
// handled by the caller. No matches → [].
[[nodiscard]] auto documentLinks(const DocumentLinksRequest& request)
  -> lsp::Array<lsp::DocumentLink>;

} // namespace vertexadt
