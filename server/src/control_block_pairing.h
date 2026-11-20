#pragma once

// Control-block pairing: if/else/endif and loop/endloop groups in one file,
// for hover and document highlight (docs/adr/0010-native-control-block-pairing.md).
// Same rules as the if/loop validator (shared per-line skip, `;!` comments
// stripped, case-sensitive keywords, inline if-then opens nothing), except
// keywords must be whole words. Ignores the problem cap and covers every
// section.

#include <lsp/types.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

enum class BlockKeyword
{
  If,
  Else,
  Endif,
  Loop,
  Endloop,
};

struct BlockKeywordSite
{
  BlockKeyword keyword = BlockKeyword::If;
  int line = 0;
  // UTF-16 columns of the keyword itself.
  int start = 0;
  int end = 0;
};

struct ControlBlock
{
  bool isLoop = false;
  // In document order: opener (if present), elses, closer (if present).
  std::vector<BlockKeywordSite> sites;
  bool hasOpener = false;
  bool hasCloser = false;
  // Opener header lines (trimmed, comment-stripped): the `if` line plus its
  // `and`/`or` continuation lines up to `then`.
  std::vector<std::string> header;
};

struct ControlBlockPairing
{
  std::vector<ControlBlock> blocks;
};

struct ControlBlockHit
{
  const ControlBlock* block = nullptr;
  std::size_t site = 0;
};

[[nodiscard]] auto pairControlBlocks(std::string_view text) -> ControlBlockPairing;

// The block whose keyword covers `position`. Hover passes endInclusive=false
// (the character under the mouse); highlight passes true so a caret right
// after the keyword still hits, like VS Code's word lookup.
[[nodiscard]] auto controlBlockAt(
  const ControlBlockPairing& pairing,
  lsp::Position position,
  bool endInclusive) -> std::optional<ControlBlockHit>;

// Pairing section prepended to the keyword hover.
[[nodiscard]] auto controlBlockHoverMarkdown(const ControlBlockHit& hit) -> std::string;

// textDocument/documentHighlight: the block's keywords when on one, else
// VS Code's textual word highlight (case-sensitive whole-word matches of the
// `\w+\$?` word under the cursor, default word separators).
[[nodiscard]] auto documentHighlights(std::string_view text, lsp::Position position)
  -> std::vector<lsp::DocumentHighlight>;

} // namespace vertexadt
