#include "newline_cleanup.h"
#include "utf16_text.h"

#include "catalog/catalog.h"
#include "command_parse.h"
#include "definition.h"
#include "text_util.h"

#include <cctype>
#include <cstddef>
#include <utility>

namespace vertexadt {
namespace {

// Node findValidationCommentIndex: first `;!` not inside `"…"` / `'…'`.
auto findValidationCommentIndex(std::string_view line) -> std::ptrdiff_t
{
  for (std::size_t i = 0; i + 1 < line.size(); ++i)
  {
    if (line[i] == ';' && line[i + 1] == '!' && !isInsideQuotes(line, i))
    {
      return static_cast<std::ptrdiff_t>(i);
    }
  }
  return -1;
}

bool isDigits(std::string_view text)
{
  if (text.empty())
  {
    return false;
  }
  for (const auto ch : text)
  {
    if (ch < '0' || ch > '9')
    {
      return false;
    }
  }
  return true;
}

// /^\d+(\.\d+)?$/
bool isBareNumericLiteral(std::string_view text)
{
  const auto dot = text.find('.');
  if (dot == std::string_view::npos)
  {
    return isDigits(text);
  }
  return isDigits(text.substr(0, dot)) && isDigits(text.substr(dot + 1));
}

auto join(const std::vector<std::string>& parts) -> std::string
{
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i)
  {
    if (i > 0)
    {
      out += ", ";
    }
    out += parts[i];
  }
  return out;
}

} // namespace

auto cleanUpLine(std::string_view lineText) -> std::optional<std::string>
{
  const auto commentIndex = findValidationCommentIndex(lineText);
  const auto code = commentIndex != -1
    ? lineText.substr(0, static_cast<std::size_t>(commentIndex))
    : lineText;
  const auto comment = commentIndex != -1
    ? lineText.substr(static_cast<std::size_t>(commentIndex))
    : std::string_view{};

  const auto parsed = parseCommand(trim(code));
  if (!parsed)
  {
    return std::nullopt; // Node throws reading functionName of null.
  }
  const auto* entry = catalog::findCommand(toLowerAscii(parsed->functionName));
  if (!entry || !entry->cleanupRules)
  {
    return std::nullopt; // unknown command throws; no cleanupRules is null.
  }

  auto params = parsed->afterParams;
  std::size_t paramIndex = 0;
  for (const auto& rule : entry->cleanupRules->afterParams)
  {
    if (rule.quote == catalog::QuoteRule::Never)
    {
      ++paramIndex;
      continue;
    }
    if (paramIndex >= params.size())
    {
      return std::nullopt; // Node throws reading trim of undefined.
    }
    const auto token = std::string(trim(params[paramIndex]));
    if (isBareNumericLiteral(token))
    {
      params[paramIndex] = "\"" + token + "\"";
      ++paramIndex;
    }
  }

  return parsed->functionName + "(" + join(parsed->insideParams) + ") " + join(params)
    + std::string(comment);
}

auto newlineCleanupEdits(
  const Snapshot& snapshot,
  lsp::Position position,
  std::string_view ch) -> std::vector<lsp::TextEdit>
{
  if (ch != "\n" || snapshot.languageId != "vertexadt" || position.line == 0)
  {
    return {};
  }
  const auto text = lineAt(snapshot.text, static_cast<std::size_t>(position.line) - 1);
  if (!text)
  {
    return {};
  }
  const auto cleaned = cleanUpLine(*text);
  if (!cleaned || *cleaned == *text)
  {
    return {};
  }
  lsp::TextEdit edit;
  edit.range = lsp::Range{
    .start = lsp::Position{.line = position.line - 1, .character = 0},
    .end = lsp::Position{
      .line = position.line - 1,
      .character = static_cast<lsp::Uint>(navigation::utf16Offset(*text, text->size())),
    },
  };
  edit.newText = *cleaned;
  return {std::move(edit)};
}

} // namespace vertexadt
