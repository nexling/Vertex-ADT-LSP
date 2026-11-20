#include "remaining_param_completion.h"
#include "utf16_text.h"

#include "call_parameters.h"

#include <algorithm>
#include <cstddef>

// Port of the client host's remaining-param provider (client/out/extension.js
// callWithPathRegex + custom/getCallParameters item mapping), reading the
// buffer-wins linked-file snapshot instead of Node's disk-only read. The line
// prefix is scanned in UTF-16 code units so cursor columns and `\s` match JS.
// The host regex is matched by hand: std::regex backtracking recursion on
// `(?:,\s*[^,]*)*` is unsafe on long lines.

namespace vertexadt {
namespace {

// `/i` without the `u` flag folds only to ASCII for ASCII patterns.
bool matchesCallAt(U16View text, std::size_t at)
{
  constexpr std::u16string_view kCall = u"call";
  if (at + kCall.size() > text.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < kCall.size(); ++i)
  {
    if (asciiLower(text[at + i]) != kCall[i])
    {
      return false;
    }
  }
  return true;
}

// Group 1 `(?:"[^"]+"|'[^']+'|[^,)]+)` starting at `a`, which must be followed
// by `,`. Returns the group's end.
auto matchFirstArgumentAt(U16View prefix, std::size_t a) -> std::optional<std::size_t>
{
  const auto n = prefix.size();
  if (a >= n)
  {
    return std::nullopt;
  }
  for (const char16_t quote : {u'"', u'\''})
  {
    if (prefix[a] == quote)
    {
      const auto close = prefix.find(quote, a + 1);
      if (close != U16View::npos && close >= a + 2 && close + 1 < n && prefix[close + 1] == u',')
      {
        return close + 1;
      }
    }
  }
  auto e = a;
  while (e < n && prefix[e] != u',' && prefix[e] != u')')
  {
    ++e;
  }
  if (e > a && e < n && prefix[e] == u',')
  {
    return e;
  }
  return std::nullopt;
}

// The host regex on the line prefix; returns group 1.
auto matchHostRegex(U16View prefix) -> std::optional<U16View>
{
  // Tail `(?:,\s*[^,]*)*,\s*$`: the prefix ends with a comma plus only spaces.
  const auto lastComma = prefix.rfind(u',');
  if (lastComma == U16View::npos || skipJsSpace(prefix, lastComma + 1) != prefix.size())
  {
    return std::nullopt;
  }
  // Leftmost match: the first `call\s*\(` whose argument is followed by a
  // comma. The optional assignment never changes which `call` matches.
  for (std::size_t k = 0; k < prefix.size(); ++k)
  {
    if (!matchesCallAt(prefix, k))
    {
      continue;
    }
    const auto paren = skipJsSpace(prefix, k + 4);
    if (paren >= prefix.size() || prefix[paren] != u'(')
    {
      continue;
    }
    const auto open = paren + 1;
    const auto a = skipJsSpace(prefix, open);
    if (const auto end = matchFirstArgumentAt(prefix, a))
    {
      return prefix.substr(a, *end - a);
    }
    // `\s*` backtracks one space into `[^,)]+` when a comma follows directly.
    if (a > open && a < prefix.size() && prefix[a] == u',')
    {
      return prefix.substr(a - 1, 1);
    }
  }
  return std::nullopt;
}

auto twoDigit(std::size_t index) -> std::string
{
  auto text = std::to_string(index);
  if (text.size() < 2)
  {
    text.insert(text.begin(), 2 - text.size(), '0');
  }
  return text;
}

} // namespace

auto remainingParamSite(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<RemainingParamSite>
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
  const auto prefix = U16View(line).substr(0, cursor);
  const auto group = matchHostRegex(prefix);
  if (!group)
  {
    return std::nullopt;
  }
  const auto commaCount = std::count(prefix.begin(), prefix.end(), u',');
  return RemainingParamSite{
    .firstArgument = utf16To8(jsTrim(*group)),
    .paramPosition = static_cast<int>(commaCount) - 1,
  };
}

auto remainingParamCompletionItems(
  const RemainingParamSite& site,
  const LibraryLayout& layout,
  const std::function<std::optional<std::string>(const std::filesystem::path&)>& textForPath)
  -> std::vector<lsp::CompletionItem>
{
  const auto parameters =
    calleeParameters(layout, textForPath, callTargetFromFirstArgument(site.firstArgument));
  const auto start = static_cast<std::size_t>(std::max(site.paramPosition, 0));
  std::vector<lsp::CompletionItem> items;
  if (start >= parameters.size())
  {
    return items;
  }
  items.reserve(parameters.size() - start);
  for (std::size_t i = start; i < parameters.size(); ++i)
  {
    const auto& parameter = parameters[i];
    lsp::CompletionItem item;
    item.label = parameter.variableName;
    item.kind = parameter.variableName.find('$') != std::string::npos
      ? lsp::CompletionItemKind::Text
      : lsp::CompletionItemKind::Variable;
    item.sortText = twoDigit(i - start);
    item.documentation = "Input Parameter " + std::to_string(i + 1) + ": "
      + parameter.comment.value_or("No comment given. See file!");
    items.push_back(std::move(item));
  }
  return items;
}

} // namespace vertexadt
