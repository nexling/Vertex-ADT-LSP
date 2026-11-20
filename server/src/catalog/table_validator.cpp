#include "table_validator.h"

#include "catalog.h"
#include "text_util.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <utility>

namespace vertexadt::catalog {
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

auto toLowerAscii(std::string_view text) -> std::string
{
  std::string out(text);
  for (char& ch : out)
  {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return out;
}

bool equalsIgnoreCase(std::string_view left, std::string_view right)
{
  if (left.size() != right.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < left.size(); ++i)
  {
    if (std::tolower(static_cast<unsigned char>(left[i]))
        != std::tolower(static_cast<unsigned char>(right[i])))
    {
      return false;
    }
  }
  return true;
}

const std::regex& basicNameRe()
{
  static const std::regex re{R"(^(?:[a-zA-Z][a-zA-Z0-9_]*|\d+)$)"};
  return re;
}

const std::regex& basicVarRe()
{
  static const std::regex re{R"(^[a-zA-Z][a-zA-Z0-9_]*$)"};
  return re;
}

const std::regex& stringVarRe()
{
  static const std::regex re{R"(^[a-zA-Z][a-zA-Z0-9_]*\$$)"};
  return re;
}

const std::regex& tickerRe()
{
  static const std::regex re{R"(^([a-zA-Z][a-zA-Z0-9_]*)\((.*)\)$)"};
  return re;
}

const std::regex& mathExprRe()
{
  static const std::regex re{R"(^[^\s()]+(\s*[+*/-]\s*[^\s()]+)+$)"};
  return re;
}

const std::regex& staticStringRe()
{
  static const std::regex re{R"("[^"]*")"};
  return re;
}

const std::regex& staticString2Re()
{
  static const std::regex re{R"('[^']*')"};
  return re;
}

const std::regex& stringVariableTokenRe()
{
  static const std::regex re{R"([a-zA-Z][a-zA-Z0-9_]*\$?(?:\([A-Za-z0-9]*\))?)"};
  return re;
}

const std::regex& concatOpRe()
{
  static const std::regex re{R"(\s*&\s*)"};
  return re;
}

const std::regex& concatProtectRe()
{
  static const std::regex re{
    R"("[^"]*"(?:\s*&\s*[a-zA-Z][a-zA-Z0-9_]*\$?)+|[a-zA-Z][a-zA-Z0-9_]*\$?(?:\s*&\s*"[^"]*")+|"[^"]*"(?:\s*&\s*"[^"]*")+)"};
  return re;
}

const std::regex& assignmentRe()
{
  static const std::regex re{R"(^([a-zA-Z][a-zA-Z0-9_]*\$?(?:\([^)]*\))?)\s*=\s*(.+)$)"};
  return re;
}

const std::regex& funcOpenRe()
{
  static const std::regex re{R"(^(\w+)\s*\()"};
  return re;
}

const std::regex& funcNoParenRe()
{
  static const std::regex re{R"(^(\w+)\b(.*)$)"};
  return re;
}

const std::regex& numericRe()
{
  static const std::regex re{R"(^-?\d+(\.\d+)?$)"};
  return re;
}

const std::regex& digitsRe()
{
  static const std::regex re{R"(^\d+$)"};
  return re;
}

const std::regex& stringIndexRe()
{
  static const std::regex re{R"(^([A-Za-z][a-zA-Z0-9_]*\$)\[[^\]]+\]$)"};
  return re;
}

const std::regex& concatPlaceholderRe()
{
  static const std::regex re{R"(__CONCAT_(\d+)__)"};
  return re;
}

bool regexMatch(const std::regex& re, std::string_view text)
{
  return std::regex_match(text.begin(), text.end(), re);
}

bool regexSearch(const std::regex& re, std::string_view text)
{
  return std::regex_search(text.begin(), text.end(), re);
}

bool isMathOperator(char ch)
{
  return ch == '+' || ch == '-' || ch == '*' || ch == '/';
}

bool isStartOfNewParam(std::string_view str)
{
  if (str.empty())
  {
    return false;
  }
  const auto ch = static_cast<unsigned char>(str.front());
  return std::isalpha(ch) != 0 || ch == '_' || str.front() == '"';
}

bool isEndOfParam(std::string_view str)
{
  if (str.empty())
  {
    return false;
  }
  const auto ch = static_cast<unsigned char>(str.back());
  return std::isalnum(ch) != 0 || ch == '_' || ch == ')' || ch == ']' || str.back() == '"';
}


// Exact hand-written equivalents of the simple patterns below, so the per-line
// command-syntax path avoids std::regex. Character classes are ASCII, as in the
// "C" locale the regexes use.
bool isAsciiAlpha(char ch)
{
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool isAsciiDigit(char ch)
{
  return ch >= '0' && ch <= '9';
}

bool isIdentTail(char ch)
{
  return isAsciiAlpha(ch) || isAsciiDigit(ch) || ch == '_';
}

bool hasLineBreak(std::string_view text)
{
  return text.find_first_of("\r\n") != std::string_view::npos;
}

// End of `[a-zA-Z][a-zA-Z0-9_]*` starting at `pos`, or `pos` when there is none.
std::size_t identEnd(std::string_view text, std::size_t pos)
{
  if (pos >= text.size() || !isAsciiAlpha(text[pos]))
  {
    return pos;
  }
  ++pos;
  while (pos < text.size() && isIdentTail(text[pos]))
  {
    ++pos;
  }
  return pos;
}

// regex_match ^[a-zA-Z][a-zA-Z0-9_]*$
bool matchBasicVar(std::string_view text)
{
  return !text.empty() && identEnd(text, 0) == text.size();
}

// regex_match ^[a-zA-Z][a-zA-Z0-9_]*\$$
bool matchStringVar(std::string_view text)
{
  return text.size() >= 2 && text.back() == '$' && matchBasicVar(text.substr(0, text.size() - 1));
}

// regex_match ^\d+$
bool matchDigits(std::string_view text)
{
  return !text.empty() && std::all_of(text.begin(), text.end(), isAsciiDigit);
}

// regex_match ^(?:[a-zA-Z][a-zA-Z0-9_]*|\d+)$
bool matchBasicName(std::string_view text)
{
  return matchBasicVar(text) || matchDigits(text);
}

// regex_match ^-?(\d+(\.\d+)?|\.\d+)$ : `.25` and `-.5` too (`linewidth .25`
// in the vxbd320 corpus, docs/adr/0020).
bool matchNumeric(std::string_view text)
{
  std::size_t i = 0;
  if (i < text.size() && text[i] == '-')
  {
    ++i;
  }
  const auto intStart = i;
  while (i < text.size() && isAsciiDigit(text[i]))
  {
    ++i;
  }
  if (i == intStart)
  {
    return i < text.size() && text[i] == '.' && matchDigits(text.substr(i + 1));
  }
  if (i == text.size())
  {
    return true;
  }
  return text[i] == '.' && matchDigits(text.substr(i + 1));
}

// regex_search [a-zA-Z][a-zA-Z0-9_]*\$?(?:\([A-Za-z0-9]*\))? : any ASCII letter.
bool searchStringVariableToken(std::string_view text)
{
  return std::any_of(text.begin(), text.end(), isAsciiAlpha);
}

// regex_search "[^"]*" (or '[^']*') : at least two of that quote.
bool searchQuotedRun(std::string_view text, char quote)
{
  const auto first = text.find(quote);
  return first != std::string_view::npos && text.find(quote, first + 1) != std::string_view::npos;
}

// regex_search \s*&\s* : any '&'.
bool searchConcatOp(std::string_view text)
{
  return text.find('&') != std::string_view::npos;
}

// regex_search ^(\w+)\s*\( at position 0: the name and the index of '('.
std::optional<std::pair<std::string, std::size_t>> matchFuncOpen(std::string_view text)
{
  std::size_t i = 0;
  while (i < text.size() && isIdentTail(text[i]))
  {
    ++i;
  }
  if (i == 0)
  {
    return std::nullopt;
  }
  auto j = i;
  while (j < text.size() && isAsciiSpace(static_cast<unsigned char>(text[j])))
  {
    ++j;
  }
  if (j >= text.size() || text[j] != '(')
  {
    return std::nullopt;
  }
  return std::pair<std::string, std::size_t>{std::string(text.substr(0, i)), j};
}

std::optional<std::pair<std::string, std::string>> matchCommandAssignment(std::string_view text)
{
  if (text.empty())
  {
    return std::nullopt;
  }

  if (!hasLineBreak(text))
  {
    // ^([a-zA-Z][a-zA-Z0-9_]*\$?(?:\([^)]*\))?)\s*=\s*(.+)$ : every quantifier is
    // forced, so there is exactly one way to match.
    auto i = identEnd(text, 0);
    if (i == 0)
    {
      return std::nullopt;
    }
    if (i < text.size() && text[i] == '$')
    {
      ++i;
    }
    if (i < text.size() && text[i] == '(')
    {
      const auto close = text.find(')', i + 1);
      if (close == std::string_view::npos)
      {
        return std::nullopt;
      }
      i = close + 1;
    }
    const auto nameEnd = i;
    while (i < text.size() && isAsciiSpace(static_cast<unsigned char>(text[i])))
    {
      ++i;
    }
    if (i >= text.size() || text[i] != '=')
    {
      return std::nullopt;
    }
    ++i;
    if (i >= text.size())
    {
      return std::nullopt;
    }
    auto valueStart = i;
    while (valueStart < text.size() && isAsciiSpace(static_cast<unsigned char>(text[valueStart])))
    {
      ++valueStart;
    }
    if (valueStart == text.size())
    {
      valueStart = text.size() - 1; // .+ takes back the last whitespace character
    }
    return std::pair<std::string, std::string>{
      std::string(text.substr(0, nameEnd)), std::string(text.substr(valueStart))};
  }
  std::match_results<std::string_view::const_iterator> match;
  if (!std::regex_match(text.begin(), text.end(), match, assignmentRe()))
  {
    return std::nullopt;
  }
  return std::pair<std::string, std::string>{match[1].str(), match[2].str()};
}

auto splitKeepSeparators(std::string_view text, const std::regex& re) -> std::vector<std::string>
{
  std::vector<std::string> parts;
  if (&re == &concatOpRe() && !searchConcatOp(text))
  {
    parts.emplace_back(text);
    return parts;
  }
  std::cregex_iterator it(text.data(), text.data() + text.size(), re);
  std::cregex_iterator last;
  std::size_t pos = 0;
  for (; it != last; ++it)
  {
    const auto start = static_cast<std::size_t>(it->position());
    parts.emplace_back(text.substr(pos, start - pos));
    parts.push_back(it->str());
    pos = start + it->length();
  }
  parts.emplace_back(text.substr(pos));
  return parts;
}

auto stripStringIndexSuffix(std::string_view expr) -> std::string
{
  if (expr.empty())
  {
    return std::string(expr);
  }
  const auto trimmed = trimCopy(expr);
  std::smatch match;
  if (std::regex_match(trimmed, match, stringIndexRe()))
  {
    return match[1].str();
  }
  return trimmed;
}

bool mapHasLowercaseName(std::span<const NameType> rows, std::string_view lowercaseName)
{
  for (const auto& row : rows)
  {
    if (row.name == lowercaseName)
    {
      return true;
    }
  }
  return false;
}

bool isValidMathOperand(std::string_view operandView)
{
  auto operand = trimCopy(operandView);
  if (operand.empty())
  {
    return false;
  }
  if (matchNumeric(operand))
  {
    return true;
  }
  if (operand.front() == '-' && matchBasicVar(operand.substr(1)))
  {
    return true;
  }
  if (matchBasicVar(operand))
  {
    return true;
  }
  std::smatch tickerMatch;
  if (operand.find('(') != std::string::npos && std::regex_match(operand, tickerMatch, tickerRe()))
  {
    if (!matchBasicVar(tickerMatch[1].str()))
    {
      return false;
    }
    return isValidMathOperand(tickerMatch[2].str());
  }
  return false;
}

bool isValidFlatMathExpression(std::string expr)
{
  expr.erase(std::remove_if(expr.begin(), expr.end(), [](unsigned char ch) {
    return isAsciiSpace(ch);
  }), expr.end());
  if (expr.empty())
  {
    return false;
  }

  std::vector<std::string> tokens;
  std::string current;
  for (char ch : expr)
  {
    if (isMathOperator(ch))
    {
      if (!current.empty())
      {
        tokens.push_back(std::move(current));
        current.clear();
      }
      tokens.emplace_back(1, ch);
    }
    else
    {
      current.push_back(ch);
    }
  }
  if (!current.empty())
  {
    tokens.push_back(std::move(current));
  }
  if (tokens.empty())
  {
    return false;
  }

  if (tokens[0] == "-" && tokens.size() > 1)
  {
    if (isValidMathOperand(tokens[1]))
    {
      tokens[0] = "-" + tokens[1];
      tokens.erase(tokens.begin() + 1);
    }
    else
    {
      return false;
    }
  }

  for (std::size_t i = 0; i < tokens.size(); ++i)
  {
    if (i % 2 == 0)
    {
      if (!isValidMathOperand(tokens[i]))
      {
        return false;
      }
    }
    else if (tokens[i].size() != 1 || !isMathOperator(tokens[i][0]))
    {
      return false;
    }
  }
  return tokens.size() % 2 == 1;
}

bool isValidMathOperandWithCommands(std::string_view operandView);
bool isValidMathExpressionWithCommands(std::string_view exprView);

bool isValidFlatMathExpressionWithCommands(std::string expr)
{
  expr.erase(std::remove_if(expr.begin(), expr.end(), [](unsigned char ch) {
    return isAsciiSpace(ch);
  }), expr.end());
  if (expr.empty())
  {
    return false;
  }

  std::vector<std::string> tokens;
  std::string current;
  for (char ch : expr)
  {
    if (isMathOperator(ch))
    {
      if (!current.empty())
      {
        tokens.push_back(std::move(current));
        current.clear();
      }
      tokens.emplace_back(1, ch);
    }
    else
    {
      current.push_back(ch);
    }
  }
  if (!current.empty())
  {
    tokens.push_back(std::move(current));
  }
  if (tokens.empty())
  {
    return false;
  }

  for (std::size_t i = 0; i < tokens.size(); ++i)
  {
    if (i % 2 == 0)
    {
      if (!isValidMathOperandWithCommands(tokens[i]))
      {
        return false;
      }
    }
    else if (tokens[i].size() != 1 || !isMathOperator(tokens[i][0]))
    {
      return false;
    }
  }
  return tokens.size() % 2 == 1;
}

bool isValidMathOperandWithCommands(std::string_view operandView)
{
  auto operand = trimCopy(operandView);
  if (operand.empty())
  {
    return false;
  }
  if (matchNumeric(operand))
  {
    return true;
  }
  if (operand == "VALID_SUBEXPR")
  {
    return true;
  }
  if (isValidCommand(operand))
  {
    return true;
  }
  if (matchBasicVar(operand))
  {
    return true;
  }
  std::smatch tickerMatch;
  if (operand.find('(') != std::string::npos && std::regex_match(operand, tickerMatch, tickerRe()))
  {
    if (!matchBasicVar(tickerMatch[1].str()))
    {
      return false;
    }
    return isValidMathOperandWithCommands(tickerMatch[2].str());
  }
  return false;
}

bool isValidMathExpressionWithCommands(std::string_view exprView)
{
  std::string expr = trimCopy(exprView);
  if (expr.empty())
  {
    return false;
  }

  std::vector<std::size_t> parenStack;
  std::size_t i = 0;
  while (i < expr.size())
  {
    const char ch = expr[i];
    if (ch == '(')
    {
      std::string beforeParen;
      std::ptrdiff_t j = static_cast<std::ptrdiff_t>(i) - 1;
      while (j >= 0 && (std::isalpha(static_cast<unsigned char>(expr[static_cast<std::size_t>(j)])) != 0
                        || expr[static_cast<std::size_t>(j)] == '_'))
      {
        beforeParen.insert(beforeParen.begin(), expr[static_cast<std::size_t>(j)]);
        --j;
      }
      if (!beforeParen.empty() && isValidCommand(beforeParen + expr.substr(i)))
      {
        int parenCount = 1;
        ++i;
        while (i < expr.size() && parenCount > 0)
        {
          if (expr[i] == '(')
          {
            ++parenCount;
          }
          if (expr[i] == ')')
          {
            --parenCount;
          }
          ++i;
        }
        continue;
      }
      parenStack.push_back(i);
      ++i;
    }
    else if (ch == ')')
    {
      if (parenStack.empty())
      {
        return false;
      }
      const auto openIndex = parenStack.back();
      parenStack.pop_back();
      const auto subExpr = expr.substr(openIndex + 1, i - openIndex - 1);
      if (!isValidMathExpressionWithCommands(subExpr))
      {
        return false;
      }
      expr = expr.substr(0, openIndex) + "VALID_SUBEXPR" + expr.substr(i + 1);
      i = openIndex + std::string("VALID_SUBEXPR").size();
    }
    else
    {
      ++i;
    }
  }
  if (!parenStack.empty())
  {
    return false;
  }
  return isValidFlatMathExpressionWithCommands(expr);
}

auto parameterOrdinalName(std::size_t index) -> std::string
{
  if (index == 0)
  {
    return "First parameter";
  }
  if (index == 1)
  {
    return "Second parameter";
  }
  if (index == 2)
  {
    return "Third parameter";
  }
  return "Parameter " + std::to_string(index + 1);
}

int emptySlotArityCap(const Arity& maxParams)
{
  if (maxParams.isOmitted() || maxParams.isUnbounded())
  {
    return -1;
  }
  return maxParams.value;
}

void collectEmptySlotErrors(
  std::vector<std::string>& errors,
  const std::vector<std::string>& params,
  int arityCap,
  std::string_view location,
  std::string_view commandName)
{
  const auto limit = arityCap < 0
    ? params.size()
    : std::min(params.size(), static_cast<std::size_t>(arityCap));
  for (std::size_t i = 0; i < limit; ++i)
  {
    if (params[i].empty())
    {
      errors.push_back(
        parameterOrdinalName(i) + " " + std::string(location) + " is missing in " + std::string(commandName));
    }
  }
}

bool slotKindOk(SlotKind kind, VariableKind variableKind, std::string_view text)
{
  switch (kind)
  {
  case SlotKind::NormalVariable:
    return isValidNormalVariable(text);
  case SlotKind::StringVariable:
    return isValidStringVariable(text);
  case SlotKind::AnyVariable:
    return isValidAnyVariable(text);
  case SlotKind::StringExpression:
    return isValidStringExpression(text);
  case SlotKind::StringOrVariable:
    if (variableKind == VariableKind::AnyVariable)
    {
      return isValidAnyVariable(text) || isValidStringExpression(text);
    }
    return isValidNormalVariable(text) || isValidStringExpression(text);
  case SlotKind::NumberOrVariable:
    return isValidNormalVariable(text);
  case SlotKind::DigitLiteralOrVariable:
    return matchDigits(text) || isValidNormalVariable(text);
  }
  return false;
}

auto interpolateTail(std::string_view templ, std::size_t index) -> std::string
{
  auto out = std::string(templ);
  const auto n = std::to_string(index + 1);
  std::size_t pos = 0;
  while ((pos = out.find("{n}", pos)) != std::string::npos)
  {
    out.replace(pos, 3, n);
    pos += n.size();
  }
  return out;
}

std::optional<ValidatorDiagnostic> checkSlots(
  const std::vector<std::string>& params,
  std::span<const Slot> slots)
{
  for (const auto& slot : slots)
  {
    if (slot.index >= params.size())
    {
      continue;
    }
    const auto& value = params[slot.index];
    if (value.empty())
    {
      continue;
    }
    if (!slotKindOk(slot.kind, slot.variableKind, value))
    {
      return ValidatorDiagnostic{.message = std::string(slot.invalid)};
    }
  }
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> checkTail(
  const std::vector<std::string>& params,
  const SlotTail* tail)
{
  if (!tail)
  {
    return std::nullopt;
  }
  for (std::size_t i = tail->from; i < params.size(); ++i)
  {
    if (params[i].empty())
    {
      continue;
    }
    if (!slotKindOk(tail->kind, tail->variableKind, params[i]))
    {
      return ValidatorDiagnostic{.message = interpolateTail(tail->invalid, i)};
    }
  }
  return std::nullopt;
}

void mix(std::size_t& acc, std::string_view text)
{
  acc = acc * 131u + text.size();
  if (!text.empty())
  {
    acc ^= static_cast<unsigned char>(text.front());
    acc ^= static_cast<unsigned char>(text.back()) << 8;
  }
}

void mix(std::size_t& acc, const char* text)
{
  mix(acc, text ? std::string_view{text} : std::string_view{});
}

void mixSlot(std::size_t& acc, const Slot& slot)
{
  acc ^= slot.index;
  acc ^= static_cast<std::size_t>(slot.kind) << 4;
  acc ^= static_cast<std::size_t>(slot.variableKind) << 8;
  acc ^= slot.required ? 1u : 0u;
  mix(acc, slot.invalid);
}

void mixTail(std::size_t& acc, const SlotTail* tail)
{
  if (!tail)
  {
    return;
  }
  acc ^= tail->from;
  acc ^= static_cast<std::size_t>(tail->kind) << 4;
  acc ^= static_cast<std::size_t>(tail->variableKind) << 8;
  mix(acc, tail->invalid);
}

} // namespace

auto findMatchingParenIndex(std::string_view text, std::size_t openParenIndex) -> std::ptrdiff_t
{
  if (openParenIndex >= text.size() || text[openParenIndex] != '(')
  {
    return -1;
  }

  int parenCount = 1;
  char activeQuote = 0;
  for (std::size_t i = openParenIndex + 1; i < text.size(); ++i)
  {
    const char ch = text[i];
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

auto parseParameters(std::string_view paramString, bool acceptSpacesAsSeparators) -> std::vector<std::string>
{
  if (paramString.empty() || trimCopy(paramString).empty())
  {
    return {};
  }

  std::string processed(paramString);
  std::vector<std::string> concatenations;
  {
    std::string rebuilt;
    rebuilt.reserve(processed.size());
    // Every concatProtectRe alternative contains '&'.
    std::cregex_iterator it = processed.find('&') == std::string::npos
      ? std::cregex_iterator()
      : std::cregex_iterator(processed.data(), processed.data() + processed.size(), concatProtectRe());
    std::cregex_iterator last;
    std::size_t pos = 0;
    for (; it != last; ++it)
    {
      const auto start = static_cast<std::size_t>(it->position());
      rebuilt.append(processed, pos, start - pos);
      rebuilt += "__CONCAT_" + std::to_string(concatenations.size()) + "__";
      concatenations.push_back(it->str());
      pos = start + it->length();
    }
    rebuilt.append(processed, pos, processed.size() - pos);
    processed = std::move(rebuilt);
  }

  std::vector<std::string> params;
  std::string current;
  bool inQuotes = false;
  char quoteChar = 0;
  int parenDepth = 0;
  int bracketDepth = 0;
  bool lastWasCommaSeparator = false;

  for (std::size_t i = 0; i < processed.size(); ++i)
  {
    const char ch = processed[i];
    if (!inQuotes && ch == '"')
    {
      inQuotes = true;
      quoteChar = ch;
      lastWasCommaSeparator = false;
      current += ch;
    }
    else if (inQuotes && ch == quoteChar)
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
      const auto trimmedCurrent = trimCopy(current);
      std::string_view rest(processed);
      rest.remove_prefix(i + 1);
      std::size_t next = 0;
      while (next < rest.size() && isAsciiSpace(static_cast<unsigned char>(rest[next])))
      {
        ++next;
      }
      const char nextNonSpace = next < rest.size() ? rest[next] : 0;
      const char prevNonSpace = trimmedCurrent.empty() ? 0 : trimmedCurrent.back();
      const auto afterTrim = trimCopy(rest);

      if (nextNonSpace && prevNonSpace && isMathOperator(prevNonSpace) && !isStartOfNewParam(afterTrim))
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
        params.push_back(trimmedCurrent);
        current.clear();
        lastWasCommaSeparator = true;
      }
    }
    else if (
      acceptSpacesAsSeparators && parenDepth == 0 && bracketDepth == 0
      && isAsciiSpace(static_cast<unsigned char>(ch)))
    {
      const auto trimmedCurrent = trimCopy(current);
      if (trimmedCurrent.empty())
      {
        continue;
      }
      std::string_view rest(processed);
      rest.remove_prefix(i + 1);
      std::size_t next = 0;
      while (next < rest.size() && isAsciiSpace(static_cast<unsigned char>(rest[next])))
      {
        ++next;
      }
      const char nextNonSpace = next < rest.size() ? rest[next] : 0;
      char prevNonSpace = 0;
      for (std::ptrdiff_t p = static_cast<std::ptrdiff_t>(current.size()) - 1; p >= 0; --p)
      {
        if (!isAsciiSpace(static_cast<unsigned char>(current[static_cast<std::size_t>(p)])))
        {
          prevNonSpace = current[static_cast<std::size_t>(p)];
          break;
        }
      }
      if (nextNonSpace && prevNonSpace
          && (isMathOperator(prevNonSpace) || isMathOperator(nextNonSpace)))
      {
        lastWasCommaSeparator = false;
        current += ch;
      }
      else
      {
        params.push_back(trimmedCurrent);
        current.clear();
        lastWasCommaSeparator = false;
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

  const auto trimmedCurrent = trimCopy(current);
  if (!trimmedCurrent.empty())
  {
    params.push_back(trimmedCurrent);
  }
  else if (lastWasCommaSeparator)
  {
    params.emplace_back();
  }

  for (auto& param : params)
  {
    if (param.find("__CONCAT_") == std::string::npos)
    {
      continue;
    }
    std::string rebuilt;
    std::sregex_iterator it(param.begin(), param.end(), concatPlaceholderRe());
    std::sregex_iterator last;
    std::size_t pos = 0;
    for (; it != last; ++it)
    {
      rebuilt.append(param, pos, static_cast<std::size_t>(it->position()) - pos);
      const auto index = static_cast<std::size_t>(std::stoul((*it)[1].str()));
      if (index < concatenations.size())
      {
        rebuilt += concatenations[index];
      }
      pos = static_cast<std::size_t>(it->position() + it->length());
    }
    rebuilt.append(param, pos, param.size() - pos);
    param = std::move(rebuilt);
  }

  return params;
}

auto parseCommand(
  std::string_view command,
  std::string_view trimmedLine,
  bool mustHaveAssignment) -> std::optional<ParsedCommand>
{
  auto commandText = trimCopy(command);
  std::optional<std::string> assignment;
  std::string commandPart = commandText;

  if (auto matched = matchCommandAssignment(commandText))
  {
    assignment = matched->first;
    commandPart = matched->second;
  }
  else if (mustHaveAssignment)
  {
    if (auto matched = matchCommandAssignment(trimmedLine))
    {
      assignment = matched->first;
      commandPart = matched->second;
    }
  }

  auto funcMatch = matchFuncOpen(commandPart);
  if (!funcMatch)
  {
    commandPart = commandText;
    funcMatch = matchFuncOpen(commandPart);
    if (!funcMatch)
    {
      return std::nullopt;
    }
  }

  const auto functionName = std::move(funcMatch->first);
  const auto openIndex = funcMatch->second;
  const auto closeIndex = findMatchingParenIndex(commandPart, openIndex);
  if (closeIndex < 0)
  {
    return std::nullopt;
  }

  const auto insideParens = commandPart.substr(openIndex + 1, static_cast<std::size_t>(closeIndex) - openIndex - 1);
  const auto afterParens = trimCopy(commandPart.substr(static_cast<std::size_t>(closeIndex) + 1));
  auto insideParams = parseParameters(insideParens);
  auto afterParams = afterParens.empty() ? std::vector<std::string>{} : parseParameters(afterParens);
  return ParsedCommand{
    .assignment = std::move(assignment),
    .functionName = functionName,
    .insideParams = std::move(insideParams),
    .afterParams = std::move(afterParams),
  };
}

auto parseCommandNoParens(
  std::string_view command,
  std::string_view trimmedLine,
  bool mustHaveAssignment,
  bool acceptSpacesAsSeparators) -> std::optional<ParsedCommand>
{
  auto commandText = trimCopy(command);
  std::optional<std::string> assignment;
  std::string commandPart = commandText;

  if (auto matched = matchCommandAssignment(commandText))
  {
    assignment = matched->first;
    commandPart = matched->second;
  }
  else if (mustHaveAssignment)
  {
    if (auto matched = matchCommandAssignment(trimmedLine))
    {
      assignment = matched->first;
      commandPart = matched->second;
    }
  }

  std::string functionName;
  std::string paramsPart;
  if (!hasLineBreak(commandPart))
  {
    // ^(\w+)\b(.*)$ : the word run is maximal and the rest is everything after it.
    std::size_t wordEnd = 0;
    while (wordEnd < commandPart.size() && isIdentTail(commandPart[wordEnd]))
    {
      ++wordEnd;
    }
    if (wordEnd == 0)
    {
      return std::nullopt;
    }
    functionName = commandPart.substr(0, wordEnd);
    paramsPart = trimCopy(std::string_view(commandPart).substr(wordEnd));
  }
  else
  {
    std::smatch funcMatch;
    if (!std::regex_match(commandPart, funcMatch, funcNoParenRe()))
    {
      return std::nullopt;
    }
    functionName = funcMatch[1].str();
    paramsPart = trimCopy(funcMatch[2].str());
  }
  if (!paramsPart.empty() && paramsPart.front() == '(')
  {
    return std::nullopt;
  }

  auto afterParams = paramsPart.empty()
    ? std::vector<std::string>{}
    : parseParameters(paramsPart, acceptSpacesAsSeparators);
  return ParsedCommand{
    .assignment = std::move(assignment),
    .functionName = functionName,
    .insideParams = {},
    .afterParams = std::move(afterParams),
  };
}

bool isValidMathExpression(std::string_view exprView)
{
  std::string expr = trimCopy(exprView);
  if (expr.empty())
  {
    return false;
  }

  std::vector<std::size_t> parenStack;
  std::size_t i = 0;
  while (i < expr.size())
  {
    const char ch = expr[i];
    if (ch == '(')
    {
      parenStack.push_back(i);
      ++i;
    }
    else if (ch == ')')
    {
      if (parenStack.empty())
      {
        return false;
      }
      const auto openIndex = parenStack.back();
      parenStack.pop_back();
      const auto subExpr = expr.substr(openIndex + 1, i - openIndex - 1);
      if (!isValidMathExpression(subExpr))
      {
        return false;
      }
      expr = expr.substr(0, openIndex) + "VALID_SUBEXPR" + expr.substr(i + 1);
      i = openIndex + std::string("VALID_SUBEXPR").size();
    }
    else
    {
      ++i;
    }
  }
  if (!parenStack.empty())
  {
    return false;
  }
  return isValidFlatMathExpression(expr);
}

bool isValidNormalVariable(std::string_view exprView)
{
  if (exprView.empty())
  {
    return false;
  }
  auto expr = trimCopy(exprView);
  if (expr.empty() || expr.size() > 255)
  {
    return false;
  }
  // mathExprRe cannot match without one of +*/-, so the two cheap tests decide alone.
  if (expr.find('(') != std::string::npos || expr.find_first_of("+*/-") != std::string::npos)
  {
    return isValidMathExpression(expr);
  }
  std::smatch tickerMatch;
  if (expr.find('(') != std::string::npos && std::regex_match(expr, tickerMatch, tickerRe()))
  {
    const auto baseVar = tickerMatch[1].str();
    if (!baseVar.empty() && baseVar.back() == '$')
    {
      return false;
    }
    if (!isValidNormalVariable(baseVar))
    {
      return false;
    }
    return isValidNormalVariable(tickerMatch[2].str());
  }
  if (matchNumeric(expr))
  {
    return true;
  }
  return matchBasicName(expr);
}

bool isValidStringVariable(std::string_view exprView)
{
  if (exprView.empty())
  {
    return false;
  }
  auto expr = stripStringIndexSuffix(exprView);
  if (expr.empty() || expr.size() > 16)
  {
    return false;
  }
  if (searchStringVariableToken(expr) && expr.find('$') != std::string::npos)
  {
    return true;
  }
  std::smatch tickerMatch;
  if (expr.find('(') != std::string::npos && std::regex_match(expr, tickerMatch, tickerRe()))
  {
    const auto baseVar = tickerMatch[1].str();
    if (baseVar.empty() || baseVar.back() != '$')
    {
      return false;
    }
    if (!matchStringVar(baseVar))
    {
      return false;
    }
    return isValidStringVariable(tickerMatch[2].str());
  }
  return matchStringVar(expr);
}

bool isValidAnyVariable(std::string_view expr)
{
  return isValidNormalVariable(expr) || isValidStringVariable(expr);
}

bool isAsciiStringCommand(std::string_view expr)
{
  if (expr.empty())
  {
    return false;
  }
  const auto trimmed = trimCopy(expr);
  if (!trimmed.starts_with("ascii("))
  {
    return false;
  }
  constexpr std::size_t openIndex = 5;
  const auto closeIndex = findMatchingParenIndex(trimmed, openIndex);
  if (closeIndex < 0 || static_cast<std::size_t>(closeIndex) != trimmed.size() - 1)
  {
    return false;
  }
  const auto inner = trimCopy(trimmed.substr(openIndex + 1, static_cast<std::size_t>(closeIndex) - openIndex - 1));
  if (inner.empty())
  {
    return false;
  }
  return isValidNormalVariable(inner);
}

bool isValidStringExpression(std::string_view expr)
{
  if (expr.empty())
  {
    return false;
  }
  const auto parts = splitKeepSeparators(expr, concatOpRe());
  bool stillvalid = false;
  for (std::size_t i = 0; i < parts.size(); ++i)
  {
    const auto part = trimCopy(parts[i]);
    if (part.empty())
    {
      continue;
    }
    if (i % 2 == 0)
    {
      const auto stringPart = stripStringIndexSuffix(part);
      const bool isValid = searchQuotedRun(part, '"')
        || searchQuotedRun(part, '\'')
        || (searchStringVariableToken(stringPart) && stringPart.find('$') != std::string::npos)
        || isAsciiStringCommand(part);
      if (parts.size() > 1)
      {
        stillvalid = matchBasicVar(part);
      }
      if (!isValid && !stillvalid)
      {
        return false;
      }
    }
    else if (!searchConcatOp(part))
    {
      return false;
    }
  }
  return true;
}

bool isValidCallPathParameter(std::string_view expr)
{
  if (expr.empty())
  {
    return false;
  }
  const auto parts = splitKeepSeparators(expr, concatOpRe());
  for (std::size_t i = 0; i < parts.size(); ++i)
  {
    const auto part = trimCopy(parts[i]);
    if (part.empty())
    {
      continue;
    }
    if (i % 2 == 0)
    {
      const bool isStringLike = searchQuotedRun(part, '"')
        || searchQuotedRun(part, '\'')
        || isValidStringVariable(part);
      if (!isStringLike)
      {
        return false;
      }
    }
    else if (!searchConcatOp(part))
    {
      return false;
    }
  }
  return true;
}

bool isValidCommand(std::string_view expr)
{
  const auto trimmed = trimCopy(expr);
  if (trimmed.empty())
  {
    return false;
  }
  const auto funcMatch = matchFuncOpen(trimmed);
  if (!funcMatch)
  {
    return false;
  }
  const auto functionName = toLowerAscii(funcMatch->first);
  if (findCommand(functionName))
  {
    return true;
  }
  if (mapHasLowercaseName(returnTypeMap(), functionName)
      || mapHasLowercaseName(inputParamTypeMap(), functionName))
  {
    return true;
  }
  return false;
}

bool isValidVariableOrCommand(std::string_view expr)
{
  if (isValidNormalVariable(expr))
  {
    return true;
  }
  if (isValidCommand(expr))
  {
    return true;
  }
  if (isValidMathExpressionWithCommands(expr))
  {
    return true;
  }
  return false;
}

auto validateCommandStructure(
  const ParsedCommand& parsed,
  const CommandStructure& structure,
  std::string_view commandName) -> std::vector<std::string>
{
  if (structure.hasAssignment == AssignmentPresence::Required && !parsed.assignment)
  {
    return {std::string(commandName) + " requires an assignment variable"};
  }
  if (structure.hasAssignment == AssignmentPresence::Forbidden && parsed.assignment)
  {
    return {std::string(commandName) + " should not have an assignment variable"};
  }

  std::vector<std::string> errors;
  collectEmptySlotErrors(
    errors, parsed.insideParams, emptySlotArityCap(structure.maxParamsInside),
    "inside parentheses", commandName);
  collectEmptySlotErrors(
    errors, parsed.afterParams, emptySlotArityCap(structure.maxParamsOutside),
    "after parentheses", commandName);

  if (!structure.minParamsInside.isOmitted() && !structure.minParamsInside.isUnbounded()
      && static_cast<int>(parsed.insideParams.size()) < structure.minParamsInside.value)
  {
    errors.push_back(
      std::string(commandName) + " requires at least " + std::to_string(structure.minParamsInside.value)
      + " parameters inside parentheses");
  }
  if (!structure.paramsOutside.isOmitted() && !structure.paramsOutside.isUnbounded()
      && static_cast<int>(parsed.afterParams.size()) < structure.paramsOutside.value)
  {
    errors.push_back(
      std::string(commandName) + " requires at least " + std::to_string(structure.paramsOutside.value)
      + " parameters after parentheses");
  }
  if (!structure.maxParamsInside.isOmitted() && !structure.maxParamsInside.isUnbounded()
      && static_cast<int>(parsed.insideParams.size()) > structure.maxParamsInside.value)
  {
    errors.push_back(
      std::string(commandName) + " accepts at most " + std::to_string(structure.maxParamsInside.value)
      + " parameters inside parentheses");
  }
  if (!structure.maxParamsOutside.isOmitted() && !structure.maxParamsOutside.isUnbounded()
      && static_cast<int>(parsed.afterParams.size()) > structure.maxParamsOutside.value)
  {
    errors.push_back(
      std::string(commandName) + " accepts at most " + std::to_string(structure.maxParamsOutside.value)
      + " parameters after parentheses");
  }
  return errors;
}

auto findParenthesizedCommandSegments(std::string_view line, std::string_view commandName)
  -> std::vector<std::string>
{
  std::vector<std::string> segments;
  const auto needle = toLowerAscii(commandName);
  std::size_t i = 0;
  while (i < line.size())
  {
    if (!isWordChar(static_cast<unsigned char>(line[i]))
        || (i > 0 && isWordChar(static_cast<unsigned char>(line[i - 1]))))
    {
      ++i;
      continue;
    }
    if (i + needle.size() > line.size() || !equalsIgnoreCase(line.substr(i, needle.size()), needle))
    {
      ++i;
      continue;
    }
    auto j = i + needle.size();
    if (j < line.size() && isWordChar(static_cast<unsigned char>(line[j])))
    {
      ++i;
      continue;
    }
    while (j < line.size() && isAsciiSpace(static_cast<unsigned char>(line[j])))
    {
      ++j;
    }
    if (j >= line.size() || line[j] != '(')
    {
      ++i;
      continue;
    }
    if (vertexadt::isInsideQuotes(line, i))
    {
      ++i;
      continue;
    }
    const auto closeIndex = findMatchingParenIndex(line, j);
    if (closeIndex < 0)
    {
      ++i;
      continue;
    }
    const auto endIndex = static_cast<std::size_t>(closeIndex) + 1;
    auto startIndex = i;
    const auto prefix = line.substr(0, i);
    static const std::regex assignmentPrefixRe{R"(([A-Za-z][A-Za-z0-9_]*\$?)\s*=\s*$)"};
    std::match_results<std::string_view::const_iterator> assignmentMatch;
    if (std::regex_search(prefix.begin(), prefix.end(), assignmentMatch, assignmentPrefixRe)
        && static_cast<std::size_t>(assignmentMatch.position()) + assignmentMatch.length() == prefix.size())
    {
      startIndex = prefix.size() - assignmentMatch.length();
    }
    segments.push_back(trimCopy(line.substr(startIndex, endIndex - startIndex)));
    i = j + 1;
  }
  return segments;
}

auto validateCallCommandSyntax(std::string_view line) -> std::optional<std::string>
{
  for (const auto& segment : findParenthesizedCommandSegments(line, "call"))
  {
    const auto parsed = parseCommand(segment, segment, false);
    if (!parsed || toLowerAscii(parsed->functionName) != "call")
    {
      return "Invalid syntax for 'call'";
    }
    if (parsed->insideParams.size() < 1)
    {
      return "call requires at least 1 parameter inside parentheses";
    }
    if (!isValidCallPathParameter(parsed->insideParams[0]))
    {
      return "First parameter inside parentheses must be a valid string or string variable in call";
    }
  }
  return std::nullopt;
}

std::size_t tableValidatorFingerprint(const TableValidator& row)
{
  std::size_t acc = 0;
  if (row.assignment)
  {
    acc ^= static_cast<std::size_t>(row.assignment->presence);
    acc ^= row.assignment->hasKind ? 1u : 0u;
    acc ^= static_cast<std::size_t>(row.assignment->kind) << 2;
    acc ^= static_cast<std::size_t>(row.assignment->variableKind) << 6;
    mix(acc, row.assignment->missing);
    mix(acc, row.assignment->invalid);
  }
  mix(acc, row.tooFewInside);
  mix(acc, row.tooManyInside);
  mix(acc, row.tooFewAfter);
  mix(acc, row.tooManyAfter);
  for (const Slot& slot : row.inside)
  {
    mixSlot(acc, slot);
  }
  for (const Slot& slot : row.after)
  {
    mixSlot(acc, slot);
  }
  mixTail(acc, row.insideTail);
  mixTail(acc, row.afterTail);
  return acc;
}

std::optional<ValidatorDiagnostic> applyTableValidator(const TableValidator& row)
{
  (void)tableValidatorFingerprint(row);
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> applyTableValidator(
  const TableValidator& row,
  const ParsedCommand& parsed)
{
  if (row.assignment && row.assignment->hasKind && parsed.assignment)
  {
    if (!slotKindOk(row.assignment->kind, row.assignment->variableKind, *parsed.assignment))
    {
      if (row.assignment->invalid)
      {
        return ValidatorDiagnostic{.message = row.assignment->invalid};
      }
    }
  }
  if (auto error = checkSlots(parsed.insideParams, row.inside))
  {
    return error;
  }
  if (auto error = checkSlots(parsed.afterParams, row.after))
  {
    return error;
  }
  if (auto error = checkTail(parsed.insideParams, row.insideTail))
  {
    return error;
  }
  if (auto error = checkTail(parsed.afterParams, row.afterTail))
  {
    return error;
  }
  return std::nullopt;
}

namespace {

int unmatchedOpenParens(std::string_view text)
{
  int depth = 0;
  for (const char ch : text)
  {
    if (ch == '(')
    {
      ++depth;
    }
    else if (ch == ')' && depth > 0)
    {
      --depth;
    }
  }
  return depth;
}

// `and` / `or` as a whole word at the start of `text` (any case): its length.
std::size_t leadingLogicalWord(std::string_view text)
{
  for (const std::string_view word : {std::string_view{"and"}, std::string_view{"or"}})
  {
    if (text.size() >= word.size() && toLowerAscii(text.substr(0, word.size())) == word
        && (text.size() == word.size()
            || !(std::isalnum(static_cast<unsigned char>(text[word.size()])) != 0
                 || text[word.size()] == '_' || text[word.size()] == '$')))
    {
      return word.size();
    }
  }
  return 0;
}

} // namespace

bool isValueExpressionTail(std::string_view before, std::string_view tail)
{
  const auto trimmed = trimCopy(tail);
  if (trimmed.empty())
  {
    return false;
  }
  if (trimmed == ";")
  {
    return true;
  }
  const char first = trimmed.front();
  if (isMathOperator(first))
  {
    // The call's value stands in as an operand: alone, or inside the value
    // written before it (`e = (findstr(a$,b$)-1)`).
    return isValidMathExpressionWithCommands("0" + trimmed)
      || isValidMathExpressionWithCommands(trimCopy(before) + "0" + trimmed);
  }
  if (first == ')')
  {
    return unmatchedOpenParens(before) > 0;
  }
  std::size_t connector = 0;
  if (first == '<' || first == '>' || first == '=')
  {
    connector = 1;
    while (connector < trimmed.size() && (trimmed[connector] == '<' || trimmed[connector] == '>'
                                           || trimmed[connector] == '='))
    {
      ++connector;
    }
  }
  else
  {
    connector = leadingLogicalWord(trimmed);
  }
  // A comparison or and/or needs a right-hand side.
  return connector > 0 && !trimCopy(std::string_view(trimmed).substr(connector)).empty();
}

} // namespace vertexadt::catalog
