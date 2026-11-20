#include "call_param_sites.h"

#include "command_parse.h"
#include "utf16_text.h"

#include <algorithm>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <regex>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace vertexadt {
namespace {

bool isWordChar(unsigned char ch)
{
  return std::isalnum(ch) || ch == '_';
}

bool isAsciiSpace(unsigned char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

bool startsWithCall(std::string_view text, std::size_t index)
{
  if (index + 4 > text.size())
  {
    return false;
  }
  auto eq = [](char ch, char lower) {
    return std::tolower(static_cast<unsigned char>(ch)) == lower;
  };
  return eq(text[index], 'c') && eq(text[index + 1], 'a') && eq(text[index + 2], 'l')
    && eq(text[index + 3], 'l');
}

bool lineHasCallToken(std::string_view line)
{
  for (std::size_t i = 0; i < line.size(); ++i)
  {
    if (startsWithCall(line, i) && (i == 0 || !isWordChar(static_cast<unsigned char>(line[i - 1]))))
    {
      const auto after = i + 4;
      if (after == line.size() || !isWordChar(static_cast<unsigned char>(line[after])))
      {
        return true;
      }
    }
  }
  return false;
}

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

auto parseIncludeCallPathAndSection(std::string_view rawPath, std::string_view remainder)
  -> ParsedPath
{
  auto pathPart = replaceBackslashes(stripPathQuotes(rawPath));
  pathPart = std::string(trim(pathPart));
  std::string targetSection;
  static const std::regex pathSection{R"(^(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$)"};
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
      static const std::regex stripSection{R"(\s+SECTION(?:OFFILE)?$)", std::regex::icase};
      pathPart = std::regex_replace(pathPart, stripSection, "");
      pathPart = std::string(trim(pathPart));
    }
  }
  const auto rem = trim(remainder);
  if (!rem.empty())
  {
    static const std::regex remRe{R"(^([A-Za-z_][A-Za-z0-9_]*)\b)"};
    std::cmatch remMatch;
    if (std::regex_search(rem.data(), rem.data() + rem.size(), remMatch, remRe)
        && !isSectionKeyword(remMatch[1].str()))
    {
      targetSection = remMatch[1].str();
    }
  }
  return ParsedPath{.filePath = std::move(pathPart), .targetSection = std::move(targetSection)};
}

bool looksLikeQuotedString(std::string_view part)
{
  static const std::regex dquote{R"("[^"]*")"};
  static const std::regex squote{R"('[^']*')"};
  return std::regex_search(part.begin(), part.end(), dquote)
    || std::regex_search(part.begin(), part.end(), squote);
}

bool isValidStringVariable(std::string_view expr)
{
  auto text = std::string(trim(expr));
  if (text.empty() || text.size() > 16)
  {
    return false;
  }
  static const std::regex variable{
    R"([a-zA-Z][a-zA-Z0-9_]*\$?(?:\([A-Za-z0-9]*\))?)"};
  if (std::regex_search(text, variable) && text.find('$') != std::string::npos)
  {
    return true;
  }
  static const std::regex stringVar{R"(^[a-zA-Z][a-zA-Z0-9_]*\$)"};
  return std::regex_match(text, stringVar);
}

bool isValidCallPathParameter(std::string_view expr)
{
  if (trim(expr).empty())
  {
    return false;
  }
  static const std::regex concatOp{R"(\s*&\s*)"};
  const std::string text{expr};
  std::vector<std::string> parts;
  auto searchStart = text.cbegin();
  std::smatch match;
  while (std::regex_search(searchStart, text.cend(), match, concatOp))
  {
    parts.emplace_back(searchStart, match[0].first);
    parts.emplace_back(match[0].str());
    searchStart = match[0].second;
  }
  parts.emplace_back(searchStart, text.cend());

  for (std::size_t i = 0; i < parts.size(); ++i)
  {
    const auto part = std::string(trim(parts[i]));
    if (part.empty())
    {
      continue;
    }
    if (i % 2 == 0)
    {
      if (!looksLikeQuotedString(part) && !isValidStringVariable(part))
      {
        return false;
      }
    }
    else
    {
      static const std::regex concatOnly{R"(^\s*&\s*$)"};
      if (!std::regex_match(parts[i], concatOnly))
      {
        return false;
      }
    }
  }
  return true;
}

bool fileExists(const std::filesystem::path& path)
{
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec);
}

auto expectedCountForResolved(
  const ValidateStores& stores,
  const std::filesystem::path& resolved,
  std::optional<std::string_view> targetSection) -> int
{
  if (stores.textForPath)
  {
    (void)stores.textForPath(resolved);
  }
  const auto normalized = normalizeFsPath(resolved.string());
  if (stores.linkedFiles && stores.index)
  {
    return stores.linkedFiles->expectedCallParamCount(
      normalized, targetSection, *stores.index);
  }
  if (stores.linkedFiles)
  {
    if (auto snapshot = stores.linkedFiles->get(normalized); snapshot && !snapshot->text.empty())
    {
      return expectedCallParamCountFromText(snapshot->text, targetSection);
    }
  }
  if (stores.textForPath)
  {
    if (auto text = stores.textForPath(resolved); text && !text->empty())
    {
      return expectedCallParamCountFromText(*text, targetSection);
    }
  }
  return 0;
}

auto jsonInt(std::int64_t n) -> lsp::json::Value
{
  return lsp::json::Value(static_cast<std::int32_t>(n));
}

void emitCountDiagnostic(
  ValidateContext& ctx,
  int lineIndex,
  std::string_view line,
  const CallParamSite& site)
{
  const auto missing = site.expected > site.argCount;
  lsp::Diagnostic diagnostic;
  diagnostic.range = lsp::Range{
    .start = lsp::Position{
      .line = static_cast<lsp::Uint>(lineIndex),
      .character = static_cast<lsp::Uint>(utf16Offset(line, static_cast<std::size_t>(site.openParenIndex))),
    },
    .end = lsp::Position{
      .line = static_cast<lsp::Uint>(lineIndex),
      .character = static_cast<lsp::Uint>(utf16Offset(line, static_cast<std::size_t>(site.closeParenIndex) + 1)),
    },
  };
  diagnostic.message = "call expects " + std::to_string(site.expected) + " parameters but "
    + std::to_string(site.argCount) + " were provided";
  diagnostic.severity = missing ? lsp::DiagnosticSeverity::Error : lsp::DiagnosticSeverity::Warning;
  diagnostic.source = "ADT-Syntax";
  diagnostic.code = missing ? std::string("vertexadt/missing-call-params")
                            : std::string("vertexadt/extra-call-params");

  // Node's `data` payload: line + UTF-16 offsets on the code line. Code
  // actions read it back from the request; JS drops undefined removeStart/End.
  lsp::json::Object data;
  data["line"] = jsonInt(lineIndex);
  if (missing)
  {
    data["insertOffset"] = jsonInt(static_cast<lsp::Uint>(utf16Offset(line, static_cast<std::size_t>(site.closeParenIndex))));
    data["missingCount"] = jsonInt(site.expected - site.argCount);
  }
  else
  {
    data["extraCount"] = jsonInt(site.argCount - site.expected);
  }
  data["expected"] = jsonInt(site.expected);
  data["actual"] = jsonInt(site.argCount);
  data["calleeDisplayPath"] = lsp::json::Value(site.calleeDisplayPath);
  if (!missing)
  {
    if (const auto removal = extraCallParamRemovalRange(line, site))
    {
      data["removeStart"] = jsonInt(removal->first);
      data["removeEnd"] = jsonInt(removal->second);
    }
  }
  diagnostic.data = lsp::json::Value(std::move(data));
  ctx.append(std::move(diagnostic));
}

// Per-pass memo: raw callee path + target section -> expected count (0 when
// unresolved, missing, or no `$CALLPARAM`). Resolution and the count depend
// only on that key, the layout, the disk and the linked-file snapshot, so one
// validate pass answers each callee once instead of re-parsing it per site.
using ExpectedCountMemo = std::unordered_map<std::string, int>;

auto expectedCountForPath(const ValidateStores& stores, const ParsedPath& parsedPath) -> int
{
  const auto resolved = stores.layout->resolveVertexFilePath(parsedPath.filePath);
  if (!resolved || !fileExists(*resolved))
  {
    return 0;
  }
  std::optional<std::string_view> target;
  if (!parsedPath.targetSection.empty())
  {
    target = parsedPath.targetSection;
  }
  return expectedCountForResolved(stores, *resolved, target);
}

auto sitesOnLine(
  const ValidateStores& stores,
  std::string_view line,
  ExpectedCountMemo* memo = nullptr) -> std::vector<CallParamSite>
{
  std::vector<CallParamSite> sites;
  if (!lineHasCallToken(line) || !stores.layout)
  {
    return sites;
  }

  for (std::size_t i = 0; i < line.size(); ++i)
  {
    if (!startsWithCall(line, i))
    {
      continue;
    }
    if (i > 0 && isWordChar(static_cast<unsigned char>(line[i - 1])))
    {
      continue;
    }
    std::size_t j = i + 4;
    while (j < line.size() && isAsciiSpace(static_cast<unsigned char>(line[j])))
    {
      ++j;
    }
    if (j >= line.size() || line[j] != '(')
    {
      continue;
    }
    if (isInsideQuotes(line, i))
    {
      continue;
    }

    const auto openParenIndex = static_cast<int>(j);
    const auto closeParenIndex = findMatchingParenIndex(line, openParenIndex);
    if (closeParenIndex == -1)
    {
      continue;
    }

    auto startIndex = i;
    const auto prefix = std::string(line.substr(0, i));
    static const std::regex assignmentPrefix{R"(([A-Za-z][A-Za-z0-9_]*\$?)\s*=\s*$)"};
    std::smatch assignmentMatch;
    if (std::regex_search(prefix, assignmentMatch, assignmentPrefix)
        && assignmentMatch.position() + assignmentMatch.length() == prefix.size())
    {
      startIndex = prefix.size() - assignmentMatch[0].length();
    }

    const auto segment = std::string(
      trim(line.substr(startIndex, static_cast<std::size_t>(closeParenIndex) + 1 - startIndex)));
    const auto parsed = parseCommand(segment);
    if (!parsed || toLowerAscii(parsed->functionName) != "call" || parsed->insideParams.empty()
        || !isValidCallPathParameter(parsed->insideParams[0]))
    {
      continue;
    }

    const auto parsedPath = parseIncludeCallPathAndSection(parsed->insideParams[0], "");
    int expected = 0;
    if (memo)
    {
      auto key = parsedPath.filePath + '\n' + parsedPath.targetSection;
      if (const auto hit = memo->find(key); hit != memo->end())
      {
        expected = hit->second;
      }
      else
      {
        expected = expectedCountForPath(stores, parsedPath);
        memo->emplace(std::move(key), expected);
      }
    }
    else
    {
      expected = expectedCountForPath(stores, parsedPath);
    }
    if (expected <= 0)
    {
      continue;
    }

    const auto argCount = static_cast<int>(parsed->insideParams.size()) - 1;
    if (argCount == expected)
    {
      continue;
    }
    sites.push_back(CallParamSite{
      .openParenIndex = openParenIndex,
      .closeParenIndex = closeParenIndex,
      .expected = expected,
      .argCount = argCount,
      .calleeDisplayPath = std::string(trim(parsed->insideParams[0])),
    });
  }
  return sites;
}

} // namespace

void familyCallParamCount(ValidateContext& ctx)
{
  if (!ctx.settings.validateParameterCount)
  {
    return;
  }

  ExpectedCountMemo memo;
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    ctx.pollCancel();
    if (ctx.atCap())
    {
      return;
    }

    auto line = std::string(ctx.prep->lines[static_cast<std::size_t>(i)]);
    const auto trimmed = trim(line);
    if (trimmed.empty() || trimmed.starts_with("!"))
    {
      continue;
    }
    if (lineInVerificationIgnoredSection(i, ctx.prep->sections))
    {
      continue;
    }
    line = stripValidationComment(line);
    const auto bang = findInlineBangCommentIndex(line);
    const auto codeLine = bang < 0 ? line : line.substr(0, static_cast<std::size_t>(bang));
    for (const auto& site : sitesOnLine(ctx.stores, codeLine, &memo))
    {
      emitCountDiagnostic(ctx, i, codeLine, site);
    }
  }
}

auto callParamSitesOnLine(const ValidateStores& stores, std::string_view codeLine)
  -> std::vector<CallParamSite>
{
  return sitesOnLine(stores, codeLine);
}

auto missingCallParamInsertText(int missingCount) -> std::string
{
  std::string text;
  for (int i = 0; i < missingCount; ++i)
  {
    text += ",\"\"";
  }
  return text;
}

auto extraCallParamRemovalRange(std::string_view codeLine, const CallParamSite& site)
  -> std::optional<std::pair<int, int>>
{
  // Work in UTF-16 so indexes and JS `\s` match Node's string indexes.
  const auto line = utf8To16(codeLine);
  const auto open = static_cast<std::size_t>(
    static_cast<lsp::Uint>(utf16Offset(codeLine, static_cast<std::size_t>(site.openParenIndex))));
  const auto close = static_cast<std::size_t>(
    static_cast<lsp::Uint>(utf16Offset(codeLine, static_cast<std::size_t>(site.closeParenIndex))));
  if (close <= open || close > line.size())
  {
    return std::nullopt;
  }

  // findCallParamRangesInParen: argument starts; empty slots give no range.
  std::vector<std::size_t> starts;
  std::size_t i = open + 1;
  while (i < close)
  {
    while (i < close && (line[i] == u' ' || line[i] == u','))
    {
      ++i;
    }
    if (i >= close)
    {
      break;
    }
    starts.push_back(i);
    bool inQuotes = false;
    int depth = 0;
    while (i < close)
    {
      const auto ch = line[i];
      if (ch == u'"' && depth == 0)
      {
        inQuotes = !inQuotes;
      }
      else if (!inQuotes)
      {
        if (ch == u'(')
        {
          ++depth;
        }
        else if (ch == u')')
        {
          if (depth > 0)
          {
            --depth;
          }
        }
        else if (ch == u',' && depth == 0)
        {
          break;
        }
      }
      ++i;
    }
  }

  const auto firstExtra = static_cast<std::size_t>(site.expected) + 1;
  if (site.expected < 0 || starts.size() <= firstExtra)
  {
    return std::nullopt;
  }
  auto deleteStart = starts[firstExtra];
  const auto floor = open + 1;
  while (deleteStart > floor && isJsSpace(line[deleteStart - 1]))
  {
    --deleteStart;
  }
  if (deleteStart > floor && line[deleteStart - 1] == u',')
  {
    --deleteStart;
  }
  while (deleteStart > floor && isJsSpace(line[deleteStart - 1]))
  {
    --deleteStart;
  }
  return std::pair<int, int>{static_cast<int>(deleteStart), static_cast<int>(close)};
}

auto callParamWorkspaceEdits(
  const ValidateStores& stores,
  std::string_view content,
  const std::function<void()>& pollCancel) -> std::vector<CallParamEdit>
{
  std::vector<CallParamEdit> edits;
  int lineIndex = 0;
  std::size_t lineStart = 0;
  while (lineStart <= content.size())
  {
    if (pollCancel)
    {
      pollCancel();
    }
    const auto newline = content.find('\n', lineStart);
    const auto lineEnd = newline == std::string_view::npos ? content.size() : newline;
    const auto stripped = stripValidationComment(content.substr(lineStart, lineEnd - lineStart));
    const auto bang = findInlineBangCommentIndex(stripped);
    const auto codeLine = bang < 0
      ? std::string_view(stripped)
      : std::string_view(stripped).substr(0, static_cast<std::size_t>(bang));

    for (const auto& site : sitesOnLine(stores, codeLine))
    {
      if (site.expected > site.argCount)
      {
        const auto at = static_cast<int>(
          static_cast<lsp::Uint>(utf16Offset(codeLine, static_cast<std::size_t>(site.closeParenIndex))));
        edits.push_back(CallParamEdit{
          .line = lineIndex,
          .startCharacter = at,
          .endCharacter = at,
          .newText = missingCallParamInsertText(site.expected - site.argCount),
        });
      }
      else if (const auto removal = extraCallParamRemovalRange(codeLine, site))
      {
        edits.push_back(CallParamEdit{
          .line = lineIndex,
          .startCharacter = removal->first,
          .endCharacter = removal->second,
          .newText = {},
        });
      }
    }

    if (newline == std::string_view::npos)
    {
      break;
    }
    lineStart = newline + 1;
    ++lineIndex;
  }

  std::stable_sort(edits.begin(), edits.end(), [](const CallParamEdit& a, const CallParamEdit& b) {
    if (a.line != b.line)
    {
      return a.line > b.line;
    }
    return a.startCharacter > b.startCharacter;
  });
  return edits;
}

} // namespace vertexadt
