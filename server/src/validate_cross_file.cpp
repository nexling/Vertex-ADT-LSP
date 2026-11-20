#include "validate.h"

#include "namespace_graph.h"

#include "text_util.h"
#include "utf16_text.h"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

using EntryMap = IndexEntryMap;

using ConflictScope = NamespaceClosure;

struct Participant
{
  std::string effectiveScopeKey;
  int number = 0;
  std::string varName;
  std::string filePath;
  int line = 0;
  std::string lineText;
  std::string sectionName;
};

struct ConflictPeer
{
  std::string varName;
  std::string filePath;
  int line = 0;
  std::string lineText;
  std::string sectionName;
};

struct Conflict
{
  int number = 0;
  std::string varName;
  std::string filePath;
  int line = 0;
  std::string lineText;
  std::string sectionName;
  ConflictPeer conflictsWith;
};

struct ChainHop
{
  std::string fromFile;
  std::string toFile;
  std::string linkFile;
  int line = 0;
  std::string lineText;
  std::string type;
};

struct Neighbor
{
  std::string to;
  int line = 0;
  std::string lineText;
  std::string type;
  std::string fromFile;
};

bool isValueDeclarationType(const std::optional<std::string>& variableType)
{
  return variableType && toLowerAscii(*variableType) == "value";
}


bool declarationMatchesLinkSection(
  std::string_view declSectionName,
  std::string_view targetSection,
  std::string_view sourceSection)
{
  if (!targetSection.empty())
  {
    return declSectionName == targetSection;
  }
  return declSectionName == sourceSection;
}

std::string basenameOf(std::string_view path)
{
  const auto slash = path.find_last_of("/\\");
  return slash == std::string_view::npos
    ? std::string(path)
    : std::string(path.substr(slash + 1));
}

std::string displayBasename(const EntryMap& byKey, std::string_view key)
{
  const auto it = byKey.find(std::string(key));
  if (it != byKey.end())
  {
    return basenameOf(it->second->identity.displayPath);
  }
  return basenameOf(key);
}

const IndexEntry* findEntry(const EntryMap& byKey, std::string_view key)
{
  const auto it = byKey.find(std::string(key));
  return it == byKey.end() ? nullptr : it->second.get();
}

void putParsed(
  EntryMap& byKey,
  OverlayIdentity identity,
  std::string_view text,
  const LibraryLayout& layout)
{
  if (byKey.contains(identity.key))
  {
    return;
  }
  auto parsed = parseIndexEntry(text, std::move(identity), layout);
  const auto key = parsed.identity.key;
  byKey.emplace(key, std::make_shared<const IndexEntry>(std::move(parsed)));
}

EntryMap loadConflictIndex(ValidateContext& ctx)
{
  EntryMap byKey;
  if (ctx.stores.index)
  {
    for (const auto& entry : ctx.stores.index->snapshot())
    {
      if (entry)
      {
        byKey.emplace(entry->identity.key, entry);
      }
    }
  }

  std::optional<OverlayIdentity> identity;
  if (ctx.stores.layout)
  {
    identity = ctx.stores.layout->overlayForUri(ctx.prep->snapshot.uri);
  }
  // Like Node, compare against the workspace index only. Validation-excluded
  // files (debug dumps, vxdbase_winstyle shells) are never indexed, so they
  // never participate. This file's own entry falls back to its live text.
  if (identity && !byKey.contains(identity->key) && ctx.stores.layout)
  {
    putParsed(byKey, *identity, ctx.prep->snapshot.text, *ctx.stores.layout);
  }
  return byKey;
}

std::vector<std::size_t> computeLineOffsets(std::string_view text)
{
  std::vector<std::size_t> offsets{0};
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    const auto ch = static_cast<unsigned char>(text[i]);
    if (ch == '\r' || ch == '\n')
    {
      if (ch == '\r' && i + 1 < text.size()
          && static_cast<unsigned char>(text[i + 1]) == '\n')
      {
        ++i;
      }
      offsets.push_back(i + 1);
    }
  }
  return offsets;
}

lsp::Position positionAt(std::string_view text, const std::vector<std::size_t>& offsets, std::size_t offset)
{
  offset = std::min(offset, text.size());
  if (offsets.empty())
  {
    return {.line = 0, .character = static_cast<lsp::Uint>(utf16Offset(text, offset))};
  }
  std::size_t low = 0;
  std::size_t high = offsets.size();
  while (low < high)
  {
    const auto mid = (low + high) / 2;
    if (offsets[mid] > offset)
    {
      high = mid;
    }
    else
    {
      low = mid + 1;
    }
  }
  const auto line = low == 0 ? 0 : low - 1;
  const auto start = offsets[line];
  return {
    .line = static_cast<lsp::Uint>(line),
    .character = static_cast<lsp::Uint>(utf16Offset(text.substr(start), offset - start)),
  };
}

lsp::Range indexLineRange(int line, std::string_view lineText)
{
  const auto safeLine = static_cast<lsp::Uint>(std::max(0, line));
  return {
    .start = {.line = safeLine, .character = 0},
    .end = {
      .line = safeLine,
      .character = static_cast<lsp::Uint>(utf16Length(lineText)),
    },
  };
}



bool fileHasForwardNamespaceLinks(const EntryMap& byKey, std::string_view filePath)
{
  const auto* entry = findEntry(byKey, filePath);
  if (entry == nullptr)
  {
    return false;
  }
  if (!entry->includeLinks.empty())
  {
    return true;
  }
  return commonsGated(*entry) && !entry->callLinks.empty();
}


bool closuresEqual(const ConflictScope& a, const ConflictScope& b)
{
  return a.fileSet == b.fileSet;
}

std::vector<ConflictScope> declarationConflictScopes(const EntryMap& byKey, std::string_view filePath)
{
  std::vector<ConflictScope> scopes;
  const auto addScope = [&](std::string_view root) {
    auto scope = forwardNamespaceClosure(byKey, root);
    for (const auto& existing : scopes)
    {
      if (closuresEqual(existing, scope))
      {
        return;
      }
    }
    scopes.push_back(std::move(scope));
  };

  const auto parents = incomingNamespaceParents(byKey, filePath);
  if (parents.empty())
  {
    addScope(filePath);
  }
  else
  {
    for (const auto& root : ultimateNamespaceRoots(byKey, filePath))
    {
      addScope(root);
    }
    if (fileHasForwardNamespaceLinks(byKey, filePath))
    {
      addScope(filePath);
    }
  }
  if (scopes.empty())
  {
    addScope(filePath);
  }
  return scopes;
}

void appendNativeAndPastes(
  std::vector<Participant>& participants,
  const IndexEntry& entry,
  const EntryMap& byKey)
{
  const auto& memberKey = entry.identity.key;
  for (const auto& decl : entry.declarations)
  {
    if (decl.number <= 0 || isValueDeclarationType(decl.variableType))
    {
      continue;
    }
    participants.push_back(Participant{
      .effectiveScopeKey = memberKey + "::" + decl.sectionName,
      .number = decl.number,
      .varName = decl.varName,
      .filePath = memberKey,
      .line = decl.line,
      .lineText = decl.lineText,
      .sectionName = decl.sectionName,
    });
  }

  auto pasteLinks = [&](const std::vector<FileLink>& links) {
    for (const auto& link : links)
    {
      const auto* callee = findEntry(byKey, link.target);
      if (callee == nullptr)
      {
        continue;
      }
      for (const auto& decl : callee->declarations)
      {
        if (decl.number <= 0 || isValueDeclarationType(decl.variableType))
        {
          continue;
        }
        if (!declarationMatchesLinkSection(decl.sectionName, link.targetSection, link.sourceSection))
        {
          continue;
        }
        participants.push_back(Participant{
          .effectiveScopeKey = memberKey + "::" + link.sourceSection,
          .number = decl.number,
          .varName = decl.varName,
          .filePath = link.target,
          .line = decl.line,
          .lineText = decl.lineText,
          .sectionName = link.sourceSection,
        });
      }
    }
  };

  pasteLinks(entry.includeLinks);
  if (commonsGated(entry))
  {
    pasteLinks(entry.callLinks);
  }
}

std::vector<Participant> effectiveParticipants(
  const EntryMap& byKey,
  const std::vector<std::string>& groupPaths)
{
  std::vector<Participant> participants;
  for (const auto& filePath : groupPaths)
  {
    const auto* entry = findEntry(byKey, filePath);
    if (entry == nullptr)
    {
      continue;
    }
    appendNativeAndPastes(participants, *entry, byKey);
  }
  return participants;
}

std::vector<Conflict> findConflicts(
  const EntryMap& byKey,
  const std::vector<std::string>& groupPaths)
{
  std::vector<Conflict> conflicts;
  std::unordered_map<std::string, Participant> numberOwners;
  std::unordered_set<std::string> conflictKeys;
  const auto participants = effectiveParticipants(byKey, groupPaths);

  for (const auto& participant : participants)
  {
    const auto ownerKey =
      participant.effectiveScopeKey + "::" + std::to_string(participant.number);
    const auto existingIt = numberOwners.find(ownerKey);
    if (existingIt == numberOwners.end())
    {
      numberOwners.emplace(ownerKey, participant);
      continue;
    }
    const auto& existing = existingIt->second;
    if (existing.varName == participant.varName)
    {
      continue;
    }
    auto first = participant.varName;
    auto second = existing.varName;
    if (second < first)
    {
      std::swap(first, second);
    }
    const auto pairKey = ownerKey + "::" + first + "::" + second;
    if (!conflictKeys.insert(pairKey).second)
    {
      continue;
    }
    conflicts.push_back(Conflict{
      .number = participant.number,
      .varName = participant.varName,
      .filePath = participant.filePath,
      .line = participant.line,
      .lineText = participant.lineText,
      .sectionName = participant.sectionName,
      .conflictsWith = {
        .varName = existing.varName,
        .filePath = existing.filePath,
        .line = existing.line,
        .lineText = existing.lineText,
        .sectionName = existing.sectionName,
      },
    });
    conflicts.push_back(Conflict{
      .number = existing.number,
      .varName = existing.varName,
      .filePath = existing.filePath,
      .line = existing.line,
      .lineText = existing.lineText,
      .sectionName = existing.sectionName,
      .conflictsWith = {
        .varName = participant.varName,
        .filePath = participant.filePath,
        .line = participant.line,
        .lineText = participant.lineText,
        .sectionName = participant.sectionName,
      },
    });
  }
  return conflicts;
}

std::vector<Neighbor> forwardNeighbors(const EntryMap& byKey, std::string_view filePath)
{
  std::vector<Neighbor> neighbors;
  const auto* entry = findEntry(byKey, filePath);
  if (entry == nullptr)
  {
    return neighbors;
  }
  for (const auto& link : entry->includeLinks)
  {
    neighbors.push_back(Neighbor{
      .to = link.target,
      .line = link.line,
      .lineText = link.lineText,
      .type = "include",
      .fromFile = std::string(filePath),
    });
  }
  if (commonsGated(*entry))
  {
    for (const auto& link : entry->callLinks)
    {
      neighbors.push_back(Neighbor{
        .to = link.target,
        .line = link.line,
        .lineText = link.lineText,
        .type = "call",
        .fromFile = std::string(filePath),
      });
    }
  }
  return neighbors;
}

std::vector<Neighbor> scopedNeighbors(
  const EntryMap& byKey,
  std::string_view filePath,
  const std::unordered_set<std::string>& scopeFiles)
{
  std::vector<Neighbor> neighbors;
  const auto normalized = std::string(filePath);
  if (!scopeFiles.contains(normalized))
  {
    return neighbors;
  }
  for (const auto& neighbor : forwardNeighbors(byKey, normalized))
  {
    if (scopeFiles.contains(neighbor.to))
    {
      neighbors.push_back(neighbor);
    }
  }
  for (const auto& [otherPath, otherEntry] : byKey)
  {
    if (!scopeFiles.contains(otherPath) || otherPath == normalized)
    {
      continue;
    }
    for (const auto& link : otherEntry->includeLinks)
    {
      if (link.target == normalized)
      {
        neighbors.push_back(Neighbor{
          .to = otherPath,
          .line = link.line,
          .lineText = link.lineText,
          .type = "include",
          .fromFile = otherPath,
        });
      }
    }
    if (commonsGated(*otherEntry))
    {
      for (const auto& link : otherEntry->callLinks)
      {
        if (link.target == normalized)
        {
          neighbors.push_back(Neighbor{
            .to = otherPath,
            .line = link.line,
            .lineText = link.lineText,
            .type = "call",
            .fromFile = otherPath,
          });
        }
      }
    }
  }
  return neighbors;
}

std::optional<std::vector<ChainHop>> findDependencyChain(
  const EntryMap& byKey,
  std::string_view startPath,
  std::string_view endPath,
  const std::unordered_set<std::string>& scopeFiles)
{
  const auto start = std::string(startPath);
  const auto end = std::string(endPath);
  if (!scopeFiles.contains(start) || !scopeFiles.contains(end))
  {
    return std::nullopt;
  }
  if (start == end)
  {
    return std::vector<ChainHop>{};
  }

  std::deque<std::string> queue{start};
  std::unordered_set<std::string> visited{start};
  struct Parent
  {
    std::string previous;
    std::string linkFile;
    int line = 0;
    std::string lineText;
    std::string type;
  };
  std::unordered_map<std::string, Parent> parent;

  while (!queue.empty())
  {
    auto current = std::move(queue.front());
    queue.pop_front();
    if (current == end)
    {
      break;
    }
    for (const auto& neighbor : scopedNeighbors(byKey, current, scopeFiles))
    {
      if (!visited.insert(neighbor.to).second)
      {
        continue;
      }
      parent[neighbor.to] = Parent{
        .previous = current,
        .linkFile = neighbor.fromFile,
        .line = neighbor.line,
        .lineText = neighbor.lineText,
        .type = neighbor.type,
      };
      queue.push_back(neighbor.to);
    }
  }

  if (!parent.contains(end))
  {
    return std::nullopt;
  }

  std::vector<ChainHop> chain;
  auto node = end;
  while (node != start)
  {
    const auto it = parent.find(node);
    if (it == parent.end())
    {
      return std::nullopt;
    }
    chain.push_back(ChainHop{
      .fromFile = it->second.previous,
      .toFile = node,
      .linkFile = it->second.linkFile,
      .line = it->second.line,
      .lineText = it->second.lineText,
      .type = it->second.type,
    });
    node = it->second.previous;
  }
  std::reverse(chain.begin(), chain.end());
  return chain;
}

std::string linkLabel(std::string_view type)
{
  return type == "call" ? "call()" : "# include";
}

std::string formatChainDescription(
  const EntryMap& byKey,
  const std::optional<std::vector<ChainHop>>& chain,
  std::string_view otherBasename)
{
  if (!chain || chain->empty())
  {
    return {};
  }
  if (chain->size() == 1)
  {
    const auto& step = (*chain)[0];
    return " " + std::string(otherBasename) + " is linked through "
      + displayBasename(byKey, step.linkFile) + " on line " + std::to_string(step.line + 1)
      + " (" + linkLabel(step.type) + ").";
  }
  std::string parts;
  for (const auto& step : *chain)
  {
    if (!parts.empty())
    {
      parts += " → ";
    }
    parts += displayBasename(byKey, step.linkFile) + " (line " + std::to_string(step.line + 1)
      + ", " + linkLabel(step.type) + ")";
  }
  return " " + std::string(otherBasename) + " is linked through " + parts + ".";
}

lsp::Uri uriForKey(const EntryMap& byKey, std::string_view key)
{
  const auto* entry = findEntry(byKey, key);
  if (entry != nullptr)
  {
    return entry->identity.uri;
  }
  return {};
}

std::unordered_set<int> usedDeclarationNumbers(const EntryMap& byKey)
{
  std::unordered_set<int> used;
  for (const auto& [_, entry] : byKey)
  {
    for (const auto& decl : entry->declarations)
    {
      if (decl.number > 0)
      {
        used.insert(decl.number);
      }
    }
  }
  return used;
}

int nextGlobalDeclarationNumber(int afterNumber, const std::unordered_set<int>& used)
{
  auto candidate = std::max(1, afterNumber + 1);
  while (used.contains(candidate))
  {
    ++candidate;
  }
  return candidate;
}

} // namespace

void familyCrossFileDeclarationConflicts(ValidateContext& ctx)
{
  if (!ctx.settings.strictModeEnabled || ctx.stores.layout == nullptr)
  {
    return;
  }

  const auto identity = ctx.stores.layout->overlayForUri(ctx.prep->snapshot.uri);
  if (!identity)
  {
    return;
  }

  auto byKey = loadConflictIndex(ctx);

  const auto currentKey = identity->key;
  const auto scopes = declarationConflictScopes(byKey, currentKey);
  const auto usedNumbers = usedDeclarationNumbers(byKey);
  const auto lineOffsets = computeLineOffsets(ctx.prep->snapshot.text);
  std::unordered_set<std::string> seenConflictKeys;

  for (const auto& scope : scopes)
  {
    ctx.pollCancel();
    const auto conflicts = findConflicts(byKey, scope.files);
    for (const auto& conflict : conflicts)
    {
      if (conflict.filePath != currentKey || conflict.conflictsWith.filePath == currentKey)
      {
        continue;
      }
      const auto conflictKey = std::to_string(conflict.number) + "::"
        + conflict.conflictsWith.filePath + "::" + conflict.varName + "::"
        + conflict.sectionName + "::" + scope.root;
      if (!seenConflictKeys.insert(conflictKey).second)
      {
        continue;
      }

      auto lineText = conflict.lineText;
      if (lineText.empty() && conflict.line >= 0
          && conflict.line < static_cast<int>(ctx.prep->lines.size()))
      {
        lineText = std::string(ctx.prep->lines[static_cast<std::size_t>(conflict.line)]);
      }

      const auto otherBasename = displayBasename(byKey, conflict.conflictsWith.filePath);
      const auto chain = findDependencyChain(
        byKey, currentKey, conflict.conflictsWith.filePath, scope.fileSet);
      const auto chainDescription = formatChainDescription(byKey, chain, otherBasename);
      const auto scopePrefix = scopes.size() > 1
        ? " [scope: " + displayBasename(byKey, scope.root) + "]"
        : std::string{};
      const auto sectionPrefix = conflict.sectionName.empty()
        ? std::string{}
        : " [section: " + conflict.sectionName + "]";
      const auto nextAvailable = nextGlobalDeclarationNumber(conflict.number, usedNumbers);
      const auto inner = "Variable " + conflict.varName + " shares declaration number "
        + std::to_string(conflict.number) + " with " + conflict.conflictsWith.varName
        + " in " + otherBasename + "." + chainDescription + scopePrefix + sectionPrefix
        + " Next available: " + std::to_string(nextAvailable) + ".";

      const auto lineIndex = conflict.line < 0 ? 0 : static_cast<std::size_t>(conflict.line);
      const auto offset = lineIndex < lineOffsets.size()
        ? lineOffsets[lineIndex]
        : ctx.prep->snapshot.text.size();

      lsp::Diagnostic diagnostic;
      diagnostic.range = {
        .start = positionAt(ctx.prep->snapshot.text, lineOffsets, offset + 1),
        .end = positionAt(ctx.prep->snapshot.text, lineOffsets, offset + lineText.size()),
      };
      diagnostic.message = lineText + " has an error: " + inner;
      diagnostic.severity = lsp::DiagnosticSeverity::Information;
      diagnostic.source = "ADT-Syntax";

      std::vector<lsp::DiagnosticRelatedInformation> related;
      related.push_back(lsp::DiagnosticRelatedInformation{
        .location = {
          .uri = uriForKey(byKey, conflict.conflictsWith.filePath),
          .range = indexLineRange(conflict.conflictsWith.line, conflict.conflictsWith.lineText),
        },
        .message = conflict.conflictsWith.varName + " = " + std::to_string(conflict.number)
          + " in " + otherBasename,
      });
      if (chain)
      {
        for (const auto& step : *chain)
        {
          related.push_back(lsp::DiagnosticRelatedInformation{
            .location = {
              .uri = uriForKey(byKey, step.linkFile),
              .range = indexLineRange(step.line, step.lineText),
            },
            .message = linkLabel(step.type) + " in " + displayBasename(byKey, step.linkFile) + " line "
              + std::to_string(step.line + 1) + " → " + displayBasename(byKey, step.toFile),
          });
        }
      }
      diagnostic.relatedInformation = std::move(related);
      ctx.append(std::move(diagnostic));
    }
  }
}

} // namespace vertexadt
