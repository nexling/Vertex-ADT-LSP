#include "substring_index.h"

#include "text_util.h"
#include "utf16_text.h"
#include "validate.h"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>

namespace vertexadt {
namespace {

// ASCII letters, digits, `_`, and any UTF-8 byte (names like `å$`).
bool isIdentChar(char ch)
{
  const auto byte = static_cast<unsigned char>(ch);
  return byte >= 0x80 || std::isalnum(byte) != 0 || ch == '_';
}

bool isBlank(char ch)
{
  return ch == ' ' || ch == '\t';
}

struct Span
{
  std::size_t start = 0;
  std::size_t end = 0;
};

auto trimSpan(std::string_view line, Span span) -> Span
{
  while (span.start < span.end && isBlank(line[span.start]))
  {
    ++span.start;
  }
  while (span.end > span.start && isBlank(line[span.end - 1]))
  {
    --span.end;
  }
  return span;
}

// `[+-]?digits`, else nullopt (variables, expressions, overflow).
auto literalInteger(std::string_view text) -> std::optional<long long>
{
  if (text.empty())
  {
    return std::nullopt;
  }
  bool negative = false;
  if (text.front() == '+' || text.front() == '-')
  {
    negative = text.front() == '-';
    text.remove_prefix(1);
  }
  if (text.empty())
  {
    return std::nullopt;
  }
  for (const auto ch : text)
  {
    if (std::isdigit(static_cast<unsigned char>(ch)) == 0)
    {
      return std::nullopt;
    }
  }
  long long value = 0;
  const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || ptr != text.data() + text.size())
  {
    return std::nullopt;
  }
  return negative ? -value : value;
}

struct BracketArgs
{
  std::vector<Span> args;
  std::size_t close = 0; // index of the matching `]`
};

// Top-level comma-separated arguments from just after `[` to its matching `]`.
// Nested `[]` / `()` and quotes are skipped. nullopt when the line ends (or an
// unquoted `!` comment starts) before the bracket closes.
auto parseBracketArgs(std::string_view line, std::size_t open) -> std::optional<BracketArgs>
{
  BracketArgs out;
  int depth = 0;
  char quote = 0;
  std::size_t argStart = open + 1;
  for (std::size_t i = open + 1; i < line.size(); ++i)
  {
    const auto ch = line[i];
    if (quote)
    {
      if (ch == quote)
      {
        quote = 0;
      }
      continue;
    }
    switch (ch)
    {
    case '"':
    case '\'':
      quote = ch;
      break;
    case '!':
      return std::nullopt;
    case '[':
    case '(':
      ++depth;
      break;
    case ')':
      --depth;
      break;
    case ']':
      if (depth == 0)
      {
        out.args.push_back(Span{argStart, i});
        out.close = i;
        return out;
      }
      --depth;
      break;
    case ',':
      if (depth == 0)
      {
        out.args.push_back(Span{argStart, i});
        argStart = i + 1;
      }
      break;
    default:
      break;
    }
  }
  return std::nullopt;
}

// `name$[` begins a statement: line start, after `;`, or after a `then` word.
bool atStatementStart(std::string_view line, std::size_t nameStart)
{
  auto k = nameStart;
  while (k > 0 && isBlank(line[k - 1]))
  {
    --k;
  }
  if (k == 0 || line[k - 1] == ';')
  {
    return true;
  }
  if (k < 4)
  {
    return false;
  }
  const auto word = line.substr(k - 4, 4);
  if (toLowerAscii(word) != "then")
  {
    return false;
  }
  return k == 4 || !isIdentChar(line[k - 5]);
}

// A single `=` (not `==`) after the closing `]`, blanks allowed.
bool followedByAssignment(std::string_view line, std::size_t close)
{
  auto p = close + 1;
  while (p < line.size() && isBlank(line[p]))
  {
    ++p;
  }
  return p < line.size() && line[p] == '='
    && (p + 1 >= line.size() || line[p + 1] != '=');
}

void checkSubstring(
  std::string_view line,
  std::size_t nameStart,
  std::size_t open,
  std::vector<SubstringIssue>& issues)
{
  const auto parsed = parseBracketArgs(line, open);
  if (!parsed || parsed->args.size() != 2)
  {
    return;
  }
  const auto startSpan = trimSpan(line, parsed->args[0]);
  const auto endSpan = trimSpan(line, parsed->args[1]);
  const auto startText = line.substr(startSpan.start, startSpan.end - startSpan.start);
  const auto endText = line.substr(endSpan.start, endSpan.end - endSpan.start);
  const auto start = literalInteger(startText);
  const auto end = literalInteger(endText);

  if (start && *start < 1)
  {
    issues.push_back(SubstringIssue{
      .kind = SubstringIssueKind::StartBelowOne,
      .startByte = startSpan.start,
      .endByte = startSpan.end,
      .message = "Substring start index must be 1 or higher (strings are 1-indexed); got "
        + std::string(startText) + ".",
    });
  }
  if (end && *end < 0)
  {
    issues.push_back(SubstringIssue{
      .kind = SubstringIssueKind::NegativeEnd,
      .startByte = endSpan.start,
      .endByte = endSpan.end,
      .message = "Substring end index must be 0 (to end) or a positive position; got "
        + std::string(endText) + ".",
    });
  }
  if (start && end && *start >= 1 && *end > 0 && *start > *end)
  {
    issues.push_back(SubstringIssue{
      .kind = SubstringIssueKind::StartAfterEnd,
      .startByte = open + 1,
      .endByte = parsed->close,
      .message = "Substring start " + std::to_string(*start) + " is after end "
        + std::to_string(*end) + "; result is empty.",
    });
  }
  if (atStatementStart(line, nameStart) && followedByAssignment(line, parsed->close))
  {
    issues.push_back(SubstringIssue{
      .kind = SubstringIssueKind::Assignment,
      .startByte = nameStart,
      .endByte = parsed->close + 1,
      .message = "Cannot assign to a substring (A$[i,j] = ...); build the new string with "
                 "concatenation instead.",
    });
  }
}

auto severityFor(SubstringIssueKind kind) -> lsp::DiagnosticSeverity
{
  switch (kind)
  {
  case SubstringIssueKind::NegativeEnd:
  case SubstringIssueKind::StartAfterEnd:
    return lsp::DiagnosticSeverity::Warning;
  default:
    return lsp::DiagnosticSeverity::Error;
  }
}

auto codeFor(SubstringIssueKind kind) -> std::string_view
{
  switch (kind)
  {
  case SubstringIssueKind::StartBelowOne:
    return kSubstringStartIndexCode;
  case SubstringIssueKind::NegativeEnd:
    return kSubstringEndIndexCode;
  case SubstringIssueKind::StartAfterEnd:
    return kSubstringEmptyRangeCode;
  case SubstringIssueKind::Assignment:
    return kSubstringAssignmentCode;
  }
  return kSubstringStartIndexCode;
}

} // namespace

auto findSubstringIssuesInLine(std::string_view line) -> std::vector<SubstringIssue>
{
  std::vector<SubstringIssue> issues;
  char quote = 0;
  for (std::size_t i = 0; i < line.size(); ++i)
  {
    const auto ch = line[i];
    if (quote)
    {
      if (ch == quote)
      {
        quote = 0;
      }
      continue;
    }
    if (ch == '"' || ch == '\'')
    {
      quote = ch;
      continue;
    }
    if (ch == '!')
    {
      break;
    }
    if (ch != '$' || i + 1 >= line.size() || line[i + 1] != '[' || i == 0
      || !isIdentChar(line[i - 1]))
    {
      continue;
    }
    auto nameStart = i;
    while (nameStart > 0 && isIdentChar(line[nameStart - 1]))
    {
      --nameStart;
    }
    if (nameStart > 0 && line[nameStart - 1] == '$')
    {
      --nameStart; // System variables such as `$DIR_X$`.
    }
    // Continue at `[`, so substrings nested in the arguments are checked too.
    checkSubstring(line, nameStart, i + 1, issues);
  }
  return issues;
}

bool isSubstringCode(std::string_view code)
{
  return code == kSubstringStartIndexCode || code == kSubstringEndIndexCode
    || code == kSubstringEmptyRangeCode || code == kSubstringAssignmentCode;
}

void familySubstringIndex(ValidateContext& ctx)
{
  const auto& dbcreate = dbcreateMapFor(ctx);
  const auto& lines = ctx.prep->lines;
  for (int index = 0; index < static_cast<int>(lines.size()); ++index)
  {
    const auto raw = lines[static_cast<std::size_t>(index)];
    if (raw.find("$[") == std::string_view::npos || dbcreate.continuationLines.contains(index))
    {
      continue;
    }
    const auto trimmed = trim(raw);
    // Directives, `# include` lines and `#NAME#` section markers.
    if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == '!')
    {
      continue;
    }
    for (const auto& issue : findSubstringIssuesInLine(raw))
    {
      if (ctx.atCap())
      {
        return;
      }
      lsp::Diagnostic diagnostic;
      diagnostic.range.start.line = static_cast<std::uint32_t>(index);
      diagnostic.range.start.character = static_cast<std::uint32_t>(utf16Offset(raw, issue.startByte));
      diagnostic.range.end.line = static_cast<std::uint32_t>(index);
      diagnostic.range.end.character = static_cast<std::uint32_t>(utf16Offset(raw, issue.endByte));
      diagnostic.severity = severityFor(issue.kind);
      diagnostic.source = std::string(kSubstringSource);
      diagnostic.code = std::string(codeFor(issue.kind));
      diagnostic.message = issue.message;
      ctx.append(std::move(diagnostic));
    }
  }
}

} // namespace vertexadt
