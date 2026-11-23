#pragma once

#include "completion_ranking.h"
#include "document_store.h"

#include <lsp/types.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Substring matching for slot completion families
// (docs/adr/0002-native-slot-completion-substring-match.md). In a slot the
// server filters family items itself on every keystroke (the response is
// `isIncomplete`): an item stays when its label contains the typed text
// anywhere, case-insensitively, and its filterText becomes VS Code's typed
// text so the client keeps it. Start-of-label matches sort before the rest.

// Invariant-locale lowercase used by slot substring matching.
[[nodiscard]] auto foldSlotCase(std::u16string_view text) -> std::u16string;

// The cursor's line and the `\w+\$?` word (language-configuration.json
// wordPattern) that ends at the cursor. UTF-16 columns.
struct SlotTyping
{
  std::u16string line;
  lsp::Uint lineNumber = 0;
  std::size_t cursor = 0;
  // == cursor when no word ends at the cursor.
  std::size_t wordStart = 0;

  [[nodiscard]] auto cursorPosition() const -> lsp::Position;
  [[nodiscard]] auto wordStartPosition() const -> lsp::Position;
  [[nodiscard]] bool hasTypedWord() const { return wordStart < cursor; }
};

[[nodiscard]] auto slotTyping(const Snapshot& snapshot, lsp::Position position) -> SlotTyping;

// Sites are detected at the cursor first (families that already accept a
// partly typed token, such as quoted form names and include/call paths, keep
// their cursor behavior), then at the typed word's start (so `new_macro(cor`
// is the `new_macro(` slot).
[[nodiscard]] auto slotSitePositions(const SlotTyping& typing) -> std::vector<lsp::Position>;

// Stretches textEdit ranges that end before the cursor to it, keeps the items
// whose label contains the typed text (VS Code's filter text: the textEdit
// range start to the cursor, else the typed word; leading spaces and quotes
// ignored for matching), sets filterText to VS Code's filter text when it is
// non-empty, and sets sortText to group + key: group `0` for a start-of-label
// match or nothing typed, `1` elsewhere; key is the item's own sortText, else
// its label. Order is kept.
[[nodiscard]] auto shapeSlotItems(std::vector<lsp::CompletionItem> items, const SlotTyping& typing)
  -> std::vector<lsp::CompletionItem>;

// Cache key for the slot being typed in: line number and the line text
// before the typed word. A family listing does not depend on the typed word
// itself (cached textEdit ranges are stretched to the cursor on reuse).
[[nodiscard]] auto slotListingKey(const SlotTyping& typing) -> std::string;

// Whether reopening the popup would only offer what is already typed: the one
// shaped item's label is the typed text, case included (docs/adr/0021).
[[nodiscard]] bool isOnlyExactMatch(
  const std::vector<lsp::CompletionItem>& items,
  const SlotTyping& typing);

// The last raw listing per (document, family). `reuse` (a re-request for an
// incomplete list) returns the stored listing when its key matches, with
// textEdit ends at or after the stored cursor moved by the line's growth or
// shrinkage since (text right of the slot keeps its place); otherwise
// `compute` runs and its listing is stored. Nullopt (canceled) is not stored.
[[nodiscard]] auto cachedSlotListing(
  std::string_view uri,
  CompletionFamily family,
  const std::string& key,
  const SlotTyping& typing,
  bool reuse,
  const std::function<std::optional<std::vector<lsp::CompletionItem>>()>& compute)
  -> std::optional<std::vector<lsp::CompletionItem>>;

} // namespace vertexadt
