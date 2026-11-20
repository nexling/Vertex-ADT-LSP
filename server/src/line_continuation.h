#pragma once

// Trailing `\` line continuation (docs/adr/0014-native-if-header-continuation.md).
// Shared by the if-header validators and the indentation formatter.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// A line that can carry or receive a continuation: not blank, not a `!`
// comment, not a `#` directive, and something left after `;!` stripping.
[[nodiscard]] bool isContinuationCodeLine(std::string_view line);

// A code line whose last character, after `;!` stripping and trailing
// whitespace (CR included), is `\`.
[[nodiscard]] bool endsWithContinuation(std::string_view line);

// One physical line's share of a joined logical line.
struct JoinedSegment
{
  int line = 0;
  std::size_t lineByte = 0;    // first byte of the piece in the physical line
  std::size_t joinedStart = 0; // first byte of the piece in the joined text
  std::size_t length = 0;
};

struct JoinedLines
{
  // `;!`-stripped pieces. Each `\`, its line ending and the next line's
  // leading whitespace become one space. The first line keeps its indentation.
  std::string text;
  int firstLine = 0;
  int lastLine = 0;
  std::vector<JoinedSegment> segments;
  // The chain's last `\` (byte on `lastLine`) when the next line is blank, a
  // comment, a directive, or past the end. The `\` is not part of `text`.
  std::optional<std::size_t> danglingBackslash;

  struct Location
  {
    int line = 0;
    std::size_t byte = 0;
  };
  // Physical line and byte of a joined-text offset. The space that stands for
  // a `\` maps to the end of the earlier piece.
  [[nodiscard]] auto locate(std::size_t joinedOffset) const -> Location;
};

// The logical line starting at `first`: `first` plus every line after one
// that ends in `\`, up to the first line that is not a code line. A line that
// does not continue yields a single segment.
[[nodiscard]] auto joinContinuedLines(const std::vector<std::string_view>& lines, std::size_t first)
  -> JoinedLines;

} // namespace vertexadt
