#include "impact_codelens.h"

#include "namespace_graph.h"
#include "text_util.h"
#include "utf16_text.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

constexpr std::string_view kShowImpactAnalysisCommand = "vertexadt.showImpactAnalysis";

using lsp::json::Array;
using lsp::json::Object;
using lsp::json::Value;

auto str(std::string_view text) -> Value
{
  return Value(std::string(text));
}

struct IncomingReference
{
  std::string fromKey;
  const FileLink* link = nullptr;
  bool isCall = false;
};

auto entryMap(const ImpactContext& context) -> IndexEntryMap
{
  IndexEntryMap byKey;
  for (const auto& entry : context.index)
  {
    byKey.emplace(entry->identity.key, entry);
  }
  return byKey;
}

// server.js getIncomingReferences over overlay identities, sorted by source
// identity localeCompare then line.
auto incomingReferences(const IndexEntryMap& byKey, std::string_view target)
  -> std::vector<IncomingReference>
{
  std::vector<IncomingReference> references;
  for (const auto& [key, entry] : byKey)
  {
    for (const auto& link : entry->includeLinks)
    {
      if (link.target == target)
      {
        references.push_back({key, &link, false});
      }
    }
    if (commonsGated(*entry))
    {
      for (const auto& link : entry->callLinks)
      {
        if (link.target == target)
        {
          references.push_back({key, &link, true});
        }
      }
    }
  }
  std::stable_sort(
    references.begin(),
    references.end(),
    [](const IncomingReference& a, const IncomingReference& b) {
      const auto cmp = jsLocaleCompare(a.fromKey, b.fromKey);
      if (cmp != 0)
      {
        return cmp < 0;
      }
      return a.link->line < b.link->line;
    });
  return references;
}

auto targetIdentity(const ImpactContext& context, const lsp::Uri& uri)
  -> std::optional<OverlayIdentity>
{
  const auto path = pathFromUri(uri);
  if (!context.layout.shouldIndex(path))
  {
    return std::nullopt;
  }
  return context.layout.overlayForPath(path);
}

auto trimmed(std::string_view text) -> std::string
{
  const auto wide = utf8To16(text);
  return utf16To8(jsTrim(wide));
}

auto lensPosition(int character) -> Value
{
  Object position;
  position["line"] = Value(0);
  position["character"] = Value(character);
  return Value(std::move(position));
}

} // namespace

auto impactCodeLenses(
  const ImpactContext& context,
  const std::string& uriText,
  const std::optional<Snapshot>& snapshot) -> Value
{
  Array lenses;
  if (!context.enabled || (snapshot && snapshot->languageId != "vertexadt"))
  {
    return Value(std::move(lenses));
  }
  const auto identity = targetIdentity(context, lsp::Uri::parse(uriText));
  if (!identity)
  {
    return Value(std::move(lenses));
  }
  const auto byKey = entryMap(context);
  const auto incoming = incomingReferences(byKey, identity->key);
  if (static_cast<double>(incoming.size()) < context.minIncomingCount)
  {
    return Value(std::move(lenses));
  }

  Object range;
  range["start"] = lensPosition(0);
  range["end"] = lensPosition(1);
  Object data;
  data["kind"] = str("impact");
  data["uri"] = str(uriText);
  Object lens;
  lens["range"] = Value(std::move(range));
  lens["data"] = Value(std::move(data));
  lenses.push_back(Value(std::move(lens)));
  return Value(std::move(lenses));
}

auto resolveImpactCodeLens(const ImpactContext& context, Value codeLens) -> Value
{
  if (!codeLens.isObject())
  {
    return codeLens;
  }
  auto& lens = codeLens.object();
  const auto* data = lens.find("data");
  if (!data || !data->isObject())
  {
    return codeLens;
  }
  const auto* kind = data->object().find("kind");
  const auto* uriValue = data->object().find("uri");
  if (!kind || !kind->isString() || kind->string() != "impact" || !uriValue
      || !uriValue->isString() || uriValue->string().empty())
  {
    return codeLens;
  }
  const auto uriText = uriValue->string();

  const auto byKey = entryMap(context);
  const auto identity = targetIdentity(context, lsp::Uri::parse(uriText));
  const auto incoming = identity
    ? incomingReferences(byKey, identity->key)
    : std::vector<IncomingReference>{};

  int calls = 0;
  int includes = 0;
  Array referenceRows;
  std::vector<std::string> uniqueFrom;
  std::unordered_set<std::string> uniqueFromSet;
  for (const auto& reference : incoming)
  {
    if (reference.isCall)
    {
      ++calls;
    }
    else
    {
      ++includes;
    }
    const auto from = programPathForKey(context.layout, byKey, reference.fromKey);
    Object row;
    row["fromPath"] = str(reference.fromKey);
    row["fromDisplayPath"] = str(from.displayPath);
    row["fromUri"] = str(from.uri);
    row["line"] = Value(reference.link->line);
    row["lineText"] = str(trimmed(reference.link->lineText));
    row["type"] = str(reference.isCall ? "call" : "include");
    row["sourceSection"] = str(reference.link->sourceSection);
    row["targetSection"] = str(reference.link->targetSection);
    referenceRows.push_back(Value(std::move(row)));
    if (uniqueFromSet.insert(reference.fromKey).second)
    {
      uniqueFrom.push_back(reference.fromKey);
    }
  }

  if (calls + includes == 0)
  {
    lens.remove("command");
    return codeLens;
  }

  struct FileRow
  {
    std::string path;
    ProgramPath info;
  };
  struct Scope
  {
    std::string rootPath;
    ProgramPath root;
    std::vector<FileRow> files;
  };
  std::vector<NamespaceClosure> seenClosures;
  std::vector<Scope> scopes;
  for (const auto& fromKey : uniqueFrom)
  {
    for (const auto& rootKey : ultimateNamespaceRoots(byKey, fromKey))
    {
      auto closure = forwardNamespaceClosure(byKey, rootKey);
      const auto duplicate = std::any_of(
        seenClosures.begin(),
        seenClosures.end(),
        [&](const NamespaceClosure& seen) { return seen.fileSet == closure.fileSet; });
      if (duplicate)
      {
        continue;
      }
      Scope scope;
      scope.rootPath = rootKey;
      scope.root = programPathForKey(context.layout, byKey, rootKey);
      for (const auto& fileKey : closure.files)
      {
        scope.files.push_back({fileKey, programPathForKey(context.layout, byKey, fileKey)});
      }
      std::stable_sort(
        scope.files.begin(),
        scope.files.end(),
        [](const FileRow& a, const FileRow& b) {
          return jsLocaleCompare(a.info.displayPath, b.info.displayPath) < 0;
        });
      scopes.push_back(std::move(scope));
      seenClosures.push_back(std::move(closure));
    }
  }
  std::stable_sort(scopes.begin(), scopes.end(), [](const Scope& a, const Scope& b) {
    return jsLocaleCompare(a.root.displayPath, b.root.displayPath) < 0;
  });

  std::unordered_set<std::string> blastRadius;
  for (const auto& closure : seenClosures)
  {
    blastRadius.insert(closure.fileSet.begin(), closure.fileSet.end());
  }

  Array scopeRows;
  for (const auto& scope : scopes)
  {
    Array files;
    for (const auto& file : scope.files)
    {
      Object row;
      row["path"] = str(file.path);
      row["displayPath"] = str(file.info.displayPath);
      row["uri"] = str(file.info.uri);
      files.push_back(Value(std::move(row)));
    }
    Object row;
    row["rootPath"] = str(scope.rootPath);
    row["rootDisplayPath"] = str(scope.root.displayPath);
    row["rootUri"] = str(scope.root.uri);
    row["files"] = Value(std::move(files));
    row["fileCount"] = Value(static_cast<int>(scope.files.size()));
    scopeRows.push_back(Value(std::move(row)));
  }

  Object incomingObject;
  incomingObject["calls"] = Value(calls);
  incomingObject["includes"] = Value(includes);
  incomingObject["references"] = Value(std::move(referenceRows));
  Object analysis;
  analysis["incoming"] = Value(std::move(incomingObject));
  analysis["scopes"] = Value(std::move(scopeRows));
  analysis["totalBlastRadiusFiles"] = Value(static_cast<int>(blastRadius.size()));
  Object argument;
  argument["uri"] = str(uriText);
  argument["analysis"] = Value(std::move(analysis));
  Array arguments;
  arguments.push_back(Value(std::move(argument)));

  Object command;
  command["title"] = str(
    "Impact: " + std::to_string(calls) + " calls, " + std::to_string(includes)
    + " includes | Blast radius: " + std::to_string(blastRadius.size()) + " instances in "
    + std::to_string(scopes.size()) + " program(s)");
  command["command"] = str(kShowImpactAnalysisCommand);
  command["arguments"] = Value(std::move(arguments));
  lens["command"] = Value(std::move(command));
  return codeLens;
}

} // namespace vertexadt
