#include "workspace_exports.h"

#include "library_paths.h"
#include "namespace_graph.h"
#include "text_util.h"
#include "utf16_text.h"

#include <lsp/error.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

using lsp::json::Object;
using lsp::json::Value;

constexpr int kExportMaxProblems = 10000;
constexpr std::size_t kFilesPerBatch = 10;

auto noRootResult() -> Value
{
  Object result;
  result["error"] = Value(std::string("No workspace root found"));
  return Value(std::move(result));
}

auto utf8Path(const std::filesystem::path& path) -> std::string
{
  const auto u8 = path.u8string();
  return {reinterpret_cast<const char*>(u8.data()), u8.size()};
}

// Node path.normalize(path.join(root, name)).
auto outputPath(const std::filesystem::path& root, std::string_view name) -> std::filesystem::path
{
  auto path = (root / std::filesystem::path(name)).lexically_normal();
  path.make_preferred();
  return path;
}

// server.js escapeCsvField: quote iff the cell holds `"`, `,`, `\r` or `\n`.
auto csvField(std::string_view value) -> std::string
{
  if (value.find_first_of("\",\r\n") == std::string_view::npos)
  {
    return std::string(value);
  }
  std::string quoted = "\"";
  for (const auto ch : value)
  {
    if (ch == '"')
    {
      quoted += '"';
    }
    quoted += ch;
  }
  quoted += '"';
  return quoted;
}

auto csvRow(const std::vector<std::string>& values) -> std::string
{
  std::string row;
  for (std::size_t i = 0; i < values.size(); ++i)
  {
    if (i > 0)
    {
      row += ',';
    }
    row += csvField(values[i]);
  }
  row += '\n';
  return row;
}

void writeFile(const std::filesystem::path& path, const std::string& content, bool append)
{
  std::ofstream out(path, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
  if (!out)
  {
    throw lsp::RequestError(
      lsp::MessageError::InternalError,
      "Could not write " + utf8Path(path));
  }
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!out)
  {
    throw lsp::RequestError(
      lsp::MessageError::InternalError,
      "Could not write " + utf8Path(path));
  }
}

auto readDisk(const std::filesystem::path& path) -> std::optional<std::string>
{
  std::ifstream in(path, std::ios::binary);
  if (!in)
  {
    return std::nullopt;
  }
  std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  if (in.bad())
  {
    return std::nullopt;
  }
  return text;
}

auto indexByKey(const std::vector<std::shared_ptr<const IndexEntry>>& entries) -> IndexEntryMap
{
  IndexEntryMap byKey;
  for (const auto& entry : entries)
  {
    if (entry)
    {
      byKey.emplace(entry->identity.key, entry);
    }
  }
  return byKey;
}

// server.js resolveProgramDisplayPath for a filesystem path: the index entry
// stored at exactly that path, else relative to the workspace root, else the
// base name.
class AbsPathDisplay
{
public:
  AbsPathDisplay(const std::vector<std::shared_ptr<const IndexEntry>>& entries,
    std::filesystem::path root)
    : root_(std::move(root))
  {
    for (const auto& entry : entries)
    {
      if (entry && !entry->identity.absPath.empty())
      {
        byPath_.emplace(normalizeFsPath(entry->identity.absPath.string()),
          entry->identity.displayPath);
      }
    }
  }

  [[nodiscard]] auto operator()(const std::filesystem::path& path) const -> std::string
  {
    if (const auto it = byPath_.find(normalizeFsPath(path.string())); it != byPath_.end())
    {
      return it->second;
    }
    if (!root_.empty())
    {
      const auto relative = path.lexically_normal().lexically_relative(root_.lexically_normal());
      const auto text = relative.generic_u8string();
      if (!relative.empty() && !relative.is_absolute() && !text.starts_with(u8".."))
      {
        return {reinterpret_cast<const char*>(text.data()), text.size()};
      }
    }
    return utf8Path(path.filename());
  }

private:
  std::filesystem::path root_;
  std::unordered_map<std::string, std::string> byPath_;
};

auto severityLabel(const lsp::Diagnostic& diagnostic) -> std::string_view
{
  if (!diagnostic.severity)
  {
    return "unknown";
  }
  switch (diagnostic.severity->value())
  {
  case 1:
    return "error";
  case 2:
    return "warning";
  case 3:
    return "information";
  case 4:
    return "hint";
  default:
    return "unknown";
  }
}

// Node `diagnostic.code || ''`.
auto codeText(const lsp::Diagnostic& diagnostic) -> std::string
{
  if (!diagnostic.code)
  {
    return {};
  }
  if (const auto* text = std::get_if<std::string>(&*diagnostic.code))
  {
    return *text;
  }
  if (const auto* number = std::get_if<std::int32_t>(&*diagnostic.code))
  {
    return *number == 0 ? std::string() : std::to_string(*number);
  }
  return {};
}

auto messageText(const lsp::Diagnostic& diagnostic) -> std::string
{
  if (const auto* text = std::get_if<std::string>(&diagnostic.message))
  {
    return *text;
  }
  if (const auto* markup = std::get_if<lsp::MarkupContent>(&diagnostic.message))
  {
    return markup->value;
  }
  return {};
}

} // namespace

auto exportWorkspaceReports(
  WorkspaceState& workspace,
  const std::function<void()>& pollCancel) -> Value
{
  workspace.rebuildIndex(pollCancel);
  const auto& layout = workspace.layout();
  const auto root = layout.workspaceRoot();
  if (layout.folders().empty() || root.empty())
  {
    return noRootResult();
  }

  const auto entries = workspace.index().snapshot();
  const auto byKey = indexByKey(entries);

  // Program usage: every stored link counts (no commons gate).
  struct Usage
  {
    std::string displayPath;
    int callCount = 0;
    int includeCount = 0;
  };
  std::unordered_map<std::string, Usage> usage;
  const auto usageRow = [&](const std::string& target) -> Usage& {
    auto [it, inserted] = usage.try_emplace(target);
    if (inserted)
    {
      it->second.displayPath = programPathForKey(layout, byKey, target).displayPath;
    }
    return it->second;
  };
  // Declaration numbers keyed by (name, number).
  std::map<std::pair<std::string, int>, int> declarationCounts;
  struct LocalSlotRow
  {
    std::string displayPath;
    std::string varName;
    int number = 0;
  };
  std::vector<LocalSlotRow> localSlots;

  for (const auto& entry : entries)
  {
    if (!entry)
    {
      continue;
    }
    for (const auto& link : entry->callLinks)
    {
      ++usageRow(link.target).callCount;
    }
    for (const auto& link : entry->includeLinks)
    {
      ++usageRow(link.target).includeCount;
    }
    for (const auto& decl : entry->declarations)
    {
      if (decl.number > 0)
      {
        ++declarationCounts[{decl.varName, decl.number}];
      }
      if (entry->isCallCallee && isLocalSlotName(decl.varName))
      {
        localSlots.push_back({entry->identity.displayPath, decl.varName, decl.number});
      }
    }
  }
  if (pollCancel)
  {
    pollCancel();
  }

  std::vector<const Usage*> usageRows;
  for (const auto& [_, row] : usage)
  {
    if (row.callCount > 0 || row.includeCount > 0)
    {
      usageRows.push_back(&row);
    }
  }
  std::stable_sort(usageRows.begin(), usageRows.end(), [](const Usage* a, const Usage* b) {
    return jsLocaleCompare(a->displayPath, b->displayPath) < 0;
  });

  std::vector<std::pair<std::pair<std::string, int>, int>> declarationRows(
    declarationCounts.begin(), declarationCounts.end());
  std::stable_sort(declarationRows.begin(), declarationRows.end(),
    [](const auto& a, const auto& b) {
      const auto cmp = jsLocaleCompare(a.first.first, b.first.first);
      if (cmp != 0)
      {
        return cmp < 0;
      }
      return a.first.second < b.first.second;
    });

  std::stable_sort(localSlots.begin(), localSlots.end(),
    [](const LocalSlotRow& a, const LocalSlotRow& b) {
      const auto pathCmp = jsLocaleCompare(a.displayPath, b.displayPath);
      if (pathCmp != 0)
      {
        return pathCmp < 0;
      }
      const auto slotCmp = jsLocaleCompare(a.varName, b.varName, true);
      if (slotCmp != 0)
      {
        return slotCmp < 0;
      }
      return a.number < b.number;
    });

  std::string programUsage = csvRow({"program", "call_count", "include_count"});
  for (const auto* row : usageRows)
  {
    programUsage += csvRow(
      {row->displayPath, std::to_string(row->callCount), std::to_string(row->includeCount)});
  }
  std::string declarationNumbers = csvRow({"variable_name", "declaration_number", "count"});
  for (const auto& [key, count] : declarationRows)
  {
    declarationNumbers +=
      csvRow({key.first, std::to_string(key.second), std::to_string(count)});
  }
  std::string localSlotLines = csvRow({"program", "local_slot_name", "declaration_number"});
  for (const auto& row : localSlots)
  {
    localSlotLines += csvRow({row.displayPath, row.varName, std::to_string(row.number)});
  }

  const auto programUsagePath = outputPath(root, "vertexadt-program-usage.csv");
  const auto declarationNumbersPath = outputPath(root, "vertexadt-declaration-numbers.csv");
  const auto localSlotsPath = outputPath(root, "vertexadt-local-slots.csv");
  writeFile(programUsagePath, programUsage, false);
  writeFile(declarationNumbersPath, declarationNumbers, false);
  writeFile(localSlotsPath, localSlotLines, false);

  Object result;
  result["programUsagePath"] = Value(utf8Path(programUsagePath));
  result["declarationNumbersPath"] = Value(utf8Path(declarationNumbersPath));
  result["localSlotsPath"] = Value(utf8Path(localSlotsPath));
  return Value(std::move(result));
}

auto exportWorkspaceDiagnostics(
  WorkspaceState& workspace,
  const std::function<void()>& pollCancel) -> Value
{
  workspace.rebuildIndex(pollCancel);
  const auto& layout = workspace.layout();
  const auto root = layout.workspaceRoot();
  if (layout.folders().empty() || root.empty())
  {
    return noRootResult();
  }

  const auto includeSystem = !workspace.settings().exportCustomOnly;
  const auto files = workspace.libraryTreeFiles(includeSystem, pollCancel);
  const auto display = AbsPathDisplay(workspace.index().snapshot(), root);
  const auto diagnosticsPath = outputPath(root, "vertexadt-workspace-diagnostics.csv");
  writeFile(diagnosticsPath,
    csvRow({"program", "line", "column", "severity", "code", "source", "message",
      "related_program"}),
    false);

  int fileCount = 0;
  int diagnosticCount = 0;
  int errorCount = 0;
  int warningCount = 0;
  int informationCount = 0;

  for (std::size_t batchStart = 0; batchStart < files.size(); batchStart += kFilesPerBatch)
  {
    const auto batchEnd = std::min(batchStart + kFilesPerBatch, files.size());
    std::string chunk;
    for (auto i = batchStart; i < batchEnd; ++i)
    {
      if (pollCancel)
      {
        pollCancel();
      }
      const auto& path = files[i];
      auto text = readDisk(path);
      if (!text || isValidationExcluded(*text))
      {
        continue;
      }
      ++fileCount;
      const auto snapshot = Snapshot{
        .uri = uriFromPath(path),
        .version = 1,
        .languageId = "vertexadt",
        .text = std::move(*text),
      };
      const auto diagnostics =
        workspace.runExportDocumentValidate(snapshot, kExportMaxProblems, pollCancel);
      const auto program = display(path);
      for (const auto& diagnostic : diagnostics)
      {
        std::string related;
        if (diagnostic.relatedInformation && !diagnostic.relatedInformation->empty())
        {
          const auto& uri = diagnostic.relatedInformation->front().location.uri;
          const auto relatedPath = uri.isFileUri() ? uri.fsPath() : std::string();
          if (!relatedPath.empty())
          {
            related = display(std::filesystem::path(relatedPath));
          }
        }
        const auto severity = severityLabel(diagnostic);
        chunk += csvRow({
          program,
          std::to_string(diagnostic.range.start.line + 1),
          std::to_string(diagnostic.range.start.character + 1),
          std::string(severity),
          codeText(diagnostic),
          diagnostic.source.value_or(std::string()),
          messageText(diagnostic),
          related,
        });
        ++diagnosticCount;
        if (severity == "error")
        {
          ++errorCount;
        }
        else if (severity == "warning")
        {
          ++warningCount;
        }
        else if (severity == "information")
        {
          ++informationCount;
        }
      }
    }
    if (!chunk.empty())
    {
      writeFile(diagnosticsPath, chunk, true);
    }
  }

  Object result;
  result["diagnosticsPath"] = Value(utf8Path(diagnosticsPath));
  result["fileCount"] = Value(static_cast<std::int32_t>(fileCount));
  result["diagnosticCount"] = Value(static_cast<std::int32_t>(diagnosticCount));
  result["errorCount"] = Value(static_cast<std::int32_t>(errorCount));
  result["warningCount"] = Value(static_cast<std::int32_t>(warningCount));
  result["informationCount"] = Value(static_cast<std::int32_t>(informationCount));
  return Value(std::move(result));
}

} // namespace vertexadt
