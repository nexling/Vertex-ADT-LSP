#pragma once

#include <cstddef>
#include <string_view>

namespace vertexadt {

// Character classes of MSVC ECMAScript `std::regex` over `char` in the classic
// "C" locale, for hand matchers that replace a regex byte-for-byte. Bytes
// >= 0x80 are never letters, word characters or spaces.

// `\s`
inline bool isRegexSpace(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

// `[A-Za-z]`
inline bool isAsciiLetter(char ch)
{
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

// `\d`
inline bool isAsciiDigit(char ch)
{
  return ch >= '0' && ch <= '9';
}

// `\w`, also `[a-zA-Z0-9_]`
inline bool isRegexWord(char ch)
{
  return isAsciiLetter(ch) || isAsciiDigit(ch) || ch == '_';
}

// Bytes `.` does not match (MSVC: only `\n` and `\r`).
inline bool isRegexDotExcluded(char ch)
{
  return ch == '\n' || ch == '\r';
}

// End of `\s*` from `i`.
inline std::size_t skipRegexSpace(std::string_view text, std::size_t i)
{
  while (i < text.size() && isRegexSpace(text[i]))
  {
    ++i;
  }
  return i;
}

// End of `\w*` from `i`.
inline std::size_t regexWordRunEnd(std::string_view text, std::size_t i)
{
  while (i < text.size() && isRegexWord(text[i]))
  {
    ++i;
  }
  return i;
}

} // namespace vertexadt
