#include "line_continuation.h"

#include "text_util.h"

#include <algorithm>

namespace vertexadt {
namespace {

bool isBlankByte(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

// The `;!`-stripped line without trailing whitespace.
std::string_view codeEnd(std::string_view line)
{
  for (std::size_t i = 0; i + 1 < line.size(); ++i)
  {
    if (line[i] == ';' && line[i + 1] == '!' && !isInsideQuotes(line, i))
    {
      line = line.substr(0, i);
      break;
    }
  }
  while (!line.empty() && isBlankByte(line.back()))
  {
    line.remove_suffix(1);
  }
  return line;
}

} // namespace

bool isContinuationCodeLine(std::string_view line)
{
  const auto trimmed = trim(line);
  if (trimmed.empty() || trimmed.front() == '!' || trimmed.front() == '#')
  {
    return false;
  }
  return !trim(codeEnd(line)).empty();
}

bool endsWithContinuation(std::string_view line)
{
  if (!isContinuationCodeLine(line))
  {
    return false;
  }
  const auto code = codeEnd(line);
  return !code.empty() && code.back() == '\\';
}

auto JoinedLines::locate(std::size_t joinedOffset) const -> Location
{
  if (segments.empty())
  {
    return Location{.line = firstLine, .byte = 0};
  }
  const JoinedSegment* hit = &segments.front();
  for (const auto& segment : segments)
  {
    if (segment.joinedStart > joinedOffset)
    {
      break;
    }
    hit = &segment;
  }
  auto within = joinedOffset - std::min(joinedOffset, hit->joinedStart);
  if (within > hit->length)
  {
    within = hit->length;
  }
  return Location{.line = hit->line, .byte = hit->lineByte + within};
}

auto joinContinuedLines(const std::vector<std::string_view>& lines, std::size_t first) -> JoinedLines
{
  JoinedLines out;
  out.firstLine = static_cast<int>(first);
  out.lastLine = static_cast<int>(first);
  if (first >= lines.size())
  {
    return out;
  }

  std::size_t index = first;
  while (true)
  {
    const auto line = lines[index];
    auto piece = codeEnd(line);
    std::size_t lead = 0;
    if (index != first)
    {
      while (lead < piece.size() && isBlankByte(piece[lead]))
      {
        ++lead;
      }
    }
    const bool continues = endsWithContinuation(line);
    auto body = piece.substr(lead);
    if (continues)
    {
      body.remove_suffix(1); // the `\`
    }
    out.segments.push_back(JoinedSegment{
      .line = static_cast<int>(index),
      .lineByte = lead,
      .joinedStart = out.text.size(),
      .length = body.size(),
    });
    out.text += body;
    out.lastLine = static_cast<int>(index);
    if (!continues)
    {
      break;
    }
    if (index + 1 >= lines.size() || !isContinuationCodeLine(lines[index + 1]))
    {
      out.danglingBackslash = piece.size() - 1;
      break;
    }
    out.text += ' ';
    ++index;
  }
  return out;
}

} // namespace vertexadt
