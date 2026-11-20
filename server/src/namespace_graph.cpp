#include "namespace_graph.h"

#include "text_util.h"

#include <filesystem>
#include <utility>

namespace vertexadt {

bool commonsGated(const IndexEntry& entry)
{
  return entry.commonIndexes || entry.commonPoints;
}

auto incomingNamespaceParents(const IndexEntryMap& byKey, std::string_view target)
  -> std::unordered_set<std::string>
{
  std::unordered_set<std::string> parents;
  for (const auto& [otherKey, entry] : byKey)
  {
    for (const auto& link : entry->includeLinks)
    {
      if (link.target == target)
      {
        parents.insert(otherKey);
      }
    }
    if (commonsGated(*entry))
    {
      for (const auto& link : entry->callLinks)
      {
        if (link.target == target)
        {
          parents.insert(otherKey);
        }
      }
    }
  }
  return parents;
}

auto ultimateNamespaceRoots(const IndexEntryMap& byKey, std::string_view filePath)
  -> std::vector<std::string>
{
  std::vector<std::string> roots;
  std::unordered_set<std::string> rootSet;
  std::unordered_set<std::string> visited;
  std::vector<std::string> stack{std::string(filePath)};
  while (!stack.empty())
  {
    auto node = std::move(stack.back());
    stack.pop_back();
    if (!visited.insert(node).second)
    {
      continue;
    }
    const auto parents = incomingNamespaceParents(byKey, node);
    if (parents.empty())
    {
      if (rootSet.insert(node).second)
      {
        roots.push_back(node);
      }
      continue;
    }
    for (const auto& parent : parents)
    {
      if (!visited.contains(parent))
      {
        stack.push_back(parent);
      }
    }
  }
  return roots;
}

auto forwardNamespaceClosure(const IndexEntryMap& byKey, std::string_view root)
  -> NamespaceClosure
{
  NamespaceClosure scope;
  scope.root = std::string(root);
  std::vector<std::string> stack{scope.root};
  while (!stack.empty())
  {
    auto current = std::move(stack.back());
    stack.pop_back();
    if (!scope.fileSet.insert(current).second)
    {
      continue;
    }
    scope.files.push_back(current);
    const auto it = byKey.find(current);
    if (it == byKey.end())
    {
      continue;
    }
    const auto& entry = *it->second;
    for (const auto& link : entry.includeLinks)
    {
      stack.push_back(link.target);
    }
    if (commonsGated(entry))
    {
      for (const auto& link : entry.callLinks)
      {
        stack.push_back(link.target);
      }
    }
  }
  return scope;
}

auto programPathForKey(
  const LibraryLayout& layout,
  const IndexEntryMap& byKey,
  const std::string& key) -> ProgramPath
{
  if (const auto it = byKey.find(key); it != byKey.end())
  {
    return {it->second->identity.displayPath, it->second->identity.uri.toString()};
  }
  const auto& root = layout.workspaceRoot();
  const auto relative = std::filesystem::path(key);
  if (root.empty())
  {
    return {key, uriFromPath(relative).toString()};
  }
  std::filesystem::path abs;
  if (toLowerAscii(root.filename().string()).starts_with("custom"))
  {
    abs = root / relative;
  }
  else if (layout.customOccupies(key))
  {
    abs = root / layout.customFolderName() / relative;
  }
  else
  {
    abs = root / "system" / relative;
  }
  return {abs.lexically_relative(root).generic_string(), uriFromPath(abs).toString()};
}

} // namespace vertexadt
