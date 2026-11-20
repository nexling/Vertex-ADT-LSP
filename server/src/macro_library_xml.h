#pragma once

#include "library_paths.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace vertexadt {

// Custom macro libraries: `complibs/{library}` folders holding Vertex `d_*`
// databases and their `{database}.xml` sidecars written by the Vertex Dbase
// Converter helper. Shared by object-parameter completion and macro
// library/code completion. Runs on a heavy worker, never the message loop.

// server.js resolveMacroLibraryPath: the custom name is the last `/` segment of
// the last workspace folder URI containing `custom` (case-sensitive, else
// `custom`); the first folder whose URI contains it, joined with
// `complibs/{library}`. Nullopt for an empty library or no such folder. The
// folder may not exist.
[[nodiscard]] auto resolveMacroLibraryPath(std::string_view library, const LibraryLayout& layout)
  -> std::optional<std::filesystem::path>;

// server.js getMacroLibraries' Custom tree: the same custom name as
// resolveMacroLibraryPath, then the first folder whose URI contains it and
// that has a `complibs` directory. Nullopt when none.
[[nodiscard]] auto customComplibsDirectory(const LibraryLayout& layout)
  -> std::optional<std::filesystem::path>;

// server.js listMacroLibraryXmlFiles: regular files named `d_*` ending `.xml`
// in any case. Directory order; unreadable → empty.
[[nodiscard]] auto macroLibraryXmlFiles(const std::filesystem::path& libraryPath)
  -> std::vector<std::string>;

// server.js isPrimaryMacroDatabaseFile: a name ending `c`/`l` (any case) is a
// companion when the name without that letter is also a database file.
[[nodiscard]] bool isPrimaryMacroDatabaseFile(
  std::string_view fileName,
  const std::unordered_set<std::string>& allDatabaseFiles);

// Primary databases in `libraryPath`: extensionless regular files whose name
// starts with `d_`, minus companions. Directory order.
[[nodiscard]] auto primaryMacroDatabaseFiles(const std::filesystem::path& libraryPath)
  -> std::vector<std::filesystem::path>;

// Stale: no `{database}.xml`, or the database is newer than it.
[[nodiscard]] bool macroDatabaseXmlIsStale(const std::filesystem::path& databasePath);

enum class MacroLibraryExportResult
{
  Done,     // every stale primary was attempted (failures are logged, not fatal)
  Canceled, // stopped at the first canceled export; later databases untouched
};

struct MacroLibraryExportRequest
{
  std::filesystem::path libraryPath;
  std::string_view extensionUri;
  std::function<bool()> isCanceled;
  // Defaults to stderr.
  std::function<void(std::string_view)> log;
};

// server.js ensureMacroLibraryXmlExports: export each stale primary database in
// order through the helper. A missing helper is logged once and stops the
// loop; a failed export is logged and the loop goes on. A missing folder does
// nothing.
[[nodiscard]] auto ensureMacroLibraryXmlExports(const MacroLibraryExportRequest& request)
  -> MacroLibraryExportResult;

struct MacroLibraryCell
{
  std::string name;
  // textContent, JS-trimmed.
  std::string value;
  // textContent as stored (macro library/code completion keeps spaces).
  std::string raw;
};

using MacroLibraryRecord = std::vector<MacroLibraryCell>;

// Every `Record` element in document order, each with its named `Cell`
// descendants (cells without a `Name` are dropped).
[[nodiscard]] auto parseMacroLibraryXml(std::string_view xml) -> std::vector<MacroLibraryRecord>;

// Reads `d_{library}.xml`, else `d_FRM_SHEET.xml`, from `libraryPath` and
// parses it. Unreadable files are empty.
[[nodiscard]] auto readMacroLibraryRecords(
  const std::filesystem::path& libraryPath,
  std::string_view library) -> std::vector<MacroLibraryRecord>;

} // namespace vertexadt
