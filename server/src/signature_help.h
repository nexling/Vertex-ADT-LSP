#pragma once

#include "document_store.h"
#include "library_paths.h"

#include <lsp/types.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace vertexadt {

// textDocument/signatureHelp: the active command-catalog command or
// parenthesized `call(` on the current-line prefix. Heavy: a `call(` reads the
// callee's buffer-wins linked-file snapshot.

struct SignatureHelpInput
{
  const Snapshot& snapshot;
  lsp::Position position;
  const LibraryLayout& layout;
  std::function<std::optional<std::string>(const std::filesystem::path&)> textForPath;
};

[[nodiscard]] auto signatureHelpProviderOptions() -> lsp::SignatureHelpOptions;

// Null off `vertexadt`, with no active command, or when the cursor is past the
// last label of the active list (including closed parens with no after-paren
// labels).
[[nodiscard]] auto signatureHelp(const SignatureHelpInput& input)
  -> lsp::TextDocumentSignatureHelpResult;

} // namespace vertexadt
