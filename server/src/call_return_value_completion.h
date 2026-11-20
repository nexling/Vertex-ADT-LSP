#pragma once

#include "document_store.h"
#include "library_paths.h"

#include <lsp/types.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Call return-value completion. Ordinary textDocument/completion adds
// `return:ret{field}$` snippets on the line after an immediately previous
// `Name$ = call(...)` assignment, one per `KEY=` field the callee packs into its
// return string. Heavy: resolve plus the whole-file buffer-wins callee snapshot.

struct CallReturnValueSite
{
  // Host regex group 1: the caller's `\w+$` name.
  std::string callerName;
  // Host regex group 2, untrimmed (UTF-8).
  std::string rawPath;
};

// Cheap, on the message loop: vertexadt, the position is not on line 0, and
// the previous line matches the host regex
// /(\w+\$)\s*=\s*call\(\s*("[^"]+"|'[^']+'|[^,)]+)(?:,\s*[^)]*)*\)/
// The current line's text and cursor column are not consulted.
[[nodiscard]] auto callReturnValueSite(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<CallReturnValueSite>;

// server.js extractReturnValues: the `KEY` of every field on every format line,
// whole-file, in document order, with no dedup.
[[nodiscard]] auto extractReturnFields(std::string_view calleeText) -> std::vector<std::string>;

// The family's items (never the cheap list). Empty when the callee does not
// resolve, has no snapshot text, or packs no fields.
[[nodiscard]] auto callReturnValueCompletionItems(
  const CallReturnValueSite& site,
  const LibraryLayout& layout,
  const std::function<std::optional<std::string>(const std::filesystem::path&)>& textForPath)
  -> std::vector<lsp::CompletionItem>;

} // namespace vertexadt
