#include "validate.h"
#include "utf16_text.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

bool isJsWordChar(unsigned char ch)
{
  return std::isalnum(ch) != 0 || ch == '_';
}

bool hasWordBoundaryAt(std::string_view text, std::size_t index)
{
  const bool prevWord = index > 0 && isJsWordChar(static_cast<unsigned char>(text[index - 1]));
  const bool curWord = index < text.size() && isJsWordChar(static_cast<unsigned char>(text[index]));
  return prevWord != curWord;
}

bool startsWithWord(std::string_view text, std::string_view word, bool caseInsensitive)
{
  if (text.size() < word.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < word.size(); ++i)
  {
    auto have = static_cast<unsigned char>(text[i]);
    auto want = static_cast<unsigned char>(word[i]);
    if (caseInsensitive)
    {
      have = static_cast<unsigned char>(std::tolower(have));
      want = static_cast<unsigned char>(std::tolower(want));
    }
    if (have != want)
    {
      return false;
    }
  }
  return hasWordBoundaryAt(text, 0) && hasWordBoundaryAt(text, word.size());
}

bool containsWord(std::string_view text, std::string_view word, bool caseInsensitive)
{
  if (word.empty() || text.size() < word.size())
  {
    return false;
  }
  for (std::size_t i = 0; i + word.size() <= text.size(); ++i)
  {
    bool match = true;
    for (std::size_t j = 0; j < word.size(); ++j)
    {
      auto have = static_cast<unsigned char>(text[i + j]);
      auto want = static_cast<unsigned char>(word[j]);
      if (caseInsensitive)
      {
        have = static_cast<unsigned char>(std::tolower(have));
        want = static_cast<unsigned char>(std::tolower(want));
      }
      if (have != want)
      {
        match = false;
        break;
      }
    }
    if (match && hasWordBoundaryAt(text, i) && hasWordBoundaryAt(text, i + word.size()))
    {
      return true;
    }
  }
  return false;
}

std::optional<std::size_t> findWord(std::string_view text, std::string_view word, bool caseInsensitive)
{
  if (word.empty() || text.size() < word.size())
  {
    return std::nullopt;
  }
  for (std::size_t i = 0; i + word.size() <= text.size(); ++i)
  {
    bool match = true;
    for (std::size_t j = 0; j < word.size(); ++j)
    {
      auto have = static_cast<unsigned char>(text[i + j]);
      auto want = static_cast<unsigned char>(word[j]);
      if (caseInsensitive)
      {
        have = static_cast<unsigned char>(std::tolower(have));
        want = static_cast<unsigned char>(std::tolower(want));
      }
      if (have != want)
      {
        match = false;
        break;
      }
    }
    if (match && hasWordBoundaryAt(text, i) && hasWordBoundaryAt(text, i + word.size()))
    {
      return i;
    }
  }
  return std::nullopt;
}

bool searchRegex(std::string_view text, const std::regex& re)
{
  return std::regex_search(text.data(), text.data() + text.size(), re);
}

auto stripQuotedStringsForIfCheck(std::string_view text) -> std::string
{
  std::string out;
  out.reserve(text.size());
  std::size_t i = 0;
  while (i < text.size())
  {
    const char ch = text[i];
    if (ch == '"' || ch == '\'')
    {
      const char quote = ch;
      out.push_back(quote);
      ++i;
      while (i < text.size() && text[i] != quote)
      {
        ++i;
      }
      out.push_back(quote);
      if (i < text.size())
      {
        ++i;
      }
      continue;
    }
    out.push_back(ch);
    ++i;
  }
  return out;
}

struct TextSpan
{
  std::size_t start = 0;
  std::size_t length = 0;
};

// The trimmed text between `if` and the first `then`, as a span of `line`.
auto extractIfConditionSpan(std::string_view line) -> std::optional<TextSpan>
{
  const auto stripped = stripValidationComment(line);
  const auto trimmed = trim(stripped);
  if (!startsWithWord(trimmed, "if", true))
  {
    return std::nullopt;
  }
  const auto thenPos = findWord(trimmed, "then", true);
  if (!thenPos)
  {
    return std::nullopt;
  }
  static const std::regex ifPrefixRe{R"(^\s*if\s+)", std::regex::icase};
  std::cmatch prefix;
  if (!std::regex_search(trimmed.data(), trimmed.data() + trimmed.size(), prefix, ifPrefixRe))
  {
    return std::nullopt;
  }
  if (static_cast<std::size_t>(prefix.length()) > *thenPos)
  {
    return std::nullopt;
  }
  const auto raw = trimmed.substr(
    static_cast<std::size_t>(prefix.length()),
    *thenPos - static_cast<std::size_t>(prefix.length()));
  const auto condition = trim(raw);
  return TextSpan{
    .start = static_cast<std::size_t>(condition.data() - stripped.data()),
    .length = condition.size(),
  };
}

auto extractIfConditionFromLine(std::string_view line) -> std::optional<std::string>
{
  const auto span = extractIfConditionSpan(line);
  if (!span)
  {
    return std::nullopt;
  }
  return std::string(line.substr(span->start, span->length));
}

bool hasDanglingIfLogicalOperator(std::string_view condition)
{
  if (condition.empty())
  {
    return false;
  }
  static const std::regex re{R"(\s+(and|or|&&|\|\|)\s*$)", std::regex::icase};
  const auto stripped = stripQuotedStringsForIfCheck(condition);
  return std::regex_search(stripped, re);
}

bool hasLeadingIfLogicalOperator(std::string_view condition)
{
  if (condition.empty())
  {
    return false;
  }
  const auto expr = std::string(trim(stripQuotedStringsForIfCheck(condition)));
  static const std::regex wordRe{R"(^(and|or)\b)", std::regex::icase};
  static const std::regex symbolRe{R"(^(&&|\|\|))"};
  return std::regex_search(expr, wordRe) || std::regex_search(expr, symbolRe);
}

auto getIfLeadingLogicalOperatorError(std::string_view condition) -> std::optional<std::string>
{
  if (hasLeadingIfLogicalOperator(condition))
  {
    return std::string{"Logical operator must not appear directly after if without a preceding condition."};
  }
  return std::nullopt;
}

auto getIfContinuationLinePlacementError(std::string_view trimmedLine) -> std::optional<std::string>
{
  static const std::regex wordThen{R"(^\s*(and|or)\b\s*then\b)", std::regex::icase};
  static const std::regex symbolThen{R"(^\s*(&&|\|\|)\s*then\b)", std::regex::icase};
  if (searchRegex(trimmedLine, wordThen) || searchRegex(trimmedLine, symbolThen))
  {
    return std::string{"Logical operator must not appear directly before then without a condition."};
  }
  static const std::regex wordOnly{R"(^\s*(and|or)\s*$)", std::regex::icase};
  static const std::regex symbolOnly{R"(^\s*(&&|\|\|)\s*$)"};
  if (searchRegex(trimmedLine, wordOnly) || searchRegex(trimmedLine, symbolOnly))
  {
    return std::string{"Logical and/or continuation line must include a condition after the operator."};
  }
  static const std::regex wordOp{R"(^\s*(and|or)\b)", std::regex::icase};
  static const std::regex wordOpCond{R"(^\s*(and|or)\b\s+.+)", std::regex::icase};
  if (searchRegex(trimmedLine, wordOp) && !searchRegex(trimmedLine, wordOpCond))
  {
    return std::string{"Logical and/or continuation line must include a condition after the operator."};
  }
  static const std::regex symbolOp{R"(^\s*(&&|\|\|))", std::regex::icase};
  static const std::regex symbolOpCond{R"(^\s*(&&|\|\|)\s*.+)", std::regex::icase};
  if (searchRegex(trimmedLine, symbolOp) && !searchRegex(trimmedLine, symbolOpCond))
  {
    return std::string{"Logical and/or continuation line must include a condition after the operator."};
  }
  return std::nullopt;
}

bool isContinuationOperatorStart(std::string_view trimmed)
{
  static const std::regex word{R"(^\s*(and|or)\b)", std::regex::icase};
  static const std::regex symbol{R"(^\s*(&&|\|\|))"};
  return searchRegex(trimmed, word) || searchRegex(trimmed, symbol);
}

auto getIfLogicalOperatorPlacementError(std::string_view line, bool pendingIfHeader)
  -> std::optional<std::string>
{
  const auto stripped = stripValidationComment(line);
  const auto trimmed = trim(stripped);
  if (trimmed.empty())
  {
    return std::nullopt;
  }

  if (const auto ifCondition = extractIfConditionFromLine(line))
  {
    if (auto leading = getIfLeadingLogicalOperatorError(*ifCondition))
    {
      return leading;
    }
    if (hasDanglingIfLogicalOperator(*ifCondition))
    {
      return std::string{"Logical operator must not appear directly before then without a condition."};
    }
  }

  if (startsWithWord(trimmed, "if", true) && !containsWord(trimmed, "then", true))
  {
    static const std::regex afterIfRe{R"(^\s*if\s+)", std::regex::icase};
    std::cmatch afterIf;
    const char* begin = trimmed.data();
    if (std::regex_search(begin, begin + trimmed.size(), afterIf, afterIfRe))
    {
      const auto after = trimmed.substr(static_cast<std::size_t>(afterIf.length()));
      if (auto leading = getIfLeadingLogicalOperatorError(after))
      {
        return leading;
      }
      if (hasDanglingIfLogicalOperator(after))
      {
        return std::string{"Logical operator at end of if line must continue on the next line with and/or."};
      }
    }
  }

  if (pendingIfHeader && isContinuationOperatorStart(trimmed))
  {
    return getIfContinuationLinePlacementError(trimmed);
  }
  return std::nullopt;
}

constexpr auto kNoOffset = std::string_view::npos;

struct OperatorScope
{
  bool hasAnd = false;
  bool hasOr = false;
  std::size_t firstAnd = kNoOffset;
  std::size_t firstOr = kNoOffset;
};

void recordIfLogicalOperator(OperatorScope& scope, bool isAnd, std::size_t offset)
{
  if (isAnd)
  {
    scope.hasAnd = true;
    scope.firstAnd = std::min(scope.firstAnd, offset);
  }
  else
  {
    scope.hasOr = true;
    scope.firstOr = std::min(scope.firstOr, offset);
  }
}

// An operator error and the condition offset of the operator that caused it:
// the first one of the second kind.
struct OperatorError
{
  std::string message;
  std::size_t offset = 0;
};

auto validateIfOperatorScope(const OperatorScope& scope) -> std::optional<OperatorError>
{
  if (scope.hasAnd && scope.hasOr)
  {
    return OperatorError{
      .message = "If condition with both and and or requires parentheses to clarify precedence.",
      .offset = std::max(scope.firstAnd, scope.firstOr),
    };
  }
  return std::nullopt;
}

// stripQuotedStringsForIfCheck with the quoted text blanked instead of
// dropped, so offsets still match the condition.
auto blankQuotedStrings(std::string_view text) -> std::string
{
  std::string out(text);
  char quote = 0;
  for (auto& ch : out)
  {
    if (quote != 0)
    {
      if (ch == quote)
      {
        quote = 0;
      }
      else
      {
        ch = ' ';
      }
    }
    else if (ch == '"' || ch == '\'')
    {
      quote = ch;
    }
  }
  return out;
}

bool matchWordOperator(std::string_view remainder, std::string_view word, std::size_t& length)
{
  std::size_t i = 0;
  while (i < remainder.size() && std::isspace(static_cast<unsigned char>(remainder[i])) != 0)
  {
    ++i;
  }
  if (i == 0)
  {
    return false;
  }
  if (i + word.size() > remainder.size())
  {
    return false;
  }
  for (std::size_t j = 0; j < word.size(); ++j)
  {
    if (std::tolower(static_cast<unsigned char>(remainder[i + j])) != word[j])
    {
      return false;
    }
  }
  if (!hasWordBoundaryAt(remainder, i) || !hasWordBoundaryAt(remainder, i + word.size()))
  {
    return false;
  }
  length = i + word.size();
  return true;
}

auto ifConditionOperatorError(std::string_view condition) -> std::optional<OperatorError>
{
  if (condition.empty())
  {
    return std::nullopt;
  }
  const auto expr = blankQuotedStrings(condition);
  std::vector<OperatorScope> scopeStack{OperatorScope{}};
  bool hasWordStyle = false;
  bool hasSymbolStyle = false;
  bool anyOperator = false;
  std::size_t firstWord = kNoOffset;
  std::size_t firstSymbol = kNoOffset;

  for (std::size_t i = 0; i < expr.size(); ++i)
  {
    const char ch = expr[i];
    if (ch == '(')
    {
      scopeStack.push_back(OperatorScope{});
      continue;
    }
    if (ch == ')')
    {
      if (scopeStack.size() > 1)
      {
        if (auto error = validateIfOperatorScope(scopeStack.back()))
        {
          return error;
        }
        scopeStack.pop_back();
      }
      continue;
    }
    if (ch == '&' && i + 1 < expr.size() && expr[i + 1] == '&')
    {
      anyOperator = true;
      hasSymbolStyle = true;
      firstSymbol = std::min(firstSymbol, i);
      recordIfLogicalOperator(scopeStack.back(), true, i);
      ++i;
      continue;
    }
    if (ch == '|' && i + 1 < expr.size() && expr[i + 1] == '|')
    {
      anyOperator = true;
      hasSymbolStyle = true;
      firstSymbol = std::min(firstSymbol, i);
      recordIfLogicalOperator(scopeStack.back(), false, i);
      ++i;
      continue;
    }
    const auto remainder = std::string_view(expr).substr(i);
    std::size_t length = 0;
    if (matchWordOperator(remainder, "and", length))
    {
      const auto at = i + length - 3;
      anyOperator = true;
      hasWordStyle = true;
      firstWord = std::min(firstWord, at);
      recordIfLogicalOperator(scopeStack.back(), true, at);
      i += length - 1;
      continue;
    }
    if (matchWordOperator(remainder, "or", length))
    {
      const auto at = i + length - 2;
      anyOperator = true;
      hasWordStyle = true;
      firstWord = std::min(firstWord, at);
      recordIfLogicalOperator(scopeStack.back(), false, at);
      i += length - 1;
    }
  }

  if (!anyOperator)
  {
    return std::nullopt;
  }
  if (hasWordStyle && hasSymbolStyle)
  {
    return OperatorError{
      .message =
        "If condition must not mix word operators (and/or) with symbol operators (&&/||). Use one style consistently or add parentheses.",
      .offset = std::max(firstWord, firstSymbol),
    };
  }
  for (std::size_t s = scopeStack.size(); s-- > 1;)
  {
    if (auto error = validateIfOperatorScope(scopeStack[s]))
    {
      return error;
    }
  }
  return validateIfOperatorScope(scopeStack.front());
}

auto getIfConditionOperatorError(std::optional<std::string> condition) -> std::optional<std::string>
{
  if (!condition)
  {
    return std::nullopt;
  }
  auto error = ifConditionOperatorError(*condition);
  if (!error)
  {
    return std::nullopt;
  }
  return std::move(error->message);
}

bool advancePendingIfHeaderState(std::string_view trimmedLine, bool current)
{
  if (startsWithWord(trimmedLine, "endif", true) || startsWithWord(trimmedLine, "else", true))
  {
    return false;
  }
  const bool hasIf = startsWithWord(trimmedLine, "if", true);
  const bool hasThen = containsWord(trimmedLine, "then", true);
  if (hasIf && !hasThen)
  {
    return true;
  }
  if (hasThen && current)
  {
    return false;
  }
  if (current)
  {
    return isContinuationOperatorStart(trimmedLine);
  }
  return false;
}

struct LineFlags
{
  bool startsWithIf = false;
  bool startsWithEndif = false;
  bool startsWithLoop = false;
  bool startsWithEndloop = false;
  bool startsWithElse = false;
  bool startsWithInclude = false;
  bool beginsWithIfKeyword = false;
  bool beginsWithLoopKeyword = false;
  bool hasThen = false;
  bool inlineIfThen = false;
};

LineFlags classify(const PerLineState& line)
{
  LineFlags flags;
  const auto trimmed = line.trimmed;
  flags.startsWithIf = trimmed.starts_with("if");
  flags.startsWithEndif = trimmed.starts_with("endif");
  flags.startsWithLoop = trimmed.starts_with("loop");
  flags.startsWithEndloop = trimmed.starts_with("endloop");
  flags.startsWithElse = trimmed.starts_with("else");
  flags.startsWithInclude = trimmed.starts_with("# include");
  flags.beginsWithIfKeyword = startsWithWord(trimmed, "if", false);
  flags.beginsWithLoopKeyword = startsWithWord(trimmed, "loop", false);
  flags.hasThen = containsWord(line.stripped, "then", true);
  flags.inlineIfThen = isInlineIfThen(trimmed);
  return flags;
}

auto lineStartOffset(const ValidateContext& ctx, int line) -> std::size_t
{
  std::size_t offset = 0;
  const int n = static_cast<int>(ctx.prep->lines.size());
  const int count = line < n ? line : n;
  for (int i = 0; i < count; ++i)
  {
    offset += ctx.prep->lines[static_cast<std::size_t>(i)].size();
    if (i < n - 1)
    {
      ++offset;
    }
  }
  return offset;
}

auto positionAt(const ValidateContext& ctx, std::size_t offset) -> lsp::Position
{
  const auto n = ctx.prep->snapshot.text.size();
  if (offset > n)
  {
    offset = n;
  }
  std::size_t lineStart = 0;
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    const auto lineLen = ctx.prep->lines[static_cast<std::size_t>(i)].size();
    const bool last = i == static_cast<int>(ctx.prep->lines.size()) - 1;
    const auto extent = last ? lineLen : lineLen + 1;
    if (offset < lineStart + extent || last)
    {
      auto character = offset - lineStart;
      if (character > lineLen)
      {
        character = lineLen;
      }
      return lsp::Position{
        .line = static_cast<lsp::Uint>(i),
        .character = static_cast<lsp::Uint>(
          utf16Offset(ctx.prep->lines[static_cast<std::size_t>(i)], character)),
      };
    }
    lineStart += extent;
  }
  return lsp::Position{
    .line = static_cast<lsp::Uint>(ctx.prep->lines.size()),
    .character = 0,
  };
}

lsp::Range rangeFromOffsets(const ValidateContext& ctx, std::size_t start, std::size_t end)
{
  return lsp::Range{.start = positionAt(ctx, start), .end = positionAt(ctx, end)};
}

lsp::Diagnostic makeDiagnostic(
  const ValidateContext& ctx,
  lsp::Range range,
  std::string message,
  lsp::DiagnosticSeverity severity,
  std::string related)
{
  lsp::Diagnostic diagnostic;
  diagnostic.range = range;
  diagnostic.message = std::move(message);
  diagnostic.severity = severity;
  diagnostic.source = std::string{"ADT-Syntax"};
  diagnostic.relatedInformation = std::vector<lsp::DiagnosticRelatedInformation>{
    lsp::DiagnosticRelatedInformation{
      .location = lsp::Location{.uri = ctx.prep->snapshot.uri, .range = range},
      .message = std::move(related),
    },
  };
  return diagnostic;
}

lsp::Diagnostic wrappedDiagnostic(
  const ValidateContext& ctx,
  int line,
  std::string_view lineText,
  std::string inner,
  lsp::DiagnosticSeverity severity)
{
  const auto offset = lineStartOffset(ctx, line);
  const auto range = rangeFromOffsets(ctx, offset + 1, offset + lineText.size());
  auto message = std::string(lineText) + " has an error: " + inner;
  return makeDiagnostic(ctx, range, std::move(message), severity, std::move(inner));
}

std::string_view originalLine(const ValidateContext& ctx, int line)
{
  if (line < 0 || line >= static_cast<int>(ctx.prep->lines.size()))
  {
    return {};
  }
  return ctx.prep->lines[static_cast<std::size_t>(line)];
}

void updateStacksSilently(ValidateContext& ctx, const PerLineState& line, const LineFlags& flags)
{
  auto& state = ctx.ifLoop;
  if (flags.startsWithIf && !flags.inlineIfThen)
  {
    state.ifStack.push_back(IfLoopIndentState::Frame{line.index, line.indentation});
  }
  else if (flags.startsWithEndif && !state.ifStack.empty())
  {
    state.ifStack.pop_back();
  }

  if (flags.startsWithLoop)
  {
    state.loopStack.push_back(IfLoopIndentState::Frame{line.index, line.indentation});
  }
  else if (flags.startsWithEndloop && !state.loopStack.empty())
  {
    state.loopStack.pop_back();
  }

  if ((flags.beginsWithIfKeyword || flags.beginsWithLoopKeyword) && !flags.inlineIfThen)
  {
    state.indentStack.push_back(
      IfLoopIndentState::IndentFrame{.opener = line.indentation, .isLoop = flags.beginsWithLoopKeyword});
  }
  else if (flags.startsWithEndif || flags.startsWithEndloop || flags.startsWithElse)
  {
    if (!state.indentStack.empty() && !flags.startsWithElse)
    {
      state.indentStack.pop_back();
    }
  }
}

void matchIfEndif(ValidateContext& ctx, const PerLineState& line, const LineFlags& flags)
{
  if (ctx.atCap())
  {
    return;
  }
  auto& stack = ctx.ifLoop.ifStack;
  if (flags.startsWithIf)
  {
    if (!flags.inlineIfThen)
    {
      stack.push_back(IfLoopIndentState::Frame{line.index, line.indentation});
    }
    return;
  }
  if (!flags.startsWithEndif)
  {
    return;
  }
  if (stack.empty())
  {
    ctx.append(wrappedDiagnostic(
      ctx,
      line.index,
      line.stripped,
      "endif without matching if",
      lsp::DiagnosticSeverity::Error));
    return;
  }
  stack.pop_back();
}

void matchLoopEndloop(ValidateContext& ctx, const PerLineState& line, const LineFlags& flags)
{
  if (ctx.atCap())
  {
    return;
  }
  auto& stack = ctx.ifLoop.loopStack;
  if (flags.startsWithLoop)
  {
    stack.push_back(IfLoopIndentState::Frame{line.index, line.indentation});
    return;
  }
  if (!flags.startsWithEndloop)
  {
    return;
  }
  if (stack.empty())
  {
    ctx.append(wrappedDiagnostic(
      ctx,
      line.index,
      line.stripped,
      "endloop without matching loop",
      lsp::DiagnosticSeverity::Error));
    return;
  }
  const auto top = stack.back();
  if (top.indentation != line.indentation)
  {
    ctx.append(wrappedDiagnostic(
      ctx,
      line.index,
      line.stripped,
      "endloop at different indentation level",
      lsp::DiagnosticSeverity::Warning));
  }
  stack.pop_back();
}

// `if` as the first word, case-sensitive; `$` continues the word (ADR 0013).
bool startsWithIfKeyword(std::string_view trimmed)
{
  return trimmed.starts_with("if")
    && (trimmed.size() == 2 || (!isJsWordChar(static_cast<unsigned char>(trimmed[2])) && trimmed[2] != '$'));
}

bool isIfHeaderContinuation(ValidateContext& ctx, int line)
{
  return ifHeaderLinesFor(ctx).startByContinuation.contains(line);
}

auto continuedIfHeaderAt(ValidateContext& ctx, int line) -> const JoinedLines*
{
  const auto& headers = ifHeaderLinesFor(ctx).headerByStart;
  const auto it = headers.find(line);
  return it == headers.end() ? nullptr : &it->second;
}

// A `\`-continued if header seen as one line: the joined text in place of the
// first line's stripped text.
struct HeaderView
{
  PerLineState line;
  LineFlags flags;
};

// Filled in place: `line.trimmed` views into `line.stripped`.
void viewHeader(const PerLineState& first, const JoinedLines& header, HeaderView& view)
{
  view.line.index = first.index;
  view.line.raw = first.raw;
  view.line.indentation = first.indentation;
  view.line.stripped = header.text;
  view.line.trimmed = trim(view.line.stripped);
  view.flags = classify(view.line);
  if (header.danglingBackslash)
  {
    // A broken header opens a block, as an if without then does.
    view.flags.inlineIfThen = false;
  }
}

// Flags for a line, or for the whole header when `\` continues it.
LineFlags classifyLogical(ValidateContext& ctx, const PerLineState& line)
{
  if (const auto* header = continuedIfHeaderAt(ctx, line.index))
  {
    HeaderView view;
    viewHeader(line, *header, view);
    return view.flags;
  }
  return classify(line);
}

bool nextPendingIfHeader(ValidateContext& ctx, const PerLineState& line)
{
  if (const auto* header = continuedIfHeaderAt(ctx, line.index))
  {
    if (header->danglingBackslash)
    {
      return true;
    }
    return advancePendingIfHeaderState(trim(header->text), ctx.ifLoop.pendingIfHeader);
  }
  return advancePendingIfHeaderState(line.trimmed, ctx.ifLoop.pendingIfHeader);
}

std::string strippedLine(const ValidateContext& ctx, int line)
{
  return stripValidationComment(originalLine(ctx, line));
}

constexpr std::string_view kDanglingContinuation = "Line continuation '\\' must be followed by a code line.";

void checkContinuedIfHeader(ValidateContext& ctx, const PerLineState& line, const JoinedLines& header)
{
  HeaderView view;
  viewHeader(line, header, view);
  const auto& joined = view.line;

  if (header.danglingBackslash)
  {
    if (!ctx.atCap())
    {
      const auto at = lineStartOffset(ctx, header.lastLine) + *header.danglingBackslash;
      ctx.append(makeDiagnostic(
        ctx,
        rangeFromOffsets(ctx, at, at + 1),
        std::string(kDanglingContinuation),
        lsp::DiagnosticSeverity::Error,
        std::string(kDanglingContinuation)));
    }
    ctx.ifLoop.pendingIfHeader = true;
    matchIfEndif(ctx, line, view.flags);
    return;
  }

  if (!ctx.atCap())
  {
    if (auto placement = getIfLogicalOperatorPlacementError(joined.stripped, ctx.ifLoop.pendingIfHeader))
    {
      ctx.append(wrappedDiagnostic(
        ctx,
        line.index,
        line.stripped,
        std::move(*placement),
        lsp::DiagnosticSeverity::Error));
    }
  }

  if (!ctx.atCap() && !containsWord(joined.stripped, "then", false))
  {
    const auto offset = lineStartOffset(ctx, line.index);
    const auto range = rangeFromOffsets(ctx, offset, offset + line.stripped.size());
    ctx.append(makeDiagnostic(
      ctx,
      range,
      line.stripped + " has an error.",
      lsp::DiagnosticSeverity::Error,
      "If statement must be followed by a then."));
  }
  if (view.flags.hasThen && !ctx.atCap())
  {
    if (const auto span = extractIfConditionSpan(joined.stripped))
    {
      const auto condition = std::string_view(joined.stripped).substr(span->start, span->length);
      if (auto mix = ifConditionOperatorError(condition))
      {
        // Reported on the physical line that holds the offending operator.
        const auto where = header.locate(span->start + mix->offset).line;
        ctx.append(wrappedDiagnostic(
          ctx,
          where,
          strippedLine(ctx, where),
          std::move(mix->message),
          lsp::DiagnosticSeverity::Error));
      }
    }
  }

  ctx.ifLoop.pendingIfHeader = advancePendingIfHeaderState(joined.trimmed, ctx.ifLoop.pendingIfHeader);
  matchIfEndif(ctx, line, view.flags);
}

} // namespace

const IfHeaderLines& ifHeaderLinesFor(ValidateContext& ctx)
{
  if (!ctx.ifHeaderLines)
  {
    IfHeaderLines map;
    const auto& lines = ctx.prep->lines;
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
      if (!endsWithContinuation(lines[i]))
      {
        continue;
      }
      auto joined = joinContinuedLines(lines, i);
      const auto last = static_cast<std::size_t>(joined.lastLine);
      // Lines continuing any other statement are skipped, so they never
      // start a header themselves.
      if (startsWithIfKeyword(trim(stripValidationComment(lines[i]))))
      {
        for (auto k = i + 1; k <= last; ++k)
        {
          map.startByContinuation.emplace(static_cast<int>(k), static_cast<int>(i));
        }
        map.headerByStart.emplace(static_cast<int>(i), std::move(joined));
      }
      for (auto k = i + 1; k <= last; ++k)
      {
        map.anyContinuation.insert(static_cast<int>(k));
      }
      i = last;
    }
    ctx.ifHeaderLines = std::move(map);
  }
  return *ctx.ifHeaderLines;
}

void silentIfLoopIndentStackUpdate(ValidateContext& ctx, PerLineState& line)
{
  if (isIfHeaderContinuation(ctx, line.index))
  {
    return;
  }
  const auto flags = classifyLogical(ctx, line);
  updateStacksSilently(ctx, line, flags);
  ctx.ifLoop.pendingIfHeader = nextPendingIfHeader(ctx, line);
}

void familyLeadingDigit(ValidateContext& ctx, PerLineState& line)
{
  if (ctx.atCap())
  {
    return;
  }
  static const std::regex pattern{R"(^\s*[0-9][A-Za-z0-9_-]*\s*=\s*)"};
  std::cmatch match;
  const auto& stripped = line.stripped;
  if (!std::regex_search(stripped.c_str(), stripped.c_str() + stripped.size(), match, pattern))
  {
    return;
  }
  const auto matchStart = lineStartOffset(ctx, line.index) + static_cast<std::size_t>(match.position());
  const auto range = rangeFromOffsets(ctx, matchStart, matchStart + static_cast<std::size_t>(match.length()));
  auto message = match.str() + " has an error.";
  ctx.append(makeDiagnostic(
    ctx,
    range,
    std::move(message),
    lsp::DiagnosticSeverity::Error,
    "Variable can't start with a number."));
}

void familyIfLogicalAndMatching(ValidateContext& ctx, PerLineState& line)
{
  // Continuation lines were checked with their header's first line.
  if (isIfHeaderContinuation(ctx, line.index))
  {
    return;
  }
  if (const auto* header = continuedIfHeaderAt(ctx, line.index))
  {
    checkContinuedIfHeader(ctx, line, *header);
    return;
  }

  const auto flags = classify(line);

  if (!ctx.atCap())
  {
    if (auto placement = getIfLogicalOperatorPlacementError(line.stripped, ctx.ifLoop.pendingIfHeader))
    {
      ctx.append(wrappedDiagnostic(
        ctx,
        line.index,
        line.stripped,
        std::move(*placement),
        lsp::DiagnosticSeverity::Error));
    }
  }

  if (flags.startsWithIf)
  {
    if (!ctx.atCap() && containsWord(line.stripped, "if", false) && !containsWord(line.stripped, "then", false))
    {
      const auto offset = lineStartOffset(ctx, line.index);
      const auto range = rangeFromOffsets(ctx, offset, offset + line.stripped.size());
      auto message = line.stripped + " has an error.";
      ctx.append(makeDiagnostic(
        ctx,
        range,
        std::move(message),
        lsp::DiagnosticSeverity::Error,
        "If statement must be followed by a then."));
    }
    if (flags.hasThen && !ctx.atCap())
    {
      if (auto mix = getIfConditionOperatorError(extractIfConditionFromLine(line.stripped)))
      {
        ctx.append(wrappedDiagnostic(
          ctx,
          line.index,
          line.stripped,
          std::move(*mix),
          lsp::DiagnosticSeverity::Error));
      }
    }
  }

  ctx.ifLoop.pendingIfHeader = advancePendingIfHeaderState(line.trimmed, ctx.ifLoop.pendingIfHeader);
  matchIfEndif(ctx, line, flags);
  matchLoopEndloop(ctx, line, flags);
}

void familyEqualsAngle(ValidateContext& ctx, PerLineState& line)
{
  const auto& code = line.codeLine;
  for (std::size_t i = 0; i + 1 < code.size(); ++i)
  {
    if (code[i] == '=' && code[i + 1] == '<')
    {
      if (ctx.atCap())
      {
        return;
      }
      ctx.append(wrappedDiagnostic(
        ctx,
        line.index,
        line.stripped,
        "Incorrect operator =<, try: <=",
        lsp::DiagnosticSeverity::Error));
      ++i;
    }
  }
  for (std::size_t i = 0; i + 1 < code.size(); ++i)
  {
    if (code[i] == '=' && code[i + 1] == '>')
    {
      if (ctx.atCap())
      {
        return;
      }
      ctx.append(wrappedDiagnostic(
        ctx,
        line.index,
        line.stripped,
        "Incorrect operator =>, try: >=",
        lsp::DiagnosticSeverity::Error));
      ++i;
    }
  }
}

namespace {

bool opensIndentBlock(const LineFlags& flags)
{
  return (flags.beginsWithIfKeyword || flags.beginsWithLoopKeyword) && !flags.inlineIfThen;
}

// The file's indent step (docs/adr/0026-native-over-indent-warnings.md): the
// most common positive gap between an if/loop line and its block's first body
// line, in raw characters. Walks the same lines and stacks as familyIndentLine.
int indentStepFor(ValidateContext& ctx)
{
  auto& cached = ctx.ifLoop.indentStep;
  if (cached)
  {
    return *cached;
  }
  const auto& continuations = ifHeaderLinesFor(ctx).anyContinuation;
  struct Open
  {
    int opener = -1;
    bool sawBody = false;
  };
  std::vector<Open> stack;
  std::unordered_map<int, int> gapCounts;
  const auto& lines = ctx.prep->lines;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto raw = lines[static_cast<std::size_t>(i)];
    if (isSharedPerLineSkip(raw) || continuations.contains(i))
    {
      continue;
    }
    PerLineState line;
    line.index = i;
    line.raw = raw;
    line.stripped = stripValidationComment(raw);
    line.trimmed = trim(line.stripped);
    if (line.trimmed.empty())
    {
      continue;
    }
    line.indentation = static_cast<int>(line.trimmed.data() - line.stripped.data());
    const auto flags = classifyLogical(ctx, line);
    if (flags.startsWithEndif || flags.startsWithEndloop || flags.startsWithElse)
    {
      if (!flags.startsWithElse && !stack.empty())
      {
        stack.pop_back();
      }
      continue;
    }
    if (!stack.empty() && !stack.back().sawBody && !flags.startsWithInclude)
    {
      stack.back().sawBody = true;
      const auto gap = line.indentation - stack.back().opener;
      if (gap > 0)
      {
        ++gapCounts[gap];
      }
    }
    if (opensIndentBlock(flags))
    {
      stack.push_back(Open{line.indentation});
    }
  }
  // The most common gap; a tie goes to the smaller one.
  int step = 0;
  int best = 0;
  for (const auto& [gap, count] : gapCounts)
  {
    if (count > best || (count == best && gap < step))
    {
      step = gap;
      best = count;
    }
  }
  cached = step;
  return step;
}

auto blockWord(const IfLoopIndentState::IndentFrame& frame) -> std::string
{
  return frame.isLoop ? "'loop'" : "'if'";
}

// A direct body line of the innermost open block (an if/loop opener included).
void checkBodyLine(ValidateContext& ctx, const PerLineState& line, const LineFlags& flags)
{
  auto& stack = ctx.ifLoop.indentStack;
  if (stack.empty() || flags.startsWithInclude || line.indentation < 0)
  {
    return;
  }
  auto& top = stack.back();
  if (line.indentation <= top.opener)
  {
    ctx.append(wrappedDiagnostic(
      ctx,
      line.index,
      line.stripped,
      "This line should be more indented than the matching 'if' or 'loop'.",
      lsp::DiagnosticSeverity::Warning));
    return;
  }
  // Continuation lines of other `\` groups keep their own offsets.
  if (ifHeaderLinesFor(ctx).anyContinuation.contains(line.index))
  {
    return;
  }
  if (top.body < 0)
  {
    top.body = line.indentation;
    const auto step = indentStepFor(ctx);
    if (step > 0 && line.indentation - top.opener > step)
    {
      ctx.append(wrappedDiagnostic(
        ctx,
        line.index,
        line.stripped,
        "This line is indented more than one level deeper than the matching " + blockWord(top) + ".",
        lsp::DiagnosticSeverity::Warning));
    }
    return;
  }
  if (line.indentation > top.body)
  {
    ctx.append(wrappedDiagnostic(
      ctx,
      line.index,
      line.stripped,
      "This line is indented more than the other lines in this " + blockWord(top) + " block.",
      lsp::DiagnosticSeverity::Warning));
  }
}

} // namespace

void familyIndentLine(ValidateContext& ctx, PerLineState& line)
{
  if (ctx.atCap() || isIfHeaderContinuation(ctx, line.index))
  {
    return;
  }
  const auto flags = classifyLogical(ctx, line);
  auto& stack = ctx.ifLoop.indentStack;
  if (flags.startsWithInclude && line.indentation > 0)
  {
    ctx.append(wrappedDiagnostic(
      ctx,
      line.index,
      line.stripped,
      "Lines starting with '# include' should not be indented.",
      lsp::DiagnosticSeverity::Warning));
    return;
  }

  if (flags.startsWithEndif || flags.startsWithEndloop || flags.startsWithElse)
  {
    if (stack.empty())
    {
      ctx.append(wrappedDiagnostic(
        ctx,
        line.index,
        line.stripped,
        "Found an 'endif', 'endloop', or 'else' without a matching 'if' or 'loop'.",
        lsp::DiagnosticSeverity::Warning));
    }
    else if (flags.startsWithEndloop)
    {
      // Deeper endloop lines are reported by "endloop at different indentation level".
      if (line.indentation < stack.back().opener)
      {
        ctx.append(wrappedDiagnostic(
          ctx,
          line.index,
          line.stripped,
          "This line should be more indented than the matching 'if' or 'loop'.",
          lsp::DiagnosticSeverity::Warning));
      }
    }
    else if (line.indentation >= 0 && line.indentation != stack.back().opener)
    {
      ctx.append(wrappedDiagnostic(
        ctx,
        line.index,
        line.stripped,
        "This line should line up with the matching " + blockWord(stack.back()) + ".",
        lsp::DiagnosticSeverity::Warning));
    }
    if (!flags.startsWithElse && !stack.empty())
    {
      stack.pop_back();
    }
    return;
  }

  checkBodyLine(ctx, line, flags);
  if (opensIndentBlock(flags))
  {
    stack.push_back(
      IfLoopIndentState::IndentFrame{.opener = line.indentation, .isLoop = flags.beginsWithLoopKeyword});
  }
}

void familyLeftoverIf(ValidateContext& ctx)
{
  for (const auto& frame : ctx.ifLoop.ifStack)
  {
    const auto text = originalLine(ctx, frame.line);
    ctx.append(
      wrappedDiagnostic(
        ctx,
        frame.line,
        text,
        "if without matching endif",
        lsp::DiagnosticSeverity::Error),
      true,
      true);
  }
}

void familyLeftoverLoop(ValidateContext& ctx)
{
  for (const auto& frame : ctx.ifLoop.loopStack)
  {
    const auto text = originalLine(ctx, frame.line);
    ctx.append(
      wrappedDiagnostic(
        ctx,
        frame.line,
        text,
        "loop without matching endloop",
        lsp::DiagnosticSeverity::Error),
      true,
      true);
  }
}

void familyLeftoverIndent(ValidateContext& ctx)
{
  const auto n = ctx.ifLoop.indentStack.size();
  if (n == 0)
  {
    return;
  }
  auto inner = "Found " + std::to_string(n) + " 'if' or 'loop' without a matching 'endif' or 'endloop'.";
  ctx.append(
    wrappedDiagnostic(
      ctx,
      static_cast<int>(ctx.prep->lines.size()),
      "",
      std::move(inner),
      lsp::DiagnosticSeverity::Warning),
    true,
    false);
}

} // namespace vertexadt
