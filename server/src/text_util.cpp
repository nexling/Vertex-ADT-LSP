#include "text_util.h"

#include "utf16_text.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace vertexadt {
namespace {

bool isAsciiSpace(unsigned char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

const std::regex& callParamRe()
{
  static const std::regex re{R"re((\w+)\$?\s*=\s*get\(\s*\$(\w+)\s*\))re"};
  return re;
}

const std::regex& sectionMarkerRe()
{
  static const std::regex re{R"re(^#([A-Za-z_][A-Za-z0-9_]*)#$)re"};
  return re;
}

const std::regex& localSlotRe()
{
  static const std::regex re{R"re(^l\d+[A-Za-z][A-Za-z0-9_-]*\$?$)re"};
  return re;
}

const std::regex& callParamIndexRe()
{
  static const std::regex re{R"re(^CALLPARAM(\d+)$)re", std::regex::icase};
  return re;
}

} // namespace

auto splitLines(std::string_view text) -> std::vector<std::string_view>
{
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (start <= text.size())
  {
    const auto end = text.find('\n', start);
    if (end == std::string_view::npos)
    {
      lines.push_back(text.substr(start));
      break;
    }
    lines.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  return lines;
}

auto trim(std::string_view text) -> std::string_view
{
  std::size_t begin = 0;
  while (begin < text.size() && isAsciiSpace(static_cast<unsigned char>(text[begin])))
  {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && isAsciiSpace(static_cast<unsigned char>(text[end - 1])))
  {
    --end;
  }
  return text.substr(begin, end - begin);
}

auto toLowerAscii(std::string_view text) -> std::string
{
  std::string out(text);
  for (char& ch : out)
  {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return out;
}

auto replaceBackslashes(std::string_view text) -> std::string
{
  std::string out(text);
  for (char& ch : out)
  {
    if (ch == '\\')
    {
      ch = '/';
    }
  }
  return out;
}

auto normalizeFsPath(std::string_view path) -> std::string
{
  auto out = replaceBackslashes(path);
  while (out.size() > 1 && (out.back() == '/' || out.back() == '\\'))
  {
    out.pop_back();
  }
  return toLowerAscii(out);
}

bool isInsideQuotes(std::string_view line, std::size_t position)
{
  char active = 0;
  const auto n = std::min(position, line.size());
  for (std::size_t i = 0; i < n; ++i)
  {
    if (active)
    {
      if (line[i] == active)
      {
        active = 0;
      }
      continue;
    }
    if (line[i] == '"' || line[i] == '\'')
    {
      active = line[i];
    }
  }
  return active != 0;
}

auto findBangCommentIndex(std::string_view line) -> std::ptrdiff_t
{
  // `;`, `!` and quotes are ASCII, so a byte scan finds the same position as
  // Node's code-unit scan.
  for (std::size_t i = 0; i + 1 < line.size(); ++i)
  {
    if (line[i] == ';' && line[i + 1] == '!' && !isInsideQuotes(line, i))
    {
      return static_cast<std::ptrdiff_t>(i);
    }
  }
  return -1;
}

bool lineHasIgnoreClose(std::string_view line)
{
  const auto commentIndex = findBangCommentIndex(line);
  if (commentIndex < 0)
  {
    return false;
  }
  const auto suffix = utf8To16(line.substr(static_cast<std::size_t>(commentIndex)));
  const auto matchesAt = [&](std::size_t at, std::u16string_view lowerWord)
  {
    if (at + lowerWord.size() > suffix.size())
    {
      return false;
    }
    for (std::size_t k = 0; k < lowerWord.size(); ++k)
    {
      if (asciiLower(suffix[at + k]) != lowerWord[k])
      {
        return false;
      }
    }
    return true;
  };
  for (std::size_t i = 0; i < suffix.size(); ++i)
  {
    if ((i > 0 && isJsWord(suffix[i - 1])) || !matchesAt(i, u"ignore"))
    {
      continue;
    }
    auto j = i + 6;
    if (j >= suffix.size() || !isJsSpace(suffix[j]))
    {
      continue;
    }
    j = skipJsSpace(suffix, j);
    if (matchesAt(j, u"close") && (j + 5 == suffix.size() || !isJsWord(suffix[j + 5])))
    {
      return true;
    }
  }
  return false;
}

auto stripValidationComment(std::string_view line) -> std::string
{
  for (std::size_t i = 0; i + 1 < line.size(); ++i)
  {
    if (line[i] == ';' && line[i + 1] == '!' && !isInsideQuotes(line, i))
    {
      return std::string(line.substr(0, i));
    }
  }
  return std::string(line);
}

bool isSharedPerLineSkip(std::string_view raw)
{
  const auto trimmed = trim(raw);
  return trimmed.empty()
    || trimmed.starts_with("!")
    || trimmed.starts_with("messagebox")
    || trimmed.starts_with("##")
    || trimmed.ends_with("##")
    || trimmed.starts_with("debug")
    || (trimmed.front() == '#' && trimmed.back() == '#')
    || trimmed.starts_with("# attributes on");
}

bool isInlineIfThen(std::string_view trimmed)
{
  static const std::regex re{R"(if\s+.*\s+then\s+.*)", std::regex::icase};
  return std::regex_search(trimmed.data(), trimmed.data() + trimmed.size(), re);
}

auto findInlineBangCommentIndex(std::string_view line) -> std::ptrdiff_t
{
  for (std::size_t i = 0; i < line.size(); ++i)
  {
    if (line[i] == '!' && !isInsideQuotes(line, i))
    {
      return static_cast<std::ptrdiff_t>(i);
    }
  }
  return -1;
}

bool isBangComment(std::string_view text)
{
  const auto trimmed = trim(text);
  return !trimmed.empty() && trimmed.front() == '!';
}

bool isDebugDump(std::string_view text)
{
  return text.find("START OF DEBUG in FILE") != std::string_view::npos
    || text.find("START OF DEBUG in SECTION") != std::string_view::npos;
}

bool isVxdbaseWinstyleShell(std::string_view text)
{
  if (text.empty())
  {
    return false;
  }
  const auto lines = splitLines(text);
  if (!parseFileSections(lines).empty())
  {
    return false;
  }
  static const std::regex marker{R"re(^vxdbase_winstyle\b)re", std::regex::icase};
  for (const auto line : lines)
  {
    const auto trimmed = trim(line);
    if (trimmed.empty() || trimmed.front() == '!')
    {
      continue;
    }
    std::cmatch match;
    if (std::regex_search(trimmed.data(), trimmed.data() + trimmed.size(), match, marker))
    {
      return true;
    }
  }
  return false;
}

bool isValidationExcluded(std::string_view text)
{
  return isDebugDump(text) || isVxdbaseWinstyleShell(text);
}

auto parseFileSections(const std::vector<std::string_view>& lines) -> std::vector<NamedSection>
{
  struct Open
  {
    std::string name;
    int line = 0;
  };
  std::vector<Open> stack;
  std::vector<NamedSection> sections;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    if (lines[static_cast<std::size_t>(i)].empty()
        || lines[static_cast<std::size_t>(i)].front() != '#')
    {
      continue;
    }
    const auto trimmed = std::string(trim(lines[static_cast<std::size_t>(i)]));
    std::smatch match;
    if (!std::regex_match(trimmed, match, sectionMarkerRe()))
    {
      continue;
    }
    const auto name = match[1].str();
    auto open = std::find_if(stack.begin(), stack.end(), [&](const Open& item) {
      return item.name == name;
    });
    if (open == stack.end())
    {
      stack.push_back(Open{.name = name, .line = i});
    }
    else
    {
      sections.push_back(NamedSection{
        .name = name,
        .openLine = open->line,
        .closeLine = i,
      });
      stack.erase(open);
    }
  }
  return sections;
}

auto sectionNameForLine(int line, const std::vector<NamedSection>& sections) -> std::string
{
  for (const auto& section : sections)
  {
    if (line > section.openLine && line < section.closeLine)
    {
      return section.name;
    }
  }
  return {};
}

bool isVerificationIgnoredSection(std::string_view sectionName)
{
  return sectionName == "EDIT_FORM";
}

bool lineInVerificationIgnoredSection(int line, const std::vector<NamedSection>& sections)
{
  return isVerificationIgnoredSection(sectionNameForLine(line, sections));
}

auto captureHoverComment(std::string_view line) -> std::optional<std::string>
{
  // Hand match of the first `;\s*!"([^"]*)"`.
  for (auto semi = line.find(';'); semi != std::string_view::npos; semi = line.find(';', semi + 1))
  {
    auto i = semi + 1;
    while (i < line.size() && isAsciiSpace(static_cast<unsigned char>(line[i])))
    {
      ++i;
    }
    if (i + 1 >= line.size() || line[i] != '!' || line[i + 1] != '"')
    {
      continue;
    }
    const auto close = line.find('"', i + 2);
    if (close != std::string_view::npos)
    {
      return std::string(line.substr(i + 2, close - i - 2));
    }
  }
  return std::nullopt;
}

auto expectedCallParamCountFromText(
  std::string_view text,
  std::optional<std::string_view> targetSection) -> int
{
  const auto lines = splitLines(text);
  const auto sections = parseFileSections(lines);
  int maxIndex = 0;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    if (targetSection && !targetSection->empty())
    {
      if (sectionNameForLine(i, sections) != *targetSection)
      {
        continue;
      }
    }
    const auto line = lines[static_cast<std::size_t>(i)];
    const char* begin = line.data();
    const char* end = begin + line.size();
    for (std::cregex_iterator it(begin, end, callParamRe()), last; it != last; ++it)
    {
      const auto name = (*it)[2].str();
      std::smatch indexMatch;
      if (std::regex_match(name, indexMatch, callParamIndexRe()))
      {
        maxIndex = std::max(maxIndex, std::stoi(indexMatch[1].str()));
      }
    }
  }
  return maxIndex;
}

bool isLocalSlotName(std::string_view name)
{
  return std::regex_match(name.begin(), name.end(), localSlotRe());
}

auto stripPathQuotes(std::string_view value) -> std::string
{
  const auto trimmed = trim(value);
  if (trimmed.size() >= 2)
  {
    const auto first = trimmed.front();
    const auto last = trimmed.back();
    if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
    {
      return std::string(trimmed.substr(1, trimmed.size() - 2));
    }
  }
  return std::string(trimmed);
}

namespace {

bool isSuffixWordChar(unsigned char ch)
{
  return std::isalnum(ch) != 0 || ch == '_';
}

bool isSuffixIdentStart(unsigned char ch)
{
  return std::isalpha(ch) != 0 || ch == '_';
}

bool isSectionKeyword(std::string_view token)
{
  const auto lower = toLowerAscii(token);
  return lower == "section" || lower == "sectionoffile";
}

// Balanced quotes, or a lone leading quote (an unterminated token).
auto stripPathQuotesLoose(std::string_view value) -> std::string
{
  auto stripped = stripPathQuotes(value);
  if (!stripped.empty() && (stripped.front() == '"' || stripped.front() == '\''))
  {
    stripped.erase(0, 1);
  }
  return stripped;
}

} // namespace

auto splitIncludeCallTarget(std::string_view rawPath, std::string_view lineRemainder)
  -> IncludeCallTarget
{
  IncludeCallTarget result;
  auto pathPart = std::string(trim(replaceBackslashes(stripPathQuotesLoose(rawPath))));

  // `^(.+?)\s+([A-Za-z_]\w*)$` on the path.
  std::size_t identStart = pathPart.size();
  while (identStart > 0 && isSuffixWordChar(static_cast<unsigned char>(pathPart[identStart - 1])))
  {
    --identStart;
  }
  if (identStart > 0 && identStart < pathPart.size()
      && isAsciiSpace(static_cast<unsigned char>(pathPart[identStart - 1]))
      && isSuffixIdentStart(static_cast<unsigned char>(pathPart[identStart])))
  {
    const auto token = pathPart.substr(identStart);
    const auto prefix = std::string(trim(std::string_view(pathPart).substr(0, identStart)));
    if (!prefix.empty())
    {
      if (!isSectionKeyword(token))
      {
        result.targetSection = token;
      }
      pathPart = prefix;
    }
  }
  result.filePath = std::move(pathPart);

  const auto remainder = trim(lineRemainder);
  if (!remainder.empty() && isSuffixIdentStart(static_cast<unsigned char>(remainder.front())))
  {
    std::size_t end = 1;
    while (end < remainder.size() && isSuffixWordChar(static_cast<unsigned char>(remainder[end])))
    {
      ++end;
    }
    const auto token = remainder.substr(0, end);
    if (!isSectionKeyword(token))
    {
      result.targetSection = std::string(token);
    }
  }
  return result;
}

} // namespace vertexadt
