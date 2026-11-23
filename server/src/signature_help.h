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

// textDocument/signatureHelp: the innermost command-catalog command or
// parenthesized `call(` around the cursor that has something to show, on the
// cursor's line joined with the `\` lines before it. Heavy: a `call(` reads the
// callee's buffer-wins linked-file snapshot.

struct SignatureHelpInput
{
  const Snapshot& snapshot;
  lsp::Position position;
  const LibraryLayout& layout;
  std::function<std::optional<std::string>(const std::filesystem::path&)> textForPath;
};

[[nodiscard]] auto signatureHelpProviderOptions() -> lsp::SignatureHelpOptions;

// Null off `vertexadt`, in comments, or when no enclosing command has a label
// for the cursor's argument. A `call(` whose callee is missing or has no inputs
// has none.
[[nodiscard]] auto signatureHelp(const SignatureHelpInput& input)
  -> lsp::TextDocumentSignatureHelpResult;

// vertexadt/signatureHelpSites: name positions of the enclosing commands that
// have something to show, innermost (the shown one) first. The client uses it
// to keep an escaped box closed (docs/adr/0022).
[[nodiscard]] auto signatureHelpSites(const SignatureHelpInput& input)
  -> std::vector<lsp::Position>;

} // namespace vertexadt
