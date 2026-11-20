#pragma once

#include "validate.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vertexadt {

// One resolved parenthesized `call(` site whose argument count differs from
// the callee's expected call-param count. Paren indexes are bytes on the code
// line (after stripValidationComment and inline-bang truncation).
struct CallParamSite
{
  int openParenIndex = 0;
  int closeParenIndex = 0;
  int expected = 0;
  int argCount = 0;
  std::string calleeDisplayPath;
};

// One zero-width insert or removal on a line. Characters are UTF-16 units.
struct CallParamEdit
{
  int line = 0;
  int startCharacter = 0;
  int endCharacter = 0;
  std::string newText;
};

// Node `analyzeCallParamSitesOnLine`, mismatches only.
[[nodiscard]] auto callParamSitesOnLine(const ValidateStores& stores, std::string_view codeLine)
  -> std::vector<CallParamSite>;

// Node `buildMissingCallParamInsertText`: `,""` repeated `missingCount` times.
[[nodiscard]] auto missingCallParamInsertText(int missingCount) -> std::string;

// Node `findCallParamRangesInParen` + `buildExtraCallParamRemovalRange` as a
// UTF-16 [start, end) range on the code line: from the comma before the first
// extra argument through, not including, `)`. nullopt when there is no extra.
[[nodiscard]] auto extraCallParamRemovalRange(std::string_view codeLine, const CallParamSite& site)
  -> std::optional<std::pair<int, int>>;

// Node `buildCallParamWorkspaceEditsForContent`: `\n` split, comment strip and
// inline-bang truncation only (no whole-line bang / `#EDIT_FORM#` skips),
// missing inserts and extra removals, reverse-sorted by line then start.
[[nodiscard]] auto callParamWorkspaceEdits(
  const ValidateStores& stores,
  std::string_view content,
  const std::function<void()>& pollCancel) -> std::vector<CallParamEdit>;

} // namespace vertexadt
