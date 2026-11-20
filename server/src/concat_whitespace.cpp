#include "concat_whitespace.h"

#include "utf16_text.h"
#include "validate.h"

#include <cstdint>
#include <string>

namespace vertexadt {
namespace {

bool isBlank(char ch)
{
  return ch == ' ' || ch == '\t';
}

// Whole lines the check never reads: blank, `!` comments, `##` directives and
// `#NAME#` section markers.
bool skipLine(std::string_view raw)
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

// Closed `@nodebugstart` … `@nodebugend` ranges (inclusive), the same pairing
// as the validate post-filter, so fix-all never edits a line it hides.
auto nodebugLines(const std::vector<std::string_view>& lines) -> std::vector<bool>
{
  std::vector<bool> hidden(lines.size(), false);
  int activeStart = -1;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto line = lines[static_cast<std::size_t>(i)];
    const auto hasStart = line.find("@nodebugstart") != std::string_view::npos;
    const auto hasEnd = line.find("@nodebugend") != std::string_view::npos;
    if (hasStart && activeStart == -1)
    {
      activeStart = i;
    }
    else if (hasEnd)
    {
      if (activeStart == -1)
      {
        continue;
      }
      for (int j = activeStart; j <= i; ++j)
      {
        hidden[static_cast<std::size_t>(j)] = true;
      }
      activeStart = -1;
    }
    else if (hasStart)
    {
      activeStart = i;
    }
  }
  return hidden;
}

} // namespace

auto findConcatWhitespaceInLine(std::string_view line) -> std::vector<ConcatWhitespaceSpan>
{
  std::vector<ConcatWhitespaceSpan> spans;
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
    if (ch != '&')
    {
      continue;
    }
    if (i + 1 < line.size() && line[i + 1] == '&')
    {
      ++i; // `&&` is the logical AND, not a concatenation.
      continue;
    }

    auto start = i;
    while (start > 0 && isBlank(line[start - 1]))
    {
      --start;
    }
    if (start == 0)
    {
      start = i; // Indentation (continuation lines) does not count.
    }
    auto end = i + 1;
    while (end < line.size() && isBlank(line[end]))
    {
      ++end;
    }
    if (start != i || end != i + 1)
    {
      spans.push_back(ConcatWhitespaceSpan{.startByte = start, .endByte = end});
    }
    i = end - 1;
  }
  return spans;
}

auto findConcatWhitespaceIssues(
  const std::vector<std::string_view>& lines,
  const std::vector<NamedSection>& sections) -> std::vector<ConcatWhitespaceIssue>
{
  std::vector<ConcatWhitespaceIssue> issues;
  const auto hidden = nodebugLines(lines);
  for (int index = 0; index < static_cast<int>(lines.size()); ++index)
  {
    const auto raw = lines[static_cast<std::size_t>(index)];
    if (raw.find('&') == std::string_view::npos || skipLine(raw)
      || hidden[static_cast<std::size_t>(index)]
      || lineInVerificationIgnoredSection(index, sections))
    {
      continue;
    }
    for (const auto& span : findConcatWhitespaceInLine(raw))
    {
      issues.push_back(ConcatWhitespaceIssue{
        .line = index,
        .startCharacter = utf16Offset(raw, span.startByte),
        .endCharacter = utf16Offset(raw, span.endByte),
      });
    }
  }
  return issues;
}

void familyConcatWhitespace(ValidateContext& ctx)
{
  for (const auto& issue : findConcatWhitespaceIssues(ctx.prep->lines, ctx.prep->sections))
  {
    if (ctx.atCap())
    {
      return;
    }
    lsp::Diagnostic diagnostic;
    diagnostic.range.start.line = static_cast<std::uint32_t>(issue.line);
    diagnostic.range.start.character = static_cast<std::uint32_t>(issue.startCharacter);
    diagnostic.range.end.line = static_cast<std::uint32_t>(issue.line);
    diagnostic.range.end.character = static_cast<std::uint32_t>(issue.endCharacter);
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.source = std::string("ADT-Syntax");
    diagnostic.code = std::string(kConcatWhitespaceCode);
    diagnostic.message = std::string(kConcatWhitespaceMessage);
    ctx.append(std::move(diagnostic));
  }
}

} // namespace vertexadt
