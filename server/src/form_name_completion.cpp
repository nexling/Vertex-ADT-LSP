#include "form_name_completion.h"

#include "text_util.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Port of server/out/formNameCompletion.js (analyzeFormNameCompletion,
// collectFormNameEntries, resolveFormsDirectory) and the client host's item
// mapping, with Node's custom/getFormNames folded into ordinary completion.
// The line is scanned in UTF-16 code units so columns match LSP positions. The
// command regex is matched by hand.

namespace vertexadt {
namespace {

constexpr std::array<std::u16string_view, 4> kCommands = {
  u"edit_obj_data", u"edit_jnt_data", u"dbedit", u"set_macro_form"};

struct CommandMatch
{
  std::u16string_view name;
  std::size_t openParen = 0;
};

struct ParsedArg
{
  std::size_t start = 0;
  std::size_t end = 0;
};

struct ParsedCall
{
  std::vector<ParsedArg> args;
  std::ptrdiff_t closeParen = -1;
};

bool isQuote(char16_t ch)
{
  return ch == u'"' || ch == u'\'';
}

// isCursorInComment: `;` outside quotes in [0, cursor).
bool cursorInComment(U16View line, std::size_t cursor)
{
  char16_t quote = 0;
  for (std::size_t i = 0; i < cursor; ++i)
  {
    const auto ch = line[i];
    if (quote)
    {
      if (ch == quote)
      {
        quote = 0;
      }
      continue;
    }
    if (isQuote(ch))
    {
      quote = ch;
      continue;
    }
    if (ch == u';')
    {
      return true;
    }
  }
  return false;
}

// One exec of /\b(edit_obj_data|edit_jnt_data|dbedit|set_macro_form)\s*\(/i at
// `at`: the match end (one past `(`) and the alternative, or nullopt.
auto matchCommandAt(U16View line, std::size_t at)
  -> std::optional<std::pair<std::size_t, std::u16string_view>>
{
  if (at > 0 && isJsWord(line[at - 1]))
  {
    return std::nullopt;
  }
  for (const auto name : kCommands)
  {
    if (at + name.size() > line.size())
    {
      continue;
    }
    bool same = true;
    for (std::size_t k = 0; k < name.size() && same; ++k)
    {
      same = asciiLower(line[at + k]) == name[k];
    }
    if (!same)
    {
      continue;
    }
    const auto paren = skipJsSpace(line, at + name.size());
    if (paren < line.size() && line[paren] == u'(')
    {
      return std::pair{paren + 1, name};
    }
  }
  return std::nullopt;
}

// findLastCommandBeforeCursor: global exec loop, last match whose start and `(`
// are both before the cursor.
auto findLastCommandBeforeCursor(U16View line, std::size_t cursor) -> std::optional<CommandMatch>
{
  std::optional<CommandMatch> found;
  std::size_t from = 0;
  while (from < line.size())
  {
    std::optional<std::pair<std::size_t, std::u16string_view>> match;
    std::size_t index = from;
    for (; index < line.size(); ++index)
    {
      match = matchCommandAt(line, index);
      if (match)
      {
        break;
      }
    }
    if (!match)
    {
      break;
    }
    const auto openParen = match->first - 1;
    if (index < cursor && openParen < cursor)
    {
      found = CommandMatch{.name = match->second, .openParen = openParen};
    }
    if (index >= cursor)
    {
      break;
    }
    from = match->first;
  }
  return found;
}

// parseCallArguments: depth + quotes; top-level commas split, the matching `)`
// closes. Unclosed: the last argument runs to EOL.
auto parseCallArguments(U16View line, std::size_t openParen) -> ParsedCall
{
  ParsedCall parsed;
  parsed.args.push_back({.start = openParen + 1, .end = openParen + 1});
  int depth = 1;
  char16_t quote = 0;
  for (std::size_t i = openParen + 1; i < line.size() && depth > 0; ++i)
  {
    const auto ch = line[i];
    if (quote)
    {
      if (ch == quote)
      {
        quote = 0;
      }
      continue;
    }
    if (isQuote(ch))
    {
      quote = ch;
      continue;
    }
    if (ch == u'(')
    {
      ++depth;
      continue;
    }
    if (ch == u')')
    {
      if (--depth == 0)
      {
        parsed.args.back().end = i;
        parsed.closeParen = static_cast<std::ptrdiff_t>(i);
        return parsed;
      }
      continue;
    }
    if (ch == u',' && depth == 1)
    {
      parsed.args.back().end = i;
      parsed.args.push_back({.start = i + 1, .end = i + 1});
    }
  }
  parsed.args.back().end = line.size();
  return parsed;
}

auto argumentIndexAtCursor(const ParsedCall& parsed, std::size_t cursor) -> std::ptrdiff_t
{
  if (parsed.closeParen >= 0 && static_cast<std::ptrdiff_t>(cursor) > parsed.closeParen)
  {
    return -1;
  }
  for (std::size_t i = 0; i < parsed.args.size(); ++i)
  {
    if (cursor >= parsed.args[i].start && cursor <= parsed.args[i].end)
    {
      return static_cast<std::ptrdiff_t>(i);
    }
  }
  return -1;
}

// stringLiteralContext. Only space and tab are skipped (not JS `\s`).
auto stringLiteralContext(U16View line, std::size_t regionStart, std::size_t cursor)
  -> std::optional<FormNameSite>
{
  auto i = regionStart;
  while (i < cursor && (line[i] == u' ' || line[i] == u'\t'))
  {
    ++i;
  }
  if (i >= cursor)
  {
    return FormNameSite{.insideQuotes = false, .replaceStart = cursor, .replaceEnd = cursor};
  }
  const auto quote = line[i];
  if (!isQuote(quote))
  {
    return std::nullopt;
  }
  const auto contentStart = i + 1;
  const auto close = line.find(quote, contentStart);
  if (close != U16View::npos && cursor > close)
  {
    return std::nullopt;
  }
  return FormNameSite{
    .insideQuotes = true,
    .replaceStart = contentStart,
    .replaceEnd = close != U16View::npos ? close : std::max(cursor, contentStart),
  };
}

// String.prototype.toLowerCase: full Unicode simple lowercase (a-ring etc. matter in
// Swedish form names), not ASCII-only.
auto jsToLower(const std::string& text) -> std::string
{
  auto wide = utf8To16(text);
  if (wide.empty())
  {
    return {};
  }
  const auto size = static_cast<int>(wide.size());
  std::u16string out(wide.size(), u'\0');
  const auto written = LCMapStringEx(
    LOCALE_NAME_INVARIANT,
    LCMAP_LOWERCASE,
    reinterpret_cast<LPCWSTR>(wide.data()),
    size,
    reinterpret_cast<LPWSTR>(out.data()),
    size,
    nullptr,
    nullptr,
    0);
  if (written <= 0)
  {
    return toLowerAscii(text);
  }
  out.resize(static_cast<std::size_t>(written));
  return utf16To8(out);
}

auto pathToUtf8(const std::filesystem::path& path) -> std::string
{
  const auto text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

// fs.existsSync && statSync().isDirectory(): follows symlinks.
bool isExistingDirectory(const std::filesystem::path& path)
{
  std::error_code ec;
  return !path.empty() && std::filesystem::is_directory(path, ec) && !ec;
}

// listTopLevelFileNames: Dirent.isFile() does not follow symlinks.
auto listTopLevelFileNames(const std::filesystem::path& dir) -> std::vector<std::string>
{
  std::vector<std::string> names;
  if (!isExistingDirectory(dir))
  {
    return names;
  }
  std::error_code ec;
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
  {
    std::error_code statusEc;
    const auto status = it->symlink_status(statusEc);
    if (!statusEc && std::filesystem::is_regular_file(status))
    {
      names.push_back(pathToUtf8(it->path().filename()));
    }
  }
  if (ec)
  {
    return {};
  }
  return names;
}

// Approximates Node's localeCompare(..., { sensitivity: 'base' }) (ICU root):
// whitespace, punctuation and symbols in this order, then digits, then letters
// case-folded. Other code units sort after, by value.
auto primaryWeight(char16_t ch) -> int
{
  constexpr std::u16string_view kOrder =
    u" _-,;:!?.'\"()[]{}@*/\\&#%`^+<=>|~$0123456789abcdefghijklmnopqrstuvwxyz";
  const auto lowered = asciiLower(ch);
  const auto at = kOrder.find(lowered);
  if (at != std::u16string_view::npos)
  {
    return static_cast<int>(at);
  }
  return static_cast<int>(kOrder.size()) + static_cast<int>(ch);
}

// Primary-strength collation key: FoldString(MAP_COMPOSITE) splits accented
// letters into base + combining marks, and the marks are dropped, so `Ä` ties
// with `a` like ICU's base strength does.
auto primaryKey(const std::string& text) -> std::vector<int>
{
  const auto wide = utf8To16(text);
  std::u16string folded;
  if (!wide.empty())
  {
    const auto size = static_cast<int>(wide.size());
    const auto needed =
      FoldStringW(MAP_COMPOSITE, reinterpret_cast<LPCWSTR>(wide.data()), size, nullptr, 0);
    if (needed > 0)
    {
      folded.resize(static_cast<std::size_t>(needed));
      const auto written = FoldStringW(
        MAP_COMPOSITE,
        reinterpret_cast<LPCWSTR>(wide.data()),
        size,
        reinterpret_cast<LPWSTR>(folded.data()),
        needed);
      folded.resize(written > 0 ? static_cast<std::size_t>(written) : 0);
    }
    if (folded.empty())
    {
      folded = wide;
    }
  }
  std::vector<int> key;
  key.reserve(folded.size());
  for (const auto ch : folded)
  {
    if (ch >= 0x0300 && ch <= 0x036F)
    {
      continue;
    }
    key.push_back(primaryWeight(ch));
  }
  return key;
}

auto filenameOf(const std::filesystem::path& path) -> std::string
{
  // path.basename ignores a trailing separator.
  auto copy = path;
  if (!copy.has_filename() && copy.has_parent_path())
  {
    copy = copy.parent_path();
  }
  return pathToUtf8(copy.filename());
}

bool startsWithCustomInsensitive(std::string_view name)
{
  return name.size() >= 6 && toLowerAscii(name.substr(0, 6)) == "custom";
}

bool isSystemInsensitive(std::string_view name)
{
  return toLowerAscii(name) == "system";
}

auto lineAndCursor(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<std::pair<U16, std::size_t>>
{
  if (snapshot.languageId != "vertexadt")
  {
    return std::nullopt;
  }
  const auto row = lineAt(snapshot.text, position.line);
  if (!row)
  {
    return std::nullopt;
  }
  auto line = utf8To16(*row);
  const auto cursor = std::min<std::size_t>(position.character, line.size());
  return std::pair{std::move(line), cursor};
}

} // namespace

auto analyzeFormNameCompletion(U16View line, std::size_t cursor) -> std::optional<FormNameSite>
{
  cursor = std::min(cursor, line.size());
  if (cursorInComment(line, cursor))
  {
    return std::nullopt;
  }
  const auto command = findLastCommandBeforeCursor(line, cursor);
  if (!command)
  {
    return std::nullopt;
  }
  const auto parsed = parseCallArguments(line, command->openParen);
  if (command->name == u"set_macro_form")
  {
    if (parsed.closeParen < 0 || static_cast<std::ptrdiff_t>(cursor) <= parsed.closeParen)
    {
      return std::nullopt;
    }
    return stringLiteralContext(line, static_cast<std::size_t>(parsed.closeParen) + 1, cursor);
  }
  if (parsed.closeParen >= 0 && static_cast<std::ptrdiff_t>(cursor) > parsed.closeParen)
  {
    return std::nullopt;
  }
  if (argumentIndexAtCursor(parsed, cursor) != 1)
  {
    return std::nullopt;
  }
  return stringLiteralContext(line, parsed.args[1].start, cursor);
}

auto formNameSite(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<FormNameSite>
{
  const auto located = lineAndCursor(snapshot, position);
  if (!located)
  {
    return std::nullopt;
  }
  return analyzeFormNameCompletion(located->first, located->second);
}

auto collectFormNameEntries(
  const std::filesystem::path& customFormsDir,
  const std::filesystem::path& systemFormsDir) -> std::vector<FormNameEntry>
{
  std::vector<FormNameEntry> merged;
  std::unordered_set<std::string> seen;
  const auto append = [&](std::vector<std::string> names, std::string_view origin)
  {
    for (auto& name : names)
    {
      if (name.empty() || !seen.insert(jsToLower(name)).second)
      {
        continue;
      }
      merged.push_back({.name = std::move(name), .origin = std::string(origin)});
    }
  };
  append(listTopLevelFileNames(customFormsDir), "custom/forms");
  append(listTopLevelFileNames(systemFormsDir), "system/forms");
  std::vector<std::pair<std::vector<int>, FormNameEntry>> keyed;
  keyed.reserve(merged.size());
  for (auto& entry : merged)
  {
    keyed.emplace_back(primaryKey(entry.name), std::move(entry));
  }
  // Equal keys keep merge order (Custom first), like Array.prototype.sort.
  std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });
  merged.clear();
  for (auto& [key, entry] : keyed)
  {
    merged.push_back(std::move(entry));
  }
  return merged;
}

auto resolveFormsDirectory(LibraryTree tree, const LibraryLayout& layout)
  -> std::filesystem::path
{
  return resolveLibrarySubdirectory(tree, layout, "forms");
}

auto resolveLibrarySubdirectory(LibraryTree tree, const LibraryLayout& layout, std::string_view subdir)
  -> std::filesystem::path
{
  const auto sub = std::filesystem::u8path(subdir);
  const auto& root = layout.workspaceRoot();
  std::vector<std::filesystem::path> candidates;
  for (const auto& folder : layout.folders())
  {
    const auto folderPath = pathFromUri(folder.uri);
    if (folderPath.empty())
    {
      continue;
    }
    const auto base = filenameOf(folderPath);
    if (tree == LibraryTree::Custom ? startsWithCustomInsensitive(base) : isSystemInsensitive(base))
    {
      candidates.push_back(folderPath / sub);
    }
  }
  if (!root.empty())
  {
    const bool rootIsCustom = startsWithCustomInsensitive(filenameOf(root));
    if (tree == LibraryTree::Custom)
    {
      candidates.push_back(
        rootIsCustom ? root / sub
                     : root / std::filesystem::u8path(layout.customFolderName()) / sub);
    }
    else
    {
      candidates.push_back(root / "system" / sub);
      if (rootIsCustom)
      {
        candidates.push_back(root.parent_path() / "system" / sub);
      }
    }
  }
  for (const auto& candidate : candidates)
  {
    if (isExistingDirectory(candidate))
    {
      return candidate;
    }
  }
  return {};
}

auto formNameCompletionItems(
  const FormNameSite& site,
  lsp::Position position,
  const LibraryLayout& layout) -> std::vector<lsp::CompletionItem>
{
  const auto entries = collectFormNameEntries(
    resolveFormsDirectory(LibraryTree::Custom, layout),
    resolveFormsDirectory(LibraryTree::System, layout));
  const lsp::Range range{
    .start = {.line = position.line, .character = static_cast<lsp::Uint>(site.replaceStart)},
    .end = {.line = position.line, .character = static_cast<lsp::Uint>(site.replaceEnd)},
  };
  std::vector<lsp::CompletionItem> items;
  items.reserve(entries.size());
  for (const auto& entry : entries)
  {
    const auto insert = site.insideQuotes ? entry.name : "\"" + entry.name + "\"";
    lsp::CompletionItem item;
    item.label = entry.name;
    item.kind = lsp::CompletionItemKind::File;
    item.detail = entry.origin;
    item.filterText = entry.name;
    item.sortText = jsToLower(entry.name);
    item.insertText = insert;
    item.textEdit = lsp::TextEdit{.range = range, .newText = insert};
    items.push_back(std::move(item));
  }
  return items;
}

} // namespace vertexadt
