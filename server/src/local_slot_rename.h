#pragma once

#include "workspace_index.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// One local-slot rename: every file-wide word reference of `oldName` becomes
// the case-preserving `newName`.
struct LocalSlotRenamePair
{
  std::string oldName;
  std::string newName;
};

// One text edit on a single line. Characters are UTF-16 code units.
struct LocalSlotRenameEdit
{
  int line = 0;
  int startCharacter = 0;
  int endCharacter = 0;
  std::string newText;
};

// Node `buildFileWideVariableRenameEdits`. Skips whole-line bang comments,
// `;!` tails, quoted text, and `foo$` when renaming `foo`; does not skip
// `#EDIT_FORM#` bodies. Longer old names go first, one edit per range,
// returned reverse-sorted by line then start character. Shared by the
// callee-local-naming QuickFix and `vertexadt.assignLocalSlotNames`.
[[nodiscard]] auto fileWideLocalSlotRenameEdits(
  const std::vector<std::string_view>& lines,
  const std::vector<LocalSlotRenamePair>& pairs) -> std::vector<LocalSlotRenameEdit>;

// Node `getLocalSlotPrefix` (`/^l(\d+)/`, case-sensitive; '' when absent)
// and `getLocalSlotBaseName`, shared with family 16.
[[nodiscard]] auto localSlotNamePrefix(std::string_view name) -> std::string;
[[nodiscard]] auto localSlotNameBase(std::string_view name) -> std::string;

// Node `getFileLocalSlotPrefix` over an index snapshot: the file's own
// canonical prefix, else `l{maxUsed+1}` from the other call-callee entries.
// Implemented beside family 16 so both share one formula.
[[nodiscard]] auto fileLocalSlotPrefixForPath(
  const std::vector<std::shared_ptr<const IndexEntry>>& entries,
  const std::filesystem::path& absPath) -> std::string;

// Node `buildLocalSlotRenamePairs`: non-`lN` declaration-number names (no
// verification-ignored skip), then wrong-prefix local-slot names (with the
// skip); value-typed rows skipped; first win per old name.
[[nodiscard]] auto localSlotRenamePairs(std::string_view content, std::string_view filePrefix)
  -> std::vector<LocalSlotRenamePair>;

} // namespace vertexadt
