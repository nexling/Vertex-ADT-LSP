#include "workspace_commands.h"

#include "call_param_sites.h"
#include "library_paths.h"
#include "local_slot_rename.h"
#include "text_util.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

auto position(int line, int character) -> lsp::json::Value
{
  lsp::json::Object object;
  object["line"] = lsp::json::Value(static_cast<std::int32_t>(line));
  object["character"] = lsp::json::Value(static_cast<std::int32_t>(character));
  return lsp::json::Value(std::move(object));
}

struct FileToScan
{
  std::string uri;
  std::filesystem::path absPath;
  std::optional<std::string> bufferText;
};

} // namespace

auto executeCommandOptions() -> lsp::ExecuteCommandOptions
{
  return lsp::ExecuteCommandOptions{.commands = {}};
}

auto fixMissingCallParameters(
  WorkspaceState& workspace,
  const std::function<void()>& pollCancel) -> lsp::json::Value
{
  lsp::json::Object result;
  lsp::json::Object changes;
  int fixedCount = 0;

  if (workspace.settings().validateParameterCount)
  {
    // Keyed by normalized path, index first, then open buffers override.
    std::map<std::string, FileToScan> files;
    for (const auto& entry : workspace.index().snapshot())
    {
      if (!entry || entry->identity.absPath.empty())
      {
        continue;
      }
      const auto key = normalizeFsPath(entry->identity.absPath.string());
      files.emplace(key, FileToScan{
        .uri = uriFromPath(entry->identity.absPath).toString(),
        .absPath = entry->identity.absPath,
        .bufferText = std::nullopt,
      });
    }
    auto& documents = workspace.documents();
    for (const auto& uri : documents.openUris())
    {
      const auto snapshot = documents.snapshot(uri);
      if (!snapshot || snapshot->languageId != "vertexadt" || !uri.isFileUri())
      {
        continue;
      }
      const auto absPath = std::filesystem::path(uri.fsPath());
      const auto key = normalizeFsPath(absPath.string());
      if (key.empty())
      {
        continue;
      }
      files[key] = FileToScan{
        .uri = uri.toString(),
        .absPath = absPath,
        .bufferText = snapshot->text,
      };
    }

    const auto stores = ValidateStores{
      .extracts = &workspace.extracts(),
      .linkedFiles = &workspace.linkedFiles(),
      .index = &workspace.index(),
      .layout = &workspace.layout(),
      .textForPath = [&workspace](const std::filesystem::path& path) {
        return workspace.textForPath(path);
      },
    };

    for (auto& [key, file] : files)
    {
      if (pollCancel)
      {
        pollCancel();
      }
      auto text = file.bufferText ? std::move(file.bufferText) : workspace.textForPath(file.absPath);
      if (!text || text->empty())
      {
        continue;
      }
      const auto edits = callParamWorkspaceEdits(stores, *text, pollCancel);
      if (edits.empty())
      {
        continue;
      }
      lsp::json::Array textEdits;
      for (const auto& edit : edits)
      {
        lsp::json::Object range;
        range["start"] = position(edit.line, edit.startCharacter);
        range["end"] = position(edit.line, edit.endCharacter);
        lsp::json::Object textEdit;
        textEdit["range"] = lsp::json::Value(std::move(range));
        textEdit["newText"] = lsp::json::Value(edit.newText);
        textEdits.push_back(lsp::json::Value(std::move(textEdit)));
      }
      fixedCount += static_cast<int>(edits.size());
      changes[file.uri] = lsp::json::Value(std::move(textEdits));
    }
  }

  result["changes"] = lsp::json::Value(std::move(changes));
  result["fixedCount"] = lsp::json::Value(static_cast<std::int32_t>(fixedCount));
  return lsp::json::Value(std::move(result));
}

namespace {

auto assignResult(lsp::json::Object changes, int assignedCount, std::string_view error = {})
  -> lsp::json::Value
{
  lsp::json::Object result;
  result["changes"] = lsp::json::Value(std::move(changes));
  result["assignedCount"] = lsp::json::Value(static_cast<std::int32_t>(assignedCount));
  if (!error.empty())
  {
    result["error"] = lsp::json::Value(std::string(error));
  }
  return lsp::json::Value(std::move(result));
}

// `arguments[0].uri` (the host forwards `[{ uri }]`).
auto targetUriArgument(const lsp::json::Value& params) -> std::optional<std::string>
{
  if (!params.isObject())
  {
    return std::nullopt;
  }
  const auto* arguments = params.object().find("arguments");
  const lsp::json::Value* holder = nullptr;
  if (arguments && arguments->isArray() && !arguments->array().empty())
  {
    holder = &arguments->array().front();
  }
  const auto* uri = holder && holder->isObject() ? holder->object().find("uri") : nullptr;
  if (!uri || !uri->isString() || uri->string().empty())
  {
    return std::nullopt;
  }
  return uri->string();
}

} // namespace

auto assignLocalSlotNames(
  WorkspaceState& workspace,
  const lsp::json::Value& params,
  const std::function<void()>& pollCancel) -> lsp::json::Value
{
  const auto uriText = targetUriArgument(params);
  if (!uriText)
  {
    return assignResult({}, 0, "No target file specified");
  }
  const auto uri = lsp::Uri::parse(*uriText);
  if (!uri.isFileUri() || uri.fsPath().empty())
  {
    return assignResult({}, 0, "Invalid target file URI");
  }
  const auto absPath = std::filesystem::path(uri.fsPath());

  workspace.rebuildIndex(pollCancel);

  std::optional<std::string> text;
  if (const auto snapshot = workspace.documents().snapshot(uri))
  {
    if (snapshot->languageId != "vertexadt")
    {
      return assignResult({}, 0);
    }
    text = snapshot->text;
  }
  else
  {
    text = workspace.textForPath(absPath);
  }
  if (!text || text->empty())
  {
    return assignResult({}, 0, "Could not read target file");
  }

  const auto filePrefix = fileLocalSlotPrefixForPath(workspace.index().snapshot(), absPath);
  const auto pairs = localSlotRenamePairs(*text, filePrefix);
  if (pairs.empty())
  {
    return assignResult({}, 0);
  }
  if (pollCancel)
  {
    pollCancel();
  }
  const auto edits = fileWideLocalSlotRenameEdits(splitLines(*text), pairs);
  if (edits.empty())
  {
    return assignResult({}, 0);
  }
  lsp::json::Array textEdits;
  for (const auto& edit : edits)
  {
    lsp::json::Object range;
    range["start"] = position(edit.line, edit.startCharacter);
    range["end"] = position(edit.line, edit.endCharacter);
    lsp::json::Object textEdit;
    textEdit["range"] = lsp::json::Value(std::move(range));
    textEdit["newText"] = lsp::json::Value(edit.newText);
    textEdits.push_back(lsp::json::Value(std::move(textEdit)));
  }
  lsp::json::Object changes;
  // Keyed by the request's own URI so the editor matches its open document.
  changes[*uriText] = lsp::json::Value(std::move(textEdits));
  return assignResult(std::move(changes), static_cast<int>(edits.size()));
}

} // namespace vertexadt
