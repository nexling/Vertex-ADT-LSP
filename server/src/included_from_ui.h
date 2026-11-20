#pragma once

// Editor surfaces for inferred `##included_from` parents
// (docs/adr/0015-native-inferred-included-from.md): the line-1 CodeLens and
// the "Pin ##included_from" source actions.

#include "document_store.h"
#include "library_paths.h"
#include "workspace_index.h"

#include <lsp/json/json.h>

#include <optional>
#include <string_view>

namespace vertexadt {

inline constexpr std::string_view kOpenIncludedFromParentsCommand = "vertexadt.openIncludedFromParents";

struct ParentUiContext
{
  const LibraryLayout& layout;
  const WorkspaceIndex* index = nullptr;
  bool inferParents = true;
};

// One resolved lens at (0,0)-(0,1) listing the inferred parents ("Included
// from: a.prg +3", or "N parents, too many to infer, pin one" over the cap)
// with `vertexadt.openIncludedFromParents` and `[[{ uri, displayPath }]]`.
// Nothing when a written `##included_from` covers the whole file or there
// are no candidates.
[[nodiscard]] auto inferredParentsCodeLens(const ParentUiContext& context, const Snapshot& snapshot)
  -> std::optional<lsp::json::Value>;

// `source` code actions: one "Pin ##included_from <path>" per inferred parent
// candidate, plus "Pin all" when there are several. Each inserts the lines at
// the top of the file.
[[nodiscard]] auto pinIncludedFromActions(
  const ParentUiContext& context,
  const Snapshot& snapshot,
  const lsp::json::Value& uri) -> lsp::json::Array;

} // namespace vertexadt
