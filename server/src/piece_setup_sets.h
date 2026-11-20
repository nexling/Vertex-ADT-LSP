#pragma once

#include "document_store.h"
#include "library_paths.h"
#include "utf16_text.h"

#include <lsp/types.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// piece.setup set completion (docs/adr/0004-native-piece-setup-set-completion.md).
// In the set slot of `new_piece(` (6th inside argument) and `set_piece_set(…)`
// (1st after-paren argument), quoted or empty, ordinary textDocument/completion
// adds the piece sets that Vertex's vxsettings define for the active
// configuration, and hover on a quoted set string shows the set's resolved lines.
// Native only; the frozen Node server has no counterpart.
//
// Sources: `system/programs/vxsettings.xml` with the Custom
// `programs/vxsettings.xml` applied onto it as a GUID overlay, filtered by
// `system/programs/vxsettings_config.xml`.

// Settings condition: names (may contain `-`) joined by `and` / `or`, each
// optionally preceded by `not`; no parentheses. Evaluated as a strict
// left-to-right fold against one active value, names compared
// case-insensitively. Blank expressions, a blank active value and unparsable
// expressions all match (fail open).
[[nodiscard]] bool settingsConditionMatches(std::string_view expression, std::string_view active);

// The four `vxsettings_config.xml` values a DataItem is filtered on. Empty
// means unknown, so the matching attribute never filters.
struct VxSettingsConfig
{
  std::string application;
  std::string config;
  std::string units;
  std::string language;
};

[[nodiscard]] auto parseVxSettingsConfig(std::string_view xml) -> VxSettingsConfig;

struct PieceSetLine
{
  std::string text;
  // Came from the Custom overlay (REPLACE or ADD).
  bool custom = false;
};

// One piece set: an active `function_begin` and the active `include_set` /
// `param` lines up to its `function_end`.
struct PieceSet
{
  // First word of the function_begin Data, read whole (dots included).
  std::string name;
  // The rest of the function_begin Data, one pair of surrounding double
  // quotes dropped (`"WALL FRAMING"` → `WALL FRAMING`).
  std::string description;
  std::vector<PieceSetLine> includeSets;
  std::vector<PieceSetLine> params;
  // The whole SetupItem was added by Custom.
  bool custom = false;
  // The function_begin itself came from Custom.
  bool nameCustom = false;
};

// Builds the System piece.setup tree (the SetupItem whose trimmed Summary is
// `piece.setup`), applies the Custom file onto it by GUID (matching SetupItems
// merge, matching DataItems are replaced whatever their `Custom` value, the
// rest are added; an added DataItem goes before the item's trailing
// function_end), then collects sets from every descendant SetupItem whose
// trimmed Summary starts with `piece.setup`. A DataItem is active when its
// Config, Application, Units and Language conditions all match; other
// attributes (Optbit, UILanguage, ...) are ignored. In one SetupItem the first
// active function_begin opens a set and later ones are ignored until
// function_end; set_begin fragments are skipped. Names dedupe case-sensitively,
// first wins. Empty or unparsable System text gives no sets.
[[nodiscard]] auto collectPieceSets(
  std::string_view systemXml,
  std::string_view customXml,
  const VxSettingsConfig& config) -> std::vector<PieceSet>;

struct PieceSetSources
{
  // Empty when the directory does not exist.
  std::filesystem::path system;
  std::filesystem::path custom;
  std::filesystem::path config;
};

// `programs/` under the layout's System and Custom trees (the forms/ lookup in
// form_name_completion.h).
[[nodiscard]] auto resolvePieceSetSources(const LibraryLayout& layout) -> PieceSetSources;

// collectPieceSets over the files on disk. Cached on each file's path, size
// and modification time, so an edit, a removal or a config change rebuilds on
// the next call. Missing files read as empty. Thread-safe.
[[nodiscard]] auto loadPieceSets(const PieceSetSources& sources)
  -> std::shared_ptr<const std::vector<PieceSet>>;

enum class PieceSetCommand
{
  // `new_piece(…)`: the 6th argument inside the parentheses.
  NewPiece,
  // `set_piece_set(obj) …`: the 1st argument after `)`; the 2nd is the mode.
  SetPieceSet,
};

struct PieceSetSite
{
  PieceSetCommand command = PieceSetCommand::NewPiece;
  // true: replace the quoted content. false: empty slot, insert `"NAME"`.
  bool insideQuotes = false;
  // Empty slot right after `)`: insert ` "NAME"`.
  bool leadingSpace = false;
  // UTF-16 columns on the cursor's line.
  std::size_t replaceStart = 0;
  std::size_t replaceEnd = 0;
  // The quoted content (UTF-8), up to the closing quote or end of line.
  // Empty when !insideQuotes.
  std::string content;
  // set_piece_set's mode when written as a literal number or `+` sum of
  // numbers (`2`, `1+2`); nullopt when missing or anything else.
  std::optional<long long> mode;
};

// Cursor in the set slot of the last `new_piece(` / `set_piece_set(` (word
// boundary, case-insensitive) before it, not in a `!` comment: inside its
// quotes, or in a slot holding only spaces/tabs before the cursor (the caller
// retries at a typed word's start, so `…,FRA` is the empty slot at `F`).
[[nodiscard]] auto analyzePieceSetSite(U16View line, std::size_t cursor)
  -> std::optional<PieceSetSite>;

// Cheap: vertexadt and analyzePieceSetSite on the current line.
[[nodiscard]] auto pieceSetSite(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<PieceSetSite>;

// Completion and hover documentation for one set.
[[nodiscard]] auto pieceSetMarkdown(const PieceSet& set) -> std::string;

// Label and detail are the NAME, inserted as is inside quotes or as `"NAME"`
// (with a leading space after `)`) in an empty slot; documentation is
// pieceSetMarkdown. Never filtered here (shapeSlotItems does that).
[[nodiscard]] auto pieceSetCompletionItems(
  const PieceSetSite& site,
  lsp::Position position,
  const std::vector<PieceSet>& sets) -> std::vector<lsp::CompletionItem>;

// The set a quoted site names. new_piece: by NAME. set_piece_set: a literal
// mode with bit 1 clear matches DESCR, bit 1 set NAME, an unknown mode NAME
// then DESCR. First in file order wins.
[[nodiscard]] auto findPieceSetForSite(const PieceSetSite& site, const std::vector<PieceSet>& sets)
  -> const PieceSet*;

// Hover markdown for the set named by the quoted string under the cursor.
[[nodiscard]] auto pieceSetHover(
  const Snapshot& snapshot,
  lsp::Position position,
  const LibraryLayout& layout) -> std::optional<std::string>;

} // namespace vertexadt
