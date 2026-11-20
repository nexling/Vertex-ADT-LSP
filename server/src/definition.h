#pragma once

#include "document_store.h"
#include "library_paths.h"
#include "workspace_index.h"

#include <lsp/types.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Text for a linked file, buffer-wins over disk. nullopt when unreadable.
using LinkedTextReader =
  std::function<std::optional<std::string>(const std::filesystem::path&)>;

struct DefinitionContext
{
  const Snapshot& snapshot;
  lsp::Position position;
  const LibraryLayout& layout;
  LinkedTextReader textForPath;
  // Inferred `##included_from` parents (docs/adr/0015-native-inferred-included-from.md).
  const WorkspaceIndex* index = nullptr;
  bool inferParents = false;
};

// Shared JS-regex helpers for definition and references (ASCII word chars;
// non-ASCII bytes are non-word, as in UTF-16 JS without the `u` flag).
namespace navigation {
[[nodiscard]] bool isJsWordChar(char ch);
// JS word boundary (\b) at byte index `at`.
[[nodiscard]] bool isJsBoundary(std::string_view line, std::size_t at);
// ASCII case-insensitive `name` at byte index `at`.
[[nodiscard]] bool icaseAt(std::string_view line, std::size_t at, std::string_view name);
// UTF-16 code units in line[0, byteIndex).
[[nodiscard]] auto utf16Offset(std::string_view line, std::size_t byteIndex) -> int;
// JS text.split(/\r?\n/).
[[nodiscard]] auto splitJsLines(std::string_view text) -> std::vector<std::string_view>;
} // namespace navigation

// Where Node's findVariableDefinition lands: URI, line, UTF-16 start column.
// `path` is set when the site is a linked file (not this buffer).
struct DefinitionSite
{
  lsp::Uri uri;
  std::optional<std::filesystem::path> path;
  int line = 0;
  int character = 0;
};

// Definition word under a UTF-16 position: /\b[A-Za-z][A-Za-z0-9_-]*\$?\b/g,
// inclusive end, null when the match starts inside quotes. Not cheap hover's
// /\w+\$?/ — the mismatch is copied from Node on purpose.
[[nodiscard]] auto definitionWordAtPosition(std::string_view lineText, int character)
  -> std::optional<std::string>;

// Keyword labels (name or name without `$`) and command names (without `$`)
// are not definition targets. Exact case.
[[nodiscard]] bool isValidDefinitionTarget(std::string_view word);

// Node findVariableDefinition for an already-validated word: debug-dump
// most-recent assignment (including `cursorLine`), else nearest declaration
// above `cursorLine`, else one `##included_from` hop, last include first.
// A written `##included_from` parent is searched only above its earliest
// include of this file when `index` knows one
// (docs/adr/0015-native-inferred-included-from.md).
[[nodiscard]] auto locateDefinition(
  const Snapshot& snapshot,
  std::string_view word,
  int cursorLine,
  const LibraryLayout& layout,
  const LinkedTextReader& textForPath,
  const WorkspaceIndex* index = nullptr) -> std::optional<DefinitionSite>;

// When locateDefinition finds nothing: one site per inferred parent that
// reaches the cursor's section and declares `word` above its include point.
[[nodiscard]] auto inferredDefinitionSites(
  const Snapshot& snapshot,
  std::string_view word,
  int cursorLine,
  const LibraryLayout& layout,
  const LinkedTextReader& textForPath,
  const WorkspaceIndex* index,
  bool inferParents) -> std::vector<DefinitionSite>;

// textDocument/definition. Heavy: may read linked-file text. Never reads the
// live-buffer or include-neighborhood extracts. Miss → null.
[[nodiscard]] auto findDefinition(const DefinitionContext& context)
  -> lsp::TextDocumentDefinitionResult;

} // namespace vertexadt
