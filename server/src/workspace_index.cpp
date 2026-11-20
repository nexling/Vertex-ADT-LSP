#include "workspace_index.h"

#include <algorithm>

#include "text_util.h"

#include <cctype>
#include <regex>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

const std::regex& indexAssignRe()
{
  static const std::regex re{R"re(\b[A-Za-z][A-Za-z0-9_-]*\b(?=\s*=))re"};
  return re;
}

const std::regex& indexAssignLettersRe()
{
  static const std::regex re{R"re(\b[A-Za-z]*\b(?!$)(?=\s*=))re"};
  return re;
}

const std::regex& localSlotsRe()
{
  static const std::regex re{R"re(^@local_slots\s+(.+)$)re", std::regex::icase};
  return re;
}

struct RequireMarker
{
  std::string_view marker;
  std::string_view directive;
};

constexpr RequireMarker kRequireMarkers[] = {
  {"@require_common_indexes", "common_indexes"},
  {"@require_common_dbases", "common_dbases"},
  {"@require_common_elements", "common_elements"},
  {"@require_common_files", "common_files"},
  {"@require_common_points", "common_points"},
};

bool isSectionKeyword(std::string_view token)
{
  const auto lower = toLowerAscii(token);
  return lower == "section" || lower == "sectionoffile";
}

struct ParsedPath
{
  std::string filePath;
  std::string targetSection;
};

ParsedPath parseIncludeCallPathAndSection(std::string_view rawPath, std::string_view remainder)
{
  auto pathPart = replaceBackslashes(stripPathQuotes(rawPath));
  pathPart = std::string(trim(pathPart));
  std::string targetSection;
  static const std::regex pathSection{R"re(^(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$)re"};
  std::smatch match;
  if (std::regex_match(pathPart, match, pathSection))
  {
    const auto token = match[2].str();
    if (!isSectionKeyword(token))
    {
      targetSection = token;
      pathPart = std::string(trim(match[1].str()));
    }
    else
    {
      static const std::regex stripSection{R"re(\s+SECTION(?:OFFILE)?$)re", std::regex::icase};
      pathPart = std::regex_replace(pathPart, stripSection, "");
      pathPart = std::string(trim(pathPart));
    }
  }
  const auto rem = trim(remainder);
  if (!rem.empty())
  {
    static const std::regex remRe{R"re(^([A-Za-z_][A-Za-z0-9_]*)\b)re"};
    std::cmatch remMatch;
    if (std::regex_search(rem.data(), rem.data() + rem.size(), remMatch, remRe)
        && !isSectionKeyword(remMatch[1].str()))
    {
      targetSection = remMatch[1].str();
    }
  }
  return ParsedPath{.filePath = std::move(pathPart), .targetSection = std::move(targetSection)};
}

std::optional<std::string> assignmentName(std::string_view fragment)
{
  std::cmatch match;
  const char* begin = fragment.data();
  const char* end = begin + fragment.size();
  if (std::regex_search(begin, end, match, indexAssignRe()))
  {
    return match[0].str();
  }
  if (std::regex_search(begin, end, match, indexAssignLettersRe()) && !match[0].str().empty())
  {
    return match[0].str();
  }
  return std::nullopt;
}

std::optional<int> declarationNumber(std::string_view fragment)
{
  const auto eq = fragment.find('=');
  if (eq == std::string_view::npos)
  {
    return std::nullopt;
  }
  auto value = std::string(trim(fragment.substr(eq + 1)));
  if (value.empty())
  {
    return std::nullopt;
  }
  std::size_t idx = 0;
  try
  {
    const auto number = std::stoi(value, &idx);
    if (idx == 0)
    {
      return std::nullopt;
    }
    while (idx < value.size() && std::isspace(static_cast<unsigned char>(value[idx])))
    {
      ++idx;
    }
    if (idx != value.size())
    {
      return std::nullopt;
    }
    return number;
  }
  catch (...)
  {
    return std::nullopt;
  }
}

std::optional<std::string> hoverTypeOnLine(std::string_view line)
{
  const auto comment = captureHoverComment(line);
  if (!comment)
  {
    return std::nullopt;
  }
  const auto open = comment->find('[');
  const auto close = comment->find(']', open);
  if (open == std::string::npos || close == std::string::npos || close <= open)
  {
    return std::nullopt;
  }
  return toLowerAscii(trim(std::string_view(*comment).substr(open + 1, close - open - 1)));
}

} // namespace

auto parseIndexEntry(
  std::string_view text,
  OverlayIdentity identity,
  const LibraryLayout& layout) -> IndexEntry
{
  IndexEntry entry;
  entry.identity = std::move(identity);
  const auto lines = splitLines(text);
  const auto sections = parseFileSections(lines);
  entry.expectedCallParamCount = expectedCallParamCountFromText(text, std::nullopt);

  struct TypeWrite
  {
    std::string name;
    std::string type;
    int line = 0;
  };
  std::vector<TypeWrite> types;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    if (auto type = hoverTypeOnLine(lines[static_cast<std::size_t>(i)]))
    {
      static const std::regex nameRe{R"re(\b([A-Za-z][A-Za-z0-9_-]*)\s*=)re"};
      std::cmatch match;
      const auto line = lines[static_cast<std::size_t>(i)];
      if (std::regex_search(line.data(), line.data() + line.size(), match, nameRe))
      {
        types.push_back(TypeWrite{.name = match[1].str(), .type = *type, .line = i});
      }
    }
  }

  std::vector<std::pair<int, int>> blocks;
  int open = -1;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto trimmed = trim(lines[static_cast<std::size_t>(i)]);
    if (trimmed.starts_with("@declarationnumbercheckstart"))
    {
      open = i;
    }
    else if (trimmed.starts_with("@declarationnumbercheckend") && open != -1)
    {
      blocks.emplace_back(open, i);
      open = -1;
    }
  }

  std::unordered_set<std::string> seenVariables;
  for (const auto& [start, end] : blocks)
  {
    for (int i = start + 1; i < end; ++i)
    {
      const auto sectionName = sectionNameForLine(i, sections);
      if (isVerificationIgnoredSection(sectionName))
      {
        continue;
      }
      const auto line = lines[static_cast<std::size_t>(i)];
      std::size_t fragStart = 0;
      while (fragStart <= line.size())
      {
        const auto semi = line.find(';', fragStart);
        const auto fragment = semi == std::string_view::npos
          ? line.substr(fragStart)
          : line.substr(fragStart, semi - fragStart);
        fragStart = semi == std::string_view::npos ? line.size() + 1 : semi + 1;
        if (isBangComment(fragment))
        {
          continue;
        }
        const auto name = assignmentName(fragment);
        const auto key = sectionName + "::" + (name ? *name : std::string{});
        if (!name || !seenVariables.insert(key).second)
        {
          continue;
        }
        const auto number = declarationNumber(fragment);
        if (!number || *number <= 0)
        {
          continue;
        }
        std::optional<std::string> variableType = hoverTypeOnLine(line);
        if (!variableType)
        {
          for (const auto& write : types)
          {
            if (write.name == *name && write.line <= i)
            {
              variableType = write.type;
            }
          }
        }
        entry.declarations.push_back(Declaration{
          .varName = *name,
          .number = *number,
          .line = i,
          .lineText = std::string(line),
          .sectionName = sectionName,
          .variableType = std::move(variableType),
        });
      }
    }
  }

  std::unordered_set<std::string> seenIncludes;
  std::unordered_set<std::string> seenCalls;
  static const std::regex callQuoted{R"re(call\s*\(\s*["']([^"']+)["'])re", std::regex::icase};
  static const std::regex callDollar{R"re(call\s*\(\s*(\$[^"',)]+))re", std::regex::icase};
  static const std::regex callSpace{R"re(\bcall\s+([^\s,(]+))re", std::regex::icase};

  auto addLink = [&](std::vector<FileLink>& links,
                     std::unordered_set<std::string>& seen,
                     const ParsedPath& parsed,
                     int line,
                     std::string_view rawLine,
                     std::string_view sourceSection) {
    const auto resolved = layout.resolveVertexFilePath(parsed.filePath);
    if (!resolved)
    {
      return;
    }
    const auto relative = layout.libraryRelativeFromAbs(*resolved);
    if (!relative)
    {
      return;
    }
    const auto key = toLowerAscii(*relative) + "::" + parsed.targetSection;
    if (!seen.insert(key).second)
    {
      return;
    }
    links.push_back(FileLink{
      .target = toLowerAscii(*relative),
      .targetSection = parsed.targetSection,
      .sourceSection = std::string(sourceSection),
      .line = line,
      .lineText = std::string(rawLine),
    });
  };

  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto raw = lines[static_cast<std::size_t>(i)];
    const auto line = trim(raw);
    if (line.starts_with("!"))
    {
      continue;
    }
    static const std::regex commonIndexesRe{R"re(^common_indexes\s+1\b)re", std::regex::icase};
    static const std::regex commonPointsRe{R"re(^common_points\s+1\b)re", std::regex::icase};
    std::cmatch commonMatch;
    if (std::regex_search(line.data(), line.data() + line.size(), commonMatch, commonIndexesRe))
    {
      entry.commonIndexes = true;
    }
    if (std::regex_search(line.data(), line.data() + line.size(), commonMatch, commonPointsRe))
    {
      entry.commonPoints = true;
    }
    const auto sourceSection = sectionNameForLine(i, sections);
    if (lineInVerificationIgnoredSection(i, sections))
    {
      continue;
    }
    if (line.starts_with("# include"))
    {
      const auto after = trim(line.substr(std::string_view("# include").size()));
      static const std::regex pathRe{R"re(^((?:"[^"]+"|'[^']+'|[^\s#;]+))\s*(.*)$)re"};
      std::cmatch pathMatch;
      if (std::regex_match(after.data(), after.data() + after.size(), pathMatch, pathRe))
      {
        addLink(
          entry.includeLinks,
          seenIncludes,
          parseIncludeCallPathAndSection(pathMatch[1].str(), pathMatch[2].str()),
          i,
          raw,
          sourceSection);
      }
      continue;
    }

    auto scanCalls = [&](const std::regex& re) {
      for (std::cregex_iterator it(line.data(), line.data() + line.size(), re), last; it != last; ++it)
      {
        addLink(
          entry.callLinks,
          seenCalls,
          parseIncludeCallPathAndSection((*it)[1].str(), {}),
          i,
          raw,
          sourceSection);
      }
    };
    scanCalls(callQuoted);
    scanCalls(callDollar);
    scanCalls(callSpace);
  }

  std::unordered_set<std::string> seenRequires;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    if (lineInVerificationIgnoredSection(i, sections))
    {
      continue;
    }
    const auto trimmed = trim(lines[static_cast<std::size_t>(i)]);
    if (trimmed.starts_with("!"))
    {
      continue;
    }
    const auto sectionName = sectionNameForLine(i, sections);
    for (const auto& marker : kRequireMarkers)
    {
      if (trimmed.size() >= marker.marker.size()
          && trimmed.substr(0, marker.marker.size()) == marker.marker)
      {
        const auto key = std::string(marker.directive) + "::" + sectionName;
        if (seenRequires.insert(key).second)
        {
          entry.calleeCommonRequirements.push_back(CalleeCommonRequirement{
            .directive = std::string(marker.directive),
            .sectionName = sectionName,
          });
        }
      }
    }
  }

  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    if (lineInVerificationIgnoredSection(i, sections))
    {
      continue;
    }
    const auto trimmed = std::string(trim(lines[static_cast<std::size_t>(i)]));
    if (trimmed.starts_with("!"))
    {
      continue;
    }
    std::smatch match;
    if (!std::regex_match(trimmed, match, localSlotsRe()))
    {
      continue;
    }
    std::string rest = match[1].str();
    std::size_t start = 0;
    while (start <= rest.size())
    {
      const auto comma = rest.find(',', start);
      const auto part = comma == std::string::npos
        ? std::string_view(rest).substr(start)
        : std::string_view(rest).substr(start, comma - start);
      start = comma == std::string::npos ? rest.size() + 1 : comma + 1;
      const auto name = std::string(trim(part));
      if (isLocalSlotName(name))
      {
        entry.localSlotsManifest.push_back(name);
      }
    }
    break;
  }

  return entry;
}

namespace {

// "target|section|line|kind" per link, plus each target once with the common
// flags, so a flag flip also counts as a change for every call target.
auto linkSignature(const IndexEntry& entry) -> std::unordered_set<std::string>
{
  std::unordered_set<std::string> signature;
  const auto flags = std::string(entry.commonIndexes ? "1" : "0") + (entry.commonPoints ? "1" : "0");
  for (const auto& link : entry.includeLinks)
  {
    signature.insert(link.target + "|" + link.targetSection + "|" + std::to_string(link.line) + "|i");
  }
  for (const auto& link : entry.callLinks)
  {
    signature.insert(
      link.target + "|" + link.targetSection + "|" + std::to_string(link.line) + "|c" + flags);
  }
  return signature;
}

auto changedTargets(const IndexEntry* before, const IndexEntry* after) -> std::vector<std::string>
{
  const auto a = before ? linkSignature(*before) : std::unordered_set<std::string>{};
  const auto b = after ? linkSignature(*after) : std::unordered_set<std::string>{};
  std::unordered_set<std::string> targets;
  auto collect = [&](const std::unordered_set<std::string>& from, const std::unordered_set<std::string>& other) {
    for (const auto& item : from)
    {
      if (!other.contains(item))
      {
        targets.insert(item.substr(0, item.find('|')));
      }
    }
  };
  collect(a, b);
  collect(b, a);
  return {targets.begin(), targets.end()};
}

} // namespace

auto WorkspaceIndex::upsert(IndexEntry entry) -> std::vector<std::string>
{
  const auto lock = std::lock_guard(mutex_);
  // Copy the key first: C++17 sequences the RHS of `m[k] = f(move(entry))`
  // before operator[], which would insert under a moved-from empty key.
  auto key = entry.identity.key;
  auto next = std::make_shared<const IndexEntry>(std::move(entry));
  const auto it = entries_.find(key);
  auto targets = changedTargets(it == entries_.end() ? nullptr : it->second.get(), next.get());
  entries_[std::move(key)] = std::move(next);
  reverseValid_ = false;
  return targets;
}

auto WorkspaceIndex::removeKey(std::string_view key) -> std::vector<std::string>
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = entries_.find(std::string(key));
  if (it == entries_.end())
  {
    return {};
  }
  auto targets = changedTargets(it->second.get(), nullptr);
  entries_.erase(it);
  reverseValid_ = false;
  return targets;
}

void WorkspaceIndex::clear()
{
  const auto lock = std::lock_guard(mutex_);
  entries_.clear();
  reverseValid_ = false;
}

void WorkspaceIndex::refreshCalleeFlags()
{
  const auto lock = std::lock_guard(mutex_);
  std::unordered_set<std::string> callees;
  for (const auto& [_, entry] : entries_)
  {
    for (const auto& link : entry->callLinks)
    {
      callees.insert(link.target);
    }
  }
  for (auto& [key, entry] : entries_)
  {
    auto copy = *entry;
    copy.isCallCallee = callees.contains(key);
    entry = std::make_shared<const IndexEntry>(std::move(copy));
  }
  reverseValid_ = false;
}

auto WorkspaceIndex::entry(std::string_view key) const -> std::shared_ptr<const IndexEntry>
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = entries_.find(std::string(key));
  if (it == entries_.end())
  {
    return {};
  }
  return it->second;
}

auto WorkspaceIndex::entryByAbsPath(const std::filesystem::path& absPath) const
  -> std::shared_ptr<const IndexEntry>
{
  const auto lock = std::lock_guard(mutex_);
  const auto normalized = normalizeFsPath(absPath.string());
  for (const auto& [_, entry] : entries_)
  {
    if (normalizeFsPath(entry->identity.absPath.string()) == normalized)
    {
      return entry;
    }
  }
  return {};
}

auto WorkspaceIndex::snapshot() const -> std::vector<std::shared_ptr<const IndexEntry>>
{
  const auto lock = std::lock_guard(mutex_);
  std::vector<std::shared_ptr<const IndexEntry>> out;
  out.reserve(entries_.size());
  for (const auto& [_, entry] : entries_)
  {
    out.push_back(entry);
  }
  return out;
}

auto WorkspaceIndex::incomingLinks(std::string_view targetKey) const -> std::vector<IncomingLink>
{
  const auto lock = std::lock_guard(mutex_);
  if (!reverseValid_)
  {
    reverse_.clear();
    std::vector<std::string> keys;
    keys.reserve(entries_.size());
    for (const auto& [key, _] : entries_)
    {
      keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    for (const auto& key : keys)
    {
      const auto& entry = entries_.at(key);
      for (const auto& link : entry->includeLinks)
      {
        reverse_[link.target].push_back(IncomingLink{.parent = entry, .link = link, .isCall = false});
      }
      for (const auto& link : entry->callLinks)
      {
        reverse_[link.target].push_back(IncomingLink{.parent = entry, .link = link, .isCall = true});
      }
    }
    reverseValid_ = true;
  }
  const auto it = reverse_.find(std::string(targetKey));
  if (it == reverse_.end())
  {
    return {};
  }
  return it->second;
}

} // namespace vertexadt
