#pragma once

#include "library_paths.h"

#include <lsp/types.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace vertexadt {

// How a `$DIR_*` token maps onto the library trees.
enum class DirTokenKind
{
  Fallback, // custom/<folder> if it exists, else system/<folder>, else none/<folder>
  Pinned,   // always <tree>/<folder>
  This,     // folder of the document being edited
  ActProj,  // shared/projects/<actproj>: a helper value, never a real path
};

enum class DirTree
{
  Custom,
  System,
  Shared,
};

struct DirToken
{
  std::string_view token;
  std::string_view folder;
  DirTokenKind kind = DirTokenKind::Fallback;
  DirTree tree = DirTree::Custom; // Pinned only
};

// The one token table. Covers every catalog `$DIR_*` whose detail starts with
// "returns". Matching is exact and case-sensitive.
[[nodiscard]] auto dirTokens() -> std::span<const DirToken>;
[[nodiscard]] auto findDirToken(std::string_view token) -> const DirToken*;

// A token occurrence: `$DIR_` plus the following [A-Za-z0-9_] run, byte offsets.
struct DirTokenSpan
{
  const DirToken* token = nullptr;
  std::size_t begin = 0;
  std::size_t end = 0;
};

// First table token in `text` (at or after `from`).
[[nodiscard]] auto findDirTokenSpan(std::string_view text, std::size_t from = 0)
  -> std::optional<DirTokenSpan>;

struct DirPathDisplay
{
  std::string text;                             // relative, e.g. custom/macros/x.prg
  std::optional<std::filesystem::path> target;  // set only when it exists on disk
};

// `remainder` is the text after the token, e.g. "/triggers/save_building".
// It may hold `<name>` placeholders, which never resolve to a target.
[[nodiscard]] auto displayDirPath(
  const LibraryLayout& layout,
  const std::filesystem::path& documentPath,
  const DirToken& token,
  std::string_view remainder) -> DirPathDisplay;

// Displays a traced value from its first token on. Nullopt without a token.
[[nodiscard]] auto displayDirValue(
  const LibraryLayout& layout,
  const std::filesystem::path& documentPath,
  std::string_view value) -> std::optional<DirPathDisplay>;

// The value of `name` at `line`: its last assignment at or above `line`.
// Follows `&` concatenation, `#var#` interpolation, `get($DIR_X)` (the folder
// plus a trailing `/`) and copies,
// up to kDirTraceMaxHops variable lookups. Unknown parts become `<name>` or
// `<expr>`. Nullopt when `name` has no assignment.
inline constexpr int kDirTraceMaxHops = 8;
[[nodiscard]] auto traceVariableValue(std::string_view text, int line, std::string_view name)
  -> std::optional<std::string>;

struct DirTokenHover
{
  const DirToken* token = nullptr;
  DirPathDisplay path;
};

// Hover on a `$DIR_*` token: the token plus the path written after it.
[[nodiscard]] auto dirTokenHover(
  const LibraryLayout& layout,
  const std::filesystem::path& documentPath,
  std::string_view text,
  lsp::Position position) -> std::optional<DirTokenHover>;

// Hover on a variable whose traced value holds a `$DIR_*` path. An occurrence
// right of `=` on its own assignment line shows the value before that line.
// Values show as built: trailing, duplicate and missing slashes are kept.
[[nodiscard]] auto dirVariableHover(
  const LibraryLayout& layout,
  const std::filesystem::path& documentPath,
  std::string_view text,
  lsp::Position position,
  std::string_view name) -> std::optional<DirPathDisplay>;

// Markdown for a path line: a code span, linked when the target exists.
[[nodiscard]] auto dirPathMarkdown(const DirPathDisplay& path) -> std::string;

} // namespace vertexadt
