#pragma once

#include "document_store.h"
#include "extracts.h"
#include "library_paths.h"
#include "linked_files.h"

#include <lsp/types.h>

#include <functional>
#include <memory>

namespace vertexadt {

struct DocumentPrep;

struct SemanticTokensRequest
{
  const Snapshot& snapshot;
  bool semanticHighlightingEnabled = true;
  bool debugValidationCommands = false;
  bool strictModeEnabled = false;
  ExtractStore* extracts = nullptr;
  LinkedFileStore* linkedFiles = nullptr;
  const LibraryLayout* layout = nullptr;
  TextLookup textForPath;
  std::function<void()> throwIfCanceled;
  // Shared URI+version prep (the editor pull's cache). Empty: build it here.
  std::function<std::shared_ptr<const DocumentPrep>()> prepare;
};

// Advertise full only; legend tokenTypes order is ABI.
[[nodiscard]] auto semanticTokensOptions() -> lsp::SemanticTokensOptions;

// textDocument/semanticTokens/full. Heavy: reuses this URI+version prep
// (type timeline) through `prepare`. generic / unknown are not pushed. Missing document is handled
// by the caller. Off / non-vertexadt / empty timeline → { data: [] }.
[[nodiscard]] auto semanticTokensFull(const SemanticTokensRequest& request)
  -> lsp::SemanticTokens;

} // namespace vertexadt
