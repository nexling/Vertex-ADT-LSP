#include "utf16_text.h"

#include <algorithm>

namespace vertexadt {
namespace {

auto decodeUtf8(std::string_view text, std::size_t& index) -> char32_t
{
  const auto lead = static_cast<unsigned char>(text[index++]);
  if (lead < 0x80)
  {
    return lead;
  }
  int extra = 0;
  char32_t cp = 0;
  if ((lead & 0xE0) == 0xC0)
  {
    extra = 1;
    cp = lead & 0x1F;
  }
  else if ((lead & 0xF0) == 0xE0)
  {
    extra = 2;
    cp = lead & 0x0F;
  }
  else if ((lead & 0xF8) == 0xF0)
  {
    extra = 3;
    cp = lead & 0x07;
  }
  else
  {
    return 0xFFFD;
  }
  for (int i = 0; i < extra; ++i)
  {
    if (index >= text.size())
    {
      return 0xFFFD;
    }
    const auto cont = static_cast<unsigned char>(text[index]);
    if ((cont & 0xC0) != 0x80)
    {
      return 0xFFFD;
    }
    cp = (cp << 6) | (cont & 0x3F);
    ++index;
  }
  return cp;
}

void appendUtf8(std::string& out, char32_t cp)
{
  if (cp < 0x80)
  {
    out.push_back(static_cast<char>(cp));
  }
  else if (cp < 0x800)
  {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
  else if (cp < 0x10000)
  {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
  else
  {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

} // namespace

auto utf8To16(std::string_view text) -> U16
{
  U16 out;
  out.reserve(text.size());
  std::size_t index = 0;
  while (index < text.size())
  {
    const auto cp = decodeUtf8(text, index);
    if (cp <= 0xFFFF)
    {
      out.push_back(static_cast<char16_t>(cp));
    }
    else
    {
      const auto value = cp - 0x10000;
      out.push_back(static_cast<char16_t>(0xD800 + (value >> 10)));
      out.push_back(static_cast<char16_t>(0xDC00 + (value & 0x3FF)));
    }
  }
  return out;
}

auto utf16Offset(std::string_view text, std::size_t byteOffset) -> std::size_t
{
  const auto limit = std::min(byteOffset, text.size());
  std::size_t units = 0;
  std::size_t index = 0;
  while (index < limit)
  {
    if (static_cast<unsigned char>(text[index]) < 0x80)
    {
      ++units;
      ++index;
      continue;
    }
    const auto cp = decodeUtf8(text.substr(0, limit), index);
    units += cp > 0xFFFF ? 2 : 1;
  }
  return units;
}

auto utf16Length(std::string_view text) -> std::size_t
{
  return utf16Offset(text, text.size());
}

auto utf16To8(U16View text) -> std::string
{
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    char32_t cp = text[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < text.size()
        && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
    {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (text[i + 1] - 0xDC00);
      ++i;
    }
    appendUtf8(out, cp);
  }
  return out;
}

bool isJsSpace(char16_t ch)
{
  switch (ch)
  {
  case u'\t':
  case u'\n':
  case u'\v':
  case u'\f':
  case u'\r':
  case u' ':
  case 0x00A0:
  case 0x1680:
  case 0x2028:
  case 0x2029:
  case 0x202F:
  case 0x205F:
  case 0x3000:
  case 0xFEFF:
    return true;
  default:
    return ch >= 0x2000 && ch <= 0x200A;
  }
}

bool isJsWord(char16_t ch)
{
  return (ch >= u'a' && ch <= u'z') || (ch >= u'A' && ch <= u'Z') || (ch >= u'0' && ch <= u'9')
    || ch == u'_';
}

char16_t asciiLower(char16_t ch)
{
  return (ch >= u'A' && ch <= u'Z') ? static_cast<char16_t>(ch - u'A' + u'a') : ch;
}

auto jsTrim(U16View text) -> U16View
{
  std::size_t begin = 0;
  while (begin < text.size() && isJsSpace(text[begin]))
  {
    ++begin;
  }
  return jsTrimEnd(text.substr(begin));
}

auto jsTrimEnd(U16View text) -> U16View
{
  auto end = text.size();
  while (end > 0 && isJsSpace(text[end - 1]))
  {
    --end;
  }
  return text.substr(0, end);
}

auto skipJsSpace(U16View text, std::size_t i) -> std::size_t
{
  while (i < text.size() && isJsSpace(text[i]))
  {
    ++i;
  }
  return i;
}

auto lineAt(std::string_view text, std::size_t line) -> std::optional<std::string_view>
{
  std::size_t start = 0;
  for (std::size_t i = 0; i < line; ++i)
  {
    const auto next = text.find('\n', start);
    if (next == std::string_view::npos)
    {
      return std::nullopt;
    }
    start = next + 1;
  }
  auto end = text.find('\n', start);
  if (end == std::string_view::npos)
  {
    end = text.size();
  }
  auto row = text.substr(start, end - start);
  if (!row.empty() && row.back() == '\r')
  {
    row.remove_suffix(1);
  }
  return row;
}

namespace {

auto collationPrimaryWeight(char16_t ch) -> int
{
  constexpr std::u16string_view kOrder =
    u" _-,;:!?.'\"()[]{}@*/\\&#%`^+<=>|~$0123456789abcdefghijklmnopqrstuvwxyz";
  const auto at = kOrder.find(asciiLower(ch));
  return at != std::u16string_view::npos
    ? static_cast<int>(at)
    : static_cast<int>(kOrder.size()) + static_cast<int>(ch);
}

bool isAsciiDigit16(char16_t ch)
{
  return ch >= u'0' && ch <= u'9';
}

} // namespace

int jsLocaleCompare(std::string_view a, std::string_view b, bool numeric)
{
  const auto wa = utf8To16(a);
  const auto wb = utf8To16(b);
  std::size_t i = 0;
  std::size_t j = 0;
  while (i < wa.size() && j < wb.size())
  {
    if (numeric && isAsciiDigit16(wa[i]) && isAsciiDigit16(wb[j]))
    {
      auto endA = i;
      while (endA < wa.size() && isAsciiDigit16(wa[endA]))
      {
        ++endA;
      }
      auto endB = j;
      while (endB < wb.size() && isAsciiDigit16(wb[endB]))
      {
        ++endB;
      }
      auto startA = i;
      while (startA + 1 < endA && wa[startA] == u'0')
      {
        ++startA;
      }
      auto startB = j;
      while (startB + 1 < endB && wb[startB] == u'0')
      {
        ++startB;
      }
      const auto lenA = endA - startA;
      const auto lenB = endB - startB;
      if (lenA != lenB)
      {
        return lenA < lenB ? -1 : 1;
      }
      const auto cmp = wa.compare(startA, lenA, wb, startB, lenB);
      if (cmp != 0)
      {
        return cmp < 0 ? -1 : 1;
      }
      i = endA;
      j = endB;
      continue;
    }
    const auto pa = collationPrimaryWeight(wa[i]);
    const auto pb = collationPrimaryWeight(wb[j]);
    if (pa != pb)
    {
      return pa < pb ? -1 : 1;
    }
    ++i;
    ++j;
  }
  const auto restA = wa.size() - i;
  const auto restB = wb.size() - j;
  if (restA != restB)
  {
    return restA < restB ? -1 : 1;
  }
  const auto n = std::min(wa.size(), wb.size());
  for (std::size_t k = 0; k < n; ++k)
  {
    if (wa[k] != wb[k] && asciiLower(wa[k]) == asciiLower(wb[k]))
    {
      // Same primary weight, so they differ only in ASCII case.
      return wa[k] > wb[k] ? -1 : 1;
    }
  }
  return 0;
}

} // namespace vertexadt
