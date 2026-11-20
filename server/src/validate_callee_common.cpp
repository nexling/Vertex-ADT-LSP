#include "validate.h"
#include "utf16_text.h"

#include <cctype>
#include <regex>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

struct RequirementSpec
{
  std::string_view marker;
  std::string_view directive;
};

constexpr RequirementSpec kRequirements[] = {
  {"@require_common_indexes", "common_indexes"},
  {"@require_common_dbases", "common_dbases"},
  {"@require_common_elements", "common_elements"},
  {"@require_common_files", "common_files"},
  {"@require_common_points", "common_points"},
};

const RequirementSpec* specForDirective(std::string_view directive)
{
  for (const auto& spec : kRequirements)
  {
    if (spec.directive == directive)
    {
      return &spec;
    }
  }
  return nullptr;
}

bool isWordChar(unsigned char ch)
{
  return std::isalnum(ch) != 0 || ch == '_';
}

bool isSectionKeyword(std::string_view token)
{
  const auto lower = toLowerAscii(token);
  return lower == "section" || lower == "sectionoffile";
}

bool looksLikeQuotedString(std::string_view text)
{
  const auto trimmed = trim(text);
  if (trimmed.size() < 2)
  {
    return false;
  }
  const auto first = trimmed.front();
  const auto last = trimmed.back();
  return (first == '"' && last == '"') || (first == '\'' && last == '\'');
}

bool looksLikeStringVariable(std::string_view text)
{
  const auto trimmed = trim(text);
  if (trimmed.empty() || trimmed.size() > 16 || trimmed.find('$') == std::string_view::npos)
  {
    return false;
  }
  const auto first = static_cast<unsigned char>(trimmed.front());
  if (!std::isalpha(first))
  {
    return false;
  }
  for (char ch : trimmed)
  {
    if (!isWordChar(static_cast<unsigned char>(ch)) && ch != '$' && ch != '(' && ch != ')')
    {
      return false;
    }
  }
  return true;
}

bool isValidCallPathParameter(std::string_view expr)
{
  const auto trimmed = trim(expr);
  if (trimmed.empty())
  {
    return false;
  }
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  while (start <= trimmed.size())
  {
    const auto amp = trimmed.find('&', start);
    const auto part = amp == std::string_view::npos
      ? trimmed.substr(start)
      : trimmed.substr(start, amp - start);
    parts.push_back(trim(part));
    if (amp == std::string_view::npos)
    {
      break;
    }
    start = amp + 1;
  }
  if (parts.empty())
  {
    return false;
  }
  for (std::size_t i = 0; i < parts.size(); ++i)
  {
    if (parts[i].empty())
    {
      continue;
    }
    if (!looksLikeQuotedString(parts[i]) && !looksLikeStringVariable(parts[i]))
    {
      return false;
    }
  }
  return true;
}

std::ptrdiff_t findMatchingParenIndex(std::string_view text, std::size_t openParenIndex)
{
  if (openParenIndex >= text.size() || text[openParenIndex] != '(')
  {
    return -1;
  }
  int parenCount = 1;
  bool inQuote = false;
  for (std::size_t i = openParenIndex + 1; i < text.size(); ++i)
  {
    const auto ch = text[i];
    if (inQuote)
    {
      if (ch == '"')
      {
        inQuote = false;
      }
      continue;
    }
    if (ch == '"')
    {
      inQuote = true;
      continue;
    }
    if (ch == '(')
    {
      ++parenCount;
    }
    else if (ch == ')')
    {
      --parenCount;
      if (parenCount == 0)
      {
        return static_cast<std::ptrdiff_t>(i);
      }
    }
  }
  return -1;
}

std::vector<std::string> parseCommaParams(std::string_view text)
{
  std::vector<std::string> params;
  std::string current;
  int parenDepth = 0;
  int bracketDepth = 0;
  bool inQuote = false;
  bool lastWasComma = false;
  for (char ch : text)
  {
    if (inQuote)
    {
      current.push_back(ch);
      if (ch == '"')
      {
        inQuote = false;
      }
      lastWasComma = false;
      continue;
    }
    if (ch == '"')
    {
      inQuote = true;
      current.push_back(ch);
      lastWasComma = false;
      continue;
    }
    if (ch == '(')
    {
      ++parenDepth;
      current.push_back(ch);
      lastWasComma = false;
      continue;
    }
    if (ch == ')')
    {
      --parenDepth;
      current.push_back(ch);
      lastWasComma = false;
      continue;
    }
    if (ch == '[')
    {
      ++bracketDepth;
      current.push_back(ch);
      lastWasComma = false;
      continue;
    }
    if (ch == ']')
    {
      if (bracketDepth > 0)
      {
        --bracketDepth;
      }
      current.push_back(ch);
      lastWasComma = false;
      continue;
    }
    if (ch == ',' && parenDepth == 0 && bracketDepth == 0)
    {
      params.push_back(std::string(trim(current)));
      current.clear();
      lastWasComma = true;
      continue;
    }
    current.push_back(ch);
    if (!std::isspace(static_cast<unsigned char>(ch)))
    {
      lastWasComma = false;
    }
  }
  const auto trailing = std::string(trim(current));
  if (!trailing.empty())
  {
    params.push_back(trailing);
  }
  else if (lastWasComma)
  {
    params.push_back({});
  }
  return params;
}

struct ParsedCallPath
{
  std::string filePath;
  std::string targetSection;
};

ParsedCallPath parseIncludeCallPathAndSection(std::string_view rawPath, std::string_view remainder)
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
  return ParsedCallPath{.filePath = std::move(pathPart), .targetSection = std::move(targetSection)};
}

struct CallSite
{
  int lineIndex = 0;
  std::string_view lineText;
  std::string targetSection;
  std::filesystem::path resolvedPath;
};

bool appendResolvedCallTarget(
  std::vector<CallSite>& sites,
  int lineIndex,
  std::string_view lineText,
  std::string_view rawPath,
  std::string_view remainder,
  const LibraryLayout& layout)
{
  const auto parsed = parseIncludeCallPathAndSection(rawPath, remainder);
  const auto resolved = layout.resolveVertexFilePath(parsed.filePath);
  if (!resolved)
  {
    return false;
  }
  if (!layout.overlayForPath(*resolved))
  {
    return false;
  }
  sites.push_back(CallSite{
    .lineIndex = lineIndex,
    .lineText = lineText,
    .targetSection = parsed.targetSection,
    .resolvedPath = *resolved,
  });
  return true;
}

void extractParenthesizedCallTargets(
  std::string_view line,
  int lineIndex,
  std::vector<CallSite>& sites,
  const LibraryLayout& layout)
{
  const auto lower = toLowerAscii(line);
  std::size_t search = 0;
  while (search < lower.size())
  {
    const auto at = lower.find("call", search);
    if (at == std::string::npos)
    {
      break;
    }
    search = at + 1;
    if (at > 0 && isWordChar(static_cast<unsigned char>(line[at - 1])))
    {
      continue;
    }
    if (at + 4 < line.size() && isWordChar(static_cast<unsigned char>(line[at + 4])))
    {
      continue;
    }
    if (isInsideQuotes(line, at))
    {
      continue;
    }
    auto after = at + 4;
    while (after < line.size() && std::isspace(static_cast<unsigned char>(line[after])))
    {
      ++after;
    }
    if (after >= line.size() || line[after] != '(')
    {
      continue;
    }
    const auto close = findMatchingParenIndex(line, after);
    if (close < 0)
    {
      continue;
    }
    const auto inside = line.substr(after + 1, static_cast<std::size_t>(close) - after - 1);
    const auto params = parseCommaParams(inside);
    if (params.empty() || !isValidCallPathParameter(params[0]))
    {
      continue;
    }
    std::string remainder;
    if (params.size() > 1)
    {
      remainder = params[1];
      for (std::size_t i = 2; i < params.size(); ++i)
      {
        remainder.push_back(',');
        remainder += params[i];
      }
    }
    appendResolvedCallTarget(sites, lineIndex, line, params[0], remainder, layout);
  }
}

void extractSpaceFormCallTargets(
  std::string_view line,
  int lineIndex,
  std::vector<CallSite>& sites,
  const LibraryLayout& layout)
{
  const auto lower = toLowerAscii(line);
  std::size_t search = 0;
  while (search < lower.size())
  {
    const auto at = lower.find("call", search);
    if (at == std::string::npos)
    {
      break;
    }
    search = at + 1;
    if (at > 0 && isWordChar(static_cast<unsigned char>(line[at - 1])))
    {
      continue;
    }
    if (at + 4 < line.size() && isWordChar(static_cast<unsigned char>(line[at + 4])))
    {
      continue;
    }
    if (isInsideQuotes(line, at))
    {
      continue;
    }
    auto after = at + 4;
    if (after >= line.size() || !std::isspace(static_cast<unsigned char>(line[after])))
    {
      continue;
    }
    while (after < line.size() && std::isspace(static_cast<unsigned char>(line[after])))
    {
      ++after;
    }
    if (after >= line.size() || line[after] == '(' || line[after] == ',')
    {
      continue;
    }
    auto end = after;
    while (end < line.size())
    {
      const auto ch = line[end];
      if (std::isspace(static_cast<unsigned char>(ch)) || ch == ',' || ch == '(')
      {
        break;
      }
      ++end;
    }
    if (end == after)
    {
      continue;
    }
    const auto token = line.substr(after, end - after);
    appendResolvedCallTarget(sites, lineIndex, line, token, {}, layout);
  }
}

std::vector<CallSite> findCallSites(const ValidateContext& ctx)
{
  std::vector<CallSite> sites;
  if (!ctx.stores.layout)
  {
    return sites;
  }
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    const auto line = ctx.prep->lines[static_cast<std::size_t>(i)];
    const auto trimmed = trim(line);
    if (trimmed.empty() || trimmed.front() == '!')
    {
      continue;
    }
    const auto lower = toLowerAscii(trimmed);
    if (lower.find("call") == std::string::npos)
    {
      continue;
    }
    extractParenthesizedCallTargets(line, i, sites, *ctx.stores.layout);
    extractSpaceFormCallTargets(line, i, sites, *ctx.stores.layout);
  }
  return sites;
}

std::vector<std::string> selectCalleeCommonRequirements(
  const std::vector<CalleeCommonRequirement>& requirements,
  std::string_view targetSection)
{
  std::vector<std::string> selected;
  std::unordered_set<std::string> seen;
  const auto section = std::string(targetSection);
  for (const auto& req : requirements)
  {
    if (req.directive.empty())
    {
      continue;
    }
    if (section.empty() || req.sectionName.empty() || req.sectionName == section)
    {
      if (seen.insert(req.directive).second)
      {
        selected.push_back(req.directive);
      }
    }
  }
  return selected;
}

bool callerHasCommonDirective(
  const ValidateContext& ctx,
  std::string_view directive,
  std::string_view sourceSection)
{
  static const std::regex indexes{R"re(^common_indexes\s+1\b)re", std::regex::icase};
  static const std::regex dbases{R"re(^common_dbases\s+1\b)re", std::regex::icase};
  static const std::regex elements{R"re(^common_elements\s+1\b)re", std::regex::icase};
  static const std::regex files{R"re(^common_files\s+1\b)re", std::regex::icase};
  static const std::regex points{R"re(^common_points\s+1\b)re", std::regex::icase};
  const std::regex* pattern = nullptr;
  if (directive == "common_indexes")
  {
    pattern = &indexes;
  }
  else if (directive == "common_dbases")
  {
    pattern = &dbases;
  }
  else if (directive == "common_elements")
  {
    pattern = &elements;
  }
  else if (directive == "common_files")
  {
    pattern = &files;
  }
  else if (directive == "common_points")
  {
    pattern = &points;
  }
  else
  {
    return false;
  }

  const auto wanted = std::string(sourceSection);
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    if (lineInVerificationIgnoredSection(i, ctx.prep->sections))
    {
      continue;
    }
    if (sectionNameForLine(i, ctx.prep->sections) != wanted)
    {
      continue;
    }
    const auto trimmed = trim(ctx.prep->lines[static_cast<std::size_t>(i)]);
    if (trimmed.starts_with("!"))
    {
      continue;
    }
    std::cmatch match;
    if (std::regex_search(trimmed.data(), trimmed.data() + trimmed.size(), match, *pattern)
        && match.position() == 0)
    {
      return true;
    }
  }
  return false;
}

std::shared_ptr<const IndexEntry> calleeIndexEntry(
  const ValidateContext& ctx,
  const CallSite& site)
{
  if (!ctx.stores.index || !ctx.stores.layout)
  {
    return {};
  }
  if (const auto identity = ctx.stores.layout->overlayForPath(site.resolvedPath))
  {
    if (auto entry = ctx.stores.index->entry(identity->key))
    {
      return entry;
    }
  }
  return ctx.stores.index->entryByAbsPath(site.resolvedPath);
}

std::vector<std::string> requirementsForSite(
  const ValidateContext& ctx,
  const CallSite& site)
{
  const auto entry = calleeIndexEntry(ctx, site);
  if (!entry)
  {
    return {};
  }
  return selectCalleeCommonRequirements(entry->calleeCommonRequirements, site.targetSection);
}

lsp::Diagnostic wrappedWarning(
  const ValidateContext& ctx,
  const CallSite& site,
  std::string inner)
{
  const auto line = static_cast<lsp::Uint>(site.lineIndex);
  const auto endChar = static_cast<lsp::Uint>(utf16Length(site.lineText));
  const auto startChar = site.lineText.empty() ? 0u : 1u;
  const lsp::Range range{
    .start = {.line = line, .character = startChar},
    .end = {.line = line, .character = endChar},
  };
  std::string message = std::string(site.lineText) + " has an error: " + inner;
  lsp::Diagnostic diagnostic{
    .range = range,
    .message = message,
    .severity = lsp::DiagnosticSeverity::Warning,
    .source = std::string("ADT-Syntax"),
  };
  diagnostic.relatedInformation = std::vector<lsp::DiagnosticRelatedInformation>{
    lsp::DiagnosticRelatedInformation{
      .location = {.uri = ctx.prep->snapshot.uri, .range = range},
      .message = std::move(inner),
    },
  };
  return diagnostic;
}

} // namespace

void familyCalleeCommonRequirements(ValidateContext& ctx)
{
  ctx.pollCancel();
  const auto sites = findCallSites(ctx);
  for (const auto& site : sites)
  {
    ctx.pollCancel();
    if (lineInVerificationIgnoredSection(site.lineIndex, ctx.prep->sections))
    {
      continue;
    }
    const auto required = requirementsForSite(ctx, site);
    if (required.empty())
    {
      continue;
    }
    const auto sourceSection = sectionNameForLine(site.lineIndex, ctx.prep->sections);
    const auto calleeLabel = site.resolvedPath.filename().string();
    const auto locationHint = sourceSection.empty()
      ? std::string("in this file")
      : "in #" + sourceSection + "#";
    for (const auto& directive : required)
    {
      const auto* spec = specForDirective(directive);
      if (!spec || callerHasCommonDirective(ctx, directive, sourceSection))
      {
        continue;
      }
      auto inner = "call() to " + calleeLabel + " requires " + std::string(spec->directive)
        + " 1 " + locationHint + " (declared via " + std::string(spec->marker) + ").";
      ctx.append(wrappedWarning(ctx, site, std::move(inner)));
    }
  }
}

} // namespace vertexadt
