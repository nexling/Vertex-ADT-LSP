#include "dbcreate_field_completion.h"
#include "utf16_text.h"

#include "text_util.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Port of client/out/extension.js getDbcreateCompletionContext /
// getDbcreateAfterParamsContext / stripDbcreateComment /
// isDbcreateContinuation / trimDbcreateContinuation /
// buildDbcreateCompletionItems. Lines and the cursor are UTF-16 so
// substring(0, character) matches JS. Do not reuse the command-syntax
// dbcreate logical-line map or stripValidationComment.

namespace vertexadt {
namespace {

bool isJsWord(char16_t ch)
{
  return (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z')
    || (ch >= u'0' && ch <= u'9') || ch == u'_';
}

auto dropCr(std::string_view row) -> std::string_view
{
  if (!row.empty() && row.back() == '\r')
  {
    row.remove_suffix(1);
  }
  return row;
}

auto linesUtf16(std::string_view text) -> std::vector<U16>
{
  const auto rows = splitLines(text);
  std::vector<U16> lines;
  lines.reserve(rows.size());
  for (const auto row : rows)
  {
    lines.push_back(utf8To16(dropCr(row)));
  }
  return lines;
}

// stripDbcreateComment: first `;!` substring, not quote-aware.
auto stripDbcreateComment(U16View text) -> U16View
{
  for (std::size_t i = 0; i + 1 < text.size(); ++i)
  {
    if (text[i] == u';' && text[i + 1] == u'!')
    {
      return text.substr(0, i);
    }
  }
  return text;
}

bool isDbcreateContinuation(U16View text)
{
  const auto stripped = jsTrimEnd(stripDbcreateComment(text));
  return !stripped.empty() && stripped.back() == u'\\';
}

// trimDbcreateContinuation: strip `;!`, drop trailing `\s*\\\s*$`, trim.
auto trimDbcreateContinuation(U16View text) -> U16View
{
  auto stripped = stripDbcreateComment(text);
  std::size_t end = stripped.size();
  while (end > 0 && isJsSpace(stripped[end - 1]))
  {
    --end;
  }
  if (end > 0 && stripped[end - 1] == u'\\')
  {
    --end;
    while (end > 0 && isJsSpace(stripped[end - 1]))
    {
      --end;
    }
    stripped = stripped.substr(0, end);
  }
  return jsTrim(stripped);
}

bool equalsIgnoreCaseAscii(U16View text, U16View lowerAscii)
{
  if (text.size() != lowerAscii.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    auto ch = text[i];
    if (ch >= u'A' && ch <= u'Z')
    {
      ch = static_cast<char16_t>(ch - u'A' + u'a');
    }
    if (ch != lowerAscii[i])
    {
      return false;
    }
  }
  return true;
}

bool startsDbcreateAt(U16View text, std::size_t i)
{
  constexpr U16View kName = u"dbcreate";
  if (i + kName.size() > text.size())
  {
    return false;
  }
  if (i > 0 && isJsWord(text[i - 1]))
  {
    return false;
  }
  return equalsIgnoreCaseAscii(text.substr(i, kName.size()), kName);
}

// /\bdbcreate\s*\(/i
bool hasDbcreateOpenParen(U16View text)
{
  constexpr std::size_t kNameSize = 8;
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    if (!startsDbcreateAt(text, i))
    {
      continue;
    }
    auto j = i + kNameSize;
    while (j < text.size() && isJsSpace(text[j]))
    {
      ++j;
    }
    if (j < text.size() && text[j] == u'(')
    {
      return true;
    }
  }
  return false;
}

// /\bdbcreate\s*\([^)]*\)\s*(.*)$/i — leftmost successful match; first `)`.
auto afterParamsText(U16View prefix) -> std::optional<U16View>
{
  constexpr std::size_t kNameSize = 8;
  for (std::size_t i = 0; i < prefix.size(); ++i)
  {
    if (!startsDbcreateAt(prefix, i))
    {
      continue;
    }
    auto j = i + kNameSize;
    while (j < prefix.size() && isJsSpace(prefix[j]))
    {
      ++j;
    }
    if (j >= prefix.size() || prefix[j] != u'(')
    {
      continue;
    }
    ++j;
    while (j < prefix.size() && prefix[j] != u')')
    {
      ++j;
    }
    if (j >= prefix.size())
    {
      continue;
    }
    ++j;
    while (j < prefix.size() && isJsSpace(prefix[j]))
    {
      ++j;
    }
    return prefix.substr(j);
  }
  return std::nullopt;
}

struct AfterParamsContext
{
  int commaCount = 0;
  U16View fieldText;
};

auto getDbcreateAfterParamsContext(U16View afterParams) -> AfterParamsContext
{
  AfterParamsContext result;
  char16_t inString = 0;
  for (std::size_t i = 0; i < afterParams.size(); ++i)
  {
    const auto ch = afterParams[i];
    if (inString)
    {
      if (ch == inString)
      {
        inString = 0;
      }
      continue;
    }
    if (ch == u'"' || ch == u'\'')
    {
      inString = ch;
      continue;
    }
    if (ch == u',')
    {
      ++result.commaCount;
      if (result.commaCount == 3)
      {
        result.fieldText = afterParams.substr(i + 1);
        break;
      }
    }
  }
  return result;
}

// /\s*\\\s*/g → single space.
auto replaceMidlineBackslash(U16View text) -> U16
{
  U16 out;
  out.reserve(text.size());
  std::size_t i = 0;
  while (i < text.size())
  {
    auto j = i;
    while (j < text.size() && isJsSpace(text[j]))
    {
      ++j;
    }
    if (j < text.size() && text[j] == u'\\')
    {
      ++j;
      while (j < text.size() && isJsSpace(text[j]))
      {
        ++j;
      }
      out.push_back(u' ');
      i = j;
      continue;
    }
    out.push_back(text[i]);
    ++i;
  }
  return out;
}

auto splitJsWhitespace(U16View text) -> std::vector<U16View>
{
  std::vector<U16View> tokens;
  std::size_t i = 0;
  while (i < text.size())
  {
    while (i < text.size() && isJsSpace(text[i]))
    {
      ++i;
    }
    if (i >= text.size())
    {
      break;
    }
    auto j = i;
    while (j < text.size() && !isJsSpace(text[j]))
    {
      ++j;
    }
    tokens.push_back(text.substr(i, j - i));
    i = j;
  }
  return tokens;
}

struct DbcreateContext
{
  int tokenSlot = 0;
};

auto getDbcreateCompletionContext(const std::vector<U16>& lines, lsp::Position position)
  -> std::optional<DbcreateContext>
{
  if (position.line >= lines.size())
  {
    return std::nullopt;
  }

  auto startLine = static_cast<int>(position.line);
  while (startLine > 0)
  {
    if (!isDbcreateContinuation(lines[static_cast<std::size_t>(startLine) - 1]))
    {
      break;
    }
    --startLine;
  }

  const auto& startLineText = lines[static_cast<std::size_t>(startLine)];
  if (!hasDbcreateOpenParen(stripDbcreateComment(startLineText)))
  {
    return std::nullopt;
  }

  U16 joined;
  bool firstPart = true;
  for (int lineIndex = startLine; lineIndex <= static_cast<int>(position.line); ++lineIndex)
  {
    U16View lineText = lines[static_cast<std::size_t>(lineIndex)];
    if (lineIndex == static_cast<int>(position.line))
    {
      const auto cursor = std::min<std::size_t>(position.character, lineText.size());
      lineText = lineText.substr(0, cursor);
    }
    const auto trimmedPart = trimDbcreateContinuation(lineText);
    if (trimmedPart.empty())
    {
      continue;
    }
    if (!firstPart)
    {
      joined.push_back(u' ');
    }
    joined.append(trimmedPart);
    firstPart = false;
  }

  const auto logicalPrefix = jsTrim(joined);
  const auto afterParams = afterParamsText(logicalPrefix);
  if (!afterParams)
  {
    return std::nullopt;
  }

  const auto after = getDbcreateAfterParamsContext(*afterParams);
  if (after.commaCount < 3)
  {
    return std::nullopt;
  }

  const auto normalized = replaceMidlineBackslash(after.fieldText);
  const auto trimmedField = jsTrim(normalized);
  const auto fieldTokens = splitJsWhitespace(trimmedField);
  const auto tokenSlot = fieldTokens.empty()
    ? 0
    : static_cast<int>(fieldTokens.size() % 4);
  return DbcreateContext{.tokenSlot = tokenSlot};
}

lsp::CompletionItem slotItem(
  std::string label,
  lsp::CompletionItemKind kind,
  std::string detail)
{
  lsp::CompletionItem item;
  item.label = label;
  item.kind = kind;
  item.insertText = std::move(label);
  item.detail = std::move(detail);
  return item;
}

auto buildItems(int tokenSlot) -> std::vector<lsp::CompletionItem>
{
  std::vector<lsp::CompletionItem> items;
  if (tokenSlot == 0)
  {
    items.push_back(slotItem(
      "FIELDNAME",
      lsp::CompletionItemKind::EnumMember,
      "wanted fielname - case sensitive, usually uppercase!"));
  }
  if (tokenSlot == 1)
  {
    static constexpr std::pair<const char*, const char*> kTypes[] = {
      {"A", "Alpha field type"},
      {"N", "Numeric field type"},
      {"B", "Binary field type"},
      {"T", "Text field type"},
      {"M", "Memo field type"},
      {"U", "Unicode field type"},
    };
    for (const auto& [label, detail] : kTypes)
    {
      items.push_back(slotItem(label, lsp::CompletionItemKind::EnumMember, detail));
    }
  }
  if (tokenSlot == 2)
  {
    static constexpr const char* kLengths[] = {
      "1", "8", "16", "32", "64", "128", "255",
    };
    for (const auto* length : kLengths)
    {
      items.push_back(slotItem(length, lsp::CompletionItemKind::Value, "Field length"));
    }
  }
  if (tokenSlot == 3)
  {
    static constexpr std::pair<const char*, const char*> kIndex[] = {
      {"x", "No index"},
      {"IND", "Indexed field"},
      {"UNIQUE", "Unique indexed field"},
    };
    for (const auto& [label, detail] : kIndex)
    {
      items.push_back(slotItem(label, lsp::CompletionItemKind::EnumMember, detail));
    }
  }
  return items;
}

} // namespace

auto dbcreateFieldCompletionItems(const Snapshot& snapshot, lsp::Position position)
  -> std::vector<lsp::CompletionItem>
{
  if (snapshot.languageId != "vertexadt")
  {
    return {};
  }
  const auto lines = linesUtf16(snapshot.text);
  const auto context = getDbcreateCompletionContext(lines, position);
  if (!context)
  {
    return {};
  }
  return buildItems(context->tokenSlot);
}

} // namespace vertexadt
