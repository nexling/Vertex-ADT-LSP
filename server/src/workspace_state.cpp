#include "workspace_state.h"

#include "text_util.h"
#include "validate.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace vertexadt {
namespace {

constexpr auto kDebounce = std::chrono::milliseconds(300);

std::string readAll(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in)
  {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string uriKey(const lsp::Uri& uri)
{
  return uri.toString();
}

std::optional<lsp::Uri> rootUriFrom(const lsp::InitializeParams& params)
{
  if (!params.rootUri.isNull())
  {
    return params.rootUri.value();
  }
  return std::nullopt;
}

std::vector<lsp::WorkspaceFolder> foldersFrom(const lsp::InitializeParams& params)
{
  if (!params.workspaceFolders || params.workspaceFolders->isNull())
  {
    return {};
  }
  return params.workspaceFolders->value();
}

int jsonInt(const lsp::json::Value& value, int fallback)
{
  if (value.isInteger())
  {
    return static_cast<int>(value.integer());
  }
  if (value.isNumber())
  {
    return static_cast<int>(value.number());
  }
  return fallback;
}

// Any case-insensitive `call` (a call site or a `$CALLPARAM` read).
bool mentionsCall(std::string_view text)
{
  for (std::size_t i = 0; i + 4 <= text.size(); ++i)
  {
    if ((text[i] | 0x20) == 'c' && (text[i + 1] | 0x20) == 'a' && (text[i + 2] | 0x20) == 'l'
      && (text[i + 3] | 0x20) == 'l')
    {
      return true;
    }
  }
  return false;
}

bool settingsAffectDiagnostics(const lsp::json::Value& settings)
{
  if (!settings.isObject())
  {
    return true;
  }
  const auto& object = settings.object();
  return object.contains("vertexadt")
    || object.contains("diagnostics")
    || object.contains("variables")
    || object.contains("strictMode")
    || object.contains("locals")
    || object.contains("calls")
    || object.contains("maxNumberOfProblems")
    || object.contains("noteUnusedVariables")
    || object.contains("warnVectorToValueTransition")
    || object.contains("warnUsedBeforeAssignment")
    || object.contains("warnUnclosedResources")
    || object.contains("warnInfiniteLoops")
    || object.contains("noteUnusedVariables")
    || object.contains("warnOnRedeclaration")
    || object.contains("validateUniqueNames")
    || object.contains("requireLNamesInCallees")
    || object.contains("validateParameterCount")
    || object.contains("crossCallTypeChecks")
    || object.contains("inferIncludedFrom")
    || object.contains("enabled");
}

std::optional<bool> jsonBool(const lsp::json::Value& value)
{
  if (value.isBoolean())
  {
    return value.boolean();
  }
  return std::nullopt;
}

void applyBool(
  bool& dest,
  const lsp::json::Object& object,
  const char* key)
{
  if (const auto* flag = object.find(key))
  {
    if (auto parsed = jsonBool(*flag))
    {
      dest = *parsed;
    }
  }
}

} // namespace

WorkspaceState::WorkspaceState(
  DocumentStore& documents,
  WorkScheduler& scheduler,
  RefreshHooks hooks)
  : documents_(documents)
  , scheduler_(scheduler)
  , hooks_(std::move(hooks))
  , debounceThread_([this] { runDebounceLoop(); })
{
}

WorkspaceState::~WorkspaceState()
{
  stopBackground();
}

void WorkspaceState::stopBackground()
{
  stopping_ = true;
  {
    const auto lock = std::lock_guard(debounceMutex_);
    stop_ = true;
  }
  debounceCv_.notify_all();
  if (debounceThread_.joinable())
  {
    debounceThread_.join();
  }
}

void WorkspaceState::applySettingsJson(const lsp::json::Value& value)
{
  if (!value.isObject())
  {
    return;
  }
  const auto& object = value.object();
  if (const auto* nested = object.find("vertexadt"))
  {
    applySettingsJson(*nested);
  }
  if (const auto* snippets = object.find("snippets"); snippets && snippets->isObject())
  {
    if (const auto* flag = snippets->object().find("useExpandedSyntax"); flag && flag->isBoolean())
    {
      settings_.useExpandedSyntax = flag->boolean();
    }
  }
  if (const auto* flag = object.find("useExpandedSyntax"); flag && flag->isBoolean())
  {
    settings_.useExpandedSyntax = flag->boolean();
  }
  if (const auto* uri = object.find("extensionUri"); uri && uri->isString())
  {
    settings_.extensionUri = uri->string();
  }
  if (const auto* diagnostics = object.find("diagnostics"); diagnostics && diagnostics->isObject())
  {
    const auto& nested = diagnostics->object();
    if (const auto* max = nested.find("maxNumberOfProblems"))
    {
      settings_.maxNumberOfProblems = jsonInt(*max, settings_.maxNumberOfProblems);
    }
    applyBool(settings_.warnUnclosedResources, nested, "warnUnclosedResources");
    applyBool(settings_.warnInfiniteLoops, nested, "warnInfiniteLoops");
    const auto* customOnly = nested.find("exportCustomOnly");
    settings_.exportCustomOnly =
      !(customOnly && customOnly->isBoolean() && !customOnly->boolean());
  }
  if (const auto* max = object.find("maxNumberOfProblems"))
  {
    settings_.maxNumberOfProblems = jsonInt(*max, settings_.maxNumberOfProblems);
  }
  applyBool(settings_.warnUnclosedResources, object, "warnUnclosedResources");
  applyBool(settings_.warnInfiniteLoops, object, "warnInfiniteLoops");
  if (const auto* variables = object.find("variables"); variables && variables->isObject())
  {
    const auto& nested = variables->object();
    applyBool(settings_.warnVectorToValueTransition, nested, "warnVectorToValueTransition");
    applyBool(settings_.warnUsedBeforeAssignment, nested, "warnUsedBeforeAssignment");
    applyBool(settings_.noteUnusedVariables, nested, "noteUnusedVariables");
  }
  applyBool(settings_.warnVectorToValueTransition, object, "warnVectorToValueTransition");
  applyBool(settings_.warnUsedBeforeAssignment, object, "warnUsedBeforeAssignment");
  applyBool(settings_.noteUnusedVariables, object, "noteUnusedVariables");
  if (const auto* strict = object.find("strictMode"); strict && strict->isObject())
  {
    applyBool(settings_.strictModeEnabled, strict->object(), "enabled");
    applyBool(settings_.crossCallTypeChecks, strict->object(), "crossCallTypeChecks");
  }
  applyBool(settings_.crossCallTypeChecks, object, "crossCallTypeChecks");
  applyBool(settings_.inferIncludedFrom, object, "inferIncludedFrom");
  if (const auto* locals = object.find("locals"); locals && locals->isObject())
  {
    const auto& nested = locals->object();
    applyBool(settings_.warnOnRedeclaration, nested, "warnOnRedeclaration");
    applyBool(settings_.validateUniqueNames, nested, "validateUniqueNames");
    if (const auto* flag = nested.find("requireLNamesInCallees"))
    {
      settings_.requireLNamesInCallees = jsonBool(*flag);
    }
  }
  applyBool(settings_.warnOnRedeclaration, object, "warnOnRedeclaration");
  applyBool(settings_.validateUniqueNames, object, "validateUniqueNames");
  if (const auto* flag = object.find("requireLNamesInCallees"))
  {
    settings_.requireLNamesInCallees = jsonBool(*flag);
  }
  if (const auto* calls = object.find("calls"); calls && calls->isObject())
  {
    applyBool(settings_.validateParameterCount, calls->object(), "validateParameterCount");
  }
  applyBool(settings_.validateParameterCount, object, "validateParameterCount");
  if (const auto* highlighting = object.find("semanticHighlighting");
      highlighting && highlighting->isObject())
  {
    applyBool(settings_.semanticHighlightingEnabled, highlighting->object(), "enabled");
  }
  applyBool(settings_.semanticHighlightingEnabled, object, "semanticHighlighting");
  if (const auto* debug = object.find("debug"); debug && debug->isObject())
  {
    applyBool(settings_.debugValidationCommands, debug->object(), "validationCommands");
  }
  applyBool(settings_.debugValidationCommands, object, "debugValidationCommands");
  if (const auto* completion = object.find("completion"); completion && completion->isObject())
  {
    applyBool(settings_.macroCodeFirst, completion->object(), "macroCodeFirst");
  }
  applyBool(settings_.macroCodeFirst, object, "macroCodeFirst");
  if (const auto* format = object.find("format"); format && format->isObject())
  {
    applyBool(settings_.formatEnabled, format->object(), "enable");
  }
  if (const auto* impact = object.find("impactAnalysis"); impact && impact->isObject())
  {
    const auto& nested = impact->object();
    const auto* enabled = nested.find("enabled");
    settings_.impactAnalysisEnabled = !(enabled && enabled->isBoolean() && !enabled->boolean());
    const auto* minCount = nested.find("minIncomingCount");
    double threshold = 2;
    if (minCount && minCount->isNumber() && minCount->number() >= 1)
    {
      threshold = minCount->number();
    }
    settings_.impactAnalysisMinIncomingCount = threshold;
  }
}

void WorkspaceState::initialize(const lsp::InitializeParams& params)
{
  layout_.setFolders(foldersFrom(params), rootUriFrom(params));
  if (params.initializationOptions)
  {
    const auto lock = std::lock_guard(settingsMutex_);
    applySettingsJson(*params.initializationOptions);
  }
}

auto WorkspaceState::settings() const -> LanguageServerSettings
{
  const auto lock = std::lock_guard(settingsMutex_);
  return settings_;
}

auto WorkspaceState::documents() -> DocumentStore&
{
  return documents_;
}

auto WorkspaceState::layout() const -> const LibraryLayout&
{
  return layout_;
}

auto WorkspaceState::extracts() -> ExtractStore&
{
  return extracts_;
}

auto WorkspaceState::index() -> WorkspaceIndex&
{
  return index_;
}

auto WorkspaceState::linkedFiles() -> LinkedFileStore&
{
  return linkedFiles_;
}

auto WorkspaceState::crossCall() -> CrossCallResolver&
{
  return crossCall_;
}

void WorkspaceState::didOpen(const lsp::DidOpenTextDocumentParams& params)
{
  documents_.open(params);
  // Debug traces: no linked copy, extracts, includes or indexing
  // (docs/adr/0025-debug-trace-fast-path.md).
  if (documents_.isDebugTrace(params.textDocument.uri))
  {
    return;
  }
  auto snapshot = documents_.snapshot(params.textDocument.uri);
  if (!snapshot)
  {
    return;
  }
  dropDiagnosticCache(snapshot->uri);
  const auto normalized = normalizeFsPath(snapshot->uri.fsPath());
  linkedFiles_.put(normalized, snapshot->text, true);
  scheduleExtracts(*snapshot, true);
  scheduleIndexNow(*snapshot);
}

void WorkspaceState::didChange(const lsp::DidChangeTextDocumentParams& params)
{
  documents_.change(params);
  if (documents_.isDebugTrace(params.textDocument.uri))
  {
    return;
  }
  auto snapshot = documents_.snapshot(params.textDocument.uri);
  if (!snapshot)
  {
    return;
  }
  dropDiagnosticCache(snapshot->uri);
  const auto normalized = normalizeFsPath(snapshot->uri.fsPath());
  linkedFiles_.put(normalized, snapshot->text, true);
  dirtyNeighborhoodsTouching(normalized);

  const auto previousNeighborhood = extracts_.neighborhood(snapshot->uri);
  const auto previousGraph = previousNeighborhood
    ? previousNeighborhood->includedFromPaths
    : std::vector<std::string>{};
  const auto opened = *snapshot;
  scheduleExtracts(opened, false);
  scheduler_.postCheapDetached([this, opened, previousGraph]() {
    const auto nextGraph = fillIncludeNeighborhoodExtract(
      opened.text,
      pathFromUri(opened.uri),
      layout_,
      &index_,
      settings().inferIncludedFrom,
      [this](const std::filesystem::path& path) { return textForPath(path); }).includedFromPaths;
    if (nextGraph != previousGraph)
    {
      extracts_.markNeighborhoodDirty(opened.uri);
      refillNeighborhood(opened);
    }
  });

  DebounceBatch batch;
  batch.indexUris.insert(uriKey(snapshot->uri));
  if (isCrossFileCapable(snapshot->text))
  {
    batch.neighborRefresh = true;
  }
  batch.codeLensRefresh = true;
  scheduleDebounced(std::move(batch));
}

void WorkspaceState::didClose(const lsp::DidCloseTextDocumentParams& params)
{
  const auto uri = params.textDocument.uri;
  if (documents_.isDebugTrace(uri))
  {
    documents_.close(uri);
    return;
  }
  const auto normalized = normalizeFsPath(uri.fsPath());
  documents_.close(uri);
  extracts_.eraseLive(uri);
  extracts_.eraseNeighborhood(uri);
  dropDiagnosticCache(uri);
  linkedFiles_.drop(normalized);
  dirtyNeighborhoodsTouching(normalized);

  DebounceBatch batch;
  batch.diskPaths.insert(uri.fsPath());
  batch.neighborRefresh = true;
  batch.codeLensRefresh = true;
  scheduleDebounced(std::move(batch));
}

void WorkspaceState::didChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params)
{
  DebounceBatch batch;
  for (const auto& event : params.changes)
  {
    const auto path = pathFromUri(event.uri);
    const auto normalized = normalizeFsPath(path.string());
    if (documents_.isOpenNormalizedPath(normalized))
    {
      continue;
    }
    if (layout_.isUserTree(path))
    {
      continue;
    }
    linkedFiles_.drop(normalized);
    dirtyNeighborhoodsTouching(normalized);
    // A created parent may now resolve an `##included_from` that did not.
    prepCache_.clear();
    batch.diskPaths.insert(path.string());
    batch.neighborRefresh = true;
    batch.codeLensRefresh = true;
  }
  if (!batch.diskPaths.empty())
  {
    scheduleDebounced(std::move(batch));
  }
}

void WorkspaceState::didChangeConfiguration(const lsp::DidChangeConfigurationParams& params)
{
  bool tokensChanged = false;
  bool impactChanged = false;
  bool inferChanged = false;
  {
    const auto lock = std::lock_guard(settingsMutex_);
    const auto beforeHighlight = settings_.semanticHighlightingEnabled;
    const auto beforeDebug = settings_.debugValidationCommands;
    const auto beforeImpactEnabled = settings_.impactAnalysisEnabled;
    const auto beforeImpactMin = settings_.impactAnalysisMinIncomingCount;
    const auto beforeInfer = settings_.inferIncludedFrom;
    applySettingsJson(params.settings);
    inferChanged = beforeInfer != settings_.inferIncludedFrom;
    tokensChanged = beforeHighlight != settings_.semanticHighlightingEnabled
      || beforeDebug != settings_.debugValidationCommands;
    impactChanged = beforeImpactEnabled != settings_.impactAnalysisEnabled
      || beforeImpactMin != settings_.impactAnalysisMinIncomingCount;
  }
  prepCache_.clear();
  if (inferChanged)
  {
    refillAllOpenNeighborhoods();
  }
  const auto diagnostics = settingsAffectDiagnostics(params.settings);
  if (diagnostics)
  {
    dropAllDiagnosticCaches();
    notifyDiagnosticRefresh();
  }
  if (!diagnostics || impactChanged)
  {
    notifyCodeLensRefresh();
  }
  if (tokensChanged || !diagnostics)
  {
    notifyTokenRefresh();
  }
}

void WorkspaceState::didChangeWorkspaceFolders(const lsp::DidChangeWorkspaceFoldersParams& params)
{
  auto folders = layout_.folders();
  for (const auto& removed : params.event.removed)
  {
    std::erase_if(folders, [&](const lsp::WorkspaceFolder& folder) {
      return folder.uri == removed.uri;
    });
  }
  folders.insert(folders.end(), params.event.added.begin(), params.event.added.end());
  layout_.setFolders(std::move(folders), std::nullopt);
  prepCache_.clear();
  DebounceBatch batch;
  batch.fullRebuild = true;
  batch.dropAllDiagnostics = true;
  batch.tokensRefresh = true;
  batch.codeLensRefresh = true;
  scheduleDebounced(std::move(batch));
}

void WorkspaceState::dropDiagnosticCache(const lsp::Uri& uri)
{
  const auto lock = std::lock_guard(diagnosticMutex_);
  diagnosticCache_.erase(uriKey(uri));
  prepCache_.erase(uriKey(uri));
}

void WorkspaceState::dropAllDiagnosticCaches()
{
  const auto lock = std::lock_guard(diagnosticMutex_);
  diagnosticCache_.clear();
  prepCache_.clear();
}

auto WorkspaceState::cachedDiagnostics(const lsp::Uri& uri, int version) const
  -> std::optional<CachedDiagnosticReport>
{
  const auto lock = std::lock_guard(diagnosticMutex_);
  const auto it = diagnosticCache_.find(uriKey(uri));
  if (it == diagnosticCache_.end() || it->second.version != version)
  {
    return std::nullopt;
  }
  return it->second;
}

void WorkspaceState::storeDiagnosticCache(
  const lsp::Uri& uri,
  int version,
  std::vector<lsp::Diagnostic> items)
{
  const auto live = documents_.snapshot(uri);
  if (!live || live->version != version)
  {
    return;
  }
  const auto lock = std::lock_guard(diagnosticMutex_);
  const auto key = uriKey(uri);
  const auto it = diagnosticCache_.find(key);
  if (it != diagnosticCache_.end() && it->second.version > version)
  {
    return;
  }
  diagnosticCache_[key] = CachedDiagnosticReport{
    .version = version,
    .resultId = std::to_string(version),
    .items = std::move(items),
  };
}

auto WorkspaceState::runFullDocumentValidate(
  const Snapshot& snapshot,
  std::function<void()> throwIfCanceled) -> std::vector<lsp::Diagnostic>
{
  const auto settings = this->settings();
  ValidateContext ctx;
  ctx.settings = ValidateSettings{
    .maxNumberOfProblems = settings.maxNumberOfProblems,
    .warnVectorToValueTransition = settings.warnVectorToValueTransition,
    .warnUsedBeforeAssignment = settings.warnUsedBeforeAssignment,
    .noteUnusedVariables = settings.noteUnusedVariables,
    .strictModeEnabled = settings.strictModeEnabled,
    .warnUnclosedResources = settings.warnUnclosedResources,
    .warnInfiniteLoops = settings.warnInfiniteLoops,
    .warnOnRedeclaration = settings.warnOnRedeclaration,
    .validateUniqueNames = settings.validateUniqueNames,
    .validateParameterCount = settings.validateParameterCount,
    .requireLNamesInCallees = settings.requireLNamesInCallees,
    .crossCallTypeChecks = settings.crossCallTypeChecks,
    .inferIncludedFrom = settings.inferIncludedFrom,
  };
  ctx.stores = ValidateStores{
    .extracts = &extracts_,
    .linkedFiles = &linkedFiles_,
    .index = &index_,
    .layout = &layout_,
    .textForPath = [this](const std::filesystem::path& path) { return textForPath(path); },
    .includedParents = &includedParents_,
    .crossCall = &crossCall_,
  };
  ctx.throwIfCanceled = std::move(throwIfCanceled);
  const auto strict = ctx.settings.strictModeEnabled;
  const auto cancel = ctx.throwIfCanceled;
  return fullDocumentValidate(std::move(ctx), snapshot, [this, &snapshot, strict, cancel]() {
    return documentPrep(snapshot, strict, cancel);
  });
}

auto WorkspaceState::documentPrep(
  const Snapshot& snapshot,
  bool strictModeEnabled,
  const std::function<void()>& throwIfCanceled) -> std::shared_ptr<const DocumentPrep>
{
  ValidateSettings settings;
  settings.strictModeEnabled = strictModeEnabled;
  settings.inferIncludedFrom = this->settings().inferIncludedFrom;
  const ValidateStores stores{
    .extracts = &extracts_,
    .linkedFiles = &linkedFiles_,
    .index = &index_,
    .layout = &layout_,
    .textForPath = [this](const std::filesystem::path& path) { return textForPath(path); },
    .includedParents = &includedParents_,
    .crossCall = &crossCall_,
  };
  return prepCache_.getOrBuild(
    uriKey(snapshot.uri),
    snapshot.version,
    strictModeEnabled,
    [this, &snapshot]() { return extracts_.neighborhoodGeneration(snapshot.uri); },
    [&]() { return buildDocumentPrep(snapshot, settings, stores, throwIfCanceled); },
    throwIfCanceled);
}

auto WorkspaceState::runExportDocumentValidate(
  const Snapshot& snapshot,
  int maxNumberOfProblems,
  std::function<void()> throwIfCanceled) -> std::vector<lsp::Diagnostic>
{
  const auto settings = this->settings();
  ValidateContext ctx;
  ctx.settings = ValidateSettings{
    .maxNumberOfProblems = maxNumberOfProblems,
    .warnVectorToValueTransition = settings.warnVectorToValueTransition,
    .warnUsedBeforeAssignment = settings.warnUsedBeforeAssignment,
    .noteUnusedVariables = settings.noteUnusedVariables,
    .strictModeEnabled = settings.strictModeEnabled,
    .warnUnclosedResources = settings.warnUnclosedResources,
    .warnInfiniteLoops = settings.warnInfiniteLoops,
    .warnOnRedeclaration = settings.warnOnRedeclaration,
    .validateUniqueNames = settings.validateUniqueNames,
    .validateParameterCount = settings.validateParameterCount,
    .requireLNamesInCallees = settings.requireLNamesInCallees,
    .crossCallTypeChecks = settings.crossCallTypeChecks,
    .inferIncludedFrom = settings.inferIncludedFrom,
  };
  ctx.stores = ValidateStores{
    .extracts = nullptr,
    .linkedFiles = &linkedFiles_,
    .index = &index_,
    .layout = &layout_,
    .textForPath = [this](const std::filesystem::path& path) { return textForPath(path); },
    .includedParents = &includedParents_,
    .crossCall = &crossCall_,
  };
  ctx.throwIfCanceled = std::move(throwIfCanceled);
  return fullDocumentValidate(std::move(ctx), snapshot);
}

auto WorkspaceState::watcherGlobs() const -> std::vector<std::string>
{
  std::vector<std::string> globs{"**/system/**"};
  const auto root = layout_.workspaceRoot();
  if (root.empty())
  {
    globs.insert(globs.begin(), "**/custom*/**");
    return globs;
  }
  const auto base = root.filename().string();
  if (base.size() >= 6 && toLowerAscii(base).starts_with("custom"))
  {
    globs.insert(globs.begin(), "**");
  }
  else
  {
    globs.insert(globs.begin(), "**/" + layout_.customFolderName() + "/**");
  }
  return globs;
}

void WorkspaceState::scheduleExtracts(const Snapshot& snapshot, bool prefetchNeighborhood)
{
  scheduler_.postCheapDetached([this, snapshot]() {
    extracts_.putLive(snapshot.uri, snapshot.version, fillLiveBufferExtract(snapshot.text));
  });
  if (prefetchNeighborhood || extracts_.neighborhoodDirty(snapshot.uri))
  {
    scheduler_.postCheapDetached([this, snapshot]() { refillNeighborhood(snapshot); });
  }
}

void WorkspaceState::scheduleIndexNow(const Snapshot& snapshot)
{
  scheduler_.postCheapDetached([this, snapshot]() {
    indexSnapshot(snapshot);
    index_.refreshCalleeFlags();
  });
}

void WorkspaceState::scheduleDebounced(DebounceBatch extra)
{
  {
    const auto lock = std::lock_guard(debounceMutex_);
    pending_.indexUris.merge(extra.indexUris);
    pending_.diskPaths.merge(extra.diskPaths);
    pending_.neighborRefresh |= extra.neighborRefresh;
    pending_.dropAllDiagnostics |= extra.dropAllDiagnostics;
    pending_.tokensRefresh |= extra.tokensRefresh;
    pending_.codeLensRefresh |= extra.codeLensRefresh;
    pending_.fullRebuild |= extra.fullRebuild;
    pending_.startupIndex |= extra.startupIndex;
    deadline_ = std::chrono::steady_clock::now() + kDebounce;
  }
  debounceCv_.notify_one();
}

void WorkspaceState::runDebounceLoop()
{
  auto lock = std::unique_lock(debounceMutex_);
  while (!stop_)
  {
    if (!deadline_)
    {
      debounceCv_.wait(lock, [this] { return stop_ || deadline_.has_value(); });
      continue;
    }
    if (debounceCv_.wait_until(lock, *deadline_) == std::cv_status::timeout)
    {
      auto batch = std::move(pending_);
      pending_ = {};
      deadline_.reset();
      lock.unlock();
      try
      {
        flush(std::move(batch));
      }
      catch (...)
      {
        // A stopped walk, or a filesystem error; the next batch retries.
      }
      lock.lock();
    }
  }
}

void WorkspaceState::flush(DebounceBatch batch)
{
  const auto pollStopping = [this]() {
    if (stopping_)
    {
      throw std::runtime_error("workspace shutting down");
    }
  };
  if (batch.fullRebuild)
  {
    // Clears the index, linked-file snapshots, neighborhoods and diagnostic
    // caches, then re-walks Custom+System (open buffers win).
    walkLibraryTrees(pollStopping, true);
  }
  else if (batch.startupIndex)
  {
    walkLibraryTrees(pollStopping, false);
  }

  for (const auto& uriText : batch.indexUris)
  {
    if (auto snapshot = documents_.snapshot(lsp::Uri::parse(uriText)))
    {
      indexSnapshot(*snapshot);
    }
  }
  for (const auto& pathText : batch.diskPaths)
  {
    indexDiskPath(std::filesystem::path(pathText));
  }
  if (!batch.indexUris.empty() || !batch.diskPaths.empty() || batch.fullRebuild
      || batch.startupIndex)
  {
    index_.refreshCalleeFlags();
  }
  bool callGraphDropped = false;
  if (!batch.indexUris.empty() || !batch.diskPaths.empty())
  {
    auto changed = batch.indexUris;
    for (const auto& pathText : batch.diskPaths)
    {
      changed.insert(uriKey(uriFromPath(std::filesystem::path(pathText))));
    }
    callGraphDropped = dropCallGraphCaches(changed);
  }
  if (batch.dropAllDiagnostics)
  {
    dropAllDiagnosticCaches();
  }
  if (batch.neighborRefresh || batch.dropAllDiagnostics || batch.fullRebuild
      || batch.startupIndex || callGraphDropped)
  {
    notifyDiagnosticRefresh();
  }
  if (batch.tokensRefresh || batch.fullRebuild || callGraphDropped)
  {
    notifyTokenRefresh();
  }
  if (batch.codeLensRefresh)
  {
    notifyCodeLensRefresh();
  }
}

bool WorkspaceState::dropCallGraphCaches(const std::unordered_set<std::string>& changedUris)
{
  bool participates = false;
  for (const auto& uriText : changedUris)
  {
    const auto snapshot = documents_.snapshot(lsp::Uri::parse(uriText));
    // A closed or disk-changed file: its text is gone or unread here.
    if (!snapshot || mentionsCall(snapshot->text))
    {
      participates = true;
      break;
    }
  }
  if (!participates)
  {
    return false;
  }
  bool dropped = false;
  for (const auto& uri : documents_.openUris())
  {
    if (changedUris.contains(uriKey(uri)))
    {
      continue;
    }
    const auto snapshot = documents_.snapshot(uri);
    if (!snapshot || !mentionsCall(snapshot->text))
    {
      continue;
    }
    dropDiagnosticCache(uri);
    dropped = true;
  }
  return dropped;
}

void WorkspaceState::indexSnapshot(const Snapshot& snapshot)
{
  if (isValidationExcluded(snapshot.text))
  {
    if (auto identity = layout_.overlayForUri(snapshot.uri))
    {
      dirtyInferredChildren(index_.removeKey(identity->key));
    }
    return;
  }
  auto identity = layout_.overlayForUri(snapshot.uri);
  if (!identity)
  {
    return;
  }
  if (!layout_.shouldIndex(identity->absPath))
  {
    dirtyInferredChildren(index_.removeKey(identity->key));
    return;
  }
  auto entry = parseIndexEntry(snapshot.text, *identity, layout_);
  for (const auto& link : entry.includeLinks)
  {
    if (auto path = layout_.resolveVertexFilePath(link.target))
    {
      if (auto text = textForPath(*path))
      {
        linkedFiles_.put(normalizeFsPath(path->string()), *text, false);
      }
    }
  }
  for (const auto& link : entry.callLinks)
  {
    if (auto path = layout_.resolveVertexFilePath(link.target))
    {
      if (auto text = textForPath(*path))
      {
        linkedFiles_.put(normalizeFsPath(path->string()), *text, false);
      }
    }
  }
  dirtyInferredChildren(index_.upsert(std::move(entry)));
}

void WorkspaceState::indexDiskPath(const std::filesystem::path& path)
{
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec))
  {
    if (auto identity = layout_.overlayForPath(path))
    {
      dirtyInferredChildren(index_.removeKey(identity->key));
      if (identity->tree == LibraryTree::Custom)
      {
        const auto systemTwin = layout_.workspaceRoot() / "system"
          / std::filesystem::path(identity->key);
        if (std::filesystem::is_regular_file(systemTwin, ec))
        {
          indexDiskPath(systemTwin);
        }
      }
    }
    return;
  }
  if (!layout_.shouldIndex(path))
  {
    if (auto identity = layout_.overlayForPath(path))
    {
      dirtyInferredChildren(index_.removeKey(identity->key));
    }
    return;
  }
  const auto text = readAll(path);
  if (isValidationExcluded(text))
  {
    if (auto identity = layout_.overlayForPath(path))
    {
      dirtyInferredChildren(index_.removeKey(identity->key));
    }
    return;
  }
  auto identity = layout_.overlayForPath(path);
  if (!identity)
  {
    return;
  }
  dirtyInferredChildren(index_.upsert(parseIndexEntry(text, *identity, layout_)));
}

void WorkspaceState::refillNeighborhood(const Snapshot& snapshot)
{
  auto extract = fillIncludeNeighborhoodExtract(
    snapshot.text,
    pathFromUri(snapshot.uri),
    layout_,
    &index_,
    settings().inferIncludedFrom,
    [this](const std::filesystem::path& path) { return textForPath(path); });
  extracts_.putNeighborhood(snapshot.uri, std::move(extract));
}

void WorkspaceState::walkLibraryTrees(const std::function<void()>& pollCancel, bool reset)
{
  if (reset)
  {
    index_.clear();
    linkedFiles_.dropAll();
    extracts_.clearNeighborhoods();
  }
  dropAllDiagnosticCaches();

  for (const auto& path : libraryTreeFiles(true, pollCancel))
  {
    if (pollCancel)
    {
      pollCancel();
    }
    const auto text = textForPath(path);
    if (!text || isValidationExcluded(*text))
    {
      continue;
    }
    if (auto identity = layout_.overlayForPath(path))
    {
      // One file that fails to parse (for example std::regex giving up on a
      // very long line) must not abort the walk and leave the index empty.
      try
      {
        index_.upsert(parseIndexEntry(*text, std::move(*identity), layout_));
      }
      catch (const std::exception& error)
      {
        std::fprintf(stderr, "vertex-adt-ls: index skipped %s: %s\n", path.string().c_str(), error.what());
      }
    }
  }
  index_.refreshCalleeFlags();
  // Neighborhoods filled while the walk ran saw a partial index.
  refillAllOpenNeighborhoods();
}

auto WorkspaceState::libraryTreeFiles(
  bool includeSystem,
  const std::function<void()>& pollCancel) -> std::vector<std::filesystem::path>
{
  std::vector<std::filesystem::path> files;
  const auto& root = layout_.workspaceRoot();
  if (root.empty())
  {
    return files;
  }
  std::vector<std::filesystem::path> trees;
  if (toLowerAscii(root.filename().string()).starts_with("custom"))
  {
    trees.push_back(root);
  }
  else
  {
    trees.push_back(root / layout_.customFolderName());
    if (includeSystem)
    {
      trees.push_back(root / "system");
    }
  }
  // Custom first: an overlay key already seen keeps its Custom file.
  std::unordered_set<std::string> seen;
  for (const auto& tree : trees)
  {
    std::error_code ec;
    if (!std::filesystem::is_directory(tree, ec))
    {
      continue;
    }
    for (std::filesystem::recursive_directory_iterator it(tree, ec), end; it != end && !ec;
         it.increment(ec))
    {
      if (pollCancel)
      {
        pollCancel();
      }
      if (!it->is_regular_file(ec))
      {
        continue;
      }
      const auto& path = it->path();
      if (!layout_.shouldIndex(path))
      {
        continue;
      }
      const auto identity = layout_.overlayForPath(path);
      if (!identity || !seen.insert(identity->key).second)
      {
        continue;
      }
      files.push_back(path);
    }
  }
  return files;
}

void WorkspaceState::rebuildIndex(const std::function<void()>& pollCancel)
{
  walkLibraryTrees(pollCancel, true);
  notifyDiagnosticRefresh();
  notifyTokenRefresh();
  notifyCodeLensRefresh();
}

void WorkspaceState::scheduleFullIndex()
{
  DebounceBatch batch;
  batch.startupIndex = true;
  batch.codeLensRefresh = true;
  scheduleDebounced(std::move(batch));
}

auto WorkspaceState::textForPath(const std::filesystem::path& path) -> std::optional<std::string>
{
  const auto normalized = normalizeFsPath(path.string());
  if (auto open = documents_.snapshotByNormalizedPath(normalized))
  {
    linkedFiles_.put(normalized, open->text, true);
    return open->text;
  }
  if (auto cached = linkedFiles_.get(normalized); cached && !cached->fromBuffer)
  {
    return cached->text;
  }
  const auto text = readAll(path);
  if (text.empty() && !std::filesystem::exists(path))
  {
    return std::nullopt;
  }
  linkedFiles_.put(normalized, text, false);
  return text;
}

void WorkspaceState::dirtyNeighborhoodsTouching(std::string_view normalizedPath)
{
  for (const auto& uri : extracts_.ownersOfIncludedPath(normalizedPath))
  {
    extracts_.markNeighborhoodDirty(uri);
    if (auto snapshot = documents_.snapshot(uri))
    {
      scheduler_.postCheapDetached([this, snapshot = *snapshot]() {
        refillNeighborhood(snapshot);
      });
    }
  }
}

void WorkspaceState::dirtyInferredChildren(const std::vector<std::string>& targets)
{
  if (targets.empty())
  {
    return;
  }
  const std::unordered_set<std::string> wanted(targets.begin(), targets.end());
  for (const auto& uri : documents_.openUris())
  {
    const auto relative = layout_.libraryRelativeFromAbs(pathFromUri(uri));
    if (!relative || !wanted.contains(toLowerAscii(*relative)))
    {
      continue;
    }
    extracts_.markNeighborhoodDirty(uri);
    if (auto snapshot = documents_.snapshot(uri))
    {
      scheduler_.postCheapDetached([this, snapshot = *snapshot]() { refillNeighborhood(snapshot); });
    }
  }
}

void WorkspaceState::refillAllOpenNeighborhoods()
{
  // Marking dirty keeps the previous extract readable (cheap hover and
  // completion) until the refill lands.
  for (const auto& uri : documents_.openUris())
  {
    extracts_.markNeighborhoodDirty(uri);
    if (auto snapshot = documents_.snapshot(uri))
    {
      scheduler_.postCheapDetached([this, snapshot = *snapshot]() { refillNeighborhood(snapshot); });
    }
  }
}

void WorkspaceState::notifyDiagnosticRefresh()
{
  if (hooks_.diagnosticRefresh)
  {
    hooks_.diagnosticRefresh();
  }
}

void WorkspaceState::notifyTokenRefresh()
{
  if (hooks_.semanticTokensRefresh)
  {
    hooks_.semanticTokensRefresh();
  }
}

void WorkspaceState::notifyCodeLensRefresh()
{
  if (hooks_.codeLensRefresh)
  {
    hooks_.codeLensRefresh();
  }
}

} // namespace vertexadt
