// Indentation formatter: depth rules, if headers, backslash groups, refusals,
// range edits (docs/adr/0013-native-indentation-formatter.md).
// Build: cmake --build build --config Release --target indent-formatter-test
// Run:   build\Release\indent-formatter-test.exe
#include "indent_formatter.h"

#include <initializer_list>
#include <iostream>
#include <string>

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
  check(got == want, got == want ? what : what + "\n         got:\n" + got + "\n         want:\n" + want);
}

std::string join(std::initializer_list<const char*> lines, const char* eol = "\n")
{
  std::string out;
  bool first = true;
  for (const auto* line : lines)
  {
    out += (first ? "" : eol);
    out += line;
    first = false;
  }
  return out;
}

void expectFormat(const std::string& input, const std::string& want, const std::string& what, int size = 2)
{
  const auto got = formatIndentation(input, size);
  checkEq(got ? *got : std::string("<refused>"), want, what);
  if (got)
  {
    const auto again = formatIndentation(*got, size);
    check(again && *again == *got, what + " (idempotent)");
  }
}

void expectRefusal(const std::string& input, const std::string& reason, const std::string& what)
{
  const auto plan = planIndentation(input, 2);
  checkEq(plan.refusal ? plan.refusal->reason : std::string("<formatted>"), reason, what);
  const auto edits = indentationEdits(input, 2);
  check(edits.edits.empty() && edits.refusal.has_value(), what + " (no edits)");
}

} // namespace

int main()
{
  std::cout << "nesting\n";
  expectFormat(
    join({"if a then", "x = 1", "\tloop 3", "      y = 2", "endloop", "else", "x = 2", "  endif"}),
    join({"if a then", "  x = 1", "  loop 3", "    y = 2", "  endloop", "else", "  x = 2", "endif"}),
    "if/else/endif with nested loop, tabs replaced");
  expectFormat(
    join({"if a then", "x = 1", "endif"}),
    join({"if a then", "    x = 1", "endif"}),
    "indent size follows tabSize",
    4);
  expectFormat(
    join({"if a then", "if !Obj then exit", "x = 1", "endif"}),
    join({"if a then", "  if !Obj then exit", "  x = 1", "endif"}),
    "inline if-then opens no block");
  expectFormat(
    join({"if (StudW = 0)then StudW = 90", "if a$ = \"then x\" then", "x = 1", "endif"}),
    join({"if (StudW = 0)then StudW = 90", "if a$ = \"then x\" then", "  x = 1", "endif"}),
    "`)then` is inline; a quoted then is not the keyword");
  expectFormat(
    join({"if a then;", "if b then; !note", "x = 1", "endif", "endif"}),
    join({"if a then;", "  if b then; !note", "    x = 1", "  endif", "endif"}),
    "`then;` and `then; !comment` still open a block");
  expectFormat(
    join({"loop", "if$ = 1", "endloopx = 1", "endloop"}),
    join({"loop", "  if$ = 1", "  endloopx = 1", "endloop"}),
    "keywords are whole words");
  expectFormat(
    join({"If a Then", "x = 1"}),
    join({"If a Then", "x = 1"}),
    "keywords are case-sensitive");
  expectFormat(
    join({"if a then ;! open", "x = 1 ;! endif", "endif ;! close"}),
    join({"if a then ;! open", "  x = 1 ;! endif", "endif ;! close"}),
    "`;!` comments are ignored for keywords");

  std::cout << "comments, blanks, directives\n";
  expectFormat(
    join({"#BUILD#", "  # include $DIR/a.prg", "if a then", "!note", ";! other", "   ", "x = 1", "endif", "#BUILD#"}),
    join({"#BUILD#", "# include $DIR/a.prg", "if a then", "  !note", "  ;! other", "", "  x = 1", "endif", "#BUILD#"}),
    "comments take the depth, blanks emptied, directives at column 0");
  expectFormat(
    join({"if a then", "\tx = \"\tkeep\"   ", "endif"}),
    join({"if a then", "  x = \"\tkeep\"   ", "endif"}),
    "only leading whitespace changes");
  expectFormat(
    join({"if a then", "x = 1", "endif", ""}, "\r\n"),
    join({"if a then", "  x = 1", "endif", ""}, "\r\n"),
    "CRLF kept");

  std::cout << "if headers and backslash groups\n";
  expectFormat(
    join({"loop", "if a or \\", "b or \\", "c then", "x = 1", "endif", "endloop"}),
    join({"loop", "  if a or \\", "      b or \\", "      c then", "    x = 1", "  endif", "endloop"}),
    "if header continuation lines at depth + 2 levels");
  expectFormat(
    join({"if a or \\", "b then exit", "x = 1"}),
    join({"if a or \\", "    b then exit", "x = 1"}),
    "multi-line inline if opens no block");
  expectFormat(
    join({"if a then", "      dbcreate(1, \\", "               2, \\", "                 3)", "y = 1", "endif"}),
    join({"if a then", "  dbcreate(1, \\", "           2, \\", "             3)", "  y = 1", "endif"}),
    "backslash group keeps its offsets");
  expectFormat(
    join({"    x = 1 \\", "\t\t  2", "y"}),
    join({"x = 1 \\", "  2", "y"}),
    "tab counts as tabSize columns in a group offset");

  std::cout << "refusals\n";
  expectRefusal(join({"x = 1", "endif"}), "unmatched `endif` at line 2", "stray endif");
  expectRefusal(join({"else"}), "unmatched `else` at line 1", "stray else");
  expectRefusal(join({"endloop"}), "unmatched `endloop` at line 1", "stray endloop");
  expectRefusal(join({"if a then", "loop", "endif", "endloop"}),
    "`endif` at line 3 is inside `loop` at line 2", "crossed nesting");
  expectRefusal(join({"loop", "if a then", "endloop"}),
    "`endloop` at line 3 is inside `if` at line 2", "crossed nesting, endloop");
  expectRefusal(join({"if a then", "x = 1"}), "`if` at line 1 is never closed", "unclosed at end of file");
  expectRefusal(join({"#A#", "loop", "#A#", "#B#", "endloop", "#B#"}),
    "`loop` at line 2 is not closed before section `#A#` at line 3", "block across a section header");
  expectRefusal(join({"if Bearing", "x = 1", "endif"}), "`if` at line 1 has no `then`", "if without then");
  expectRefusal(join({"if !Ret exit"}), "`if` at line 1 has no `then`", "if without then, inline-looking");
  expectRefusal(join({"if a or \\", ""}), "the condition of `if` at line 1 is not terminated",
    "header runs into a blank line");
  expectRefusal(join({"if a or \\"}), "the condition of `if` at line 1 is not terminated",
    "header runs off the end");
  expectRefusal(join({"if a or \\", "! note", "b then", "endif"}),
    "the condition of `if` at line 1 is not terminated", "header runs into a `!` comment");
  expectFormat(
    join({"if a or \\  ;! why", "b then", "x = 1", "endif"}),
    join({"if a or \\  ;! why", "    b then", "  x = 1", "endif"}),
    "`;!` comment and blanks after `\\`");
  checkEq(indentRefusalMessage(IndentRefusal{.line = 1, .reason = "unmatched `endif` at line 2"}),
    "Vertex ADT: not formatted: unmatched `endif` at line 2.", "refusal message");

  std::cout << "edits\n";
  {
    const auto text = join({"if a then", "x = 1", "\t\ty = 2", "  endif", "  \t"});
    const auto all = indentationEdits(text, 2);
    check(all.edits.size() == 4, "one edit per changed line (" + std::to_string(all.edits.size()) + ")");
    const auto& tab = all.edits[1];
    check(tab.range.start.line == 2 && tab.range.end.character == 2 && tab.newText == "  ",
      "tab edit replaces only the leading whitespace");
    const auto& blank = all.edits[3];
    check(blank.range.start.line == 4 && blank.range.end.character == 3 && blank.newText.empty(),
      "blank line edit empties the line");
    const auto ranged = indentationEdits(text, 2, 2, 2);
    check(ranged.edits.size() == 1 && ranged.edits[0].range.start.line == 2,
      "range formatting edits only the selected lines, depth from the whole text");
    const auto broken = indentationEdits(text + "\nendif", 2, 1, 1);
    check(broken.edits.empty() && broken.refusal, "range formatting refuses on any imbalance");
  }

  std::cout << (failures == 0 ? "all passed\n" : std::to_string(failures) + " failed\n");
  return failures == 0 ? 0 : 1;
}
