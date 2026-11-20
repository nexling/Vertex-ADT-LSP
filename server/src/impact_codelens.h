#pragma once

#include "document_store.h"
#include "library_paths.h"
#include "workspace_index.h"

#include <lsp/json/json.h>
#include <lsp/message_base.h>
#include <lsp/types.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vertexadt {

// textDocument/codeLens and codeLens/resolve on raw JSON both ways: the
// resolved command carries the free-form `{ uri, analysis }` argument.
struct CodeLensRequest
{
  static constexpr auto Kind = lsp::MessageKind::Request;
  using Params = lsp::json::Value;
  using Result = lsp::json::Value;
};

struct CodeLensResolveRequest
{
  static constexpr auto Kind = lsp::MessageKind::Request;
  using Params = lsp::json::Value;
  using Result = lsp::json::Value;
};

struct ImpactContext
{
  const LibraryLayout& layout;
  std::vector<std::shared_ptr<const IndexEntry>> index;
  bool enabled = true;
  double minIncomingCount = 2;
};

// Impact CodeLens list: at most one unresolved lens at (0,0)-(0,1) with data
// `{ kind: 'impact', uri }` when enabled, the URI is an indexable Custom/System
// overlay identity, and its incoming index links (includes always,
// commons-gated calls) reach `minIncomingCount`. An open non-`vertexadt`
// document lists nothing. `data.uri` echoes `uriText` as the client sent it.
[[nodiscard]] auto impactCodeLenses(
  const ImpactContext& context,
  const std::string& uriText,
  const std::optional<Snapshot>& snapshot) -> lsp::json::Value;

// Impact CodeLens resolve (server.js onCodeLensResolve + analyzeFileImpact):
// fills `Impact: {n} calls, {m} includes | Blast radius: {files} instances in
// {programs} program(s)` and `vertexadt.showImpactAnalysis` with
// `[{ uri, analysis }]`; no command when incoming is 0. Non-impact data comes
// back unchanged.
[[nodiscard]] auto resolveImpactCodeLens(const ImpactContext& context, lsp::json::Value codeLens)
  -> lsp::json::Value;

} // namespace vertexadt
