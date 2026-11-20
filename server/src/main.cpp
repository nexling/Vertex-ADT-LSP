#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <lsp/error.h>
#include <lsp/io/standard_io.h>
#include <lsp/json/json.h>
#include <lsp/server_endpoint.h>

#include "catalog/catalog.h"
#include "code_actions.h"
#include "program_move.h"
#include "program_move_commands.h"
#include "workspace_commands.h"
#include "workspace_exports.h"
#include "call_return_value_completion.h"
#include "cheap_completion.h"
#include "completion_ranking.h"
#include "slot_completion.h"
#include "cheap_hover.h"
#include "control_block_pairing.h"
#include "indent_formatter.h"
#include "text_util.h"
#include "dbcreate_field_completion.h"
#include "debug_filename_completion.h"
#include "form_name_completion.h"
#include "impact_codelens.h"
#include "definition.h"
#include "document_links.h"
#include "document_store.h"
#include "include_call_completion.h"
#include "remaining_param_completion.h"
#include "newline_cleanup.h"
#include "macro_library_completion.h"
#include "object_parameter_completion.h"
#include "piece_setup_sets.h"
#include "pull_diagnostics.h"
#include "references.h"
#include "semantic_tokens.h"
#include "signature_help.h"
#include "work_scheduler.h"
#include "workspace_state.h"

namespace {

constexpr std::string_view kSlowHeavyMarker = "vertex-adt-ls:slow-heavy";
constexpr auto kSlowHeavySlice = std::chrono::milliseconds(5);
constexpr auto kSlowHeavyBudget = std::chrono::milliseconds(250);

void logStdErr(std::string_view message)
{
  std::cerr << message << std::endl;
}

auto runCheapHover(
  vertexadt::WorkspaceState* workspace,
  const std::optional<vertexadt::Snapshot>& snapshot,
  lsp::Position position) -> lsp::TextDocumentHoverResult
{
  if (!workspace || !snapshot)
  {
    return nullptr;
  }
  const auto settings = workspace->settings();
  return vertexadt::cheapHover({
    .snapshot = *snapshot,
    .position = position,
    .extracts = workspace->extracts(),
    .extensionUri = settings.extensionUri,
    .layout = &workspace->layout(),
  });
}

auto runCheapCompletion(
  vertexadt::WorkspaceState* workspace,
  const std::optional<vertexadt::Snapshot>& snapshot) -> lsp::TextDocumentCompletionResult
{
  if (!workspace || !snapshot)
  {
    return nullptr;
  }
  const auto settings = workspace->settings();
  return vertexadt::cheapCompletion({
    .snapshot = *snapshot,
    .extracts = workspace->extracts(),
    .useExpandedSyntax = settings.useExpandedSyntax,
  });
}

void pollHeavyCancel(
  lsp::MessageHandler& handler,
  const lsp::RequestId& id,
  const std::optional<vertexadt::Snapshot>& snapshot)
{
  const auto slow = snapshot
    && snapshot->text.find(kSlowHeavyMarker) != std::string::npos;
  if (slow)
  {
    const auto deadline = std::chrono::steady_clock::now() + kSlowHeavyBudget;
    while (std::chrono::steady_clock::now() < deadline)
    {
      if (handler.isCanceled(id))
      {
        throw lsp::RequestError(lsp::MessageError::RequestCancelled, "Canceled");
      }
      std::this_thread::sleep_for(kSlowHeavySlice);
    }
  }

  if (handler.isCanceled(id))
  {
    throw lsp::RequestError(lsp::MessageError::RequestCancelled, "Canceled");
  }
}

auto emptySemanticTokens(int version) -> lsp::SemanticTokens
{
  lsp::SemanticTokens tokens;
  tokens.resultId = std::to_string(version);
  return tokens;
}

// Runs `work` on the heavy pool for `uri`, tracked so didChange/didClose can
// cancel it, and polls `$/cancelRequest` while waiting.
template <typename Result, typename Work>
auto heavyRequest(
  vertexadt::WorkScheduler& scheduler,
  const lsp::Uri& uri,
  int version,
  Work work) -> lsp::TaskFunction<Result>
{
  const auto id = lsp::RequestContext::get().id();
  scheduler.trackHeavy(uri, id, version);

  struct HeavyJob
  {
    vertexadt::WorkScheduler* scheduler = nullptr;
    lsp::Uri uri;
    lsp::RequestId id;
    Work work;
  };
  auto job = std::make_shared<HeavyJob>(HeavyJob{
    .scheduler = &scheduler,
    .uri = uri,
    .id = id,
    .work = std::move(work),
  });

  return lsp::TaskFunction<Result>([job]() {
    struct Complete
    {
      HeavyJob* job = nullptr;
      ~Complete()
      {
        job->scheduler->completeHeavy(job->uri, job->id);
      }
    } complete{job.get()};

    auto future = job->scheduler->submitHeavy(
      job->uri,
      [job]() -> Result
      {
        return job->work(job->id);
      });

    while (future.wait_for(kSlowHeavySlice) != std::future_status::ready)
    {
      lsp::RequestContext::get().throwIfCanceled();
    }
    return future.get();
  });
}

struct ReferencesRequest
{
  static constexpr auto Kind = lsp::MessageKind::Request;
  using Params = lsp::json::Value;
  using Result = lsp::TextDocumentReferencesResult;
};

} // namespace

auto main() -> int
{
  try
  {
    const auto catalogs = vertexadt::catalog::linkIntoExecutable();
    if (catalogs.commandCount != 276
        || catalogs.tableValidatorCount != 271
        || catalogs.customValidatorCount != 5
        || catalogs.keywordCount != 1336)
    {
      logStdErr("compiled catalogs failed to link into the language-server executable");
      return EXIT_FAILURE;
    }

    auto documents = vertexadt::DocumentStore{};
    std::unique_ptr<vertexadt::WorkspaceState> workspace;
    auto scheduler = vertexadt::WorkScheduler{};
    auto endpoint = lsp::ServerEndpoint(lsp::io::standardIO());

    auto refreshHooks = vertexadt::RefreshHooks{
      .diagnosticRefresh =
        [&] {
          endpoint.workspaceDiagnosticRefresh([](auto&&) {});
        },
      .semanticTokensRefresh =
        [&] {
          endpoint.workspaceSemanticTokensRefresh([](auto&&) {});
        },
      .codeLensRefresh =
        [&] {
          endpoint.workspaceCodeLensRefresh([](auto&&) {});
        },
    };

    // Indentation formatter (docs/adr/0013): whole buffer, answered on the
    // message loop. A refusal returns no edits and shows a warning.
    const auto formatIndentationRequest =
      [&](const lsp::Uri& uri, const lsp::FormattingOptions& options, std::optional<lsp::Range> range)
      -> lsp::NullOr<lsp::Array<lsp::TextEdit>>
    {
      const auto snapshot = documents.snapshot(uri);
      if (!snapshot || snapshot->languageId != "vertexadt" || vertexadt::isDebugDump(snapshot->text)
          || (workspace && !workspace->settings().formatEnabled))
      {
        return nullptr;
      }
      const auto indentSize = static_cast<int>(options.tabSize);
      auto result = range
        ? vertexadt::indentationEdits(snapshot->text, indentSize,
            static_cast<int>(range->start.line),
            static_cast<int>(range->end.line)
              - (range->end.character == 0 && range->end.line > range->start.line ? 1 : 0))
        : vertexadt::indentationEdits(snapshot->text, indentSize);
      if (result.refusal)
      {
        endpoint.windowShowMessage(lsp::ShowMessageParams{
          .type = lsp::MessageType::Warning,
          .message = vertexadt::indentRefusalMessage(*result.refusal),
        });
        return nullptr;
      }
      return lsp::Array<lsp::TextEdit>(std::move(result.edits));
    };

    endpoint
      .onInitialize(
        [&](lsp::InitializeParams&& params) -> lsp::InitializeResult
        {
          if (!workspace)
          {
            workspace = std::make_unique<vertexadt::WorkspaceState>(
              documents, scheduler, refreshHooks);
          }
          workspace->initialize(params);
          return {
            .capabilities = {
              .positionEncoding = lsp::PositionEncodingKind::UTF16,
              .textDocumentSync = lsp::TextDocumentSyncOptions{
                .openClose = true,
                .change = lsp::TextDocumentSyncKind::Incremental,
              },
              .completionProvider = lsp::CompletionOptions{
                .triggerCharacters = lsp::Array<lsp::String>{" ", ",", "(", "/", "\"", ")"},
                .resolveProvider = true,
              },
              .hoverProvider = true,
              .signatureHelpProvider = vertexadt::signatureHelpProviderOptions(),
              .definitionProvider = true,
              .referencesProvider = true,
              .documentHighlightProvider = true,
              .codeActionProvider = vertexadt::codeActionOptions(),
              .codeLensProvider = lsp::CodeLensOptions{
                .resolveProvider = true,
              },
              .documentLinkProvider = lsp::DocumentLinkOptions{
                .resolveProvider = false,
              },
              .documentFormattingProvider = true,
              .documentRangeFormattingProvider = true,
              .documentOnTypeFormattingProvider = lsp::DocumentOnTypeFormattingOptions{
                .firstTriggerCharacter = "\n",
              },
              .executeCommandProvider = vertexadt::executeCommandOptions(),
              .semanticTokensProvider = vertexadt::semanticTokensOptions(),
              .diagnosticProvider = vertexadt::pullDiagnosticOptions(),
              .workspace = lsp::WorkspaceOptions{
                .workspaceFolders = lsp::WorkspaceFoldersServerCapabilities{
                  .supported = true,
                  .changeNotifications = true,
                },
                .fileOperations = vertexadt::programMoveFileOperations(),
              },
            },
            .serverInfo = lsp::ServerInfo{
              .name = "vertex-adt-ls",
            },
          };
        })
      .onInitialized(
        [&](lsp::InitializedParams&&)
        {
          if (!workspace)
          {
            return;
          }
          lsp::json::Array watchers;
          for (const auto& glob : workspace->watcherGlobs())
          {
            lsp::json::Object watcher;
            watcher["globPattern"] = glob;
            watchers.push_back(lsp::json::Value(std::move(watcher)));
          }
          lsp::json::Object options;
          options["watchers"] = std::move(watchers);
          lsp::Registration registration;
          registration.id = "vertex-adt-watched-files";
          registration.method = "workspace/didChangeWatchedFiles";
          registration.registerOptions = lsp::json::Value(std::move(options));
          endpoint.clientRegisterCapability(
            lsp::RegistrationParams{.registrations = {std::move(registration)}},
            [](auto&&) {});
          // Node walks the whole Custom+System tree in onInitialized; Impact
          // CodeLens and the cross-file families read that index.
          workspace->scheduleFullIndex();
        })
      .onTextDocumentDidOpen(
        [&](lsp::DidOpenTextDocumentParams&& params)
        {
          if (workspace)
          {
            workspace->didOpen(params);
          }
        })
      .onTextDocumentDidChange(
        [&](lsp::DidChangeTextDocumentParams&& params)
        {
          const auto uri = params.textDocument.uri;
          if (workspace)
          {
            workspace->didChange(params);
          }
          scheduler.noteEdited(uri);
          scheduler.cancelHeavyForUri(endpoint.messageHandler(), uri);
        })
      .onTextDocumentDidClose(
        [&](lsp::DidCloseTextDocumentParams&& params)
        {
          scheduler.cancelHeavyForUri(endpoint.messageHandler(), params.textDocument.uri);
          if (workspace)
          {
            workspace->didClose(params);
          }
        })
      .onWorkspaceDidChangeWatchedFiles(
        [&](lsp::DidChangeWatchedFilesParams&& params)
        {
          if (workspace)
          {
            workspace->didChangeWatchedFiles(params);
          }
        })
      .onWorkspaceDidChangeConfiguration(
        [&](lsp::DidChangeConfigurationParams&& params)
        {
          if (workspace)
          {
            workspace->didChangeConfiguration(params);
          }
        })
      .onWorkspaceDidChangeWorkspaceFolders(
        [&](lsp::DidChangeWorkspaceFoldersParams&& params)
        {
          if (workspace)
          {
            workspace->didChangeWorkspaceFolders(params);
          }
        })
      .onTextDocumentHover(
        [&](lsp::HoverParams&& params) -> lsp::TextDocumentHoverResult
        {
          auto snapshot = documents.snapshot(params.textDocument.uri);
          const auto position = params.position;
          auto* ws = workspace.get();
          return scheduler.runCheap(
            [snapshot = std::move(snapshot), ws, position]()
            {
              return runCheapHover(ws, snapshot, position);
            });
        })
      .onTextDocumentDocumentHighlight(
        [&](lsp::DocumentHighlightParams&& params) -> lsp::TextDocumentDocumentHighlightResult
        {
          auto snapshot = documents.snapshot(params.textDocument.uri);
          const auto position = params.position;
          return scheduler.runCheap(
            [snapshot = std::move(snapshot), position]() -> lsp::TextDocumentDocumentHighlightResult
            {
              if (!snapshot || snapshot->languageId != "vertexadt")
              {
                return nullptr;
              }
              return vertexadt::documentHighlights(snapshot->text, position);
            });
        })
      .onTextDocumentCompletion(
        [&](lsp::CompletionParams&& params)
          -> lsp::RequestResult<lsp::TextDocumentCompletionResult>
        {
          auto snapshot = documents.snapshot(params.textDocument.uri);
          auto* ws = workspace.get();
          auto cheap = scheduler.runCheap(
            [&snapshot, ws]()
            {
              return runCheapCompletion(ws, snapshot);
            });
          // Local-only dbcreate field and debug-filename items: same response as the cheap
          // list, after that list is ready, so they never delay it. Any family item ranks
          // the cheap items below every family band (completion_ranking.h). In a slot the
          // family items are substring-filtered on the typed text and the list is
          // incomplete, so VS Code asks again per keystroke (slot_completion.h).
          using vertexadt::CompletionFamily;
          const auto triggerKind = params.context
            ? static_cast<int>(params.context->triggerKind.value())
            : 0;
          // A typed trigger character (space, `,`, `(`, ...) opens a popup only where a
          // completion family has items; the keyword-variable list waits for the first
          // typed word character (VS Code quick suggestions). See
          // docs/adr/0007-native-trigger-characters-open-family-sites-only.md.
          const bool typedTrigger = triggerKind == 2 && params.context->triggerCharacter
            && !params.context->triggerCharacter->empty();
          const auto noPopup = []() -> lsp::TextDocumentCompletionResult
          {
            return lsp::CompletionList{.isIncomplete = false, .items = {}};
          };
          std::size_t cheapCount = 0;
          bool ranked = false;
          bool slotActive = false;
          vertexadt::SlotTyping typing;
          std::vector<lsp::Position> sitePositions;
          if (cheap.holdsAlternative<lsp::CompletionList>() && snapshot)
          {
            typing = vertexadt::slotTyping(*snapshot, params.position);
            sitePositions = vertexadt::slotSitePositions(typing);
            auto& list = cheap.get<lsp::CompletionList>();
            cheapCount = list.items.size();
            // dbcreate offers the next field slot at the end of a word, so the local
            // families try the typed word's start first: typing `NAME` filters the
            // name slot, not the type slot after it.
            const std::vector<lsp::Position> localPositions(
              sitePositions.rbegin(), sitePositions.rend());
            auto local = [&](CompletionFamily family, const auto& itemsAt)
            {
              for (const auto& at : localPositions)
              {
                auto items = itemsAt(at);
                if (!items.empty())
                {
                  slotActive = true;
                  ranked |= vertexadt::appendFamilyItems(
                    list, family, vertexadt::shapeSlotItems(std::move(items), typing));
                  return;
                }
              }
            };
            local(
              CompletionFamily::DbcreateField,
              [&](lsp::Position at)
              {
                return vertexadt::dbcreateFieldCompletionItems(*snapshot, at);
              });
            // Local-only debug-filename item (this buffer + basename).
            local(
              CompletionFamily::DebugFilename,
              [&](lsp::Position at)
              {
                return vertexadt::debugFilenameCompletionItems(*snapshot, at);
              });
            if (ranked)
            {
              vertexadt::rankCheapItemsBelowFamilies(list, cheapCount);
            }
            if (slotActive)
            {
              list.isIncomplete = true;
            }
          }
          // Off include/call directive lines, remaining-param comma slots, and
          // lines after a `Name$ = call(...)` the cheap list answers alone,
          // joined on the message loop. Otherwise the heavy pool appends
          // include/call path and section items, the callee's remaining-param
          // names, the callee's return-value snippets, and/or Custom-over-System
          // `forms/` names in a form-name slot, and/or `get_*`/`set_*` object
          // parameters, and/or Custom `complibs/` library names or codes, and/or
          // vxsettings piece.setup sets in the `new_piece` set string, each
          // ranked above the cheap list. Sites are found at the cursor, then at
          // the typed word's start.
          if (!ws || !snapshot || !cheap.holdsAlternative<lsp::CompletionList>())
          {
            return cheap;
          }
          auto findSite = [&sitePositions](const auto& detect)
          {
            using Site = std::decay_t<decltype(*detect(sitePositions.front()))>;
            std::optional<std::pair<Site, lsp::Position>> found;
            for (const auto& at : sitePositions)
            {
              if (auto site = detect(at))
              {
                found.emplace(std::move(*site), at);
                break;
              }
            }
            return found;
          };
          auto includeCallSite = findSite(
            [&](lsp::Position at) -> std::optional<bool>
            {
              if (vertexadt::isIncludeCallCompletionSite(*snapshot, at))
              {
                return true;
              }
              return std::nullopt;
            });
          auto remainingSite = findSite(
            [&](lsp::Position at) { return vertexadt::remainingParamSite(*snapshot, at); });
          auto returnValueSite = findSite(
            [&](lsp::Position at) { return vertexadt::callReturnValueSite(*snapshot, at); });
          auto formSite = findSite(
            [&](lsp::Position at) { return vertexadt::formNameSite(*snapshot, at); });
          auto objectParamSite = findSite(
            [&](lsp::Position at) { return vertexadt::objectParameterSite(*snapshot, at); });
          const bool macroCodeFirst = ws->settings().macroCodeFirst;
          auto macroLibrarySite = findSite(
            [&](lsp::Position at)
            { return vertexadt::macroLibrarySite(*snapshot, at, macroCodeFirst); });
          auto pieceSetSite = findSite(
            [&](lsp::Position at) { return vertexadt::pieceSetSite(*snapshot, at); });
          if (!includeCallSite && !remainingSite && !returnValueSite && !formSite
              && !objectParamSite && !macroLibrarySite && !pieceSetSite)
          {
            if (typedTrigger && !ranked)
            {
              return noPopup();
            }
            return cheap;
          }
          const auto version = snapshot->version;
          auto* handler = &endpoint.messageHandler();
          return heavyRequest<lsp::TextDocumentCompletionResult>(
            scheduler,
            params.textDocument.uri,
            version,
            [snapshot = std::move(snapshot),
             cheap = std::move(cheap),
             ws,
             handler,
             triggerKind,
             typedTrigger,
             noPopup,
             localRanked = ranked,
             cheapCount,
             typing = std::move(typing),
             includeCallSite,
             remainingSite = std::move(remainingSite),
             returnValueSite = std::move(returnValueSite),
             formSite,
             objectParamSite = std::move(objectParamSite),
             macroLibrarySite = std::move(macroLibrarySite),
             pieceSetSite = std::move(pieceSetSite)](
              const lsp::RequestId& id) -> lsp::TextDocumentCompletionResult
            {
              pollHeavyCancel(*handler, id, snapshot);
              const auto textForPath = [ws](const std::filesystem::path& path)
              {
                return ws->textForPath(path);
              };
              auto result = cheap;
              auto& list = result.get<lsp::CompletionList>();
              list.isIncomplete = true;
              bool ranked = false;
              const auto uri = std::string(snapshot->uri.path());
              // A re-request for an incomplete list reuses the slot's listing.
              const bool reuse = triggerKind == 3;
              using Listing = std::optional<std::vector<lsp::CompletionItem>>;
              auto family = [&](
                              CompletionFamily kind,
                              const std::function<Listing()>& compute)
              {
                auto items = vertexadt::cachedSlotListing(
                  uri, kind, vertexadt::slotListingKey(typing), reuse, compute);
                if (!items)
                {
                  throw lsp::RequestError(lsp::MessageError::RequestCancelled, "Canceled");
                }
                ranked |= vertexadt::appendFamilyItems(
                  list, kind, vertexadt::shapeSlotItems(std::move(*items), typing));
              };
              if (includeCallSite)
              {
                const auto at = includeCallSite->second;
                family(CompletionFamily::IncludeCall, [&]() -> Listing
                {
                  return vertexadt::includeCallCompletionItems({
                    .snapshot = *snapshot,
                    .position = at,
                    .triggerKind = triggerKind,
                    .layout = ws->layout(),
                    .textForPath = textForPath,
                  });
                });
              }
              if (remainingSite)
              {
                family(CompletionFamily::RemainingParam, [&]() -> Listing
                {
                  return vertexadt::remainingParamCompletionItems(
                    remainingSite->first, ws->layout(), textForPath);
                });
              }
              if (returnValueSite)
              {
                family(CompletionFamily::CallReturnValue, [&]() -> Listing
                {
                  return vertexadt::callReturnValueCompletionItems(
                    returnValueSite->first, ws->layout(), textForPath);
                });
              }
              if (formSite)
              {
                const auto at = formSite->second;
                family(CompletionFamily::FormName, [&]() -> Listing
                {
                  return vertexadt::formNameCompletionItems(formSite->first, at, ws->layout());
                });
              }
              if (objectParamSite)
              {
                family(CompletionFamily::ObjectParameter, [&]() -> Listing
                {
                  const auto settings = ws->settings();
                  return vertexadt::objectParameterCompletionItems({
                    .snapshot = *snapshot,
                    .site = objectParamSite->first,
                    .extensionUri = settings.extensionUri,
                    .strictModeEnabled = settings.strictModeEnabled,
                    .extracts = &ws->extracts(),
                    .linkedFiles = &ws->linkedFiles(),
                    .layout = &ws->layout(),
                    .textForPath = textForPath,
                    .index = &ws->index(),
                    .crossCall = &ws->crossCall(),
                    .throwIfCanceled =
                      [handler, &id]()
                      {
                        if (handler->isCanceled(id))
                        {
                          throw lsp::RequestError(
                            lsp::MessageError::RequestCancelled, "Canceled");
                        }
                      },
                    .isCanceled = [handler, &id]() { return handler->isCanceled(id); },
                  });
                });
              }
              if (macroLibrarySite && macroLibrarySite->first.codeFirst)
              {
                // Code-first pairs come filtered and ranked from their own
                // index (no slot cache: exports land between keystrokes).
                const auto settings = ws->settings();
                ranked |= vertexadt::appendFamilyItems(
                  list,
                  CompletionFamily::MacroLibrary,
                  vertexadt::macroCodeFirstCompletionItems({
                    .site = macroLibrarySite->first,
                    .layout = ws->layout(),
                    .extensionUri = settings.extensionUri,
                    .line = typing.line,
                    .lineNumber = typing.lineNumber,
                    .cursor = typing.cursor,
                    .log = {},
                  }));
              }
              else if (macroLibrarySite)
              {
                family(CompletionFamily::MacroLibrary, [&]() -> Listing
                {
                  const auto settings = ws->settings();
                  return vertexadt::macroLibraryCompletionItems({
                    .site = macroLibrarySite->first,
                    .layout = ws->layout(),
                    .extensionUri = settings.extensionUri,
                    .isCanceled = [handler, &id]() { return handler->isCanceled(id); },
                    .log = {},
                  });
                });
              }
              if (pieceSetSite)
              {
                const auto at = pieceSetSite->second;
                family(CompletionFamily::PieceSet, [&]() -> Listing
                {
                  const auto sets =
                    vertexadt::loadPieceSets(vertexadt::resolvePieceSetSources(ws->layout()));
                  return vertexadt::pieceSetCompletionItems(pieceSetSite->first, at, *sets);
                });
              }
              if (typedTrigger && !ranked && !localRanked)
              {
                return noPopup();
              }
              if (ranked)
              {
                vertexadt::rankCheapItemsBelowFamilies(list, cheapCount);
              }
              return result;
            });
        })
      .onTextDocumentDefinition(
        [&](lsp::DefinitionParams&& params)
          -> lsp::RequestResult<lsp::TextDocumentDefinitionResult>
        {
          auto snapshot = documents.snapshot(params.textDocument.uri);
          const auto version = snapshot ? snapshot->version : 0;
          auto* ws = workspace.get();
          auto* handler = &endpoint.messageHandler();
          return heavyRequest<lsp::TextDocumentDefinitionResult>(
            scheduler,
            params.textDocument.uri,
            version,
            [snapshot = std::move(snapshot), ws, handler, position = params.position](
              const lsp::RequestId& id) -> lsp::TextDocumentDefinitionResult
            {
              pollHeavyCancel(*handler, id, snapshot);
              if (!ws || !snapshot)
              {
                return nullptr;
              }
              return vertexadt::findDefinition({
                .snapshot = *snapshot,
                .position = position,
                .layout = ws->layout(),
                .textForPath =
                  [ws](const std::filesystem::path& path)
                  {
                    return ws->textForPath(path);
                  },
                .index = &ws->index(),
                .inferParents = ws->settings().inferIncludedFrom,
              });
            });
        })
      .onTextDocumentSignatureHelp(
        [&](lsp::SignatureHelpParams&& params)
          -> lsp::RequestResult<lsp::TextDocumentSignatureHelpResult>
        {
          // Heavy: a `call(` reads the callee's linked-file snapshot. The
          // SignatureHelpContext is ignored; every request recomputes.
          auto snapshot = documents.snapshot(params.textDocument.uri);
          const auto version = snapshot ? snapshot->version : 0;
          auto* ws = workspace.get();
          auto* handler = &endpoint.messageHandler();
          return heavyRequest<lsp::TextDocumentSignatureHelpResult>(
            scheduler,
            params.textDocument.uri,
            version,
            [snapshot = std::move(snapshot), ws, handler, position = params.position](
              const lsp::RequestId& id) -> lsp::TextDocumentSignatureHelpResult
            {
              pollHeavyCancel(*handler, id, snapshot);
              if (!ws || !snapshot)
              {
                return nullptr;
              }
              return vertexadt::signatureHelp({
                .snapshot = *snapshot,
                .position = position,
                .layout = ws->layout(),
                .textForPath =
                  [ws](const std::filesystem::path& path)
                  {
                    return ws->textForPath(path);
                  },
              });
            });
        })
      .onCompletionItemResolve(
        [](lsp::CompletionItem&& item) -> lsp::CompletionItem
        {
          return vertexadt::resolveKeywordCompletion(std::move(item));
        })
      .onTextDocumentDocumentLink(
        [&](lsp::DocumentLinkParams&& params)
          -> lsp::RequestResult<lsp::TextDocumentDocumentLinkResult>
        {
          auto snapshot = documents.snapshot(params.textDocument.uri);
          const auto version = snapshot ? snapshot->version : 0;
          auto* ws = workspace.get();
          auto* handler = &endpoint.messageHandler();
          return heavyRequest<lsp::TextDocumentDocumentLinkResult>(
            scheduler,
            params.textDocument.uri,
            version,
            [snapshot = std::move(snapshot), ws, handler](const lsp::RequestId& id)
              -> lsp::TextDocumentDocumentLinkResult
            {
              pollHeavyCancel(*handler, id, snapshot);
              if (!ws || !snapshot)
              {
                return lsp::Array<lsp::DocumentLink>{};
              }
              return vertexadt::documentLinks({
                .snapshot = *snapshot,
                .layout = ws->layout(),
              });
            });
        })
      .onTextDocumentSemanticTokensFull(
        [&](lsp::SemanticTokensParams&& params)
          -> lsp::RequestResult<lsp::TextDocumentSemanticTokensFullResult>
        {
          auto snapshot = documents.snapshot(params.textDocument.uri);
          const auto version = snapshot ? snapshot->version : 0;
          auto* ws = workspace.get();
          auto* handler = &endpoint.messageHandler();
          return heavyRequest<lsp::TextDocumentSemanticTokensFullResult>(
            scheduler,
            params.textDocument.uri,
            version,
            [snapshot = std::move(snapshot), ws, handler](const lsp::RequestId& id)
              -> lsp::TextDocumentSemanticTokensFullResult
            {
              pollHeavyCancel(*handler, id, snapshot);
              if (!ws || !snapshot)
              {
                return emptySemanticTokens(snapshot ? snapshot->version : 0);
              }
              const auto settings = ws->settings();
              const std::function<void()> throwIfCanceled = [handler, id]()
              {
                if (handler->isCanceled(id))
                {
                  throw lsp::RequestError(lsp::MessageError::RequestCancelled, "Canceled");
                }
              };
              return vertexadt::semanticTokensFull({
                .snapshot = *snapshot,
                .semanticHighlightingEnabled = settings.semanticHighlightingEnabled,
                .debugValidationCommands = settings.debugValidationCommands,
                .strictModeEnabled = settings.strictModeEnabled,
                .extracts = &ws->extracts(),
                .linkedFiles = &ws->linkedFiles(),
                .layout = &ws->layout(),
                .textForPath =
                  [ws](const std::filesystem::path& path)
                  {
                    return ws->textForPath(path);
                  },
                .throwIfCanceled = throwIfCanceled,
                .prepare =
                  [ws, &snapshot = *snapshot, strict = settings.strictModeEnabled, throwIfCanceled]()
                  {
                    return ws->documentPrep(snapshot, strict, throwIfCanceled);
                  },
              });
            });
        })
      .onTextDocumentOnTypeFormatting(
        [&](lsp::DocumentOnTypeFormattingParams&& params)
          -> lsp::TextDocumentOnTypeFormattingResult
        {
          // Newline cleanup: one previous-line parse, local to this buffer,
          // answered on the message loop.
          const auto snapshot = documents.snapshot(params.textDocument.uri);
          if (!snapshot)
          {
            return nullptr;
          }
          auto edits = vertexadt::newlineCleanupEdits(*snapshot, params.position, params.ch);
          if (edits.empty())
          {
            return nullptr;
          }
          return lsp::Array<lsp::TextEdit>(std::move(edits));
        })
      .onTextDocumentFormatting(
        [&](lsp::DocumentFormattingParams&& params) -> lsp::TextDocumentFormattingResult
        {
          return formatIndentationRequest(params.textDocument.uri, params.options, std::nullopt);
        })
      .onTextDocumentRangeFormatting(
        [&](lsp::DocumentRangeFormattingParams&& params) -> lsp::TextDocumentRangeFormattingResult
        {
          return formatIndentationRequest(params.textDocument.uri, params.options, params.range);
        })
      .onShutdown([]() -> lsp::ShutdownResult { return {}; })
      .onExit([]() {});

    // textDocument/references takes raw params so a missing `context` defaults
    // to includeDeclaration true (lsp::ReferenceParams requires it).
    endpoint.messageHandler().onCustom<ReferencesRequest>(
      "textDocument/references",
      [&](lsp::json::Value&& rawParams)
        -> lsp::RequestResult<lsp::TextDocumentReferencesResult>
      {
        auto params = vertexadt::parseReferenceParams(std::move(rawParams));
        auto snapshot = documents.snapshot(params.textDocument.uri);
        const auto version = snapshot ? snapshot->version : 0;
        auto* ws = workspace.get();
        auto* handler = &endpoint.messageHandler();
        return heavyRequest<lsp::TextDocumentReferencesResult>(
          scheduler,
          params.textDocument.uri,
          version,
          [snapshot = std::move(snapshot),
           ws,
           handler,
           position = params.position,
           includeDeclaration = params.context.includeDeclaration](
            const lsp::RequestId& id) -> lsp::TextDocumentReferencesResult
          {
            pollHeavyCancel(*handler, id, snapshot);
            if (!ws || !snapshot)
            {
              return lsp::Array<lsp::Location>{};
            }
            return vertexadt::findReferences({
              .snapshot = *snapshot,
              .position = position,
              .includeDeclaration = includeDeclaration,
              .layout = ws->layout(),
              .textForPath =
                [ws](const std::filesystem::path& path)
                {
                  return ws->textForPath(path);
                },
              .index = &ws->index(),
              .inferParents = ws->settings().inferIncludedFrom,
            });
          });
      });

    // textDocument/codeAction: request diagnostics plus one buffer line each,
    // answered on the message loop. Raw JSON both ways (see code_actions.h).
    endpoint.messageHandler().onCustom<vertexadt::CodeActionRequest>(
      "textDocument/codeAction",
      [&](lsp::json::Value&& rawParams) -> lsp::json::Value
      {
        const auto* textDocument = rawParams.isObject()
          ? rawParams.object().find("textDocument")
          : nullptr;
        const auto* uri = textDocument && textDocument->isObject()
          ? textDocument->object().find("uri")
          : nullptr;
        std::optional<vertexadt::Snapshot> snapshot;
        if (uri && uri->isString())
        {
          snapshot = documents.snapshot(lsp::Uri::parse(uri->string()));
        }
        const auto callParamValidation = workspace
          ? workspace->settings().validateParameterCount
          : true;
        if (!workspace)
        {
          return vertexadt::codeActions(snapshot, rawParams, callParamValidation);
        }
        const vertexadt::ParentUiContext parents{
          .layout = workspace->layout(),
          .index = &workspace->index(),
          .inferParents = workspace->settings().inferIncludedFrom,
        };
        return vertexadt::codeActions(snapshot, rawParams, callParamValidation, &parents);
      });

    // textDocument/codeLens + codeLens/resolve: Impact CodeLens over the
    // workspace index on the heavy pool. Raw JSON both ways (see
    // impact_codelens.h).
    const auto impactContext = [&workspace]() -> std::optional<vertexadt::ImpactContext>
    {
      if (!workspace)
      {
        return std::nullopt;
      }
      const auto settings = workspace->settings();
      return vertexadt::ImpactContext{
        .layout = workspace->layout(),
        .index = workspace->index().snapshot(),
        .enabled = settings.impactAnalysisEnabled,
        .minIncomingCount = settings.impactAnalysisMinIncomingCount,
      };
    };
    endpoint.messageHandler().onCustom<vertexadt::CodeLensRequest>(
      "textDocument/codeLens",
      [&](lsp::json::Value&& rawParams) -> lsp::RequestResult<lsp::json::Value>
      {
        const auto* textDocument = rawParams.isObject()
          ? rawParams.object().find("textDocument")
          : nullptr;
        const auto* uriValue = textDocument && textDocument->isObject()
          ? textDocument->object().find("uri")
          : nullptr;
        if (!uriValue || !uriValue->isString())
        {
          return lsp::json::Value(lsp::json::Array{});
        }
        const auto uri = lsp::Uri::parse(uriValue->string());
        auto snapshot = documents.snapshot(uri);
        const auto version = snapshot ? snapshot->version : 0;
        auto* handler = &endpoint.messageHandler();
        return heavyRequest<lsp::json::Value>(
          scheduler,
          uri,
          version,
          [snapshot = std::move(snapshot), uriText = uriValue->string(), handler, impactContext, &workspace](
            const lsp::RequestId& id) -> lsp::json::Value
          {
            pollHeavyCancel(*handler, id, snapshot);
            const auto context = impactContext();
            if (!context)
            {
              return lsp::json::Value(lsp::json::Array{});
            }
            auto lenses = vertexadt::impactCodeLenses(*context, uriText, snapshot);
            if (snapshot && lenses.isArray() && workspace)
            {
              const vertexadt::ParentUiContext parents{
                .layout = workspace->layout(),
                .index = &workspace->index(),
                .inferParents = workspace->settings().inferIncludedFrom,
              };
              if (auto lens = vertexadt::inferredParentsCodeLens(parents, *snapshot))
              {
                lenses.array().push_back(std::move(*lens));
              }
            }
            return lenses;
          });
      });
    endpoint.messageHandler().onCustom<vertexadt::CodeLensResolveRequest>(
      "codeLens/resolve",
      [&](lsp::json::Value&& rawParams) -> lsp::RequestResult<lsp::json::Value>
      {
        static const auto kCodeLensResolveUri =
          lsp::Uri::parse("vertexadt-workspace:/codeLensResolve");
        auto* handler = &endpoint.messageHandler();
        return heavyRequest<lsp::json::Value>(
          scheduler,
          kCodeLensResolveUri,
          0,
          [lens = std::move(rawParams), handler, impactContext](
            const lsp::RequestId& id) mutable -> lsp::json::Value
          {
            pollHeavyCancel(*handler, id, std::nullopt);
            const auto context = impactContext();
            if (!context)
            {
              return std::move(lens);
            }
            return vertexadt::resolveImpactCodeLens(*context, std::move(lens));
          });
      });

    // workspace/executeCommand: user-invoked workspace walks on the heavy
    // pool. Raw JSON both ways (see workspace_commands.h).
    endpoint.messageHandler().onCustom<vertexadt::ExecuteCommandRequest>(
      "workspace/executeCommand",
      [&](lsp::json::Value&& rawParams) -> lsp::RequestResult<lsp::json::Value>
      {
        const auto* command = rawParams.isObject() ? rawParams.object().find("command") : nullptr;
        const auto commandId = command && command->isString() ? command->string() : std::string();
        const auto assign = commandId == vertexadt::kAssignLocalSlotNamesCommand;
        const auto reports = commandId == vertexadt::kExportWorkspaceReportsCommand;
        const auto exportDiagnostics = commandId == vertexadt::kExportWorkspaceDiagnosticsCommand;
        const auto moveProgram = commandId == vertexadt::kMoveProgramCommand;
        if (!assign && !reports && !exportDiagnostics && !moveProgram
            && commandId != vertexadt::kFixMissingCallParametersCommand)
        {
          throw lsp::RequestError(
            lsp::MessageError::InvalidParams,
            "Unknown command");
        }
        auto* ws = workspace.get();
        auto* handler = &endpoint.messageHandler();
        static const auto kWorkspaceCommandUri =
          lsp::Uri::parse("vertexadt-workspace:/executeCommand");
        return heavyRequest<lsp::json::Value>(
          scheduler,
          kWorkspaceCommandUri,
          0,
          [ws, handler, assign, reports, exportDiagnostics, moveProgram, params = std::move(rawParams)](
            const lsp::RequestId& id)
            -> lsp::json::Value
          {
            const auto pollCancel = [handler, id]()
            {
              if (handler->isCanceled(id))
              {
                throw lsp::RequestError(lsp::MessageError::RequestCancelled, "Canceled");
              }
            };
            if (!ws && moveProgram)
            {
              lsp::json::Object error;
              error["error"] = lsp::json::Value(std::string("No workspace root found"));
              return lsp::json::Value(std::move(error));
            }
            if (moveProgram)
            {
              return vertexadt::moveProgramCommand(*ws, params, pollCancel);
            }
            if (!ws && (reports || exportDiagnostics))
            {
              lsp::json::Object error;
              error["error"] = lsp::json::Value(std::string("No workspace root found"));
              return lsp::json::Value(std::move(error));
            }
            if (!ws)
            {
              lsp::json::Object empty;
              empty["changes"] = lsp::json::Value(lsp::json::Object{});
              empty[assign ? "assignedCount" : "fixedCount"] = lsp::json::Value(0);
              return lsp::json::Value(std::move(empty));
            }
            if (assign)
            {
              return vertexadt::assignLocalSlotNames(*ws, params, pollCancel);
            }
            if (reports)
            {
              return vertexadt::exportWorkspaceReports(*ws, pollCancel);
            }
            if (exportDiagnostics)
            {
              return vertexadt::exportWorkspaceDiagnostics(*ws, pollCancel);
            }
            return vertexadt::fixMissingCallParameters(*ws, pollCancel);
          });
      });

    // workspace/willRenameFiles: program move from explorer drags
    // (docs/adr/0017-native-program-move.md). Text edits only; the report goes
    // to the host as a notification.
    endpoint.messageHandler().onCustom<vertexadt::WillRenameFilesRequest>(
      "workspace/willRenameFiles",
      [&](lsp::json::Value&& rawParams) -> lsp::RequestResult<lsp::json::Value>
      {
        auto* ws = workspace.get();
        auto* handler = &endpoint.messageHandler();
        auto* server = &endpoint;
        static const auto kWillRenameUri = lsp::Uri::parse("vertexadt-workspace:/willRenameFiles");
        return heavyRequest<lsp::json::Value>(
          scheduler,
          kWillRenameUri,
          0,
          [ws, handler, server, params = std::move(rawParams)](const lsp::RequestId& id) -> lsp::json::Value
          {
            if (!ws)
            {
              return {};
            }
            const auto pollCancel = [handler, id]()
            {
              if (handler->isCanceled(id))
              {
                throw lsp::RequestError(lsp::MessageError::RequestCancelled, "Canceled");
              }
            };
            return vertexadt::willRenameFilesEdit(
              *ws,
              params,
              pollCancel,
              [server](lsp::json::Value report)
              {
                server->customNotification(vertexadt::kProgramMoveReportNotification, report);
              });
          });
      });

    vertexadt::registerPullDiagnostics(
      endpoint,
      [&](lsp::DocumentDiagnosticParams&& params)
        -> lsp::RequestResult<vertexadt::PullDiagnosticReport>
      {
        auto snapshot = documents.snapshot(params.textDocument.uri);
        const auto version = snapshot ? snapshot->version : 0;
        if (workspace)
        {
          if (auto cached = workspace->cachedDiagnostics(params.textDocument.uri, version))
          {
            if (params.previousResultId && *params.previousResultId == cached->resultId)
            {
              return vertexadt::PullDiagnosticReport{
                lsp::UnchangedDocumentDiagnosticReport{
                  .kind = "unchanged",
                  .resultId = cached->resultId,
                },
              };
            }
            return vertexadt::PullDiagnosticReport{
              lsp::FullDocumentDiagnosticReport{
                .kind = "full",
                .items = cached->items,
                .resultId = cached->resultId,
              },
            };
          }
        }

        const auto id = lsp::RequestContext::get().id();
        scheduler.trackHeavy(params.textDocument.uri, id, version);

        struct HeavyJob
        {
          vertexadt::WorkScheduler* scheduler = nullptr;
          vertexadt::WorkspaceState* workspace = nullptr;
          lsp::MessageHandler* handler = nullptr;
          lsp::Uri uri;
          lsp::RequestId id;
          std::optional<vertexadt::Snapshot> snapshot;
        };
        auto job = std::make_shared<HeavyJob>(HeavyJob{
          .scheduler = &scheduler,
          .workspace = workspace.get(),
          .handler = &endpoint.messageHandler(),
          .uri = params.textDocument.uri,
          .id = id,
          .snapshot = std::move(snapshot),
        });

        return lsp::TaskFunction<vertexadt::PullDiagnosticReport>([job]() {
          struct Complete
          {
            HeavyJob* job = nullptr;
            ~Complete()
            {
              job->scheduler->completeHeavy(job->uri, job->id);
            }
          } complete{job.get()};

          auto future = job->scheduler->submitHeavy(
            job->uri,
            [job]()
            {
              pollHeavyCancel(*job->handler, job->id, job->snapshot);
              if (!job->workspace || !job->snapshot)
              {
                return vertexadt::emptyFullPullReport();
              }
              auto items = job->workspace->runFullDocumentValidate(
                *job->snapshot,
                [job]()
                {
                  if (job->handler->isCanceled(job->id))
                  {
                    throw lsp::RequestError(
                      lsp::MessageError::RequestCancelled, "Canceled");
                  }
                });
              job->workspace->storeDiagnosticCache(
                job->snapshot->uri, job->snapshot->version, items);
              return vertexadt::PullDiagnosticReport{
                lsp::FullDocumentDiagnosticReport{
                  .kind = "full",
                  .items = std::move(items),
                  .resultId = std::to_string(job->snapshot->version),
                },
              };
            });

          while (future.wait_for(kSlowHeavySlice) != std::future_status::ready)
          {
            lsp::RequestContext::get().throwIfCanceled();
          }
          return future.get();
        });
      });

    endpoint.runMessageLoop();
    // Join the debounce thread (startup walk, refresh hooks) while the
    // endpoint it notifies is still alive. The workspace itself outlives the
    // scheduler, whose pools may still run detached jobs against it.
    if (workspace)
    {
      workspace->stopBackground();
    }
    return EXIT_SUCCESS;
  }
  catch (const std::exception& e)
  {
    logStdErr(e.what());
    return EXIT_FAILURE;
  }
}
