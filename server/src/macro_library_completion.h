#pragma once

#include "document_store.h"
#include "library_paths.h"
#include "utf16_text.h"

#include <lsp/types.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Macro library/code completion. Ordinary textDocument/completion adds Custom
// `complibs/` library names (Folder) in the quoted 1st inside argument and
// unique `CODE`/`SHTCODE` cells (Value) in the quoted 2nd inside argument of
// lowercase glued `new_macro(` / `new_djoint(` / `new_sheet(`. Heavy: directory
// walk, Vertex Dbase Converter helper on stale primary `d_*`, XML parse.

struct MacroLibrarySite
{
  // true: list libraries. false: list `library`'s codes (never empty).
  bool librarySlot = true;
  std::string library;
  // Code-first mode (docs/adr/0008-native-macro-code-first-completion.md):
  // the 1st argument lists `LIBRARY/CODE` pairs instead of libraries.
  bool codeFirst = false;
  // UTF-16 column just after the command's `(` (code-first edit start).
  std::size_t argStart = 0;
};

// extension.js libraryCodeCommands on the UTF-16 line prefix: first of the
// three regexes that matches wins. A code slot with an empty library is no
// site (Node's `!params.library` → []).
//
// With `codeFirst`, an occurrence whose 1st argument is still being typed
// (nothing, an unquoted word, or an unclosed quote; no comma after the
// command) is a code-first site instead, whatever precedes the command.
// A closed quoted library stays a code slot.
[[nodiscard]] auto analyzeMacroLibrarySite(U16View prefix, bool codeFirst = false)
  -> std::optional<MacroLibrarySite>;

// Cheap, on the message loop: vertexadt and analyzeMacroLibrarySite on
// `line[0..cursor)`.
[[nodiscard]] auto macroLibrarySite(
  const Snapshot& snapshot,
  lsp::Position position,
  bool codeFirst = false) -> std::optional<MacroLibrarySite>;

// The code-first replace range on `line`: from `argStart` through the 1st
// argument and, when a comma follows, the 2nd argument. `cursor` is inside
// the 1st argument. UTF-16 columns, end exclusive.
[[nodiscard]] auto macroCodeFirstEditEnd(U16View line, std::size_t argStart, std::size_t cursor)
  -> std::size_t;

// Code-first matching on one pair. Typed text without `/` is a substring of
// `LIBRARY/CODE`; with `/`, the left part is a substring of the library and
// the right part of the code. Case-insensitive. Nullopt: no match; true: the
// code matched (ranked first); false: only the library matched.
[[nodiscard]] auto matchMacroCodeFirst(U16View typed, U16View library, U16View code)
  -> std::optional<bool>;

struct MacroLibraryCompletionRequest
{
  const MacroLibrarySite& site;
  const LibraryLayout& layout;
  std::string_view extensionUri;
  std::function<bool()> isCanceled;
  // Defaults to stderr.
  std::function<void(std::string_view)> log;
};

// server.js getMacroLibraries: uppercased Custom `complibs/` directory names
// holding a `d_*.xml` after ensure-export. Nullopt when canceled.
[[nodiscard]] auto listMacroLibraries(const MacroLibraryCompletionRequest& request)
  -> std::optional<std::vector<std::string>>;

// server.js getMacroCodes: unique first `CODE`/`SHTCODE` textContent per
// Record from `d_{library}.xml`, else `d_FRM_SHEET.xml`. Nullopt when canceled.
[[nodiscard]] auto listMacroCodes(const MacroLibraryCompletionRequest& request)
  -> std::optional<std::vector<std::string>>;

// The family's items (never the cheap list): Folder/Value, insertText
// `"label"`. Nullopt when canceled.
[[nodiscard]] auto macroLibraryCompletionItems(const MacroLibraryCompletionRequest& request)
  -> std::optional<std::vector<lsp::CompletionItem>>;

struct MacroCodeFirstRequest
{
  const MacroLibrarySite& site;
  const LibraryLayout& layout;
  std::string_view extensionUri;
  // The cursor's whole line, its number, and the UTF-16 cursor column.
  U16View line;
  lsp::Uint lineNumber = 0;
  std::size_t cursor = 0;
  // Defaults to stderr (the background export logs here too).
  std::function<void(std::string_view)> log;
};

// Code-first items, already filtered and ranked (sortText match group + rank;
// the caller adds the family band, no slot cache or shapeSlotItems). Libraries
// are listed from whatever `d_*.xml` they hold now; stale or missing exports
// run on one background pass at a time, and a database whose export failed is
// retried only after it changes. Nothing listable yet but exports pending: one
// `Exporting N macro libraries…` item whose edit leaves the line unchanged.
[[nodiscard]] auto macroCodeFirstCompletionItems(const MacroCodeFirstRequest& request)
  -> std::vector<lsp::CompletionItem>;

} // namespace vertexadt
