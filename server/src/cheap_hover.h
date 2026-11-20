#pragma once

#include "document_store.h"
#include "extracts.h"
#include "library_paths.h"

#include <lsp/types.h>

#include <optional>
#include <string>
#include <string_view>

namespace vertexadt {

struct CheapHoverContext
{
  const Snapshot& snapshot;
  lsp::Position position;
  const ExtractStore& extracts;
  std::string_view extensionUri;
  // $DIR_* path hover; skipped when null.
  const LibraryLayout* layout = nullptr;
};

[[nodiscard]] auto wordAtPosition(std::string_view text, lsp::Position position)
  -> std::optional<std::string>;

[[nodiscard]] auto cheapHover(const CheapHoverContext& context) -> lsp::TextDocumentHoverResult;

} // namespace vertexadt
