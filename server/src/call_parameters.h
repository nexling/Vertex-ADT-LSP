#pragma once

#include "library_paths.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// The callee's `name = get($IDENT)` rows (server.js extractFunctionParameters
// over targetSectionSupport.js extractCallParametersFromText). Shared by
// remaining-param completion and `call(` signature help.

struct CallParameter
{
  // Left-hand text including a trailing `$` (`obj`, `Name$`).
  std::string variableName;
  // `IDENT` from `get($IDENT)`. Stored, never shown.
  std::string parameterName;
  // `split('"')[1]` after the line's first `;!` outside quotes, when non-empty.
  std::optional<std::string> comment;
};

// Every `name = get($IDENT)` in source order (not CALLPARAM-filtered). A
// non-empty target section keeps only lines whose section name equals it.
// Bang lines and `#EDIT_FORM#` are not skipped.
[[nodiscard]] auto extractFunctionParameters(
  std::string_view text,
  std::optional<std::string_view> targetSection) -> std::vector<CallParameter>;

struct CallTarget
{
  std::string filePath;
  std::optional<std::string> targetSection;
};

// Host first-argument parse: trim, drop one leading and one trailing quote,
// then parseIncludeCallPathAndSection(…, ''). Only the first argument's suffix
// names a target section; SECTION / SECTIONOFFILE do not.
[[nodiscard]] auto callTargetFromFirstArgument(std::string_view firstArgument) -> CallTarget;

// Same parse without the leading trim: the call return-value host strips quotes
// from the raw regex group, so `"callee.prg" ` keeps a dirty `callee.prg"`.
[[nodiscard]] auto callTargetFromUntrimmedArgument(std::string_view argument) -> CallTarget;

// resolveVertexFilePath, then the buffer-wins linked-file snapshot, then the
// extract. Unresolved path or missing/empty snapshot gives no rows (no stored
// whole-file count fallback).
[[nodiscard]] auto calleeParameters(
  const LibraryLayout& layout,
  const std::function<std::optional<std::string>(const std::filesystem::path&)>& textForPath,
  const CallTarget& target) -> std::vector<CallParameter>;

} // namespace vertexadt
