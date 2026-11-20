// $DIR_* token table, path display, value tracing and hover.
// Build: cmake --build build --config Release --target dir-paths-test
// Run:   build\Release\dir-paths-test.exe
#include "dir_paths.h"
#include "library_paths.h"

#include <algorithm>
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

std::string show(const std::optional<std::string>& value)
{
  return value ? *value : std::string("<nullopt>");
}

void touch(const fs::path& path)
{
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << "! fixture\n";
}

// Position of the `nth` occurrence of `needle` (+ `offset`) in `text`.
lsp::Position at(std::string_view text, std::string_view needle, int offset = 0)
{
  const auto index = text.find(needle);
  const auto before = text.substr(0, index);
  const auto line = static_cast<lsp::Uint>(std::count(before.begin(), before.end(), '\n'));
  const auto lineStart = before.rfind('\n');
  const auto column = lineStart == std::string_view::npos ? index : index - lineStart - 1;
  return lsp::Position{.line = line, .character = static_cast<lsp::Uint>(column + offset)};
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
  f.root = fs::temp_directory_path() / "vertex-adt-dir-paths-test";
  fs::remove_all(f.root);
  touch(f.root / "custom/macros/triggers/save_building");
  touch(f.root / "system/macros/secframing/globalsizeminpmaxp.prg");
  touch(f.root / "system/dbases/x.prg");
  touch(f.root / "custom/complibs/DOOR_CUSTOM/door_d.vxp");
  fs::create_directories(f.root / "custom/picts");
  fs::create_directories(f.root / "system/symbols");
  f.doc = f.root / "custom/macros/secframing/beam.prg";
  touch(f.doc);
  f.layout.setFolders({lsp::WorkspaceFolder{.uri = uriFromPath(f.root), .name = "lib"}}, std::nullopt);
  return f;
}

std::string displayText(const Fixture& f, std::string_view token, std::string_view remainder)
{
  const auto* row = findDirToken(token);
  if (!row)
  {
    return "<no token>";
  }
  return displayDirPath(f.layout, f.doc, *row, remainder).text;
}

bool hasTarget(const Fixture& f, std::string_view token, std::string_view remainder)
{
  const auto* row = findDirToken(token);
  return row && displayDirPath(f.layout, f.doc, *row, remainder).target.has_value();
}

void tableTests(const Fixture& f)
{
  std::cout << "table\n";
  check(findDirToken("$DIR_MACROS") != nullptr, "$DIR_MACROS is a token");
  check(findDirToken("$dir_macros") == nullptr, "matching is case-sensitive");
  check(findDirToken("$DIR_SYSTEM") != nullptr, "$DIR_SYSTEM is a token");
  check(findDirToken("$DIR_SYSTEM&") == nullptr, "$DIR_SYSTEM& is not");
  check(findDirToken("$DIR_HOME") == nullptr, "tokens without a 'returns' detail are skipped");
  check(findDirToken("$DIR_ACTPROJ") != nullptr, "$DIR_ACTPROJ is a token");

  checkEq(show(f.layout.convertVertexPath("$DIR_MACROS")), "macros", "convert MACROS");
  checkEq(show(f.layout.convertVertexPath("$DIR_SYSPROGS_SHARED")), "shared/sysprogs",
    "convert SYSPROGS_SHARED no longer collides with SYSPROGS");
  checkEq(show(f.layout.convertVertexPath("$DIR_PICTS_SHARED")), "shared/picts",
    "convert PICTS_SHARED no longer collides with PICTS");
  checkEq(show(f.layout.convertVertexPath("$DIR_SYMBOLS_SYSTEM")), "system/symbols",
    "convert SYMBOLS_SYSTEM");
  checkEq(show(f.layout.convertVertexPath("$DIR_CUSTOM")), "custom", "convert CUSTOM");
  checkEq(show(f.layout.convertVertexPath("#$DIR_SETUP#")), "setup", "convert inside #..#");
  checkEq(show(f.layout.convertVertexPath("$dir_macros")), "<nullopt>", "convert is case-sensitive");
  checkEq(show(f.layout.convertVertexPath("$DIR_THIS")), "<nullopt>", "convert THIS needs a document");
  checkEq(show(f.layout.convertVertexPath("$DIR_LIGHTWORKS")), "lw", "convert LIGHTWORKS");
  checkEq(show(f.layout.convertVertexPath("$DIR_PIPELIB")), "pipelibrary", "convert PIPELIB");
}

void displayTests(const Fixture& f)
{
  std::cout << "display\n";
  checkEq(displayText(f, "$DIR_MACROS", ""), "custom/macros", "folder in custom");
  check(hasTarget(f, "$DIR_MACROS", ""), "  ... links");
  checkEq(displayText(f, "$DIR_MACROS", "/triggers/save_building"),
    "custom/macros/triggers/save_building", "file in custom, no extension added");
  check(hasTarget(f, "$DIR_MACROS", "/triggers/save_building"), "  ... links");
  checkEq(displayText(f, "$DIR_MACROS", "/secframing/globalsizeminpmaxp.prg"),
    "system/macros/secframing/globalsizeminpmaxp.prg", "file only in system");
  checkEq(displayText(f, "$DIR_MACROS", "/nope.prg"), "none/macros/nope.prg", "file in neither");
  check(!hasTarget(f, "$DIR_MACROS", "/nope.prg"), "  ... no link");
  checkEq(displayText(f, "$DIR_DBASES", ""), "system/dbases", "folder only in system");
  checkEq(displayText(f, "$DIR_BEND", ""), "none/bend", "folder in neither");
  checkEq(displayText(f, "$DIR_SYMBOLS", ""), "system/symbols", "SYMBOLS falls back like the rest");
  checkEq(displayText(f, "$DIR_SYMBOLS_SYSTEM", ""), "system/symbols", "SYMBOLS_SYSTEM pinned");
  checkEq(displayText(f, "$DIR_SYSBASE", ""), "none/sysbase", "SYSBASE maps to sysbase");
  checkEq(displayText(f, "$DIR_PICTS", ""), "custom/picts", "PICTS");
  checkEq(displayText(f, "$DIR_PICTS_SHARED", ""), "shared/picts", "PICTS_SHARED without a shared folder");
  check(!hasTarget(f, "$DIR_PICTS_SHARED", ""), "  ... no link");
  checkEq(displayText(f, "$DIR_SYSPROGS_SHARED", "/zip.exe"), "shared/sysprogs/zip.exe", "SYSPROGS_SHARED");
  checkEq(displayText(f, "$DIR_SHARED", ""), "shared", "SHARED");
  checkEq(displayText(f, "$DIR_SYSTEM", "/macros"), "system/macros", "SYSTEM");
  checkEq(displayText(f, "$DIR_CUSTOM", "/macros"), "custom/macros", "CUSTOM");
  check(hasTarget(f, "$DIR_CUSTOM", "/macros"), "  ... links");
  checkEq(displayText(f, "$DIR_THIS", ""), "custom/macros/secframing", "THIS is the document folder");
  check(hasTarget(f, "$DIR_THIS", ""), "  ... links");
  checkEq(displayText(f, "$DIR_THIS", "/beam_web.prg"), "custom/macros/secframing/beam_web.prg",
    "THIS with a missing file");
  check(!hasTarget(f, "$DIR_THIS", "/beam_web.prg"), "  ... no link");
  checkEq(displayText(f, "$DIR_ACTPROJ", "/x"), "shared/projects/<actproj>/x", "ACTPROJ helper value");
  check(!hasTarget(f, "$DIR_ACTPROJ", ""), "  ... no link");
  checkEq(displayText(f, "$DIR_MACROS", "/<name$>"), "custom/macros/<name$>",
    "placeholder takes the tree from the folder");
  check(!hasTarget(f, "$DIR_MACROS", "/<name$>"), "  ... no link");
  checkEq(displayText(f, "$DIR_MACROS", "\\triggers\\save_building"),
    "custom/macros/triggers/save_building", "backslashes");
}

void traceTests(const Fixture& f)
{
  std::cout << "trace\n";
  checkEq(show(traceVariableValue("p$ = \"$DIR_MACROS/x.prg\"\n", 0, "p$")), "$DIR_MACROS/x.prg", "literal");
  checkEq(show(traceVariableValue("d$ = get($DIR_FORMS)\n", 0, "d$")), "$DIR_FORMS/", "get() ends with a slash");
  checkEq(show(traceVariableValue("d$=get( $DIR_FORMS )", 0, "d$")), "$DIR_FORMS/", "get() with spaces");

  const std::string chain =
    "a$ = \"$DIR_MACROS\"\n"
    "b$ = a$ & \"/triggers\"\n"
    "c$ = b$ & \"/save_building\"\n"
    "q$ = c$\n";
  checkEq(show(traceVariableValue(chain, 3, "c$")), "$DIR_MACROS/triggers/save_building", "& chain");
  checkEq(show(traceVariableValue(chain, 3, "q$")), "$DIR_MACROS/triggers/save_building", "copy");
  checkEq(show(traceVariableValue(chain, 0, "c$")), "<nullopt>", "assignment below the line is ignored");

  checkEq(show(traceVariableValue("p$ = \"$DIR_MACROS/\" & name$\n", 0, "p$")), "$DIR_MACROS/<name$>",
    "unknown variable");
  checkEq(show(traceVariableValue("p$ = \"$DIR_MACROS/\" & lower(x$)\n", 0, "p$")), "$DIR_MACROS/<expr>",
    "unknown expression");
  checkEq(show(traceVariableValue("p$ = \"$DIR_MACROS\" + \"/x\"\n", 0, "p$")), "<expr>",
    "+ is not concatenation");
  checkEq(show(traceVariableValue("p$ = \"a&b\" & \"c\"\n", 0, "p$")), "a&bc", "& inside a string");

  const std::string last =
    "p$ = \"$DIR_MACROS\"\n"
    "x = 1\n"
    "p$ = \"$DIR_COMPLIBS\"\n"
    "x = 2\n";
  checkEq(show(traceVariableValue(last, 1, "p$")), "$DIR_MACROS", "nearest assignment above (1)");
  checkEq(show(traceVariableValue(last, 3, "p$")), "$DIR_COMPLIBS", "nearest assignment above (2)");

  const std::string self =
    "p$ = \"$DIR_MACROS\"\n"
    "p$ = p$ & \"/triggers\"\n";
  checkEq(show(traceVariableValue(self, 1, "p$")), "$DIR_MACROS/triggers", "self reference");

  const std::string interp =
    "s$ = \"triggers\"\n"
    "p$ = \"$DIR_MACROS/#s$#/save_building\"\n";
  checkEq(show(traceVariableValue(interp, 1, "p$")), "$DIR_MACROS/triggers/save_building", "#var# interpolation");
  checkEq(show(traceVariableValue("p$ = \"#$DIR_SETUP#/GEOMPROP.CUSTOM\"\n", 0, "p$")),
    "$DIR_SETUP/GEOMPROP.CUSTOM", "#$DIR_X# interpolation");
  checkEq(show(traceVariableValue("p$ = \"$DIR_MACROS/#n$#\"\n", 0, "p$")), "$DIR_MACROS/<n$>",
    "unknown interpolated variable");

  checkEq(show(traceVariableValue("p$ = \"$DIR_MACROS\" ! comment & more\n", 0, "p$")), "$DIR_MACROS",
    "trailing comment");
  checkEq(show(traceVariableValue("  p$ = \"$DIR_MACROS\"\r\n", 0, "p$")), "$DIR_MACROS", "indent and CRLF");
  checkEq(show(traceVariableValue("if p$ = \"$DIR_MACROS\"\n", 0, "p$")), "<nullopt>", "if is not an assignment");
  checkEq(show(traceVariableValue("P$ = \"$DIR_MACROS\"\n", 0, "p$")), "<nullopt>", "names are exact");

  std::string hops = "v0$ = \"$DIR_MACROS\"\n";
  for (int i = 1; i <= 9; ++i)
  {
    hops += "v" + std::to_string(i) + "$ = v" + std::to_string(i - 1) + "$\n";
  }
  checkEq(show(traceVariableValue(hops, 9, "v8$")), "$DIR_MACROS", "8 hops resolve");
  checkEq(show(traceVariableValue(hops, 9, "v9$")), "<v0$>", "9 hops stop");

  const auto shown = dirVariableHover(f.layout, f.doc, chain, lsp::Position{.line = 3, .character = 0}, "c$");
  checkEq(shown ? shown->text : "<nullopt>", "custom/macros/triggers/save_building", "variable hover");
  check(shown && shown->target, "  ... links");
  check(!dirVariableHover(f.layout, f.doc, "p$ = \"abc\"\n", lsp::Position{}, "p$"), "no hover without a token");
  check(!dirVariableHover(f.layout, f.doc, hops, lsp::Position{.line = 9, .character = 0}, "v9$"),
    "no hover past the hop cap");
}

std::string varHoverText(const Fixture& f, std::string_view text, lsp::Position position, std::string_view name)
{
  const auto shown = dirVariableHover(f.layout, f.doc, text, position, name);
  return shown ? shown->text + (shown->target ? " (link)" : "") : "<nullopt>";
}

void buildUpTests(const Fixture& f)
{
  std::cout << "building on a resolved path\n";
  const std::string text =
    "Dir$ = get($DIR_COMPLIBS)\n"
    "Lib$ = \"DOOR_CUSTOM\"\n"
    "Dir$ = Dir$&Lib$&\"/\"\n"
    "Path$ = Dir$&\"door_d.vxp\"\n";
  checkEq(varHoverText(f, text, at(text, "Dir$"), "Dir$"), "custom/complibs/ (link)", "line 1 Dir$");
  checkEq(varHoverText(f, text, at(text, "Dir$ = Dir$"), "Dir$"), "custom/complibs/DOOR_CUSTOM/ (link)",
    "line 3 Dir$, left of =");
  checkEq(varHoverText(f, text, at(text, "Dir$&Lib$"), "Dir$"), "custom/complibs/ (link)",
    "line 3 Dir$, right of = uses the previous value");
  checkEq(varHoverText(f, text, at(text, "Dir$&\"door"), "Dir$"), "custom/complibs/DOOR_CUSTOM/ (link)",
    "line 4 Dir$");
  checkEq(varHoverText(f, text, at(text, "Path$"), "Path$"), "custom/complibs/DOOR_CUSTOM/door_d.vxp (link)",
    "line 4 Path$");
  checkEq(varHoverText(f, text, at(text, "Lib$"), "Lib$"), "<nullopt>", "Lib$ holds no token");
  checkEq(show(traceVariableValue(text, 3, "Path$")), "$DIR_COMPLIBS/DOOR_CUSTOM/door_d.vxp", "trace text");

  const std::string dup = "a$ = get($DIR_COMPLIBS) & \"/DOOR_CUSTOM\"\n";
  checkEq(varHoverText(f, dup, at(dup, "a$"), "a$"), "custom/complibs//DOOR_CUSTOM (link)",
    "duplicate slash is shown, existence check ignores it");
  const std::string glued = "g$ = \"$DIR_MACROS\" & \"DOOR\"\n";
  checkEq(varHoverText(f, glued, at(glued, "g$"), "g$"), "none/macrosDOOR", "missing slash is shown glued");
  const std::string gluedVar =
    "Lib$ = \"DOOR\"\n"
    "g$ = \"$DIR_MACROS\" & Lib$\n";
  checkEq(varHoverText(f, gluedVar, at(gluedVar, "g$"), "g$"), "none/macrosDOOR", "missing slash before a variable");
  const std::string interp = "g$ = \"#$DIR_MACROS#DOOR\"\n";
  checkEq(varHoverText(f, interp, at(interp, "g$"), "g$"), "none/macrosDOOR", "missing slash after #token#");
}

std::string tokenHoverText(const Fixture& f, std::string_view text, lsp::Position position)
{
  const auto hover = dirTokenHover(f.layout, f.doc, text, position);
  if (!hover)
  {
    return "<nullopt>";
  }
  return std::string(hover->token->token) + " -> " + hover->path.text;
}

void tokenHoverTests(const Fixture& f)
{
  std::cout << "token hover\n";
  const std::string call = "call $DIR_MACROS/triggers/save_building BRACKETSECTION\n";
  checkEq(tokenHoverText(f, call, at(call, "$DIR_MACROS")),
    "$DIR_MACROS -> custom/macros/triggers/save_building", "call path, on $");
  checkEq(tokenHoverText(f, call, at(call, "$DIR_MACROS", 10)),
    "$DIR_MACROS -> custom/macros/triggers/save_building", "call path, last char");
  checkEq(tokenHoverText(f, call, at(call, "/triggers")), "<nullopt>", "after the token");
  checkEq(tokenHoverText(f, call, at(call, "call")), "<nullopt>", "before the token");

  const std::string include = "# include $DIR_MACROS/triggers/save_building SECTIONOFFILE\n";
  checkEq(tokenHoverText(f, include, at(include, "DIR_")),
    "$DIR_MACROS -> custom/macros/triggers/save_building", "include path");

  const std::string edit = "text_edit \"#$DIR_SETUP#/GEOMPROP.CUSTOM\", \"x\", 3\n";
  checkEq(tokenHoverText(f, edit, at(edit, "$DIR_SETUP")),
    "$DIR_SETUP -> none/setup/GEOMPROP.CUSTOM", "#..# in a string");

  const std::string get = "str$ = get($DIR_FORMS)\n";
  checkEq(tokenHoverText(f, get, at(get, "$DIR_FORMS")), "$DIR_FORMS -> none/forms", "get()");

  const std::string quoted = "Ret$=call(\"$DIR_MACROS/secframing/globalsizeminpmaxp.prg\",checkmac)\n";
  checkEq(tokenHoverText(f, quoted, at(quoted, "$DIR_MACROS")),
    "$DIR_MACROS -> system/macros/secframing/globalsizeminpmaxp.prg", "quoted call path");

  const std::string interp =
    "sub$ = \"triggers\"\n"
    "p$ = \"$DIR_MACROS/#sub$#/save_building\"\n";
  checkEq(tokenHoverText(f, interp, at(interp, "$DIR_MACROS")),
    "$DIR_MACROS -> custom/macros/triggers/save_building", "interpolated remainder");

  const std::string concat = "p$ = \"$DIR_MACROS\" & \"/x\"\n";
  checkEq(tokenHoverText(f, concat, at(concat, "$DIR_MACROS")), "$DIR_MACROS -> custom/macros",
    "token alone in its string");

  const std::string lower = "call $dir_macros/x\n";
  checkEq(tokenHoverText(f, lower, at(lower, "$dir")), "<nullopt>", "lowercase token");
  const std::string home = "p$ = get($DIR_HOME)\n";
  checkEq(tokenHoverText(f, home, at(home, "$DIR_HOME")), "<nullopt>", "token without a mapping");
  const std::string sys = "p$ = \"$DIR_SYSTEM&\"\n";
  checkEq(tokenHoverText(f, sys, at(sys, "$DIR_SYSTEM")), "$DIR_SYSTEM -> system", "$DIR_SYSTEM before &");

  const std::string md = dirPathMarkdown(DirPathDisplay{.text = "shared/projects/<actproj>"});
  checkEq(md, "`shared/projects/<actproj>`", "markdown without link");
  const auto linked = dirPathMarkdown(DirPathDisplay{.text = "custom/macros", .target = f.root / "custom/macros"});
  check(linked.starts_with("[`custom/macros`](file:///") && linked.ends_with("/custom/macros)"),
    "markdown with link  [got: " + linked + "]");
}

} // namespace

int main()
{
  auto fixture = makeFixture();
  tableTests(fixture);
  displayTests(fixture);
  traceTests(fixture);
  buildUpTests(fixture);
  tokenHoverTests(fixture);
  fs::remove_all(fixture.root);
  std::cout << (failures == 0 ? "ALL OK\n" : std::to_string(failures) + " FAILED\n");
  return failures == 0 ? 0 : 1;
}
