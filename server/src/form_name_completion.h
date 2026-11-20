#pragma once

#include "document_store.h"
#include "library_paths.h"
#include "utf16_text.h"

#include <lsp/types.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Form-name completion. Ordinary textDocument/completion adds Custom-over-System
// top-level `forms/` file names in the string-literal form slot of
// `edit_obj_data` / `edit_jnt_data` / `dbedit` (2nd inside argument) and
// `set_macro_form` (after-paren argument). Heavy: two directory listings, not
// the workspace index.

struct FormNameSite
{
  // false: empty slot, insert `"name"`. true: replace the quoted content.
  bool insideQuotes = false;
  // UTF-16 columns on the cursor's line.
  std::size_t replaceStart = 0;
  std::size_t replaceEnd = 0;
};

// formNameCompletion.js analyzeFormNameCompletion on one line and UTF-16 cursor.
[[nodiscard]] auto analyzeFormNameCompletion(U16View line, std::size_t cursor)
  -> std::optional<FormNameSite>;

// Cheap, on the message loop: vertexadt and analyzeFormNameCompletion on the
// current line.
[[nodiscard]] auto formNameSite(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<FormNameSite>;

struct FormNameEntry
{
  std::string name;
  // `custom/forms` or `system/forms`.
  std::string origin;
};

// formNameCompletion.js collectFormNameEntries: top-level files of each
// directory (empty path → none), Custom name wins case-insensitively, sorted
// like localeCompare with sensitivity base.
[[nodiscard]] auto collectFormNameEntries(
  const std::filesystem::path& customFormsDir,
  const std::filesystem::path& systemFormsDir) -> std::vector<FormNameEntry>;

// formNameCompletion.js resolveFormsDirectory over the layout's workspace
// folders, workspace root, and detected Custom folder name. Empty when none exists.
[[nodiscard]] auto resolveFormsDirectory(LibraryTree tree, const LibraryLayout& layout)
  -> std::filesystem::path;

// resolveFormsDirectory for any top-level library subdirectory (`forms`,
// `programs`, ...). Empty when none exists.
[[nodiscard]] auto resolveLibrarySubdirectory(
  LibraryTree tree, const LibraryLayout& layout, std::string_view subdir)
  -> std::filesystem::path;

// The family's items (never the cheap list).
[[nodiscard]] auto formNameCompletionItems(
  const FormNameSite& site,
  lsp::Position position,
  const LibraryLayout& layout) -> std::vector<lsp::CompletionItem>;

} // namespace vertexadt
