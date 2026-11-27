// File, map, error and form command entries
// (docs/adr/0028-file-map-error-form-command-entries.md): file_exists,
// dircreate, map_store, show_error, edit_obj_data.
#include "validate.h"

#include <iostream>
#include <string>

using namespace vertexadt;

namespace {

int failures = 0;
bool strict = false;

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

const std::string kPrelude =
  "Params = get($CALLPARAM1)\n"
  "This = get($CALLPARAM2)\n"
  "ok = 0\n"
  "num = 0\n"
  "Time_in_ms = 5000\n"
  "Path$ = \"p\"\n"
  "Map$ = \"m\"\n"
  "Form$ = \"f\"\n"
  "header$ = \"h\"\n"
  "Message$ = \"m\"\n";
constexpr int kFirstBodyLine = 10;

// Every diagnostic message on the lines of `body` (after the prelude), joined with " | ".
std::string messages(const std::string& body)
{
  const std::string text = kPrelude + body + "\n";
  ValidateContext ctx;
  ctx.settings.strictModeEnabled = strict;
  const Snapshot doc{
    .uri = lsp::Uri::parse("file:///c%3A/w/commands.prg"),
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
    if (const auto* message = std::get_if<std::string>(&diagnostic.message))
    {
      out += (out.empty() ? "" : " | ") + *message;
    }
  }
  return out;
}

void flagged(const std::string& line, const std::string& want)
{
  checkEq(messages(line), want, line);
}

} // namespace

int main()
{
  std::cout << "the requested forms are clean\n";
  checkEq(
    messages(
      "file_exists(\"string\")\n"
      "dircreate \"string\"\n"
      "ok = map_store(\"string\")\n"
      "show_error \"string\"\n"
      "show_error \"string\",\"string\"\n"
      "show_error \"string\",\"string\",5000\n"
      "ok = edit_obj_data(Params,\"string\",0)"),
    "", "whole block");

  std::cout << "corpus forms stay clean (C:\\vxbd320)\n";
  checkEq(messages("ok = file_exists(Path$)"), "", "file_exists assigned");
  checkEq(messages("if (len(Path$) && file_exists(Path$)) then\nendif"), "", "file_exists in an if condition");
  checkEq(messages("if file_exists(Path$)=1 then\nendif"), "", "file_exists compared");
  checkEq(messages("dircreate Path$"), "", "dircreate variable");
  checkEq(messages("dircreate(Path$)"), "", "dircreate with parentheses");
  checkEq(messages("map_store(Map$)"), "", "map_store without assignment");
  checkEq(messages("show_error Message$"), "", "show_error variable message");
  checkEq(messages("show_error \"a \"&Path$&\" b\""), "", "show_error concatenated message");
  checkEq(messages("show_error Message$, header$, Time_in_ms"), "", "show_error all variables");
  checkEq(messages("show_error \"a\", header$"), "", "show_error header variable");
  checkEq(messages("show_error \"a\" \"b\""), "", "show_error separated by a space");
  checkEq(messages("show_error \"a\"&num&\"b\", \"c\",1000"), "", "show_error number in the message");
  checkEq(messages("if edit_obj_data(Params,\"FHM_form\",0) then\nendif"), "", "edit_obj_data in an if condition");
  checkEq(messages("ok = edit_obj_data(This,Form$,1)"), "", "edit_obj_data form variable");
  checkEq(messages("edit_obj_data(This,\"f.vxf\",1)"), "", "edit_obj_data without assignment");
  checkEq(messages("ok = edit_obj_data(This,\"f\",2)"), "", "edit_obj_data other mode literal");

  std::cout << "invalid uses are flagged\n";
  flagged("ok = file_exists()", "file_exists requires at least 1 parameters inside parentheses");
  flagged("ok = file_exists(Path$,1)", "file_exists accepts at most 1 parameters inside parentheses");
  flagged("ok = file_exists(5)", "File path parameter must be a valid string in file_exists");
  flagged("ok = dircreate Path$", "dircreate should not have an assignment variable");
  flagged("dircreate 5", "Directory path parameter must be a valid string in dircreate");
  flagged("dircreate Path$,Path$", "dircreate accepts at most 1 parameters inside parentheses");
  flagged("ok = map_store(5)", "Map parameter must be a valid string in map_store");
  flagged("ok = map_store(Map$,1)", "map_store accepts at most 1 parameters inside parentheses");
  flagged("show_error", "show_error requires at least 1 parameters after parentheses");
  flagged("show_error 5", "Message parameter must be a valid string in show_error");
  flagged("show_error \"a\",5000", "Header parameter must be a valid string in show_error");
  flagged("show_error \"a\",0", "Header parameter must be a valid string in show_error");
  flagged("show_error \"a\",\"b\",\"c\"", "Time parameter must be a valid variable in show_error");
  flagged("show_error \"a\",\"b\",1,2", "show_error accepts at most 3 parameters after parentheses");
  flagged("ok = show_error \"a\"", "show_error should not have an assignment variable");
  flagged("ok = edit_obj_data(Params,\"f\")", "edit_obj_data requires at least 3 parameters inside parentheses");
  flagged("ok = edit_obj_data(Params,\"f\",0,1)", "edit_obj_data accepts at most 3 parameters inside parentheses");
  flagged("ok = edit_obj_data(\"x\",\"f\",0)", "Object parameter must be a valid variable in edit_obj_data");
  flagged("ok = edit_obj_data(Params,0,0)", "Form parameter must be a valid string in edit_obj_data");
  flagged("ok = edit_obj_data(Params,\"f\",\"0\")", "Mode parameter must be a valid variable in edit_obj_data");

  std::cout << "strict mode\n";
  strict = true;
  checkEq(
    messages(
      "ok = file_exists(Path$)\n"
      "dircreate(Path$)\n"
      "ok = map_store(Map$)\n"
      "show_error Message$, header$, Time_in_ms\n"
      "ok = edit_obj_data(Params,Form$,num)\n"
      "ok = edit_obj_data(This,\"f\",1)"),
    "", "typed block is clean");
  flagged("ok = file_exists(num)", "File path parameter must be a valid string in file_exists");
  flagged("show_error Message$, header$, Path$", "Time parameter must be a valid variable in show_error");

  std::cout << (failures == 0 ? "all passed\n" : std::to_string(failures) + " failed\n");
  return failures == 0 ? 0 : 1;
}
