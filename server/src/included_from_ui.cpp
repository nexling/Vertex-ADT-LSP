#include "included_from_ui.h"

#include "parent_context.h"

#include <string>
#include <vector>

namespace vertexadt {
namespace {

using lsp::json::Array;
using lsp::json::Object;
using lsp::json::Value;

auto str(std::string_view text) -> Value
{
  return Value(std::string(text));
}

auto position(int line, int character) -> Value
{
  Object object;
  object["line"] = Value(line);
  object["character"] = Value(character);
  return Value(std::move(object));
}

auto range(int line, int startCharacter, int endCharacter) -> Value
{
  Object object;
  object["start"] = position(line, startCharacter);
  object["end"] = position(line, endCharacter);
  return Value(std::move(object));
}

auto links(const ParentUiContext& context, const Snapshot& snapshot) -> ParentLinks
{
  return resolveParentLinks(
    snapshot.text, pathFromUri(snapshot.uri), context.layout, context.index, context.inferParents);
}

// A library-relative path `findIncludedFile` resolves (it needs a `/`).
auto directivePath(const InferredParent& parent) -> std::string
{
  return parent.displayPath.find('/') == std::string::npos ? "./" + parent.displayPath
                                                           : parent.displayPath;
}

auto pinAction(const Value& uri, const std::string& title, const std::string& newText) -> Value
{
  Object edit;
  edit["range"] = range(0, 0, 0);
  edit["newText"] = str(newText);
  Array edits;
  edits.push_back(Value(std::move(edit)));
  Object changes;
  changes[uri.string()] = Value(std::move(edits));
  Object workspaceEdit;
  workspaceEdit["changes"] = Value(std::move(changes));
  Object action;
  action["title"] = str(title);
  action["kind"] = str("source");
  action["edit"] = Value(std::move(workspaceEdit));
  return Value(std::move(action));
}

} // namespace

auto inferredParentsCodeLens(const ParentUiContext& context, const Snapshot& snapshot)
  -> std::optional<lsp::json::Value>
{
  if (snapshot.languageId != "vertexadt" || !context.inferParents)
  {
    return std::nullopt;
  }
  const auto resolved = links(context, snapshot);
  if (resolved.writtenCoversFile || resolved.candidates.empty())
  {
    return std::nullopt;
  }
  const auto& parents = resolved.candidates;
  std::string title = "Included from: ";
  if (resolved.overCap)
  {
    title += std::to_string(parents.size()) + " parents, too many to infer, pin one";
  }
  else if (parents.size() <= 2)
  {
    title += parents.front().displayPath;
    if (parents.size() == 2)
    {
      title += ", " + parents.back().displayPath;
    }
  }
  else
  {
    title += parents.front().displayPath + " +" + std::to_string(parents.size() - 1);
  }

  Array rows;
  for (const auto& parent : parents)
  {
    Object row;
    row["uri"] = str(uriFromPath(parent.absPath).toString());
    row["displayPath"] = str(parent.displayPath);
    rows.push_back(Value(std::move(row)));
  }
  Array arguments;
  arguments.push_back(Value(std::move(rows)));
  Object command;
  command["title"] = str(title);
  command["command"] = str(kOpenIncludedFromParentsCommand);
  command["arguments"] = Value(std::move(arguments));
  Object lens;
  lens["range"] = range(0, 0, 1);
  lens["command"] = Value(std::move(command));
  return Value(std::move(lens));
}

auto pinIncludedFromActions(
  const ParentUiContext& context,
  const Snapshot& snapshot,
  const lsp::json::Value& uri) -> lsp::json::Array
{
  Array actions;
  if (snapshot.languageId != "vertexadt" || !context.inferParents || !uri.isString())
  {
    return actions;
  }
  const auto resolved = links(context, snapshot);
  if (resolved.writtenCoversFile || resolved.candidates.empty())
  {
    return actions;
  }
  const std::string eol = snapshot.text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
  std::string all;
  for (const auto& parent : resolved.candidates)
  {
    const auto line = "##included_from " + directivePath(parent) + eol;
    all += line;
    actions.push_back(pinAction(uri, "Pin ##included_from " + directivePath(parent), line));
  }
  if (resolved.candidates.size() > 1)
  {
    actions.push_back(pinAction(
      uri,
      "Pin all " + std::to_string(resolved.candidates.size()) + " inferred parents as ##included_from",
      all));
  }
  return actions;
}

} // namespace vertexadt
