#pragma once

#include "workspace_state.h"

#include <lsp/json/json.h>
#include <lsp/message_base.h>
#include <lsp/types.h>

#include <functional>
#include <string_view>

namespace vertexadt {

inline constexpr std::string_view kFixMissingCallParametersCommand =
  "vertexadt.fixMissingCallParameters";
inline constexpr std::string_view kAssignLocalSlotNamesCommand =
  "vertexadt.assignLocalSlotNames";

// workspace/executeCommand on raw JSON both ways: the results carry
// WorkspaceEdit-shaped maps, and lsp::WorkspaceEdit is not nothrow-move on
// MSVC (see pull_diagnostics.h).
struct ExecuteCommandRequest
{
  static constexpr auto Kind = lsp::MessageKind::Request;
  using Params = lsp::json::Value;
  using Result = lsp::json::Value;
};

// Advertised with an empty `commands` list. vscode-languageclient registers
// every listed ID as a VS Code command, which would collide with the host's
// own package.json registrations; the host forwards by ID instead.
[[nodiscard]] auto executeCommandOptions() -> lsp::ExecuteCommandOptions;

// `vertexadt.fixMissingCallParameters`: re-walks every workspace-index path
// plus every open `vertexadt` buffer with a path (buffer wins, else disk) and
// returns `{ changes, fixedCount }` (missing inserts + extra removals; the
// host applies the edit). Setting off: `{ changes: {}, fixedCount: 0 }`.
[[nodiscard]] auto fixMissingCallParameters(
  WorkspaceState& workspace,
  const std::function<void()>& pollCancel) -> lsp::json::Value;

// `vertexadt.assignLocalSlotNames` with arguments `[{ uri }]`: rebuilds the
// workspace index, then renames every non-`lN` / wrong-prefix
// declaration-number name in that one document (open buffer wins, else disk)
// to `{filePrefix}{base}`, file-wide and case-preserving. No callee or
// `requireLNamesInCallees` gate. Returns `{ changes, assignedCount, error? }`
// with `assignedCount` = text-edit count; the host applies the edit. An open
// non-`vertexadt` document yields `{ changes: {}, assignedCount: 0 }`.
[[nodiscard]] auto assignLocalSlotNames(
  WorkspaceState& workspace,
  const lsp::json::Value& params,
  const std::function<void()>& pollCancel) -> lsp::json::Value;

} // namespace vertexadt
