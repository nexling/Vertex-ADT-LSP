#include "program_move_commands.h"

#include "program_move.h"
#include "text_util.h"

namespace vertexadt {
namespace {

auto stringField(const lsp::json::Value& object, std::string_view name) -> std::optional<std::string>
{
  if (!object.isObject())
  {
    return std::nullopt;
  }
  const auto* value = object.object().find(std::string(name));
  if (!value || !value->isString())
  {
    return std::nullopt;
  }
  return value->string();
}

bool boolField(const lsp::json::Value& object, std::string_view name)
{
  if (!object.isObject())
  {
    return false;
  }
  const auto* value = object.object().find(std::string(name));
  return value && value->isBoolean() && value->boolean();
}

auto firstArgument(const lsp::json::Value& params) -> lsp::json::Value
{
  if (!params.isObject())
  {
    return {};
  }
  const auto* arguments = params.object().find("arguments");
  if (!arguments || !arguments->isArray() || arguments->array().empty())
  {
    return {};
  }
  return arguments->array().front();
}

auto single(std::string_view key, lsp::json::Value value) -> lsp::json::Value
{
  lsp::json::Object out;
  out[std::string(key)] = std::move(value);
  return lsp::json::Value(std::move(out));
}

auto errorResult(std::string message) -> lsp::json::Value
{
  return single("error", lsp::json::Value(std::move(message)));
}

auto makeInput(WorkspaceState& workspace, const std::function<void()>& pollCancel, MoveOrigin origin)
  -> ProgramMoveInput
{
  return ProgramMoveInput{
    .layout = workspace.layout(),
    .renames = {},
    .origin = origin,
    .confirmShadow = false,
    .scanFiles = [&workspace, pollCancel]() { return workspace.libraryTreeFiles(true, pollCancel); },
    .textForPath = [&workspace](const std::filesystem::path& path) { return workspace.textForPath(path); },
    .openVersion =
      [&workspace](const std::filesystem::path& path) -> std::optional<int>
    {
      if (auto snapshot = workspace.documents().snapshotByNormalizedPath(normalizeFsPath(path.string())))
      {
        return snapshot->version;
      }
      return std::nullopt;
    },
    .pollCancel = pollCancel,
  };
}

} // namespace

auto programMoveFileOperations() -> lsp::FileOperationOptions
{
  lsp::FileOperationOptions options;
  options.willRename = lsp::FileOperationRegistrationOptions{
    .filters = {lsp::FileOperationFilter{
      .pattern = lsp::FileOperationPattern{.glob = "**/*"},
      .scheme = "file",
    }},
  };
  return options;
}

auto moveProgramCommand(
  WorkspaceState& workspace,
  const lsp::json::Value& params,
  const std::function<void()>& pollCancel) -> lsp::json::Value
{
  const auto argument = firstArgument(params);
  const auto uri = stringField(argument, "uri");
  if (!uri)
  {
    return errorResult("No file to move.");
  }
  const auto source = pathFromUri(lsp::Uri::parse(*uri));
  const auto mode = stringField(argument, "mode").value_or("plan");
  const auto& layout = workspace.layout();

  if (mode == "default")
  {
    if (const auto text = defaultMoveTargetText(layout, source))
    {
      return single("target", lsp::json::Value(*text));
    }
    return errorResult("The file is not inside the Custom or System tree.");
  }

  const auto target = resolveMoveTargetText(layout, source, stringField(argument, "target").value_or(""));
  if (!target.path)
  {
    return errorResult(target.error);
  }
  if (mode == "validate")
  {
    return single("ok", lsp::json::Value(true));
  }

  auto input = makeInput(workspace, pollCancel, MoveOrigin::Command);
  input.renames.push_back(MoveRename{.oldPath = source, .newPath = *target.path});
  input.confirmShadow = boolField(argument, "confirmShadow");
  const auto plan = planProgramMove(input);
  if (plan.refusal)
  {
    return single("refusal", lsp::json::Value(*plan.refusal));
  }
  if (plan.confirmation)
  {
    return single("confirmation", lsp::json::Value(*plan.confirmation));
  }
  lsp::json::Object out;
  out["edit"] = programMoveEditJson(plan, true);
  out["report"] = programMoveReportJson(plan);
  return lsp::json::Value(std::move(out));
}

auto willRenameFilesEdit(
  WorkspaceState& workspace,
  const lsp::json::Value& params,
  const std::function<void()>& pollCancel,
  const std::function<void(lsp::json::Value)>& report) -> lsp::json::Value
{
  const auto* files = params.isObject() ? params.object().find("files") : nullptr;
  if (!files || !files->isArray())
  {
    return {};
  }
  auto input = makeInput(workspace, pollCancel, MoveOrigin::WillRename);
  for (const auto& file : files->array())
  {
    const auto oldUri = stringField(file, "oldUri");
    const auto newUri = stringField(file, "newUri");
    if (!oldUri || !newUri)
    {
      continue;
    }
    input.renames.push_back(MoveRename{
      .oldPath = pathFromUri(lsp::Uri::parse(*oldUri)),
      .newPath = pathFromUri(lsp::Uri::parse(*newUri)),
    });
  }
  if (input.renames.empty())
  {
    return {};
  }
  const auto plan = planProgramMove(input);
  if (plan.fileMoves.empty())
  {
    return {};
  }
  if (report)
  {
    report(programMoveReportJson(plan));
  }
  if (plan.edits.empty())
  {
    return {};
  }
  return programMoveEditJson(plan, false);
}

} // namespace vertexadt
