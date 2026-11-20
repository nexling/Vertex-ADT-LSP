#include "debug_dump.h"

#include "text_util.h"

#include <algorithm>
#include <regex>

namespace vertexadt {
namespace {

const std::regex& valueTailRe()
{
  static const std::regex re{R"re(^<(.+?)>\s*(?:_\w+_)?\s*$)re"};
  return re;
}

const std::regex& counterRe()
{
  static const std::regex re{R"re(^\d+/)re"};
  return re;
}

const std::regex& standaloneAssignRe()
{
  static const std::regex re{R"re(^\s*(\w+\$?)\s*=\s*)re"};
  return re;
}

const std::regex& ifHeaderRe()
{
  static const std::regex re{R"re(^(?:else\s+)?if\b)re", std::regex::icase};
  return re;
}

const std::regex& thenAssignRe()
{
  static const std::regex re{R"re(\bthen\s+(\w+\$?)\s*=\s*)re", std::regex::icase};
  return re;
}

const std::regex& booleanRe()
{
  static const std::regex re{R"re(^(true|false)$)re", std::regex::icase};
  return re;
}

const std::regex& funcCallRe()
{
  static const std::regex re{R"re(^\s*(\w+)\((\w+\$?)\)\s*(.*))re"};
  return re;
}

const std::regex& varNameRe()
{
  static const std::regex re{R"re(^\w+\$?$)re"};
  return re;
}

std::vector<std::string> splitComma(std::string_view text)
{
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= text.size())
  {
    const auto comma = text.find(',', start);
    if (comma == std::string_view::npos)
    {
      parts.emplace_back(trim(text.substr(start)));
      break;
    }
    parts.emplace_back(trim(text.substr(start, comma - start)));
    start = comma + 1;
  }
  return parts;
}

std::vector<std::string> splitValueGroups(std::string_view text)
{
  std::vector<std::string> groups;
  constexpr std::string_view kSep = " , ";
  std::size_t start = 0;
  while (start <= text.size())
  {
    const auto found = text.find(kSep, start);
    if (found == std::string_view::npos)
    {
      groups.emplace_back(trim(text.substr(start)));
      break;
    }
    groups.emplace_back(trim(text.substr(start, found - start)));
    start = found + kSep.size();
  }
  return groups;
}

std::string stripCr(std::string_view line)
{
  if (!line.empty() && line.back() == '\r')
  {
    line.remove_suffix(1);
  }
  return std::string(line);
}

} // namespace

auto parseLineAssignments(std::string_view lineText)
  -> std::unordered_map<std::string, std::string>
{
  std::unordered_map<std::string, std::string> assignments;
  const auto line = stripCr(lineText);
  const auto lastAngle = line.rfind('<');
  if (lastAngle == std::string::npos)
  {
    return assignments;
  }

  const auto tail = line.substr(lastAngle);
  std::smatch valueMatch;
  if (!std::regex_match(tail, valueMatch, valueTailRe()))
  {
    return assignments;
  }

  const auto rawValue = std::string(trim(valueMatch[1].str()));
  if (rawValue.empty() || std::regex_search(rawValue, counterRe()))
  {
    return assignments;
  }

  const auto codePart = std::string(trim(std::string_view(line).substr(0, lastAngle)));

  std::smatch assignMatch;
  if (std::regex_search(codePart, assignMatch, standaloneAssignRe()))
  {
    assignments[assignMatch[1].str()] = rawValue;
    return assignments;
  }

  if (std::regex_search(codePart, ifHeaderRe()))
  {
    std::string thenName;
    for (std::sregex_iterator it(codePart.begin(), codePart.end(), thenAssignRe()), last;
         it != last;
         ++it)
    {
      thenName = (*it)[1].str();
    }
    if (!thenName.empty())
    {
      if (std::regex_match(rawValue, booleanRe()))
      {
        return assignments;
      }
      assignments[thenName] = rawValue;
      return assignments;
    }
  }

  std::smatch funcMatch;
  if (std::regex_match(codePart, funcMatch, funcCallRe()))
  {
    const auto parenArg = funcMatch[2].str();
    const auto restRaw = std::string(trim(funcMatch[3].str()));
    if (!restRaw.empty())
    {
      const auto restParts = splitComma(restRaw);
      const auto allAreVarNames = std::all_of(
        restParts.begin(),
        restParts.end(),
        [](const std::string& part) { return std::regex_match(part, varNameRe()); });
      if (allAreVarNames)
      {
        const auto valueGroups = splitValueGroups(rawValue);
        if (restParts.size() == valueGroups.size() && restParts.size() > 1)
        {
          for (std::size_t i = 0; i < restParts.size(); ++i)
          {
            assignments[restParts[i]] = valueGroups[i];
          }
          return assignments;
        }
      }
    }
    assignments[parenArg] = rawValue;
  }

  return assignments;
}

auto findMostRecentDebugValue(
  std::string_view text,
  int hoverLine,
  std::string_view varName) -> std::optional<DebugAssignment>
{
  const auto lines = splitLines(text);
  std::optional<DebugAssignment> found;
  const auto last = std::min(static_cast<int>(lines.size()) - 1, hoverLine);
  for (int line = 0; line <= last; ++line)
  {
    auto assignments = parseLineAssignments(lines[static_cast<std::size_t>(line)]);
    auto it = assignments.find(std::string(varName));
    if (it != assignments.end())
    {
      found = DebugAssignment{.value = std::move(it->second), .line = line};
    }
  }
  return found;
}

} // namespace vertexadt
