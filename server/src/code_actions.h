#pragma once

#include "document_store.h"
#include "included_from_ui.h"

#include <lsp/json/json.h>
#include <lsp/message_base.h>
#include <lsp/types.h>

#include <optional>

namespace vertexadt {

// textDocument/codeAction on raw JSON. Params stay raw so the request's
// diagnostics are attached back verbatim (Node echoes the client object), and
// the result stays raw because lsp::WorkspaceEdit embeds Map, which is not
// nothrow-move on MSVC (see pull_diagnostics.h).
struct CodeActionRequest
{
  static constexpr auto Kind = lsp::MessageKind::Request;
  using Params = lsp::json::Value;
  using Result = lsp::json::Value;
};

[[nodiscard]] auto codeActionOptions() -> lsp::CodeActionOptions;

// Node onCodeAction. Branch order follows Node: call-param QuickFix /
// SourceFixAll (gated by `callParamValidationEnabled`), IGNORE CLOSE
// QuickFixes, callee-local-naming rename QuickFixes, then (native only)
// concat-whitespace QuickFix / SourceFixAll, then substring-start-index QuickFix. Consumes
// context.diagnostics and their `data`; no re-walk. Returns an array; empty
// when the document is not open or is not `vertexadt`.
[[nodiscard]] auto codeActions(
  const std::optional<Snapshot>& snapshot,
  const lsp::json::Value& params,
  bool callParamValidationEnabled,
  const ParentUiContext* parents = nullptr) -> lsp::json::Value;

} // namespace vertexadt
