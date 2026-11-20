#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace vertexadt {

// Shared helpers for ports of JavaScript string logic. LSP positions count
// UTF-16 code units, and JS `\s`, `\w` and String.prototype.trim are defined on
// code units, so a JS regex or a trim gets ported against a UTF-16 copy of the
// line. Use these instead of adding local copies.

using U16 = std::u16string;
using U16View = std::u16string_view;

// UTF-8 to UTF-16. Invalid or truncated sequences decode to U+FFFD, the same
// thing the editor's buffer would show.
[[nodiscard]] auto utf8To16(std::string_view text) -> U16;

// LSP character for a byte offset into one line: the UTF-16 code units in the
// first `byteOffset` bytes of `text` (clamped to its size), decoded the same
// way as utf8To16. Use this wherever a range column comes from a UTF-8 index.
[[nodiscard]] auto utf16Offset(std::string_view text, std::size_t byteOffset) -> std::size_t;

// JS `text.length` for a UTF-8 line: utf16Offset(text, text.size()).
[[nodiscard]] auto utf16Length(std::string_view text) -> std::size_t;

// UTF-16 to UTF-8. Surrogate pairs combine; lone surrogates encode as is.
[[nodiscard]] auto utf16To8(U16View text) -> std::string;

// JS `\s` / String.prototype.trim whitespace.
[[nodiscard]] bool isJsSpace(char16_t ch);

// JS `\w` without the `u` flag: `[A-Za-z0-9_]`.
[[nodiscard]] bool isJsWord(char16_t ch);

// ASCII-only lowercase, like a JS `/i` regex without the `u` flag.
[[nodiscard]] char16_t asciiLower(char16_t ch);

// String.prototype.trim / trimEnd.
[[nodiscard]] auto jsTrim(U16View text) -> U16View;
[[nodiscard]] auto jsTrimEnd(U16View text) -> U16View;

// First index at or after `i` that is not JS whitespace (or `text.size()`).
[[nodiscard]] auto skipJsSpace(U16View text, std::size_t i) -> std::size_t;

// VS Code `document.lineAt(line).text`: the 0-based line split on `\n`, with
// a trailing `\r` dropped. Nullopt past the last line.
[[nodiscard]] auto lineAt(std::string_view text, std::size_t line)
  -> std::optional<std::string_view>;

// Approximates String.prototype.localeCompare (ICU root, default strength):
// whitespace, punctuation and symbols, then digits, then letters case-folded;
// on a primary tie lowercase sorts before uppercase. Other code units sort
// after, by value. `numeric` approximates `{ numeric: true }`: digit runs
// compare by value (leading zeros ignored).
[[nodiscard]] int jsLocaleCompare(std::string_view a, std::string_view b, bool numeric = false);

} // namespace vertexadt
