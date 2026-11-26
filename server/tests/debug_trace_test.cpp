// Debug traces (docs/adr/0025-debug-trace-fast-path.md): the hand-written
// assignment parser matches the regex one, the index answers exactly what the
// full-scan hover answered, line splitting, and timings.
// Build: cmake --build build --config Release --target debug-trace-test
// Run:   build\Release\debug-trace-test.exe [--parity] [file.dbg ...]
//   With files: parity of parser + hover on each (--parity also runs the
//   O(n) regex hover on sampled lines) and timings.
#include "cheap_hover.h"
#include "debug_dump.h"
#include "debug_trace.h"
#include "semantic_tokens.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

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

auto spansAsMap(std::string_view line) -> std::map<std::string, std::string>
{
  std::vector<DebugAssignmentSpan> spans;
  parseLineAssignmentSpans(line, spans);
  auto stripped = line;
  if (!stripped.empty() && stripped.back() == '\r')
  {
    stripped.remove_suffix(1);
  }
  std::map<std::string, std::string> out;
  for (const auto& span : spans)
  {
    out[std::string(stripped.substr(span.nameBegin, span.nameEnd - span.nameBegin))] =
      std::string(stripped.substr(span.valueBegin, span.valueEnd - span.valueBegin));
  }
  return out;
}

auto regexAsMap(std::string_view line) -> std::map<std::string, std::string>
{
  const auto parsed = parseLineAssignments(line);
  return {parsed.begin(), parsed.end()};
}

std::string render(const std::map<std::string, std::string>& map)
{
  std::string out = "{";
  for (const auto& [k, v] : map)
  {
    out += k + "=[" + v + "] ";
  }
  return out + "}";
}

void checkParserLine(std::string_view line)
{
  const auto want = regexAsMap(line);
  const auto got = spansAsMap(line);
  check(
    got == want,
    "parser parity: " + std::string(line.substr(0, 60))
      + (got == want ? "" : "\n         got:  " + render(got) + "\n         want: " + render(want)));
}

std::string readFile(const std::string& path)
{
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

double msSince(std::chrono::steady_clock::time_point start)
{
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

std::string hoverText(const lsp::TextDocumentHoverResult& result)
{
  if (result.isNull())
  {
    return "";
  }
  const auto& hover = result.value();
  if (const auto* markup = std::get_if<lsp::MarkupContent>(&hover.contents))
  {
    return markup->value;
  }
  return "<non-markup>";
}

void unitCases()
{
  std::cout << "parser cases\n";
  for (const auto* line : {
         "P = 1                          <1>",
         "  timer = 1                    <1>\r",
         "D= 5                           <5>",
         "name$ = \"abc\"                <\"abc\">",
         "if ok1 and ok2 then            <true> _if_begin_",
         "if a > 1 then x = 3            <3> _if_begin_",
         "If A>1 Then X$ = \"q\"         <\"q\">",
         "else if a then y = 2           <TRUE>",
         "elseif a then y = 2            <7>",
         "if a then b = 1 then c = 2     <2>",
         "endif                          <> _if_end_",
         "get_vxgeom(l36lochkGeom) cmppce   <35993.6,18134.9,-126 , 32383.6,18134.9,-126>",
         "vec_sub(D) EP2,SP2             <-3610,0,0>",
         "get_xy(P) a,b                  <1 , 2>",
         "get_xy(P) a,b                  <1 , 2 , 3>",
         "get_xy(P) a, b, a              <1 , 2 , 3>",
         "get_xy(P$) a$                  <x>",
         "loop                           <3/10>",
         "x = 1                          <12/3>",
         "x = a < b                      <1>",
         "x = 1                          <a<b>",
         "x = 1                          < 1 >  _bad",
         "x = 1                          <1> _a_ _b_",
         "x = 1                          <1>   _ab_   ",
         "x = 1                          <1>x",
         "x = 1                          <>",
         "x = 1                          <  >",
         "ret$=call(\"$DIR_MACROS/a.prg ADDORUPDROW\",panel_label$,fu...  <\"\">",
         "else                                                     ",
         " START OF DEBUG in FILE \"C:/x.prg\" at 2026-11-26 15:16:24  <> _file_begin_",
         "x == 2                         <0>",
         "1 = 2                          <2>",
       })
  {
    checkParserLine(line);
  }

  std::cout << "header detection\n";
  check(hasDebugHeader("\xEF\xBB\xBF START OF DEBUG in FILE \"x\""), "header at start");
  check(!hasDebugHeader(std::string(5000, ' ') + "START OF DEBUG in FILE"), "header past 4 KB ignored");
  check(isDebugTraceDocument("/c:/x/_a.DBG", "x = 1"), ".dbg path (any case)");
  check(!isDebugTraceDocument("/c:/x/a.prg", "x = 1"), "plain source");

  std::cout << "line split\n";
  {
    const std::string_view line = "if a<b then x = 1   <1> _if_begin_\r";
    const auto parts = splitDebugLine(line);
    check(line.substr(parts.codeBegin, parts.codeEnd - parts.codeBegin) == "if a<b then x = 1", "code part");
    check(line.substr(parts.valueBegin, parts.valueEnd - parts.valueBegin) == "<1>", "value part");
    check(line.substr(parts.markerBegin, parts.markerEnd - parts.markerBegin) == "_if_begin_", "marker part");
  }
  {
    const std::string_view line = "else                     ";
    const auto parts = splitDebugLine(line);
    check(line.substr(parts.codeBegin, parts.codeEnd - parts.codeBegin) == "else", "no value: whole code");
    check(parts.valueBegin == parts.valueEnd, "no value: empty value");
  }
  {
    const std::string_view line = "x = _tmp_";
    const auto parts = splitDebugLine(line);
    check(parts.markerBegin == parts.markerEnd, "marker needs a value before it");
    check(line.substr(parts.codeBegin, parts.codeEnd - parts.codeBegin) == "x = _tmp_", "marker-like name stays code");
  }
  {
    const std::string_view line = " END of FILE \"x\" at SECTION #A#  <> _file_end_";
    const auto parts = splitDebugLine(line);
    check(parts.kind == DebugLineParts::Kind::Structural, "END of FILE is structural");
  }

  std::cout << "index and hover\n";
  {
    auto text = std::make_shared<const std::string>(
      " START OF DEBUG in FILE \"x\"  <> _file_begin_\n"
      "aa = 1   <1>\n"
      "b = 2   <2>\n"
      "aa = 3   <3>\r\n"
      "c = aa   <3>\n");
    DebugTrace trace(text, 7);
    trace.setIndex(buildDebugAssignmentIndex(trace));
    check(trace.lineCount() == 6, "line count includes the empty last line");
    const auto at = [&](int line, std::string_view name) {
      const auto got = trace.mostRecentAssignment(line, name);
      const auto want = findMostRecentDebugValue(*text, line, name);
      const auto same = got.has_value() == want.has_value()
        && (!got || (got->value == want->value && got->line == want->line));
      check(same, "most recent " + std::string(name) + " at line " + std::to_string(line));
    };
    at(0, "aa");
    at(1, "aa");
    at(2, "aa");
    at(3, "aa");
    at(99, "aa");
    at(4, "b");
    at(4, "zz");
    const auto md = hoverText(debugTraceHover(trace, lsp::Position{.line = 4, .character = 4}));
    check(md == "**aa** = `3`\n\n_Set on line 4_", "hover markdown: " + md);

    const auto tokens = debugTraceSemanticTokens(trace, 0, trace.lineCount());
    check(!tokens.data.empty() && tokens.data.size() % 5 == 0, "tokens encoded in fives");
  }
}

void fileCases(const std::string& path, bool fullParity)
{
  std::cout << "file " << path << '\n';
  auto start = std::chrono::steady_clock::now();
  auto text = std::make_shared<const std::string>(readFile(path));
  std::cout << "  read " << text->size() << " bytes in " << msSince(start) << " ms\n";

  start = std::chrono::steady_clock::now();
  DebugTrace trace(text, 1);
  std::cout << "  line table: " << trace.lineCount() << " lines in " << msSince(start) << " ms\n";

  start = std::chrono::steady_clock::now();
  const auto lexical = debugTraceSemanticTokens(trace, trace.lineCount() / 2, trace.lineCount() / 2 + 120);
  std::cout << "  range tokens (120 lines, no index): " << lexical.data.size() / 5 << " tokens in "
            << msSince(start) << " ms\n";

  start = std::chrono::steady_clock::now();
  auto index = buildDebugAssignmentIndex(trace);
  const auto indexMs = msSince(start);
  trace.setIndex(index);
  std::cout << "  index: " << index->byName.size() << " names in " << indexMs << " ms\n";
  check(indexMs < 1000.0, "index under 1 s");

  start = std::chrono::steady_clock::now();
  const auto range = debugTraceSemanticTokens(trace, trace.lineCount() / 2, trace.lineCount() / 2 + 120);
  const auto rangeMs = msSince(start);
  std::cout << "  range tokens (120 lines, indexed): " << range.data.size() / 5 << " tokens in " << rangeMs
            << " ms\n";
  check(rangeMs < 100.0, "range under 100 ms");

  start = std::chrono::steady_clock::now();
  const auto full = debugTraceSemanticTokens(trace, 0, trace.lineCount());
  std::cout << "  full tokens: " << full.data.size() / 5 << " tokens in " << msSince(start) << " ms\n";

  // Hover: worst case is a name near the end of the file.
  int hovers = 0;
  double worstHover = 0;
  for (int line = trace.lineCount() - 1; line >= 0 && hovers < 50; line -= std::max(1, trace.lineCount() / 50))
  {
    const auto lineText = trace.line(line);
    const auto parts = splitDebugLine(lineText);
    if (parts.codeEnd <= parts.codeBegin)
    {
      continue;
    }
    start = std::chrono::steady_clock::now();
    (void)debugTraceHover(trace, lsp::Position{.line = static_cast<lsp::Uint>(line), .character = static_cast<lsp::Uint>(parts.codeBegin)});
    worstHover = std::max(worstHover, msSince(start));
    ++hovers;
  }
  std::cout << "  hover: worst of " << hovers << " = " << worstHover << " ms\n";
  check(worstHover < 20.0, "hover under 20 ms");

  // Parser parity on every line.
  start = std::chrono::steady_clock::now();
  int mismatches = 0;
  for (int i = 0; i < trace.lineCount(); ++i)
  {
    const auto line = trace.line(i);
    if (spansAsMap(line) != regexAsMap(line))
    {
      if (++mismatches <= 5)
      {
        std::cout << "  mismatch line " << i + 1 << ": " << line.substr(0, 120) << '\n'
                  << "    got:  " << render(spansAsMap(line)) << "\n    want: " << render(regexAsMap(line))
                  << '\n';
      }
    }
  }
  check(mismatches == 0, "parser parity on all " + std::to_string(trace.lineCount()) + " lines ("
    + std::to_string(msSince(start)) + " ms)");

  if (fullParity)
  {
    // Old O(n) hover against the index on sampled lines and names.
    int compared = 0;
    int differ = 0;
    for (int line = 0; line < trace.lineCount() && compared < 40; line += std::max(1, trace.lineCount() / 40))
    {
      std::vector<DebugAssignmentSpan> spans;
      // Names: any assigned name at or above this line, from the index.
      for (const auto& [name, entries] : index->byName)
      {
        if (entries.front().line > line)
        {
          continue;
        }
        const auto got = trace.mostRecentAssignment(line, name);
        const auto want = findMostRecentDebugValue(*text, line, name);
        const auto same = got.has_value() == want.has_value()
          && (!got || (got->value == want->value && got->line == want->line));
        differ += same ? 0 : 1;
        ++compared;
        break;
      }
    }
    check(differ == 0, "hover parity on " + std::to_string(compared) + " sampled lookups");
  }
}

} // namespace

int main(int argc, char** argv)
{
  unitCases();
  bool fullParity = false;
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--parity")
    {
      fullParity = true;
      continue;
    }
    fileCases(arg, fullParity);
  }
  std::cout << (failures == 0 ? "PASS" : "FAIL") << " (" << failures << " failures)\n";
  return failures == 0 ? 0 : 1;
}
