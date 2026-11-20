#pragma once

#include "document_store.h"
#include "extracts.h"

#include <lsp/types.h>

namespace vertexadt {

struct CheapCompletionContext
{
  const Snapshot& snapshot;
  const ExtractStore& extracts;
  bool useExpandedSyntax = false;
};

[[nodiscard]] auto cheapCompletion(const CheapCompletionContext& context)
  -> lsp::TextDocumentCompletionResult;

[[nodiscard]] auto resolveKeywordCompletion(lsp::CompletionItem item) -> lsp::CompletionItem;

} // namespace vertexadt
