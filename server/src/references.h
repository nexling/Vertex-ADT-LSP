#pragma once

#include "definition.h"
#include "document_store.h"
#include "library_paths.h"

#include <lsp/json/json.h>
#include <lsp/types.h>

namespace vertexadt {

struct ReferencesContext
{
  const Snapshot& snapshot;
  lsp::Position position;
  bool includeDeclaration = true;
  const LibraryLayout& layout;
  LinkedTextReader textForPath;
  // Inferred `##included_from` parents (docs/adr/0015-native-inferred-included-from.md).
  const WorkspaceIndex* index = nullptr;
  bool inferParents = false;
};

// textDocument/references (Node onReferences + findAllVariableReferences).
// Heavy: reads linked-file text. Same word filter as definition. Files: this
// buffer, then the definition file, then every `##included_from` (one hop),
// buffer-wins. Not `# include`, `call`, the include neighborhood, or the index.
// Missing word / keyword / command / non-vertexadt → [].
[[nodiscard]] auto findReferences(const ReferencesContext& context)
  -> lsp::TextDocumentReferencesResult;

// Params arrive as raw JSON so a missing `context` defaults to
// includeDeclaration true (lsp::ReferenceParams requires it).
[[nodiscard]] auto parseReferenceParams(lsp::json::Value&& json) -> lsp::ReferenceParams;

} // namespace vertexadt
