// Trailing `\` line continuation: code-line rule, joining, position map
// (docs/adr/0014-native-if-header-continuation.md).
// Build: cmake --build build --config Release --target line-continuation-test
// Run:   build\Release\line-continuation-test.exe
#include "line_continuation.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace vertexadt;

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
  std::cout << (ok ? "  ok   " : "  FAIL ") << what << '\n';
  if (!ok)
  {
    ++failures;
  }
}

void checkEq(const std::string& got, const std::string& want, const std::string& what)
{
  check(got == want, got == want ? what : what + " (got \"" + got + "\", want \"" + want + "\")");
}

} // namespace

int main()
{
  std::cout << "code lines\n";
  check(isContinuationCodeLine("  x = 1"), "statement is a code line");
  check(!isContinuationCodeLine(""), "empty line is not");
  check(!isContinuationCodeLine(" \t\r"), "blank line is not");
  check(!isContinuationCodeLine("  ! note \\"), "`!` comment is not");
  check(!isContinuationCodeLine("# include $DIR/a.prg"), "`#` directive is not");
  check(!isContinuationCodeLine("  ;! only a comment"), "nothing left after `;!` is not");

  std::cout << "continuation\n";
  check(endsWithContinuation("if a or \\"), "trailing `\\`");
  check(endsWithContinuation("if a or \\   \r"), "trailing whitespace and CR after `\\`");
  check(endsWithContinuation("if a or \\ ;! note"), "`;!` comment after `\\`");
  check(!endsWithContinuation("if a or b"), "no `\\`");
  check(!endsWithContinuation("x$ = \"a\\\" ;! \\"), "`\\` only inside the `;!` comment");
  check(!endsWithContinuation("! note \\"), "`!` comment ending in `\\`");

  std::cout << "join\n";
  {
    const std::vector<std::string_view> lines{"  if a or \\\r", "     b or \\  ", "\tc then x = 1", "y = 2"};
    const auto joined = joinContinuedLines(lines, 0);
    checkEq(joined.text, "  if a or  b or  c then x = 1", "pieces joined with one space per `\\`");
    check(joined.firstLine == 0 && joined.lastLine == 2, "spans the three lines");
    check(!joined.danglingBackslash, "terminated");
    check(joined.segments.size() == 3, "one segment per line");
    const auto b = joined.locate(joined.text.find('b'));
    check(b.line == 1 && b.byte == 5, "`b` maps to line 1 byte 5");
    const auto x = joined.locate(joined.text.find('x'));
    check(x.line == 2 && x.byte == 8, "`x` maps to line 2 byte 8");
    const auto gap = joined.locate(joined.text.find("  b") + 1);
    check(gap.line == 0, "the space for a `\\` maps to the earlier line");
  }
  {
    const std::vector<std::string_view> lines{"x = 1"};
    const auto joined = joinContinuedLines(lines, 0);
    checkEq(joined.text, "x = 1", "a line that does not continue");
    check(joined.lastLine == 0 && !joined.danglingBackslash, "single segment");
  }
  {
    const std::vector<std::string_view> lines{"if a ;! why \\", "b then"};
    const auto joined = joinContinuedLines(lines, 0);
    checkEq(joined.text, "if a", "`\\` inside a comment does not continue");
  }

  std::cout << "dangling\n";
  for (const auto* next : {"", "   ", "! note", "#BUILD#", "  ;! note"})
  {
    const std::vector<std::string_view> lines{"if a or \\ ", next, "x = 1"};
    const auto joined = joinContinuedLines(lines, 0);
    check(joined.danglingBackslash == std::optional<std::size_t>{8} && joined.lastLine == 0,
      std::string("stops before \"") + next + "\"");
    checkEq(joined.text, "if a or ", "dangling `\\` is not in the text");
  }
  {
    const std::vector<std::string_view> lines{"if a or \\", "  b or \\"};
    const auto joined = joinContinuedLines(lines, 0);
    check(joined.danglingBackslash == std::optional<std::size_t>{7} && joined.lastLine == 1,
      "runs off the end on the second line");
  }

  if (failures == 0)
  {
    std::cout << "all passed\n";
    return 0;
  }
  std::cout << failures << " failed\n";
  return 1;
}
