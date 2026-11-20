#pragma once

#include "bracket_balance.h"
#include "cross_call_types.h"
#include "document_store.h"
#include "extracts.h"
#include "included_parent_cache.h"
#include "library_paths.h"
#include "line_continuation.h"
#include "linked_files.h"
#include "text_util.h"
#include "workspace_index.h"

#include <lsp/types.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vertexadt {

// This-pass type writes. Family 1 fills collection; later families read it.
struct TypeWrite
{
  std::string name;
  std::string type;
  int line = 0;
  std::string section;
};

struct TypeTimeline
{
  std::vector<TypeWrite> writes;
  // Indexes into `writes` per name, in write order. Filled when the prep is
  // finished (buildDocumentPrep), empty while it is collected.
  std::unordered_map<std::string, std::vector<std::size_t>> byName;
};

struct ValidateSettings
{
  int maxNumberOfProblems = 100;
  bool warnVectorToValueTransition = true;
  bool warnUsedBeforeAssignment = true;
  bool noteUnusedVariables = false;
  bool strictModeEnabled = false;
  bool warnUnclosedResources = true;
  bool warnOnRedeclaration = true;
  bool validateUniqueNames = true;
  bool validateParameterCount = true;
  // nullopt: follow strictModeEnabled (package default is true when set).
  std::optional<bool> requireLNamesInCallees;
  // vertexadt.strictMode.crossCallTypeChecks: cross-call type diagnostics,
  // only while strict mode is on (docs/adr/0009-native-cross-call-types.md).
  bool crossCallTypeChecks = true;
  // vertexadt.inferIncludedFrom: parents inferred from `# include` / `call`
  // links feed the include neighborhood
  // (docs/adr/0015-native-inferred-included-from.md).
  bool inferIncludedFrom = true;
};

// Owned by Implement if/loop/indent matching. Other families must not read it.
struct IfLoopIndentState
{
  struct Frame
  {
    int line = 0;
    int indentation = -1;
  };

  std::vector<Frame> ifStack;
  std::vector<Frame> loopStack;
  std::vector<int> indentStack;
  bool pendingIfHeader = false;
};

struct ValidateStores
{
  ExtractStore* extracts = nullptr;
  LinkedFileStore* linkedFiles = nullptr;
  WorkspaceIndex* index = nullptr;
  const LibraryLayout* layout = nullptr;
  TextLookup textForPath;
  // Null: every `##included_from` parent is re-extracted.
  IncludedParentCache* includedParents = nullptr;
  // Null: no cross-call types (callers' arguments, callees' return fields).
  CrossCallResolver* crossCall = nullptr;
};

struct DbcreateLineFlags
{
  bool isStart = false;
  bool isContinuation = false;
};

// Command syntax: multi-line `dbcreate` joined into one logical line, keyed by
// its first line index; the other lines are continuations.
struct DbcreateLogicalLines
{
  std::unordered_map<int, std::string> logicalLineByStart;
  std::unordered_set<int> continuationLines;
};

// If headers continued with `\` (docs/adr/0014-native-if-header-continuation.md):
// each joined header keyed by its `if` line; the other lines map to it.
struct IfHeaderLines
{
  std::unordered_map<int, JoinedLines> headerByStart;
  std::unordered_map<int, int> startByContinuation;
};

struct PerLineState
{
  int index = 0;
  std::string_view raw;
  std::string stripped;
  std::string_view trimmed;
  std::string lowercaseTrimmed;
  std::string codeLine;
  int indentation = -1;
};

// The prep for one URI+version: lines, sections, live-buffer and
// include-neighborhood extracts, and this-pass type timeline. Built once and
// shared read-only by the editor pull and semantic tokens. `lines` view into
// `snapshot.text`, so a prep is never moved after it is built.
struct DocumentPrep
{
  Snapshot snapshot;
  std::vector<std::string_view> lines;
  std::vector<NamedSection> sections;
  VariableExtract live;
  VariableExtract neighborhood;
  TypeTimeline types;
  // Cross-call facts and resolutions this prep's timeline was built with.
  std::shared_ptr<const CrossCallReport> crossCall;
  // ExtractStore neighborhood generation this prep read or wrote; 0 without a store.
  std::uint64_t neighborhoodGeneration = 0;
  bool strictModeEnabled = false;

  DocumentPrep() = default;
  DocumentPrep(const DocumentPrep&) = delete;
  DocumentPrep& operator=(const DocumentPrep&) = delete;
};

// One URI+version full-document validate. Families that are not implemented
// yet emit nothing; the order, skip, cap, and post-filter still run.
struct ValidateContext
{
  std::shared_ptr<const DocumentPrep> prep;
  // Only while collectTypeTimeline builds a prep: where its writes go.
  TypeTimeline* buildingTypes = nullptr;
  // Only while collectTypeTimeline builds a prep: cross-call writes to add.
  const CrossCallInjection* injection = nullptr;
  ValidateSettings settings;
  ValidateStores stores;
  IfLoopIndentState ifLoop;
  std::vector<lsp::Diagnostic> diagnostics;
  // Used-before-declaration: filled on first use, one entry per line.
  std::vector<DbcreateLineFlags> usedBeforeDbcreateFlags;
  // Strict command-syntax type lookups: indexes into `prep->types.writes` per
  // name, in write order. Keys view into the prep. Filled on the first strict
  // command-syntax line.
  std::unordered_map<std::string_view, std::vector<std::size_t>> typeWritesByName;
  bool typeWritesByNameBuilt = false;
  // Command syntax: filled on first use.
  std::optional<DbcreateLogicalLines> commandSyntaxDbcreateLines;
  // If headers continued with `\`: filled on first use.
  std::optional<IfHeaderLines> ifHeaderLines;
  // Bracket balance: filled on first use (command syntax or its own family).
  std::optional<std::vector<BracketIssue>> bracketIssues;
  int problems = 0;
  std::function<void()> throwIfCanceled;

  [[nodiscard]] bool atCap() const;
  void pollCancel() const;
  void append(lsp::Diagnostic diagnostic, bool ignoreCap = false, bool bumpProblems = true);
};

// Returns the shared prep for a snapshot (from a cache, or freshly built).
using PrepProvider = std::function<std::shared_ptr<const DocumentPrep>()>;

// Lines, sections, extracts, then collectTypeTimeline. Puts the live extract
// and (when dirty) the neighborhood extract into `stores.extracts`. Reads only
// `settings.strictModeEnabled`. A given `injection` is used as is; otherwise
// `stores.crossCall` (when set) analyzes the snapshot's call graph.
[[nodiscard]] auto buildDocumentPrep(
  Snapshot snapshot,
  const ValidateSettings& settings,
  const ValidateStores& stores,
  const std::function<void()>& throwIfCanceled,
  const CrossCallInjection* injection = nullptr) -> std::shared_ptr<const DocumentPrep>;

// `prepare` empty: build the prep without a cache.
[[nodiscard]] auto fullDocumentValidate(
  ValidateContext ctx,
  const Snapshot& snapshot,
  const PrepProvider& prepare = {}) -> std::vector<lsp::Diagnostic>;

// `[type]` of the first-inserted comment for `name` (live-buffer extract, then
// include-neighborhood extract), trimmed and lowercased. Empty when the first
// entry has no comment or no brackets, or the name is in neither extract.
[[nodiscard]] auto commentObjectType(const ValidateContext& ctx, std::string_view name)
  -> std::string;

// Family implementations live in validate_*.cpp. Stubs emit nothing.
void collectTypeTimeline(ValidateContext& ctx);
void familyVectorToValue(ValidateContext& ctx);
void silentIfLoopIndentStackUpdate(ValidateContext& ctx, PerLineState& line);
void familyLeadingDigit(ValidateContext& ctx, PerLineState& line);
void familyIfLogicalAndMatching(ValidateContext& ctx, PerLineState& line);
void familyUsedBeforeDeclaration(ValidateContext& ctx, PerLineState& line);
void familyEqualsAngle(ValidateContext& ctx, PerLineState& line);
void familyCommandSyntax(ValidateContext& ctx, PerLineState& line);
void familyIndentLine(ValidateContext& ctx, PerLineState& line);
void familyLeftoverIf(ValidateContext& ctx);
void familyLeftoverLoop(ValidateContext& ctx);
void familyLeftoverIndent(ValidateContext& ctx);
void familyUnusedVariables(ValidateContext& ctx);
void familyCallParamCount(ValidateContext& ctx);
// validate_cross_call_types.cpp: cross-call argument / comment / conflict checks.
void familyCrossCallTypes(ValidateContext& ctx);
void familyUnknownSection(ValidateContext& ctx);
inline constexpr std::string_view kIncludedFromNotFoundCode = "vertexadt/included-from-not-found";
void familyIncludedFromNotFound(ValidateContext& ctx);
void familyUnclosedResources(ValidateContext& ctx);
void familyInFileDeclarationDuplicates(ValidateContext& ctx);
void familyRedeclaration(ValidateContext& ctx);
void familyLocalSlotsManifest(ValidateContext& ctx);
void familyCalleeCommonRequirements(ValidateContext& ctx);
void familyCrossFileDeclarationConflicts(ValidateContext& ctx);
void familyLocalSlotNameConflicts(ValidateContext& ctx);
void familyCalleeLocalNaming(ValidateContext& ctx);
void familyJointSection(ValidateContext& ctx);
// bracket_balance.cpp: unbalanced `(` `)` / `[` `]`, one per logical line.
void familyBracketBalance(ValidateContext& ctx);
// concat_whitespace.cpp: whitespace touching a concatenation `&`.
void familyConcatWhitespace(ValidateContext& ctx);
// substring_index.cpp: literal `A$[i,j]` index checks and substring assignment.
void familySubstringIndex(ValidateContext& ctx);

// Multi-line `dbcreate(` logical lines for this pass, built on first use.
// Shared by command syntax and the substring index check.
[[nodiscard]] const DbcreateLogicalLines& dbcreateMapFor(ValidateContext& ctx);

// `\`-continued if headers for this pass, built on first use. Shared by the
// if/loop/indent families and command syntax.
[[nodiscard]] const IfHeaderLines& ifHeaderLinesFor(ValidateContext& ctx);

// Bracket-balance issues for this pass, built on first use. Command syntax
// skips lines they cover so a broken line gets one diagnostic.
[[nodiscard]] const std::vector<BracketIssue>& bracketIssuesFor(ValidateContext& ctx);

// listUniqueTargetSectionNames: closed `#NAME#` pairs, drop SECTION /
// SECTIONOFFILE, first duplicate wins, sort(). Shared by the unknown-section
// check and include/call section completion.
[[nodiscard]] auto uniqueTargetSectionNames(std::string_view text) -> std::vector<std::string>;

} // namespace vertexadt
