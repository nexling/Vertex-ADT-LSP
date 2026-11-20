// Control-block pairing: if/else/endif and loop/endloop groups, the hover
// section, and document highlight (block keywords or textual word fallback).
// Build: cmake --build build --config Release --target control-block-pairing-test
// Run:   build\Release\control-block-pairing-test.exe
#include "cheap_hover.h"
#include "control_block_pairing.h"
#include "extracts.h"

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
  check(got == want, got == want ? what : what + "\n         got:  [" + got + "]\n         want: [" + want + "]");
}

lsp::Position pos(int line, int character)
{
  return lsp::Position{static_cast<lsp::Uint>(line), static_cast<lsp::Uint>(character)};
}

// Highlights rendered as "line:start-end" joined by spaces.
std::string highlights(std::string_view text, int line, int character)
{
  std::string out;
  for (const auto& h : documentHighlights(text, pos(line, character)))
  {
    out += (out.empty() ? "" : " ") + std::to_string(h.range.start.line) + ":"
      + std::to_string(h.range.start.character) + "-" + std::to_string(h.range.end.character);
  }
  return out;
}

std::string pairingHover(std::string_view text, int line, int character)
{
  const auto pairing = pairControlBlocks(text);
  const auto hit = controlBlockAt(pairing, pos(line, character), false);
  return hit ? controlBlockHoverMarkdown(*hit) : std::string("<none>");
}

std::string fullHover(const std::string& text, int line, int character)
{
  const Snapshot doc{
    .uri = lsp::Uri::parse("file:///c%3A/w/h.prg"),
    .version = 1,
    .languageId = "vertexadt",
    .text = text};
  ExtractStore extracts;
  const auto result = cheapHover({.snapshot = doc, .position = pos(line, character), .extracts = extracts, .extensionUri = ""});
  if (result.isNull())
  {
    return "<null>";
  }
  return std::get<lsp::MarkupContent>(result.value().contents).value;
}

void testPairing()
{
  std::cout << "pairing\n";
  const std::string text =
    "if a = 1 then\n"        // 0
    "  loop 3\n"             // 1
    "    if b = 2 then x = 1\n" // 2 inline: no block
    "  endloop\n"            // 3
    "else\n"                 // 4
    "  b = 2\n"              // 5
    "endif\n";               // 6
  checkEq(highlights(text, 0, 0), "0:0-2 4:0-4 6:0-5", "caret on if: if/else/endif");
  checkEq(highlights(text, 6, 5), "0:0-2 4:0-4 6:0-5", "caret right after endif still hits");
  checkEq(highlights(text, 4, 2), "0:0-2 4:0-4 6:0-5", "caret on else");
  checkEq(highlights(text, 3, 4), "1:2-6 3:2-9", "caret on endloop: loop/endloop");
  checkEq(highlights(text, 2, 4), "0:0-2 2:4-6", "inline if-then: plain word highlight (every `if` word)");

  checkEq(pairingHover(text, 6, 2), "**Closes `if` (line 1):**\n```vertexadt\nif a = 1 then\n```",
    "endif hover shows opener");
  checkEq(pairingHover(text, 4, 1),
    "**Else of `if` (line 1):**\n```vertexadt\nif a = 1 then\n```\n\nClosed by `endif` at line 7",
    "else hover shows opener and closer");
  checkEq(pairingHover(text, 0, 1), "Closed by `endif` at line 7 (`else` at line 5)", "if hover");
  checkEq(pairingHover(text, 1, 3), "Closed by `endloop` at line 4", "loop hover");
  checkEq(pairingHover(text, 3, 3), "**Closes `loop` (line 2):**\n```vertexadt\nloop 3\n```", "endloop hover");
  checkEq(pairingHover(text, 6, 5), "<none>", "hover is end-exclusive");
}

void testMultiLineHeaderAndComments()
{
  std::cout << "header / comments / skip\n";
  const std::string text =
    "if a = 1 ;! first\n" // 0
    "  ! a comment line\n" // 1
    "  and b = 2\n"        // 2
    "  or c = 3 then\n"    // 3
    "  x = 1\n"            // 4
    "endif ;! done\n";     // 5
  checkEq(pairingHover(text, 5, 0),
    "**Closes `if` (line 1):**\n```vertexadt\nif a = 1\nand b = 2\nor c = 3 then\n```",
    "multi-line header, comments stripped and skipped");

  const std::string noThen = "if a = 1\n  x = 2\nendif\n";
  checkEq(pairingHover(noThen, 2, 0), "**Closes `if` (line 1):**\n```vertexadt\nif a = 1\n```",
    "header stops at a non-continuation line");
}

void testUnmatchedAndWordBoundary()
{
  std::cout << "unmatched / word boundary / case\n";
  const std::string text =
    "endif\n"           // 0
    "if a then\n"       // 1
    "elsewhere = 1\n"   // 2 not else
    "endiffoo = 2\n"    // 3 not endif
    "ENDIF\n"           // 4 not endif (case-sensitive)
    "else\n";           // 5
  checkEq(pairingHover(text, 0, 1), "No matching `if`", "stray endif");
  checkEq(highlights(text, 0, 1), "0:0-5", "stray endif highlights only itself");
  checkEq(pairingHover(text, 1, 0), "No matching `endif`", "unclosed if");
  checkEq(highlights(text, 1, 0), "1:0-2 5:0-4", "elsewhere/endiffoo/ENDIF are not keywords");
  checkEq(pairingHover(text, 2, 1), "<none>", "elsewhere has no pairing");
  checkEq(pairingHover(text, 5, 0), "**Else of `if` (line 2):**\n```vertexadt\nif a then\n```\n\nNo matching `endif`",
    "else of unclosed if");
  checkEq(pairingHover("loop\n", 0, 0), "No matching `endloop`", "unclosed loop");
  checkEq(pairingHover("else\n", 0, 0), "No matching `if`", "stray else");
}

void testSkippedSections()
{
  std::cout << "sections\n";
  const std::string text =
    "#EDIT_FORM#\n"   // 0 skipped header line
    "if a then\n"     // 1
    "#END_EDIT_FORM#\n"
    "endif\n";        // 3
  checkEq(highlights(text, 3, 0), "1:0-2 3:0-5", "pairs across verification-ignored sections");
}

void testTextualFallback()
{
  std::cout << "textual fallback\n";
  const std::string text =
    "abc = 1\n"          // 0
    "abc$ = \"abc\"\n"   // 1
    "xabc = abc.d\n"     // 2
    "Abc = 2\n";         // 3
  checkEq(highlights(text, 0, 1), "0:0-3 1:0-3 1:8-11 2:7-10",
    "case-sensitive whole word, `$` and `.` and quotes are separators");
  checkEq(highlights(text, 1, 2), "1:0-4", "string variable word includes $");
  checkEq(highlights(text, 0, 3), "0:0-3 1:0-3 1:8-11 2:7-10", "caret right after word");
  checkEq(highlights(text, 0, 5), "", "caret on whitespace/operator: none");
  checkEq(highlights("then x\nthen\n", 0, 1), "0:0-4 1:0-4", "then is a plain word");
}

void testFullHover()
{
  std::cout << "full hover\n";
  const std::string text = "if a = 1 then\n  b = 2\nendif\n";
  const auto hover = fullHover(text, 2, 1);
  check(hover == "**Closes `if` (line 1):**\n```vertexadt\nif a = 1 then\n```\n\n---\n"
      " The 'endif' keyword is used to end an 'if' statement block.",
    "endif hover: pairing, one rule, catalog text [" + hover + "]");
  const auto loopHover = fullHover("loop 3\nendloop\n", 1, 1);
  check(loopHover.starts_with("**Closes `loop` (line 1):**\n```vertexadt\nloop 3\n```\n\n---\n\nExample:"),
    "endloop hover: pairing, rule, then catalog examples [" + loopHover + "]");
  const auto thenHover = fullHover(text, 0, 10);
  check(thenHover.find("Closed by") == std::string::npos && thenHover.find("Closes") == std::string::npos,
    "then hover has no pairing section");
}

} // namespace

int main()
{
  testPairing();
  testMultiLineHeaderAndComments();
  testUnmatchedAndWordBoundary();
  testSkippedSections();
  testTextualFallback();
  testFullHover();
  std::cout << (failures == 0 ? "all passed\n" : std::to_string(failures) + " failed\n");
  return failures == 0 ? 0 : 1;
}
