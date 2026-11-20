#pragma once

#include <lsp/message_base.h>
#include <lsp/request_result.h>
#include <lsp/server_endpoint.h>
#include <lsp/types.h>

#include <type_traits>
#include <utility>

namespace vertexadt {

// LSP document pull report without relatedDocuments. Do not use
// lsp::DocumentDiagnosticReport or ServerEndpoint::onTextDocumentDiagnostic:
// that Result embeds Map (unordered_map), which is not nothrow-move on MSVC,
// and MessageHandler always instantiates ThreadPool::addTask.
using PullDiagnosticReport = lsp::OneOf<
  lsp::FullDocumentDiagnosticReport,
  lsp::UnchangedDocumentDiagnosticReport>;

struct PullDiagnosticRequest
{
  static constexpr auto Kind = lsp::MessageKind::Request;
  using Params = lsp::DocumentDiagnosticParams;
  using Result = PullDiagnosticReport;
};

static_assert(std::is_nothrow_move_constructible_v<PullDiagnosticReport>);
static_assert(
  std::is_nothrow_move_constructible_v<lsp::RequestResult<PullDiagnosticReport>>);

inline auto pullDiagnosticOptions() -> lsp::DiagnosticOptions
{
  return {
    .interFileDependencies = false,
    .workspaceDiagnostics = false,
  };
}

inline auto emptyFullPullReport() -> PullDiagnosticReport
{
  return lsp::FullDocumentDiagnosticReport{
    .kind = "full",
    .items = {},
  };
}

// Later tickets must register textDocument/diagnostic through this hook, never
// onTextDocumentDiagnostic. The callback may return PullDiagnosticReport,
// TaskFunction<PullDiagnosticReport>, or RequestResult<PullDiagnosticReport>.
template<typename F>
void registerPullDiagnostics(lsp::ServerEndpoint& endpoint, F&& callback)
{
  endpoint.messageHandler().onCustom<PullDiagnosticRequest>(
    "textDocument/diagnostic", std::forward<F>(callback));
}

} // namespace vertexadt
