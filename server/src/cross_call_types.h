#pragma once

#include "extracts.h"
#include "library_paths.h"

#include <lsp/types.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vertexadt {

class WorkspaceIndex;
class IncludedParentCache;
struct DocumentPrep;
struct ValidateSettings;

// Cross-call types (docs/adr/0009-native-cross-call-types.md): object types
// that flow over `call(` edges. Caller argument types reach the callee's
// `name = get($CALLPARAMn)` reads, and callee `"KEY=" & var` return fields
// reach the caller's `X$ = strfield(R$,"KEY")` / `Y = X$` receivers.

// ---------------------------------------------------------------------------
// Pure per-text facts.

struct CallArgFact
{
  std::string text;      // trimmed argument text
  std::size_t start = 0; // byte offsets on the raw line, trimmed
  std::size_t end = 0;
};

struct CallSiteFact
{
  int line = 0;
  std::string section;
  std::string calleePath;    // path part of the first argument
  std::string targetSection; // named-section suffix, empty for whole file
  std::vector<CallArgFact> args; // after the path
  std::string assignedTo;        // `R` of `R$ = call(...)`, else empty
  bool ignoreType = false;       // IGNORE TYPE marker on the line
};

struct ParamReadFact
{
  int line = 0;
  std::string section;
  std::string name; // left-hand name, never ends in `$`
  int index = 0;    // n of `$CALLPARAMn`
  std::size_t nameStart = 0;
  std::size_t nameEnd = 0;
  std::string commentType; // `[type]` of the line's `;!"…"` comment, lowercased
  std::size_t commentStart = 0; // `;!` … end of line, when commentType is set
  std::size_t commentEnd = 0;
};

struct ReturnFieldFact
{
  int line = 0;
  std::string section;
  std::string key;
  std::string value; // token right after `"KEY=" &`, may be empty
  std::size_t keyStart = 0;
  std::size_t keyEnd = 0;
};

struct ReceiverFact
{
  int line = 0;
  std::string section;
  std::string name; // object name that receives the field (no `$`)
  std::size_t nameStart = 0;
  std::size_t nameEnd = 0;
  std::size_t callSite = 0; // index into CallFacts::calls
  std::string key;
  std::string commentType;
  std::size_t commentStart = 0;
  std::size_t commentEnd = 0;
};

struct CallFacts
{
  std::vector<CallSiteFact> calls;
  std::vector<ParamReadFact> params;
  std::vector<ReturnFieldFact> returnFields;
  std::vector<ReceiverFact> receivers;
  std::vector<std::string> includedFrom; // raw `##included_from` remainders
};

[[nodiscard]] auto extractCallFacts(std::string_view text) -> CallFacts;

// Same type, both in the vector family, or either side untyped/generic.
[[nodiscard]] bool crossCallTypesCompatible(std::string_view a, std::string_view b);

// Object type of a call argument at `line` from a finished type timeline, or
// empty when it carries none (strings, expressions, untyped names).
[[nodiscard]] auto crossCallArgumentType(
  const DocumentPrep& prep,
  std::string_view argument,
  int line) -> std::string;

// ---------------------------------------------------------------------------
// Injection: what collectTypeTimeline adds for this file.

struct CrossCallInjection
{
  struct Write
  {
    std::string name;
    std::string type;
  };
  // Pushed after the line's own writes, so they win on that line.
  std::unordered_map<int, std::vector<Write>> lineWrites;
  // section + '\n' + name -> type; replaces the `[type]` comment fallback.
  std::unordered_map<std::string, std::string> overrides;

  [[nodiscard]] bool empty() const { return lineWrites.empty() && overrides.empty(); }
  [[nodiscard]] auto signature() const -> std::string;
};

// ---------------------------------------------------------------------------
// Report: what the diagnostics family needs for this file.

struct CrossCallLocation
{
  lsp::Uri uri;
  int line = 0;
  std::size_t startCharacter = 0; // UTF-16
  std::size_t endCharacter = 0;
  std::string type;
};

struct CrossCallParam
{
  std::size_t read = 0; // index into facts.params
  std::string type;     // agreed inferred type, empty on conflict or none
  bool conflict = false;
  std::vector<CrossCallLocation> contributions;
};

struct CrossCallReturnField
{
  std::string key;
  bool conflict = false;
  std::vector<CrossCallLocation> lines; // format lines with their value types
};

struct CrossCallArgExpectation
{
  int line = 0;
  std::size_t site = 0; // index into facts.calls
  std::size_t arg = 0;  // index into that site's args
  std::string expectedType;
  CrossCallLocation callee; // the callee's `get($CALLPARAMn)` comment
};

struct CrossCallReceiver
{
  std::size_t receiver = 0; // index into facts.receivers
  std::string type;         // returned type, empty when none / conflict
  std::vector<CrossCallLocation> sources;
};

struct CrossCallReport
{
  std::shared_ptr<const CallFacts> facts;
  std::vector<CrossCallParam> params;
  std::vector<CrossCallReturnField> returnFields;
  std::vector<CrossCallArgExpectation> argExpectations;
  std::vector<CrossCallReceiver> receivers;
};

// Every return field key of `facts` with its typed format lines (from the
// finished `prep`) and whether those types disagree.
[[nodiscard]] auto crossCallReturnFieldTypes(
  const DocumentPrep& prep,
  const CallFacts& facts,
  const lsp::Uri& uri) -> std::vector<CrossCallReturnField>;

struct CrossCallAnalysis
{
  CrossCallInjection injection;
  std::shared_ptr<const CrossCallReport> report;
  // A prep of the same text already built with exactly `injection` during
  // the walk (callees needed this file's argument types); its timeline can
  // be reused instead of collecting it again.
  std::shared_ptr<const DocumentPrep> sameInjectionPrep;
};

// ---------------------------------------------------------------------------
// Resolver: walks the workspace call graph. Thread-safe; one walk at a time.

struct CrossCallEnvironment
{
  const LibraryLayout* layout = nullptr;
  const WorkspaceIndex* index = nullptr;
  TextLookup textForPath;
  IncludedParentCache* includedParents = nullptr;
  bool strictModeEnabled = false;
  std::function<void()> pollCancel;
};

class CrossCallResolver
{
public:
  // Injection and report for one file (open or not). `text` is its buffer.
  [[nodiscard]] auto analyze(
    const std::filesystem::path& path,
    const std::string& text,
    const CrossCallEnvironment& env) -> CrossCallAnalysis;

  void clear();

private:
  struct PrepEntry
  {
    std::size_t textHash = 0;
    std::string injectionSignature;
    bool strict = false;
    std::vector<std::pair<std::string, std::size_t>> dependencies;
    std::shared_ptr<const DocumentPrep> prep;
  };
  struct FactsEntry
  {
    std::size_t textHash = 0;
    std::shared_ptr<const CallFacts> facts;
  };

  friend class CrossCallWalk;

  std::mutex walkMutex_;
  std::mutex cacheMutex_;
  std::unordered_map<std::string, std::vector<PrepEntry>> preps_;
  std::unordered_map<std::string, FactsEntry> facts_;
};

} // namespace vertexadt
