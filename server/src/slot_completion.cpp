#include "slot_completion.h"

#include "utf16_text.h"

#include <windows.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <utility>
#include <variant>

namespace vertexadt {
namespace {

auto foldCase(U16View text) -> std::u16string
{
  std::u16string out(text);
  if (out.empty())
  {
    return out;
  }
  const auto size = static_cast<int>(out.size());
  const auto written = LCMapStringEx(
    LOCALE_NAME_INVARIANT,
    LCMAP_LOWERCASE,
    reinterpret_cast<LPCWSTR>(text.data()),
    size,
    reinterpret_cast<LPWSTR>(out.data()),
    size,
    nullptr,
    nullptr,
    0);
  if (written != size)
  {
    out.assign(text);
    for (auto& ch : out)
    {
      ch = asciiLower(ch);
    }
  }
  return out;
}

// The item's plain TextEdit range (the families never send InsertReplaceEdit).
auto editRange(lsp::CompletionItem& item) -> lsp::Range*
{
  if (!item.textEdit)
  {
    return nullptr;
  }
  auto* edit = std::get_if<lsp::TextEdit>(&*item.textEdit);
  return edit ? &edit->range : nullptr;
}

// VS Code's filter text for `item`: from its textEdit range start (same line,
// at or before the cursor) to the cursor, else the typed word.
auto filterTextFor(lsp::CompletionItem& item, const SlotTyping& typing) -> U16View
{
  const U16View line(typing.line);
  auto start = typing.wordStart;
  const auto* range = editRange(item);
  if (range && range->start.line == typing.lineNumber && range->start.character <= typing.cursor)
  {
    start = range->start.character;
  }
  return line.substr(start, typing.cursor - start);
}

// Leading spaces and quotes are not part of what the user is looking for.
auto matchTextOf(U16View filter) -> U16View
{
  std::size_t i = 0;
  while (i < filter.size() && (isJsSpace(filter[i]) || filter[i] == u'"' || filter[i] == u'\''))
  {
    ++i;
  }
  return filter.substr(i);
}

struct CacheEntry
{
  std::string key;
  std::size_t cursor = 0;
  std::size_t lineSize = 0;
  std::vector<lsp::CompletionItem> items;
};

std::mutex cacheMutex;
std::map<std::pair<std::string, char>, CacheEntry> cache;

} // namespace

auto foldSlotCase(U16View text) -> std::u16string
{
  return foldCase(text);
}

auto SlotTyping::cursorPosition() const -> lsp::Position
{
  return {.line = lineNumber, .character = static_cast<lsp::Uint>(cursor)};
}

auto SlotTyping::wordStartPosition() const -> lsp::Position
{
  return {.line = lineNumber, .character = static_cast<lsp::Uint>(wordStart)};
}

auto slotTyping(const Snapshot& snapshot, lsp::Position position) -> SlotTyping
{
  SlotTyping typing;
  typing.lineNumber = position.line;
  if (const auto row = lineAt(snapshot.text, position.line))
  {
    typing.line = utf8To16(*row);
  }
  typing.cursor = std::min<std::size_t>(position.character, typing.line.size());
  auto start = typing.cursor;
  // `\w+\$?`: an optional trailing `$` right before the cursor, after a word.
  if (start > 0 && typing.line[start - 1] == u'$' && start > 1
      && isJsWord(typing.line[start - 2]))
  {
    --start;
  }
  while (start > 0 && isJsWord(typing.line[start - 1]))
  {
    --start;
  }
  typing.wordStart = start;
  return typing;
}

auto slotSitePositions(const SlotTyping& typing) -> std::vector<lsp::Position>
{
  if (!typing.hasTypedWord())
  {
    return {typing.cursorPosition()};
  }
  return {typing.cursorPosition(), typing.wordStartPosition()};
}

auto shapeSlotItems(std::vector<lsp::CompletionItem> items, const SlotTyping& typing)
  -> std::vector<lsp::CompletionItem>
{
  std::vector<lsp::CompletionItem> kept;
  kept.reserve(items.size());
  for (auto& item : items)
  {
    if (auto* range = editRange(item);
        range && range->end.line == typing.lineNumber && range->end.character < typing.cursor)
    {
      range->end.character = static_cast<lsp::Uint>(typing.cursor);
    }
    const auto filter = filterTextFor(item, typing);
    const auto needle = foldCase(matchTextOf(filter));
    char group = '0';
    if (!needle.empty())
    {
      const auto at = foldCase(utf8To16(item.label)).find(needle);
      if (at == std::u16string::npos)
      {
        continue;
      }
      group = at == 0 ? '0' : '1';
    }
    if (!filter.empty())
    {
      item.filterText = utf16To8(filter);
    }
    item.sortText = std::string(1, group) + (item.sortText ? *item.sortText : item.label);
    kept.push_back(std::move(item));
  }
  return kept;
}

bool isOnlyExactMatch(const std::vector<lsp::CompletionItem>& items, const SlotTyping& typing)
{
  if (items.size() != 1)
  {
    return false;
  }
  auto item = items.front();
  return utf16To8(matchTextOf(filterTextFor(item, typing))) == item.label;
}

auto slotListingKey(const SlotTyping& typing) -> std::string
{
  return std::to_string(typing.lineNumber) + ':'
    + utf16To8(U16View(typing.line).substr(0, typing.wordStart));
}

auto cachedSlotListing(
  std::string_view uri,
  CompletionFamily family,
  const std::string& key,
  const SlotTyping& typing,
  bool reuse,
  const std::function<std::optional<std::vector<lsp::CompletionItem>>()>& compute)
  -> std::optional<std::vector<lsp::CompletionItem>>
{
  const auto slot = std::pair{std::string(uri), static_cast<char>(family)};
  if (reuse)
  {
    std::lock_guard lock(cacheMutex);
    const auto found = cache.find(slot);
    if (found != cache.end() && found->second.key == key)
    {
      const auto& entry = found->second;
      auto items = entry.items;
      for (auto& item : items)
      {
        auto* range = editRange(item);
        if (range && range->end.line == typing.lineNumber && range->end.character >= entry.cursor)
        {
          const auto end = range->end.character + typing.line.size();
          range->end.character = static_cast<lsp::Uint>(
            end > entry.lineSize + typing.cursor ? end - entry.lineSize : typing.cursor);
        }
      }
      return items;
    }
  }
  auto items = compute();
  if (items)
  {
    std::lock_guard lock(cacheMutex);
    cache[slot] = CacheEntry{
      .key = key, .cursor = typing.cursor, .lineSize = typing.line.size(), .items = *items};
  }
  return items;
}

} // namespace vertexadt
