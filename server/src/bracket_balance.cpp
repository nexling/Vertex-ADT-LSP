#include "bracket_balance.h"

#include "utf16_text.h"
#include "validate.h"

#include <cctype>
#include <cstdint>
#include <optional>

namespace vertexadt {

bool isNonCodeLine(std::string_view raw)
{
  const auto trimmed = trim(raw);
  if (trimmed.empty() || trimmed.front() == '!')
  {
    return true;
  }
  if (trimmed.starts_with("##") || trimmed.ends_with("##"))
  {
    return true;
  }
  return trimmed.size() > 1 && trimmed.front() == '#' && trimmed.back() == '#';
}

auto codeOf(std::string_view raw) -> CodeSpan
{
  char quote = 0;
  std::size_t end = raw.size();
  for (std::size_t i = 0; i < raw.size(); ++i)
  {
    const auto ch = raw[i];
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
    }
    else if (ch == ';' && i + 1 < raw.size() && raw[i + 1] == '!')
    {
      end = i;
      break;
    }
  }
  auto code = raw.substr(0, end);
  auto trimEnd = [&code] {
    while (!code.empty() && std::isspace(static_cast<unsigned char>(code.back())) != 0)
    {
      code.remove_suffix(1);
    }
  };
  trimEnd();
  CodeSpan out;
  if (!code.empty() && code.back() == '\\')
  {
    out.continued = true;
    code.remove_suffix(1);
    trimEnd();
  }
  out.code = code;
  return out;
}

namespace {

bool isWordChar(char ch)
{
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

// `name` directly before `at` (word chars, optionally ending in `$`), or empty
// when it is not an identifier (nothing, a space, or a leading digit).
auto nameBefore(std::string_view code, std::size_t at) -> std::string
{
  auto end = at;
  auto start = end;
  if (start > 0 && code[start - 1] == '$')
  {
    --start;
  }
  const auto wordEnd = start;
  while (start > 0 && isWordChar(code[start - 1]))
  {
    --start;
  }
  if (start == wordEnd || std::isdigit(static_cast<unsigned char>(code[start])) != 0)
  {
    return {};
  }
  return std::string(code.substr(start, end - start));
}

struct Opener
{
  int line = 0;
  std::size_t byte = 0;
  char kind = '(';
  std::string name;
};

auto kindNoun(char kind) -> std::string
{
  return kind == '(' || kind == ')' ? "parentheses" : "brackets";
}

// Walks one logical line at a time so a `(` may close on a `\` continuation line.
class Walker
{
public:
  Walker(const std::vector<std::string_view>& lines, std::vector<BracketIssue>& out)
    : lines_(lines), out_(out)
  {
  }

  void feed(int index)
  {
    const auto raw = lines_[static_cast<std::size_t>(index)];
    const auto span = codeOf(raw);
    codeEnds_.push_back({index, span.code.size()});
    if (!failed_)
    {
      scan(index, raw, span.code);
    }
    if (!span.continued)
    {
      flush();
    }
  }

  void flush()
  {
    if (!failed_ && !stack_.empty())
    {
      const auto& open = stack_.back();
      std::size_t endByte = open.byte + 1;
      for (const auto& [line, end] : codeEnds_)
      {
        if (line == open.line)
        {
          endByte = end;
        }
      }
      const auto raw = lines_[static_cast<std::size_t>(open.line)];
      auto message = "Mismatching " + kindNoun(open.kind);
      if (!open.name.empty())
      {
        message += " for '" + open.name + "'";
      }
      out_.push_back(BracketIssue{
        .startLine = open.line,
        .startCharacter = utf16Offset(raw, open.byte),
        .endLine = open.line,
        .endCharacter = utf16Offset(raw, endByte),
        .message = std::move(message),
      });
    }
    stack_.clear();
    codeEnds_.clear();
    failed_ = false;
  }

private:
  void scan(int index, std::string_view raw, std::string_view code)
  {
    char quote = 0;
    for (std::size_t i = 0; i < code.size(); ++i)
    {
      const auto ch = code[i];
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
      }
      else if (ch == '(' || ch == '[')
      {
        stack_.push_back(Opener{.line = index, .byte = i, .kind = ch, .name = nameBefore(code, i)});
      }
      else if (ch == ')' || ch == ']')
      {
        const char want = ch == ')' ? '(' : '[';
        if (stack_.empty())
        {
          fail(BracketIssue{
            .startLine = index,
            .startCharacter = utf16Offset(raw, i),
            .endLine = index,
            .endCharacter = utf16Offset(raw, i + 1),
            .message = "Mismatching " + kindNoun(ch),
          });
          return;
        }
        const auto& open = stack_.back();
        if (open.kind != want)
        {
          const auto openRaw = lines_[static_cast<std::size_t>(open.line)];
          fail(BracketIssue{
            .startLine = open.line,
            .startCharacter = utf16Offset(openRaw, open.byte),
            .endLine = index,
            .endCharacter = utf16Offset(raw, i + 1),
            .message = std::string("Expected '") + (open.kind == '(' ? ')' : ']') + "' but found '" + ch + "'",
          });
          return;
        }
        stack_.pop_back();
      }
    }
  }

  void fail(BracketIssue issue)
  {
    out_.push_back(std::move(issue));
    failed_ = true;
  }

  const std::vector<std::string_view>& lines_;
  std::vector<BracketIssue>& out_;
  std::vector<Opener> stack_;
  std::vector<std::pair<int, std::size_t>> codeEnds_;
  bool failed_ = false;
};

} // namespace

auto findBracketIssues(
  const std::vector<std::string_view>& lines,
  const std::vector<NamedSection>& sections) -> std::vector<BracketIssue>
{
  std::vector<BracketIssue> issues;
  Walker walker(lines, issues);
  for (int index = 0; index < static_cast<int>(lines.size()); ++index)
  {
    const auto raw = lines[static_cast<std::size_t>(index)];
    if (isNonCodeLine(raw) || lineInVerificationIgnoredSection(index, sections))
    {
      walker.flush();
      continue;
    }
    walker.feed(index);
  }
  walker.flush();
  return issues;
}

const std::vector<BracketIssue>& bracketIssuesFor(ValidateContext& ctx)
{
  if (!ctx.bracketIssues)
  {
    ctx.bracketIssues = findBracketIssues(ctx.prep->lines, ctx.prep->sections);
  }
  return *ctx.bracketIssues;
}

void familyBracketBalance(ValidateContext& ctx)
{
  for (const auto& issue : bracketIssuesFor(ctx))
  {
    if (ctx.atCap())
    {
      return;
    }
    lsp::Diagnostic diagnostic;
    diagnostic.range.start.line = static_cast<std::uint32_t>(issue.startLine);
    diagnostic.range.start.character = static_cast<std::uint32_t>(issue.startCharacter);
    diagnostic.range.end.line = static_cast<std::uint32_t>(issue.endLine);
    diagnostic.range.end.character = static_cast<std::uint32_t>(issue.endCharacter);
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.source = std::string(kBracketBalanceSource);
    diagnostic.code = std::string(kBracketBalanceCode);
    diagnostic.message = issue.message;
    ctx.append(std::move(diagnostic));
  }
}

} // namespace vertexadt
