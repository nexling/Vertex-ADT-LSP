// Point, edge, note, map and system command entries
// (docs/adr/0023-note-map-system-command-entries.md): get_pnt_param,
// set_piece_inst, set_edge_shape, get_edge_dir, proj_vxgeom_dir, notewinopen,
// notewinclose, note, dwgsetlevels, function, folder_files, map_set, system,
// bgsystem, run_open, get_actual_piece, randomnumber.
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
  "edge = get($CALLPARAM1)\n"
  "H = get($CALLPARAM2)\n"
  "SP = get($CALLPARAM3)\n"
  "MP = get($CALLPARAM3)\n"
  "Dir = get($CALLPARAM3)\n"
  "GeomProj = get($CALLPARAM3)\n"
  "G = get($CALLPARAM3)\n"
  "P0 = get($CALLPARAM3)\n"
  "D0 = get($CALLPARAM3)\n"
  "num = randomnumber(1,2)\n"
  "ok = 0\n"
  "Ok = 0\n"
  "Mode = 0\n"
  "OGU_MIN = 0\n"
  "mac = 0\n"
  "pce = 0\n"
  "Geom2 = 0\n"
  "GeomEdge = 0\n"
  "AP = 0\n"
  "AZ = 0\n"
  "End = 0\n"
  "Jnt = 0\n"
  "actprojpath$ = \"p\"\n"
  "filename$ = \"f\"\n"
  "LevelGroup$ = \"l\"\n"
  "Command$ = \"c\"\n"
  "fullpathtest$ = \"t\"\n"
  "Path$ = \"p\"\n"
  "Filter$ = \"*\"\n"
  "Map$ = \"m\"\n";
constexpr int kFirstBodyLine = 30;

// Every diagnostic message on the lines of `body` (after the prelude), joined with " | ".
std::string messages(const std::string& body)
{
  const std::string text = kPrelude + body + "\n";
  ValidateContext ctx;
  ctx.settings.strictModeEnabled = strict;
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
      "get_pnt_param(SP) \"SP\",mac,1\n"
      "set_piece_inst(pce) \"string\",\"string\"\n"
      "ok = set_edge_shape(edge,Geom2,\"PERP\",0)\n"
      "get_edge_dir(edge) MP,Dir\n"
      "proj_vxgeom_dir(GeomProj) GeomEdge,AP,AZ,AZ\n"
      "notewinopen(1) \"KONVERTERAR PLAN(du kan avsluta med ESC)\",8,60\n"
      "numfiles = folder_files(actprojpath$)\n"
      "note(1) \"Fixar: \"&filename$\n"
      "dwgsetlevels \"level_group=\"\"\"&LevelGroup$&\"\"\"\"\n"
      "function DELALLREFDWGS\n"
      "ok = map_set(\"FILENAMES\",filename$,\"1\")\n"
      "Ok = system(Command$)\n"
      "notewinclose(1)\n"
      "ok = bgsystem(Command$)\n"
      "ok = run_open(fullpathtest$)\n"
      "Pc= get_actual_piece(End)\n"
      "waitperiod = randomnumber(500,1500)"),
    "", "whole block");

  std::cout << "corpus forms stay clean (C:\\vxbd320)\n";
  checkEq(messages("get_pnt_param(SP)\"START\",mac"), "", "get_pnt_param without mode or space");
  checkEq(messages("get_pnt_param(SP) \"P\"&filename$,mac,1"), "", "get_pnt_param concatenated name");
  checkEq(messages("ok =set_edge_shape(edge,Geom2,\"PERP\")"), "", "set_edge_shape without angle");
  checkEq(messages("Ok = folder_files(Path$,Filter$,Map$,Mode)"), "", "folder_files with four parameters");
  checkEq(messages("ok = map_set(Map$,pce,\"1\")"), "", "map_set object key");
  checkEq(messages("ok = map_set(Map$,\"OGU_MIN\",OGU_MIN)"), "", "map_set variable value");
  checkEq(messages("Ok = system(Command$,2)"), "", "system with a second parameter");
  checkEq(messages("Ok=system(\"copy \"&Path$&\" \"&filename$)"), "", "system concatenation");
  checkEq(messages("note(1) \"Nr \"&Mode,3"), "", "note with a number");
  checkEq(messages("ok = ok+1;note(1) \"Nr \"&ok,2"), "", "note as the second statement");
  checkEq(messages("if ok == 0 then note(0) filename$&\"x\",1"), "", "note after then");
  checkEq(messages("note filename$,2"), "", "note without an index is not validated");
  checkEq(messages("note = 1"), "", "variable named note");
  checkEq(messages("dwgsetlevels filename$"), "", "dwgsetlevels variable");
  checkEq(messages("function \"PIIRTO 12\""), "", "function with a quoted name");
  checkEq(messages("set_djmember(Jnt) get_actual_piece(End),\"PCE\""), "", "nested get_actual_piece");

  checkEq(messages("Function$ = get(\"$CALLPARAM1\")"), "", "string variable named like a command");
  checkEq(messages("ok = map_set(Map$,\"SG2BRIX\",\"Index (Brix) = Factor (CF) x P\")"), "",
    "`) =` inside a string (dbdemo03.prg)");
  std::cout << "invalid uses are flagged\n";
  const auto flagged = [](const std::string& line, const std::string& want) {
    checkEq(messages(line), want, line);
  };
  flagged("ok = get_pnt_param(SP) \"SP\",mac,1", "Command 'get_pnt_param' should not have an assignment variable");
  flagged("get_pnt_param(SP) mac,1,1", "Name parameter after parentheses must be a valid string in get_pnt_param");
  flagged("get_pnt_param(SP) \"SP\"", "get_pnt_param requires at least 2 parameters after parentheses");
  flagged("set_piece_inst(pce) \"LAYER\",179",
    "Value parameter after parentheses must be a valid string in set_piece_inst");
  flagged("set_piece_inst(pce) \"LAYER\"", "set_piece_inst requires at least 2 parameters after parentheses");
  flagged("ok = set_edge_shape(edge,Geom2,0)", "Mode parameter must be a valid string in set_edge_shape");
  flagged("set_edge_shape(edge,Geom2,\"PERP\",0)", "set_edge_shape requires an assignment variable");
  flagged("get_edge_dir(edge) MP", "get_edge_dir requires at least 2 parameters after parentheses");
  flagged("proj_vxgeom_dir(GeomProj) GeomEdge,AP,AZ",
    "proj_vxgeom_dir requires at least 4 parameters after parentheses");
  flagged("notewinopen(1) 8,60", "notewinopen requires at least 3 parameters after parentheses");
  flagged("notewinopen(1) 5,8,60", "Header parameter after parentheses must be a valid string in notewinopen");
  flagged("notewinclose()", "notewinclose requires at least 1 parameters inside parentheses");
  flagged("note(1)", "note requires at least 1 parameters after parentheses");
  flagged("note(1) 5", "Message parameter after parentheses must be a valid string in note");
  flagged("dwgsetlevels 5", "Settings parameter must be a valid string in dwgsetlevels");
  flagged("function", "function requires at least 1 parameters after parentheses");
  flagged("function ONE,TWO", "function accepts at most 1 parameters after parentheses");
  flagged("folder_files(Path$)", "folder_files requires an assignment variable");
  flagged("ok = folder_files(5)", "Path parameter must be a valid string in folder_files");
  flagged("ok = map_set(pce,\"a\",\"b\")", "Map parameter must be a valid string in map_set");
  flagged("ok = map_set(Map$,\"a\")", "map_set requires at least 3 parameters inside parentheses");
  flagged("system(Command$)", "system requires an assignment variable");
  flagged("ok = system(5)", "Command parameter must be a valid string in system");
  flagged("bgsystem(Command$)", "bgsystem requires an assignment variable");
  flagged("ok = bgsystem(Command$,1)", "bgsystem accepts at most 1 parameters inside parentheses");
  flagged("run_open(Path$)", "run_open requires an assignment variable");
  flagged("ok = run_open(5)", "File parameter must be a valid string in run_open");
  flagged("get_actual_piece(End)", "get_actual_piece requires an assignment variable");
  flagged("ok = get_actual_piece(\"x\")", "Piece end parameter must be a valid variable in get_actual_piece");
  flagged("ok = randomnumber(500)", "randomnumber requires at least 2 parameters inside parentheses");
  flagged("ok = randomnumber(\"a\",1500)", "Lowest parameter must be a valid variable in randomnumber");

  std::cout << "strict mode\n";
  strict = true;
  checkEq(messages("get_edge_dir(edge) P0,D0\nproj_vxgeom_dir(G) H,P0,D0,D0"), "",
    "get_edge_dir outputs are a point and a direction");
  flagged("get_edge_dir(edge) P0,D0\nproj_vxgeom_dir(G) H,D0,D0,D0",
    "proj_vxgeom_dir: plane point 'D0' should be of type [pvector] but is 'dvector'");
  flagged("w = randomnumber(filename$,5)", "Lowest parameter must be a valid variable in randomnumber");
  flagged("set_piece_inst(num) \"LAYER\",\"1\"",
    "set_piece_inst: input profile 'num' should be of type [profile] but is 'value'");
  flagged("x = get_actual_piece(num)",
    "get_actual_piece: piece end 'num' should be of type [profileend] but is 'value'");

  std::cout << (failures == 0 ? "all passed\n" : std::to_string(failures) + " failed\n");
  return failures == 0 ? 0 : 1;
}
