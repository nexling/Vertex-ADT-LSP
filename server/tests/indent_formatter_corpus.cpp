// Indentation formatter corpus gate (docs/adr/0013-native-indentation-formatter.md).
// For every Vertex ADT file under a root: formatting must be idempotent and
// change nothing but leading whitespace. Refusals are counted, not failures.
// Build: cmake --build build --config Release --target indent-formatter-corpus
// Run:   build\Release\indent-formatter-corpus.exe C:\vxbd320 [tabSize] [--list-refusals]
#include "indent_formatter.h"
#include "text_util.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace vertexadt;

namespace {

bool isVertexFile(const fs::path& path)
{
  auto ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext == ".prg" || ext == ".adt" || ext == ".p" || ext == ".vxt" || ext == ".3d" || ext == ".2d";
}

std::string readFile(const fs::path& path)
{
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

std::string_view stripLead(std::string_view line)
{
  std::size_t i = 0;
  while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
  {
    ++i;
  }
  return line.substr(i);
}

// Same lines with leading spaces/tabs removed, whitespace-only lines emptied.
bool onlyLeadingWhitespaceChanged(std::string_view before, std::string_view after, int& badLine)
{
  const auto a = splitLines(before);
  const auto b = splitLines(after);
  if (a.size() != b.size())
  {
    badLine = -1;
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i)
  {
    auto x = stripLead(a[i]);
    auto y = stripLead(b[i]);
    if (trim(x).empty() && trim(y).empty())
    {
      if (!y.empty() && y != "\r")
      {
        badLine = static_cast<int>(i);
        return false;
      }
      continue;
    }
    if (x != y)
    {
      badLine = static_cast<int>(i);
      return false;
    }
  }
  return true;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    std::cerr << "usage: indent-formatter-corpus <root> [tabSize] [--list-refusals]\n";
    return 2;
  }
  const fs::path root = argv[1];
  // A single file: print its formatted text (or the refusal) for inspection.
  if (fs::is_regular_file(root))
  {
    const auto text = readFile(root);
    const auto formatted = formatIndentation(text, argc > 2 ? std::stoi(argv[2]) : 2);
    if (!formatted)
    {
      std::cout << indentRefusalMessage(*planIndentation(text, 2).refusal) << "\n";
      return 1;
    }
    std::cout << *formatted;
    return 0;
  }
  int tabSize = 2;
  bool listRefusals = false;
  for (int i = 2; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--list-refusals")
    {
      listRefusals = true;
    }
    else
    {
      tabSize = std::stoi(arg);
    }
  }

  int files = 0;
  int skippedDump = 0;
  int unchanged = 0;
  int changed = 0;
  int refused = 0;
  int failures = 0;
  long long changedLines = 0;
  std::map<std::string, int> refusalKinds;
  static const std::regex lineNumbers{R"(line \d+)"};
  static const std::regex sectionNames{R"(`#\w+#`)"};

  for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied))
  {
    if (!entry.is_regular_file() || !isVertexFile(entry.path()))
    {
      continue;
    }
    const auto text = readFile(entry.path());
    if (isDebugDump(text))
    {
      ++skippedDump;
      continue;
    }
    ++files;
    const auto plan = planIndentation(text, tabSize);
    if (plan.refusal)
    {
      ++refused;
      auto kind = std::regex_replace(plan.refusal->reason, lineNumbers, "line N");
      kind = std::regex_replace(kind, sectionNames, "`#S#`");
      ++refusalKinds[kind];
      if (listRefusals)
      {
        std::cout << "refused " << entry.path().string() << ": " << plan.refusal->reason << '\n';
      }
      continue;
    }
    const auto once = formatIndentation(text, tabSize);
    const auto twice = once ? formatIndentation(*once, tabSize) : std::nullopt;
    int badLine = 0;
    if (!once || !twice || *once != *twice)
    {
      ++failures;
      std::cout << "FAIL not idempotent: " << entry.path().string() << '\n';
      continue;
    }
    if (!onlyLeadingWhitespaceChanged(text, *once, badLine))
    {
      ++failures;
      std::cout << "FAIL non-whitespace change: " << entry.path().string() << " line " << badLine + 1 << '\n';
      continue;
    }
    if (*once == text)
    {
      ++unchanged;
    }
    else
    {
      ++changed;
      changedLines += static_cast<long long>(indentationEdits(text, tabSize).edits.size());
    }
  }

  std::cout << "files " << files << " (debug dumps skipped " << skippedDump << ")\n"
            << "  formatted, already correct " << unchanged << '\n'
            << "  formatted, changed         " << changed << " (" << changedLines << " lines)\n"
            << "  refused                    " << refused << '\n';
  for (const auto& [kind, count] : refusalKinds)
  {
    std::cout << "    " << count << "  " << kind << '\n';
  }
  std::cout << "failures " << failures << '\n';
  return failures == 0 ? 0 : 1;
}
