#include "local_slot_rename.h"

#include "object_parameter_completion.h"
#include "text_util.h"
#include "utf16_text.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <tuple>

namespace vertexadt {
namespace {

// JS `\w` on one UTF-8 byte: multi-byte sequences are never word characters
// without the `u` flag, so byte-wise matching agrees with the UTF-16 regex.
bool isWordByte(char ch)
{
  return isJsWord(static_cast<char16_t>(static_cast<unsigned char>(ch)));
}

bool isBoundary(std::string_view text, std::size_t at)
{
  const auto left = at > 0 && isWordByte(text[at - 1]);
  const auto right = at < text.size() && isWordByte(text[at]);
  return left != right;
}

bool equalsAsciiIgnoreCase(std::string_view a, std::string_view b)
{
  return a.size() == b.size()
    && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
         return std::tolower(static_cast<unsigned char>(x))
           == std::tolower(static_cast<unsigned char>(y));
       });
}

auto upper(std::string_view text) -> std::string
{
  std::string out(text);
  for (auto& ch : out)
  {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return out;
}

auto lower(std::string_view text) -> std::string
{
  return toLowerAscii(text);
}

bool isUpperLetter(char ch)
{
  return std::isupper(static_cast<unsigned char>(ch)) != 0;
}

// Node `applyCasePattern`.
auto applyCasePattern(std::string_view source, std::string_view pattern) -> std::string
{
  if (source.empty() || pattern.empty())
  {
    return std::string(source);
  }
  if (pattern == upper(pattern) && pattern != lower(pattern))
  {
    return upper(source);
  }
  if (pattern == lower(pattern))
  {
    return lower(source);
  }
  const auto rest = std::string(pattern.substr(1));
  // JS `p[0] === p[0].toUpperCase()`: an uppercase letter or a non-letter.
  if (std::islower(static_cast<unsigned char>(pattern.front())) == 0 && rest == lower(rest))
  {
    return upper(source.substr(0, 1)) + lower(source.substr(1));
  }
  std::string result;
  result.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i)
  {
    const auto ch = source[i];
    if (i >= pattern.size())
    {
      result.push_back(ch);
      continue;
    }
    const auto p = pattern[i];
    if (isUpperLetter(p))
    {
      result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    else
    {
      // JS: `p === p.toLowerCase()` holds for every non-uppercase ASCII char.
      result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
  }
  return result;
}

// Node `stripLocalSlotPrefixFromMatchedText`.
auto stripOldPrefix(std::string_view oldName, std::string_view matched) -> std::string_view
{
  const auto oldPrefix = localSlotNamePrefix(oldName);
  if (oldPrefix.empty() || matched.size() <= oldPrefix.size())
  {
    return matched;
  }
  if (equalsAsciiIgnoreCase(matched.substr(0, oldPrefix.size()), oldPrefix))
  {
    return matched.substr(oldPrefix.size());
  }
  return matched;
}

// Node `buildCasePreservingLocalSlotRenameText`.
auto casePreservingText(const LocalSlotRenamePair& pair, std::string_view matched) -> std::string
{
  const auto slotPrefix = localSlotNamePrefix(pair.newName);
  auto canonicalBase = localSlotNameBase(pair.newName);
  if (canonicalBase.empty())
  {
    canonicalBase = isLocalSlotName(pair.oldName) ? localSlotNameBase(pair.oldName)
                                                  : extractBaseVariableName(pair.oldName);
  }
  return slotPrefix + applyCasePattern(canonicalBase, stripOldPrefix(pair.oldName, matched));
}

struct Reference
{
  int line = 0;
  int start = 0;
  int end = 0;
  std::string matchedText;
};

// Node `findVariableReferencesInLines` with `buildVariableReferenceRegex`
// (`\bname\b(?!\$)` / `\bname$\b`, `gi`), matched on UTF-8 bytes and reported
// in UTF-16 units.
auto findReferences(const std::vector<std::string_view>& lines, std::string_view varName)
  -> std::vector<Reference>
{
  std::vector<Reference> references;
  const auto expected = extractBaseVariableName(varName);
  if (expected.empty())
  {
    return references;
  }
  const auto dollarName = expected.back() == '$';
  for (int lineIndex = 0; lineIndex < static_cast<int>(lines.size()); ++lineIndex)
  {
    const auto raw = lines[static_cast<std::size_t>(lineIndex)];
    const auto wide = utf8To16(raw);
    const auto trimmed = jsTrim(wide);
    if (!trimmed.empty() && trimmed.front() == u'!')
    {
      continue;
    }
    const auto code = stripValidationComment(raw);
    const std::string_view text(code);
    std::size_t i = 0;
    while (i + expected.size() <= text.size())
    {
      const auto end = i + expected.size();
      const auto matches = equalsAsciiIgnoreCase(text.substr(i, expected.size()), expected)
        && isBoundary(text, i) && isBoundary(text, end)
        && (dollarName || end >= text.size() || text[end] != '$');
      if (!matches)
      {
        ++i;
        continue;
      }
      if (!isInsideQuotes(text, i))
      {
        const auto start = static_cast<int>(utf8To16(text.substr(0, i)).size());
        const auto length = static_cast<int>(utf8To16(text.substr(i, expected.size())).size());
        references.push_back(Reference{
          .line = lineIndex,
          .start = start,
          .end = start + length,
          .matchedText = std::string(text.substr(i, expected.size())),
        });
      }
      i = end;
    }
  }
  return references;
}

} // namespace

auto fileWideLocalSlotRenameEdits(
  const std::vector<std::string_view>& lines,
  const std::vector<LocalSlotRenamePair>& pairs) -> std::vector<LocalSlotRenameEdit>
{
  std::vector<LocalSlotRenameEdit> edits;
  std::set<std::tuple<int, int, int>> seen;
  auto sorted = pairs;
  std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
    return utf8To16(a.oldName).size() > utf8To16(b.oldName).size();
  });
  for (const auto& pair : sorted)
  {
    for (const auto& ref : findReferences(lines, pair.oldName))
    {
      if (!seen.emplace(ref.line, ref.start, ref.end).second)
      {
        continue;
      }
      edits.push_back(LocalSlotRenameEdit{
        .line = ref.line,
        .startCharacter = ref.start,
        .endCharacter = ref.end,
        .newText = casePreservingText(pair, ref.matchedText),
      });
    }
  }
  std::stable_sort(edits.begin(), edits.end(), [](const auto& a, const auto& b) {
    if (a.line != b.line)
    {
      return a.line > b.line;
    }
    return a.startCharacter > b.startCharacter;
  });
  return edits;
}

} // namespace vertexadt
