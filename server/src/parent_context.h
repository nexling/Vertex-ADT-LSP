#pragma once

// Inferred and written `##included_from` parents
// (docs/adr/0015-native-inferred-included-from.md).
//
// A child file's parents come from its written `##included_from` lines and,
// where no written line covers a scope, from the workspace index's incoming
// `# include` links (and `call` links from callers that set `common_indexes 1`
// or `common_points 1`). Each parent contributes the names it declares before
// its earliest include point, in that same section, plus its own parent
// context there. Written parents are unioned; inferred parents are
// intersected per child section.

#include "extracts.h"
#include "library_paths.h"
#include "workspace_index.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Above this many distinct inferred parents nothing is inferred.
inline constexpr int kMaxInferredParents = 10;

struct WrittenDirective
{
  int line = 0;
  std::string rawPath;
  // Section the directive is written in; "" outside every section.
  std::string section;
  std::optional<std::filesystem::path> resolved;
  // Earliest line in the parent that includes or calls the child (matching
  // section first); nullopt: no include point, the whole parent counts.
  std::optional<int> cutoffLine;
};

struct InferredParent
{
  std::filesystem::path absPath;
  std::string displayPath;
  // Earliest include of the whole child (no section).
  std::optional<int> wholeLine;
  // Earliest include per child section name, lowercased.
  std::vector<std::pair<std::string, int>> sectionLines;
};

struct ParentLinks
{
  std::vector<WrittenDirective> written;
  // A resolved written directive outside every section: inference is off.
  bool writtenCoversFile = false;
  // Section names (lowercased) a resolved written directive is written in.
  std::vector<std::string> writtenSections;
  // Empty when inference is off, suppressed, or over the cap.
  std::vector<InferredParent> inferred;
  // Every inferred parent candidate, also over the cap (CodeLens, pinning).
  std::vector<InferredParent> candidates;
  // Distinct inferred parent candidates before the cap.
  int inferredCandidates = 0;
  bool overCap = false;
};

// Trimmed lines starting with `##included_from`, resolved with
// `findIncludedFile`. Cut-off lines are left unset.
[[nodiscard]] auto parseWrittenDirectives(std::string_view text, const LibraryLayout& layout)
  -> std::vector<WrittenDirective>;

// `text` with every line a parent contributes at `cutoff` kept and the rest
// blanked (line count preserved): lines above the cut-off whose innermost
// section is none or the cut-off line's section. nullopt keeps everything.
[[nodiscard]] auto linesBeforeIncludePoint(std::string_view text, std::optional<int> cutoff) -> std::string;

[[nodiscard]] auto resolveParentLinks(
  std::string_view text,
  const std::filesystem::path& path,
  const LibraryLayout& layout,
  const WorkspaceIndex* index,
  bool inferParents) -> ParentLinks;

} // namespace vertexadt
