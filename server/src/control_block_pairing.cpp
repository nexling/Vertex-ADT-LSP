#include "control_block_pairing.h"

#include "text_util.h"
#include "utf16_text.h"

#include <cctype>
#include <string>
#include <utility>

namespace vertexadt {
namespace {

constexpr std::size_t kMaxHeaderLines = 20;

bool isWordChar(char ch)
{
  const auto c = static_cast<unsigned char>(ch);
  return std::isalnum(c) != 0 || c == '_';
}

// `trimmed` starts with `word` as a whole, case-sensitive word. `$` also
// counts as part of the word so `if$` (a string variable) is not a keyword.
bool startsWithKeyword(std::string_view trimmed, std::string_view word)
{
  if (!trimmed.starts_with(word))
  {
    return false;
  }
  return trimmed.size() == word.size()
    || (!isWordChar(trimmed[word.size()]) && trimmed[word.size()] != '$');
}

bool equalsIgnoreCase(char a, char b)
{
  return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

bool containsWordIgnoreCase(std::string_view text, std::string_view word)
{
  for (std::size_t i = 0; i + word.size() <= text.size(); ++i)
  {
    bool match = true;
    for (std::size_t j = 0; j < word.size() && match; ++j)
    {
      match = equalsIgnoreCase(text[i + j], word[j]);
    }
    if (!match)
    {
      continue;
    }
    const bool leftOk = i == 0 || !isWordChar(text[i - 1]);
    const bool rightOk = i + word.size() == text.size() || !isWordChar(text[i + word.size()]);
    if (leftOk && rightOk)
    {
      return true;
    }
  }
  return false;
}

bool startsWithWordIgnoreCase(std::string_view text, std::string_view word)
{
  if (text.size() < word.size())
  {
    return false;
  }
  for (std::size_t j = 0; j < word.size(); ++j)
  {
    if (!equalsIgnoreCase(text[j], word[j]))
    {
      return false;
    }
  }
  return text.size() == word.size() || !isWordChar(text[word.size()]);
}

bool isContinuationStart(std::string_view trimmed)
{
  return startsWithWordIgnoreCase(trimmed, "and")
    || startsWithWordIgnoreCase(trimmed, "or")
    || trimmed.starts_with("&&")
    || trimmed.starts_with("||");
}

struct CodeLine
{
  bool kept = false;
  std::string stripped;
  std::size_t indentBytes = 0;
  std::string_view trimmed() const { return trim(stripped); }
};

auto codeLines(const std::vector<std::string_view>& lines) -> std::vector<CodeLine>
{
  std::vector<CodeLine> out(lines.size());
  for (std::size_t i = 0; i < lines.size(); ++i)
  {
    if (isSharedPerLineSkip(lines[i]))
    {
      continue;
    }
    auto& line = out[i];
    line.stripped = stripValidationComment(lines[i]);
    const auto trimmedText = trim(line.stripped);
    if (trimmedText.empty())
    {
      continue;
    }
    line.kept = true;
    line.indentBytes = static_cast<std::size_t>(trimmedText.data() - line.stripped.data());
  }
  return out;
}

auto headerLines(const std::vector<CodeLine>& lines, std::size_t opener) -> std::vector<std::string>
{
  std::vector<std::string> header;
  const auto first = lines[opener].trimmed();
  header.emplace_back(first);
  if (containsWordIgnoreCase(first, "then"))
  {
    return header;
  }
  for (std::size_t i = opener + 1; i < lines.size() && header.size() < kMaxHeaderLines; ++i)
  {
    if (!lines[i].kept)
    {
      continue;
    }
    const auto trimmedText = lines[i].trimmed();
    const bool hasThen = containsWordIgnoreCase(trimmedText, "then");
    if (!isContinuationStart(trimmedText) && !hasThen)
    {
      break;
    }
    header.emplace_back(trimmedText);
    if (hasThen)
    {
      break;
    }
  }
  return header;
}

std::string_view keywordText(BlockKeyword keyword)
{
  switch (keyword)
  {
  case BlockKeyword::If: return "if";
  case BlockKeyword::Else: return "else";
  case BlockKeyword::Endif: return "endif";
  case BlockKeyword::Loop: return "loop";
  case BlockKeyword::Endloop: return "endloop";
  }
  return {};
}

std::string lineLabel(int line)
{
  return std::to_string(line + 1);
}

std::string codeBlock(const std::vector<std::string>& header)
{
  std::string out = "```vertexadt\n";
  for (const auto& line : header)
  {
    out += line;
    out += '\n';
  }
  out += "```";
  return out;
}

const BlockKeywordSite* closerOf(const ControlBlock& block)
{
  return block.hasCloser ? &block.sites.back() : nullptr;
}

std::string closedByText(const ControlBlock& block, bool listElses)
{
  const auto closer = block.isLoop ? std::string_view{"endloop"} : std::string_view{"endif"};
  if (!block.hasCloser)
  {
    return "No matching `" + std::string(closer) + "`";
  }
  std::string out = "Closed by `" + std::string(closer) + "` at line " + lineLabel(closerOf(block)->line);
  if (!listElses)
  {
    return out;
  }
  std::vector<int> elses;
  for (const auto& site : block.sites)
  {
    if (site.keyword == BlockKeyword::Else)
    {
      elses.push_back(site.line);
    }
  }
  if (elses.empty())
  {
    return out;
  }
  out += elses.size() == 1 ? " (`else` at line " : " (`else` at lines ";
  for (std::size_t i = 0; i < elses.size(); ++i)
  {
    out += (i == 0 ? "" : ", ") + lineLabel(elses[i]);
  }
  out += ")";
  return out;
}

// VS Code's default `editor.wordSeparators`, plus whitespace and line breaks.
bool isWordBoundaryChar(char16_t ch)
{
  constexpr std::u16string_view kSeparators = u"`~!@#$%^&*()-=+[{]}\\|;:'\",.<>/?";
  return ch == u' ' || ch == u'\t' || ch == u'\r' || ch == u'\n'
    || kSeparators.find(ch) != std::u16string_view::npos;
}

bool isAsciiWordChar16(char16_t ch)
{
  return (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z') || (ch >= u'0' && ch <= u'9')
    || ch == u'_';
}

// VS Code getWordAtPosition with wordPattern `\b\w+\$?`: start <= column <= end.
std::optional<std::u16string> wordAtCaret(std::u16string_view line, std::size_t column)
{
  std::size_t i = 0;
  while (i < line.size())
  {
    if (!isAsciiWordChar16(line[i]))
    {
      ++i;
      continue;
    }
    const auto start = i;
    while (i < line.size() && isAsciiWordChar16(line[i]))
    {
      ++i;
    }
    if (i < line.size() && line[i] == u'$')
    {
      ++i;
    }
    if (column >= start && column <= i)
    {
      return std::u16string(line.substr(start, i - start));
    }
    if (start > column)
    {
      break;
    }
  }
  return std::nullopt;
}

// VS Code TextModel.findMatches(word, isRegex=false, matchCase=true,
// wordSeparators) whole-word validity.
auto textualHighlights(const std::vector<std::string_view>& lines, std::u16string_view word)
  -> std::vector<lsp::DocumentHighlight>
{
  std::vector<lsp::DocumentHighlight> out;
  for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
  {
    const auto text = utf8To16(lines[lineIndex]);
    const std::u16string_view view(text);
    std::size_t from = 0;
    while (from + word.size() <= view.size())
    {
      const auto at = view.find(word, from);
      if (at == std::u16string_view::npos)
      {
        break;
      }
      const auto end = at + word.size();
      const bool leftOk = at == 0 || isWordBoundaryChar(view[at - 1]) || isWordBoundaryChar(view[at]);
      const bool rightOk = end == view.size() || isWordBoundaryChar(view[end])
        || isWordBoundaryChar(view[end - 1]);
      if (leftOk && rightOk)
      {
        out.push_back(lsp::DocumentHighlight{
          .range = lsp::Range{
            .start = {static_cast<lsp::Uint>(lineIndex), static_cast<lsp::Uint>(at)},
            .end = {static_cast<lsp::Uint>(lineIndex), static_cast<lsp::Uint>(end)},
          },
          .kind = lsp::DocumentHighlightKind::Text,
        });
      }
      from = at + word.size();
    }
  }
  return out;
}

} // namespace

auto pairControlBlocks(std::string_view text) -> ControlBlockPairing
{
  const auto lines = splitLines(text);
  const auto code = codeLines(lines);
  ControlBlockPairing pairing;
  auto& blocks = pairing.blocks;
  std::vector<std::size_t> ifStack;
  std::vector<std::size_t> loopStack;

  for (std::size_t i = 0; i < code.size(); ++i)
  {
    if (!code[i].kept)
    {
      continue;
    }
    const auto trimmedText = code[i].trimmed();
    const auto site = [&](BlockKeyword keyword) {
      const auto startByte = code[i].indentBytes;
      const auto start = utf16Offset(lines[i], startByte);
      return BlockKeywordSite{
        .keyword = keyword,
        .line = static_cast<int>(i),
        .start = static_cast<int>(start),
        .end = static_cast<int>(start + keywordText(keyword).size()),
      };
    };
    const auto standalone = [&](BlockKeyword keyword, bool isLoop) {
      ControlBlock block;
      block.isLoop = isLoop;
      block.hasCloser = keyword == BlockKeyword::Endif || keyword == BlockKeyword::Endloop;
      block.sites.push_back(site(keyword));
      blocks.push_back(std::move(block));
    };

    if (startsWithKeyword(trimmedText, "if"))
    {
      if (isInlineIfThen(trimmedText))
      {
        continue;
      }
      ControlBlock block;
      block.hasOpener = true;
      block.sites.push_back(site(BlockKeyword::If));
      block.header = headerLines(code, i);
      ifStack.push_back(blocks.size());
      blocks.push_back(std::move(block));
    }
    else if (startsWithKeyword(trimmedText, "else"))
    {
      if (ifStack.empty())
      {
        standalone(BlockKeyword::Else, false);
      }
      else
      {
        blocks[ifStack.back()].sites.push_back(site(BlockKeyword::Else));
      }
    }
    else if (startsWithKeyword(trimmedText, "endif"))
    {
      if (ifStack.empty())
      {
        standalone(BlockKeyword::Endif, false);
      }
      else
      {
        auto& block = blocks[ifStack.back()];
        block.sites.push_back(site(BlockKeyword::Endif));
        block.hasCloser = true;
        ifStack.pop_back();
      }
    }
    else if (startsWithKeyword(trimmedText, "loop"))
    {
      ControlBlock block;
      block.isLoop = true;
      block.hasOpener = true;
      block.sites.push_back(site(BlockKeyword::Loop));
      block.header.emplace_back(trimmedText);
      loopStack.push_back(blocks.size());
      blocks.push_back(std::move(block));
    }
    else if (startsWithKeyword(trimmedText, "endloop"))
    {
      if (loopStack.empty())
      {
        standalone(BlockKeyword::Endloop, true);
      }
      else
      {
        auto& block = blocks[loopStack.back()];
        block.sites.push_back(site(BlockKeyword::Endloop));
        block.hasCloser = true;
        loopStack.pop_back();
      }
    }
  }
  return pairing;
}

auto controlBlockAt(const ControlBlockPairing& pairing, lsp::Position position, bool endInclusive)
  -> std::optional<ControlBlockHit>
{
  const auto line = static_cast<int>(position.line);
  const auto column = static_cast<int>(position.character);
  for (const auto& block : pairing.blocks)
  {
    for (std::size_t i = 0; i < block.sites.size(); ++i)
    {
      const auto& site = block.sites[i];
      if (site.line == line && column >= site.start
          && (column < site.end || (endInclusive && column == site.end)))
      {
        return ControlBlockHit{.block = &block, .site = i};
      }
    }
  }
  return std::nullopt;
}

auto controlBlockHoverMarkdown(const ControlBlockHit& hit) -> std::string
{
  const auto& block = *hit.block;
  const auto& site = block.sites[hit.site];
  const auto opener = block.isLoop ? std::string_view{"loop"} : std::string_view{"if"};
  switch (site.keyword)
  {
  case BlockKeyword::If:
  case BlockKeyword::Loop:
    return closedByText(block, true);
  case BlockKeyword::Else:
  case BlockKeyword::Endif:
  case BlockKeyword::Endloop:
  {
    if (!block.hasOpener)
    {
      return "No matching `" + std::string(opener) + "`";
    }
    const auto verb = site.keyword == BlockKeyword::Else ? "Else of" : "Closes";
    std::string out = std::string("**") + verb + " `" + std::string(opener) + "` (line "
      + lineLabel(block.sites.front().line) + "):**\n" + codeBlock(block.header);
    if (site.keyword == BlockKeyword::Else)
    {
      out += "\n\n" + closedByText(block, false);
    }
    return out;
  }
  }
  return {};
}

auto documentHighlights(std::string_view text, lsp::Position position)
  -> std::vector<lsp::DocumentHighlight>
{
  const auto pairing = pairControlBlocks(text);
  if (const auto hit = controlBlockAt(pairing, position, true))
  {
    std::vector<lsp::DocumentHighlight> out;
    for (const auto& site : hit->block->sites)
    {
      out.push_back(lsp::DocumentHighlight{
        .range = lsp::Range{
          .start = {static_cast<lsp::Uint>(site.line), static_cast<lsp::Uint>(site.start)},
          .end = {static_cast<lsp::Uint>(site.line), static_cast<lsp::Uint>(site.end)},
        },
        .kind = lsp::DocumentHighlightKind::Text,
      });
    }
    return out;
  }

  const auto lines = splitLines(text);
  if (position.line >= lines.size())
  {
    return {};
  }
  const auto line = utf8To16(lines[position.line]);
  const auto word = wordAtCaret(line, position.character);
  if (!word)
  {
    return {};
  }
  return textualHighlights(lines, *word);
}

} // namespace vertexadt
