#pragma once

#include <lsp/types.h>

#include <cstddef>
#include <vector>

namespace vertexadt {

// Native completion ranking (docs/adr/0001-native-completion-ranks-slot-items-first.md).
// When a slot-specific family adds items, they sort above the cheap
// keyword-variable list: family items get sortText `0` + family digit + their
// family key (own sortText, else label; slot_completion.h puts a match group
// in front of it); cheap items get `1` + label. A
// response with no family items keeps the cheap list untouched. Deliberate
// divergence from the frozen Node server.

// The digit is the existing append order, so two families at one site keep it.
enum class CompletionFamily : char
{
  DbcreateField = '0',
  DebugFilename = '1',
  IncludeCall = '2',
  RemainingParam = '3',
  CallReturnValue = '4',
  FormName = '5',
  ObjectParameter = '6',
  MacroLibrary = '7',
  PieceSet = '8',
};

// Appends `family`'s items to `list`, ranked into the family band. Returns
// whether any were appended.
auto appendFamilyItems(
  lsp::CompletionList& list,
  CompletionFamily family,
  std::vector<lsp::CompletionItem>&& items) -> bool;

// Ranks the cheap items `list.items[0..cheapCount)` below every family band.
// Idempotent.
void rankCheapItemsBelowFamilies(lsp::CompletionList& list, std::size_t cheapCount);

} // namespace vertexadt
