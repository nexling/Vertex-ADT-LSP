#include "validate.h"

#include "catalog/catalog.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

bool isJsWordChar(unsigned char ch)
{
  return std::isalnum(ch) != 0 || ch == '_';
}

bool isAlpha(unsigned char ch)
{
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

bool isDeclBodyChar(unsigned char ch)
{
  return isJsWordChar(ch) || ch == '-';
}

bool isAsciiSpace(unsigned char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

bool wordBoundaryAt(std::string_view text, std::size_t index)
{
  const bool left = index > 0 && isJsWordChar(static_cast<unsigned char>(text[index - 1]));
  const bool right = index < text.size() && isJsWordChar(static_cast<unsigned char>(text[index]));
  return left != right;
}

bool startsWithKeyword(std::string_view text, std::string_view keyword)
{
  std::size_t i = 0;
  while (i < text.size() && isAsciiSpace(static_cast<unsigned char>(text[i])))
  {
    ++i;
  }
  if (i + keyword.size() > text.size())
  {
    return false;
  }
  for (std::size_t k = 0; k < keyword.size(); ++k)
  {
    const auto have = static_cast<unsigned char>(text[i + k]);
    const auto want = static_cast<unsigned char>(keyword[k]);
    if (std::tolower(have) != std::tolower(want))
    {
      return false;
    }
  }
  const auto after = i + keyword.size();
  if (after < text.size() && isJsWordChar(static_cast<unsigned char>(text[after])))
  {
    return false;
  }
  return true;
}

bool startsWithAndOr(std::string_view text)
{
  return startsWithKeyword(text, "and") || startsWithKeyword(text, "or");
}

bool startsWithAmpOrPipe(std::string_view text)
{
  const auto trimmed = trim(text);
  return trimmed.starts_with("&&") || trimmed.starts_with("||");
}

bool startsWithPendingContinuation(std::string_view text)
{
  return startsWithAndOr(text) || startsWithAmpOrPipe(text);
}

std::optional<std::size_t> findKeyword(std::string_view text, std::string_view keyword)
{
  if (keyword.empty() || text.size() < keyword.size())
  {
    return std::nullopt;
  }
  for (std::size_t i = 0; i + keyword.size() <= text.size(); ++i)
  {
    bool same = true;
    for (std::size_t k = 0; k < keyword.size(); ++k)
    {
      const auto have = static_cast<unsigned char>(text[i + k]);
      const auto want = static_cast<unsigned char>(keyword[k]);
      if (std::tolower(have) != std::tolower(want))
      {
        same = false;
        break;
      }
    }
    if (!same)
    {
      continue;
    }
    if (i > 0 && isJsWordChar(static_cast<unsigned char>(text[i - 1])))
    {
      continue;
    }
    const auto after = i + keyword.size();
    if (after < text.size() && isJsWordChar(static_cast<unsigned char>(text[after])))
    {
      continue;
    }
    return i;
  }
  return std::nullopt;
}

bool isKnownFunctionToken(std::string_view token)
{
  static const auto keys = [] {
    std::unordered_set<std::string> out;
    out.insert("call");
    for (const auto& entry : catalog::commandCatalog())
    {
      out.emplace(entry.name);
    }
    auto addName = [&](std::string_view name) { out.emplace(name); };
    for (const auto& row : catalog::returnTypeMap())
    {
      addName(row.name);
    }
    for (const auto& row : catalog::inputParamTypeMap())
    {
      addName(row.name);
    }
    for (const auto& row : catalog::paramTypeMap())
    {
      addName(row.command);
    }
    for (const auto& row : catalog::multiParamTypeMap())
    {
      addName(row.command);
    }
    return out;
  }();
  return keys.contains(toLowerAscii(token));
}

auto extractVariableReferencesFromExpression(std::string_view expression) -> std::vector<std::string>
{
  if (expression.empty())
  {
    return {};
  }

  std::string sanitized;
  sanitized.reserve(expression.size());
  char activeQuote = 0;
  for (const char ch : expression)
  {
    if (activeQuote)
    {
      if (ch == activeQuote)
      {
        activeQuote = 0;
      }
      sanitized.push_back(' ');
      continue;
    }
    if (ch == '"')
    {
      activeQuote = ch;
      sanitized.push_back(' ');
      continue;
    }
    sanitized.push_back(ch);
  }

  std::vector<std::string> references;
  std::unordered_set<std::string> seen;
  std::size_t i = 0;
  while (i < sanitized.size())
  {
    const auto lead = static_cast<unsigned char>(sanitized[i]);
    if (!isAlpha(lead))
    {
      ++i;
      continue;
    }
    std::size_t j = i + 1;
    while (j < sanitized.size() && isJsWordChar(static_cast<unsigned char>(sanitized[j])))
    {
      ++j;
    }
    if (j < sanitized.size() && sanitized[j] == '$')
    {
      ++j;
    }
    const auto name = sanitized.substr(i, j - i);
    std::size_t k = j;
    while (k < sanitized.size() && isAsciiSpace(static_cast<unsigned char>(sanitized[k])))
    {
      ++k;
    }
    if (k < sanitized.size() && sanitized[k] == '(' && isKnownFunctionToken(name))
    {
      i = j;
      continue;
    }
    if (seen.insert(name).second)
    {
      references.push_back(name);
    }
    i = j;
  }
  return references;
}

bool shouldSkipUnusedVariableReference(std::string_view variableName, std::string_view line)
{
  if (catalog::isKeywordLabel(variableName))
  {
    return true;
  }
  if (variableName == "SHEETORIG" || variableName == "AREAORIG")
  {
    return true;
  }
  const auto variableIndex = line.find(variableName);
  const auto firstHashIndex = line.find('#');
  const auto lastHashIndex = line.rfind('#');
  if (variableIndex != std::string_view::npos
      && firstHashIndex != std::string_view::npos
      && lastHashIndex != std::string_view::npos
      && firstHashIndex < variableIndex
      && variableIndex < lastHashIndex)
  {
    return true;
  }
  return false;
}

auto extractIfConditionFromLine(std::string_view line) -> std::optional<std::string>
{
  const auto trimmed = std::string(trim(stripValidationComment(line)));
  if (!startsWithKeyword(trimmed, "if"))
  {
    return std::nullopt;
  }
  const auto thenAt = findKeyword(trimmed, "then");
  if (!thenAt)
  {
    return std::nullopt;
  }
  std::size_t i = 0;
  while (i < trimmed.size() && isAsciiSpace(static_cast<unsigned char>(trimmed[i])))
  {
    ++i;
  }
  if (i + 2 > trimmed.size())
  {
    return std::nullopt;
  }
  if (std::tolower(static_cast<unsigned char>(trimmed[i])) != 'i'
      || std::tolower(static_cast<unsigned char>(trimmed[i + 1])) != 'f')
  {
    return std::nullopt;
  }
  std::size_t afterIf = i + 2;
  if (afterIf >= trimmed.size() || !isAsciiSpace(static_cast<unsigned char>(trimmed[afterIf])))
  {
    return std::nullopt;
  }
  while (afterIf < trimmed.size() && isAsciiSpace(static_cast<unsigned char>(trimmed[afterIf])))
  {
    ++afterIf;
  }
  if (afterIf > *thenAt)
  {
    return std::nullopt;
  }
  return std::string(trim(std::string_view(trimmed).substr(afterIf, *thenAt - afterIf)));
}

bool advancePendingIfHeaderState(std::string_view trimmedLine, bool currentPendingIfHeader)
{
  if (startsWithKeyword(trimmedLine, "endif") || startsWithKeyword(trimmedLine, "else"))
  {
    return false;
  }
  const auto hasIf = startsWithKeyword(trimmedLine, "if");
  const auto hasThen = findKeyword(trimmedLine, "then").has_value();
  if (hasIf && !hasThen)
  {
    return true;
  }
  if (hasThen && currentPendingIfHeader)
  {
    return false;
  }
  if (currentPendingIfHeader)
  {
    return startsWithPendingContinuation(trimmedLine);
  }
  return false;
}

void addIfConditionExpressionRefs(
  std::string_view conditionText,
  std::string_view line,
  std::unordered_set<std::string>& references)
{
  if (conditionText.empty())
  {
    return;
  }
  for (const auto& name : extractVariableReferencesFromExpression(conditionText))
  {
    if (!shouldSkipUnusedVariableReference(name, line))
    {
      references.insert(name);
    }
  }
}

auto collectIfConditionReferences(std::string_view line, bool isPendingIfHeader)
  -> std::unordered_set<std::string>
{
  std::unordered_set<std::string> references;
  const auto trimmed = std::string(trim(stripValidationComment(line)));
  if (trimmed.empty())
  {
    return references;
  }
  if (auto ifCondition = extractIfConditionFromLine(line))
  {
    addIfConditionExpressionRefs(*ifCondition, line, references);
    return references;
  }
  if (startsWithKeyword(trimmed, "if") && !findKeyword(trimmed, "then"))
  {
    std::size_t i = 0;
    while (i < trimmed.size() && isAsciiSpace(static_cast<unsigned char>(trimmed[i])))
    {
      ++i;
    }
    i += 2;
    while (i < trimmed.size() && isAsciiSpace(static_cast<unsigned char>(trimmed[i])))
    {
      ++i;
    }
    addIfConditionExpressionRefs(std::string_view(trimmed).substr(std::min(i, trimmed.size())), line, references);
    return references;
  }
  if (isPendingIfHeader && startsWithPendingContinuation(trimmed))
  {
    addIfConditionExpressionRefs(trimmed, line, references);
  }
  return references;
}

struct TextSpan
{
  std::size_t start = 0;
  std::size_t end = 0;
};

auto getIfConditionTextSpans(
  std::string_view splitline,
  std::string_view line,
  bool isPendingIfHeader) -> std::vector<TextSpan>
{
  std::vector<TextSpan> spans;
  const auto trimmed = std::string(trim(stripValidationComment(line)));
  if (trimmed.empty())
  {
    return spans;
  }
  if (auto ifCondition = extractIfConditionFromLine(line))
  {
    const auto conditionIndex = splitline.find(*ifCondition);
    if (conditionIndex != std::string_view::npos)
    {
      spans.push_back(TextSpan{
        .start = conditionIndex,
        .end = conditionIndex + ifCondition->size(),
      });
    }
    return spans;
  }
  if (startsWithKeyword(trimmed, "if") && !findKeyword(trimmed, "then"))
  {
    std::size_t i = 0;
    while (i < trimmed.size() && isAsciiSpace(static_cast<unsigned char>(trimmed[i])))
    {
      ++i;
    }
    i += 2;
    while (i < trimmed.size() && isAsciiSpace(static_cast<unsigned char>(trimmed[i])))
    {
      ++i;
    }
    const auto afterIf = std::string(std::string_view(trimmed).substr(std::min(i, trimmed.size())));
    const auto afterIfIndex = splitline.find(afterIf);
    if (afterIfIndex != std::string_view::npos)
    {
      spans.push_back(TextSpan{.start = afterIfIndex, .end = afterIfIndex + afterIf.size()});
    }
    return spans;
  }
  if (isPendingIfHeader && startsWithPendingContinuation(trimmed))
  {
    spans.push_back(TextSpan{.start = 0, .end = splitline.size()});
  }
  return spans;
}

bool isIndexInsideIfConditionSpans(std::size_t index, const std::vector<TextSpan>& spans)
{
  for (const auto& span : spans)
  {
    if (index >= span.start && index < span.end)
    {
      return true;
    }
  }
  return false;
}

auto parseDeclarationVariablesFromSplitLine(
  std::string_view splitline,
  std::string_view line,
  bool isPendingIfHeader) -> std::unordered_set<std::string>
{
  std::unordered_set<std::string> names;
  if (splitline.empty() || trim(splitline).starts_with("!"))
  {
    return names;
  }
  const auto conditionSpans = getIfConditionTextSpans(splitline, line, isPendingIfHeader);
  std::size_t i = 0;
  while (i < splitline.size())
  {
    const auto lead = static_cast<unsigned char>(splitline[i]);
    if (!isAlpha(lead) || !wordBoundaryAt(splitline, i))
    {
      ++i;
      continue;
    }
    std::size_t j = i + 1;
    while (j < splitline.size() && isDeclBodyChar(static_cast<unsigned char>(splitline[j])))
    {
      ++j;
    }
    bool found = false;
    for (std::size_t end = j; end > i; --end)
    {
      if (!wordBoundaryAt(splitline, end))
      {
        continue;
      }
      std::size_t k = end;
      while (k < splitline.size() && isAsciiSpace(static_cast<unsigned char>(splitline[k])))
      {
        ++k;
      }
      if (k < splitline.size() && splitline[k] == '=' && (k + 1 >= splitline.size() || splitline[k + 1] != '='))
      {
        const auto varName = std::string(splitline.substr(i, end - i));
        if (!catalog::isKeywordLabel(varName) && !isIndexInsideIfConditionSpans(i, conditionSpans))
        {
          names.insert(varName);
        }
        i = end;
        found = true;
        break;
      }
    }
    if (!found)
    {
      i = j;
    }
  }
  return names;
}

auto stripCallSegmentsForUnusedReferenceScan(std::string_view codeLine) -> std::string
{
  static const std::regex callRe{R"(call\s*\([^)]*\))", std::regex::icase};
  return std::regex_replace(std::string(codeLine), callRe, "call()");
}

auto collectReferencesFromValidationLine(
  std::string_view codeLine,
  std::string_view line,
  bool isPendingIfHeader) -> std::unordered_set<std::string>
{
  std::unordered_set<std::string> references;
  for (const auto& name : extractVariableReferencesFromExpression(
         stripCallSegmentsForUnusedReferenceScan(codeLine)))
  {
    if (!shouldSkipUnusedVariableReference(name, line))
    {
      references.insert(name);
    }
  }
  for (const auto& name : collectIfConditionReferences(line, isPendingIfHeader))
  {
    references.insert(name);
  }
  return references;
}

std::ptrdiff_t findMatchingParenIndex(std::string_view text, std::size_t openParenIndex)
{
  if (openParenIndex >= text.size() || text[openParenIndex] != '(')
  {
    return -1;
  }
  int parenCount = 1;
  char activeQuote = 0;
  for (std::size_t i = openParenIndex + 1; i < text.size(); ++i)
  {
    const auto ch = text[i];
    if (activeQuote)
    {
      if (ch == activeQuote)
      {
        activeQuote = 0;
      }
      continue;
    }
    if (ch == '"')
    {
      activeQuote = ch;
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

std::optional<std::pair<std::string, std::string>> matchCommandAssignment(std::string_view text)
{
  if (text.empty())
  {
    return std::nullopt;
  }
  const auto lead = static_cast<unsigned char>(text[0]);
  if (!isAlpha(lead))
  {
    return std::nullopt;
  }
  std::size_t i = 1;
  while (i < text.size() && isJsWordChar(static_cast<unsigned char>(text[i])))
  {
    ++i;
  }
  if (i < text.size() && text[i] == '$')
  {
    ++i;
  }
  if (i < text.size() && text[i] == '(')
  {
    const auto close = findMatchingParenIndex(text, i);
    if (close < 0)
    {
      return std::nullopt;
    }
    i = static_cast<std::size_t>(close) + 1;
  }
  std::size_t eq = i;
  while (eq < text.size() && isAsciiSpace(static_cast<unsigned char>(text[eq])))
  {
    ++eq;
  }
  if (eq >= text.size() || text[eq] != '=')
  {
    return std::nullopt;
  }
  std::size_t rhs = eq + 1;
  while (rhs < text.size() && isAsciiSpace(static_cast<unsigned char>(text[rhs])))
  {
    ++rhs;
  }
  if (rhs >= text.size())
  {
    return std::nullopt;
  }
  return std::make_pair(std::string(text.substr(0, i)), std::string(text.substr(rhs)));
}

auto parseParameters(std::string_view paramString) -> std::vector<std::string>
{
  const auto trimmed = trim(paramString);
  if (trimmed.empty())
  {
    return {};
  }
  std::vector<std::string> params;
  std::string current;
  bool inQuotes = false;
  int parenDepth = 0;
  int bracketDepth = 0;
  bool lastWasCommaSeparator = false;
  for (const char ch : trimmed)
  {
    if (!inQuotes && ch == '"')
    {
      inQuotes = true;
      lastWasCommaSeparator = false;
      current.push_back(ch);
    }
    else if (inQuotes && ch == '"')
    {
      inQuotes = false;
      lastWasCommaSeparator = false;
      current.push_back(ch);
    }
    else if (inQuotes)
    {
      lastWasCommaSeparator = false;
      current.push_back(ch);
    }
    else if (ch == '(')
    {
      ++parenDepth;
      lastWasCommaSeparator = false;
      current.push_back(ch);
    }
    else if (ch == ')')
    {
      --parenDepth;
      lastWasCommaSeparator = false;
      current.push_back(ch);
    }
    else if (ch == '[')
    {
      ++bracketDepth;
      lastWasCommaSeparator = false;
      current.push_back(ch);
    }
    else if (ch == ']')
    {
      if (bracketDepth > 0)
      {
        --bracketDepth;
      }
      lastWasCommaSeparator = false;
      current.push_back(ch);
    }
    else if (ch == ',' && parenDepth == 0 && bracketDepth == 0)
    {
      params.push_back(std::string(trim(current)));
      current.clear();
      lastWasCommaSeparator = true;
    }
    else
    {
      if (!isAsciiSpace(static_cast<unsigned char>(ch)))
      {
        lastWasCommaSeparator = false;
      }
      current.push_back(ch);
    }
  }
  if (!trim(current).empty())
  {
    params.push_back(std::string(trim(current)));
  }
  else if (lastWasCommaSeparator)
  {
    params.push_back({});
  }
  return params;
}

struct ParsedCommand
{
  std::string functionName;
  std::vector<std::string> insideParams;
};

std::optional<ParsedCommand> parseCommand(std::string_view command)
{
  auto text = std::string(trim(command));
  auto commandPart = text;
  if (auto assignment = matchCommandAssignment(text))
  {
    commandPart = assignment->second;
  }
  auto parseCall = [](std::string_view part) -> std::optional<ParsedCommand> {
    std::size_t i = 0;
    if (i >= part.size() || !isJsWordChar(static_cast<unsigned char>(part[i])))
    {
      return std::nullopt;
    }
    std::size_t j = i + 1;
    while (j < part.size() && isJsWordChar(static_cast<unsigned char>(part[j])))
    {
      ++j;
    }
    std::size_t k = j;
    while (k < part.size() && isAsciiSpace(static_cast<unsigned char>(part[k])))
    {
      ++k;
    }
    if (k >= part.size() || part[k] != '(')
    {
      return std::nullopt;
    }
    const auto close = findMatchingParenIndex(part, k);
    if (close < 0)
    {
      return std::nullopt;
    }
    ParsedCommand parsed;
    parsed.functionName = std::string(part.substr(i, j - i));
    parsed.insideParams = parseParameters(part.substr(k + 1, static_cast<std::size_t>(close) - k - 1));
    return parsed;
  };
  if (auto parsed = parseCall(commandPart))
  {
    return parsed;
  }
  return parseCall(text);
}

bool looksLikeQuotedString(std::string_view part)
{
  static const std::regex dq{R"("[^"]*")"};
  static const std::regex sq{R"('[^']*')"};
  std::cmatch match;
  return std::regex_search(part.data(), part.data() + part.size(), match, dq)
    || std::regex_search(part.data(), part.data() + part.size(), match, sq);
}

bool isValidStringVariable(std::string_view expr)
{
  if (expr.empty() || expr.size() > 16 || expr.find('$') == std::string_view::npos)
  {
    return false;
  }
  static const std::regex stringVar{R"(^[a-zA-Z][a-zA-Z0-9_]*\$$)"};
  static const std::regex variable{R"([a-zA-Z][a-zA-Z0-9_]*\$?(?:\([A-Za-z0-9]*\))?)"};
  std::cmatch match;
  if (std::regex_search(expr.data(), expr.data() + expr.size(), match, variable) && expr.find('$') != std::string_view::npos)
  {
    return true;
  }
  return std::regex_match(expr.begin(), expr.end(), stringVar);
}

bool isValidCallPathParameter(std::string_view expr)
{
  if (expr.empty())
  {
    return false;
  }
  static const std::regex concatSplit{R"((\s*&\s*))"};
  const std::string text(expr);
  std::vector<std::string> parts;
  std::size_t last = 0;
  for (std::sregex_iterator it(text.begin(), text.end(), concatSplit), end; it != end; ++it)
  {
    parts.push_back(text.substr(last, static_cast<std::size_t>(it->position()) - last));
    parts.push_back(it->str());
    last = static_cast<std::size_t>(it->position() + it->length());
  }
  parts.push_back(text.substr(last));
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
    else if (std::string(trim(part)) != "&")
    {
      return false;
    }
  }
  return true;
}

auto findParenthesizedCommandSegments(std::string_view line, std::string_view commandName)
  -> std::vector<std::string>
{
  std::vector<std::string> segments;
  const auto needle = toLowerAscii(commandName);
  std::size_t i = 0;
  while (i < line.size())
  {
    if (!wordBoundaryAt(line, i))
    {
      ++i;
      continue;
    }
    if (i + needle.size() > line.size())
    {
      break;
    }
    bool same = true;
    for (std::size_t k = 0; k < needle.size(); ++k)
    {
      if (std::tolower(static_cast<unsigned char>(line[i + k])) != static_cast<unsigned char>(needle[k]))
      {
        same = false;
        break;
      }
    }
    if (!same)
    {
      ++i;
      continue;
    }
    const auto afterName = i + needle.size();
    if (afterName < line.size() && isJsWordChar(static_cast<unsigned char>(line[afterName])))
    {
      ++i;
      continue;
    }
    std::size_t j = afterName;
    while (j < line.size() && isAsciiSpace(static_cast<unsigned char>(line[j])))
    {
      ++j;
    }
    if (j >= line.size() || line[j] != '(')
    {
      ++i;
      continue;
    }
    if (isInsideQuotes(line, i))
    {
      ++i;
      continue;
    }
    const auto close = findMatchingParenIndex(line, j);
    if (close < 0)
    {
      ++i;
      continue;
    }
    const auto endIndex = static_cast<std::size_t>(close) + 1;
    std::size_t startIndex = i;
    const auto prefix = line.substr(0, i);
    static const std::regex assignRe{R"(([A-Za-z][A-Za-z0-9_]*\$?)\s*=\s*$)"};
    std::cmatch match;
    if (std::regex_search(prefix.data(), prefix.data() + prefix.size(), match, assignRe))
    {
      startIndex = prefix.size() - match.length();
    }
    segments.push_back(std::string(trim(line.substr(startIndex, endIndex - startIndex))));
    i = endIndex;
  }
  return segments;
}

auto getCallCommandVariableReferences(std::string_view line) -> std::vector<std::string>
{
  std::unordered_set<std::string> references;
  for (const auto& segment : findParenthesizedCommandSegments(line, "call"))
  {
    const auto parsed = parseCommand(segment);
    if (!parsed || toLowerAscii(parsed->functionName) != "call")
    {
      continue;
    }
    for (std::size_t index = 0; index < parsed->insideParams.size(); ++index)
    {
      if (index == 0 && !isValidCallPathParameter(parsed->insideParams[index]))
      {
        continue;
      }
      for (const auto& name : extractVariableReferencesFromExpression(parsed->insideParams[index]))
      {
        references.insert(name);
      }
    }
  }
  return {references.begin(), references.end()};
}

struct IncludedNames
{
  std::vector<std::string> variables;
  std::vector<std::string> stringVariables;
};

bool alreadyHasName(const std::vector<std::string>& names, std::string_view name)
{
  return std::find(names.begin(), names.end(), name) != names.end();
}

auto extractVariablesFromIncludedFile(std::string_view fileContent) -> IncludedNames
{
  IncludedNames extracted;
  for (const auto raw : splitLines(fileContent))
  {
    auto line = std::string(raw);
    const auto trimmed = trim(line);
    if (trimmed.starts_with("!") || trimmed.starts_with("#"))
    {
      continue;
    }
    line = stripValidationComment(line);

    std::size_t i = 0;
    while (i < line.size())
    {
      if (!isAlpha(static_cast<unsigned char>(line[i])) || !wordBoundaryAt(line, i))
      {
        ++i;
        continue;
      }
      std::size_t j = i + 1;
      while (j < line.size() && isDeclBodyChar(static_cast<unsigned char>(line[j])))
      {
        ++j;
      }
      if (j < line.size() && line[j] == '$')
      {
        std::size_t k = j + 1;
        while (k < line.size() && isAsciiSpace(static_cast<unsigned char>(line[k])))
        {
          ++k;
        }
        if (k < line.size() && line[k] == '=')
        {
          const auto varName = line.substr(i, j - i) + "$";
          if (!alreadyHasName(extracted.stringVariables, varName))
          {
            extracted.stringVariables.push_back(varName);
          }
          i = k;
          continue;
        }
      }

      if (wordBoundaryAt(line, j))
      {
        std::size_t k = j;
        if (k < line.size() && line[k] == '(')
        {
          const auto close = line.find(')', k);
          if (close == std::string::npos)
          {
            i = j;
            continue;
          }
          k = close + 1;
        }
        if (k < line.size())
        {
          std::size_t n = k;
          while (n < line.size() && isAsciiSpace(static_cast<unsigned char>(line[n])))
          {
            ++n;
          }
          if (n < line.size() && line[n] == '=')
          {
            const auto varName = line.substr(i, j - i);
            if (!alreadyHasName(extracted.variables, varName))
            {
              extracted.variables.push_back(varName);
            }
            i = n;
            continue;
          }
        }
      }
      i = j;
    }

    i = 0;
    while (i < line.size())
    {
      if (!(i + 3 <= line.size()
            && line[i] == 's' && line[i + 1] == 'e' && line[i + 2] == 't'
            && wordBoundaryAt(line, i)
            && (i + 3 >= line.size() || !isJsWordChar(static_cast<unsigned char>(line[i + 3])))))
      {
        ++i;
        continue;
      }
      std::size_t k = i + 3;
      if (k >= line.size() || !isAsciiSpace(static_cast<unsigned char>(line[k])))
      {
        ++i;
        continue;
      }
      while (k < line.size() && isAsciiSpace(static_cast<unsigned char>(line[k])))
      {
        ++k;
      }
      if (k >= line.size() || !isAlpha(static_cast<unsigned char>(line[k])))
      {
        ++i;
        continue;
      }
      std::size_t n = k + 1;
      while (n < line.size() && isDeclBodyChar(static_cast<unsigned char>(line[n])))
      {
        ++n;
      }
      if (wordBoundaryAt(line, n))
      {
        const auto varName = line.substr(k, n - k);
        if (!alreadyHasName(extracted.variables, varName))
        {
          extracted.variables.push_back(varName);
        }
      }
      i = n;
    }
  }
  return extracted;
}

auto makeUnusedVariableKey(std::string_view sectionName, std::string_view varName) -> std::string
{
  return std::string(sectionName) + "::" + std::string(varName);
}

auto scanFileForVariableReferences(
  std::string_view fileContent,
  const std::unordered_set<std::string>& targetNames,
  const std::optional<std::string>& restrictToSection) -> std::unordered_set<std::string>
{
  std::unordered_set<std::string> found;
  if (fileContent.empty() || targetNames.empty())
  {
    return found;
  }
  const auto lines = splitLines(fileContent);
  const auto sections = parseFileSections(lines);
  bool pendingIfHeader = false;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto originalTrimmed = trim(lines[static_cast<std::size_t>(i)]);
    if (originalTrimmed.starts_with("!") || originalTrimmed.starts_with("##") || originalTrimmed.empty())
    {
      continue;
    }
    if (restrictToSection && sectionNameForLine(i, sections) != *restrictToSection)
    {
      continue;
    }
    auto line = std::string(stripValidationComment(lines[static_cast<std::size_t>(i)]));
    const auto bangCommentIndex = findInlineBangCommentIndex(line);
    const auto codeLine = bangCommentIndex < 0
      ? std::string_view(line)
      : std::string_view(line).substr(0, static_cast<std::size_t>(bangCommentIndex));
    const auto trimmedLine = std::string(trim(stripValidationComment(line)));
    const auto refs = collectReferencesFromValidationLine(codeLine, line, pendingIfHeader);
    for (const auto& name : refs)
    {
      if (targetNames.contains(name))
      {
        found.insert(name);
      }
    }
    pendingIfHeader = advancePendingIfHeaderState(trimmedLine, pendingIfHeader);
  }
  return found;
}

std::optional<std::string> loadLinkedFileText(ValidateContext& ctx, std::string_view inputPath)
{
  if (inputPath.empty())
  {
    return std::nullopt;
  }
  std::optional<std::filesystem::path> resolved;
  if (ctx.stores.layout)
  {
    resolved = ctx.stores.layout->resolveVertexFilePath(inputPath);
  }
  if (!resolved)
  {
    std::filesystem::path asPath{std::string(inputPath)};
    if (asPath.is_absolute())
    {
      resolved = std::move(asPath);
    }
  }
  if (!resolved)
  {
    return std::nullopt;
  }
  if (ctx.stores.textForPath)
  {
    return ctx.stores.textForPath(*resolved);
  }
  if (ctx.stores.linkedFiles)
  {
    if (auto snapshot = ctx.stores.linkedFiles->get(normalizeFsPath(resolved->string())))
    {
      return snapshot->text;
    }
  }
  return std::nullopt;
}

std::shared_ptr<const IndexEntry> thisIndexEntry(ValidateContext& ctx)
{
  if (!ctx.stores.index)
  {
    return {};
  }
  if (ctx.stores.layout)
  {
    if (auto identity = ctx.stores.layout->overlayForUri(ctx.prep->snapshot.uri))
    {
      if (auto entry = ctx.stores.index->entry(identity->key))
      {
        return entry;
      }
    }
  }
  return ctx.stores.index->entryByAbsPath(pathFromUri(ctx.prep->snapshot.uri));
}

lsp::Position positionAt(std::string_view text, std::size_t offset)
{
  if (offset > text.size())
  {
    offset = text.size();
  }
  lsp::Uint line = 0;
  lsp::Uint character = 0;
  std::size_t i = 0;
  while (i < offset)
  {
    const auto lead = static_cast<unsigned char>(text[i]);
    if (lead == '\n')
    {
      ++line;
      character = 0;
      ++i;
      continue;
    }
    if (lead < 0x80)
    {
      ++character;
      ++i;
      continue;
    }
    int extra = 0;
    unsigned cp = 0;
    if ((lead & 0xE0) == 0xC0)
    {
      extra = 1;
      cp = lead & 0x1F;
    }
    else if ((lead & 0xF0) == 0xE0)
    {
      extra = 2;
      cp = lead & 0x0F;
    }
    else if ((lead & 0xF8) == 0xF0)
    {
      extra = 3;
      cp = lead & 0x07;
    }
    else
    {
      ++character;
      ++i;
      continue;
    }
    if (i + static_cast<std::size_t>(extra) >= text.size())
    {
      ++character;
      break;
    }
    bool ok = true;
    for (int n = 0; n < extra; ++n)
    {
      const auto cont = static_cast<unsigned char>(text[i + 1 + n]);
      if ((cont & 0xC0) != 0x80)
      {
        ok = false;
        break;
      }
      cp = (cp << 6) | (cont & 0x3F);
    }
    if (!ok)
    {
      ++character;
      ++i;
      continue;
    }
    character += cp > 0xFFFFu ? 2u : 1u;
    i += static_cast<std::size_t>(extra) + 1;
  }
  return lsp::Position{.line = line, .character = character};
}

lsp::Diagnostic reportUnused(
  ValidateContext& ctx,
  int lineIndex,
  std::string_view lineText,
  std::string_view inner)
{
  std::size_t offset = 0;
  for (int i = 0; i < lineIndex && i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    offset += ctx.prep->lines[static_cast<std::size_t>(i)].size();
    if (offset < ctx.prep->snapshot.text.size() && ctx.prep->snapshot.text[offset] == '\n')
    {
      ++offset;
    }
  }
  const auto start = positionAt(ctx.prep->snapshot.text, offset + 1);
  const auto end = positionAt(ctx.prep->snapshot.text, offset + lineText.size());
  const auto range = lsp::Range{.start = start, .end = end};
  std::string wrapped;
  wrapped.reserve(lineText.size() + inner.size() + 16);
  wrapped.append(lineText);
  wrapped.append(" has an error: ");
  wrapped.append(inner);
  lsp::Diagnostic diagnostic;
  diagnostic.range = range;
  diagnostic.severity = lsp::DiagnosticSeverity::Information;
  diagnostic.message = std::move(wrapped);
  diagnostic.source = std::string("ADT-Syntax");
  diagnostic.relatedInformation = std::vector<lsp::DiagnosticRelatedInformation>{
    lsp::DiagnosticRelatedInformation{
      .location = lsp::Location{.uri = ctx.prep->snapshot.uri, .range = range},
      .message = std::string(inner),
    },
  };
  return diagnostic;
}

struct UnusedDecl
{
  std::string sectionName;
  std::string varName;
  int lineIndex = 0;
  std::string lineText;
};

} // namespace

void familyUnusedVariables(ValidateContext& ctx)
{
  if (ctx.settings.noteUnusedVariables != true)
  {
    return;
  }

  ctx.pollCancel();

  std::vector<UnusedDecl> declarations;
  std::unordered_map<std::string, std::size_t> declarationIndex;
  std::unordered_map<std::string, int> usageCounts;

  auto ensureDeclaration =
    [&](std::string_view sectionName, std::string_view varName, int lineIndex, std::string_view lineText) {
      const auto key = makeUnusedVariableKey(sectionName, varName);
      if (declarationIndex.contains(key))
      {
        return;
      }
      declarationIndex[key] = declarations.size();
      declarations.push_back(UnusedDecl{
        .sectionName = std::string(sectionName),
        .varName = std::string(varName),
        .lineIndex = lineIndex,
        .lineText = std::string(lineText),
      });
      usageCounts[key] = 0;
    };

  auto incrementUsage = [&](std::string_view sectionName, std::string_view varName) {
    const auto key = makeUnusedVariableKey(sectionName, varName);
    if (declarationIndex.contains(key))
    {
      ++usageCounts[key];
    }
  };

  const auto& ifHeaders = ifHeaderLinesFor(ctx);
  bool pendingIfHeader = false;
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    auto line = std::string(ctx.prep->lines[static_cast<std::size_t>(i)]);
    const auto originalTrimmed = trim(line);
    if (originalTrimmed.starts_with("!"))
    {
      continue;
    }
    if (originalTrimmed.starts_with("##") && !originalTrimmed.starts_with("##included_from"))
    {
      continue;
    }
    if (originalTrimmed.empty())
    {
      continue;
    }
    line = stripValidationComment(line);
    const auto sectionName = sectionNameForLine(i, ctx.prep->sections);
    if (lineInVerificationIgnoredSection(i, ctx.prep->sections))
    {
      pendingIfHeader = advancePendingIfHeaderState(trim(stripValidationComment(line)), pendingIfHeader);
      continue;
    }
    const auto bangCommentIndex = findInlineBangCommentIndex(line);
    const auto codeLine = bangCommentIndex < 0
      ? std::string_view(line)
      : std::string_view(line).substr(0, static_cast<std::size_t>(bangCommentIndex));
    const auto trimmedLine = std::string(trim(stripValidationComment(line)));
    // A `\`-continued if header line is condition up to `then`; only the
    // statement after it can declare (ADR 0014).
    std::string_view declarationText = line;
    if (ifHeaders.startByContinuation.contains(i))
    {
      const auto thenAt = findKeyword(line, "then");
      declarationText = thenAt ? std::string_view(line).substr(*thenAt + 4) : std::string_view{};
    }
    std::unordered_set<std::string> declarationNamesOnLine;
    std::size_t fragStart = 0;
    while (fragStart <= declarationText.size())
    {
      const auto semi = declarationText.find(';', fragStart);
      const auto fragment = semi == std::string::npos
        ? declarationText.substr(fragStart)
        : declarationText.substr(fragStart, semi - fragStart);
      fragStart = semi == std::string::npos ? declarationText.size() + 1 : semi + 1;
      for (const auto& varName : parseDeclarationVariablesFromSplitLine(fragment, line, pendingIfHeader))
      {
        ensureDeclaration(sectionName, varName, i, line);
        declarationNamesOnLine.insert(varName);
      }
    }
    auto refs = collectReferencesFromValidationLine(codeLine, line, pendingIfHeader);
    for (const auto& declName : declarationNamesOnLine)
    {
      refs.erase(declName);
    }
    for (const auto& varName : refs)
    {
      incrementUsage(sectionName, varName);
    }
    pendingIfHeader = advancePendingIfHeaderState(trimmedLine, pendingIfHeader);
  }

  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    const auto trimmed = trim(ctx.prep->lines[static_cast<std::size_t>(i)]);
    if (!trimmed.starts_with("##included_from"))
    {
      continue;
    }
    // Matched on the trimmed line: Node's untrimmed `(.+?)$` never matches a
    // CRLF line (`.` stops at `\r`), so Node drops the directive in CRLF files.
    // Spec tickets 30/31 say the directive contributes regardless (ticket 65).
    static const std::regex includedRe{R"(##included_from\s+(.+?)$)", std::regex::icase};
    const auto raw = std::string(ctx.prep->lines[static_cast<std::size_t>(i)]);
    const auto trimmedOwned = std::string(trimmed);
    std::smatch match;
    if (!std::regex_search(trimmedOwned, match, includedRe))
    {
      continue;
    }
    const auto sectionName = sectionNameForLine(i, ctx.prep->sections);
    const auto remainder = std::string(trim(match[1].str()));
    auto includedText = loadLinkedFileText(ctx, remainder);
    if (!includedText)
    {
      continue;
    }
    const auto includedVars = extractVariablesFromIncludedFile(*includedText);
    std::unordered_set<std::string> includedNames;
    for (const auto& name : includedVars.variables)
    {
      includedNames.insert(name);
      ensureDeclaration(sectionName, name, i, raw);
    }
    for (const auto& name : includedVars.stringVariables)
    {
      includedNames.insert(name);
      ensureDeclaration(sectionName, name, i, raw);
    }
    const auto foundInIncluded = scanFileForVariableReferences(*includedText, includedNames, std::nullopt);
    for (const auto& varName : foundInIncluded)
    {
      incrementUsage(sectionName, varName);
    }
  }

  std::shared_ptr<const IndexEntry> storedEntry = thisIndexEntry(ctx);
  IndexEntry parsedEntry;
  const IndexEntry* entry = storedEntry.get();
  if (!entry && ctx.stores.layout)
  {
    if (auto identity = ctx.stores.layout->overlayForUri(ctx.prep->snapshot.uri))
    {
      parsedEntry = parseIndexEntry(ctx.prep->snapshot.text, *identity, *ctx.stores.layout);
      entry = &parsedEntry;
    }
  }
  if (entry)
  {
    for (const auto& link : entry->includeLinks)
    {
      const auto sourceSection = link.sourceSection;
      const auto targetText = loadLinkedFileText(ctx, link.target);
      if (!targetText)
      {
        continue;
      }
      std::optional<std::string> restrictToSection;
      if (!link.targetSection.empty())
      {
        restrictToSection = link.targetSection;
      }
      for (const auto& decl : declarations)
      {
        if (decl.sectionName != sourceSection)
        {
          continue;
        }
        const auto found = scanFileForVariableReferences(
          *targetText, std::unordered_set<std::string>{decl.varName}, restrictToSection);
        if (found.contains(decl.varName))
        {
          const auto key = makeUnusedVariableKey(decl.sectionName, decl.varName);
          ++usageCounts[key];
        }
      }
    }

    if (entry->commonIndexes || entry->commonPoints)
    {
      for (const auto& link : entry->callLinks)
      {
        const auto sourceSection = link.sourceSection;
        if (link.line < 0 || link.line >= static_cast<int>(ctx.prep->lines.size()))
        {
          continue;
        }
        auto line = std::string(stripValidationComment(ctx.prep->lines[static_cast<std::size_t>(link.line)]));
        const auto bangCommentIndex = findInlineBangCommentIndex(line);
        const auto codeLine = bangCommentIndex < 0
          ? std::string_view(line)
          : std::string_view(line).substr(0, static_cast<std::size_t>(bangCommentIndex));
        const auto callArgs = getCallCommandVariableReferences(codeLine);
        std::vector<std::string> declaredArgs;
        for (const auto& argName : callArgs)
        {
          if (declarationIndex.contains(makeUnusedVariableKey(sourceSection, argName)))
          {
            declaredArgs.push_back(argName);
          }
        }
        if (declaredArgs.empty())
        {
          continue;
        }
        const auto targetText = loadLinkedFileText(ctx, link.target);
        if (!targetText)
        {
          continue;
        }
        std::unordered_set<std::string> wanted{declaredArgs.begin(), declaredArgs.end()};
        const auto foundInCallee = scanFileForVariableReferences(*targetText, wanted, std::nullopt);
        for (const auto& argName : declaredArgs)
        {
          if (foundInCallee.contains(argName))
          {
            incrementUsage(sourceSection, argName);
          }
        }
      }
    }
  }

  for (const auto& decl : declarations)
  {
    const auto key = makeUnusedVariableKey(decl.sectionName, decl.varName);
    if (usageCounts[key] != 0)
    {
      continue;
    }
    const auto inner = "Variable " + decl.varName + " is declared but not used.";
    ctx.append(reportUnused(ctx, decl.lineIndex, decl.lineText, inner));
  }
}

} // namespace vertexadt
