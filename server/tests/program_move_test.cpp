// Program move engine (docs/adr/0017-native-program-move.md).
// Build: cmake --build build --config Release --target program-move-test
// Run:   build\Release\program-move-test.exe
#include "library_paths.h"
#include "program_move.h"
#include "text_util.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
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

void write(const fs::path& path, const std::string& text)
{
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << text;
}

std::string readAll(const fs::path& path)
{
  std::ifstream in(path, std::ios::binary);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

struct Fixture
{
  fs::path root;
  LibraryLayout layout;
  fs::path custom() const { return root / "custom"; }
  fs::path system() const { return root / "system"; }
};

Fixture makeFixture()
{
  Fixture f;
  f.root = fs::temp_directory_path() / "vertex-adt-program-move-test";
  fs::remove_all(f.root);
  write(f.custom() / "macros/secframing/beam.prg",
    "# include frag.p\n"
    "call(\"beam.prg INNER\")\n"
    "#INNER#\n"
    "#END_INNER#\n");
  write(f.custom() / "macros/secframing/frag.p", "##included_from beam.prg\nx = 1\n");
  write(f.custom() / "macros/secframing/sibling.prg", "# include beam.prg\n");
  write(f.custom() / "macros/caller.prg",
    "x = call(\"$DIR_MACROS/secframing/beam.prg\", 1)\n"
    "y = call(\"$DIR_MACROS/Secframing/beam.prg SETUP\")\n"
    "call \"$DIR_MACROS/secframing/beam.prg\"\n"
    "n$ = \"beam.prg\"\n"
    ";! only a comment about beam.prg\n"
    "r = call(path$ & \"beam.prg\")\n"
    "z = call(\"$DIR_MACROS/secframing/sibling.prg\")\n");
  write(f.system() / "macros/syscaller.prg", "call(\"$DIR_MACROS/secframing/beam.prg\")\n");
  write(f.system() / "macros/shared.prg", "! system original\n");
  write(f.custom() / "macros/shared.prg", "! custom override\n");
  write(f.system() / "complibs/taken/x.prg", "! system\n");
  f.layout.setFolders({lsp::WorkspaceFolder{.uri = uriFromPath(f.root), .name = "lib"}}, std::nullopt);
  return f;
}

std::vector<fs::path> libraryFiles(const Fixture& f)
{
  std::vector<fs::path> files;
  for (const auto& tree : {f.custom(), f.system()})
  {
    for (const auto& entry : fs::recursive_directory_iterator(tree))
    {
      if (entry.is_regular_file())
      {
        files.push_back(entry.path());
      }
    }
  }
  return files;
}

ProgramMovePlan plan(const Fixture& f, MoveOrigin origin, fs::path from, fs::path to, bool confirm = false)
{
  ProgramMoveInput input{.layout = f.layout};
  input.renames.push_back(MoveRename{.oldPath = std::move(from), .newPath = std::move(to)});
  input.origin = origin;
  input.confirmShadow = confirm;
  input.scanFiles = [&f]() { return libraryFiles(f); };
  input.textForPath = [](const fs::path& path) -> std::optional<std::string> {
    if (!fs::is_regular_file(path))
    {
      return std::nullopt;
    }
    return readAll(path);
  };
  return planProgramMove(input);
}

// The file text after the plan's edits (ASCII fixtures: characters = bytes).
std::string edited(const ProgramMovePlan& p, const fs::path& file)
{
  auto text = readAll(file);
  for (const auto& fileEdits : p.edits)
  {
    if (normalizeFsPath(fileEdits.file.string()) != normalizeFsPath(file.string()))
    {
      continue;
    }
    auto lines = std::vector<std::string>{};
    for (const auto line : splitLines(text))
    {
      lines.emplace_back(line);
    }
    auto edits = fileEdits.edits;
    std::sort(edits.begin(), edits.end(), [](const auto& a, const auto& b) {
      return a.line != b.line ? a.line > b.line : a.startCharacter > b.startCharacter;
    });
    for (const auto& edit : edits)
    {
      auto& line = lines[static_cast<std::size_t>(edit.line)];
      line.replace(
        static_cast<std::size_t>(edit.startCharacter),
        static_cast<std::size_t>(edit.endCharacter - edit.startCharacter),
        edit.newText);
    }
    text.clear();
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
      text += lines[i];
      if (i + 1 < lines.size())
      {
        text += '\n';
      }
    }
  }
  return text;
}

int skippedWith(const ProgramMovePlan& p, std::string_view reasonPart)
{
  return static_cast<int>(std::count_if(p.skipped.begin(), p.skipped.end(), [&](const auto& site) {
    return site.reason.find(reasonPart) != std::string::npos;
  }));
}

void testScan()
{
  std::cout << "scanMoveReferences\n";
  const auto refs = scanMoveReferences(
    "x = call(\"$DIR_MACROS/a.prg SECT\", 1)\n"
    "# include 'b.p' PART\n"
    "##included_from c.prg\n"
    "! call(\"d.prg\")\n"
    "msg$ = \"call(e.prg)\"\n"
    "call SECTION FOO\n"
    "y = call($DIR_MACROS/f.prg)\n"
    "call g.prg ;! call(\"h.prg\")\n");
  std::vector<std::string> paths;
  for (const auto& ref : refs)
  {
    paths.push_back(std::to_string(ref.line) + ":" + ref.path);
  }
  std::string joined;
  for (const auto& p : paths)
  {
    joined += p + " ";
  }
  checkEq(joined, "0:$DIR_MACROS/a.prg 1:b.p 2:c.prg 6:$DIR_MACROS/f.prg 7:g.prg ", "paths, kinds, skips");
  check(!refs.empty() && refs[0].begin == 10 && refs[0].end == 10 + std::string("$DIR_MACROS/a.prg").size(),
    "path byte range excludes quote and section suffix");
}

void testFormat(const Fixture& f)
{
  std::cout << "formatMovePath\n";
  const auto caller = f.custom() / "macros";
  const auto target = f.custom() / "complibs/tools/beam2.prg";
  checkEq(show(formatMovePath(f.layout, "$DIR_MACROS/secframing/beam.prg", false, caller, target)),
    "$DIR_COMPLIBS/tools/beam2.prg", "token swaps to the folder's token");
  checkEq(show(formatMovePath(f.layout, "$DIR_MACROS/Secframing/beam.prg", false, caller,
            f.custom() / "macros/secframing/sub/beam.prg")),
    "$DIR_MACROS/Secframing/sub/beam.prg", "caller's directory case is kept");
  checkEq(show(formatMovePath(f.layout, "beam.prg", true, f.custom() / "macros/secframing",
            f.custom() / "macros/secframing/Beam2.prg")),
    "Beam2.prg", "document-relative stays relative next to the caller");
  checkEq(show(formatMovePath(f.layout, "beam.prg", true, f.custom() / "macros/secframing", target)),
    "$DIR_COMPLIBS/tools/beam2.prg", "document-relative elsewhere becomes a token path");
  checkEq(show(formatMovePath(f.layout, "$DIR_THIS/beam.prg", true, f.custom() / "macros",
            f.custom() / "macros/a/beam.prg")),
    "$DIR_THIS/a/beam.prg", "$DIR_THIS stays when under the caller");
  checkEq(show(formatMovePath(f.layout, "$DIR_MACROS\\secframing\\beam.prg", false, caller, target)),
    "$DIR_COMPLIBS\\tools\\beam2.prg", "backslash style is kept");
  checkEq(show(formatMovePath(f.layout, "macros/secframing/beam.prg", false, caller, target)),
    "complibs/tools/beam2.prg", "bare library-relative stays library-relative");
  checkEq(show(formatMovePath(f.layout, "$DIR_MACROS/beam.prg", false, caller, f.custom() / "toplevel/beam.prg")),
    "$DIR_CUSTOM/toplevel/beam.prg", "no folder token: $DIR_CUSTOM");
}

void testTargets(const Fixture& f)
{
  std::cout << "target text\n";
  const auto source = f.custom() / "macros/secframing/beam.prg";
  checkEq(show(defaultMoveTargetText(f.layout, source)), "$DIR_MACROS/secframing/beam.prg", "default target");
  const auto moved = resolveMoveTargetText(f.layout, source, "$DIR_COMPLIBS/tools/");
  check(moved.path && normalizeFsPath(moved.path->string())
        == normalizeFsPath((f.custom() / "complibs/tools/beam.prg").string()),
    "trailing slash keeps the name, Fallback token lands in Custom");
  check(!resolveMoveTargetText(f.layout, source, "$DIR_SYSTEM/macros/x.prg").path, "System target refused");
  check(!resolveMoveTargetText(f.layout, source, "$DIR_NOPE/x.prg").path, "unknown token refused");
  check(!resolveMoveTargetText(f.layout, source, "$DIR_MACROS/x.txt").path, "non-ADT extension refused");
  check(!resolveMoveTargetText(f.layout, source, "../outside/x.prg").path, "escape from Custom refused");
}

void testCommandMove(const Fixture& f)
{
  std::cout << "command move\n";
  const auto beam = f.custom() / "macros/secframing/beam.prg";
  const auto p = plan(f, MoveOrigin::Command, beam, f.custom() / "complibs/tools/beam2.prg");
  check(!p.refusal && !p.confirmation, "no refusal");
  checkEq(edited(p, f.custom() / "macros/caller.prg"),
    "x = call(\"$DIR_COMPLIBS/tools/beam2.prg\", 1)\n"
    "y = call(\"$DIR_COMPLIBS/tools/beam2.prg SETUP\")\n"
    "call \"$DIR_COMPLIBS/tools/beam2.prg\"\n"
    "n$ = \"beam.prg\"\n"
    ";! only a comment about beam.prg\n"
    "r = call(path$ & \"beam.prg\")\n"
    "z = call(\"$DIR_MACROS/secframing/sibling.prg\")\n",
    "caller rewritten, section suffix kept");
  checkEq(edited(p, f.custom() / "macros/secframing/sibling.prg"), "# include $DIR_COMPLIBS/tools/beam2.prg\n",
    "document-relative include");
  checkEq(edited(p, f.custom() / "macros/secframing/frag.p"),
    "##included_from $DIR_COMPLIBS/tools/beam2.prg\nx = 1\n", "##included_from");
  checkEq(edited(p, beam),
    "# include $DIR_MACROS/secframing/frag.p\n"
    "call(\"beam2.prg INNER\")\n"
    "#INNER#\n"
    "#END_INNER#\n",
    "moved file's own relative paths");
  check(p.updatedCount == 7, "updated count " + std::to_string(p.updatedCount));
  check(skippedWith(p, "System caller") == 1, "System caller skipped");
  check(skippedWith(p, "mention") == 2, "mention + dynamic path skipped, comment ignored");
  check(p.fileMoves.size() == 1, "one file move");

  const auto json = programMoveEditJson(p, true);
  const auto& changes = json.object().get("documentChanges").array();
  check(changes.back().object().get("kind").string() == "rename", "rename comes last");
  check(json.object().get("changeAnnotations").isObject(), "annotated for preview");
}

void testCommandRefusals(const Fixture& f)
{
  std::cout << "command refusals\n";
  const auto beam = f.custom() / "macros/secframing/beam.prg";
  check(plan(f, MoveOrigin::Command, beam, f.custom() / "macros/caller.prg").refusal.has_value(),
    "existing target refused");
  check(plan(f, MoveOrigin::Command, f.system() / "macros/syscaller.prg", f.custom() / "macros/x.prg")
          .refusal.has_value(),
    "System source refused");
  check(plan(f, MoveOrigin::Command, beam, f.custom() / "complibs/taken/x.prg").refusal.has_value(),
    "target shadowing System refused");
  const auto shadow = plan(f, MoveOrigin::Command, f.custom() / "macros/shared.prg", f.custom() / "macros/moved.prg");
  check(shadow.confirmation.has_value() && !shadow.refusal, "shadowing source asks first");
  const auto confirmed =
    plan(f, MoveOrigin::Command, f.custom() / "macros/shared.prg", f.custom() / "macros/moved.prg", true);
  check(!confirmed.confirmation && !confirmed.refusal && confirmed.fileMoves.size() == 1, "confirmed shadow plans");
  const auto caseOnly = plan(f, MoveOrigin::Command, beam, f.custom() / "macros/secframing/Beam.prg");
  check(!caseOnly.refusal, "case-only rename allowed");
  check(edited(caseOnly, f.custom() / "macros/caller.prg").starts_with("x = call(\"$DIR_MACROS/secframing/Beam.prg\""),
    "case-only rename updates callers");
}

void testWillRename(const Fixture& f)
{
  std::cout << "willRenameFiles\n";
  const auto sys = plan(f, MoveOrigin::WillRename, f.system() / "macros/syscaller.prg", f.system() / "macros/sc.prg");
  check(sys.edits.empty() && sys.warnings.size() == 1, "System move: warning, no edits");
  const auto out = plan(f, MoveOrigin::WillRename, f.custom() / "macros/secframing/beam.prg", f.root / "elsewhere/beam.prg");
  check(out.edits.empty() && out.warnings.size() == 1, "out of Custom: warning, no edits");
  const auto shadow = plan(f, MoveOrigin::WillRename, f.custom() / "macros/shared.prg", f.custom() / "macros/m.prg");
  check(!shadow.refusal && shadow.warnings.size() == 1, "shadowing source warns, not refuses");

  const auto folder = plan(f, MoveOrigin::WillRename, f.custom() / "macros/secframing", f.custom() / "macros/sf");
  check(folder.fileMoves.size() == 3, "folder expands to its ADT files");
  checkEq(edited(folder, f.custom() / "macros/caller.prg").substr(0, 41),
    "x = call(\"$DIR_MACROS/sf/beam.prg\", 1)\ny ", "callers follow the folder");
  checkEq(edited(folder, f.custom() / "macros/secframing/sibling.prg"), "# include beam.prg\n",
    "relative paths inside the folder untouched");
  const auto json = programMoveEditJson(folder, false);
  check(json.object().contains("changes") && !json.object().contains("documentChanges"), "plain changes for drags");
}

} // namespace

int main()
{
  const auto f = makeFixture();
  testScan();
  testFormat(f);
  testTargets(f);
  testCommandMove(f);
  testCommandRefusals(f);
  testWillRename(f);
  fs::remove_all(f.root);
  std::cout << (failures ? "FAILED: " : "all passed") << (failures ? std::to_string(failures) : "") << '\n';
  return failures ? 1 : 0;
}
