#include "validate.h"

#include "parent_context.h"

#include "catalog/catalog.h"
#include "regex_classes.h"
#include "utf16_text.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <regex>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

const std::unordered_set<std::string_view> kVectorTypes{
  "pvector",
  "dvector",
  "xvector",
  "yvector",
  "zvector",
  "nvector",
};

// Hand matchers for the type timeline's ECMAScript `std::regex` searches.
// Each returns exactly the regex's first match (leftmost start, then the
// regex's own backtracking order); classes are in regex_classes.h. `\w` and
// `[a-zA-Z0-9_]` are the same class, so "word run" covers both.

// (\w+)\s*\(   -> group 1 of the first match.
// Backtracking to a shorter word leaves a word character next, which fails
// `\s*\(`; a start inside a run reaches the same run end, so only each run's
// first byte needs trying.
std::optional<std::string_view> functionName(std::string_view line)
{
  std::size_t i = 0;
  while (i < line.size())
  {
    if (!isRegexWord(line[i]))
    {
      ++i;
      continue;
    }
    const auto end = regexWordRunEnd(line, i);
    const auto paren = skipRegexSpace(line, end);
    if (paren < line.size() && line[paren] == '(')
    {
      return line.substr(i, end - i);
    }
    i = end;
  }
  return std::nullopt;
}

struct AssignCall
{
  std::string_view target;
  std::string_view func;
};

// \s*([a-zA-Z][a-zA-Z0-9_]*)\s*=\s*(\w+)\s*\(   with group 2 also passing
// `funcOk`. Unanchored and without `\b`: group 1 may start at any letter, and
// a giving-back `\s*` would leave a space where a letter or word byte is
// required. Both names are therefore maximal word runs, the tail depends only
// on where group 1's run ends, and the leftmost match takes the first letter
// of the first word run whose tail matches. `get_parent\s*\(` after `=\s*`
// is the same as group 2 being exactly `get_parent`.
template <typename FuncOk>
std::optional<AssignCall> findAssignCall(std::string_view line, FuncOk funcOk)
{
  std::size_t i = 0;
  while (i < line.size())
  {
    if (!isRegexWord(line[i]))
    {
      ++i;
      continue;
    }
    const auto runEnd = regexWordRunEnd(line, i);
    auto start = i;
    while (start < runEnd && !isAsciiLetter(line[start]))
    {
      ++start;
    }
    auto j = skipRegexSpace(line, runEnd);
    if (start < runEnd && j < line.size() && line[j] == '=')
    {
      j = skipRegexSpace(line, j + 1);
      const auto funcEnd = regexWordRunEnd(line, j);
      const auto paren = skipRegexSpace(line, funcEnd);
      if (funcEnd > j && paren < line.size() && line[paren] == '(')
      {
        const auto func = line.substr(j, funcEnd - j);
        if (funcOk(func))
        {
          return AssignCall{line.substr(start, runEnd - start), func};
        }
      }
    }
    i = runEnd;
  }
  return std::nullopt;
}

// (\w+)\s*\([^)]*\)\s*(.+)   -> group 2 of the first match.
// Group 1 as in `functionName`; `[^)]*\)` ends at the first `)`. Greedy `\s*`
// then `.+` (any byte but `\n`/`\r`) to the first excluded byte; when only
// spaces are left, `\s*` gives back one byte at a time until `.+` can start.
std::optional<std::string_view> paramOutputs(std::string_view line)
{
  std::size_t i = 0;
  while (i < line.size())
  {
    if (!isRegexWord(line[i]))
    {
      ++i;
      continue;
    }
    const auto end = regexWordRunEnd(line, i);
    i = end;
    const auto paren = skipRegexSpace(line, end);
    if (paren >= line.size() || line[paren] != '(')
    {
      continue;
    }
    const auto close = line.find(')', paren + 1);
    if (close == std::string_view::npos)
    {
      continue;
    }
    const auto after = close + 1;
    auto begin = skipRegexSpace(line, after);
    if (begin == line.size())
    {
      while (begin > after && isRegexDotExcluded(line[begin - 1]))
      {
        --begin;
      }
      if (begin == after)
      {
        continue;
      }
      --begin;
    }
    auto outEnd = begin;
    while (outEnd < line.size() && !isRegexDotExcluded(line[outEnd]))
    {
      ++outEnd;
    }
    return line.substr(begin, outEnd - begin);
  }
  return std::nullopt;
}

// \b([A-Za-z][A-Za-z0-9_-]*)\s*\(\s*\)   -> group 1 of the first match.
// Backtracking to a shorter name leaves a name byte next, which fails `\s*\(`;
// a `\b` start after a `-` inside the run reaches the same run end.
std::optional<std::string_view> arrayDeclName(std::string_view line)
{
  std::size_t i = 0;
  while (i < line.size())
  {
    if (!isAsciiLetter(line[i]) || (i > 0 && isRegexWord(line[i - 1])))
    {
      ++i;
      continue;
    }
    auto end = i;
    while (end < line.size() && (isRegexWord(line[end]) || line[end] == '-'))
    {
      ++end;
    }
    auto j = skipRegexSpace(line, end);
    if (j < line.size() && line[j] == '(')
    {
      j = skipRegexSpace(line, j + 1);
      if (j < line.size() && line[j] == ')')
      {
        return line.substr(i, end - i);
      }
    }
    i = end;
  }
  return std::nullopt;
}

// \(\s*([a-zA-Z][a-zA-Z0-9_]*)   -> group 1 of the first match.
std::optional<std::string_view> firstParam(std::string_view line)
{
  for (auto open = line.find('('); open != std::string_view::npos; open = line.find('(', open + 1))
  {
    const auto begin = skipRegexSpace(line, open + 1);
    if (begin < line.size() && isAsciiLetter(line[begin]))
    {
      return line.substr(begin, regexWordRunEnd(line, begin) - begin);
    }
  }
  return std::nullopt;
}

// \(([^)]+)\)   -> group 1 of the first match: the first `(` whose first `)`
// after it is not adjacent.
std::optional<std::string_view> insideParams(std::string_view line)
{
  for (auto open = line.find('('); open != std::string_view::npos; open = line.find('(', open + 1))
  {
    const auto close = line.find(')', open + 1);
    if (close == std::string_view::npos)
    {
      return std::nullopt;
    }
    if (close > open + 1)
    {
      return line.substr(open + 1, close - open - 1);
    }
  }
  return std::nullopt;
}

// `[a-zA-Z][a-zA-Z0-9_]*` at `i`: its end, or `i` when no letter is there.
std::size_t identifierEnd(std::string_view line, std::size_t i)
{
  if (i >= line.size() || !isAsciiLetter(line[i]))
  {
    return i;
  }
  return regexWordRunEnd(line, i);
}

// Whole-line `\s*=\s*([a-zA-Z][a-zA-Z0-9_]*)\s*$` from `i`: group's text.
std::optional<std::string_view> assignedIdentifierToEnd(std::string_view line, std::size_t i)
{
  i = skipRegexSpace(line, i);
  if (i >= line.size() || line[i] != '=')
  {
    return std::nullopt;
  }
  const auto begin = skipRegexSpace(line, i + 1);
  const auto end = identifierEnd(line, begin);
  if (end == begin || skipRegexSpace(line, end) != line.size())
  {
    return std::nullopt;
  }
  return line.substr(begin, end - begin);
}

struct SimpleAssign
{
  std::string_view target;
  std::string_view source;
};

// regex_match ^\s*([a-zA-Z][a-zA-Z0-9_]*)\s*=\s*([a-zA-Z][a-zA-Z0-9_]*)\s*$
// Every piece is forced: no class next to another can trade bytes.
std::optional<SimpleAssign> simpleAssign(std::string_view line)
{
  const auto begin = skipRegexSpace(line, 0);
  const auto end = identifierEnd(line, begin);
  if (end == begin)
  {
    return std::nullopt;
  }
  const auto source = assignedIdentifierToEnd(line, end);
  if (!source)
  {
    return std::nullopt;
  }
  return SimpleAssign{line.substr(begin, end - begin), *source};
}

// regex_match ^\s*([a-zA-Z][a-zA-Z0-9_]*)\s*\([^)]+\)\s*=\s*([a-zA-Z][a-zA-Z0-9_]*)\s*$
// -> group 1. `[^)]+\)` ends at the first `)` and needs a byte before it.
std::optional<std::string_view> arrayAssignTarget(std::string_view line)
{
  const auto begin = skipRegexSpace(line, 0);
  const auto end = identifierEnd(line, begin);
  if (end == begin)
  {
    return std::nullopt;
  }
  const auto open = skipRegexSpace(line, end);
  if (open >= line.size() || line[open] != '(')
  {
    return std::nullopt;
  }
  const auto close = line.find(')', open + 1);
  if (close == std::string_view::npos || close == open + 1
    || !assignedIdentifierToEnd(line, close + 1))
  {
    return std::nullopt;
  }
  return line.substr(begin, end - begin);
}

std::string includedFromPath(std::string_view stripped)
{
  const auto trimmed = trim(stripped);
  constexpr std::string_view kPrefix = "##included_from";
  if (trimmed.size() < kPrefix.size()
    || toLowerAscii(trimmed.substr(0, kPrefix.size())) != std::string(kPrefix))
  {
    return {};
  }
  return std::string(trim(trimmed.substr(kPrefix.size())));
}

// regex_search [PXYZN](\d+)?$ : the last non-digit byte is one of PXYZN.
bool hasPxyznSuffix(std::string_view name)
{
  auto i = name.size();
  while (i > 0 && isAsciiDigit(name[i - 1]))
  {
    --i;
  }
  return i > 0 && std::string_view("PXYZN").find(name[i - 1]) != std::string_view::npos;
}

const std::regex& stringAssignRe()
{
  static const std::regex re{R"(\b([A-Za-z][A-Za-z0-9_-]*)\$(?=\s*=))"};
  return re;
}

const std::regex& includedAssignRe()
{
  static const std::regex re{R"(\b([A-Za-z][A-Za-z0-9_-]*)\b(?:\([^)]*\))?(?!$)(?=\s*=))"};
  return re;
}

const std::regex& setRe()
{
  static const std::regex re{R"(\bset\s+([A-Za-z][A-Za-z0-9_-]*)\b)"};
  return re;
}

// regex_match ^[a-zA-Z][a-zA-Z0-9_]*$
bool isOutputName(std::string_view name)
{
  return !name.empty() && identifierEnd(name, 0) == name.size();
}

const catalog::NameType* findNameType(
  std::span<const catalog::NameType> rows,
  std::string_view name)
{
  for (const auto& row : rows)
  {
    if (row.name == name)
    {
      return &row;
    }
  }
  return nullptr;
}

const catalog::CommandSlotTypes* findSlotTypes(
  std::span<const catalog::CommandSlotTypes> rows,
  std::string_view name)
{
  for (const auto& row : rows)
  {
    if (row.command == name)
    {
      return &row;
    }
  }
  return nullptr;
}

std::string_view slotTypeAt(const catalog::CommandSlotTypes& command, std::uint8_t index)
{
  for (const auto& slot : command.slots)
  {
    if (slot.index == index)
    {
      return slot.type;
    }
  }
  return {};
}

auto splitComma(std::string_view text) -> std::vector<std::string>
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

std::string stripTrailingCommaSpace(std::string text)
{
  while (!text.empty())
  {
    const auto ch = text.back();
    if (ch == ',' || ch == ' ' || ch == '\t' || ch == '\r')
    {
      text.pop_back();
      continue;
    }
    break;
  }
  return std::string(trim(text));
}

// regex_search \[(.*?)\] : the first `[` whose next `]` comes before any
// `\n`/`\r`; group 1 trimmed and lowercased.
std::string bracketType(std::string_view comment)
{
  for (auto open = comment.find('['); open != std::string_view::npos;
    open = comment.find('[', open + 1))
  {
    for (auto i = open + 1; i < comment.size() && !isRegexDotExcluded(comment[i]); ++i)
    {
      if (comment[i] == ']')
      {
        return toLowerAscii(trim(comment.substr(open + 1, i - open - 1)));
      }
    }
  }
  return {};
}

void ingestComments(
  const VariableExtract& extract,
  std::unordered_map<std::string, std::optional<std::string>>& comments)
{
  for (const auto& tuple : extract.hoverVariables)
  {
    if (!comments.contains(tuple.name))
    {
      comments.emplace(tuple.name, tuple.comment);
    }
  }
}

std::string commentTypeFor(
  const std::unordered_map<std::string, std::optional<std::string>>& comments,
  std::string_view name)
{
  const auto it = comments.find(std::string(name));
  if (it == comments.end() || !it->second)
  {
    return {};
  }
  return bracketType(*it->second);
}

void pushWrite(
  ValidateContext& ctx,
  std::string name,
  std::string type,
  int line,
  std::string section,
  int column = -1)
{
  ctx.buildingTypes->writes.push_back(TypeWrite{
    .name = std::move(name),
    .type = std::move(type),
    .line = line,
    .section = std::move(section),
    .column = column,
  });
}

// Byte column of `token`, a view into `line`.
int columnIn(std::string_view line, std::string_view token)
{
  return static_cast<int>(token.data() - line.data());
}

struct CommaPart
{
  std::string text;
  int column = -1;
};

// splitComma, keeping each trimmed part's byte column in `line` (`text` is a
// view into it).
auto splitCommaAt(std::string_view line, std::string_view text) -> std::vector<CommaPart>
{
  std::vector<CommaPart> parts;
  const auto base = columnIn(line, text);
  std::size_t start = 0;
  while (start <= text.size())
  {
    const auto comma = text.find(',', start);
    const auto piece = text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
    const auto trimmed = trim(piece);
    const auto offset = trimmed.empty() ? start : static_cast<std::size_t>(trimmed.data() - text.data());
    parts.push_back(CommaPart{std::string(trimmed), base + static_cast<int>(offset)});
    if (comma == std::string_view::npos)
    {
      break;
    }
    start = comma + 1;
  }
  return parts;
}

std::string priorType(
  const ValidateContext& ctx,
  std::string_view name,
  int line,
  std::string_view section)
{
  std::string mostRecent = "generic";
  int mostRecentLine = -1;
  for (const auto& write : ctx.buildingTypes->writes)
  {
    if (write.name == name
      && write.line < line
      && write.section == section
      && write.line >= mostRecentLine)
    {
      mostRecent = write.type;
      mostRecentLine = write.line;
    }
  }
  return mostRecent;
}

std::string vecToReturnType(const std::vector<std::string>& params)
{
  if (params.size() >= 4)
  {
    const auto& x = params[1];
    const auto& y = params[2];
    const auto& z = params[3];
    if (x == "1" && y == "0" && z == "0")
    {
      return "xvector";
    }
    if (x == "0" && y == "1" && z == "0")
    {
      return "yvector";
    }
    if (x == "0" && y == "0" && z == "1")
    {
      return "zvector";
    }
  }
  return "pvector";
}

void analyzeThisFileLine(ValidateContext& ctx, const std::string& line, int lineNumber)
{
  const auto section = sectionNameForLine(lineNumber, ctx.prep->sections);
  if (const auto functionMatch = functionName(line))
  {
    const auto anyFunc = [](std::string_view) { return true; };
    const auto assignment = findAssignCall(line, anyFunc);
    if (assignment)
    {
      const auto funcName = toLowerAscii(assignment->func);
      if (const auto* row = findNameType(catalog::returnTypeMap(), funcName))
      {
        pushWrite(
          ctx,
          std::string(assignment->target),
          std::string(row->type),
          lineNumber,
          section,
          columnIn(line, assignment->target));
      }
    }

    // When paramOutputs matches, its group 1 is functionName's run: if that
    // first `(\w+)\s*\(` run fails the tail, there is no `)` after its `(` or
    // only `\n`/`\r` follow the first one, and every later run's `(` sits
    // before that same `)` and fails the same way.
    const auto outputsText = assignment ? std::nullopt : paramOutputs(line);
    if (outputsText)
    {
      const auto funcName = toLowerAscii(*functionMatch);
      if (const auto* slots = findSlotTypes(catalog::paramTypeMap(), funcName))
      {
        const auto outputs = splitCommaAt(line, *outputsText);
        for (std::size_t i = 0; i < outputs.size(); ++i)
        {
          if (!isOutputName(outputs[i].text))
          {
            continue;
          }
          const auto type = slotTypeAt(*slots, static_cast<std::uint8_t>(i + 1));
          if (type.empty())
          {
            continue;
          }
          pushWrite(
            ctx, outputs[i].text, std::string(type), lineNumber, section, outputs[i].column);
        }
      }
    }

    if (const auto arrayName = arrayDeclName(line))
    {
      pushWrite(
        ctx, std::string(*arrayName), "array", lineNumber, section, columnIn(line, *arrayName));
    }

    const auto funcNameRaw = *functionMatch;
    const auto funcName = toLowerAscii(funcNameRaw);
    if (findNameType(catalog::inputParamTypeMap(), funcName))
    {
      if (const auto param = firstParam(line))
      {
        if (const auto* row = findNameType(catalog::inputParamTypeMap(), funcName))
        {
          pushWrite(
            ctx,
            std::string(*param),
            std::string(row->type),
            lineNumber,
            section,
            columnIn(line, *param));
        }
      }
    }

    if (funcNameRaw == "vec_to")
    {
      if (const auto param = firstParam(line))
      {
        const auto first = std::string(*param);
        const auto fullLine = std::string(trim(line));
        const auto close = fullLine.find(')');
        std::string after;
        if (close != std::string::npos)
        {
          after = std::string(trim(std::string_view(fullLine).substr(close + 1)));
        }
        auto allParams = splitComma(after);
        allParams.insert(allParams.begin(), first);
        pushWrite(
          ctx, first, vecToReturnType(allParams), lineNumber, section, columnIn(line, *param));
      }
    }

    if (funcName == "vec_copy")
    {
      if (const auto param = firstParam(line))
      {
        const auto outputParam = std::string(*param);
        const auto fullLine = std::string(trim(line));
        const auto close = fullLine.find(')');
        std::string after;
        if (close != std::string::npos)
        {
          after = std::string(trim(std::string_view(fullLine).substr(close + 1)));
        }
        const auto inputType =
          priorType(ctx, stripTrailingCommaSpace(after), lineNumber, section);
        pushWrite(ctx, outputParam, inputType, lineNumber, section, columnIn(line, *param));
      }
    }

    if (funcName == "vec_rev")
    {
      if (const auto param = firstParam(line))
      {
        const auto outputParam = std::string(*param);
        const auto fullLine = std::string(trim(line));
        const auto close = fullLine.find(')');
        std::string after;
        if (close != std::string::npos)
        {
          after = std::string(trim(std::string_view(fullLine).substr(close + 1)));
        }
        const auto inputType =
          priorType(ctx, stripTrailingCommaSpace(after), lineNumber, section);
        pushWrite(
          ctx,
          outputParam,
          inputType == "pvector" ? "pvector" : "dvector",
          lineNumber,
          section,
          columnIn(line, *param));
      }
    }

    if (funcName == "get_parent")
    {
      if (const auto param = firstParam(line))
      {
        const auto inputType = priorType(ctx, *param, lineNumber, section);
        const auto parentAssign = findAssignCall(
          line, [](std::string_view func) { return func == "get_parent"; });
        if (parentAssign)
        {
          std::string outputType;
          if (inputType == "area")
          {
            outputType = "slope";
          }
          else if (inputType == "profileend")
          {
            outputType = "profile";
          }
          if (!outputType.empty())
          {
            pushWrite(
              ctx,
              std::string(parentAssign->target),
              std::move(outputType),
              lineNumber,
              section,
              columnIn(line, parentAssign->target));
          }
        }
      }
    }

    if (const auto* multi = findSlotTypes(catalog::multiParamTypeMap(), funcName))
    {
      if (const auto inside = insideParams(line))
      {
        const auto params = splitCommaAt(line, *inside);
        for (const auto& slot : multi->slots)
        {
          const auto index = static_cast<std::size_t>(slot.index) - 1;
          if (index < params.size() && isOutputName(params[index].text))
          {
            pushWrite(
              ctx,
              params[index].text,
              std::string(slot.type),
              lineNumber,
              section,
              params[index].column);
          }
        }
      }
    }
  }

  if (const auto simple = simpleAssign(line))
  {
    const auto inputType = priorType(ctx, simple->source, lineNumber, section);
    pushWrite(
      ctx, std::string(simple->target), inputType, lineNumber, section, columnIn(line, simple->target));
  }
  else if (const auto arrayTarget = arrayAssignTarget(line))
  {
    pushWrite(
      ctx, std::string(*arrayTarget), "array", lineNumber, section, columnIn(line, *arrayTarget));
  }
}

using IncludedVar = IncludedParentVar;

struct TempType
{
  std::string name;
  std::string type;
  int line = 0;
};

std::string analyzeAssignmentType(
  const std::string& line,
  const std::string& varName,
  const std::vector<TempType>& tempTypes)
{
  const std::regex assignmentRe{"\\b" + varName + R"(\s*=\s*(\w+)\s*\()"};
  std::smatch assignmentMatch;
  if (std::regex_search(line, assignmentMatch, assignmentRe))
  {
    const auto funcName = toLowerAscii(assignmentMatch[1].str());
    if (const auto* row = findNameType(catalog::returnTypeMap(), funcName))
    {
      return std::string(row->type);
    }
  }
  const std::regex varAssignRe{"\\b" + varName + R"(\s*=\s*([a-zA-Z][a-zA-Z0-9_]*))"};
  std::smatch varMatch;
  if (std::regex_search(line, varMatch, varAssignRe))
  {
    const auto source = varMatch[1].str();
    for (const auto& temp : tempTypes)
    {
      if (temp.name == source)
      {
        return temp.type;
      }
    }
  }
  return "generic";
}

void analyzeIncludedLine(
  ValidateContext& ctx,
  const std::string& line,
  int lineNumber,
  std::vector<TempType>& tempTypes,
  std::vector<IncludedParentQuery>& queries)
{
  const auto functionMatch = functionName(line);
  if (!functionMatch)
  {
    return;
  }
  const auto funcName = toLowerAscii(*functionMatch);
  if (findNameType(catalog::inputParamTypeMap(), funcName))
  {
    if (const auto param = firstParam(line))
    {
      const auto first = std::string(*param);
      std::string outputType;
      if (const auto* row = findNameType(catalog::inputParamTypeMap(), funcName))
      {
        outputType = std::string(row->type);
      }
      if (funcName == "vec_to")
      {
        const auto fullLine = std::string(trim(line));
        const auto close = fullLine.find(')');
        std::string after;
        if (close != std::string::npos)
        {
          after = std::string(trim(std::string_view(fullLine).substr(close + 1)));
        }
        auto allParams = splitComma(after);
        allParams.insert(allParams.begin(), first);
        outputType = vecToReturnType(allParams);
      }
      if (funcName == "vec_copy")
      {
        const auto fullLine = std::string(trim(line));
        const auto close = fullLine.find(')');
        std::string after;
        if (close != std::string::npos)
        {
          after = std::string(trim(std::string_view(fullLine).substr(close + 1)));
        }
        auto name = stripTrailingCommaSpace(after);
        outputType = priorType(ctx, name, lineNumber, "");
        queries.push_back(IncludedParentQuery{std::move(name), lineNumber, outputType});
      }
      if (!outputType.empty())
      {
        tempTypes.push_back(TempType{first, outputType, lineNumber});
      }
    }
  }

  if (const auto* multi = findSlotTypes(catalog::multiParamTypeMap(), funcName))
  {
    if (const auto inside = insideParams(line))
    {
      const auto params = splitComma(*inside);
      for (const auto& slot : multi->slots)
      {
        const auto index = static_cast<std::size_t>(slot.index) - 1;
        if (index < params.size() && isOutputName(params[index]))
        {
          tempTypes.push_back(TempType{params[index], std::string(slot.type), lineNumber});
        }
      }
    }
  }

  if (const auto* outputSlots = findSlotTypes(catalog::paramTypeMap(), funcName))
  {
    const auto close = line.find(')');
    std::string after;
    if (close != std::string::npos)
    {
      after = std::string(trim(std::string_view(line).substr(close + 1)));
    }
    const auto outputs = splitComma(after);
    for (const auto& slot : outputSlots->slots)
    {
      const auto index = static_cast<std::size_t>(slot.index) - 1;
      if (index < outputs.size() && isOutputName(outputs[index]))
      {
        tempTypes.push_back(TempType{outputs[index], std::string(slot.type), lineNumber});
      }
    }
  }
}

bool alreadyHas(const std::vector<IncludedVar>& vars, std::string_view name)
{
  return std::any_of(vars.begin(), vars.end(), [&](const IncludedVar& var) {
    return var.name == name;
  });
}

auto extractIncluded(ValidateContext& ctx, std::string fileContent)
  -> std::shared_ptr<const IncludedParentExtract>
{
  auto extract = std::make_shared<IncludedParentExtract>();
  auto& variables = extract->variables;
  auto& stringVariables = extract->stringVariables;
  std::vector<TempType> tempTypes;
  const auto lines = splitLines(fileContent);
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    auto raw = std::string(lines[static_cast<std::size_t>(i)]);
    const auto trimmed = trim(raw);
    if (trimmed.starts_with("!") || trimmed.starts_with("#"))
    {
      continue;
    }
    const auto hoverText = captureHoverComment(raw);
    const auto hoverType = hoverText ? bracketType(*hoverText) : std::string{};
    raw = stripValidationComment(raw);

    for (std::sregex_iterator it(raw.begin(), raw.end(), stringAssignRe()), last; it != last; ++it)
    {
      auto name = (*it)[1].str() + "$";
      if (!alreadyHas(stringVariables, name))
      {
        stringVariables.push_back(IncludedVar{std::move(name), "string", hoverText});
      }
    }
    for (std::sregex_iterator it(raw.begin(), raw.end(), includedAssignRe()), last; it != last; ++it)
    {
      const auto varName = (*it)[1].str();
      if (alreadyHas(variables, varName))
      {
        continue;
      }
      auto variableType = analyzeAssignmentType(raw, varName, tempTypes);
      if (variableType == "generic" && !hoverType.empty())
      {
        variableType = hoverType;
      }
      variables.push_back(IncludedVar{varName, std::move(variableType), hoverText});
    }
    for (std::sregex_iterator it(raw.begin(), raw.end(), setRe()), last; it != last; ++it)
    {
      const auto varName = (*it)[1].str();
      if (!alreadyHas(variables, varName))
      {
        variables.push_back(IncludedVar{varName, "generic", std::nullopt});
      }
    }
    analyzeIncludedLine(ctx, raw, i, tempTypes, extract->priorTypeQueries);
  }

  for (auto& variable : variables)
  {
    if (variable.type != "generic")
    {
      continue;
    }
    const auto found = std::find_if(tempTypes.begin(), tempTypes.end(), [&](const TempType& temp) {
      return temp.name == variable.name;
    });
    if (found != tempTypes.end())
    {
      variable.type = found->type;
    }
  }
  extract->text = std::move(fileContent);
  return extract;
}

bool priorTypeQueriesHold(const ValidateContext& ctx, const IncludedParentExtract& extract)
{
  return std::all_of(
    extract.priorTypeQueries.begin(),
    extract.priorTypeQueries.end(),
    [&](const IncludedParentQuery& query) {
      return priorType(ctx, query.name, query.line, "") == query.answer;
    });
}

// The parent's extraction reads this pass's type writes only through
// `priorType` queries, so a cached extract is reused while the parent text is
// byte-equal and every recorded query gives the same answer.
auto includedParentExtract(ValidateContext& ctx, const std::string& normalizedPath, std::string text)
  -> std::shared_ptr<const IncludedParentExtract>
{
  auto* cache = ctx.stores.includedParents;
  if (cache)
  {
    if (auto cached = cache->get(normalizedPath);
      cached && cached->text == text && priorTypeQueriesHold(ctx, *cached))
    {
      return cached;
    }
  }
  auto built = extractIncluded(ctx, std::move(text));
  if (cache)
  {
    cache->put(normalizedPath, built);
  }
  return built;
}

void applyGenericFromComments(
  std::vector<IncludedVar>& variables,
  const std::unordered_map<std::string, std::optional<std::string>>& comments)
{
  for (auto& variable : variables)
  {
    if (variable.type != "generic")
    {
      continue;
    }
    const auto detected = commentTypeFor(comments, variable.name);
    if (!detected.empty())
    {
      variable.type = detected;
    }
  }
}

std::optional<std::string> parentText(ValidateContext& ctx, const std::filesystem::path& path)
{
  if (ctx.stores.textForPath)
  {
    return ctx.stores.textForPath(path);
  }
  if (ctx.stores.linkedFiles)
  {
    if (auto snap = ctx.stores.linkedFiles->get(normalizeFsPath(path.string())))
    {
      return snap->text;
    }
  }
  return std::nullopt;
}

// The parent's extraction over the lines it contributes at `cutoff`
// (docs/adr/0015-native-inferred-included-from.md). Cached per path+cutoff.
auto parentExtractAt(ValidateContext& ctx, const std::filesystem::path& path, std::optional<int> cutoff)
  -> std::shared_ptr<const IncludedParentExtract>
{
  auto text = parentText(ctx, path);
  if (!text)
  {
    return nullptr;
  }
  auto key = normalizeFsPath(path.string());
  if (cutoff)
  {
    key += "#" + std::to_string(*cutoff);
  }
  return includedParentExtract(ctx, key, linesBeforeIncludePoint(*text, cutoff));
}

void handleIncludedFile(
  ValidateContext& ctx,
  const std::string& stripped,
  int lineNumber,
  const std::unordered_map<std::string, std::optional<std::string>>& comments,
  std::optional<int> cutoff)
{
  const auto fileName = includedFromPath(stripped);
  if (fileName.empty() || !ctx.stores.layout)
  {
    return;
  }
  const auto resolved = ctx.stores.layout->findIncludedFile(fileName);
  if (!resolved)
  {
    return;
  }
  const auto section = sectionNameForLine(lineNumber, ctx.prep->sections);
  const auto parent = parentExtractAt(ctx, *resolved, cutoff);
  if (!parent)
  {
    return;
  }
  auto variables = parent->variables;
  applyGenericFromComments(variables, comments);
  for (const auto& variable : variables)
  {
    auto type = variable.type;
    if (type.empty())
    {
      type = commentTypeFor(comments, variable.name);
      if (type.empty())
      {
        type = "generic";
      }
    }
    pushWrite(ctx, variable.name, std::move(type), lineNumber, section);
  }
  for (const auto& variable : parent->stringVariables)
  {
    pushWrite(ctx, variable.name, "string", lineNumber, section);
  }
}

void commentFallback(
  ValidateContext& ctx,
  const std::string& line,
  int lineNumber,
  const std::unordered_map<std::string, std::optional<std::string>>& comments)
{
  const auto section = sectionNameForLine(lineNumber, ctx.prep->sections);
  // Every match of \b([a-zA-Z][a-zA-Z0-9_]*)(?!\$)\b, like sregex_iterator.
  // The name is the whole word run (a shorter one fails the closing `\b`), so
  // a run matches unless `$` follows it; either way the scan resumes after it.
  std::size_t i = 0;
  while (i < line.size())
  {
    if (!isAsciiLetter(line[i]) || (i > 0 && isRegexWord(line[i - 1])))
    {
      ++i;
      continue;
    }
    const auto startChar = i;
    i = regexWordRunEnd(line, i);
    if (i < line.size() && line[i] == '$')
    {
      continue;
    }
    const auto varName = std::string_view(line).substr(startChar, i - startChar);
    if (isInsideQuotes(line, startChar) || catalog::isKeywordLabel(varName))
    {
      continue;
    }
    if (!ctx.settings.strictModeEnabled && hasPxyznSuffix(varName))
    {
      continue;
    }
    auto type = commentTypeFor(comments, varName);
    if (!type.empty())
    {
      // A cross-call type wins over the `[type]` comment.
      if (ctx.injection)
      {
        if (const auto it = ctx.injection->overrides.find(section + '\n' + std::string(varName));
          it != ctx.injection->overrides.end())
        {
          type = it->second;
        }
      }
      pushWrite(ctx, std::string(varName), type, lineNumber, section);
    }
  }
}

bool isVectorType(std::string_view type)
{
  return kVectorTypes.contains(toLowerAscii(type));
}

bool isValueType(std::string_view type)
{
  return toLowerAscii(type) == "value";
}

lsp::Range wrappedRange(int line, std::string_view lineText)
{
  const auto n = utf16Length(lineText);
  return lsp::Range{
    .start =
      lsp::Position{
        .line = static_cast<lsp::Uint>(line),
        .character = n == 0 ? 0u : 1u,
      },
    .end =
      lsp::Position{
        .line = static_cast<lsp::Uint>(line),
        .character = static_cast<lsp::Uint>(n),
      },
  };
}

std::string rawLine(const ValidateContext& ctx, int line)
{
  if (line < 0 || line >= static_cast<int>(ctx.prep->lines.size()))
  {
    return {};
  }
  return std::string(ctx.prep->lines[static_cast<std::size_t>(line)]);
}

} // namespace

// Inferred parents (docs/adr/0015-native-inferred-included-from.md): for each
// child scope the inference covers, every name visible there gets a write
// before line 0, typed only when every reaching parent agrees on the type.
void pushInferredParentWrites(
  ValidateContext& ctx,
  const ParentLinks& links,
  const std::unordered_map<std::string, std::optional<std::string>>& comments)
{
  if (links.inferred.empty() || links.writtenCoversFile)
  {
    return;
  }
  std::vector<std::string> scopes{std::string{}};
  for (const auto& section : ctx.prep->sections)
  {
    const auto lower = toLowerAscii(section.name);
    const auto written = std::find(links.writtenSections.begin(), links.writtenSections.end(), lower)
      != links.writtenSections.end();
    if (!written && std::find(scopes.begin(), scopes.end(), section.name) == scopes.end())
    {
      scopes.push_back(section.name);
    }
  }
  for (const auto& scope : scopes)
  {
    const auto lowerScope = toLowerAscii(scope);
    std::vector<std::shared_ptr<const IncludedParentExtract>> extracts;
    bool unreadable = false;
    for (const auto& parent : links.inferred)
    {
      auto cutoff = parent.wholeLine;
      if (!scope.empty())
      {
        for (const auto& [name, line] : parent.sectionLines)
        {
          if (name == lowerScope && (!cutoff || line < *cutoff))
          {
            cutoff = line;
          }
        }
      }
      if (!cutoff)
      {
        continue;
      }
      auto extract = parentExtractAt(ctx, parent.absPath, cutoff);
      if (!extract)
      {
        unreadable = true;
        break;
      }
      extracts.push_back(std::move(extract));
    }
    if (unreadable || extracts.empty())
    {
      continue;
    }
    const auto& visible = scopeFor(ctx.prep->neighborhood, scope);
    std::vector<std::string> names(visible.variables.begin(), visible.variables.end());
    std::sort(names.begin(), names.end());
    for (const auto& name : names)
    {
      std::optional<std::string> agreed;
      bool disagree = false;
      for (const auto& extract : extracts)
      {
        auto variables = extract->variables;
        applyGenericFromComments(variables, comments);
        const auto found = std::find_if(variables.begin(), variables.end(), [&](const IncludedVar& var) {
          return var.name == name;
        });
        const auto type = found == variables.end() || found->type.empty() ? std::string{} : found->type;
        if (type.empty() || (agreed && *agreed != type))
        {
          disagree = true;
          break;
        }
        agreed = type;
      }
      if (!disagree && agreed)
      {
        pushWrite(ctx, name, *agreed, -1, scope);
      }
    }
    std::vector<std::string> stringNames(visible.stringVariables.begin(), visible.stringVariables.end());
    std::sort(stringNames.begin(), stringNames.end());
    for (const auto& name : stringNames)
    {
      pushWrite(ctx, name + "$", "string", -1, scope);
    }
  }
}

void collectTypeTimeline(ValidateContext& ctx)
{
  ctx.buildingTypes->writes.clear();
  std::unordered_map<std::string, std::optional<std::string>> comments;
  ingestComments(ctx.prep->live, comments);
  ingestComments(ctx.prep->neighborhood, comments);

  ParentLinks parentLinks;
  if (ctx.stores.layout)
  {
    parentLinks = resolveParentLinks(
      ctx.prep->snapshot.text,
      pathFromUri(ctx.prep->snapshot.uri),
      *ctx.stores.layout,
      ctx.stores.index,
      ctx.settings.inferIncludedFrom);
  }
  pushInferredParentWrites(ctx, parentLinks, comments);

  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    ctx.pollCancel();
    const auto raw = ctx.prep->lines[static_cast<std::size_t>(i)];
    const auto trimmedLine = trim(raw);
    if (trimmedLine.find("IGNORE TYPE") != std::string_view::npos)
    {
      continue;
    }
    auto line = stripValidationComment(raw);
    if (trimmedLine.starts_with("##included_from"))
    {
      std::optional<int> cutoff;
      for (const auto& directive : parentLinks.written)
      {
        if (directive.line == i)
        {
          cutoff = directive.cutoffLine;
        }
      }
      handleIncludedFile(ctx, line, i, comments, cutoff);
      continue;
    }
    if (trimmedLine.starts_with("!")
      || trimmedLine.starts_with("#include")
      || trimmedLine.starts_with("##included_from")
      || trimmedLine.starts_with("#")
      || trimmedLine.ends_with("##")
      || trimmedLine.starts_with("call ")
      || trimmedLine.starts_with("debug "))
    {
      continue;
    }
    analyzeThisFileLine(ctx, line, i);
    commentFallback(ctx, line, i, comments);
    if (ctx.injection)
    {
      if (const auto it = ctx.injection->lineWrites.find(i); it != ctx.injection->lineWrites.end())
      {
        const auto section = sectionNameForLine(i, ctx.prep->sections);
        for (const auto& write : it->second)
        {
          pushWrite(ctx, write.name, write.type, i, section);
        }
      }
    }
  }
}

void familyVectorToValue(ValidateContext& ctx)
{
  if (!ctx.settings.warnVectorToValueTransition || ctx.atCap())
  {
    return;
  }

  std::vector<std::string> nameOrder;
  std::unordered_map<std::string, std::vector<const TypeWrite*>> byName;
  byName.reserve(ctx.prep->types.writes.size());
  for (const auto& write : ctx.prep->types.writes)
  {
    auto& bucket = byName[write.name];
    if (bucket.empty())
    {
      nameOrder.push_back(write.name);
    }
    bucket.push_back(&write);
  }

  struct ScopeState
  {
    bool hadVector = false;
    const TypeWrite* firstVector = nullptr;
  };

  for (const auto& name : nameOrder)
  {
    ctx.pollCancel();
    auto& infos = byName[name];
    std::stable_sort(infos.begin(), infos.end(), [](const TypeWrite* a, const TypeWrite* b) {
      return a->line < b->line;
    });
    std::unordered_map<std::string, ScopeState> scopes;
    for (const auto* info : infos)
    {
      const auto sectionName = sectionNameForLine(info->line, ctx.prep->sections);
      if (isVerificationIgnoredSection(sectionName))
      {
        continue;
      }
      auto& state = scopes[sectionName];
      if (isVectorType(info->type))
      {
        if (!state.hadVector)
        {
          state.hadVector = true;
          state.firstVector = info;
        }
      }
      else if (state.hadVector && state.firstVector && isValueType(info->type))
      {
        const auto lineText = rawLine(ctx, info->line);
        const auto vectorType = state.firstVector->type;
        const auto sectionSuffix =
          sectionName.empty() ? std::string{} : " in section " + sectionName;
        const auto inner = "Variable " + name + " was previously typed as " + vectorType
          + sectionSuffix + " and is now typed as value.";
        lsp::Diagnostic diagnostic;
        diagnostic.range = wrappedRange(info->line, lineText);
        diagnostic.severity = lsp::DiagnosticSeverity::Warning;
        diagnostic.source = std::string("ADT-Syntax");
        diagnostic.message = lineText + " has an error: " + inner;
        const auto vectorLineText = rawLine(ctx, state.firstVector->line);
        lsp::DiagnosticRelatedInformation related;
        related.location.uri = ctx.prep->snapshot.uri;
        related.location.range = wrappedRange(state.firstVector->line, vectorLineText);
        related.message = "First typed as " + vectorType + " here";
        diagnostic.relatedInformation = lsp::Array<lsp::DiagnosticRelatedInformation>{related};
        ctx.append(std::move(diagnostic), false, true);
      }
    }
  }
}

auto commentObjectType(const ValidateContext& ctx, std::string_view name) -> std::string
{
  std::unordered_map<std::string, std::optional<std::string>> comments;
  ingestComments(ctx.prep->live, comments);
  ingestComments(ctx.prep->neighborhood, comments);
  return commentTypeFor(comments, name);
}

} // namespace vertexadt
