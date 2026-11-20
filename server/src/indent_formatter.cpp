#include "indent_formatter.h"

#include "line_continuation.h"
#include "text_util.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace vertexadt {
namespace {

bool isWordChar(char ch)
{
  const auto c = static_cast<unsigned char>(ch);
  return std::isalnum(c) != 0 || c == '_';
}

// Same whole-word, case-sensitive rule as control-block pairing (ADR 0010):
// `$` also continues the word, so `if$` is a variable, not a keyword.
bool startsWithKeyword(std::string_view trimmed, std::string_view word)
{
  if (!trimmed.starts_with(word))
  {
    return false;
  }
  return trimmed.size() == word.size()
    || (!isWordChar(trimmed[word.size()]) && trimmed[word.size()] != '$');
}

// First whole-word, case-insensitive `then` outside quotes. `)then` counts:
// the shared isInlineIfThen regex needs whitespace before `then` and misses it.
auto findThen(std::string_view text) -> std::optional<std::size_t>
{
  constexpr std::string_view word = "then";
  for (std::size_t i = 0; i + word.size() <= text.size(); ++i)
  {
    bool match = true;
    for (std::size_t j = 0; j < word.size() && match; ++j)
    {
      match = std::tolower(static_cast<unsigned char>(text[i + j])) == word[j];
    }
    if (!match)
    {
      continue;
    }
    const bool leftOk = i == 0 || !isWordChar(text[i - 1]);
    const bool rightOk = i + word.size() == text.size() || !isWordChar(text[i + word.size()]);
    if (leftOk && rightOk && !isInsideQuotes(text, i))
    {
      return i;
    }
  }
  return std::nullopt;
}

// Text after `then` holds a statement, so the if is inline and opens no
// block. `then;` and `then; !note` (empty statement, comment) still open one.
bool hasInlineStatement(std::string_view afterThen)
{
  auto rest = trim(afterThen);
  while (!rest.empty() && rest.front() == ';')
  {
    rest = trim(rest.substr(1));
  }
  return !rest.empty() && rest.front() != '!';
}

// `#NAME#`, the marker parseFileSections opens and closes sections with.
bool isSectionMarker(std::string_view trimmed)
{
  if (trimmed.size() < 3 || trimmed.front() != '#' || trimmed.back() != '#')
  {
    return false;
  }
  const auto name = trimmed.substr(1, trimmed.size() - 2);
  if (std::isdigit(static_cast<unsigned char>(name.front())) != 0)
  {
    return false;
  }
  return std::all_of(name.begin(), name.end(), isWordChar);
}

struct Line
{
  std::string_view body;     // without the trailing '\r'
  bool hasCr = false;
  std::size_t leadBytes = 0; // leading spaces and tabs
  int leadColumns = 0;       // tab = indentSize columns
  std::string_view content;  // body after the leading whitespace
  std::string code;          // content with `;!` comments stripped, trimmed
  bool blank = false;
  bool comment = false;      // `!...` or nothing left after `;!` stripping
  bool directive = false;    // starts with `#`
};

auto readLines(std::string_view text, int indentSize) -> std::vector<Line>
{
  std::vector<Line> out;
  for (const auto raw : splitLines(text))
  {
    Line line;
    line.body = raw;
    if (!line.body.empty() && line.body.back() == '\r')
    {
      line.body.remove_suffix(1);
      line.hasCr = true;
    }
    while (line.leadBytes < line.body.size()
           && (line.body[line.leadBytes] == ' ' || line.body[line.leadBytes] == '\t'))
    {
      line.leadColumns += line.body[line.leadBytes] == '\t' ? indentSize : 1;
      ++line.leadBytes;
    }
    line.content = line.body.substr(line.leadBytes);
    line.blank = trim(line.content).empty();
    if (!line.blank)
    {
      line.directive = line.content.front() == '#';
      line.code = std::string(trim(stripValidationComment(line.content)));
      line.comment = !line.directive && (line.content.front() == '!' || line.code.empty());
    }
    out.push_back(std::move(line));
  }
  return out;
}

bool continues(const Line& line)
{
  return endsWithContinuation(line.body);
}

std::string lineNo(std::size_t index)
{
  return std::to_string(index + 1);
}

struct Frame
{
  bool loop = false;
  std::size_t line = 0;
};

std::string_view frameWord(const Frame& frame)
{
  return frame.loop ? "loop" : "if";
}

IndentPlan refuse(std::size_t line, std::string reason)
{
  return IndentPlan{
    .columns = {},
    .refusal = IndentRefusal{.line = static_cast<int>(line), .reason = std::move(reason)},
  };
}

} // namespace

auto planIndentation(std::string_view text, int indentSize) -> IndentPlan
{
  const int unit = std::max(indentSize, 1);
  const auto rawLines = splitLines(text);
  const auto lines = readLines(text, unit);
  std::vector<int> columns(lines.size(), 0);
  std::vector<Frame> stack;
  const auto depthColumns = [&](std::size_t extraLevels = 0) {
    return static_cast<int>(stack.size() + extraLevels) * unit;
  };

  // Backslash continuation group (not an if header): first line's target and
  // original columns.
  bool inGroup = false;
  int groupTarget = 0;
  int groupOriginal = 0;

  for (std::size_t i = 0; i < lines.size(); ++i)
  {
    const auto& line = lines[i];
    if (line.blank)
    {
      columns[i] = 0;
      inGroup = false;
      continue;
    }
    if (inGroup)
    {
      columns[i] = std::max(0, groupTarget + line.leadColumns - groupOriginal);
      inGroup = continues(line);
      continue;
    }
    if (line.directive)
    {
      columns[i] = 0;
      if (isSectionMarker(trim(line.content)) && !stack.empty())
      {
        const auto& open = stack.back();
        return refuse(open.line,
          "`" + std::string(frameWord(open)) + "` at line " + lineNo(open.line)
            + " is not closed before section `" + std::string(trim(line.content)) + "` at line "
            + lineNo(i));
      }
      continue;
    }
    if (line.comment)
    {
      columns[i] = depthColumns();
      continue;
    }

    const std::string_view code = line.code;
    if (startsWithKeyword(code, "if"))
    {
      // Header: the if line plus every line after one ending in `\`.
      const auto header = joinContinuedLines(rawLines, i);
      if (header.danglingBackslash)
      {
        return refuse(i, "the condition of `if` at line " + lineNo(i) + " is not terminated");
      }
      const auto last = static_cast<std::size_t>(header.lastLine);
      for (auto k = i + 1; k <= last; ++k)
      {
        columns[k] = depthColumns(2);
      }
      const std::string_view joined = header.text;
      const auto then = findThen(joined);
      if (!then)
      {
        return refuse(i, "`if` at line " + lineNo(i) + " has no `then`");
      }
      columns[i] = depthColumns();
      if (!hasInlineStatement(joined.substr(*then + 4)))
      {
        stack.push_back(Frame{.loop = false, .line = i});
      }
      i = last;
      continue;
    }

    if (startsWithKeyword(code, "else") || startsWithKeyword(code, "endif"))
    {
      const auto word = startsWithKeyword(code, "else") ? std::string("else") : std::string("endif");
      if (stack.empty())
      {
        return refuse(i, "unmatched `" + word + "` at line " + lineNo(i));
      }
      if (stack.back().loop)
      {
        return refuse(i,
          "`" + word + "` at line " + lineNo(i) + " is inside `loop` at line "
            + lineNo(stack.back().line));
      }
      if (word == "else")
      {
        columns[i] = depthColumns() - unit;
      }
      else
      {
        stack.pop_back();
        columns[i] = depthColumns();
      }
    }
    else if (startsWithKeyword(code, "loop"))
    {
      columns[i] = depthColumns();
      stack.push_back(Frame{.loop = true, .line = i});
    }
    else if (startsWithKeyword(code, "endloop"))
    {
      if (stack.empty())
      {
        return refuse(i, "unmatched `endloop` at line " + lineNo(i));
      }
      if (!stack.back().loop)
      {
        return refuse(i,
          "`endloop` at line " + lineNo(i) + " is inside `if` at line " + lineNo(stack.back().line));
      }
      stack.pop_back();
      columns[i] = depthColumns();
    }
    else
    {
      columns[i] = depthColumns();
    }

    if (continues(line))
    {
      inGroup = true;
      groupTarget = columns[i];
      groupOriginal = line.leadColumns;
    }
  }

  if (!stack.empty())
  {
    const auto& open = stack.back();
    return refuse(open.line,
      "`" + std::string(frameWord(open)) + "` at line " + lineNo(open.line) + " is never closed");
  }
  return IndentPlan{.columns = std::move(columns), .refusal = std::nullopt};
}

auto indentationEdits(std::string_view text, int indentSize, int firstLine, int lastLine)
  -> IndentFormatResult
{
  auto plan = planIndentation(text, indentSize);
  if (plan.refusal)
  {
    return IndentFormatResult{.edits = {}, .refusal = std::move(plan.refusal)};
  }
  const auto lines = readLines(text, std::max(indentSize, 1));
  IndentFormatResult result;
  const int from = std::max(firstLine, 0);
  const int to = std::min(lastLine, static_cast<int>(lines.size()) - 1);
  for (int i = from; i <= to; ++i)
  {
    const auto& line = lines[static_cast<std::size_t>(i)];
    const auto want = std::string(static_cast<std::size_t>(plan.columns[static_cast<std::size_t>(i)]), ' ');
    // Blank lines lose every whitespace character, not just spaces and tabs.
    const auto have = line.blank ? line.body : line.body.substr(0, line.leadBytes);
    if (have == want)
    {
      continue;
    }
    result.edits.push_back(lsp::TextEdit{
      .range = lsp::Range{
        .start = {static_cast<lsp::Uint>(i), 0},
        .end = {static_cast<lsp::Uint>(i), static_cast<lsp::Uint>(have.size())},
      },
      .newText = want,
    });
  }
  return result;
}

auto indentationEdits(std::string_view text, int indentSize) -> IndentFormatResult
{
  return indentationEdits(text, indentSize, 0, static_cast<int>(splitLines(text).size()) - 1);
}

auto formatIndentation(std::string_view text, int indentSize) -> std::optional<std::string>
{
  const auto plan = planIndentation(text, indentSize);
  if (plan.refusal)
  {
    return std::nullopt;
  }
  const auto lines = readLines(text, std::max(indentSize, 1));
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < lines.size(); ++i)
  {
    if (i > 0)
    {
      out += '\n';
    }
    const auto& line = lines[i];
    if (!line.blank)
    {
      out.append(static_cast<std::size_t>(plan.columns[i]), ' ');
      out += line.content;
    }
    if (line.hasCr)
    {
      out += '\r';
    }
  }
  return out;
}

auto indentRefusalMessage(const IndentRefusal& refusal) -> std::string
{
  return "Vertex ADT: not formatted: " + refusal.reason + ".";
}

} // namespace vertexadt
