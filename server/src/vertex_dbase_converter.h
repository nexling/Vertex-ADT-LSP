#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>

namespace vertexadt {

// Vertex Dbase Converter helper: the spawned `Vertex_Dbase_Converter.exe`
// that exports a Vertex `d_*` database to sidecar XML.
//
// This module only spawns the helper. When a `d_*` is stale and which files
// are primary belong to the consuming completion families.

inline constexpr std::wstring_view kVertexDbaseConverterFileName = L"Vertex_Dbase_Converter.exe";

enum class HelperExportResult
{
  Exported,      // child exited 0
  HelperMissing, // no extensionUri, or no helper file at the extension root
  Failed,        // spawn failure or non-zero exit (already logged)
  Canceled,      // canceled while waiting for the slot or while the child ran
};

struct HelperExportRequest
{
  std::string_view extensionUri;
  std::filesystem::path databasePath;
  std::filesystem::path xmlPath;
  // Polled while waiting for the one helper slot and while the child runs.
  // A cancel kills the child and deletes `xmlPath` (it may be half written).
  std::function<bool()> isCanceled;
  // Defaults to stderr.
  std::function<void(std::string_view)> log;
};

// `initializationOptions.extensionUri` joined with the helper file name.
// Empty or invalid URI is nullopt (a missing helper).
[[nodiscard]] auto vertexDbaseConverterPath(std::string_view extensionUri)
  -> std::optional<std::filesystem::path>;

// Runs on a heavy worker, never the message loop. One helper child at a time
// process-wide; other callers wait for the slot. Hidden window, cwd = the
// helper's directory, argv `[databasePath, --all, --xml, --export, xmlPath]`,
// no timeout. The child lives in a kill-on-close Job object so it dies with
// the language server.
[[nodiscard]] auto exportDatabaseXml(const HelperExportRequest& request) -> HelperExportResult;

} // namespace vertexadt
