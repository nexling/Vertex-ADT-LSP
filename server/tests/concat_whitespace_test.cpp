// Whitespace around a concatenation `&`: span finder, document walk, and the
// QuickFix / SourceFixAll code actions.
// Build: cmake --build build --config Release --target concat-whitespace-test
// Run:   build\Release\concat-whitespace-test.exe
#include "code_actions.h"
#include "concat_whitespace.h"
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

// Spans rendered as "start-end" pairs, comma separated.
std::string spans(std::string_view line)
{
  std::string out;
  for (const auto& span : findConcatWhitespaceInLine(line))
  {
    out += (out.empty() ? "" : ",") + std::to_string(span.startByte) + "-"
      + std::to_string(span.endByte);
  }
  return out;
}

std::string issues(std::string_view text)
{
  const auto lines = splitLines(text);
  const auto sections = parseFileSections(lines);
  std::string out;
  for (const auto& issue : findConcatWhitespaceIssues(lines, sections))
  {
    out += (out.empty() ? "" : ",") + std::to_string(issue.line) + ":"
      + std::to_string(issue.startCharacter) + "-" + std::to_string(issue.endCharacter);
  }
  return out;
}

// Applies one action's edits (single line each, sorted bottom-up by the server).
std::string applyEdits(std::string text, const lsp::json::Value& action, const std::string& uri)
{
  const auto& edits = action.object().find("edit")->object().find("changes")->object().find(uri)->array();
  for (const auto& edit : edits)
  {
    const auto& range = edit.object().find("range")->object();
    const auto line = static_cast<std::size_t>(range.find("start")->object().find("line")->number());
    const auto start = static_cast<std::size_t>(range.find("start")->object().find("character")->number());
    const auto end = static_cast<std::size_t>(range.find("end")->object().find("character")->number());
    std::size_t lineStart = 0;
    for (std::size_t i = 0; i < line; ++i)
    {
      lineStart = text.find('\n', lineStart) + 1;
    }
    text.replace(lineStart + start, end - start, edit.object().find("newText")->string());
  }
  return text;
}

lsp::json::Value diagnosticJson(int line, int start, int end)
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
  diagnostic["code"] = lsp::json::Value(std::string(kConcatWhitespaceCode));
  diagnostic["message"] = lsp::json::Value(std::string(kConcatWhitespaceMessage));
  return lsp::json::Value(std::move(diagnostic));
}

lsp::json::Value params(const std::string& uri, lsp::json::Array diagnostics, const char* only = nullptr)
{
  lsp::json::Object textDocument;
  textDocument["uri"] = lsp::json::Value(uri);
  lsp::json::Object context;
  context["diagnostics"] = lsp::json::Value(std::move(diagnostics));
  if (only)
  {
    context["only"] = lsp::json::Value(lsp::json::Array{lsp::json::Value(std::string(only))});
  }
  lsp::json::Object object;
  object["textDocument"] = lsp::json::Value(std::move(textDocument));
  object["context"] = lsp::json::Value(std::move(context));
  return lsp::json::Value(std::move(object));
}

const lsp::json::Value* actionOfKind(const lsp::json::Value& actions, std::string_view kind)
{
  for (const auto& action : actions.array())
  {
    if (action.object().find("kind")->string() == kind)
    {
      return &action;
    }
  }
  return nullptr;
}

} // namespace

int main()
{
  std::cout << "line spans\n";
  checkEq(spans(R"(x$= date$&" "&time$)"), "", "no whitespace is valid");
  checkEq(spans(R"(x$= date$ &" " &time$)"), "9-11,14-16", "space before each & flagged");
  checkEq(spans(R"(x$= a$& b$)"), "6-8", "space after &");
  checkEq(spans(R"(x$= a$ & b$)"), "6-9", "both sides in one span");
  checkEq(spans("x$= a$\t&\tb$"), "6-9", "tabs count");
  checkEq(spans(R"(x$= a$  &   b$)"), "6-12", "runs of spaces");
  checkEq(spans(R"(x$= "a & b"&c$)"), "", "& inside double quotes ignored");
  checkEq(spans(R"(x$= 'a & b'&c$)"), "", "& inside single quotes ignored");
  checkEq(spans(R"(if a=1 && b=2 then)"), "", "&& ignored");
  checkEq(spans(R"(x$= a$&b$ ! c$ & d$)"), "", "inline ! comment ignored");
  checkEq(spans(R"(x$= a$&b$ ;! c$ & d$)"), "", ";! comment ignored");
  checkEq(spans(R"(x$= a$ &\)"), "6-8", "space before & at continuation");
  checkEq(spans(R"(x$= a$&\)"), "", "& then continuation is valid");
  checkEq(spans("x$= a$ &b$\r"), "6-8", "trailing CR is not whitespace");
  checkEq(spans(R"(debug a$ &b$)"), "8-10", "debug lines are code");
  checkEq(spans(R"(# include path$ &"x.prg")"), "15-17", "include path expression");

  std::cout << "document walk\n";
  checkEq(issues("  x$= a$ &b$\\\n    &c$"), "0:8-10", "continuation indentation does not count");
  checkEq(issues("! x$= a$ & b$\n##included_from a & b.prg\nx$= a$&b$"), "", "comment and ## lines skipped");
  checkEq(issues("#EDIT_FORM#\nx$= a$ & b$\n#EDIT_FORM#\nx$= a$ &b$"), "3:6-8", "EDIT_FORM section skipped");
  checkEq(issues("x$= a$ &b$ ;!@nodebugstart\nx$= a$ &b$\n;!@nodebugend\nx$= a$ &b$"), "3:6-8", "@nodebug range skipped");
  checkEq(issues("x$= \xC3\xA5$ &b$"), "0:6-8", "UTF-16 columns");

  std::cout << "full validate\n";
  {
    const Snapshot doc{
      .uri = lsp::Uri::parse("file:///c%3A/w/v.prg"),
      .version = 1,
      .languageId = "vertexadt",
      .text = "x$= date$ &\" \" &time$\ny$= a$&b$\n"};
    const auto diagnostics = fullDocumentValidate(ValidateContext{}, doc);
    std::string found;
    for (const auto& diagnostic : diagnostics)
    {
      const auto* code = diagnostic.code ? std::get_if<std::string>(&*diagnostic.code) : nullptr;
      if (code && *code == kConcatWhitespaceCode)
      {
        found += (found.empty() ? "" : ",") + std::to_string(diagnostic.range.start.line) + ":"
          + std::to_string(diagnostic.range.start.character) + "-"
          + std::to_string(diagnostic.range.end.character)
          + (diagnostic.severity == lsp::DiagnosticSeverity::Error ? "E" : "?");
      }
    }
    checkEq(found, "0:9-11E,0:14-16E", "both & on one line survive the post-filter as errors");
  }

  std::cout << "code actions\n";
  const std::string uri = "file:///c%3A/w/a.prg";
  const std::string text = "x$= date$ &\" \" &time$\ny$= a$ & b$\n";
  const Snapshot snapshot{.uri = lsp::Uri::parse(uri), .version = 1, .languageId = "vertexadt", .text = text};

  const auto quick = codeActions(
    snapshot, params(uri, lsp::json::Array{diagnosticJson(0, 9, 11)}, "quickfix"), true);
  const auto* fix = actionOfKind(quick, "quickfix");
  check(fix != nullptr, "quickfix offered");
  if (fix)
  {
    checkEq(applyEdits(text, *fix, uri), "x$= date$&\" \" &time$\ny$= a$ & b$\n", "quickfix fixes one &");
  }
  check(actionOfKind(quick, "source.fixAll") == nullptr, "only=quickfix gives no fixAll");

  const auto all = codeActions(
    snapshot, params(uri, lsp::json::Array{diagnosticJson(0, 9, 11)}), true);
  const auto* fixAll = actionOfKind(all, "source.fixAll");
  check(fixAll != nullptr, "fixAll offered next to the diagnostic");
  if (fixAll)
  {
    checkEq(applyEdits(text, *fixAll, uri), "x$= date$&\" \"&time$\ny$= a$&b$\n", "fixAll fixes the file");
    checkEq(fixAll->object().find("title")->string(), "Remove spaces around '&' in file (3)", "fixAll title");
  }

  const auto onSave = codeActions(snapshot, params(uri, {}, "source.fixAll"), true);
  check(actionOfKind(onSave, "source.fixAll") != nullptr, "explicit source.fixAll without diagnostics");

  const auto unrelated = codeActions(snapshot, params(uri, {}), true);
  check(actionOfKind(unrelated, "source.fixAll") == nullptr, "no fixAll on an unrelated lightbulb");

  const Snapshot clean{.uri = lsp::Uri::parse(uri), .version = 1, .languageId = "vertexadt", .text = "x$= a$&b$\n"};
  const auto none = codeActions(clean, params(uri, {}, "source.fixAll"), true);
  check(actionOfKind(none, "source.fixAll") == nullptr, "no fixAll when nothing to fix");

  std::cout << (failures == 0 ? "all passed\n" : std::to_string(failures) + " failed\n");
  return failures == 0 ? 0 : 1;
}
