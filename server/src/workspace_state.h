#pragma once

#include "cross_call_types.h"
#include "document_store.h"
#include "extracts.h"
#include "library_paths.h"
#include "linked_files.h"
#include "prep_cache.h"
#include "work_scheduler.h"
#include "workspace_index.h"

#include <lsp/types.h>
#include <lsp/uri.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vertexadt {

struct CachedDiagnosticReport
{
  int version = 0;
  std::string resultId;
  std::vector<lsp::Diagnostic> items;
};

struct RefreshHooks
{
  std::function<void()> diagnosticRefresh;
  std::function<void()> semanticTokensRefresh;
  std::function<void()> codeLensRefresh;
};

struct LanguageServerSettings
{
  std::string extensionUri;
  bool useExpandedSyntax = false;
  int maxNumberOfProblems = 100;
  bool warnVectorToValueTransition = true;
  bool warnUsedBeforeAssignment = true;
  bool noteUnusedVariables = false;
  bool strictModeEnabled = false;
  bool warnUnclosedResources = true;
  bool warnInfiniteLoops = true;
  bool warnOnRedeclaration = true;
  bool validateUniqueNames = true;
  bool validateParameterCount = true;
  std::optional<bool> requireLNamesInCallees;
  bool semanticHighlightingEnabled = true;
  bool debugValidationCommands = false;
  // vertexadt.impactAnalysis.*: on unless `enabled === false`; the threshold
  // is a number >= 1, else 2.
  bool impactAnalysisEnabled = true;
  double impactAnalysisMinIncomingCount = 2;
  // vertexadt.diagnostics.exportCustomOnly: export-only; on unless `false`.
  bool exportCustomOnly = true;
  // vertexadt.completion.macroCodeFirst: combined `LIBRARY/CODE` items in the
  // 1st argument of new_macro/new_djoint/new_sheet
  // (docs/adr/0008-native-macro-code-first-completion.md).
  bool macroCodeFirst = false;
  // vertexadt.strictMode.crossCallTypeChecks: cross-call type diagnostics
  // while strict mode is on (docs/adr/0009-native-cross-call-types.md).
  bool crossCallTypeChecks = true;
  // vertexadt.format.enable: indentation formatter answers formatting
  // requests (docs/adr/0013-native-indentation-formatter.md).
  bool formatEnabled = true;
  // vertexadt.inferIncludedFrom (docs/adr/0015-native-inferred-included-from.md).
  bool inferIncludedFrom = true;
};

// Owns live buffers, workspace index, extracts, linked-file snapshots, and the
// 300 ms invalidation timer. Message-loop methods update buffers and schedule
// work; workers read snapshots / shared_ptr copies.
class WorkspaceState
{
public:
  WorkspaceState(DocumentStore& documents, WorkScheduler& scheduler, RefreshHooks hooks);
  ~WorkspaceState();

  WorkspaceState(const WorkspaceState&) = delete;
  WorkspaceState& operator=(const WorkspaceState&) = delete;

  void initialize(const lsp::InitializeParams& params);
  void didOpen(const lsp::DidOpenTextDocumentParams& params);
  void didChange(const lsp::DidChangeTextDocumentParams& params);
  void didClose(const lsp::DidCloseTextDocumentParams& params);
  void didChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params);
  void didChangeConfiguration(const lsp::DidChangeConfigurationParams& params);
  void didChangeWorkspaceFolders(const lsp::DidChangeWorkspaceFoldersParams& params);

  [[nodiscard]] auto documents() -> DocumentStore&;
  [[nodiscard]] auto layout() const -> const LibraryLayout&;
  [[nodiscard]] auto extracts() -> ExtractStore&;
  [[nodiscard]] auto index() -> WorkspaceIndex&;
  [[nodiscard]] auto linkedFiles() -> LinkedFileStore&;
  [[nodiscard]] auto crossCall() -> CrossCallResolver&;
  [[nodiscard]] auto settings() const -> LanguageServerSettings;

  void dropDiagnosticCache(const lsp::Uri& uri);
  void dropAllDiagnosticCaches();
  [[nodiscard]] auto cachedDiagnostics(const lsp::Uri& uri, int version) const
    -> std::optional<CachedDiagnosticReport>;
  void storeDiagnosticCache(
    const lsp::Uri& uri,
    int version,
    std::vector<lsp::Diagnostic> items);
  // The shared URI+version prep (lines, sections, extracts, type timeline)
  // the editor pull and semantic tokens both read; built at most once per
  // URI+version while the include neighborhood and settings hold still.
  [[nodiscard]] auto documentPrep(
    const Snapshot& snapshot,
    bool strictModeEnabled,
    const std::function<void()>& throwIfCanceled) -> std::shared_ptr<const DocumentPrep>;

  [[nodiscard]] auto runFullDocumentValidate(
    const Snapshot& snapshot,
    std::function<void()> throwIfCanceled) -> std::vector<lsp::Diagnostic>;
  // Workspace diagnostics export (Node writeWorkspaceDiagnosticsCsv): the
  // live gates with the problem cap raised to `maxNumberOfProblems`. The
  // extract store is bypassed, so export never overwrites live-buffer or
  // neighborhood extracts and holds nothing for unopened files.
  [[nodiscard]] auto runExportDocumentValidate(
    const Snapshot& snapshot,
    int maxNumberOfProblems,
    std::function<void()> throwIfCanceled) -> std::vector<lsp::Diagnostic>;
  // Indexable ADT files under the Custom tree, then System when
  // `includeSystem` (Node walkWorkspaceAdtFiles): directory order, and an
  // overlay key already seen keeps its first (Custom) file.
  [[nodiscard]] auto libraryTreeFiles(bool includeSystem, const std::function<void()>& pollCancel)
    -> std::vector<std::filesystem::path>;
  [[nodiscard]] auto watcherGlobs() const -> std::vector<std::string>;
  // Full Custom+System index rebuild (Node `rebuildWorkspaceIndex`), used by
  // user-invoked workspace commands: drops the index, linked-file snapshots,
  // include neighborhoods and diagnostic caches, re-parses every indexable
  // file (buffer wins over disk, validation-excluded skipped), refreshes the
  // callee flags, then asks the client to re-pull diagnostics and tokens.
  void rebuildIndex(const std::function<void()>& pollCancel);
  // Startup full Custom+System index walk (Node `rebuildWorkspaceIndex` in
  // onInitialized), on the debounce thread so no request pool waits on it.
  // Only the index fills (open buffers win); include neighborhoods and
  // linked-file snapshots stay. Afterwards diagnostic caches drop and
  // diagnostics and code lenses are refreshed.
  void scheduleFullIndex();
  // Stops a running walk and joins the debounce thread. Idempotent; the
  // destructor calls it too.
  void stopBackground();
  // Linked-file text, buffer-wins over disk (cached in the linked-file store).
  auto textForPath(const std::filesystem::path& path) -> std::optional<std::string>;

private:
  struct DebounceBatch
  {
    std::unordered_set<std::string> indexUris;
    std::unordered_set<std::string> diskPaths;
    bool neighborRefresh = false;
    bool dropAllDiagnostics = false;
    bool tokensRefresh = false;
    bool codeLensRefresh = false;
    bool fullRebuild = false;
    bool startupIndex = false;
  };

  void scheduleExtracts(const Snapshot& snapshot, bool prefetchNeighborhood);
  void scheduleIndexNow(const Snapshot& snapshot);
  void scheduleDebounced(DebounceBatch extra);
  void runDebounceLoop();
  void flush(DebounceBatch batch);
  void walkLibraryTrees(const std::function<void()>& pollCancel, bool reset);
  void indexSnapshot(const Snapshot& snapshot);
  void indexDiskPath(const std::filesystem::path& path);
  void refillNeighborhood(const Snapshot& snapshot);
  void dirtyNeighborhoodsTouching(std::string_view normalizedPath);
  // Open documents whose library key is in `targets` gained or lost an
  // incoming `# include` / `call` link: their inferred parents changed.
  void dirtyInferredChildren(const std::vector<std::string>& targets);
  void refillAllOpenNeighborhoods();
  void notifyDiagnosticRefresh();
  // Drops prep and diagnostic caches of other open documents in the call
  // graph, so cross-call types pick up the edit. True when any were open.
  bool dropCallGraphCaches(const std::unordered_set<std::string>& changedUris);
  void notifyTokenRefresh();
  void notifyCodeLensRefresh();
  void applySettingsJson(const lsp::json::Value& value);

  DocumentStore& documents_;
  WorkScheduler& scheduler_;
  RefreshHooks hooks_;
  LibraryLayout layout_;
  ExtractStore extracts_;
  WorkspaceIndex index_;
  LinkedFileStore linkedFiles_;

  mutable std::mutex settingsMutex_;
  LanguageServerSettings settings_;

  mutable std::mutex diagnosticMutex_;
  std::unordered_map<std::string, CachedDiagnosticReport> diagnosticCache_;
  PrepCache prepCache_;
  IncludedParentCache includedParents_;
  CrossCallResolver crossCall_;

  std::mutex debounceMutex_;
  std::condition_variable debounceCv_;
  bool stop_ = false;
  // Set before the debounce thread is joined; long walks poll it.
  std::atomic<bool> stopping_ = false;
  DebounceBatch pending_;
  std::optional<std::chrono::steady_clock::time_point> deadline_;
  std::thread debounceThread_;
};

} // namespace vertexadt
