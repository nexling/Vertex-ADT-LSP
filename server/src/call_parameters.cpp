#include "call_parameters.h"

#include "text_util.h"

#include <regex>

namespace vertexadt {
namespace {

const std::regex& getParamRe()
{
  static const std::regex re{R"re((\w+)\$?\s*=\s*get\(\s*\$(\w+)\s*\))re"};
  return re;
}

// server.js findValidationCommentIndex: first `;!` not inside quotes.
auto findValidationCommentIndex(std::string_view line) -> std::ptrdiff_t
{
  for (std::size_t i = 0; i + 1 < line.size(); ++i)
  {
    if (line[i] == ';' && line[i + 1] == '!' && !isInsideQuotes(line, i))
    {
      return static_cast<std::ptrdiff_t>(i);
    }
  }
  return -1;
}

// `line.substring(index + 2).split('"')[1]`, kept only when non-empty.
auto quotedComment(std::string_view line) -> std::optional<std::string>
{
  const auto index = findValidationCommentIndex(line);
  if (index < 0)
  {
    return std::nullopt;
  }
  const auto rest = line.substr(static_cast<std::size_t>(index) + 2);
  const auto open = rest.find('"');
  if (open == std::string_view::npos)
  {
    return std::nullopt;
  }
  auto close = rest.find('"', open + 1);
  if (close == std::string_view::npos)
  {
    close = rest.size();
  }
  auto comment = rest.substr(open + 1, close - open - 1);
  if (comment.empty())
  {
    return std::nullopt;
  }
  return std::string(comment);
}

bool isSectionKeyword(std::string_view token)
{
  const auto lower = toLowerAscii(token);
  return lower == "section" || lower == "sectionoffile";
}

} // namespace

auto extractFunctionParameters(
  std::string_view text,
  std::optional<std::string_view> targetSection) -> std::vector<CallParameter>
{
  std::vector<CallParameter> parameters;
  const auto lines = splitLines(text);
  const auto sections = parseFileSections(lines);
  const bool named = targetSection && !targetSection->empty();
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    if (named && sectionNameForLine(i, sections) != *targetSection)
    {
      continue;
    }
    const auto line = lines[static_cast<std::size_t>(i)];
    const char* begin = line.data();
    const char* end = begin + line.size();
    std::optional<std::optional<std::string>> comment;
    for (std::cregex_iterator it(begin, end, getParamRe()), last; it != last; ++it)
    {
      const auto whole = (*it)[0].str();
      const auto variableName = std::string(trim(std::string_view(whole).substr(0, whole.find('='))));
      if (!comment)
      {
        comment = quotedComment(line);
      }
      parameters.push_back(CallParameter{
        .variableName = variableName,
        .parameterName = std::string(trim((*it)[2].str())),
        .comment = *comment,
      });
    }
  }
  return parameters;
}

namespace {

// targetSectionSupport.js parseIncludeCallPathAndSection(value, '').
auto parseCallPathAndSection(std::string_view value) -> CallTarget
{
  auto pathPart = std::string(trim(replaceBackslashes(stripPathQuotes(value))));
  std::optional<std::string> targetSection;
  static const std::regex pathSection{R"(^(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$)"};
  std::smatch match;
  if (std::regex_match(pathPart, match, pathSection))
  {
    const auto token = match[2].str();
    if (!isSectionKeyword(token))
    {
      targetSection = token;
      pathPart = std::string(trim(match[1].str()));
    }
    else
    {
      static const std::regex stripSection{R"(\s+SECTION(?:OFFILE)?$)", std::regex::icase};
      pathPart = std::string(trim(std::regex_replace(pathPart, stripSection, "")));
    }
  }
  return CallTarget{.filePath = std::move(pathPart), .targetSection = std::move(targetSection)};
}

// JS `value.replace(/^["']|["']$/g, '')`: one leading and one trailing quote.
auto dropOuterQuotes(std::string_view value) -> std::string_view
{
  if (!value.empty() && (value.front() == '"' || value.front() == '\''))
  {
    value.remove_prefix(1);
  }
  if (!value.empty() && (value.back() == '"' || value.back() == '\''))
  {
    value.remove_suffix(1);
  }
  return value;
}

} // namespace

auto callTargetFromFirstArgument(std::string_view firstArgument) -> CallTarget
{
  return parseCallPathAndSection(dropOuterQuotes(trim(firstArgument)));
}

auto callTargetFromUntrimmedArgument(std::string_view argument) -> CallTarget
{
  return parseCallPathAndSection(dropOuterQuotes(argument));
}

auto calleeParameters(
  const LibraryLayout& layout,
  const std::function<std::optional<std::string>(const std::filesystem::path&)>& textForPath,
  const CallTarget& target) -> std::vector<CallParameter>
{
  if (target.filePath.empty() || !textForPath)
  {
    return {};
  }
  const auto resolved = layout.resolveVertexFilePath(target.filePath);
  if (!resolved)
  {
    return {};
  }
  const auto text = textForPath(*resolved);
  if (!text || text->empty())
  {
    return {};
  }
  std::optional<std::string_view> section;
  if (target.targetSection)
  {
    section = *target.targetSection;
  }
  return extractFunctionParameters(*text, section);
}

} // namespace vertexadt
