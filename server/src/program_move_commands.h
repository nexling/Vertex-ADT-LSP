#pragma once

// Workspace glue for the program move engine (program_move.h,
// docs/adr/0017-native-program-move.md).

#include "workspace_state.h"

#include <lsp/json/json.h>
#include <lsp/message_base.h>
#include <lsp/types.h>

#include <functional>

namespace vertexadt {

// workspace/willRenameFiles on raw JSON both ways: the result is a
// WorkspaceEdit (see pull_diagnostics.h on lsp::WorkspaceEdit).
struct WillRenameFilesRequest
{
  static constexpr auto Kind = lsp::MessageKind::Request;
  using Params = lsp::json::Value;
  using Result = lsp::json::Value;
};

inline constexpr std::string_view kProgramMoveReportNotification = "vertexadt/programMoveReport";

// Every file and folder rename (filtered server side).
[[nodiscard]] auto programMoveFileOperations() -> lsp::FileOperationOptions;

// `vertexadt.moveProgram` with arguments `[{ uri, mode, target?, confirmShadow? }]`:
// - mode "default":  `{ target }` ($DIR_ spelling of the file) or `{ error }`
// - mode "validate": `{ ok: true }` or `{ error }` for `target`
// - mode "plan":     `{ refusal }`, `{ confirmation }`, or `{ edit, report }`
//   (edit: documentChanges with the rename; report: programMoveReportJson)
[[nodiscard]] auto moveProgramCommand(
  WorkspaceState& workspace,
  const lsp::json::Value& params,
  const std::function<void()>& pollCancel) -> lsp::json::Value;

// workspace/willRenameFiles: text edits only (null when there are none).
// `report` receives programMoveReportJson whenever an ADT file moved.
[[nodiscard]] auto willRenameFilesEdit(
  WorkspaceState& workspace,
  const lsp::json::Value& params,
  const std::function<void()>& pollCancel,
  const std::function<void(lsp::json::Value)>& report) -> lsp::json::Value;

} // namespace vertexadt
