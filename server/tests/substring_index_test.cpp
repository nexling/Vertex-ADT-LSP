// Literal substring index checks and substring assignment: line scan,
// fullDocumentValidate, and the start-index QuickFix.
// Build: cmake --build build --config Release --target substring-index-test
// Run:   build\Release\substring-index-test.exe
#include "code_actions.h"
#include "substring_index.h"
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

char kindLetter(SubstringIssueKind kind)
{
  switch (kind)
  {
  case SubstringIssueKind::StartBelowOne:
    return 'S';
  case SubstringIssueKind::NegativeEnd:
    return 'N';
  case SubstringIssueKind::StartAfterEnd:
    return 'O';
  case SubstringIssueKind::Assignment:
    return 'A';
  }
  return '?';
}

// Issues rendered as "<kind><start>-<end>", comma separated.
std::string scan(std::string_view line)
{
  std::string out;
  for (const auto& issue : findSubstringIssuesInLine(line))
  {
    out += (out.empty() ? "" : ",") + std::string(1, kindLetter(issue.kind))
      + std::to_string(issue.startByte) + "-" + std::to_string(issue.endByte);
  }
  return out;
}

std::string firstMessage(std::string_view line)
{
  const auto issues = findSubstringIssuesInLine(line);
  return issues.empty() ? "" : issues.front().message;
}

auto validate(const std::string& text) -> std::vector<lsp::Diagnostic>
{
  const Snapshot doc{
    .uri = lsp::Uri::parse("file:///c%3A/w/s.prg"),
    .version = 1,
    .languageId = "vertexadt",
    .text = text};
  return fullDocumentValidate(ValidateContext{}, doc);
}

// Substring diagnostics as "<line>:<start>-<end><E|W>".
std::string substringDiagnostics(const std::string& text)
{
  std::string out;
  for (const auto& diagnostic : validate(text))
  {
    const auto* code = diagnostic.code ? std::get_if<std::string>(&*diagnostic.code) : nullptr;
    if (!code || !isSubstringCode(*code))
    {
      continue;
    }
    out += (out.empty() ? "" : ",") + std::to_string(diagnostic.range.start.line) + ":"
      + std::to_string(diagnostic.range.start.character) + "-"
      + std::to_string(diagnostic.range.end.character)
      + (diagnostic.severity == lsp::DiagnosticSeverity::Error ? "E" : "W");
  }
  return out;
}

lsp::json::Value diagnosticJson(int line, int start, int end, std::string_view code)
{
  lsp::json::Object startPos;
  startPos["line"] = lsp::json::Value(line);
  startPos["character"] = lsp::json::Value(start);
  lsp::json::Object endPos;
  endPos["line"] = lsp::json::Value(line);
  endPos["character"] = lsp::json::Value(end);
  lsp::json::Object range;
  range["start"] = lsp::json::Value(std::move(startPos));
  range["end"] = lsp::json::Value(std::move(endPos));
  lsp::json::Object diagnostic;
  diagnostic["range"] = lsp::json::Value(std::move(range));
  diagnostic["code"] = lsp::json::Value(std::string(code));
  return lsp::json::Value(std::move(diagnostic));
}

lsp::json::Value params(const std::string& uri, lsp::json::Array diagnostics)
{
  lsp::json::Object textDocument;
  textDocument["uri"] = lsp::json::Value(uri);
  lsp::json::Object context;
  context["diagnostics"] = lsp::json::Value(std::move(diagnostics));
  lsp::json::Object object;
  object["textDocument"] = lsp::json::Value(std::move(textDocument));
  object["context"] = lsp::json::Value(std::move(context));
  return lsp::json::Value(std::move(object));
}

} // namespace

int main()
{
  std::cout << "start index\n";
  checkEq(scan("panellabel$ = filename$[0,dotindex-1]"), "S24-25", "the reported bug");
  checkEq(scan("panellabel$ = filename$[1,dotindex-1]"), "", "1-based start is valid");
  checkEq(scan("x$ = a$[-1,3]"), "S8-10", "negative start");
  checkEq(scan("x$ = a$[00,3]"), "S8-10", "00 is zero");
  checkEq(scan("x$ = a$[-0,3]"), "S8-10", "-0 is zero");
  checkEq(scan("x$ = a$[ 0 , 3 ]"), "S9-10", "spaces inside the brackets");
  checkEq(scan("x$ = a$[P-1,3]"), "", "expressions are not checked");
  checkEq(scan("x$ = a$[I,I]"), "", "variables are not checked");
  checkEq(
    firstMessage("x$ = a$[0,3]"),
    "Substring start index must be 1 or higher (strings are 1-indexed); got 0.",
    "start message");

  std::cout << "end index\n";
  checkEq(scan("x$ = a$[P+11,0]"), "", "0 end means to the end");
  checkEq(scan("x$ = a$[len(a$),len(a$)]"), "", "calls in both arguments");
  checkEq(scan("x$ = a$[1,-1]"), "N10-12", "negative end");
  checkEq(scan("x$ = a$[0,-2]"), "S8-9,N10-12", "both bad, no order warning");
  checkEq(
    firstMessage("x$ = a$[1,-1]"),
    "Substring end index must be 0 (to end) or a positive position; got -1.",
    "end message");

  std::cout << "start after end\n";
  checkEq(scan("x$ = a$[5,3]"), "O8-11", "range is the bracket contents");
  checkEq(scan("x$ = a$[3,3]"), "", "equal is one character");
  checkEq(scan("x$ = a$[5,0]"), "", "end 0 is never empty");
  checkEq(firstMessage("x$ = a$[5,3]"), "Substring start 5 is after end 3; result is empty.", "order message");

  std::cout << "matching\n";
  checkEq(scan("x$ = \"a$[0,1]\" & b$"), "", "inside double quotes");
  checkEq(scan("x$ = 'a$[0,1]'"), "", "inside single quotes");
  checkEq(scan("x$ = b$ ! a$[0,1]"), "", "after ! comment");
  checkEq(scan("x$ = b$ ;! a$[0,1]"), "", "after ;! comment");
  checkEq(scan("x = a[0,1]"), "", "no $ name");
  checkEq(scan("x$ = a$[0]"), "", "one argument");
  checkEq(scan("x$ = a$[0,1,2]"), "", "three arguments");
  checkEq(scan("x$ = a$[0,"), "", "unclosed bracket");
  checkEq(scan("Ok = dighit(Block$[,Square])"), "", "doc optional-arg notation");
  checkEq(scan("x$ = a$[b$[0,1],0]"), "S11-12", "nested inner substring checked");
  checkEq(scan("x$ = a$[0,b$[1,1]]"), "S8-9", "nested in the end argument");
  checkEq(scan("if cmp(a$[0,1],b$) then"), "S10-11", "inside a call in a condition");
  checkEq(scan("x$ = $DIR_X$[0,2]"), "S13-14", "system variable");
  checkEq(scan("debug a$[0,3]"), "S9-10", "debug line");

  std::cout << "assignment\n";
  checkEq(scan("a$[I,I]=\"T\""), "A0-7", "line start");
  checkEq(scan("  b$[5,8] = \"WELD\""), "A2-9", "indented, spaces around =");
  checkEq(scan("x=1;a$[1,1]=\"T\""), "A4-11", "after ;");
  checkEq(scan("if x=1 then a$[1,1]=\"T\""), "A12-19", "after then");
  checkEq(scan("if x=1 THEN a$[0,1]=\"T\""), "S15-16,A12-19", "start error and assignment");
  checkEq(scan("if a$[1,1]=\"T\" then"), "", "comparison in a condition");
  checkEq(scan("x$ = a$[1,1]"), "", "substring as a value");
  checkEq(scan("a$[1,1]==b$"), "", "== is not assignment");
  checkEq(scan("athen a$[1,1]=\"T\""), "", "then must be a whole word");
  checkEq(
    scan("a$[1,1]=\"T\"").empty() ? "" : findSubstringIssuesInLine("a$[1,1]=\"T\"").front().message,
    "Cannot assign to a substring (A$[i,j] = ...); build the new string with concatenation instead.",
    "assignment message");

  std::cout << "full validate\n";
  checkEq(substringDiagnostics("x$ = a$[0,1] & b$[0,2]\n"), "0:8-9E,0:18-19E", "two on one line survive the post-filter");
  checkEq(substringDiagnostics("x$ = a$[1,-1]\ny$ = a$[5,3]\n"), "0:10-12W,1:8-11W", "warnings");
  checkEq(substringDiagnostics("! x$ = a$[0,1]\n# include \"a$[0,1]\"\n"), "", "comment and # lines skipped");
  checkEq(substringDiagnostics("x$ = a$[0,1] ;!@nodebugstart\ny$ = a$[0,1]\n;!@nodebugend\n"), "", "@nodebug range skipped");
  checkEq(substringDiagnostics("x$ = \xC3\xA5$[0,1]\n"), "0:8-9E", "UTF-16 columns");
  {
    const auto diagnostics = validate("x$ = a$[0,1]\n");
    bool shaped = false;
    for (const auto& diagnostic : diagnostics)
    {
      const auto* code = diagnostic.code ? std::get_if<std::string>(&*diagnostic.code) : nullptr;
      if (code && *code == kSubstringStartIndexCode)
      {
        shaped = diagnostic.source && *diagnostic.source == kSubstringSource;
      }
    }
    check(shaped, "source is Syntax exception - ADT Plugin");
  }

  std::cout << "code actions\n";
  const std::string uri = "file:///c%3A/w/a.prg";
  const std::string text = "panellabel$ = filename$[0,dotindex-1]\n";
  const Snapshot snapshot{.uri = lsp::Uri::parse(uri), .version = 1, .languageId = "vertexadt", .text = text};
  const auto actions = codeActions(
    snapshot, params(uri, lsp::json::Array{diagnosticJson(0, 24, 25, kSubstringStartIndexCode)}), true);
  check(actions.array().size() == 1, "one action");
  if (actions.array().size() == 1)
  {
    const auto& action = actions.array().front().object();
    checkEq(action.find("title")->string(), "Use 1-based start index", "title");
    checkEq(action.find("kind")->string(), "quickfix", "kind");
    const auto& edit = action.find("edit")->object().find("changes")->object().find(uri)->array().front().object();
    checkEq(edit.find("newText")->string(), "1", "replaces with 1");
    const auto& start = edit.find("range")->object().find("start")->object();
    check(start.find("character")->number() == 24, "at the start index");
  }
  const auto none = codeActions(
    snapshot, params(uri, lsp::json::Array{diagnosticJson(0, 24, 25, kSubstringEndIndexCode)}), true);
  check(none.array().empty(), "no fix for the end-index warning");

  std::cout << (failures == 0 ? "all passed\n" : std::to_string(failures) + " failed\n");
  return failures == 0 ? 0 : 1;
}
