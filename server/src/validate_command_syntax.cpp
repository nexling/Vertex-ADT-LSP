#include "utf16_text.h"
#include "validate.h"

#include "catalog/catalog.h"
#include "catalog/custom_validators.h"
#include "catalog/table_validator.h"
#include "text_util.h"

#include <cctype>
#include <cstdint>
#include <optional>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

bool isAsciiSpace(unsigned char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

bool isWordChar(unsigned char ch)
{
  return std::isalnum(ch) != 0 || ch == '_';
}

auto trimCopy(std::string_view text) -> std::string
{
  std::size_t begin = 0;
  while (begin < text.size() && isAsciiSpace(static_cast<unsigned char>(text[begin])))
  {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && isAsciiSpace(static_cast<unsigned char>(text[end - 1])))
  {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

bool hasLineContinuation(std::string_view line)
{
  auto stripped = stripValidationComment(line);
  while (!stripped.empty() && isAsciiSpace(static_cast<unsigned char>(stripped.back())))
  {
    stripped.pop_back();
  }
  return !stripped.empty() && stripped.back() == '\\';
}

auto trimLineContinuation(std::string_view line) -> std::string
{
  static const std::regex trailingSlash{R"(\s*\\\s*$)"};
  auto stripped = stripValidationComment(line);
  stripped = std::regex_replace(stripped, trailingSlash, "");
  return trimCopy(stripped);
}

// ASCII case-insensitive substring test; a cheap gate in front of icase regexes.
bool containsIgnoreCase(std::string_view text, std::string_view lowerNeedle)
{
  if (lowerNeedle.size() > text.size())
  {
    return false;
  }
  for (std::size_t i = 0; i + lowerNeedle.size() <= text.size(); ++i)
  {
    std::size_t k = 0;
    while (k < lowerNeedle.size()
           && std::tolower(static_cast<unsigned char>(text[i + k])) == lowerNeedle[k])
    {
      ++k;
    }
    if (k == lowerNeedle.size())
    {
      return true;
    }
  }
  return false;
}

bool looksLikeDbcreateOpen(std::string_view line)
{
  if (!containsIgnoreCase(line, "dbcreate"))
  {
    return false;
  }
  static const std::regex re{R"(\bdbcreate\s*\()", std::regex::icase};
  return std::regex_search(line.begin(), line.end(), re);
}

auto buildDbcreateLogicalLineMap(const std::vector<std::string_view>& lines) -> DbcreateLogicalLines
{
  DbcreateLogicalLines out;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto currentLine = stripValidationComment(lines[static_cast<std::size_t>(i)]);
    if (!looksLikeDbcreateOpen(currentLine))
    {
      continue;
    }

    std::vector<std::string> logicalParts;
    const auto currentPart = trimLineContinuation(currentLine);
    if (!currentPart.empty())
    {
      logicalParts.push_back(currentPart);
    }

    int nextIndex = i + 1;
    bool continued = hasLineContinuation(currentLine);
    while (continued && nextIndex < static_cast<int>(lines.size()))
    {
      out.continuationLines.insert(nextIndex);
      const auto nextLine = stripValidationComment(lines[static_cast<std::size_t>(nextIndex)]);
      const auto nextPart = trimLineContinuation(nextLine);
      if (!nextPart.empty())
      {
        logicalParts.push_back(nextPart);
      }
      continued = hasLineContinuation(nextLine);
      ++nextIndex;
    }

    if (logicalParts.size() > 1 || hasLineContinuation(currentLine))
    {
      std::string joined;
      for (std::size_t p = 0; p < logicalParts.size(); ++p)
      {
        if (p > 0)
        {
          joined.push_back(' ');
        }
        joined += logicalParts[p];
      }
      out.logicalLineByStart[i] = trimCopy(joined);
    }
  }
  return out;
}

} // namespace

// Per pass. A cache keyed by the `lines` buffer address went stale when a
// later document's prep reused a freed address.
const DbcreateLogicalLines& dbcreateMapFor(ValidateContext& ctx)
{
  if (!ctx.commandSyntaxDbcreateLines)
  {
    ctx.commandSyntaxDbcreateLines = buildDbcreateLogicalLineMap(ctx.prep->lines);
  }
  return *ctx.commandSyntaxDbcreateLines;
}

namespace {

bool hasThenToken(std::string_view line)
{
  if (!containsIgnoreCase(line, "then"))
  {
    return false;
  }
  static const std::regex re{R"(\bthen\b)", std::regex::icase};
  return std::regex_search(line.begin(), line.end(), re);
}

auto afterThenFragment(std::string_view line) -> std::string
{
  static const std::regex re{R"(\bthen\b)", std::regex::icase};
  std::vector<std::string> parts;
  std::cregex_iterator it(line.data(), line.data() + line.size(), re);
  std::cregex_iterator last;
  std::size_t pos = 0;
  for (; it != last; ++it)
  {
    const auto start = static_cast<std::size_t>(it->position());
    parts.emplace_back(line.substr(pos, start - pos));
    pos = start + it->length();
  }
  parts.emplace_back(line.substr(pos));
  if (parts.size() <= 1)
  {
    return {};
  }
  std::string joined;
  for (std::size_t i = 1; i < parts.size(); ++i)
  {
    if (i > 1)
    {
      joined += "then";
    }
    joined += parts[i];
  }
  return trimCopy(joined);
}

bool glueAfterParenRemainder(const catalog::CommandStructure& structure)
{
  if (!structure.paramsOutside.isOmitted() && !structure.paramsOutside.isUnbounded()
      && structure.paramsOutside.value > 0)
  {
    return true;
  }
  return !structure.maxParamsOutside.isOmitted();
}

// A valueExpression command's call on the right of `name = ...` whose
// remainder continues the assigned value (catalog::isValueExpressionTail):
// not parameters after parentheses (docs/adr/0011).
bool remainderIsAssignedValue(
  const catalog::CommandStructure& structure,
  const std::string& trimmedLine,
  std::size_t callStart,
  std::string_view remaining)
{
  if (!structure.valueExpression)
  {
    return false;
  }
  static const std::regex assignmentRe{R"(^[A-Za-z][A-Za-z0-9_]*\$?\s*=)"};
  std::smatch match;
  if (!std::regex_search(trimmedLine, match, assignmentRe))
  {
    return false;
  }
  const auto assignmentEnd = static_cast<std::size_t>(match.length());
  if (assignmentEnd < trimmedLine.size() && trimmedLine[assignmentEnd] == '=')
  {
    return false;
  }
  if (callStart < assignmentEnd)
  {
    return false;
  }
  const auto before = std::string_view(trimmedLine).substr(assignmentEnd, callStart - assignmentEnd);
  return catalog::isValueExpressionTail(before, remaining);
}

struct SyntaxIssue
{
  std::string error;
  bool warning = false;
};

auto extractBaseVariableName(std::string_view variableName) -> std::string
{
  auto name = trimCopy(variableName);
  static const std::regex simple{R"(^[A-Za-z][A-Za-z0-9_-]*\$?$)"};
  if (std::regex_match(name, simple))
  {
    return name;
  }
  static const std::regex math{R"(^([A-Za-z][A-Za-z0-9_-]*\$?)[+\-*/]\d+)"};
  std::smatch match;
  if (std::regex_search(name, match, math) && match.position() == 0)
  {
    return match[1].str();
  }
  static const std::regex arrayIndex{R"(^([A-Za-z][A-Za-z0-9_-]*\$?)\[[^\]]+\])"};
  if (std::regex_search(name, match, arrayIndex) && match.position() == 0)
  {
    return match[1].str();
  }
  static const std::regex firstVar{R"(^([A-Za-z][A-Za-z0-9_-]*\$?))"};
  if (std::regex_search(name, match, firstVar) && match.position() == 0)
  {
    return match[1].str();
  }
  return name;
}

auto hoverTypeFrom(const std::vector<HoverTuple>& tuples, std::string_view name)
  -> std::optional<std::string>
{
  for (const auto& tuple : tuples)
  {
    if (tuple.name != name)
    {
      continue;
    }
    if (!tuple.comment)
    {
      return std::nullopt;
    }
    const auto& comment = *tuple.comment;
    const auto open = comment.find('[');
    const auto close = comment.find(']', open == std::string::npos ? 0 : open);
    if (open == std::string::npos || close == std::string::npos || close <= open)
    {
      return std::nullopt;
    }
    return toLowerAscii(trim(std::string_view(comment).substr(open + 1, close - open - 1)));
  }
  return std::nullopt;
}

auto hoverTypeFromExtracts(const ValidateContext& ctx, std::string_view name)
  -> std::optional<std::string>
{
  if (auto found = hoverTypeFrom(ctx.prep->live.hoverVariables, name))
  {
    return found;
  }
  if (auto found = hoverTypeFrom(ctx.prep->live.hoverStringVariables, name))
  {
    return found;
  }
  if (auto found = hoverTypeFrom(ctx.prep->neighborhood.hoverVariables, name))
  {
    return found;
  }
  if (auto found = hoverTypeFrom(ctx.prep->neighborhood.hoverStringVariables, name))
  {
    return found;
  }
  return std::nullopt;
}

auto getCurrentVariableType(
  const ValidateContext& ctx,
  std::string_view varName,
  int currentLine) -> std::optional<std::string>
{
  const auto sectionName = sectionNameForLine(ctx.prep->lines.empty() ? 0 : currentLine - 1, ctx.prep->sections);
  // Same scan as over every write, restricted to this name's writes in write
  // order, so the `>=` tie-break still picks the last closest write.
  const auto bucket = ctx.typeWritesByName.find(varName);
  if (bucket == ctx.typeWritesByName.end())
  {
    return hoverTypeFromExtracts(ctx, varName);
  }
  const TypeWrite* closest = nullptr;
  for (const auto index : bucket->second)
  {
    const auto& write = ctx.prep->types.writes[index];
    if (write.line <= currentLine && write.section == sectionName)
    {
      if (!closest || write.line >= closest->line)
      {
        closest = &write;
      }
    }
  }
  if (closest)
  {
    return closest->type;
  }
  return std::nullopt;
}

auto getVariableType(const ValidateContext& ctx, std::string variableName, int currentLine)
  -> std::string
{
  if (!variableName.empty() && variableName.front() == '-')
  {
    variableName.erase(variableName.begin());
  }
  if (catalog::isKeywordLabel(variableName))
  {
    return "keyword";
  }
  if (variableName.size() >= 2 && variableName.front() == '"' && variableName.back() == '"')
  {
    return "string";
  }
  static const std::regex numeric{R"(^\d+(\.\d+)?$)"};
  if (std::regex_match(variableName, numeric))
  {
    return "value";
  }

  const auto baseVariableName = extractBaseVariableName(variableName);
  static const std::regex ticker{R"(^([a-zA-Z][a-zA-Z0-9_]*)\((.*)\)$)"};
  const auto trimmedOriginal = trimCopy(variableName);
  const bool isTickerExpression = std::regex_match(trimmedOriginal, ticker);

  if (!baseVariableName.empty() && baseVariableName.back() == '$')
  {
    return "string";
  }

  if (!isTickerExpression && trimmedOriginal.find_first_of("+*/-") != std::string::npos
      && trimmedOriginal.find('(') != std::string::npos
      && trimmedOriginal.find(')') != std::string::npos)
  {
    if (catalog::isValidMathExpression(variableName))
    {
      return "value";
    }
  }

  if (auto variableType = getCurrentVariableType(ctx, baseVariableName, currentLine))
  {
    if (!variableType->empty() && *variableType != "generic")
    {
      return *variableType;
    }
  }

  if (auto detected = hoverTypeFromExtracts(ctx, baseVariableName))
  {
    if (!detected->empty() && *detected != "generic")
    {
      return *detected;
    }
  }
  return "unknown";
}

std::optional<SyntaxIssue> validateStrictMode(
  const ValidateContext& ctx,
  const catalog::ParsedCommand& parsed,
  const catalog::StrictMode& strictConfig,
  std::string_view commandName,
  int lineNumber)
{
  if (auto types = strictConfig.inputTypes; !types.empty())
  {
    for (const auto& typeCheck : types)
    {
      if (typeCheck.paramIndex >= parsed.insideParams.size())
      {
        continue;
      }
      const auto& param = parsed.insideParams[typeCheck.paramIndex];
      if (param.empty())
      {
        continue;
      }
      const auto paramType = getVariableType(ctx, param, lineNumber);
      if (paramType == "unknown")
      {
        continue;
      }
      bool allowed = false;
      for (const auto type : typeCheck.allowedTypes)
      {
        if (paramType == type)
        {
          allowed = true;
          break;
        }
      }
      if (!allowed)
      {
        std::string joined;
        for (std::size_t i = 0; i < typeCheck.allowedTypes.size(); ++i)
        {
          if (i > 0)
          {
            joined += ", ";
          }
          joined += typeCheck.allowedTypes[i];
        }
        return SyntaxIssue{
          .error = std::string(commandName) + ": " + std::string(typeCheck.paramName) + " '" + param
            + "' should be of type [" + joined + "] but is '" + paramType + "'",
        };
      }
    }
  }

  for (const auto& typeCheck : strictConfig.afterParamTypes)
  {
    if (typeCheck.paramIndex >= parsed.afterParams.size())
    {
      continue;
    }
    const auto& param = parsed.afterParams[typeCheck.paramIndex];
    if (param.empty())
    {
      continue;
    }
    const auto paramType = getVariableType(ctx, param, lineNumber);
    if (paramType == "unknown")
    {
      continue;
    }
    bool allowed = false;
    for (const auto type : typeCheck.allowedTypes)
    {
      if (paramType == type)
      {
        allowed = true;
        break;
      }
    }
    if (!allowed)
    {
      std::string joined;
      for (std::size_t i = 0; i < typeCheck.allowedTypes.size(); ++i)
      {
        if (i > 0)
        {
          joined += ", ";
        }
        joined += typeCheck.allowedTypes[i];
      }
      return SyntaxIssue{
        .error = std::string(commandName) + ": " + std::string(typeCheck.paramName) + " '" + param
          + "' should be of type [" + joined + "] but is '" + paramType + "'",
      };
    }
  }
  return std::nullopt;
}

struct FunctionCallSpan
{
  std::string fullMatch;
  std::size_t startIndex = 0;
  std::size_t endIndex = 0;
};

auto collectParenthesizedCalls(
  std::string_view trimmedLine,
  std::string_view commandName) -> std::vector<FunctionCallSpan>
{
  std::vector<FunctionCallSpan> calls;
  std::size_t i = 0;
  while (i < trimmedLine.size())
  {
    if (!isWordChar(static_cast<unsigned char>(trimmedLine[i]))
        || (i > 0 && isWordChar(static_cast<unsigned char>(trimmedLine[i - 1]))))
    {
      ++i;
      continue;
    }
    if (i + commandName.size() > trimmedLine.size()
        || toLowerAscii(trimmedLine.substr(i, commandName.size())) != commandName)
    {
      ++i;
      continue;
    }
    auto j = i + commandName.size();
    if (j < trimmedLine.size() && isWordChar(static_cast<unsigned char>(trimmedLine[j])))
    {
      ++i;
      continue;
    }
    while (j < trimmedLine.size() && isAsciiSpace(static_cast<unsigned char>(trimmedLine[j])))
    {
      ++j;
    }
    if (j >= trimmedLine.size() || trimmedLine[j] != '(')
    {
      ++i;
      continue;
    }
    if (isInsideQuotes(trimmedLine, i))
    {
      ++i;
      continue;
    }
    calls.push_back(FunctionCallSpan{
      .fullMatch = {},
      .startIndex = i,
      .endIndex = j,
    });
    i = j + 1;
  }
  return calls;
}

bool noParenHit(std::string_view trimmedLine, std::string_view commandName)
{
  std::size_t i = 0;
  while (i < trimmedLine.size())
  {
    if (!isWordChar(static_cast<unsigned char>(trimmedLine[i]))
        || (i > 0 && isWordChar(static_cast<unsigned char>(trimmedLine[i - 1]))))
    {
      ++i;
      continue;
    }
    if (i + commandName.size() <= trimmedLine.size()
        && toLowerAscii(trimmedLine.substr(i, commandName.size())) == commandName)
    {
      auto j = i + commandName.size();
      if (j < trimmedLine.size() && isWordChar(static_cast<unsigned char>(trimmedLine[j])))
      {
        ++i;
        continue;
      }
      while (j < trimmedLine.size() && isAsciiSpace(static_cast<unsigned char>(trimmedLine[j])))
      {
        ++j;
      }
      if (j < trimmedLine.size() && trimmedLine[j] == '(')
      {
        ++i;
        continue;
      }
      return true;
    }
    ++i;
  }
  return false;
}

auto validateCommandSyntax(
  const ValidateContext& ctx,
  std::string line,
  int lineNumber) -> std::vector<SyntaxIssue>
{
  line = stripValidationComment(line);
  const auto trimmedLine = trimCopy(line);
  if (trimmedLine.empty() || trimmedLine.front() == '!' || trimmedLine.front() == ';')
  {
    return {};
  }

  if (trimmedLine.find("call") != std::string::npos
      && trimmedLine.find('(') != std::string::npos
      && trimmedLine.find(')') != std::string::npos)
  {
    if (auto callError = catalog::validateCallCommandSyntax(trimmedLine))
    {
      return {SyntaxIssue{.error = std::move(*callError)}};
    }
  }

  std::size_t i = 0;
  while (i < trimmedLine.size())
  {
    if (!isWordChar(static_cast<unsigned char>(trimmedLine[i]))
        || (i > 0 && isWordChar(static_cast<unsigned char>(trimmedLine[i - 1]))))
    {
      ++i;
      continue;
    }
    std::size_t j = i;
    while (j < trimmedLine.size() && isWordChar(static_cast<unsigned char>(trimmedLine[j])))
    {
      ++j;
    }
    const auto ident = trimmedLine.substr(i, j - i);
    const auto commandName = toLowerAscii(ident);
    if (isInsideQuotes(trimmedLine, i))
    {
      i = j;
      continue;
    }
    const auto* config = catalog::findCommand(commandName);
    if (!config)
    {
      i = j;
      continue;
    }

    auto rawCalls = collectParenthesizedCalls(trimmedLine, commandName);
    std::vector<FunctionCallSpan> functionCalls;
    int numberOfCommands = 0;
    for (auto& call : rawCalls)
    {
      ++numberOfCommands;
      if (numberOfCommands == 1 && trimmedLine.find('=') != std::string::npos)
      {
        const auto prefix = trimmedLine.substr(0, call.startIndex);
        static const std::regex assignmentPrefixRe{R"(([A-Za-z][A-Za-z0-9_]*\$?)\s*=\s*$)"};
        std::smatch assignmentMatch;
        if (std::regex_search(prefix.begin(), prefix.end(), assignmentMatch, assignmentPrefixRe)
            && static_cast<std::size_t>(assignmentMatch.position()) + assignmentMatch.length() == prefix.size())
        {
          if (config->structure.hasAssignment == catalog::AssignmentPresence::Forbidden)
          {
            return {SyntaxIssue{
              .error = "Command '" + commandName + "' should not have an assignment variable",
            }};
          }
        }
      }

      const auto closeParenIndex = catalog::findMatchingParenIndex(trimmedLine, call.endIndex);
      if (closeParenIndex < 0)
      {
        // familyBracketBalance reports it, underlined from the `(`.
        return {};
      }
      const auto endIndex = static_cast<std::size_t>(closeParenIndex) + 1;
      std::string functionCall(trimmedLine.substr(call.startIndex, endIndex - call.startIndex));
      if (glueAfterParenRemainder(config->structure))
      {
        const auto remaining = trimCopy(trimmedLine.substr(endIndex));
        if (!remaining.empty()
            && !remainderIsAssignedValue(config->structure, trimmedLine, call.startIndex, remaining))
        {
          functionCall = std::string(trimmedLine.substr(call.startIndex));
        }
      }
      functionCalls.push_back(FunctionCallSpan{
        .fullMatch = std::move(functionCall),
        .startIndex = call.startIndex,
        .endIndex = endIndex,
      });
    }

    if (functionCalls.empty())
    {
      if (!config->structure.hasParentheses && noParenHit(trimmedLine, commandName))
      {
        functionCalls.push_back(FunctionCallSpan{
          .fullMatch = trimmedLine,
          .startIndex = toLowerAscii(trimmedLine).find(commandName),
          .endIndex = trimmedLine.size(),
        });
      }
      if (functionCalls.empty())
      {
        i = j;
        continue;
      }
    }

    const bool mustHaveAssignment =
      config->structure.hasAssignment == catalog::AssignmentPresence::Required;

    for (const auto& funcCall : functionCalls)
    {
      std::optional<catalog::ParsedCommand> parsed;
      if (!config->structure.hasParentheses)
      {
        parsed = catalog::parseCommandNoParens(
          funcCall.fullMatch,
          trimmedLine,
          mustHaveAssignment,
          config->structure.acceptSpacesAsSeparators);
      }
      else
      {
        parsed = catalog::parseCommand(funcCall.fullMatch, trimmedLine, mustHaveAssignment);
      }
      if (!parsed)
      {
        return {SyntaxIssue{.error = "Invalid syntax for '" + commandName + "'"}};
      }

      const auto structureErrors =
        catalog::validateCommandStructure(*parsed, config->structure, commandName);
      if (!structureErrors.empty())
      {
        std::vector<SyntaxIssue> issues;
        issues.reserve(structureErrors.size());
        for (const auto& error : structureErrors)
        {
          issues.push_back(SyntaxIssue{.error = error});
        }
        return issues;
      }

      if (config->tableValidator)
      {
        if (auto validationError = catalog::applyTableValidator(*config->tableValidator, *parsed))
        {
          return {SyntaxIssue{
            .error = std::move(validationError->message),
            .warning = validationError->warning,
          }};
        }
      }
      else if (config->customValidator != catalog::CustomValidatorId::None)
      {
        if (auto validationError = catalog::applyCustomValidator(config->customValidator, *parsed))
        {
          return {SyntaxIssue{
            .error = std::move(validationError->message),
            .warning = validationError->warning,
          }};
        }
      }

      if (ctx.settings.strictModeEnabled && config->strictMode && config->strictMode->enabled)
      {
        if (auto strictError =
              validateStrictMode(ctx, *parsed, *config->strictMode, commandName, lineNumber))
        {
          return {std::move(*strictError)};
        }
      }
    }

    return {};
  }

  return {};
}

// Byte just past the first whole-word `then`, or npos.
std::size_t afterFirstThen(std::string_view text)
{
  static const std::regex re{R"(then)", std::regex::icase};
  std::cmatch match;
  if (!std::regex_search(text.data(), text.data() + text.size(), match, re))
  {
    return std::string_view::npos;
  }
  return static_cast<std::size_t>(match.position() + match.length());
}

void emitIssues(
  ValidateContext& ctx,
  int lineIndex,
  std::string_view lineText,
  const std::vector<SyntaxIssue>& issues)
{
  for (const auto& issue : issues)
  {
    if (ctx.atCap())
    {
      return;
    }
    ctx.append(lsp::Diagnostic{
      .range = {
        .start = {.line = static_cast<lsp::Uint>(lineIndex), .character = 0},
        .end = {
          .line = static_cast<lsp::Uint>(lineIndex),
          .character = static_cast<lsp::Uint>(utf16Length(lineText)),
        },
      },
      .message = issue.error,
      .severity = issue.warning ? lsp::DiagnosticSeverity::Warning : lsp::DiagnosticSeverity::Error,
      .source = std::string{"Syntax exception - ADT Plugin"},
    });
  }
}

} // namespace

void familyCommandSyntax(ValidateContext& ctx, PerLineState& line)
{
  const auto& dbcreate = dbcreateMapFor(ctx);
  if (dbcreate.continuationLines.contains(line.index))
  {
    return;
  }
  // `\`-continued if headers are validated once, from their first line.
  const auto& ifHeaders = ifHeaderLinesFor(ctx);
  if (ifHeaders.startByContinuation.contains(line.index))
  {
    return;
  }
  for (const auto& issue : bracketIssuesFor(ctx))
  {
    if (line.index >= issue.startLine && line.index <= issue.endLine)
    {
      return; // familyBracketBalance owns this line.
    }
  }

  std::string validationLine(line.stripped);
  if (const auto it = dbcreate.logicalLineByStart.find(line.index);
      it != dbcreate.logicalLineByStart.end())
  {
    validationLine = it->second;
  }
  int reportLine = line.index;
  if (const auto header = ifHeaders.headerByStart.find(line.index);
      header != ifHeaders.headerByStart.end() && !header->second.danglingBackslash
      && hasThenToken(header->second.text))
  {
    // The statement after `then`, reported on the line that holds it.
    const auto& joined = header->second;
    validationLine = afterThenFragment(joined.text);
    const auto statementAt = afterFirstThen(joined.text);
    if (statementAt != std::string_view::npos)
    {
      reportLine = joined.locate(statementAt).line;
    }
  }
  else if (line.lowercaseTrimmed.starts_with("if") && hasThenToken(line.stripped))
  {
    validationLine = afterThenFragment(line.stripped);
  }

  if (ctx.settings.strictModeEnabled && !ctx.typeWritesByNameBuilt)
  {
    const auto& writes = ctx.prep->types.writes;
    ctx.typeWritesByName.reserve(writes.size());
    for (std::size_t i = 0; i < writes.size(); ++i)
    {
      ctx.typeWritesByName[writes[i].name].push_back(i);
    }
    ctx.typeWritesByNameBuilt = true;
  }

  const auto issues = validateCommandSyntax(ctx, std::move(validationLine), reportLine + 1);
  emitIssues(
    ctx,
    reportLine,
    ctx.prep->lines[static_cast<std::size_t>(reportLine)],
    issues);
}

} // namespace vertexadt
