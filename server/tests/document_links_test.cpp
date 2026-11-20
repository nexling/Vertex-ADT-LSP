// Document links: named-section suffixes on `call` / `# include` paths.
// Build: cmake --build build --config Release --target document-links-test
// Run:   build\Release\document-links-test.exe
#include "document_links.h"
#include "library_paths.h"
#include "text_util.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;
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

void write(const fs::path& path, std::string_view text)
{
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << text;
}

struct Fixture
{
  fs::path root;
  fs::path doc;
  LibraryLayout layout;
};

Fixture makeFixture()
{
  Fixture f;
  f.root = fs::temp_directory_path() / "vertex-adt-document-links-test";
  fs::remove_all(f.root);
  // #AFTERMPRFLOOR# on line 4 (1-based), #FIRST# on line 2.
  write(f.root / "custom/macros/addtimer.prg",
    "! timer\n#FIRST#\n#FIRST#\n#AFTERMPRFLOOR#\nx = 1\n#AFTERMPRFLOOR#\n");
  f.doc = f.root / "custom/macros/caller.prg";
  write(f.doc, "! caller\n");
  f.layout.setFolders({lsp::WorkspaceFolder{.uri = uriFromPath(f.root), .name = "lib"}}, std::nullopt);
  return f;
}

struct LinkView
{
  bool present = false;
  std::string fragment;
  std::string tooltip;
  unsigned start = 0;
  unsigned end = 0;
};

// The first link on the document's only line.
LinkView linkFor(const Fixture& f, const std::string& line)
{
  Snapshot snapshot{.uri = uriFromPath(f.doc), .version = 1, .languageId = "vertexadt", .text = line};
  const auto links = documentLinks(DocumentLinksRequest{.snapshot = snapshot, .layout = f.layout});
  LinkView view;
  if (links.empty())
  {
    return view;
  }
  const auto& link = links.front();
  view.present = link.target.has_value();
  if (link.target && link.target->hasFragment())
  {
    view.fragment = std::string(link.target->fragment());
  }
  view.tooltip = link.tooltip.value_or("");
  view.start = link.range.start.character;
  view.end = link.range.end.character;
  return view;
}

bool endsWith(const std::string& text, const std::string& tail)
{
  return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

void splitTests()
{
  std::cout << "splitIncludeCallTarget\n";
  auto t = splitIncludeCallTarget("\"$DIR_MACROS/a.prg AFTERMPRFLOOR\"", "");
  checkEq(t.filePath, "$DIR_MACROS/a.prg", "quoted suffix: path");
  checkEq(t.targetSection, "AFTERMPRFLOOR", "quoted suffix: section");
  t = splitIncludeCallTarget("\"$DIR_MACROS/a.prg\"", "");
  checkEq(t.filePath, "$DIR_MACROS/a.prg", "no suffix: path");
  checkEq(t.targetSection, "", "no suffix: section");
  t = splitIncludeCallTarget("\"$DIR_MACROS/a.prg SECTION\"", "");
  checkEq(t.filePath, "$DIR_MACROS/a.prg", "keyword cut from path");
  checkEq(t.targetSection, "", "keyword is not a suffix");
  t = splitIncludeCallTarget("$DIR_MACROS/a.prg", "  BRACKET ;! c");
  checkEq(t.targetSection, "BRACKET", "remainder suffix");
  t = splitIncludeCallTarget("\"a.prg INNER\"", "OUTER");
  checkEq(t.targetSection, "OUTER", "remainder wins");
  t = splitIncludeCallTarget("a.prg", "sectionoffile");
  checkEq(t.targetSection, "", "remainder keyword ignored");
  t = splitIncludeCallTarget("a.prg", ",x$)");
  checkEq(t.targetSection, "", "comma remainder is not a suffix");
}

void linkTests(const Fixture& f)
{
  std::cout << "links\n";

  auto v = linkFor(f, R"(ret$=call("$DIR_MACROS/addtimer.prg",panellabel$))");
  check(v.present, "plain call( links");
  checkEq(v.fragment, "", "  ... no fragment");

  const std::string quoted = R"(ret$=call("$DIR_MACROS/addtimer.prg AFTERMPRFLOOR",panellabel$))";
  v = linkFor(f, quoted);
  check(v.present, "(a) quoted suffix in call( links");
  checkEq(v.fragment, "L4", "  ... first #AFTERMPRFLOOR# line");
  check(endsWith(v.tooltip, "addtimer.prg #AFTERMPRFLOOR#"), "  ... tooltip names the section [" + v.tooltip + "]");
  check(v.start == quoted.find('"') && v.end == quoted.rfind('"') + 1, "  ... range is the whole quoted token");

  v = linkFor(f, R"(call("$DIR_MACROS/addtimer.prg FIRST"))");
  checkEq(v.fragment, "L2", "(a) bare call( with quoted suffix");

  v = linkFor(f, "call $DIR_MACROS/addtimer.prg AFTERMPRFLOOR");
  check(v.present, "(b) call space form links");
  checkEq(v.fragment, "L4", "  ... jumps to the section");

  v = linkFor(f, R"(# include "$DIR_MACROS/addtimer.prg AFTERMPRFLOOR")");
  checkEq(v.fragment, "L4", "(c) # include quoted suffix");

  v = linkFor(f, "# include $DIR_MACROS/addtimer.prg FIRST");
  checkEq(v.fragment, "L2", "(d) # include bare suffix (unchanged)");

  v = linkFor(f, R"(ret$=call("$DIR_MACROS/addtimer.prg NOSUCH",x$))");
  check(v.present, "missing section still links");
  checkEq(v.fragment, "", "  ... to the top of the file");
  check(endsWith(v.tooltip, "addtimer.prg"), "  ... tooltip has no section [" + v.tooltip + "]");

  v = linkFor(f, R"(ret$=call("$DIR_MACROS/addtimer.prg AFTERMPR",x$))");
  checkEq(v.fragment, "", "prefix name is not a match");

  v = linkFor(f, R"(ret$=call("$DIR_MACROS/addtimer.prg SECTION",x$))");
  check(v.present, "SECTION keyword in quotes links the file");
  checkEq(v.fragment, "", "  ... with no fragment");

  v = linkFor(f, R"(ret$=call("$DIR_MACROS/nosuch.prg AFTERMPRFLOOR",x$))");
  check(!v.present, "missing file gives no link");
}

} // namespace

int main()
{
  const auto f = makeFixture();
  splitTests();
  linkTests(f);
  std::cout << (failures == 0 ? "ALL PASSED\n" : std::to_string(failures) + " FAILED\n");
  return failures == 0 ? 0 : 1;
}
