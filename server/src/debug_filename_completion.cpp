#include "debug_filename_completion.h"
#include "utf16_text.h"

#include "text_util.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Port of the client/out/extension.js debug-filename completion provider and
// getEnclosingSectionName. The site test runs on UTF-16 so
// substring(0, character) matches JS. Section markers are ASCII-only, so the
// marker scan runs on UTF-8 bytes. Do not use parseFileSections /
// sectionNameForLine: this helper pairs nearest markers instead.

namespace vertexadt {
namespace {

// /^\s*debug\s+$/ on the whole prefix (lowercase, JS \s).
bool isDebugFilenameSite(std::u16string_view prefix)
{
  std::size_t i = 0;
  while (i < prefix.size() && isJsSpace(prefix[i]))
  {
    ++i;
  }
  if (prefix.substr(i, 5) != u"debug")
  {
    return false;
  }
  i += 5;
  if (i >= prefix.size())
  {
    return false;
  }
  for (; i < prefix.size(); ++i)
  {
    if (!isJsSpace(prefix[i]))
    {
      return false;
    }
  }
  return true;
}

bool isSectionNameChar(char ch)
{
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
    || (ch >= '0' && ch <= '9') || ch == '_';
}

// Successive non-overlapping /#([A-Za-z0-9_]+)#/g matches on one line.
// `last` true keeps the final match, false stops at the first.
auto sectionMarker(std::string_view line, bool last) -> std::optional<std::string_view>
{
  std::optional<std::string_view> found;
  std::size_t i = 0;
  while (i < line.size())
  {
    if (line[i] != '#')
    {
      ++i;
      continue;
    }
    auto j = i + 1;
    while (j < line.size() && isSectionNameChar(line[j]))
    {
      ++j;
    }
    if (j > i + 1 && j < line.size() && line[j] == '#')
    {
      found = line.substr(i + 1, j - i - 1);
      if (!last)
      {
        return found;
      }
      i = j + 1;
      continue;
    }
    ++i;
  }
  return found;
}

auto enclosingSectionName(const std::vector<std::string_view>& lines, int debugLine)
  -> std::optional<std::string>
{
  std::optional<std::string_view> previous;
  for (int line = debugLine - 1; line >= 0 && !previous; --line)
  {
    previous = sectionMarker(lines[static_cast<std::size_t>(line)], true);
  }
  std::optional<std::string_view> next;
  for (auto line = static_cast<std::size_t>(debugLine) + 1; line < lines.size() && !next; ++line)
  {
    next = sectionMarker(lines[line], false);
  }
  if (!previous || !next || toLowerAscii(*previous) != toLowerAscii(*next))
  {
    return std::nullopt;
  }
  return std::string(*previous);
}

// path.parse(document.fileName).base on win32: after the last `\` or `/`.
auto documentBase(const lsp::Uri& uri) -> std::string
{
  if (!uri.isValid())
  {
    return {};
  }
  auto fsPath = uri.fsPath();
  while (!fsPath.empty() && (fsPath.back() == '\\' || fsPath.back() == '/'))
  {
    fsPath.pop_back();
  }
  const auto slash = fsPath.find_last_of("\\/");
  return slash == std::string::npos ? fsPath : fsPath.substr(slash + 1);
}

} // namespace

auto debugFilenameCompletionItems(const Snapshot& snapshot, lsp::Position position)
  -> std::vector<lsp::CompletionItem>
{
  if (snapshot.languageId != "vertexadt")
  {
    return {};
  }
  auto lines = splitLines(snapshot.text);
  for (auto& row : lines)
  {
    if (!row.empty() && row.back() == '\r')
    {
      row.remove_suffix(1);
    }
  }
  const auto lineIndex = static_cast<std::size_t>(position.line);
  if (lineIndex >= lines.size())
  {
    return {};
  }
  auto prefix = utf8To16(lines[lineIndex]);
  if (prefix.size() > position.character)
  {
    prefix.resize(position.character);
  }
  if (!isDebugFilenameSite(prefix))
  {
    return {};
  }
  const auto base = documentBase(snapshot.uri);
  if (base.empty())
  {
    return {};
  }
  const auto section = enclosingSectionName(lines, static_cast<int>(position.line));
  auto item = lsp::CompletionItem();
  item.label = section ? "./_" + base + "_" + *section + ".dbg" : "./_" + base + ".dbg";
  item.kind = lsp::CompletionItemKind::File;
  item.insertText = item.label;
  item.detail = section
    ? "Debug output file for current document and section " + *section
    : std::string("Debug output file for current document");
  item.sortText = "0";
  return {std::move(item)};
}

} // namespace vertexadt
