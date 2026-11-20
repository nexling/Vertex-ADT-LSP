#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

struct NamedSection
{
  std::string name;
  int openLine = 0;
  int closeLine = 0;
};

[[nodiscard]] auto splitLines(std::string_view text) -> std::vector<std::string_view>;
[[nodiscard]] auto trim(std::string_view text) -> std::string_view;
[[nodiscard]] auto toLowerAscii(std::string_view text) -> std::string;
[[nodiscard]] auto replaceBackslashes(std::string_view text) -> std::string;
[[nodiscard]] auto normalizeFsPath(std::string_view path) -> std::string;

[[nodiscard]] bool isInsideQuotes(std::string_view line, std::size_t position);
[[nodiscard]] auto stripValidationComment(std::string_view line) -> std::string;
// Full-document validate's shared per-line skip, the part decided on the raw
// line before comment stripping (bang comments, `##`, `#SECTION#`, debug,
// messagebox, blank). Also used by control-block pairing.
[[nodiscard]] bool isSharedPerLineSkip(std::string_view raw);
// Inline if-then: `/if\s+.*\s+then\s+.*/i` on a trimmed, comment-stripped
// line. Such a line does not open an if block.
[[nodiscard]] bool isInlineIfThen(std::string_view trimmed);
[[nodiscard]] auto findInlineBangCommentIndex(std::string_view line) -> std::ptrdiff_t;
// Node unclosedResources `findBangCommentIndex`: first unquoted consecutive
// `;!` (byte index), or -1.
[[nodiscard]] auto findBangCommentIndex(std::string_view line) -> std::ptrdiff_t;
// Node `lineHasIgnoreClose`: `/\bIGNORE\s+CLOSE\b/i` on the bang-comment
// suffix, with JS `\s` / `\w` semantics. Shared by the unclosed-resource check
// and the IGNORE CLOSE code action.
[[nodiscard]] bool lineHasIgnoreClose(std::string_view line);
[[nodiscard]] bool isBangComment(std::string_view text);
[[nodiscard]] bool isDebugDump(std::string_view text);
[[nodiscard]] bool isVxdbaseWinstyleShell(std::string_view text);
[[nodiscard]] bool isValidationExcluded(std::string_view text);

[[nodiscard]] auto parseFileSections(const std::vector<std::string_view>& lines)
  -> std::vector<NamedSection>;
[[nodiscard]] auto sectionNameForLine(int line, const std::vector<NamedSection>& sections)
  -> std::string;
[[nodiscard]] bool isVerificationIgnoredSection(std::string_view sectionName);
[[nodiscard]] bool lineInVerificationIgnoredSection(
  int line,
  const std::vector<NamedSection>& sections);

[[nodiscard]] auto captureHoverComment(std::string_view line) -> std::optional<std::string>;
[[nodiscard]] auto expectedCallParamCountFromText(
  std::string_view text,
  std::optional<std::string_view> targetSection) -> int;

[[nodiscard]] bool isLocalSlotName(std::string_view name);
[[nodiscard]] auto stripPathQuotes(std::string_view value) -> std::string;

// A `# include` / `call` path split into file path and target section.
// Empty targetSection means the whole file.
struct IncludeCallTarget
{
  std::string filePath;
  std::string targetSection;
};

// Node targetSectionSupport `parseIncludeCallPathAndSection`: the named-section
// suffix is a trailing space-separated ident inside the (unquoted) path, or the
// first ident of `lineRemainder` (which wins). `SECTION` / `SECTIONOFFILE` are
// not suffixes and are cut from the path. Used by document links; the
// validators still keep their own copies.
[[nodiscard]] auto splitIncludeCallTarget(std::string_view rawPath, std::string_view lineRemainder)
  -> IncludeCallTarget;

} // namespace vertexadt
