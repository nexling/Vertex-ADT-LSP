#pragma once

// Program move (docs/adr/0017-native-program-move.md): move or rename Vertex
// ADT files and rewrite the `call(` / `# include` / `##included_from` paths
// that reach them, plus document-relative paths inside the moved files.
//
// One engine, two entries: the `vertexadt.moveProgram` command (refuses up
// front, returns a WorkspaceEdit with the rename) and `workspace/willRenameFiles`
// (cannot refuse, returns text edits only and warns).

#include "library_paths.h"

#include <lsp/json/json.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

inline constexpr std::string_view kMoveProgramCommand = "vertexadt.moveProgram";

enum class MoveOrigin
{
  Command,
  WillRename,
};

// A file or folder rename, as the client reports it.
struct MoveRename
{
  std::filesystem::path oldPath;
  std::filesystem::path newPath;
};

struct ProgramMoveInput
{
  const LibraryLayout& layout;
  std::vector<MoveRename> renames;
  MoveOrigin origin = MoveOrigin::Command;
  // Command only: the user accepted that the old path will reach System.
  bool confirmShadow = false;
  // Files whose references are scanned (the library trees), called only when
  // something will be rewritten. Moved files are added when missing.
  std::function<std::vector<std::filesystem::path>()> scanFiles;
  // Buffer-wins text.
  std::function<std::optional<std::string>(const std::filesystem::path&)> textForPath;
  // Open-buffer version, for versioned edits.
  std::function<std::optional<int>(const std::filesystem::path&)> openVersion;
  std::function<void()> pollCancel;
};

struct MoveTextEdit
{
  int line = 0;
  int startCharacter = 0; // UTF-16
  int endCharacter = 0;
  std::string newText;
};

struct MoveFileEdits
{
  std::filesystem::path file; // pre-move location
  std::optional<int> version;
  std::vector<MoveTextEdit> edits;
};

struct MoveSkippedSite
{
  std::filesystem::path file;
  int line = 0;
  std::string reason;
  std::string text;
};

struct ProgramMovePlan
{
  // Command only: nothing may happen.
  std::optional<std::string> refusal;
  // Command only: ask, then re-plan with `confirmShadow`.
  std::optional<std::string> confirmation;
  // Expanded file-level moves (folders become their ADT files).
  std::vector<MoveRename> fileMoves;
  std::vector<MoveFileEdits> edits;
  std::vector<MoveSkippedSite> skipped;
  std::vector<std::string> warnings;
  int updatedCount = 0;
};

[[nodiscard]] auto planProgramMove(const ProgramMoveInput& input) -> ProgramMovePlan;

// One `call(` / `# include` / `##included_from` path on a line: byte range of
// the path itself (no quotes, no named-section suffix).
enum class MoveRefKind
{
  Call,
  Include,
  IncludedFrom,
};

struct MoveReference
{
  int line = 0;
  std::size_t begin = 0;
  std::size_t end = 0;
  std::string path;
  MoveRefKind kind = MoveRefKind::Call;
};

[[nodiscard]] auto scanMoveReferences(std::string_view text) -> std::vector<MoveReference>;

struct ResolvedReference
{
  std::filesystem::path target;
  // Resolved against the referencing file's folder (bare path or $DIR_THIS).
  bool documentRelative = false;
};

[[nodiscard]] auto resolveMoveReference(
  const LibraryLayout& layout,
  const std::filesystem::path& documentPath,
  std::string_view path) -> std::optional<ResolvedReference>;

// The path to write in a file living in `callerDir` for `target`, in the style
// of `oldText` (see the ADR). Nullopt when no spelling reaches `target`.
[[nodiscard]] auto formatMovePath(
  const LibraryLayout& layout,
  std::string_view oldText,
  bool oldDocumentRelative,
  const std::filesystem::path& callerDir,
  const std::filesystem::path& target) -> std::optional<std::string>;

// `$DIR_*` spelling of a file, for the command's input box.
[[nodiscard]] auto defaultMoveTargetText(const LibraryLayout& layout, const std::filesystem::path& path)
  -> std::optional<std::string>;

// The Custom-tree file a typed target names (`$DIR_MACROS/x.prg`,
// `$DIR_CUSTOM/...`, `macros/x.prg`; a trailing `/` keeps the source name).
// Error text on failure.
struct MoveTargetResolution
{
  std::optional<std::filesystem::path> path;
  std::string error;
};

[[nodiscard]] auto resolveMoveTargetText(
  const LibraryLayout& layout,
  const std::filesystem::path& source,
  std::string_view text) -> MoveTargetResolution;

// WorkspaceEdit JSON. With `withRename`: documentChanges (text edits on the
// old URIs, needs-confirmation annotated, then the RenameFile ops); without:
// plain `changes` (willRenameFiles).
[[nodiscard]] auto programMoveEditJson(const ProgramMovePlan& plan, bool withRename) -> lsp::json::Value;

// `{ updatedCount, skipped: [{ uri, line, reason, text }], warnings: [] }`.
[[nodiscard]] auto programMoveReportJson(const ProgramMovePlan& plan) -> lsp::json::Value;

} // namespace vertexadt
