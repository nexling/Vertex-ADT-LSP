// Native snippet and catalog corrections (docs/adr/0016-native-snippet-and-catalog-corrections.md):
// get_obj_param / set_djmember / new_feature snippets match their syntax,
// dbdelete is validated without parentheses, dbgetactrow with them.
// Build: cmake --build build --config Release --target snippet-catalog-corrections-test
// Run:   build\Release\snippet-catalog-corrections-test.exe
#include "catalog/catalog.h"
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

std::string insertText(std::string_view label)
{
  for (const auto& entry : catalog::keywordCatalog())
  {
    if (entry.label == label)
    {
      return entry.insertText ? entry.insertText : "<none>";
    }
  }
  return "<missing>";
}

const std::string kPrelude = "Db$ = \"x\"\n";

// Messages on the lines of `body` (after the prelude) that mention `word`,
// joined with " | ".
std::string messages(const std::string& body, const std::string& word)
{
  const std::string text = kPrelude + body + "\n";
  constexpr int kFirstBodyLine = 1;
  ValidateContext ctx;
  const Snapshot doc{
    .uri = lsp::Uri::parse("file:///c%3A/w/snippets.prg"),
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
    if (!message || message->find(word) == std::string::npos)
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
  std::cout << "snippets\n";
  checkEq(insertText("get_obj_param"), "get_obj_param(${1:in_Obj},\"${2:FIELD}\")", "get_obj_param");
  checkEq(
    insertText("set_djmember"), "set_djmember(${1:in_Joint}) ${2:in_Object},\"${3:ROLE}\"", "set_djmember");
  checkEq(insertText("new_feature"), "new_feature(\"${1:TYPE}\",\"${2:0}\",\"${3:0}\")", "new_feature");

  std::cout << "snippet text (placeholders filled) validates\n";
  checkEq(messages("o = 1\nv$ = get_obj_param(o,\"FIELD\")", "get_obj_param"), "", "get_obj_param");
  checkEq(messages("j = 1\nm = 1\nset_djmember(j) m,\"ROLE\"", "set_djmember"), "", "set_djmember");
  checkEq(messages("f = new_feature(\"TYPE\",\"0\",\"0\")", "new_feature"), "", "new_feature");

  std::cout << "dbdelete: no parentheses (vxbd320 corpus)\n";
  checkEq(messages("dbdelete \"./\"&Db$", "dbdelete"), "", "string concatenation");
  checkEq(messages("dbdelete \"./nearPanels\"", "dbdelete"), "", "string literal");
  checkEq(messages("dbdelete Db$", "dbdelete"), "", "string variable");
  checkEq(messages("DBID1 = 1\ndbdelete DBID1", "dbdelete"), "", "database id variable (customb)");
  checkEq(messages("id2 = 1\ndbdelete id2", "dbdelete"), "", "lower-case id variable (customb)");
  checkEq(
    messages("dbdelete", "dbdelete"),
    "dbdelete requires at least 1 parameters after parentheses",
    "missing parameter");
  checkEq(messages("dbdelete 5", "dbdelete"), "", "a bare number passes, as for unset/exit (string-or-variable slot)");
  checkEq(messages("dbdelete(\"./x\")", "dbdelete"), "Invalid syntax for 'dbdelete'", "parentheses");
  checkEq(
    messages("dbdelete \"a\",\"b\"", "dbdelete"),
    "dbdelete accepts at most 1 parameters after parentheses",
    "too many parameters");

  std::cout << "dbgetactrow: parentheses (vxbd320 corpus)\n";
  checkEq(messages("Row_id=dbgetactrow(\"PROREG\")", "dbgetactrow"), "", "assigned");
  checkEq(messages("dbgetactrow(\"PICREG\")", "dbgetactrow"), "", "bare statement");
  checkEq(
    messages("r = dbgetactrow()", "dbgetactrow"),
    "dbgetactrow requires at least 1 parameters inside parentheses",
    "missing parameter");
  checkEq(
    messages("r = dbgetactrow(\"A\") \"B\"", "dbgetactrow"),
    "dbgetactrow accepts at most 0 parameters after parentheses",
    "parameter after parentheses");

  if (failures > 0)
  {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "all passed\n";
  return 0;
}
