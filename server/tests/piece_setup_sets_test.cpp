// piece.setup set completion: vxsettings XML reader, settings conditions,
// Custom-over-System GUID overlay, set collection and the new_piece site.
// Build: cmake --build build --config Release --target piece-setup-sets-test
// Run:   build\Release\piece-setup-sets-test.exe
#include "piece_setup_sets.h"
#include "vxsettings_xml.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

const PieceSet* findSet(const std::vector<PieceSet>& sets, std::string_view name)
{
  for (const auto& set : sets)
  {
    if (set.name == name)
    {
      return &set;
    }
  }
  return nullptr;
}

std::string names(const std::vector<PieceSet>& sets)
{
  std::string out;
  for (const auto& set : sets)
  {
    out += (out.empty() ? "" : ",") + set.name;
  }
  return out;
}

std::string lines(const std::vector<PieceSetLine>& rows)
{
  std::string out;
  for (const auto& row : rows)
  {
    out += (out.empty() ? "" : ";") + row.text + (row.custom ? "*" : "");
  }
  return out;
}

void writeFile(const fs::path& path, std::string_view text)
{
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << text;
}

// Trimmed copy of the real system file: BOM, prolog, comments, entities, a
// commented-out DataItem, trailing-space Summary, set_begin fragment, a
// non-piece setup that also uses function_begin.
constexpr std::string_view kSystem =
  "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n"
  "<!-- header comment -->\r\n"
  "<VxSettings>\r\n"
  "  <SetupItem GUID=\"other\">\r\n"
  "    <Summary>wall.setup</Summary>\r\n"
  "    <SetupItem GUID=\"other-1\">\r\n"
  "      <Summary>wall.setup X</Summary>\r\n"
  "      <DataItem GUID=\"o1\" Application=\"BD\"><Key>function_begin</Key><Data>NOTAPIECE X</Data></DataItem>\r\n"
  "    </SetupItem>\r\n"
  "  </SetupItem>\r\n"
  "  <SetupItem GUID=\"ef49c5a8-dbd3-4733-9164-a8e358924b1d\">\r\n"
  "    <Summary>piece.setup</Summary>\r\n"
  "    <Description>General settings &lt;docs&gt;\r\nfunction_begin= NAME DESCR</Description>\r\n"
  "    <SetupItem GUID=\"s-common\">\r\n"
  "      <Summary>piece.setup COMMON</Summary>\r\n"
  "      <DataItem GUID=\"c1\" Config=\"not Finland\" Application=\"BD\"><Key>function_begin</Key><Data>COMMON COMMON</Data></DataItem>\r\n"
  "      <DataItem GUID=\"c2\" Config=\"Finland\" Application=\"BD\"><Key>function_begin</Key><Data>COMMON YLEINEN</Data></DataItem>\r\n"
  "      <DataItem GUID=\"c3\" Config=\"not USA and not Steel-USA\" Application=\"BD\"><Key>include_set</Key><Data>DEFSECT|PCETYPES</Data></DataItem>\r\n"
  "      <DataItem GUID=\"c4\" Config=\"USA or Steel-USA\" Application=\"BD\" Optbit=\"not 18\"><Key>include_set</Key><Data>USONLY</Data></DataItem>\r\n"
  "      <DataItem GUID=\"c5\" Application=\"BD\"><Key>param</Key><Data>LIBRARY pipe_electrical</Data></DataItem>\r\n"
  "      <DataItem GUID=\"c6\" Application=\"BD or BD-View and not InD\" Units=\"metric\"><Key>param</Key><Data>SECTION 45x95</Data></DataItem>\r\n"
  "      <DataItem GUID=\"c7\" Application=\"BD\" Units=\"imperial\"><Key>param</Key><Data>SECTION 2x4</Data></DataItem>\r\n"
  "      <DataItem GUID=\"c8\" Application=\"G4\"><Key>param</Key><Data>G4 ONLY</Data></DataItem>\r\n"
  "      <DataItem GUID=\"c9\" Application=\"BD\" Language=\"not Finnish\"><Key>param</Key><Data>IF #A# &gt; 0 &amp;&amp; B</Data></DataItem>\r\n"
  "      <DataItem GUID=\"c10\" Application=\"BD\"><Key>function_end</Key><Data /></DataItem>\r\n"
  "    </SetupItem>\r\n"
  "    <SetupItem GUID=\"s-molding\">\r\n"
  "      <Summary>piece.setup MOLDING </Summary>\r\n"
  "      <DataItem GUID=\"m1\" Config=\"not Finland\" Application=\"BD\"><Key>function_begin</Key><Data>MOLDING MOLDING</Data></DataItem>\r\n"
  "      <DataItem GUID=\"m2\" Config=\"Finland\" Application=\"BD\"><Key>function_begin</Key><Data>MOLDING LISTA</Data></DataItem>\r\n"
  "      <!--<DataItem GUID=\"m3\" Config=\"Finland\" Application=\"BD\">\r\n"
  "        <Key>param</Key><Data>USE LISTA</Data></DataItem>-->\r\n"
  "      <DataItem GUID=\"m4\" Application=\"BD\"><Key>param</Key><Data>PCETYPE TRIMMER</Data></DataItem>\r\n"
  "      <DataItem GUID=\"m5\" Application=\"BD\"><Key>function_end</Key><Data /></DataItem>\r\n"
  "    </SetupItem>\r\n"
  "    <SetupItem GUID=\"s-framing\">\r\n"
  "      <Summary>piece.setup FRAMING.WALL</Summary>\r\n"
  "      <DataItem GUID=\"f1\" Application=\"\"><Key>function_begin</Key><Data>FRAMING.WALL \"Wall framing\"</Data></DataItem>\r\n"
  "      <DataItem GUID=\"f2\" Application=\"BD  or BD-View \"><Key>param</Key><Data><![CDATA[DESCR <raw>]]></Data></DataItem>\r\n"
  "      <DataItem GUID=\"f3\" Application=\"BD\"><Key>function_end</Key><Data /></DataItem>\r\n"
  "    </SetupItem>\r\n"
  "    <SetupItem GUID=\"s-frag\">\r\n"
  "      <Summary>piece.setup DEFSECT</Summary>\r\n"
  "      <DataItem GUID=\"d1\" Application=\"BD\"><Key>set_begin</Key><Data>DEFSECT</Data></DataItem>\r\n"
  "      <DataItem GUID=\"d2\" Application=\"BD\"><Key>param</Key><Data>SECTION 50x100</Data></DataItem>\r\n"
  "      <DataItem GUID=\"d3\" Application=\"BD\"><Key>set_end</Key><Data /></DataItem>\r\n"
  "    </SetupItem>\r\n"
  "    <SetupItem GUID=\"s-dup\">\r\n"
  "      <Summary>piece.setup common lower</Summary>\r\n"
  "      <DataItem GUID=\"u1\" Application=\"BD\"><Key>function_begin</Key><Data>common lower</Data></DataItem>\r\n"
  "      <DataItem GUID=\"u2\" Application=\"BD\"><Key>function_end</Key><Data /></DataItem>\r\n"
  "    </SetupItem>\r\n"
  "    <SetupItem GUID=\"s-dup2\">\r\n"
  "      <Summary>piece.setup COMMON again</Summary>\r\n"
  "      <DataItem GUID=\"v1\" Application=\"BD\"><Key>function_begin</Key><Data>COMMON SECOND</Data></DataItem>\r\n"
  "      <DataItem GUID=\"v2\" Application=\"BD\"><Key>function_end</Key><Data /></DataItem>\r\n"
  "    </SetupItem>\r\n"
  "  </SetupItem>\r\n"
  "</VxSettings>\r\n";

// Shape of the real Custom overlay: parent matched by GUID without Summary,
// REPLACE DataItems in an existing set, an ADD set, an ADD DataItem.
constexpr std::string_view kCustom =
  "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
  "<VxSettings>\n"
  "  <SetupItem GUID=\"4c7c8970\">\n"
  "    <SetupItem GUID=\"x\"><DataItem GUID=\"r\" Custom=\"REPLACE\"><Key>recent_projects</Key><Data>20</Data></DataItem></SetupItem>\n"
  "  </SetupItem>\n"
  "  <SetupItem GUID=\"EF49C5A8-dbd3-4733-9164-a8e358924b1d\">\n"
  "    <SetupItem GUID=\"s-common\">\n"
  "      <DataItem GUID=\"c5\" Application=\"BD\" Custom=\"REPLACE\"><Key>param</Key><Data>LIBRARY Pipe_elec</Data><OriginalData>LIBRARY pipe_electrical</OriginalData></DataItem>\n"
  "      <DataItem GUID=\"c11\" Application=\"BD\" Custom=\"ADD\"><Key>param</Key><Data>USE PANELBRÄDA</Data></DataItem>\n"
  "    </SetupItem>\n"
  "    <SetupItem GUID=\"s-molding\">\n"
  "      <DataItem GUID=\"m1\" Config=\"not Finland\" Application=\"BD\" Custom=\"FUTURE\"><Key>function_begin</Key><Data>TRIMLIST MOLDING</Data></DataItem>\n"
  "    </SetupItem>\n"
  "    <SetupItem GUID=\"s-true\" Custom=\"ADD\">\n"
  "      <Summary>piece.setup TRUEFEATS</Summary>\n"
  "      <DataItem GUID=\"t1\" Application=\"BD\" Custom=\"ADD\"><Key>function_begin</Key><Data>TRUEFEATS TRUEFEATS</Data></DataItem>\n"
  "      <DataItem GUID=\"t2\" Application=\"BD\" Custom=\"ADD\"><Key>param</Key><Data>APP.3D TRUE_FEATURES</Data></DataItem>\n"
  "      <DataItem GUID=\"t3\" Application=\"BD\" Custom=\"ADD\"><Key>function_end</Key><Data /></DataItem>\n"
  "    </SetupItem>\n"
  "  </SetupItem>\n"
  "</VxSettings>\n";

constexpr std::string_view kConfig =
  "<VxSettingsConfig>\n<Application>BD</Application>\n<Config>Sweden</Config>\n"
  "<Units>metric</Units>\n<Language>English</Language>\n</VxSettingsConfig>\n";

void testXml()
{
  std::cout << "xml reader\n";
  const auto root = parseXml(kSystem);
  check(root.has_value(), "system fixture parses");
  if (!root)
  {
    return;
  }
  checkEq(root->name, "VxSettings", "root element");
  check(root->children.size() == 2, "two top-level SetupItems (comments skipped)");
  const auto& piece = root->children[1];
  checkEq(std::string(piece.attribute("GUID").value_or("")), "ef49c5a8-dbd3-4733-9164-a8e358924b1d", "attribute");
  const auto* desc = piece.child("Description");
  check(desc && desc->text.find("<docs>") != std::string::npos, "entities decoded in text");
  const auto* framing = &piece.children[4];
  const auto* cdata = framing->children[2].child("Data");
  checkEq(cdata ? cdata->text : "", "DESCR <raw>", "CDATA kept raw");
  check(!parseXml("<a><b></a>").has_value(), "mismatched close fails");
  check(!parseXml("").has_value(), "empty input fails");
  const auto numeric = parseXml("<a>&#65;&#x42;&apos;&quot;</a>");
  checkEq(numeric ? numeric->text : "", "AB'\"", "numeric and named entities");
}

void testConditions()
{
  std::cout << "settings conditions\n";
  check(settingsConditionMatches("", "Sweden"), "empty expression matches");
  check(settingsConditionMatches("   ", "Sweden"), "blank expression matches");
  check(settingsConditionMatches("Sweden", "sweden"), "name, case-insensitive");
  check(!settingsConditionMatches("Finland", "Sweden"), "other name");
  check(settingsConditionMatches("not Finland", "Sweden"), "not other");
  check(!settingsConditionMatches("not Sweden", "Sweden"), "not self");
  check(settingsConditionMatches("USA or Steel-USA or Sweden", "Sweden"), "or chain, dashed names");
  check(!settingsConditionMatches("not Norway and not Finland and not Sweden", "Sweden"), "and-not chain excludes");
  check(settingsConditionMatches("not Norway and not Finland and not USA", "Sweden"), "and-not chain includes");
  check(settingsConditionMatches("BD or BD-View and not InD", "BD"), "mixed: BD");
  check(settingsConditionMatches("BD or BD-View and not InD", "BD-View"), "mixed: BD-View");
  check(!settingsConditionMatches("BD or BD-View and not InD", "InD"), "mixed: InD");
  check(!settingsConditionMatches("BD or BD-View and not InD", "G4"), "mixed: G4");
  check(settingsConditionMatches("not ED-Lite and ED or HD", "HD"), "mixed: HD");
  check(settingsConditionMatches("not ED-Lite and ED or HD", "ED"), "mixed: ED");
  check(!settingsConditionMatches("not ED-Lite and ED or HD", "ED-Lite"), "mixed: ED-Lite");
  // Left to right: (A or B) and C, not A or (B and C).
  check(!settingsConditionMatches("Sweden or Norway and Finland", "Sweden"), "strict left-to-right fold");
  check(settingsConditionMatches("BD  or BD-View ", "BD-View"), "extra spaces");
  check(settingsConditionMatches("not not Sweden", "Sweden"), "double not");
  check(settingsConditionMatches("Sweden and", "Norway"), "dangling operator: unparsable matches");
  check(settingsConditionMatches("or Sweden", "Norway"), "leading operator: unparsable matches");
  check(settingsConditionMatches("Finland", ""), "missing active value matches");
}

void testConfig()
{
  std::cout << "vxsettings_config\n";
  const auto config = parseVxSettingsConfig(kConfig);
  checkEq(config.application, "BD", "Application");
  checkEq(config.config, "Sweden", "Config");
  checkEq(config.units, "metric", "Units");
  checkEq(config.language, "English", "Language");
  const auto broken = parseVxSettingsConfig("not xml");
  check(broken.config.empty() && broken.application.empty(), "unparsable config is empty");
}

void testCollect()
{
  std::cout << "collect sets\n";
  const auto config = parseVxSettingsConfig(kConfig);
  const auto sweden = collectPieceSets(kSystem, "", config);
  checkEq(names(sweden), "COMMON,MOLDING,FRAMING.WALL,common", "system sets: piece.setup only, no set_begin, case-sensitive dedupe keeps first");
  if (const auto* common = findSet(sweden, "COMMON"))
  {
    checkEq(common->description, "COMMON", "Sweden picks not-Finland function_begin");
    checkEq(lines(common->includeSets), "DEFSECT|PCETYPES", "active include_set only");
    checkEq(lines(common->params), "LIBRARY pipe_electrical;SECTION 45x95;IF #A# > 0 && B", "params filtered by Application, Units, Language");
    check(!common->custom, "system set not custom");
  }
  else
  {
    check(false, "COMMON present");
  }
  if (const auto* framing = findSet(sweden, "FRAMING.WALL"))
  {
    checkEq(framing->description, "Wall framing", "quoted description: surrounding quotes dropped");
    checkEq(lines(framing->params), "DESCR <raw>", "empty Application matches");
  }
  else
  {
    check(false, "FRAMING.WALL present (dotted name read whole)");
  }

  auto finnish = config;
  finnish.config = "Finland";
  finnish.language = "Finnish";
  const auto finland = collectPieceSets(kSystem, "", finnish);
  const auto* molding = findSet(finland, "MOLDING");
  checkEq(molding ? molding->description : "", "LISTA", "Finland picks Finland function_begin");
  checkEq(molding ? lines(molding->params) : "", "PCETYPE TRIMMER", "commented-out DataItem ignored");
  const auto* common = findSet(finland, "COMMON");
  checkEq(common ? lines(common->params) : "", "LIBRARY pipe_electrical;SECTION 45x95", "not Finnish excluded");

  auto blank = VxSettingsConfig{};
  const auto open = collectPieceSets(kSystem, "", blank);
  const auto* openCommon = findSet(open, "COMMON");
  checkEq(openCommon ? openCommon->description : "", "COMMON", "missing config: first function_begin wins");
  checkEq(openCommon ? lines(openCommon->params) : "",
    "LIBRARY pipe_electrical;SECTION 45x95;SECTION 2x4;G4 ONLY;IF #A# > 0 && B",
    "missing config: every condition matches");

  check(collectPieceSets("", "", config).empty(), "no files: no sets");
  check(collectPieceSets("<broken", "", config).empty(), "unparsable system: no sets");
}

void testOverlay()
{
  std::cout << "custom overlay\n";
  const auto config = parseVxSettingsConfig(kConfig);
  const auto sets = collectPieceSets(kSystem, kCustom, config);
  checkEq(names(sets), "COMMON,TRIMLIST,FRAMING.WALL,common,TRUEFEATS", "overlay: replaced name, added set last");
  if (const auto* common = findSet(sets, "COMMON"))
  {
    checkEq(lines(common->params),
      "LIBRARY Pipe_elec*;SECTION 45x95;IF #A# > 0 && B;USE PANELBRÄDA*",
      "REPLACE in place by GUID, ADD inserted before the item's function_end");
    check(!common->custom, "merged set is not a custom set");
  }
  else
  {
    check(false, "COMMON present");
  }
  const auto* trim = findSet(sets, "TRIMLIST");
  check(trim && trim->nameCustom && !trim->custom, "unknown Custom value replaces by GUID");
  const auto* trueFeats = findSet(sets, "TRUEFEATS");
  check(trueFeats && trueFeats->custom, "ADD set is custom");
  checkEq(trueFeats ? lines(trueFeats->params) : "", "APP.3D TRUE_FEATURES*", "ADD set lines custom");

  const auto customOnly = collectPieceSets("", kCustom, config);
  check(customOnly.empty(), "custom without system: no piece.setup parent, no sets");
}

void testSite()
{
  std::cout << "new_piece site\n";
  const std::u16string line = u"newpce = new_piece(SP,5,EP,5,SelemZ,\"FRAMING.WALL\",\"WOOD\",code$)";
  const auto open = line.find(u"\"FRAMING");
  auto site = analyzePieceSetSite(line, open + 1);
  check(site.has_value(), "cursor right after opening quote");
  if (site)
  {
    check(site->replaceStart == open + 1 && site->replaceEnd == open + 13, "replaces quoted content");
    checkEq(site->content, "FRAMING.WALL", "content");
  }
  check(analyzePieceSetSite(line, open + 5).has_value(), "cursor mid-string");
  check(analyzePieceSetSite(line, open + 13).has_value(), "cursor before closing quote");
  check(!analyzePieceSetSite(line, open + 14).has_value(), "after closing quote");
  check(!analyzePieceSetSite(line, open).has_value(), "before opening quote");
  check(!analyzePieceSetSite(line, line.find(u"\"WOOD") + 1).has_value(), "7th argument");
  check(!analyzePieceSetSite(line, line.find(u"SelemZ") + 2).has_value(), "5th argument");

  const std::u16string unclosed = u"p = new_piece(a,b,c,d,e,\"FRA";
  auto partial = analyzePieceSetSite(unclosed, unclosed.size());
  check(partial && partial->replaceEnd == unclosed.size(), "unclosed string runs to cursor");

  const std::u16string empty = u"p = NEW_PIECE( a, b, c, d, e, \"\" )";
  check(analyzePieceSetSite(empty, empty.find(u"\"\"") + 1).has_value(), "empty quotes, case-insensitive command");

  const std::u16string bare = u"p = new_piece(a,b,c,d,e, )";
  const auto bareSite = analyzePieceSetSite(bare, bare.find(u", )") + 2);
  check(bareSite && !bareSite->insideQuotes && !bareSite->leadingSpace
      && bareSite->replaceStart == bareSite->replaceEnd && bareSite->content.empty(),
    "empty slot: bare site, insert at cursor");

  const std::u16string open6 = u"p = new_piece(a,b,c,d,e,";
  check(analyzePieceSetSite(open6, open6.size()).has_value(), "empty slot at end of unclosed call");

  const std::u16string typed = u"p = new_piece(a,b,c,d,e,FRA";
  check(!analyzePieceSetSite(typed, typed.size()).has_value(), "bare typed word at cursor: no site");
  const auto typedSite = analyzePieceSetSite(typed, typed.find(u"FRA"));
  check(typedSite && !typedSite->insideQuotes, "bare typed word: site at its start");

  const std::u16string expr = u"p = new_piece(a,b,c,d,e,Set$ & X";
  check(!analyzePieceSetSite(expr, expr.size()).has_value()
      && !analyzePieceSetSite(expr, expr.find(u"X")).has_value(), "other text in slot: no site");

  const std::u16string nested = u"p = new_piece(f(a,b),b,c,d,e,\"X\")";
  check(analyzePieceSetSite(nested, nested.find(u"\"X") + 1).has_value(), "nested parens do not split");

  const std::u16string other = u"p = new_piece2(a,b,c,d,e,\"X\")";
  check(!analyzePieceSetSite(other, other.find(u"\"X") + 1).has_value(), "new_piece2 is not new_piece");

  const std::u16string prefixed = u"p = my_new_piece(a,b,c,d,e,\"X\")";
  check(!analyzePieceSetSite(prefixed, prefixed.find(u"\"X") + 1).has_value(), "word boundary");

  const std::u16string commented = u"! p = new_piece(a,b,c,d,e,\"X\")";
  check(!analyzePieceSetSite(commented, commented.find(u"\"X") + 1).has_value(), "in comment");
}

void testSetPieceSetSite()
{
  std::cout << "set_piece_set site\n";
  const std::u16string line = u"set_piece_set(newpce) \"TRUEFEATS\",2";
  const auto open = line.find(u"\"TRUE");
  auto site = analyzePieceSetSite(line, open + 3);
  check(site && site->command == PieceSetCommand::SetPieceSet && site->insideQuotes, "inside quotes");
  if (site)
  {
    checkEq(site->content, "TRUEFEATS", "content");
    check(site->replaceStart == open + 1 && site->replaceEnd == open + 10, "replaces quoted content");
    check(site->mode == 2, "literal mode");
  }
  check(!analyzePieceSetSite(line, line.find(u"newpce") + 2).has_value(), "inside parens: no site");
  check(!analyzePieceSetSite(line, line.size()).has_value(), "mode argument: no site");

  const auto modeOf = [](std::u16string_view text) {
    const auto at = text.find(u'"') + 1;
    const auto found = analyzePieceSetSite(text, at);
    return found ? found->mode : std::optional<long long>{-99};
  };
  check(modeOf(u"set_piece_set(p) \"X\",1+2") == 3, "sum mode");
  check(modeOf(u"set_piece_set(p) \"X\", 1 + 4 ") == 5, "sum mode with spaces");
  check(modeOf(u"set_piece_set(p) \"X\"") == std::nullopt, "missing mode");
  check(modeOf(u"set_piece_set(p) \"X\",Mode") == std::nullopt, "variable mode");
  check(modeOf(u"set_piece_set(p) \"X\",1+M") == std::nullopt, "partly variable mode");
  check(modeOf(u"set_piece_set(p) \"X\",2 ! comment") == 2, "mode before comment");

  const std::u16string glued = u"set_piece_set(p)";
  const auto gluedSite = analyzePieceSetSite(glued, glued.size());
  check(gluedSite && !gluedSite->insideQuotes && gluedSite->leadingSpace, "right after ): leading space");
  const std::u16string spaced = u"set_piece_set(p) ";
  const auto spacedSite = analyzePieceSetSite(spaced, spaced.size());
  check(spacedSite && !spacedSite->leadingSpace, "after ) and space: no leading space");
  const std::u16string typed = u"set_piece_set(p) NEON";
  const auto typedSite = analyzePieceSetSite(typed, typed.find(u"NEON"));
  check(typedSite && !typedSite->insideQuotes && !typedSite->leadingSpace, "bare typed word: site at its start");
  const std::u16string nested = u"SET_PIECE_SET(f(a,b)) \"X\",2";
  check(analyzePieceSetSite(nested, nested.find(u"\"X") + 1).has_value(), "nested parens, case-insensitive");
  const std::u16string unclosed = u"set_piece_set(p \"X\"";
  check(!analyzePieceSetSite(unclosed, unclosed.find(u"\"X") + 1).has_value(), "no closing paren: no site");
}

void testFindForSite()
{
  std::cout << "find set for site\n";
  std::vector<PieceSet> sets(3);
  sets[0].name = "FRAMING.WALL";
  sets[0].description = "WALL FRAMING";
  sets[1].name = "NEONGREEN";
  sets[1].description = "NEONGREEN";
  sets[2].name = "WALL FRAMING";
  sets[2].description = "OTHER";
  const auto find = [&](PieceSetCommand command, std::string content, std::optional<long long> mode) {
    PieceSetSite site{.command = command, .insideQuotes = true, .content = std::move(content), .mode = mode};
    const auto* set = findPieceSetForSite(site, sets);
    return set ? set->name : std::string("<none>");
  };
  checkEq(find(PieceSetCommand::NewPiece, "WALL FRAMING", 2), "WALL FRAMING", "new_piece: NAME only, mode ignored");
  checkEq(find(PieceSetCommand::NewPiece, "NEONGREEN", std::nullopt), "NEONGREEN", "new_piece: NAME");
  checkEq(find(PieceSetCommand::SetPieceSet, "WALL FRAMING", 2), "FRAMING.WALL", "mode 2: DESCR");
  checkEq(find(PieceSetCommand::SetPieceSet, "FRAMING.WALL", 2), "<none>", "mode 2: NAME does not match");
  checkEq(find(PieceSetCommand::SetPieceSet, "FRAMING.WALL", 3), "FRAMING.WALL", "mode 1+2: NAME");
  checkEq(find(PieceSetCommand::SetPieceSet, "WALL FRAMING", 3), "WALL FRAMING", "mode 1+2: NAME, not DESCR");
  checkEq(find(PieceSetCommand::SetPieceSet, "WALL FRAMING", std::nullopt), "WALL FRAMING", "unknown mode: NAME first");
  checkEq(find(PieceSetCommand::SetPieceSet, "OTHER", std::nullopt), "WALL FRAMING", "unknown mode: then DESCR");
  PieceSetSite bare{.command = PieceSetCommand::SetPieceSet, .insideQuotes = false};
  check(findPieceSetForSite(bare, sets) == nullptr, "bare site: none");
}

void testItemsAndMarkdown()
{
  std::cout << "items and markdown\n";
  const auto config = parseVxSettingsConfig(kConfig);
  const auto sets = collectPieceSets(kSystem, kCustom, config);
  const PieceSetSite site{.insideQuotes = true, .replaceStart = 10, .replaceEnd = 12, .content = "FR"};
  const auto items = pieceSetCompletionItems(site, lsp::Position{.line = 3, .character = 11}, sets);
  check(items.size() == sets.size(), "one item per set");
  if (!items.empty())
  {
    checkEq(items[0].label, "COMMON", "label is NAME");
    checkEq(items[0].detail.value_or(""), "COMMON", "detail is NAME");
    check(items[0].textEdit.has_value(), "has text edit");
  }
  const auto newText = [](const lsp::CompletionItem& item) {
    return item.textEdit ? std::get<lsp::TextEdit>(*item.textEdit).newText : std::string{};
  };
  if (!items.empty())
  {
    checkEq(newText(items[0]), "COMMON", "inside quotes: NAME");
  }
  const PieceSetSite bare{.insideQuotes = false, .replaceStart = 5, .replaceEnd = 5};
  const auto bareItems = pieceSetCompletionItems(bare, lsp::Position{.line = 0, .character = 5}, sets);
  checkEq(bareItems.empty() ? "" : newText(bareItems[0]), "\"COMMON\"", "empty slot: quoted NAME");
  const PieceSetSite glued{.insideQuotes = false, .leadingSpace = true, .replaceStart = 5, .replaceEnd = 5};
  const auto gluedItems = pieceSetCompletionItems(glued, lsp::Position{.line = 0, .character = 5}, sets);
  checkEq(gluedItems.empty() ? "" : newText(gluedItems[0]), " \"COMMON\"", "after ): leading space");
  checkEq(gluedItems.empty() ? "" : gluedItems[0].label, "COMMON", "label stays NAME");
  const auto* common = findSet(sets, "COMMON");
  const auto markdown = common ? pieceSetMarkdown(*common) : std::string{};
  check(markdown.find("Description: COMMON") != std::string::npos, "markdown description");
  check(markdown.find("DEFSECT|PCETYPES") != std::string::npos, "markdown include_set");
  check(markdown.find("LIBRARY Pipe_elec") != std::string::npos && markdown.find("(custom)") != std::string::npos,
    "markdown marks custom lines");
  const auto* trueFeats = findSet(sets, "TRUEFEATS");
  const auto added = trueFeats ? pieceSetMarkdown(*trueFeats) : std::string{};
  check(added.find("custom") != std::string::npos, "markdown tags custom set");
}

void testLoadFromDisk()
{
  std::cout << "load from layout\n";
  const auto root = fs::temp_directory_path() / "vertex-adt-piece-sets-test";
  fs::remove_all(root);
  writeFile(root / "system/programs/vxsettings.xml", kSystem);
  writeFile(root / "system/programs/vxsettings_config.xml", kConfig);
  writeFile(root / "customa/programs/vxsettings.xml", kCustom);
  fs::create_directories(root / "customa/macros");
  LibraryLayout layout;
  layout.setFolders({lsp::WorkspaceFolder{.uri = uriFromPath(root), .name = "lib"}}, std::nullopt);
  const auto sources = resolvePieceSetSources(layout);
  check(sources.custom.filename() == "vxsettings.xml" && sources.custom.parent_path().parent_path().filename() == "customa",
    "custom programs resolved");
  check(sources.config.filename() == "vxsettings_config.xml", "config resolved");
  const auto first = loadPieceSets(sources);
  checkEq(first ? names(*first) : "", "COMMON,TRIMLIST,FRAMING.WALL,common,TRUEFEATS", "loaded");
  const auto again = loadPieceSets(sources);
  check(first && again && first.get() == again.get(), "unchanged files hit the cache");
  writeFile(root / "system/programs/vxsettings_config.xml",
    "<VxSettingsConfig><Application>BD</Application><Config>Finland</Config></VxSettingsConfig>");
  const auto changed = loadPieceSets(sources);
  const auto* molding = changed ? findSet(*changed, "MOLDING") : nullptr;
  checkEq(molding ? molding->description : "", "LISTA", "config change invalidates (size differs)");
  fs::remove(root / "system/programs/vxsettings_config.xml");
  const auto noConfig = loadPieceSets(resolvePieceSetSources(layout));
  check(noConfig && !noConfig->empty(), "missing config still loads");
  fs::remove_all(root);
}

// Optional smoke run on a real install: set VXBD_ROOT=C:\vxbd320.
void smokeRealInstall()
{
  const char* rootEnv = std::getenv("VXBD_ROOT");
  if (!rootEnv || !*rootEnv)
  {
    return;
  }
  std::cout << "real install " << rootEnv << '\n';
  LibraryLayout layout;
  layout.setFolders({lsp::WorkspaceFolder{.uri = uriFromPath(fs::path(rootEnv)), .name = "lib"}}, std::nullopt);
  const auto sources = resolvePieceSetSources(layout);
  std::cout << "  system " << sources.system.string() << "\n  custom " << sources.custom.string()
            << "\n  config " << sources.config.string() << '\n';
  const auto started = std::chrono::steady_clock::now();
  const auto sets = loadPieceSets(sources);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - started);
  std::cout << "  " << sets->size() << " sets in " << elapsed.count() << " ms\n  " << names(*sets) << '\n';
  if (const auto* framing = findSet(*sets, "FRAMING.WALL"))
  {
    std::cout << pieceSetMarkdown(*framing) << '\n';
  }
}

} // namespace

int main()
{
  smokeRealInstall();
  testXml();
  testConditions();
  testConfig();
  testCollect();
  testOverlay();
  testSite();
  testSetPieceSetSite();
  testFindForSite();
  testItemsAndMarkdown();
  testLoadFromDisk();
  std::cout << (failures ? "FAILED: " : "all passed") << (failures ? std::to_string(failures) : "") << '\n';
  return failures ? 1 : 0;
}
