#pragma once

#include "library_paths.h"
#include "workspace_index.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vertexadt {

// Workspace-index entries keyed by overlay identity (lowercase
// library-relative path).
using IndexEntryMap = std::unordered_map<std::string, std::shared_ptr<const IndexEntry>>;

// Commons-gated call: a `call` link joins a namespace walk only when its
// source entry has `common_indexes` or `common_points`. `# include` links
// always join.
[[nodiscard]] bool commonsGated(const IndexEntry& entry);

// server.js getDirectIncomingNamespaceParents: every entry with an include
// link, or a commons-gated call link, to `target`.
[[nodiscard]] auto incomingNamespaceParents(const IndexEntryMap& byKey, std::string_view target)
  -> std::unordered_set<std::string>;

// server.js getUltimateNamespaceRoots: walk incoming namespace parents up to
// every parent-less node, in discovery order. A cycle with no parent-less node
// yields no root.
[[nodiscard]] auto ultimateNamespaceRoots(const IndexEntryMap& byKey, std::string_view filePath)
  -> std::vector<std::string>;

struct NamespaceClosure
{
  std::string root;
  std::vector<std::string> files;
  std::unordered_set<std::string> fileSet;
};

// server.js getForwardNamespaceClosure: `root` plus everything reachable over
// include links and commons-gated call links. A target with no index entry
// stays in the closure with no outgoing walk.
[[nodiscard]] auto forwardNamespaceClosure(const IndexEntryMap& byKey, std::string_view root)
  -> NamespaceClosure;

struct ProgramPath
{
  std::string displayPath;
  std::string uri;
};

// server.js resolveProgramDisplayPath + fsPathToUri for an overlay identity:
// the winning index entry's display path and URI, else the Custom-then-System
// file under the workspace root (display path relative to that root).
[[nodiscard]] auto programPathForKey(
  const LibraryLayout& layout,
  const IndexEntryMap& byKey,
  const std::string& key) -> ProgramPath;

} // namespace vertexadt
