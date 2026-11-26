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

class DebugTrace;

// Advertise full and range; legend tokenTypes order is ABI (new types are
// only ever appended).
[[nodiscard]] auto semanticTokensOptions() -> lsp::SemanticTokensOptions;

// Debug trace tokens for lines [firstLine, endLine): per-line lexical
// (keywords, catalog commands, strings, numbers, comments, `$`/vector
// names), `<value>` as debugValue, `_marker_` and START/END banners as
// debugMarker. Names in the assignment index are `variable` once the index
// has landed; until then they are left uncolored
// (docs/adr/0025-debug-trace-fast-path.md).
[[nodiscard]] auto debugTraceSemanticTokens(
  const DebugTrace& trace,
  int firstLine,
  int endLine,
  const std::function<void()>& throwIfCanceled = {}) -> lsp::SemanticTokens;

// The tokens of `full` on lines range.start.line…range.end.line, re-encoded.
[[nodiscard]] auto semanticTokensInRange(const lsp::SemanticTokens& full, const lsp::Range& range)
  -> lsp::SemanticTokens;

// textDocument/semanticTokens/full. Heavy: reuses this URI+version prep
// (type timeline) through `prepare`. generic / unknown are not pushed. Missing document is handled
// by the caller. Off / non-vertexadt / empty timeline → { data: [] }.
[[nodiscard]] auto semanticTokensFull(const SemanticTokensRequest& request)
  -> lsp::SemanticTokens;

} // namespace vertexadt
