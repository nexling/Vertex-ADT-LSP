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

// Include/call path and named-section-suffix completion. Ordinary
// textDocument/completion adds these items on top of the cheap
// keyword-variable list. Heavy: directory listing plus linked-file snapshots.

enum class IncludeCallMode
{
  None,
  Paths,
  Sections,
};

struct IncludeCallCompletionRequest
{
  const Snapshot& snapshot;
  lsp::Position position;
  // LSP CompletionTriggerKind; 0 when the client sent no context. Only 1
  // (Invoked) counts as invoked.
  int triggerKind = 0;
  const LibraryLayout& layout;
  // Buffer-wins linked-file text (loadLinkedFileText).
  std::function<std::optional<std::string>(const std::filesystem::path&)> textForPath;
};

// Cheap pre-check on the message loop: vertexadt and the line prefix up to the
// cursor matches an include/call directive (matchDirective). False means this
// family adds nothing and the cheap list answers alone.
[[nodiscard]] bool isIncludeCallCompletionSite(const Snapshot& snapshot, lsp::Position position);

// The family's items (empty for mode none). Never the cheap list.
[[nodiscard]] auto includeCallCompletionItems(const IncludeCallCompletionRequest& request)
  -> std::vector<lsp::CompletionItem>;

} // namespace vertexadt
