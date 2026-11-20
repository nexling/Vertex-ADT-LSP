#include "command_parse.h"

#include <cctype>
#include <regex>
#include <utility>

#include "text_util.h"

namespace vertexadt {
namespace {

bool isAsciiSpace(unsigned char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

bool isMathOperator(char ch)
{
  return ch == '+' || ch == '-' || ch == '*' || ch == '/';
}

bool isStartOfNewParam(std::string_view text)
{
  return !text.empty() && (std::isalpha(static_cast<unsigned char>(text[0])) || text[0] == '_'
    || text[0] == '"');
}

bool isEndOfParam(std::string_view text)
{
  if (text.empty())
  {
    return false;
  }
  const auto ch = text.back();
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == ')' || ch == ']'
    || ch == '"';
}

} // namespace

int findMatchingParenIndex(std::string_view text, int openParenIndex)
{
  if (openParenIndex < 0 || static_cast<std::size_t>(openParenIndex) >= text.size()
      || text[static_cast<std::size_t>(openParenIndex)] != '(')
  {
    return -1;
  }

  int parenCount = 1;
  bool inQuote = false;
  for (std::size_t i = static_cast<std::size_t>(openParenIndex) + 1; i < text.size(); ++i)
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
        return static_cast<int>(i);
      }
    }
  }
  return -1;
}

auto parseParameters(std::string_view paramString) -> std::vector<std::string>
{
  const auto trimmedAll = trim(paramString);
  if (trimmedAll.empty())
  {
    return {};
  }

  static const std::regex concatRe{
    R"("[^"]*"(?:\s*&\s*[a-zA-Z][a-zA-Z0-9_]*\$?)+|[a-zA-Z][a-zA-Z0-9_]*\$?(?:\s*&\s*"[^"]*")+|"[^"]*"(?:\s*&\s*"[^"]*")+)"};
  std::string processed;
  std::vector<std::string> concatenations;
  const std::string original{paramString};
  auto searchStart = original.cbegin();
  std::smatch concatMatch;
  while (std::regex_search(searchStart, original.cend(), concatMatch, concatRe))
  {
    processed.append(searchStart, concatMatch[0].first);
    concatenations.push_back(concatMatch[0].str());
    processed += "__CONCAT_" + std::to_string(concatenations.size() - 1) + "__";
    searchStart = concatMatch[0].second;
  }
  processed.append(searchStart, original.cend());

  std::vector<std::string> params;
  std::string current;
  bool inQuotes = false;
  int parenDepth = 0;
  int bracketDepth = 0;
  bool lastWasCommaSeparator = false;

  for (std::size_t i = 0; i < processed.size(); ++i)
  {
    const auto ch = processed[i];
    if (!inQuotes && ch == '"')
    {
      inQuotes = true;
      lastWasCommaSeparator = false;
      current += ch;
    }
    else if (inQuotes && ch == '"')
    {
      inQuotes = false;
      lastWasCommaSeparator = false;
      current += ch;
    }
    else if (inQuotes)
    {
      lastWasCommaSeparator = false;
      current += ch;
    }
    else if (ch == '(')
    {
      ++parenDepth;
      lastWasCommaSeparator = false;
      current += ch;
    }
    else if (ch == ')')
    {
      --parenDepth;
      lastWasCommaSeparator = false;
      current += ch;
    }
    else if (ch == '[')
    {
      ++bracketDepth;
      lastWasCommaSeparator = false;
      current += ch;
    }
    else if (ch == ']')
    {
      if (bracketDepth > 0)
      {
        --bracketDepth;
      }
      lastWasCommaSeparator = false;
      current += ch;
    }
    else if (ch == ',' && parenDepth == 0 && bracketDepth == 0)
    {
      const auto trimmedCurrent = trim(current);
      const auto rest = std::string_view(processed).substr(i + 1);
      char nextNonSpace = 0;
      for (const auto next : rest)
      {
        if (!isAsciiSpace(static_cast<unsigned char>(next)))
        {
          nextNonSpace = next;
          break;
        }
      }
      const char prevNonSpace = current.empty() ? 0 : current.back();
      const auto restTrimmed = trim(rest);
      if (nextNonSpace && prevNonSpace && isMathOperator(prevNonSpace)
          && !isStartOfNewParam(restTrimmed))
      {
        lastWasCommaSeparator = false;
        current += ch;
      }
      else if (nextNonSpace && isMathOperator(nextNonSpace) && !isEndOfParam(trimmedCurrent))
      {
        lastWasCommaSeparator = false;
        current += ch;
      }
      else
      {
        params.emplace_back(trimmedCurrent);
        current.clear();
        lastWasCommaSeparator = true;
      }
    }
    else
    {
      if (!isAsciiSpace(static_cast<unsigned char>(ch)))
      {
        lastWasCommaSeparator = false;
      }
      current += ch;
    }
  }

  if (!trim(current).empty())
  {
    params.emplace_back(std::string(trim(current)));
  }
  else if (lastWasCommaSeparator)
  {
    params.emplace_back();
  }

  static const std::regex restoreRe{R"(__CONCAT_(\d+)__)"};
  for (auto& param : params)
  {
    std::string restored;
    auto start = param.cbegin();
    std::smatch restoreMatch;
    while (std::regex_search(start, param.cend(), restoreMatch, restoreRe))
    {
      restored.append(start, restoreMatch[0].first);
      const auto index = std::stoi(restoreMatch[1].str());
      if (index >= 0 && static_cast<std::size_t>(index) < concatenations.size())
      {
        restored += concatenations[static_cast<std::size_t>(index)];
      }
      start = restoreMatch[0].second;
    }
    restored.append(start, param.cend());
    param = std::move(restored);
  }
  return params;
}

auto parseCommand(std::string_view command) -> std::optional<ParsedCommand>
{
  auto text = std::string(trim(command));
  static const std::regex assignmentRe{
    R"(^([a-zA-Z][a-zA-Z0-9_]*\$?(?:\([^)]*\))?)\s*=\s*(.+)$)"};
  std::smatch assignmentMatch;
  std::string commandPart = text;
  if (std::regex_match(text, assignmentMatch, assignmentRe))
  {
    commandPart = assignmentMatch[2].str();
  }

  static const std::regex funcRe{R"(^(\w+)\s*\()"};
  std::smatch funcMatch;
  if (!std::regex_search(commandPart, funcMatch, funcRe) || funcMatch.position() != 0)
  {
    commandPart = text;
    if (!std::regex_search(commandPart, funcMatch, funcRe) || funcMatch.position() != 0)
    {
      return std::nullopt;
    }
  }

  const auto functionName = funcMatch[1].str();
  const auto startPos = static_cast<int>(funcMatch[0].length()) - 1;
  const auto insideParensEnd = findMatchingParenIndex(commandPart, startPos);
  if (insideParensEnd == -1)
  {
    return std::nullopt;
  }
  const auto insideParens = std::string_view(commandPart).substr(
    static_cast<std::size_t>(startPos) + 1,
    static_cast<std::size_t>(insideParensEnd - startPos - 1));
  const auto afterParens = trim(std::string_view(commandPart).substr(
    static_cast<std::size_t>(insideParensEnd) + 1));
  return ParsedCommand{
    .functionName = functionName,
    .insideParams = parseParameters(insideParens),
    .afterParams = afterParens.empty() ? std::vector<std::string>{} : parseParameters(afterParens),
  };
}

} // namespace vertexadt
