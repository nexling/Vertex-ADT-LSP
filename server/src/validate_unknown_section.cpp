#include "validate.h"
#include "utf16_text.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

struct SuffixSite
{
  std::string filePath;
  std::string targetSection;
  std::size_t sectionStart = 0;
  std::size_t sectionEnd = 0;
};

bool isAsciiSpace(unsigned char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

bool isWordChar(unsigned char ch)
{
  return (ch >= 'A' && ch <= 'Z')
    || (ch >= 'a' && ch <= 'z')
    || (ch >= '0' && ch <= '9')
    || ch == '_';
}

bool isIdentStart(unsigned char ch)
{
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

char asciiLower(unsigned char ch)
{
  if (ch >= 'A' && ch <= 'Z')
  {
    return static_cast<char>(ch - 'A' + 'a');
  }
  return static_cast<char>(ch);
}

bool equalsIgnoreAscii(std::string_view text, std::string_view lowerLiteral)
{
  if (text.size() != lowerLiteral.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    if (asciiLower(static_cast<unsigned char>(text[i])) != lowerLiteral[i])
    {
      return false;
    }
  }
  return true;
}

bool isSectionKeyword(std::string_view token)
{
  const auto lower = toLowerAscii(token);
  return lower == "section" || lower == "sectionoffile";
}

auto stripPathQuotesNode(std::string_view value) -> std::string
{
  const auto trimmed = trim(value);
  if (trimmed.size() >= 2)
  {
    const auto first = trimmed.front();
    const auto last = trimmed.back();
    if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
    {
      return std::string(trimmed.substr(1, trimmed.size() - 2));
    }
  }
  if (!trimmed.empty() && (trimmed.front() == '"' || trimmed.front() == '\''))
  {
    return std::string(trimmed.substr(1));
  }
  return std::string(trimmed);
}

auto takeIdent(std::string_view text) -> std::string_view
{
  if (text.empty() || !isIdentStart(static_cast<unsigned char>(text.front())))
  {
    return {};
  }
  std::size_t i = 1;
  while (i < text.size() && isWordChar(static_cast<unsigned char>(text[i])))
  {
    ++i;
  }
  return text.substr(0, i);
}

bool isCallAt(std::string_view text, std::size_t offset)
{
  constexpr std::string_view kCall = "call";
  if (offset + kCall.size() > text.size())
  {
    return false;
  }
  if (offset > 0 && isWordChar(static_cast<unsigned char>(text[offset - 1])))
  {
    return false;
  }
  return equalsIgnoreAscii(text.substr(offset, kCall.size()), kCall);
}

std::size_t skipSpaces(std::string_view text, std::size_t offset)
{
  while (offset < text.size() && isAsciiSpace(static_cast<unsigned char>(text[offset])))
  {
    ++offset;
  }
  return offset;
}

bool consumeKeywordTail(std::string& pathPart)
{
  const auto trimmed = trim(pathPart);
  const auto ident = [&] {
    std::size_t i = trimmed.size();
    while (i > 0 && isWordChar(static_cast<unsigned char>(trimmed[i - 1])))
    {
      --i;
    }
    return trimmed.substr(i);
  }();
  if (ident.empty() || !isSectionKeyword(ident))
  {
    return false;
  }
  auto cut = std::string(trimmed.substr(0, trimmed.size() - ident.size()));
  cut = std::string(trim(cut));
  pathPart = std::move(cut);
  return true;
}

struct ParsedPathSection
{
  std::string filePath;
  std::string targetSection;
};

auto parseIncludeCallPathAndSection(std::string_view rawPath, std::string_view lineRemainder)
  -> ParsedPathSection
{
  auto pathPart = replaceBackslashes(stripPathQuotesNode(rawPath));
  pathPart = std::string(trim(pathPart));
  std::string targetSection;

  const auto pathIdent = [&]() -> std::optional<std::pair<std::string, std::string>> {
    std::size_t i = pathPart.size();
    while (i > 0 && isWordChar(static_cast<unsigned char>(pathPart[i - 1])))
    {
      --i;
    }
    if (i == 0 || i == pathPart.size())
    {
      return std::nullopt;
    }
    if (!isAsciiSpace(static_cast<unsigned char>(pathPart[i - 1])))
    {
      return std::nullopt;
    }
    if (!isIdentStart(static_cast<unsigned char>(pathPart[i])))
    {
      return std::nullopt;
    }
    auto token = pathPart.substr(i);
    auto prefix = std::string(trim(std::string_view(pathPart).substr(0, i)));
    if (prefix.empty())
    {
      return std::nullopt;
    }
    return std::make_pair(std::move(prefix), std::move(token));
  }();

  if (pathIdent)
  {
    if (!isSectionKeyword(pathIdent->second))
    {
      targetSection = pathIdent->second;
      pathPart = pathIdent->first;
    }
    else
    {
      consumeKeywordTail(pathPart);
    }
  }

  const auto remainder = trim(lineRemainder);
  if (!remainder.empty())
  {
    const auto token = takeIdent(remainder);
    if (!token.empty() && !isSectionKeyword(token))
    {
      targetSection = std::string(token);
    }
  }

  return ParsedPathSection{
    .filePath = std::move(pathPart),
    .targetSection = std::move(targetSection),
  };
}

std::optional<std::pair<std::size_t, std::size_t>> findIdentRange(
  std::string_view line,
  std::string_view ident,
  std::size_t fromIndex)
{
  if (ident.empty() || fromIndex > line.size())
  {
    return std::nullopt;
  }
  auto identAt = [&](std::size_t start) {
    if (start + ident.size() > line.size())
    {
      return false;
    }
    if (line.compare(start, ident.size(), ident) != 0)
    {
      return false;
    }
    const auto after = start + ident.size();
    return after == line.size() || !isWordChar(static_cast<unsigned char>(line[after]));
  };

  for (std::size_t i = fromIndex; i < line.size(); ++i)
  {
    if (i == 0 && identAt(0))
    {
      return std::make_pair(std::size_t{0}, ident.size());
    }
    if (!isWordChar(static_cast<unsigned char>(line[i])) && identAt(i + 1))
    {
      return std::make_pair(i + 1, i + 1 + ident.size());
    }
  }
  return std::nullopt;
}

void pushSuffixSite(
  std::vector<SuffixSite>& sites,
  std::string filePath,
  std::string targetSection,
  std::optional<std::pair<std::size_t, std::size_t>> range)
{
  if (targetSection.empty() || !range)
  {
    return;
  }
  sites.push_back(SuffixSite{
    .filePath = std::move(filePath),
    .targetSection = std::move(targetSection),
    .sectionStart = range->first,
    .sectionEnd = range->second,
  });
}

std::optional<std::pair<std::string_view, std::size_t>> matchQuotedPath(std::string_view code, std::size_t pos)
{
  if (pos >= code.size())
  {
    return std::nullopt;
  }
  const auto quote = code[pos];
  if (quote != '"' && quote != '\'')
  {
    return std::nullopt;
  }
  auto close = pos + 1;
  while (close < code.size() && code[close] != quote)
  {
    ++close;
  }
  if (close >= code.size() || close == pos + 1)
  {
    return std::nullopt;
  }
  return std::make_pair(code.substr(pos, close - pos + 1), close + 1);
}

std::optional<std::pair<std::string_view, std::size_t>> matchUnquotedIncludePath(
  std::string_view code,
  std::size_t pos)
{
  if (pos >= code.size())
  {
    return std::nullopt;
  }
  std::size_t end = pos;
  while (end < code.size())
  {
    const auto ch = static_cast<unsigned char>(code[end]);
    if (isAsciiSpace(ch) || code[end] == '#' || code[end] == ';')
    {
      break;
    }
    ++end;
  }
  if (end == pos)
  {
    return std::nullopt;
  }
  return std::make_pair(code.substr(pos, end - pos), end);
}

void collectIncludeSite(std::string_view code, std::vector<SuffixSite>& sites)
{
  std::size_t i = 0;
  while (i < code.size() && isAsciiSpace(static_cast<unsigned char>(code[i])))
  {
    ++i;
  }
  constexpr std::string_view kInclude = "# include";
  if (i + kInclude.size() > code.size()
      || code.substr(i, kInclude.size()) != kInclude)
  {
    return;
  }
  i += kInclude.size();
  if (i >= code.size() || !isAsciiSpace(static_cast<unsigned char>(code[i])))
  {
    return;
  }
  i = skipSpaces(code, i);
  const auto pathStart = i;
  auto quoted = matchQuotedPath(code, i);
  std::string_view rawPath;
  std::size_t pathEnd = i;
  if (quoted)
  {
    rawPath = quoted->first;
    pathEnd = quoted->second;
  }
  else if (auto unquoted = matchUnquotedIncludePath(code, i))
  {
    rawPath = unquoted->first;
    pathEnd = unquoted->second;
  }
  else
  {
    return;
  }

  const auto remainder = code.substr(pathEnd);
  const auto parsed = parseIncludeCallPathAndSection(rawPath, remainder);
  if (parsed.targetSection.empty())
  {
    return;
  }

  const auto inner = stripPathQuotesNode(rawPath);
  std::optional<std::pair<std::size_t, std::size_t>> range;
  std::size_t innerIdentStart = inner.size();
  while (innerIdentStart > 0 && isWordChar(static_cast<unsigned char>(inner[innerIdentStart - 1])))
  {
    --innerIdentStart;
  }
  const bool innerHasSection = innerIdentStart > 0
    && innerIdentStart < inner.size()
    && isAsciiSpace(static_cast<unsigned char>(inner[innerIdentStart - 1]))
    && isIdentStart(static_cast<unsigned char>(inner[innerIdentStart]))
    && inner.substr(innerIdentStart) == parsed.targetSection;
  if (innerHasSection)
  {
    range = findIdentRange(code, parsed.targetSection, pathStart);
  }
  else
  {
    range = findIdentRange(code, parsed.targetSection, pathEnd);
  }
  pushSuffixSite(sites, parsed.filePath, parsed.targetSection, range);
}

void collectCallParenSites(std::string_view code, std::vector<SuffixSite>& sites)
{
  constexpr std::size_t kCallLen = 4;
  for (std::size_t i = 0; i < code.size();)
  {
    if (!isCallAt(code, i))
    {
      ++i;
      continue;
    }
    auto pos = skipSpaces(code, i + kCallLen);
    if (pos >= code.size() || code[pos] != '(')
    {
      ++i;
      continue;
    }
    pos = skipSpaces(code, pos + 1);
    if (pos >= code.size())
    {
      ++i;
      continue;
    }
    const auto quote = code[pos];
    if (quote != '"' && quote != '\'')
    {
      ++i;
      continue;
    }
    const auto quoteStart = pos;
    auto close = pos + 1;
    bool invalidInner = false;
    while (close < code.size() && code[close] != quote)
    {
      if (code[close] == '\\' || code[close] == '"' || code[close] == '\'')
      {
        invalidInner = true;
        break;
      }
      ++close;
    }
    if (invalidInner || close >= code.size())
    {
      ++i;
      continue;
    }
    const auto inner = code.substr(pos + 1, close - (pos + 1));
    const auto parsed = parseIncludeCallPathAndSection(inner, {});
    if (!parsed.targetSection.empty())
    {
      pushSuffixSite(
        sites,
        parsed.filePath,
        parsed.targetSection,
        findIdentRange(code, parsed.targetSection, quoteStart));
    }
    i = close + 1;
  }
}

bool takeSpaceIdent(
  std::string_view code,
  std::size_t pos,
  std::string_view& token,
  std::size_t& tokenStart,
  std::size_t& after)
{
  const auto start = skipSpaces(code, pos);
  if (start == pos || start >= code.size())
  {
    return false;
  }
  token = takeIdent(code.substr(start));
  if (token.empty())
  {
    return false;
  }
  tokenStart = start;
  after = start + token.size();
  return true;
}

void collectCallSpaceSites(std::string_view code, std::vector<SuffixSite>& sites)
{
  constexpr std::size_t kCallLen = 4;
  for (std::size_t i = 0; i < code.size();)
  {
    if (!isCallAt(code, i))
    {
      ++i;
      continue;
    }
    if (i + kCallLen >= code.size()
        || !isAsciiSpace(static_cast<unsigned char>(code[i + kCallLen])))
    {
      ++i;
      continue;
    }
    auto pathPos = skipSpaces(code, i + kCallLen);
    if (pathPos >= code.size() || code[pathPos] == '(')
    {
      ++i;
      continue;
    }

    std::string_view rawPath;
    std::string_view token;
    std::size_t tokenStart = 0;
    std::size_t matchEnd = pathPos;
    bool matched = false;

    if (auto quoted = matchQuotedPath(code, pathPos))
    {
      std::string_view spaceToken;
      std::size_t spaceTokenStart = 0;
      std::size_t after = 0;
      if (takeSpaceIdent(code, quoted->second, spaceToken, spaceTokenStart, after))
      {
        rawPath = quoted->first;
        token = spaceToken;
        tokenStart = spaceTokenStart;
        matchEnd = after;
        matched = true;
      }
    }

    if (!matched)
    {
      std::size_t end = pathPos;
      while (end < code.size())
      {
        const auto ch = code[end];
        if (isAsciiSpace(static_cast<unsigned char>(ch)) || ch == ',' || ch == '(')
        {
          break;
        }
        ++end;
      }
      if (end > pathPos)
      {
        std::string_view spaceToken;
        std::size_t spaceTokenStart = 0;
        std::size_t after = 0;
        if (takeSpaceIdent(code, end, spaceToken, spaceTokenStart, after))
        {
          rawPath = code.substr(pathPos, end - pathPos);
          token = spaceToken;
          tokenStart = spaceTokenStart;
          matchEnd = after;
          matched = true;
        }
      }
    }

    if (!matched)
    {
      ++i;
      continue;
    }

    const auto parsed = parseIncludeCallPathAndSection(rawPath, token);
    if (!parsed.targetSection.empty())
    {
      pushSuffixSite(
        sites,
        parsed.filePath,
        parsed.targetSection,
        std::make_pair(tokenStart, tokenStart + token.size()));
    }
    i = matchEnd;
  }
}

auto collectNamedSectionSuffixes(std::string_view line) -> std::vector<SuffixSite>
{
  const auto bang = findInlineBangCommentIndex(line);
  const auto code = bang < 0 ? line : line.substr(0, static_cast<std::size_t>(bang));
  const auto trimmed = trim(code);
  if (trimmed.empty() || trimmed.starts_with("!"))
  {
    return {};
  }

  std::vector<SuffixSite> sites;
  collectIncludeSite(code, sites);
  collectCallParenSites(code, sites);
  collectCallSpaceSites(code, sites);

  std::vector<SuffixSite> unique;
  std::unordered_set<std::string> seen;
  unique.reserve(sites.size());
  for (auto& site : sites)
  {
    auto key = std::to_string(site.sectionStart) + ":" + std::to_string(site.sectionEnd) + ":"
      + site.targetSection;
    if (!seen.insert(std::move(key)).second)
    {
      continue;
    }
    unique.push_back(std::move(site));
  }
  return unique;
}

auto listUniqueTargetSectionNames(std::string_view text) -> std::vector<std::string>
{
  const auto lines = splitLines(text);
  const auto sections = parseFileSections(lines);
  std::vector<std::string> names;
  std::unordered_set<std::string> seen;
  names.reserve(sections.size());
  for (const auto& section : sections)
  {
    if (isSectionKeyword(section.name) || !seen.insert(section.name).second)
    {
      continue;
    }
    names.push_back(section.name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

bool isUnknownTargetSectionName(std::string_view token, const std::vector<std::string>& names)
{
  if (token.empty() || isSectionKeyword(token))
  {
    return false;
  }
  for (const auto& name : names)
  {
    if (name == token)
    {
      return false;
    }
  }
  for (const auto& name : names)
  {
    if (name.size() >= token.size() && name.compare(0, token.size(), token) == 0)
    {
      return false;
    }
  }
  return true;
}

auto loadLinkedFileText(ValidateContext& ctx, const std::filesystem::path& resolved)
  -> std::optional<std::string>
{
  if (ctx.stores.textForPath)
  {
    return ctx.stores.textForPath(resolved);
  }
  if (ctx.stores.linkedFiles)
  {
    if (auto snapshot = ctx.stores.linkedFiles->get(normalizeFsPath(resolved.string())))
    {
      return snapshot->text;
    }
  }
  return std::nullopt;
}

lsp::Diagnostic makeDiagnostic(
  int line,
  std::string_view lineText,
  const SuffixSite& site,
  const std::filesystem::path& resolved)
{
  lsp::Diagnostic diagnostic;
  diagnostic.range = lsp::Range{
    .start = lsp::Position{
      .line = static_cast<lsp::Uint>(line),
      .character = static_cast<lsp::Uint>(utf16Offset(lineText, site.sectionStart)),
    },
    .end = lsp::Position{
      .line = static_cast<lsp::Uint>(line),
      .character = static_cast<lsp::Uint>(utf16Offset(lineText, site.sectionEnd)),
    },
  };
  diagnostic.message = "Unknown section '" + site.targetSection + "' in "
    + resolved.filename().string();
  diagnostic.severity = lsp::DiagnosticSeverity::Warning;
  diagnostic.source = "ADT-Syntax";
  diagnostic.code = std::string("vertexadt/unknown-section");
  return diagnostic;
}

} // namespace

auto uniqueTargetSectionNames(std::string_view text) -> std::vector<std::string>
{
  return listUniqueTargetSectionNames(text);
}

void familyUnknownSection(ValidateContext& ctx)
{
  if (!ctx.stores.layout)
  {
    return;
  }

  std::vector<lsp::Diagnostic> found;
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    ctx.pollCancel();
    const auto line = ctx.prep->lines[static_cast<std::size_t>(i)];
    const auto trimmed = trim(line);
    if (trimmed.empty() || trimmed.starts_with("!"))
    {
      continue;
    }
    if (lineInVerificationIgnoredSection(i, ctx.prep->sections))
    {
      continue;
    }

    const auto sites = collectNamedSectionSuffixes(line);
    for (const auto& site : sites)
    {
      if (site.targetSection.empty())
      {
        continue;
      }
      const auto resolved = ctx.stores.layout->resolveVertexFilePath(site.filePath);
      if (!resolved)
      {
        continue;
      }
      const auto targetText = loadLinkedFileText(ctx, *resolved);
      if (!targetText)
      {
        continue;
      }
      const auto names = listUniqueTargetSectionNames(*targetText);
      if (!isUnknownTargetSectionName(site.targetSection, names))
      {
        continue;
      }
      found.push_back(makeDiagnostic(i, line, site, *resolved));
    }
  }

  for (auto& diagnostic : found)
  {
    ctx.append(std::move(diagnostic));
  }
}

} // namespace vertexadt
