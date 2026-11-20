// Unbalanced parentheses and square brackets: document walk and the full
// validate pipeline (one diagnostic per line, old whole-line check removed).
// Build: cmake --build build --config Release --target bracket-balance-test
// Run:   build\Release\bracket-balance-test.exe
#include "bracket_balance.h"
#include "text_util.h"
#include "validate.h"

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
  check(got == want, what + "  [got: " + got + "]");
}

// Issues rendered as "line:start-end message" (endLine shown only when it differs).
std::string issues(std::string_view text)
{
  const auto lines = splitLines(text);
  const auto sections = parseFileSections(lines);
  std::string out;
  for (const auto& issue : findBracketIssues(lines, sections))
  {
    out += (out.empty() ? "" : " | ") + std::to_string(issue.startLine) + ":"
      + std::to_string(issue.startCharacter) + "-"
      + (issue.endLine != issue.startLine ? std::to_string(issue.endLine) + ":" : "")
      + std::to_string(issue.endCharacter) + " " + issue.message;
  }
  return out;
}

// Every diagnostic from the full pipeline as "line:start-end message".
std::string validateAll(const std::string& text)
{
  const Snapshot doc{
    .uri = lsp::Uri::parse("file:///c%3A/w/b.prg"),
    .version = 1,
    .languageId = "vertexadt",
    .text = text};
  std::string out;
  for (const auto& diagnostic : fullDocumentValidate(ValidateContext{}, doc))
  {
    const auto* message = std::get_if<std::string>(&diagnostic.message);
    out += (out.empty() ? "" : " | ") + std::to_string(diagnostic.range.start.line) + ":"
      + std::to_string(diagnostic.range.start.character) + "-"
      + std::to_string(diagnostic.range.end.character) + " " + (message ? *message : "?");
  }
  return out;
}

} // namespace

int main()
{
  std::cout << "unclosed openers\n";
  checkEq(issues(R"(ret$=call("$DIR_MACROS/addtimerforactpanel.prg AFTERGEN",Panel_Label$)"),
    "0:9-69 Mismatching parentheses for 'call'", "unclosed call( runs to end of code");
  checkEq(issues("x=(a+b"), "0:2-6 Mismatching parentheses", "bare unclosed (");
  checkEq(issues(R"(ret$=call("x",f(a,b)"), "0:15-19 Mismatching parentheses for 'f'", "innermost unclosed wins");
  checkEq(issues("x=layer[area"), "0:7-12 Mismatching brackets for 'layer'", "unclosed [ with name");
  checkEq(issues("x=a$[1"), "0:4-6 Mismatching brackets for 'a$'", "name keeps $");
  checkEq(issues("x=([1"), "0:3-5 Mismatching brackets", "innermost across kinds");
  checkEq(issues("x=ascii (35"), "0:8-11 Mismatching parentheses", "space before ( is bare");
  checkEq(issues("x=(a+b   ;! note (\r"), "0:2-6 Mismatching parentheses", "ends before comment and spaces");

  std::cout << "extra and wrong closers\n";
  checkEq(issues("a=b)+1"), "0:3-4 Mismatching parentheses", "extra )");
  checkEq(issues("a=b]+1"), "0:3-4 Mismatching brackets", "extra ]");
  checkEq(issues("x=f(a[1)]"), "0:5-8 Expected ']' but found ')'", "crossed pair");
  checkEq(issues("x=f[a(1])"), "0:5-8 Expected ')' but found ']'", "crossed pair other way");
  checkEq(issues("a)+(b"), "0:1-2 Mismatching parentheses", "first problem wins");

  std::cout << "no issue\n";
  checkEq(issues("x=f(a[1],(b+c))"), "", "balanced nesting");
  checkEq(issues(R"(x=findstri(Line$,";!"))"), "", "quoted ;! is not a comment");
  checkEq(issues(R"(x$="(" & ')' & "[")"), "", "brackets in strings ignored");
  checkEq(issues("x=f(a) ;! (unclosed"), "", ";! comment ignored");
  checkEq(issues("if (!PceT) then exit"), "", "! is logical NOT, not a comment");
  checkEq(issues(R"(if (!findstri(a$,"B") or !findstri(b$,"C")) then)"), "", "several ! inside parentheses");
  checkEq(issues("if (a != b then"), "0:3-15 Mismatching parentheses", "brackets after ! are still checked");
  checkEq(issues("! x=(a\n##included_from (a.prg\n#SEC(#"), "", "comment, ## and section lines skipped");
  checkEq(issues("#EDIT_FORM#\nx=(a\n#EDIT_FORM#"), "", "EDIT_FORM section skipped");

  std::cout << "if lines and continuation\n";
  checkEq(issues("if (a=1 then\nendif"), "0:3-12 Mismatching parentheses", "whole if line checked");
  checkEq(issues("if a=1 then x=(b"), "0:14-16 Mismatching parentheses", "after then checked");
  checkEq(issues("dbcreate(db, \\\n  \"NAME\", c, 20, 0)"), "", "dbcreate closes on continuation line");
  checkEq(issues("dbcreate(db \\\n  \"NAME\", c, 20, 0"), "0:8-11 Mismatching parentheses for 'dbcreate'",
    "unclosed opener underlined on its own line");
  checkEq(issues("x=f(a \\\n  ]"), "0:3-1:3 Expected ')' but found ']'", "crossed pair across lines");
  checkEq(issues("x=(a \\\n\ny=(b"), "0:2-4 Mismatching parentheses | 2:2-4 Mismatching parentheses",
    "blank line ends the logical line");
  checkEq(issues("x=\xC3\xA5(a"), "0:3-5 Mismatching parentheses", "UTF-16 columns");

  std::cout << "full validate\n";
  checkEq(validateAll("ret$=call(\"$DIR_MACROS/addtimerforactpanel.prg AFTERGEN\",Panel_Label$\n"),
    "0:9-69 Mismatching parentheses for 'call'", "user line: one diagnostic");
  checkEq(validateAll("bad = findstri(Line$,\";!\"   ;! should still error\n"),
    "0:14-25 Mismatching parentheses for 'findstri'", "old whole-line catalog check replaced");
  checkEq(validateAll("x=(1+2\n"), "0:2-6 Mismatching parentheses", "bare paren: no knock-on errors");
  checkEq(validateAll("x=abs(1)\n"), "", "balanced line is clean");
  checkEq(validateAll("a=1\nx=abs(a[1)]\n"), "1:7-10 Expected ']' but found ')'",
    "crossed pair suppresses the catalog parameter error");
  checkEq(validateAll("a=1\nx=abs(a)+1)\n"), "1:10-11 Mismatching parentheses", "extra ) after a command");
  checkEq(validateAll("a=1\nret$=call(\"x\",f(a,1\n"), "1:15-19 Mismatching parentheses for 'f'",
    "innermost unclosed inside call(");
  {
    const Snapshot doc{
      .uri = lsp::Uri::parse("file:///c%3A/w/b.prg"), .version = 1, .languageId = "vertexadt", .text = "x=(1\n"};
    const auto diagnostics = fullDocumentValidate(ValidateContext{}, doc);
    check(diagnostics.size() == 1 && diagnostics[0].severity == lsp::DiagnosticSeverity::Error
        && diagnostics[0].source == std::string(kBracketBalanceSource)
        && diagnostics[0].code && std::get<std::string>(*diagnostics[0].code) == kBracketBalanceCode,
      "error severity, syntax source and code");
  }

  std::cout << (failures == 0 ? "all passed\n" : std::to_string(failures) + " failed\n");
  return failures == 0 ? 0 : 1;
}
