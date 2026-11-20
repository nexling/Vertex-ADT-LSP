// findstr* value expressions (docs/adr/0011-findstr-value-expressions.md):
// arithmetic after the call's `)` in an assignment, trailing junk and bare
// statements still flagged, the findstrr catalog entry, and `value` return types.
// Build: cmake --build build --config Release --target findstr-value-expression-test
// Run:   build\Release\findstr-value-expression-test.exe
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

const std::string kPrelude = "a$ = \"AB-CD\"\nb$ = \"-\"\n";

// Messages on the lines of `body` (after the prelude) that mention findstr or
// abs, joined with " | ".
std::string messages(const std::string& body, bool strict = false)
{
  const std::string text = kPrelude + body + "\n";
  constexpr int kFirstBodyLine = 2;
  ValidateContext ctx;
  ctx.settings.strictModeEnabled = strict;
  const Snapshot doc{
    .uri = lsp::Uri::parse("file:///c%3A/w/findstr.prg"),
    .version = 1,
    .languageId = "vertexadt",
    .text = text};
  std::string out;
  for (const auto& diagnostic : fullDocumentValidate(ctx, doc))
  {
    if (static_cast<int>(diagnostic.range.start.line) < kFirstBodyLine)
    {
      continue;
    }
    const auto* message = std::get_if<std::string>(&diagnostic.message);
    if (!message
        || (message->find("findstr") == std::string::npos && message->find("abs") == std::string::npos))
    {
      continue;
    }
    out += (out.empty() ? "" : " | ") + *message;
  }
  return out;
}

} // namespace

int main()
{
  std::cout << "arithmetic after the call in an assignment\n";
  checkEq(messages("end = findstrri(a$,\"-\")-1"), "", "the reported line: -1");
  checkEq(messages("end = findstrri(a$,\"-\")+1"), "", "+1");
  checkEq(messages("e = findstrri(a$,b$) - 1"), "", "spaces around the operator");
  checkEq(messages("e = findstrri(a$,b$)-len(b$)"), "", "command operand");
  checkEq(messages("e = findstrri(a$,b$)*2+1"), "", "longer expression");
  checkEq(messages("e = findstrri(a$,b$)/(1+1)"), "", "parenthesized operand");
  checkEq(messages("e = findstr(a$,b$)-1"), "", "findstr");
  checkEq(messages("e = findstri(a$,b$)+1"), "", "findstri");
  checkEq(messages("e = findstrr(a$,b$)-1"), "", "findstrr");
  checkEq(messages("e = findstrri(a$,b$)"), "", "plain call");
  checkEq(messages("e = 1+findstrri(a$,b$)"), "", "call as right operand");
  checkEq(messages("e = 1+findstrri(a$,b$)-1"), "", "call in the middle of the value");
  checkEq(messages("e = findstr(a$,b$)-findstri(a$,b$)"), "", "two calls");
  checkEq(messages("e = findstr(a$,b$)-1 ;! note"), "", "trailing ;! comment");
  checkEq(messages("e = findstr(a$,b$) ;! note"), "", "plain call with ;! comment");

  checkEq(messages("e = (findstr(a$,b$)-1)"), "", "arithmetic inside parentheses");
  checkEq(messages("e = 2*(findstrri(a$,b$)+1)"), "", "arithmetic inside a larger parenthesized value");

  std::cout << "logic after the call in an assignment (vxbd320 corpus)\n";
  checkEq(
    messages("ok = findstri(a$,\"walls@\") or findstri(a$,\"deck@\") or findstri(a$,\"ceil@\")"),
    "", "or chain (DWGCOPY.prg)");
  checkEq(
    messages("ok2 = !findstri(a$,\"PLAN\") or findstri(a$,\"VILPLAN\")"), "", "! and or (DWGCOPY.prg)");
  checkEq(
    messages("k = findstri(a$,\"S\") and (findstri(b$,\"203\") or findstri(b$,\"208\"))"),
    "", "and with a parenthesized or (FLOOR_SHEET_P1.prg)");
  checkEq(messages("oktest = (findstri(a$,\"-M-\"))"), "", "call wrapped in parentheses (NO_STUDS.prg)");
  checkEq(
    messages("inOpening = 1\noktest = !(findstri(a$,\"-M-\") and inOpening)"),
    "", "!( call and name ) (NO_STUDS.prg)");
  checkEq(messages("Val = findstr(a$,b$);"), "", "trailing ; (fdet_filtercode.prg)");
  checkEq(messages("ok = findstrri(a$,b$) or findstrr(a$,b$)"), "", "findstrri or findstrr");
  checkEq(messages("ok = findstr(a$,b$) > 0"), "", "comparison");
  checkEq(messages("ok = findstr(a$,b$)<>0"), "", "<> comparison");
  checkEq(messages("ok = FINDSTR(a$,b$) AND 1"), "", "upper-case AND");

  std::cout << "logic tails still flagged\n";
  checkEq(
    messages("ok = findstr(a$,b$) and"),
    "findstr accepts at most 0 parameters after parentheses",
    "dangling and");
  checkEq(
    messages("ok = findstr(a$,b$) andb$"),
    "findstr accepts at most 0 parameters after parentheses",
    "and must be a whole word");
  checkEq(
    messages("ok = findstr(a$,b$))"), "",
    "stray ) is left to bracket balance (Mismatching parentheses), not command syntax");
  checkEq(
    messages("ok = findstr(a$,b$) 1"),
    "findstr accepts at most 0 parameters after parentheses",
    "number after the call");
  checkEq(
    messages("ok = findstr(a$,b$), b$"),
    "findstr accepts at most 0 parameters after parentheses",
    "comma after the call");

  std::cout << "if conditions are not command statements\n";
  checkEq(messages("if findstr(a$,b$) then\nendif"), "", "if findstr then");
  checkEq(messages("if findstr(a$,b$)>0 then\nendif"), "", "if findstr > 0 then");
  checkEq(messages("if findstri(a$,b$)-1 > 0 then\nendif"), "", "if findstri - 1 > 0 then");

  std::cout << "still flagged\n";
  checkEq(
    messages("findstrri(a$,b$)-1"),
    "findstrri accepts at most 0 parameters after parentheses",
    "bare statement");
  checkEq(
    messages("e = findstrri(a$,b$) b$"),
    "findstrri accepts at most 0 parameters after parentheses",
    "trailing junk");
  checkEq(
    messages("e = findstrri(a$,b$)-"),
    "findstrri accepts at most 0 parameters after parentheses",
    "dangling operator");
  checkEq(
    messages("e = findstr(a$,b$) b$"),
    "findstr accepts at most 0 parameters after parentheses",
    "findstr trailing junk");
  checkEq(
    messages("e = findstri(a$,b$) b$"),
    "findstri accepts at most 0 parameters after parentheses",
    "findstri trailing junk");
  checkEq(
    messages("findstr(a$,b$)-1"),
    "findstr accepts at most 0 parameters after parentheses",
    "findstr bare statement");
  checkEq(
    messages("e = findstrr(a$,b$) b$"),
    "findstrr accepts at most 0 parameters after parentheses",
    "findstrr trailing junk");

  std::cout << "findstrr catalog entry\n";
  checkEq(
    messages("e = findstrr(a$)"),
    "findstrr requires at least 2 parameters inside parentheses",
    "findstrr arity");
  checkEq(
    messages("e = findstrr(a$,b$,a$)"),
    "findstrr accepts at most 2 parameters inside parentheses",
    "findstrr max arity");
  checkEq(
    messages("e = findstrr(1,b$)"),
    messages("e = findstrri(1,b$)").empty()
      ? std::string()
      : "First parameter inside parentheses must be a valid string in findstrr",
    "findstrr string slot matches findstrri");

  std::cout << "strict mode: findstr* return value\n";
  checkEq(messages("p = findstr(a$,b$)\nabs(p)", true), "", "findstr result is a value");
  checkEq(messages("p = findstri(a$,b$)\nabs(p)", true), "", "findstri result is a value");
  checkEq(messages("p = findstrr(a$,b$)\nabs(p)", true), "", "findstrr result is a value");
  checkEq(messages("e = findstrri(a$,b$)-1\nabs(e)", true), "", "arithmetic result is a value");
  checkEq(messages("e = findstrri(a$,b$)-1", true), "", "strict accepts the reported line");

  if (failures > 0)
  {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "all passed\n";
  return 0;
}
