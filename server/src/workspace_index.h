#pragma once

#include "library_paths.h"

#include <lsp/uri.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vertexadt {

struct IndexEntry;

struct Declaration
{
  std::string varName;
  int number = 0;
  int line = 0;
  std::string lineText;
  std::string sectionName;
  std::optional<std::string> variableType;
};

struct FileLink
{
  std::string target;
  std::string targetSection;
  std::string sourceSection;
  int line = 0;
  std::string lineText;
};

// One `# include` / `call` of a file, seen from the target's side.
struct IncomingLink
{
  std::shared_ptr<const IndexEntry> parent;
  FileLink link;
  bool isCall = false;
};

struct CalleeCommonRequirement
{
  std::string directive;
  std::string sectionName;
};

struct IndexEntry
{
  OverlayIdentity identity;
  std::vector<Declaration> declarations;
  std::vector<std::string> localSlotsManifest;
  std::vector<FileLink> includeLinks;
  std::vector<FileLink> callLinks;
  bool commonIndexes = false;
  bool commonPoints = false;
  std::vector<CalleeCommonRequirement> calleeCommonRequirements;
  int expectedCallParamCount = 0;
  bool isCallCallee = false;
};

[[nodiscard]] auto parseIndexEntry(
  std::string_view text,
  OverlayIdentity identity,
  const LibraryLayout& layout) -> IndexEntry;

class WorkspaceIndex
{
public:
  // Both return the link targets (library-relative keys) whose incoming
  // `# include` / `call` links from this entry changed: what inferred
  // `##included_from` parents depend on.
  auto upsert(IndexEntry entry) -> std::vector<std::string>;
  auto removeKey(std::string_view key) -> std::vector<std::string>;
  void clear();
  void refreshCalleeFlags();

  [[nodiscard]] auto entry(std::string_view key) const -> std::shared_ptr<const IndexEntry>;
  [[nodiscard]] auto entryByAbsPath(const std::filesystem::path& absPath) const
    -> std::shared_ptr<const IndexEntry>;
  [[nodiscard]] auto snapshot() const -> std::vector<std::shared_ptr<const IndexEntry>>;
  // Every indexed `# include` / `call` link whose target is `targetKey`
  // (library-relative, lowercased), in parent-key then line order. The
  // reverse map is rebuilt lazily after any change.
  [[nodiscard]] auto incomingLinks(std::string_view targetKey) const -> std::vector<IncomingLink>;

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<const IndexEntry>> entries_;
  mutable bool reverseValid_ = false;
  mutable std::unordered_map<std::string, std::vector<IncomingLink>> reverse_;
};

} // namespace vertexadt
