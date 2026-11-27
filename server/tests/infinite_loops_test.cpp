// Infinite-loop checks (docs/adr/0027-native-infinite-loop-warnings.md).
// Build: cmake --build build --config Release --target infinite-loops-test
// Run:   build\Release\infinite-loops-test.exe
#include "infinite_loops.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace vertexadt;

namespace {

int failures = 0;

void checkEq(const std::string& got, const std::string& want, const std::string& what)
{
  const bool ok = got == want;
  std::cout << (ok ? "  ok   " : "  FAIL ") << what << '\n';
  if (!ok)
  {
    std::cout << "         got:  [" << got << "]\n         want: [" << want << "]\n";
    ++failures;
  }
}

std::string join(const std::vector<std::string>& lines)
{
  std::string out;
  for (const auto& line : lines)
  {
    out += line;
    out += "\r\n";
  }
  return out;
}

// Issues as "W3:0-4" (warning) / "I5:4-10" (information), space separated.
std::string run(const std::vector<std::string>& lines, const std::map<std::string, std::string>& files = {})
{
  const IncludeLoader loader = [&files](const IncludeCallTarget& target) -> std::optional<IncludedCode> {
    const auto it = files.find(target.filePath);
    if (it == files.end())
    {
      return std::nullopt;
    }
    return IncludedCode{.key = it->first, .text = it->second};
  };
  std::string out;
  for (const auto& issue : findInfiniteLoops(join(lines), loader))
  {
    out += out.empty() ? "" : " ";
    out += (issue.kind == InfiniteLoopKind::NoExit ? "W" : "I") + std::to_string(issue.line) + ":"
      + std::to_string(issue.startChar) + "-" + std::to_string(issue.endChar);
  }
  return out;
}

std::string firstMessage(const std::vector<std::string>& lines)
{
  const auto issues = findInfiniteLoops(join(lines), {});
  return issues.empty() ? "" : issues.front().message;
}

} // namespace

int main()
{
  std::cout << "no way out\n";
  checkEq(run({"loop", "  a = a + 1", "endloop"}), "W0:0-4", "bare loop without exitif");
  checkEq(run({"  loop ;! forever", "    a = 1", "  endloop"}), "W0:2-6", "comment after loop, indented");
  checkEq(run({"loop 10", "  a = a + 1", "endloop"}), "", "counted loop is never checked");
  checkEq(run({"loop N", "endloop"}), "", "loop with a variable count");
  checkEq(firstMessage({"loop", "endloop"}), "Infinite loop: no exitif/break/exit inside loop ... endloop.",
    "warning message");
  checkEq(run({"loop$ = 1", "loop", "endloop"}), "W1:0-4", "loop$ is a variable, not the keyword");

  std::cout << "ways out\n";
  checkEq(run({"loop", "  i = i + 1", "  exitif i > 10", "endloop"}), "", "exitif with a ticker");
  checkEq(run({"loop", "  break", "endloop"}), "", "break");
  checkEq(run({"loop", "  exit", "endloop"}), "", "exit");
  checkEq(run({"loop", "  exit \"OK\"", "endloop"}), "", "exit with a value");
  checkEq(run({"loop", "  quit", "endloop"}), "", "quit");
  checkEq(run({"loop", "  return", "endloop"}), "W0:0-4", "return is not a way out");
  checkEq(run({"loop", "  if a = 1 then break", "endloop"}), "", "inline if then break");
  checkEq(run({"loop", "  if a = 1 then b = 2 else exit", "endloop"}), "", "inline else exit");
  checkEq(run({"loop", "  a = 1 ; exitif !a", "endloop"}), "", "exitif after ;");
  checkEq(run({"loop", "  if a = 1 then", "    exitif 1", "  endif", "endloop"}), "",
    "exitif inside an if block counts");
  checkEq(run({"loop", "  exitify = 1", "endloop"}), "W0:0-4", "exitify is a variable");

  std::cout << "nested loops\n";
  checkEq(run({"loop", "  loop", "    i = i + 1", "    exitif i > 3", "  endloop", "endloop"}), "W0:0-4",
    "inner exitif does not leave the outer loop");
  checkEq(run({"loop", "  loop", "    exit", "  endloop", "endloop"}), "", "inner exit leaves every loop");
  checkEq(run({"loop", "  loop 3", "    a = 1", "  endloop", "  exitif a", "endloop"}), "",
    "counted inner loop, outer exitif changed inside it");

  std::cout << "exitif that never changes\n";
  checkEq(run({"loop", "  a = 1", "  exitif i > 10", "endloop"}), "I2:2-8", "condition variable never changes");
  checkEq(firstMessage({"loop", "  exitif i > N", "endloop"}),
    "exitif condition can never become true inside this loop: `i`, `N` are never changed in the loop body.",
    "message lists every variable");
  checkEq(firstMessage({"loop", "  exitif !Pce", "endloop"}),
    "exitif condition can never become true inside this loop: `Pce` is never changed in the loop body.",
    "inline ! is NOT, not a comment");
  checkEq(run({"loop", "  Pce = next_pce(Pce)", "  exitif !Pce", "endloop"}), "", "assigned by an expression");
  checkEq(run({"loop", "  I = I + 1", "  exitif I > n", "endloop"}), "", "one changed variable is enough");
  checkEq(run({"loop", "  i = i + 1", "  exitif I > 10", "endloop"}), "", "names compare case-insensitively");
  checkEq(run({"loop", "  Name$ = \"x\"", "  exitif Name$ = \"y\"", "endloop"}), "", "string variable assigned");
  checkEq(run({"loop", "  Arr(I) = 1", "  exitif Arr(2)", "endloop"}), "", "subscripted assignment writes Arr");
  checkEq(run({"loop", "  set Done", "  exitif Done", "endloop"}), "", "set writes");
  checkEq(run({"loop", "  if a then Done = 1", "  exitif Done", "endloop"}), "", "inline if assignment writes");
  checkEq(run({"loop", "  exitif dbfind(1) A, B, C", "endloop"}), "", "condition calls a command");
  checkEq(run({"loop", "  exitif 0", "endloop"}), "", "condition reads no variable");
  checkEq(run({"loop", "  exitif a", "  exitif b", "  b = 1", "endloop"}), "", "one plausible exitif silences all");
  checkEq(run({"loop", "  exitif a", "  if x then break", "endloop"}), "", "a break silences it");
  checkEq(run({"loop", "  exitif a", "  exitif b", "endloop"}), "I1:2-8 I2:2-8", "both exitifs reported");
  checkEq(run({"loop", "  loop 2", "    a = 1", "  endloop", "  exitif a", "endloop"}), "",
    "changed in an inner loop");
  checkEq(run({"loop", "  Text$ = \"exitif i\"", "  exitif i", "endloop"}), "I2:2-8", "quoted text is not code");

  std::cout << "command arguments\n";
  checkEq(run({"loop", "  Sup0 = get_trs_supports(Trs,Sup)", "  exitif !Sup", "endloop"}), "",
    "out_ argument inside parentheses writes");
  checkEq(run({"loop", "  Sup0 = get_trs_supports(Trs,Sup)", "  exitif !Trs", "endloop"}), "I2:2-8",
    "in_ argument does not write");
  checkEq(run({"loop", "  get_edge_dir(Edge) P,Dir", "  exitif Dir", "endloop"}), "",
    "out_ argument after the parentheses writes");
  checkEq(run({"loop", "  get_edge_dir(Edge) P,Dir", "  exitif Edge", "endloop"}), "I2:2-8",
    "in_ argument before the outputs does not write");
  checkEq(run({"loop", "  update_joints(Obj)", "  exitif !Obj", "endloop"}), "I2:2-8",
    "inObj label reads only");
  checkEq(run({"loop", "  my_own_command Obj", "  exitif !Obj", "endloop"}), "",
    "uncataloged command may write its arguments");
  checkEq(run({"loop", "  call \"sub.prg\"", "  exitif !Obj", "endloop"}), "", "call silences the check");
  checkEq(run({"loop", "  call \"sub.prg\"", "endloop"}), "W0:0-4", "call is not a way out");

  std::cout << "includes\n";
  checkEq(run({"loop", "  # include \"inc.prg\"", "endloop"}, {{"inc.prg", "exitif !Obj\r\n"}}), "",
    "exitif in an included file");
  checkEq(run({"loop", "  # include \"inc.prg\"", "endloop"}, {{"inc.prg", "a = 1\r\n"}}), "W0:0-4",
    "included file without exitif");
  checkEq(run({"loop", "  # include \"inc.prg\"", "  exitif !Obj", "endloop"}, {{"inc.prg", "Obj = 1\r\n"}}), "",
    "included assignment changes the condition");
  checkEq(run({"loop", "  # include \"inc.prg\"", "  exitif !Obj", "endloop"}, {{"inc.prg", "a = 1\r\n"}}),
    "I2:2-8", "included code that does not change it");
  checkEq(run({"loop", "  # include \"missing.prg\"", "endloop"}), "", "unresolved include silences both");
  checkEq(run({"loop", "  # include \"inc.prg\" PART", "endloop"},
            {{"inc.prg", "#OTHER#\r\nexitif 1\r\n#OTHER#\r\n#PART#\r\na = 1\r\n#PART#\r\n"}}),
    "W0:0-4", "only the target section is inlined");
  checkEq(run({"loop", "  # include \"inc.prg\" PART", "endloop"},
            {{"inc.prg", "#PART#\r\nexitif 1\r\n#PART#\r\n"}}),
    "", "exitif in the target section");
  checkEq(run({"loop", "  # include \"inc.prg\"", "endloop"},
            {{"inc.prg", "loop\r\nexitif 1\r\nendloop\r\n"}}),
    "W0:0-4", "exitif of a loop inside the include stays inside it");
  checkEq(run({"loop", "  # include \"a.prg\"", "endloop"}, {{"a.prg", "# include \"a.prg\"\r\n"}}), "",
    "include cycle is treated as unknown");

  std::cout << "structure\n";
  checkEq(run({"loop", "  a = 1"}), "", "unclosed loop: no diagnostics");
  checkEq(run({"loop", "endloop", "endloop"}), "", "stray endloop: no diagnostics");
  checkEq(run({"exitif a", "loop", "endloop"}), "W1:0-4", "exitif outside any loop is ignored");
  checkEq(run({"loop", "  a = 1 \\", "    + 2", "  exitif a", "endloop"}), "", "continued line");
  checkEq(run({"#TEST_SKIP#", "loop", "endloop", "#TEST_SKIP#"}), "W1:0-4", "plain section is checked");

  std::cout << (failures == 0 ? "all passed\n" : std::to_string(failures) + " failed\n");
  return failures == 0 ? 0 : 1;
}
