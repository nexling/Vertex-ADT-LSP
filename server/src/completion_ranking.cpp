#include "completion_ranking.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace vertexadt {

auto appendFamilyItems(
  lsp::CompletionList& list,
  CompletionFamily family,
  std::vector<lsp::CompletionItem>&& items) -> bool
{
  if (items.empty())
  {
    return false;
  }
  const auto band = std::string{'0', static_cast<char>(family)};
  for (auto& item : items)
  {
    item.sortText = band + (item.sortText ? *item.sortText : item.label);
  }
  list.items.insert(
    list.items.end(),
    std::make_move_iterator(items.begin()),
    std::make_move_iterator(items.end()));
  return true;
}

void rankCheapItemsBelowFamilies(lsp::CompletionList& list, std::size_t cheapCount)
{
  const auto end = std::min(cheapCount, list.items.size());
  for (std::size_t i = 0; i < end; ++i)
  {
    auto& item = list.items[i];
    item.sortText = "1" + item.label;
  }
}

} // namespace vertexadt
