#pragma once

#include "library_paths.h"

#include <lsp/uri.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vertexadt {

struct HoverTuple
{
  std::string name;
  std::optional<std::string> comment;
  std::string objectType;
};

// Where a name is assigned: 0-based line and byte offset of the name in the
// raw line. Only `X =` / `X$ =` at the start of a statement (line start,
// after `;`, or after `then` / `else`), `set X`, and synthetic names count. `begin` is npos for synthetic names (`find_text_param` and
// friends), which have no position of their own on the line.
struct AssignmentSite
{
  int line = 0;
  std::size_t begin = std::string::npos;
};

// Names a parent context makes visible to one part of a child file.
struct ScopedNames
{
  std::unordered_set<std::string> variables;
  std::unordered_set<std::string> stringVariables;
};

struct VariableExtract
{
  std::vector<std::string> variables;
  std::vector<std::string> stringVariables;
  std::unordered_set<std::string> variablesSet;
  std::unordered_set<std::string> stringVariablesSet;
  std::vector<HoverTuple> hoverVariables;
  std::vector<HoverTuple> hoverStringVariables;
  // Every parent path read while filling (written and inferred, transitive);
  // edits to any of them dirty this neighborhood.
  std::vector<std::string> includedFromPaths;
  // Include-neighborhood only (docs/adr/0015-native-inferred-included-from.md):
  // the names visible on lines outside every section, and per child section
  // name. `variablesSet` / `stringVariablesSet` above are the union of all
  // scopes (hover, completion); scope-aware checks use `scopeFor`.
  ScopedNames outsideSections;
  std::unordered_map<std::string, ScopedNames> bySection;
  // Live-buffer only: every assignment site per name, in line order. Keys
  // match variablesSet / stringVariablesSet (string names without `$`).
  std::unordered_map<std::string, std::vector<AssignmentSite>> assignmentSites;
  std::unordered_map<std::string, std::vector<AssignmentSite>> stringAssignmentSites;
};

// The neighborhood names visible on a line in `sectionName` ("" outside every
// section).
[[nodiscard]] auto scopeFor(const VariableExtract& neighborhood, const std::string& sectionName)
  -> const ScopedNames&;

using TextLookup = std::function<std::optional<std::string>(const std::filesystem::path&)>;

[[nodiscard]] auto fillLiveBufferExtract(std::string_view text) -> VariableExtract;

class WorkspaceIndex;

// The include neighborhood of `rootText` (at `rootPath`): written
// `##included_from` parents plus, when `inferParents` and `index` are set,
// parents inferred from `# include` / `call` links (parent_context.h).
[[nodiscard]] auto fillIncludeNeighborhoodExtract(
  std::string_view rootText,
  const std::filesystem::path& rootPath,
  const LibraryLayout& layout,
  const WorkspaceIndex* index,
  bool inferParents,
  const TextLookup& textForPath) -> VariableExtract;

// One atomic read of a document's include-neighborhood slot. `generation`
// changes whenever the slot is marked dirty or refilled; 0 when absent.
struct NeighborhoodState
{
  bool dirty = true;
  std::shared_ptr<const VariableExtract> extract;
  std::uint64_t generation = 0;
};

class ExtractStore
{
public:
  void putLive(const lsp::Uri& uri, int version, VariableExtract extract);
  void eraseLive(const lsp::Uri& uri);
  [[nodiscard]] auto live(const lsp::Uri& uri, int version) const
    -> std::shared_ptr<const VariableExtract>;

  // Returns the slot's new generation.
  auto putNeighborhood(const lsp::Uri& uri, VariableExtract extract) -> std::uint64_t;
  void markNeighborhoodDirty(const lsp::Uri& uri);
  [[nodiscard]] bool neighborhoodDirty(const lsp::Uri& uri) const;
  [[nodiscard]] auto neighborhoodState(const lsp::Uri& uri) const -> NeighborhoodState;
  [[nodiscard]] auto neighborhoodGeneration(const lsp::Uri& uri) const -> std::uint64_t;
  [[nodiscard]] auto neighborhood(const lsp::Uri& uri) const
    -> std::shared_ptr<const VariableExtract>;
  [[nodiscard]] auto ownersOfIncludedPath(std::string_view normalizedPath) const
    -> std::vector<lsp::Uri>;
  void eraseNeighborhood(const lsp::Uri& uri);
  void clearNeighborhoods();

private:
  struct LiveSlot
  {
    int version = 0;
    std::shared_ptr<const VariableExtract> extract;
  };

  struct NeighborhoodSlot
  {
    bool dirty = true;
    std::shared_ptr<const VariableExtract> extract;
    std::vector<std::string> includedPaths;
    std::uint64_t generation = 0;
  };

  std::uint64_t nextGeneration_ = 0;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, LiveSlot> live_;
  std::unordered_map<std::string, NeighborhoodSlot> neighborhood_;
  std::unordered_map<std::string, std::unordered_set<std::string>> reverseIncludes_;
};

} // namespace vertexadt
