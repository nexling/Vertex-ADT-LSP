#pragma once

#include "workspace_state.h"

#include <lsp/json/json.h>

#include <functional>
#include <string_view>

namespace vertexadt {

inline constexpr std::string_view kExportWorkspaceReportsCommand =
  "vertexadt.exportWorkspaceReports";
inline constexpr std::string_view kExportWorkspaceDiagnosticsCommand =
  "vertexadt.exportWorkspaceDiagnostics";

// `vertexadt.exportWorkspaceReports` (Node writeWorkspaceCsvReports):
// rebuilds the index, then always writes `vertexadt-program-usage.csv`,
// `vertexadt-declaration-numbers.csv` and `vertexadt-local-slots.csv` next to
// the workspace root (first folder, or its parent when there are several).
// Returns `{ programUsagePath, declarationNumbersPath, localSlotsPath }`, or
// `{ error: 'No workspace root found' }` with no folders.
[[nodiscard]] auto exportWorkspaceReports(
  WorkspaceState& workspace,
  const std::function<void()>& pollCancel) -> lsp::json::Value;

// `vertexadt.exportWorkspaceDiagnostics` (Node writeWorkspaceDiagnosticsCsv):
// rebuilds the index, then validates every disk ADT file under Custom (and
// System when `diagnostics.exportCustomOnly` is false) with the live gates and
// a 10000 problem cap, 10 files per appended batch, into
// `vertexadt-workspace-diagnostics.csv`. Returns `{ diagnosticsPath,
// fileCount, diagnosticCount, errorCount, warningCount, informationCount }`.
[[nodiscard]] auto exportWorkspaceDiagnostics(
  WorkspaceState& workspace,
  const std::function<void()>& pollCancel) -> lsp::json::Value;

} // namespace vertexadt
