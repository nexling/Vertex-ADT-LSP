// Drawing, model and file command entries (docs/adr/0020-drawing-file-command-entries.md):
// dwgopen_bg, dwgactivate, dwgempty, group_updating, groupf, groupv, dwgsetscale,
// linewidth, text_type, dwgredraw, dwgupdatetext, dwgsetlimits, dwgstore,
// dwgclose, modelopen, fileremove, write, modelstore. Covers the
// parenthesesOptional statement form and `;`-separated statements.
// Build: cmake --build build --config Release --target drawing-file-commands-test
// Run:   build\Release\drawing-file-commands-test.exe
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

const std::string kPrelude =
  "File$ = \"a.vxp\"\nFpath$ = \"p/\"\ncallname$ = \"c\"\nstring$ = \"s\"\nOk = 0\nok = 0\n";
constexpr int kFirstBodyLine = 6;

// Every diagnostic message on the lines of `body` (after the prelude), joined with " | ".
std::string messages(const std::string& body)
{
  const std::string text = kPrelude + body + "\n";
  ValidateContext ctx;
  const Snapshot doc{
    .uri = lsp::Uri::parse("file:///c%3A/w/drawing.prg"),
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

} // namespace

int main()
{
  std::cout << "the requested block is clean\n";
  checkEq(
    messages(
      "Ok=dwgopen_bg(\"Eff_CS\",File$,1,1,20,108)\n"
      "  Ok=dwgactivate(\"Eff_CS\")\n"
      "  dwgempty\n"
      "  group_updating 1\n"
      "  groupf(1) Fpath$&callname$\n"
      "  dwgsetscale 1\n"
      "  linewidth 0.18\n"
      "  text_type 1\n"
      "  dwgredraw\n"
      "  dwgupdatetext\n"
      "  dwgsetlimits\n"
      "  Ok=dwgstore(File$,0)\n"
      "  dwgactivate \"BASEDWG\"\n"
      "  dwgclose \"Eff_CS\"\n"
      "modelopen \"Eff_CS2\",model=File$\n"
      "  Ok=dwgactivate(\"Eff_CS2\")\n"
      "fileremove \"./temp.txt\"\n"
      "write(500) \"groupv(1) \"&Fpath$&callname$&\",\"&string$\n"
      "ok = modelstore(File$)\n"
      "  dwgclose \"Eff_CS2\""),
    "", "whole block");

  std::cout << "corpus forms stay clean (C:\\vxbd320)\n";
  checkEq(messages("Ok=dwgactivate \"BASEDWG\""), "", "assignment without parentheses");
  checkEq(messages("dwgactivate File$"), "", "dwgactivate with a string variable");
  checkEq(messages("fileremove(File$)"), "", "fileremove with parentheses");
  checkEq(messages("fileremove Fpath$&\"x.txt\""), "", "fileremove concatenation");
  checkEq(messages("linewidth 0.25; text_type 1"), "", "two statements on a line");
  checkEq(messages("text_type -1;      dwgsetscale 2;  linewidth 0.5"), "", "three statements");
  checkEq(messages("dwgsetlimits 0.5, 0.5, 0.5, 0.5"), "", "dwgsetlimits with limits");
  checkEq(messages("groupv(1) File$, \"X\", Ok, \"Y\", Ok"), "", "groupv name/value pairs");
  checkEq(messages("groupf File$"), "", "groupf without an index is not validated");
  checkEq(messages("modelopen File$ model=File$"), "", "modelopen with a space before model=");
  checkEq(messages("modelopen File$, model= File$"), "", "modelopen model= with a space");
  checkEq(messages("Ok = dwgstore(File$,3,\"setup\")"), "", "dwgstore with mode and setup");
  checkEq(messages("ok = modelstore(File$,Ok,1)"), "", "modelstore with three parameters");
  checkEq(messages("if Ok = 1 then dwgclose \"Eff_CS\""), "", "after then");
  checkEq(messages("write(1) Ok"), "", "write a number variable (system/macros/sort)");
  checkEq(messages("text_type"), "", "text_type without a value (WCRNPLATE_pre)");
  checkEq(messages("linewidth .25"), "", "number without a leading zero (log_op.3d)");
  checkEq(messages("linewidth -.5"), "", "negative number without a leading zero");
  checkEq(messages("LineWidth = Ok"), "", "variable named like a command");
  checkEq(messages("LineWidth(1) = Ok\nLineWidth(2) = LineWidth(1)"), "", "array element named like a command (PDFactdwg.prg)");
  checkEq(
    messages("string$ = \"plot \"\"x\"\"\"  ; ! Print PDF with linewidth$"), "",
    "`; !` comment after a statement (pdfplotset)");
  checkEq(
    messages("linewidth File$ ; ! note"), "Width parameter must be a valid variable in linewidth",
    "statement before a `; !` comment is still checked");
  checkEq(
    messages("groupv(1) File$,\"HEIGHT\",Ok,\\\n  \"WIDTH\",Ok"), "", "groupv continued with \\ (voljari)");
  checkEq(
    messages("modelopen \"MODEL\",\\\n  model=File$"), "", "modelopen continued with \\ (openmodel)");

  std::cout << "invalid uses are flagged\n";
  checkEq(
    messages("Ok=dwgopen_bg(\"Eff_CS\",File$,1,1,20)"),
    "dwgopen_bg requires at least 6 parameters inside parentheses", "dwgopen_bg too few");
  checkEq(
    messages("Ok=dwgopen_bg(1,File$,1,1,20,108)"),
    "Window name parameter must be a valid string in dwgopen_bg", "dwgopen_bg number as window name");
  checkEq(
    messages("dwgactivate"),
    "dwgactivate requires at least 1 parameters inside parentheses", "bare dwgactivate");
  checkEq(messages("dwgempty 1"), "dwgempty accepts at most 0 parameters after parentheses", "dwgempty 1");
  checkEq(
    messages("group_updating File$"),
    "Mode parameter must be a valid variable in group_updating", "group_updating string");
  checkEq(
    messages("groupf(File$) File$"),
    "Group index parameter must be a valid variable in groupf", "groupf string index");
  checkEq(
    messages("groupf(1) 5"),
    "File parameter after parentheses must be a valid string in groupf", "groupf number file");
  checkEq(messages("linewidth File$"), "Width parameter must be a valid variable in linewidth", "linewidth string");
  checkEq(
    messages("linewidth File$; text_type 1"),
    "Width parameter must be a valid variable in linewidth", "first of two statements");
  checkEq(
    messages("dwgsetlimits 1,2,3,4,5"),
    "dwgsetlimits accepts at most 4 parameters after parentheses", "dwgsetlimits five limits");
  checkEq(messages("Ok=dwgstore(1)"), "File parameter must be a valid string in dwgstore", "dwgstore number");
  checkEq(messages("dwgclose 1"), "Window name parameter must be a valid string in dwgclose", "dwgclose number");
  checkEq(
    messages("Ok=dwgclose \"x\""), "dwgclose should not have an assignment variable", "dwgclose assignment");
  checkEq(
    messages("fileremove 5"), "File path parameter must be a valid string in fileremove", "fileremove number");
  checkEq(
    messages("Ok=fileremove \"x\""), "fileremove should not have an assignment variable", "fileremove assignment");
  checkEq(messages("write(1)"), "write requires at least 1 parameters after parentheses", "write without text");
  checkEq(
    messages("write(1) \"done on \"getdate()\" at \"&gettime()"),
    "Text parameter after parentheses must be a valid string or variable in write",
    "write with a missing & (AUTODRWN.prg)");
  checkEq(
    messages("ok = modelstore(File$,\"x\")"),
    "Mode parameter must be a valid variable in modelstore", "modelstore string mode");

  std::cout << (failures == 0 ? "all passed\n" : std::to_string(failures) + " failed\n");
  return failures == 0 ? 0 : 1;
}
