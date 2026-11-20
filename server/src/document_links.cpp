#include "document_links.h"

#include "definition.h"
#include "text_util.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

using navigation::splitJsLines;
using navigation::utf16Offset;

bool existsAny(const std::filesystem::path& path)
{
  std::error_code ec;
  return std::filesystem::exists(path, ec);
}

bool isRegularFile(const std::filesystem::path& path)
{
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec);
}

// Node path.win32.isAbsolute: drive, UNC, or a leading slash.
bool isNodeAbsolute(std::string_view text)
{
  if (text.empty())
  {
    return false;
  }
  if (text[0] == '/' || text[0] == '\\')
  {
    return true;
  }
  return text.size() >= 3
    && std::isalpha(static_cast<unsigned char>(text[0])) != 0
    && text[1] == ':'
    && (text[2] == '/' || text[2] == '\\');
}

auto nativeSeparators(std::string text) -> std::string
{
#ifdef _WIN32
  for (char& ch : text)
  {
    if (ch == '/')
    {
      ch = '\\';
    }
  }
#endif
  return text;
}

auto tooltipFsPath(const lsp::Uri& uri) -> std::string
{
  return nativeSeparators(uri.fsPath());
}

auto lastUriSegment(const lsp::Uri& uri) -> std::optional<std::string>
{
  auto text = uri.toString();
  while (!text.empty() && (text.back() == '/' || text.back() == '\\'))
  {
    text.pop_back();
  }
  const auto slash = text.find_last_of('/');
  if (slash == std::string::npos)
  {
    return std::nullopt;
  }
  return lsp::Uri::decode(text.substr(slash + 1));
}

// RPC getCustomFolderName: first workspace URI whose string includes('custom')
// (case-sensitive), last `/` segment. Not LibraryLayout::customFolderName().
auto includePathCustomFolderName(const LibraryLayout& layout) -> std::string
{
  for (const auto& folder : layout.folders())
  {
    const auto text = folder.uri.toString();
    if (text.find("custom") == std::string::npos)
    {
      continue;
    }
    if (auto name = lastUriSegment(folder.uri))
    {
      return *name;
    }
  }
  return "custom";
}

auto findFileRecursive(
  const std::filesystem::path& workspaceRoot,
  std::string filePath,
  std::string customName) -> std::optional<std::filesystem::path>
{
  filePath = replaceBackslashes(filePath);
  if (filePath.find('/') == std::string::npos)
  {
    return std::nullopt;
  }
  const auto slash = filePath.find_last_of('/');
  const auto relativeFolder = filePath.substr(0, slash);
  const auto base = filePath.substr(slash + 1);
  auto systemName = std::string("system");
  const auto rootText = replaceBackslashes(workspaceRoot.string());
  if (rootText.find("custom") != std::string::npos)
  {
    customName.clear();
  }
  if (rootText.find("system") != std::string::npos)
  {
    systemName.clear();
  }
  const auto customPath = workspaceRoot / customName / relativeFolder / base;
  if (existsAny(customPath))
  {
    return customPath.lexically_normal();
  }
  const auto systemPath = workspaceRoot / systemName / relativeFolder / base;
  if (existsAny(systemPath))
  {
    return systemPath.lexically_normal();
  }
  return std::nullopt;
}

// In-process custom/resolveIncludePath. Must be a file. Unmapped $ → null.
auto resolveIncludePath(
  std::string_view inputPath,
  const LibraryLayout& layout) -> std::optional<std::filesystem::path>
{
  if (layout.folders().empty() || layout.workspaceRoot().empty())
  {
    return std::nullopt;
  }
  auto filePath = replaceBackslashes(stripPathQuotes(inputPath));
  filePath = std::string(trim(filePath));
  if (filePath.empty())
  {
    return std::nullopt;
  }
  if (!filePath.empty() && filePath.front() == '$')
  {
    const auto slash = filePath.find('/');
    const auto first = slash == std::string::npos ? filePath : filePath.substr(0, slash);
    const auto converted = layout.convertVertexPath(first);
    if (!converted)
    {
      return std::nullopt;
    }
    filePath = slash == std::string::npos ? *converted : *converted + filePath.substr(slash);
  }

  const auto& workspaceRoot = layout.workspaceRoot();
  const auto customName = includePathCustomFolderName(layout);
  std::vector<std::filesystem::path> candidates;
  if (isNodeAbsolute(filePath))
  {
    candidates.push_back(std::filesystem::path(filePath).lexically_normal());
  }
  candidates.push_back((workspaceRoot / filePath).lexically_normal());
  candidates.push_back((workspaceRoot / customName / filePath).lexically_normal());
  candidates.push_back((workspaceRoot / "system" / filePath).lexically_normal());
  if (const auto recursive = findFileRecursive(workspaceRoot, filePath, customName))
  {
    candidates.push_back(*recursive);
  }
  for (const auto& candidate : candidates)
  {
    if (isRegularFile(candidate))
    {
      return candidate;
    }
  }
  return std::nullopt;
}

auto fileUriFromNormalized(const std::filesystem::path& path) -> lsp::Uri
{
  if (path.empty())
  {
    return {};
  }
  if (path.is_absolute() || isNodeAbsolute(path.string()))
  {
    return uriFromPath(path);
  }
  auto generic = path.lexically_normal().generic_u8string();
  generic.insert(generic.begin(), u8'/');
  auto uri = lsp::Uri();
  uri.setScheme(lsp::Uri::FileScheme);
  uri.setAuthority({});
  uri.setPath(std::string_view(
    reinterpret_cast<const char*>(generic.data()), generic.size()));
  return uri;
}

// resolveVertexPath: strip both quote kinds; ${ / % → null; $ overlay then
// absolute / document-dir / each workspace folder existsSync (directories win).
auto resolveVertexPath(
  std::string_view rawPath,
  const Snapshot& snapshot,
  const LibraryLayout& layout) -> std::optional<lsp::Uri>
{
  if (rawPath.empty())
  {
    return std::nullopt;
  }
  const auto cleaned = stripPathQuotes(rawPath);
  if (cleaned.empty() || cleaned.find("${") != std::string::npos
      || cleaned.find('%') != std::string::npos)
  {
    return std::nullopt;
  }
  if (!cleaned.empty() && cleaned.front() == '$')
  {
    if (const auto overlay = resolveIncludePath(cleaned, layout))
    {
      return uriFromPath(*overlay);
    }
  }

  const auto normalized = nativeSeparators(replaceBackslashes(cleaned));
  std::vector<std::filesystem::path> candidates;
  if (isNodeAbsolute(normalized))
  {
    candidates.push_back(std::filesystem::path(normalized).lexically_normal());
  }
  const auto documentDir = pathFromUri(snapshot.uri).parent_path();
  if (!documentDir.empty())
  {
    candidates.push_back((documentDir / normalized).lexically_normal());
  }
  for (const auto& folder : layout.folders())
  {
    candidates.push_back((pathFromUri(folder.uri) / normalized).lexically_normal());
  }
  for (const auto& candidate : candidates)
  {
    if (existsAny(candidate))
    {
      return uriFromPath(candidate);
    }
  }
  return std::nullopt;
}

auto firstUserWorkspaceFolder(const LibraryLayout& layout)
  -> std::optional<std::filesystem::path>
{
  for (const auto& folder : layout.folders())
  {
    const auto name = toLowerAscii(trim(folder.name));
    const auto base = toLowerAscii(trim(pathFromUri(folder.uri).filename().string()));
    if (name.find("user") != std::string::npos || base.find("user") != std::string::npos)
    {
      return pathFromUri(folder.uri);
    }
  }
  return std::nullopt;
}

auto resolveDebugCommandPath(
  std::string_view rawPath,
  const std::filesystem::path& userRoot) -> std::optional<lsp::Uri>
{
  if (rawPath.empty())
  {
    return std::nullopt;
  }
  const auto cleaned = stripPathQuotes(rawPath);
  if (cleaned.empty() || cleaned.find("${") != std::string::npos
      || cleaned.find('%') != std::string::npos || !cleaned.starts_with("./"))
  {
    return std::nullopt;
  }
  auto relative = std::string(trim(std::string_view(cleaned).substr(2)));
  if (relative.empty())
  {
    return std::nullopt;
  }
  relative = nativeSeparators(replaceBackslashes(relative));
  if (isNodeAbsolute(relative))
  {
    return std::nullopt;
  }
  const auto target = (userRoot / relative).lexically_normal();
  const auto relativeToRoot = target.lexically_relative(userRoot.lexically_normal());
  const auto generic = relativeToRoot.generic_string();
  if (generic.empty() || generic == "." || generic.starts_with("..")
      || relativeToRoot.is_absolute())
  {
    return std::nullopt;
  }
  if (!existsAny(target))
  {
    return std::nullopt;
  }
  return uriFromPath(target);
}

auto readDiskUtf8(const std::filesystem::path& path) -> std::optional<std::string>
{
  std::ifstream in(path, std::ios::binary);
  if (!in)
  {
    return std::nullopt;
  }
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

auto resolveSectionTargetUri(
  lsp::Uri targetUri,
  std::string_view rawSectionName,
  std::unordered_map<std::string, std::optional<int>>& cache,
  bool* found = nullptr) -> lsp::Uri
{
  if (!targetUri.isValid() || rawSectionName.empty())
  {
    return targetUri;
  }
  const auto sectionName = std::string(trim(rawSectionName));
  if (sectionName.empty())
  {
    return targetUri;
  }
  const auto cacheKey = std::string(targetUri.fsPath()) + "::" + toLowerAscii(sectionName);
  auto it = cache.find(cacheKey);
  if (it == cache.end())
  {
    std::optional<int> sectionLine;
    const auto marker = toLowerAscii("#" + sectionName + "#");
    if (const auto content = readDiskUtf8(std::filesystem::path(targetUri.fsPath())))
    {
      const auto lines = splitJsLines(*content);
      for (int i = 0; i < static_cast<int>(lines.size()); ++i)
      {
        if (toLowerAscii(lines[static_cast<std::size_t>(i)]).find(marker) != std::string::npos)
        {
          sectionLine = i;
          break;
        }
      }
    }
    it = cache.emplace(cacheKey, sectionLine).first;
  }
  if (it->second && *it->second >= 0)
  {
    targetUri.setFragment("L" + std::to_string(*it->second + 1));
    if (found != nullptr)
    {
      *found = true;
    }
  }
  return targetUri;
}

auto makeLink(int line, int start, int end, const lsp::Uri& target, std::string_view tooltip)
  -> lsp::DocumentLink
{
  lsp::DocumentLink link;
  link.range = lsp::Range{
    .start = {
      .line = static_cast<lsp::Uint>(std::max(0, line)),
      .character = static_cast<lsp::Uint>(std::max(0, start)),
    },
    .end = {
      .line = static_cast<lsp::Uint>(std::max(0, line)),
      .character = static_cast<lsp::Uint>(std::max(0, end)),
    },
  };
  link.target = target;
  link.tooltip = std::string(tooltip);
  return link;
}

const std::regex& includedFromRe()
{
  static const std::regex re{
    R"re(##included_from\s+((?:"[^"]+"|'[^']+'|[^\s#;]+)))re"};
  return re;
}

const std::regex& hashIncludeRe()
{
  static const std::regex re{
    R"re(#\s*include\s+((?:"[^"]+"|'[^']+'|[^\s#;]+)))re"};
  return re;
}

const std::regex& assignmentCallRe()
{
  static const std::regex re{
    R"re(\b\w+\$?\s*=\s*call\(\s*((?:"[^"]+"|'[^']+'|[^,\)\s]+)))re"};
  return re;
}

const std::regex& callParenRe()
{
  static const std::regex re{
    R"re(\bcall\(\s*((?:"[^"]+"|'[^']+'|[^,\)\s]+)))re"};
  return re;
}

const std::regex& callSpaceRe()
{
  static const std::regex re{
    R"re(\bcall\s+((?:"[^"]+"|'[^']+'|[^\s,]+)))re"};
  return re;
}

const std::regex& startOfLineIncludeRe()
{
  static const std::regex re{R"re(^#\s*include\b)re", std::regex::icase};
  return re;
}

const std::regex& debugHeaderRe()
{
  static const std::regex re{
    R"re(START OF DEBUG(?:\s+in\s+SECTION\s+#([^#]+)#)?\s+in\s+FILE\s+"([^"]+)")re",
    std::regex::icase};
  return re;
}

const std::regex& debugCommandRe()
{
  static const std::regex re{
    R"re(^\s*debug\b\s+((?:"\./[^"]+"|'\./[^']+'|\./[^\s#;]+)))re",
    std::regex::icase};
  return re;
}

struct PathRange
{
  int lineIndex = 0;
  std::string rawPath;
  std::size_t rawStart = 0;
  // rawPath unquoted, with a quoted named-section suffix (or SECTION keyword) cut.
  std::string cutPath;
  // Suffix inside the quotes; used only when rawPath itself does not resolve.
  std::string innerSection;
  // Suffix after the path (`# include` at line start, `call path SECTION`). Wins.
  std::string remainderSection;
};

void collectPattern(
  std::string_view lineText,
  int lineIndex,
  const std::regex& pattern,
  bool remainderIsSuffix,
  std::vector<PathRange>& out)
{
  std::cregex_iterator it(lineText.data(), lineText.data() + lineText.size(), pattern);
  const std::cregex_iterator last;
  for (; it != last; ++it)
  {
    const auto& match = *it;
    PathRange item;
    item.lineIndex = lineIndex;
    item.rawPath = match[1].str();
    const auto full = match[0].str();
    const auto inner = full.rfind(item.rawPath);
    item.rawStart = static_cast<std::size_t>(match.position())
      + (inner == std::string::npos ? 0 : inner);
    const auto after = lineText.substr(
      std::min(item.rawStart + item.rawPath.size(), lineText.size()));
    const auto afterTrimmed = trim(after);
    std::size_t lead = 0;
    while (lead < lineText.size()
           && (lineText[lead] == ' ' || lineText[lead] == '\t'))
    {
      ++lead;
    }
    std::cmatch leadInclude;
    const bool includeAtLineStart = !afterTrimmed.empty()
      && std::regex_search(
        lineText.data() + lead,
        lineText.data() + lineText.size(),
        leadInclude,
        startOfLineIncludeRe())
      && leadInclude.position() == 0;
    const auto quoted = splitIncludeCallTarget(item.rawPath, {});
    item.cutPath = quoted.filePath;
    item.innerSection = quoted.targetSection;
    if (includeAtLineStart)
    {
      // Unchanged from Node: any leading ident, so `SECTION` still names a
      // `#SECTION#` marker here.
      std::size_t end = 0;
      while (end < afterTrimmed.size()
             && (std::isalnum(static_cast<unsigned char>(afterTrimmed[end])) != 0
                 || afterTrimmed[end] == '_'))
      {
        ++end;
      }
      if (end > 0 && std::isdigit(static_cast<unsigned char>(afterTrimmed.front())) == 0)
      {
        item.remainderSection = std::string(afterTrimmed.substr(0, end));
      }
    }
    else if (remainderIsSuffix)
    {
      // Any path stands in for rawPath: only the remainder's suffix is wanted.
      item.remainderSection = splitIncludeCallTarget("x", afterTrimmed).targetSection;
    }
    out.push_back(std::move(item));
  }
}

auto buildPathDocumentLinks(
  const Snapshot& snapshot,
  const LibraryLayout& layout,
  const std::vector<std::string_view>& lines,
  std::unordered_map<std::string, std::optional<int>>& sectionCache)
  -> std::vector<lsp::DocumentLink>
{
  std::vector<PathRange> pathRanges;
  struct Pattern
  {
    const std::regex* re;
    bool remainderIsSuffix;
  };
  const Pattern patterns[] = {
    {&includedFromRe(), false},
    {&hashIncludeRe(), false},
    {&assignmentCallRe(), false},
    {&callParenRe(), false},
    {&callSpaceRe(), true},
  };
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    for (const auto& pattern : patterns)
    {
      collectPattern(
        lines[static_cast<std::size_t>(i)], i, *pattern.re, pattern.remainderIsSuffix, pathRanges);
    }
  }

  // The whole token first, so links that resolve today never change; then the
  // path with its quoted named-section suffix cut.
  struct Resolved
  {
    lsp::Uri uri;
    bool cut = false;
  };
  std::unordered_map<std::string, Resolved> resolvedByPath;
  for (const auto& item : pathRanges)
  {
    if (resolvedByPath.contains(item.rawPath))
    {
      continue;
    }
    if (const auto resolved = resolveVertexPath(item.rawPath, snapshot, layout))
    {
      resolvedByPath.emplace(item.rawPath, Resolved{.uri = *resolved});
    }
    else if (item.cutPath != stripPathQuotes(item.rawPath))
    {
      if (const auto cut = resolveVertexPath(item.cutPath, snapshot, layout))
      {
        resolvedByPath.emplace(item.rawPath, Resolved{.uri = *cut, .cut = true});
      }
    }
  }

  std::vector<lsp::DocumentLink> links;
  for (const auto& item : pathRanges)
  {
    const auto found = resolvedByPath.find(item.rawPath);
    if (found == resolvedByPath.end())
    {
      continue;
    }
    auto targetUri = found->second.uri;
    const auto sectionName = !item.remainderSection.empty()
      ? item.remainderSection
      : (found->second.cut ? item.innerSection : std::string{});
    bool sectionFound = false;
    if (!sectionName.empty())
    {
      targetUri = resolveSectionTargetUri(targetUri, sectionName, sectionCache, &sectionFound);
    }
    const auto line = lines[static_cast<std::size_t>(item.lineIndex)];
    const auto start = utf16Offset(line, item.rawStart);
    const auto end = utf16Offset(line, item.rawStart + item.rawPath.size());
    auto tooltip = "Open " + tooltipFsPath(found->second.uri);
    if (sectionFound)
    {
      tooltip += " #" + sectionName + "#";
    }
    links.push_back(makeLink(item.lineIndex, start, end, targetUri, tooltip));
  }
  return links;
}

auto buildDebugHeaderLinks(
  const Snapshot& snapshot,
  const LibraryLayout& layout,
  const std::vector<std::string_view>& lines,
  std::unordered_map<std::string, std::optional<int>>& sectionCache)
  -> std::vector<lsp::DocumentLink>
{
  if (lines.empty())
  {
    return {};
  }
  const auto firstLine = lines.front();
  std::cmatch match;
  if (!std::regex_search(
        firstLine.data(), firstLine.data() + firstLine.size(), match, debugHeaderRe()))
  {
    return {};
  }
  const auto sectionName = match[1].matched ? std::string(trim(match[1].str())) : std::string{};
  const auto rawPath = std::string(trim(match[2].str()));
  if (rawPath.empty())
  {
    return {};
  }
  auto targetUri = resolveVertexPath(rawPath, snapshot, layout);
  if (!targetUri && !rawPath.starts_with("$"))
  {
    const auto normalized = nativeSeparators(replaceBackslashes(rawPath));
    targetUri = fileUriFromNormalized(std::filesystem::path(normalized));
  }
  if (!targetUri)
  {
    return {};
  }
  if (!sectionName.empty())
  {
    *targetUri = resolveSectionTargetUri(*targetUri, sectionName, sectionCache);
  }
  const auto rawStart = firstLine.find(rawPath);
  if (rawStart == std::string_view::npos)
  {
    return {};
  }
  const auto start = utf16Offset(firstLine, rawStart);
  const auto end = utf16Offset(firstLine, rawStart + rawPath.size());
  const auto tooltip = "Open " + tooltipFsPath(*targetUri);
  return {makeLink(0, start, end, *targetUri, tooltip)};
}

auto buildDebugCommandLinks(
  const LibraryLayout& layout,
  const std::vector<std::string_view>& lines) -> std::vector<lsp::DocumentLink>
{
  const auto userRoot = firstUserWorkspaceFolder(layout);
  if (!userRoot)
  {
    return {};
  }
  std::vector<lsp::DocumentLink> links;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto lineText = lines[static_cast<std::size_t>(i)];
    std::cmatch match;
    if (!std::regex_search(
          lineText.data(), lineText.data() + lineText.size(), match, debugCommandRe()))
    {
      continue;
    }
    const auto rawPath = match[1].str();
    const auto targetUri = resolveDebugCommandPath(rawPath, *userRoot);
    if (!targetUri)
    {
      continue;
    }
    const auto rawStart = lineText.find(rawPath, static_cast<std::size_t>(match.position()));
    if (rawStart == std::string_view::npos)
    {
      continue;
    }
    const auto start = utf16Offset(lineText, rawStart);
    const auto end = utf16Offset(lineText, rawStart + rawPath.size());
    const auto tooltip = "Open " + tooltipFsPath(*targetUri);
    links.push_back(makeLink(i, start, end, *targetUri, tooltip));
  }
  return links;
}

} // namespace

auto documentLinks(const DocumentLinksRequest& request) -> lsp::Array<lsp::DocumentLink>
{
  const auto lines = splitJsLines(request.snapshot.text);
  std::unordered_map<std::string, std::optional<int>> sectionCache;
  auto pathLinks = buildPathDocumentLinks(
    request.snapshot, request.layout, lines, sectionCache);
  auto headerLinks = buildDebugHeaderLinks(
    request.snapshot, request.layout, lines, sectionCache);
  auto debugLinks = buildDebugCommandLinks(request.layout, lines);

  lsp::Array<lsp::DocumentLink> links;
  links.reserve(pathLinks.size() + headerLinks.size() + debugLinks.size());
  for (auto& link : pathLinks)
  {
    links.push_back(std::move(link));
  }
  for (auto& link : headerLinks)
  {
    links.push_back(std::move(link));
  }
  for (auto& link : debugLinks)
  {
    links.push_back(std::move(link));
  }
  return links;
}

} // namespace vertexadt
