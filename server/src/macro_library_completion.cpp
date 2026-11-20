#include "macro_library_completion.h"

#include "macro_library_xml.h"
#include "slot_completion.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <map>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

// Ports of extension.js libraryCodeCommands (the site) and server.js
// custom/getMacroLibraries + custom/getMacroCodes (the listings), folded into
// ordinary textDocument/completion.

namespace vertexadt {
namespace {

constexpr std::array<std::u16string_view, 3> kCommands{
  u"new_macro(",
  u"new_djoint(",
  u"new_sheet(",
};

auto skipSpace(U16View text, std::size_t i) -> std::size_t
{
  return skipJsSpace(text, i);
}

// `"([^"]*)"|'([^']*)'` at `i`. Returns the content and the index past the
// closing quote; nullopt when `i` is not a quote or it never closes.
auto quoted(U16View text, std::size_t i) -> std::optional<std::pair<U16View, std::size_t>>
{
  if (i >= text.size() || (text[i] != u'"' && text[i] != u'\''))
  {
    return std::nullopt;
  }
  const auto close = text.find(text[i], i + 1);
  if (close == U16View::npos)
  {
    return std::nullopt;
  }
  return std::pair{text.substr(i + 1, close - i - 1), close + 1};
}

struct TailMatch
{
  std::optional<U16View> library;
};

// `\s*(?:Q1)?(?:\s*,\s*(?:Q2)?)?\s*$` from `i`. Every branch is decided by the
// next non-space character, so one greedy pass is the regex's answer.
auto matchTail(U16View text, std::size_t i) -> std::optional<TailMatch>
{
  TailMatch match;
  i = skipSpace(text, i);
  if (const auto first = quoted(text, i))
  {
    match.library = first->first;
    i = skipSpace(text, first->second);
  }
  if (i == text.size())
  {
    return match;
  }
  if (text[i] != u',')
  {
    return std::nullopt;
  }
  i = skipSpace(text, i + 1);
  if (const auto second = quoted(text, i))
  {
    i = skipSpace(text, second->second);
  }
  if (i != text.size())
  {
    return std::nullopt;
  }
  return match;
}

// String.prototype.toUpperCase for directory names.
auto jsToUpper(const std::string& text) -> std::string
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
    LCMAP_UPPERCASE,
    reinterpret_cast<LPCWSTR>(wide.data()),
    size,
    reinterpret_cast<LPWSTR>(out.data()),
    size,
    nullptr,
    nullptr,
    0);
  if (written <= 0)
  {
    auto ascii = text;
    for (auto& ch : ascii)
    {
      if (ch >= 'a' && ch <= 'z')
      {
        ch = static_cast<char>(ch - 'a' + 'A');
      }
    }
    return ascii;
  }
  out.resize(static_cast<std::size_t>(written));
  return utf16To8(out);
}

auto pathUtf8(const std::filesystem::path& path) -> std::string
{
  const auto text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

bool isQuote(char16_t ch)
{
  return ch == u'"' || ch == u'\'';
}

// Code-first 1st argument still being typed, from `i` to the end: nothing, an
// unquoted run without quotes, commas or parentheses, or an opening quote
// with no closing quote and no comma.
bool codeFirstTail(U16View text, std::size_t i)
{
  i = skipSpace(text, i);
  if (i == text.size())
  {
    return true;
  }
  const auto rest = text.substr(i);
  if (isQuote(rest[0]))
  {
    const auto body = rest.substr(1);
    return body.find(rest[0]) == U16View::npos && body.find(u',') == U16View::npos;
  }
  return rest.find_first_of(u"\"',()") == U16View::npos;
}

// End of the argument starting at `i` (after spaces): past its closing quote
// when quoted and closed, else before the next `,` / `)` with trailing spaces
// dropped.
auto argumentEnd(U16View line, std::size_t i) -> std::size_t
{
  if (i < line.size() && isQuote(line[i]))
  {
    const auto close = line.find(line[i], i + 1);
    if (close != U16View::npos)
    {
      return close + 1;
    }
  }
  auto j = i;
  while (j < line.size() && line[j] != u',' && line[j] != u')')
  {
    ++j;
  }
  while (j > i && isJsSpace(line[j - 1]))
  {
    --j;
  }
  return j;
}

auto directorySubdirs(const std::filesystem::path& root) -> std::vector<std::filesystem::path>
{
  std::vector<std::filesystem::path> dirs;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
  {
    std::error_code typeEc;
    if (it->is_directory(typeEc) && !it->is_symlink(typeEc))
    {
      dirs.push_back(it->path());
    }
  }
  return dirs;
}

auto exportRequest(const MacroLibraryCompletionRequest& request, std::filesystem::path libraryPath)
  -> MacroLibraryExportRequest
{
  return {
    .libraryPath = std::move(libraryPath),
    .extensionUri = request.extensionUri,
    .isCanceled = request.isCanceled,
    .log = request.log,
  };
}

} // namespace

auto analyzeMacroLibrarySite(U16View prefix, bool codeFirst) -> std::optional<MacroLibrarySite>
{
  for (const auto command : kCommands)
  {
    // Unanchored: the leftmost occurrence whose tail reaches the end wins.
    for (auto at = prefix.find(command); at != U16View::npos; at = prefix.find(command, at + 1))
    {
      const auto argStart = at + command.size();
      if (codeFirst && codeFirstTail(prefix, argStart))
      {
        return MacroLibrarySite{
          .librarySlot = true, .library = {}, .codeFirst = true, .argStart = argStart};
      }
      const auto tail = matchTail(prefix, argStart);
      if (!tail)
      {
        continue;
      }
      const bool librarySlot = !tail->library && prefix.find(u',') == U16View::npos;
      if (librarySlot)
      {
        return MacroLibrarySite{.librarySlot = true, .library = {}};
      }
      if (!tail->library || tail->library->empty())
      {
        return std::nullopt;
      }
      return MacroLibrarySite{.librarySlot = false, .library = utf16To8(*tail->library)};
    }
  }
  return std::nullopt;
}

auto macroLibrarySite(const Snapshot& snapshot, lsp::Position position, bool codeFirst)
  -> std::optional<MacroLibrarySite>
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
  const auto line = utf8To16(*row);
  const auto cursor = std::min<std::size_t>(position.character, line.size());
  return analyzeMacroLibrarySite(U16View(line).substr(0, cursor), codeFirst);
}

auto macroCodeFirstEditEnd(U16View line, std::size_t argStart, std::size_t cursor) -> std::size_t
{
  const auto size = line.size();
  cursor = std::clamp(cursor, (std::min)(argStart, size), size);
  const auto first = skipSpace(line, argStart);
  std::size_t end = cursor;
  if (first < cursor && isQuote(line[first]))
  {
    // An opening quote before the cursor (never closed there): its closing
    // quote, unless a `,` / `)` comes first.
    auto j = cursor;
    while (j < size && line[j] != line[first] && line[j] != u',' && line[j] != u')')
    {
      ++j;
    }
    end = j < size && line[j] == line[first] ? j + 1 : j;
  }
  else if (first < cursor)
  {
    end = (std::max)(cursor, argumentEnd(line, first));
  }
  else
  {
    end = (std::max)(cursor, argumentEnd(line, skipSpace(line, cursor)));
  }
  const auto comma = skipSpace(line, end);
  if (comma < size && line[comma] == u',')
  {
    const auto second = skipSpace(line, comma + 1);
    if (second < size && line[second] != u',' && line[second] != u')')
    {
      return argumentEnd(line, second);
    }
    return second;
  }
  return end;
}

auto matchMacroCodeFirst(U16View typed, U16View library, U16View code) -> std::optional<bool>
{
  const auto needle = foldSlotCase(typed);
  if (needle.empty())
  {
    return true;
  }
  const auto lib = foldSlotCase(library);
  const auto cod = foldSlotCase(code);
  const auto slash = needle.find(u'/');
  if (slash == std::u16string::npos)
  {
    if (cod.find(needle) != std::u16string::npos)
    {
      return true;
    }
    if ((lib + u"/" + cod).find(needle) != std::u16string::npos)
    {
      return false;
    }
    return std::nullopt;
  }
  const auto left = U16View(needle).substr(0, slash);
  const auto right = U16View(needle).substr(slash + 1);
  if (lib.find(left) == std::u16string::npos || cod.find(right) == std::u16string::npos)
  {
    return std::nullopt;
  }
  return !right.empty();
}

auto listMacroLibraries(const MacroLibraryCompletionRequest& request)
  -> std::optional<std::vector<std::string>>
{
  std::vector<std::string> libraries;
  const auto complibs = customComplibsDirectory(request.layout);
  if (!complibs)
  {
    return libraries;
  }
  for (const auto& dir : directorySubdirs(*complibs))
  {
    if (request.isCanceled && request.isCanceled())
    {
      return std::nullopt;
    }
    if (ensureMacroLibraryXmlExports(exportRequest(request, dir))
        == MacroLibraryExportResult::Canceled)
    {
      return std::nullopt;
    }
    if (!macroLibraryXmlFiles(dir).empty())
    {
      libraries.push_back(jsToUpper(pathUtf8(dir.filename())));
    }
  }
  return libraries;
}

auto listMacroCodes(const MacroLibraryCompletionRequest& request)
  -> std::optional<std::vector<std::string>>
{
  std::vector<std::string> codes;
  const auto libraryPath = resolveMacroLibraryPath(request.site.library, request.layout);
  if (!libraryPath)
  {
    return codes;
  }
  if (ensureMacroLibraryXmlExports(exportRequest(request, *libraryPath))
      == MacroLibraryExportResult::Canceled)
  {
    return std::nullopt;
  }
  for (const auto& record : readMacroLibraryRecords(*libraryPath, request.site.library))
  {
    const auto cell = std::find_if(record.begin(), record.end(), [](const auto& c) {
      return c.name == "CODE" || c.name == "SHTCODE";
    });
    if (cell == record.end())
    {
      continue;
    }
    if (std::find(codes.begin(), codes.end(), cell->raw) == codes.end())
    {
      codes.push_back(cell->raw);
    }
  }
  return codes;
}

auto macroLibraryCompletionItems(const MacroLibraryCompletionRequest& request)
  -> std::optional<std::vector<lsp::CompletionItem>>
{
  const auto labels =
    request.site.librarySlot ? listMacroLibraries(request) : listMacroCodes(request);
  if (!labels)
  {
    return std::nullopt;
  }
  const auto kind = request.site.librarySlot ? lsp::CompletionItemKind::Folder
                                             : lsp::CompletionItemKind::Value;
  std::vector<lsp::CompletionItem> items;
  items.reserve(labels->size());
  for (const auto& label : *labels)
  {
    lsp::CompletionItem item;
    item.label = label;
    item.kind = kind;
    item.insertText = "\"" + label + "\"";
    items.push_back(std::move(item));
  }
  return items;
}

namespace {

// Code-first listing across every Custom library, plus the background export
// pass (docs/adr/0008-native-macro-code-first-completion.md).
class CodeFirstIndex
{
public:
  struct Library
  {
    std::string label;
    std::vector<std::string> codes;
  };

  struct Listing
  {
    std::vector<Library> libraries;
    // Libraries with no `d_*.xml` yet whose export is pending.
    std::size_t pending = 0;
  };

  CodeFirstIndex() = default;
  CodeFirstIndex(const CodeFirstIndex&) = delete;
  CodeFirstIndex& operator=(const CodeFirstIndex&) = delete;

  ~CodeFirstIndex()
  {
    stop_ = true;
    std::thread worker;
    {
      const auto lock = std::lock_guard(mutex_);
      worker = std::move(worker_);
    }
    // The pass takes mutex_ to finish, so join outside it.
    if (worker.joinable())
    {
      worker.join();
    }
  }

  auto list(
    const std::filesystem::path& complibs,
    std::string_view extensionUri,
    const std::function<void(std::string_view)>& log) -> Listing
  {
    Listing listing;
    std::vector<std::filesystem::path> toExport;
    for (const auto& dir : directorySubdirs(complibs))
    {
      const bool needsExport = hasExportableStaleDatabase(dir);
      if (needsExport)
      {
        toExport.push_back(dir);
      }
      const auto xmlFiles = macroLibraryXmlFiles(dir);
      if (xmlFiles.empty())
      {
        listing.pending += needsExport ? 1 : 0;
        continue;
      }
      listing.libraries.push_back({
        .label = jsToUpper(pathUtf8(dir.filename())),
        .codes = codesFor(dir, xmlFiles),
      });
    }
    if (!toExport.empty())
    {
      startPass(std::move(toExport), std::string(extensionUri), log);
    }
    return listing;
  }

private:
  struct CachedCodes
  {
    std::string stamp;
    std::vector<std::string> codes;
  };

  static auto writeTime(const std::filesystem::path& path)
    -> std::optional<std::filesystem::file_time_type>
  {
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec)
    {
      return std::nullopt;
    }
    return time;
  }

  // A stale primary database whose last export did not fail at its current
  // write time.
  bool hasExportableStaleDatabase(const std::filesystem::path& dir)
  {
    for (const auto& database : primaryMacroDatabaseFiles(dir))
    {
      if (!macroDatabaseXmlIsStale(database))
      {
        continue;
      }
      const auto time = writeTime(database);
      const auto lock = std::lock_guard(mutex_);
      const auto failed = failed_.find(database);
      if (failed == failed_.end() || failed->second != time)
      {
        return true;
      }
    }
    return false;
  }

  // Unique non-empty first `CODE`/`SHTCODE` per Record, as listMacroCodes,
  // re-read only when the folder's `d_*.xml` names or write times change.
  auto codesFor(const std::filesystem::path& dir, const std::vector<std::string>& xmlFiles)
    -> std::vector<std::string>
  {
    std::string stamp;
    for (const auto& name : xmlFiles)
    {
      const auto time = writeTime(dir / std::filesystem::path(utf8To16(name)));
      stamp += name + ':'
        + std::to_string(time ? time->time_since_epoch().count() : 0) + '\n';
    }
    {
      const auto lock = std::lock_guard(mutex_);
      const auto cached = codes_.find(dir);
      if (cached != codes_.end() && cached->second.stamp == stamp)
      {
        return cached->second.codes;
      }
    }
    std::vector<std::string> codes;
    for (const auto& record : readMacroLibraryRecords(dir, pathUtf8(dir.filename())))
    {
      const auto cell = std::find_if(record.begin(), record.end(), [](const auto& c) {
        return c.name == "CODE" || c.name == "SHTCODE";
      });
      if (cell == record.end() || cell->raw.empty())
      {
        continue;
      }
      if (std::find(codes.begin(), codes.end(), cell->raw) == codes.end())
      {
        codes.push_back(cell->raw);
      }
    }
    const auto lock = std::lock_guard(mutex_);
    codes_[dir] = CachedCodes{.stamp = std::move(stamp), .codes = codes};
    return codes;
  }

  // One pass at a time; a request while one runs starts nothing.
  void startPass(
    std::vector<std::filesystem::path> dirs,
    std::string extensionUri,
    std::function<void(std::string_view)> log)
  {
    const auto lock = std::lock_guard(mutex_);
    if (running_ || stop_)
    {
      return;
    }
    if (worker_.joinable())
    {
      worker_.join();
    }
    running_ = true;
    worker_ = std::thread(
      [this, dirs = std::move(dirs), extensionUri = std::move(extensionUri), log = std::move(log)]()
      {
        for (const auto& dir : dirs)
        {
          if (stop_)
          {
            break;
          }
          const auto result = ensureMacroLibraryXmlExports({
            .libraryPath = dir,
            .extensionUri = extensionUri,
            .isCanceled = [this]() { return stop_.load(); },
            .log = log,
          });
          if (result == MacroLibraryExportResult::Canceled)
          {
            break;
          }
          // Still stale after the attempt: failed at this write time.
          for (const auto& database : primaryMacroDatabaseFiles(dir))
          {
            if (macroDatabaseXmlIsStale(database))
            {
              const auto time = writeTime(database);
              const auto lock = std::lock_guard(mutex_);
              failed_[database] = time;
            }
          }
        }
        const auto lock = std::lock_guard(mutex_);
        running_ = false;
      });
  }

  std::mutex mutex_;
  std::atomic<bool> stop_ = false;
  bool running_ = false;
  std::thread worker_;
  std::map<std::filesystem::path, CachedCodes> codes_;
  std::map<std::filesystem::path, std::optional<std::filesystem::file_time_type>> failed_;
};

auto codeFirstIndex() -> CodeFirstIndex&
{
  static CodeFirstIndex index;
  return index;
}

auto quotedArgument(const std::string& text) -> std::string
{
  return "\"" + text + "\"";
}

} // namespace

auto macroCodeFirstCompletionItems(const MacroCodeFirstRequest& request)
  -> std::vector<lsp::CompletionItem>
{
  std::vector<lsp::CompletionItem> items;
  const auto complibs = customComplibsDirectory(request.layout);
  if (!complibs)
  {
    return items;
  }
  const auto listing = codeFirstIndex().list(*complibs, request.extensionUri, request.log);

  const auto argStart = (std::min)(request.site.argStart, request.line.size());
  const auto cursor = std::clamp(request.cursor, argStart, request.line.size());
  const auto end = macroCodeFirstEditEnd(request.line, argStart, cursor);
  const auto typedRaw = request.line.substr(argStart, cursor - argStart);
  // Leading spaces and the opening quote are not part of the search.
  auto typed = typedRaw;
  while (!typed.empty() && (isJsSpace(typed.front()) || isQuote(typed.front())))
  {
    typed.remove_prefix(1);
  }
  const auto filterText = utf16To8(typedRaw);
  const lsp::Range range{
    .start = {.line = request.lineNumber, .character = static_cast<lsp::Uint>(argStart)},
    .end = {.line = request.lineNumber, .character = static_cast<lsp::Uint>(end)},
  };

  struct Pair
  {
    const std::string* library;
    const std::string* code;
    std::u16string library16;
    std::u16string code16;
    std::u16string libraryKey;
    std::u16string codeKey;
  };
  std::vector<Pair> pairs;
  for (const auto& library : listing.libraries)
  {
    for (const auto& code : library.codes)
    {
      auto library16 = utf8To16(library.label);
      auto code16 = utf8To16(code);
      auto libraryKey = foldSlotCase(library16);
      auto codeKey = foldSlotCase(code16);
      pairs.push_back({
        .library = &library.label,
        .code = &code,
        .library16 = std::move(library16),
        .code16 = std::move(code16),
        .libraryKey = std::move(libraryKey),
        .codeKey = std::move(codeKey),
      });
    }
  }

  if (pairs.empty())
  {
    if (listing.pending > 0)
    {
      // Nothing listable yet: a placeholder whose edit leaves the line as is.
      lsp::CompletionItem item;
      item.label = "Exporting " + std::to_string(listing.pending) + " macro librar"
        + (listing.pending == 1 ? "y" : "ies") + "\xE2\x80\xA6";
      item.kind = lsp::CompletionItemKind::Text;
      item.textEdit = lsp::TextEdit{
        .range = {.start = range.start,
                  .end = {.line = request.lineNumber,
                          .character = static_cast<lsp::Uint>(cursor)}},
        .newText = filterText,
      };
      if (!filterText.empty())
      {
        item.filterText = filterText;
      }
      item.sortText = "0";
      items.push_back(std::move(item));
    }
    return items;
  }

  // Same code from different libraries side by side.
  std::stable_sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) {
    if (a.codeKey != b.codeKey)
    {
      return a.codeKey < b.codeKey;
    }
    if (a.libraryKey != b.libraryKey)
    {
      return a.libraryKey < b.libraryKey;
    }
    return *a.code < *b.code;
  });

  char rankText[16];
  for (std::size_t rank = 0; rank < pairs.size(); ++rank)
  {
    const auto& pair = pairs[rank];
    const auto codeMatched = matchMacroCodeFirst(typed, pair.library16, pair.code16);
    if (!codeMatched)
    {
      continue;
    }
    lsp::CompletionItem item;
    item.label = *pair.library + "/" + *pair.code;
    item.kind = lsp::CompletionItemKind::Value;
    item.textEdit = lsp::TextEdit{
      .range = range,
      .newText = quotedArgument(*pair.library) + ", " + quotedArgument(*pair.code),
    };
    if (!filterText.empty())
    {
      item.filterText = filterText;
    }
    std::snprintf(rankText, sizeof(rankText), "%06zu", rank);
    item.sortText = std::string(1, *codeMatched ? '0' : '1') + rankText;
    items.push_back(std::move(item));
  }
  return items;
}

} // namespace vertexadt
