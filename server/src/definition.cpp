#include "definition.h"

#include "parent_context.h"

#include "catalog/catalog.h"
#include "debug_dump.h"
#include "text_util.h"

#include <cstddef>
#include <string>
#include <vector>

namespace vertexadt {
namespace {

bool isWordChar(char ch)
{
  return (ch >= 'A' && ch <= 'Z')
    || (ch >= 'a' && ch <= 'z')
    || (ch >= '0' && ch <= '9')
    || ch == '_';
}

bool isLetter(char ch)
{
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

bool isNameTail(char ch)
{
  return isWordChar(ch) || ch == '-';
}

// JS \s on the ASCII range (the files are ASCII in practice).
bool isJsSpace(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r';
}

char lowerAscii(char ch)
{
  return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

// JS \b at byte index `at` (non-ASCII bytes are non-word, as in UTF-16 JS).
bool isBoundary(std::string_view line, std::size_t at)
{
  const bool before = at > 0 && isWordChar(line[at - 1]);
  const bool after = at < line.size() && isWordChar(line[at]);
  return before != after;
}

bool icaseAt(std::string_view line, std::size_t at, std::string_view name)
{
  if (at + name.size() > line.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < name.size(); ++i)
  {
    if (lowerAscii(line[at + i]) != lowerAscii(name[i]))
    {
      return false;
    }
  }
  return true;
}

auto skipSpaces(std::string_view line, std::size_t at) -> std::size_t
{
  while (at < line.size() && isJsSpace(line[at]))
  {
    ++at;
  }
  return at;
}

auto stripDollar(std::string_view name) -> std::string_view
{
  if (!name.empty() && name.back() == '$')
  {
    name.remove_suffix(1);
  }
  return name;
}

// /\bNAME/i at `at`. NAME starts with a letter, so \b means "no word char before".
bool nameStartsAt(std::string_view line, std::size_t at, std::string_view name)
{
  return isBoundary(line, at) && icaseAt(line, at, name);
}

// /\bNAME(?:\([^)]*\))?\s*=/i
bool matchesAssign(std::string_view line, std::string_view name)
{
  for (std::size_t at = 0; at + name.size() <= line.size(); ++at)
  {
    if (!nameStartsAt(line, at, name))
    {
      continue;
    }
    auto q = at + name.size();
    if (q < line.size() && line[q] == '(')
    {
      const auto close = line.find(')', q + 1);
      if (close == std::string_view::npos)
      {
        continue;
      }
      q = close + 1;
    }
    q = skipSpaces(line, q);
    if (q < line.size() && line[q] == '=')
    {
      return true;
    }
  }
  return false;
}

// /\bset\s+NAME_WITHOUT_DOLLAR\b/i
bool matchesSet(std::string_view line, std::string_view name)
{
  const auto bare = stripDollar(name);
  if (bare.empty())
  {
    return false;
  }
  for (std::size_t at = 0; at + 3 < line.size(); ++at)
  {
    if (!isBoundary(line, at) || !icaseAt(line, at, "set"))
    {
      continue;
    }
    auto q = at + 3;
    if (q >= line.size() || !isJsSpace(line[q]))
    {
      continue;
    }
    q = skipSpaces(line, q);
    if (icaseAt(line, q, bare) && isBoundary(line, q + bare.size()))
    {
      return true;
    }
  }
  return false;
}

// /\(\s*NAME\s*\)/i
bool matchesParenOutput(std::string_view line, std::string_view name)
{
  for (std::size_t at = 0; at < line.size(); ++at)
  {
    if (line[at] != '(')
    {
      continue;
    }
    auto q = skipSpaces(line, at + 1);
    if (!icaseAt(line, q, name))
    {
      continue;
    }
    q = skipSpaces(line, q + name.size());
    if (q < line.size() && line[q] == ')')
    {
      return true;
    }
  }
  return false;
}

bool lineDeclaresVariable(std::string_view line, std::string_view name)
{
  return matchesAssign(line, name) || matchesSet(line, name);
}

auto firstNameStart(std::string_view line, std::string_view name) -> std::optional<std::size_t>
{
  for (std::size_t at = 0; at + name.size() <= line.size(); ++at)
  {
    if (nameStartsAt(line, at, name))
    {
      return at;
    }
  }
  return std::nullopt;
}

// Node findDeclarationColumn: if any declaration-ish pattern matches, the first
// /\bNAME/i; else 0. Returns a byte index into `line`.
auto declarationColumn(std::string_view line, std::string_view name) -> std::size_t
{
  if (matchesAssign(line, name) || matchesSet(line, name) || matchesParenOutput(line, name))
  {
    if (const auto at = firstNameStart(line, name))
    {
      return *at;
    }
  }
  return 0;
}

auto utf16Offset(std::string_view line, std::size_t byteIndex) -> int
{
  int units = 0;
  const auto end = std::min(byteIndex, line.size());
  for (std::size_t i = 0; i < end; ++i)
  {
    const auto byte = static_cast<unsigned char>(line[i]);
    if ((byte & 0xC0) == 0x80)
    {
      continue;
    }
    units += (byte >= 0xF0) ? 2 : 1;
  }
  return units;
}

// JS text.split(/\r?\n/).
auto splitJsLines(std::string_view text) -> std::vector<std::string_view>
{
  auto lines = splitLines(text);
  for (auto& line : lines)
  {
    if (!line.empty() && line.back() == '\r')
    {
      line.remove_suffix(1);
    }
  }
  return lines;
}

struct Target
{
  int line = 0;
  int character = 0;
};

auto targetOnLine(std::string_view rawLine, int line, std::string_view name) -> Target
{
  const auto stripped = stripValidationComment(rawLine);
  const auto column = declarationColumn(stripped, name);
  return {.line = line, .character = utf16Offset(stripped, column)};
}

// Node findVariableDeclarationInLines: beforeLine-1 … 0, skip trimmed `!`
// lines, never the cursor line itself.
auto declarationInLines(
  const std::vector<std::string_view>& lines,
  std::string_view name,
  int beforeLine) -> std::optional<Target>
{
  const auto last = static_cast<int>(lines.size()) - 1;
  for (int i = std::min(beforeLine - 1, last); i >= 0; --i)
  {
    const auto raw = lines[static_cast<std::size_t>(i)];
    if (const auto trimmed = trim(raw); !trimmed.empty() && trimmed.front() == '!')
    {
      continue;
    }
    const auto line = stripValidationComment(raw);
    if (!lineDeclaresVariable(line, name))
    {
      continue;
    }
    return Target{.line = i, .character = utf16Offset(line, declarationColumn(line, name))};
  }
  return std::nullopt;
}

// `##included_from` sites at or before the cursor line, in source order.
auto includedFromBeforeLine(const std::vector<std::string_view>& lines, int beforeLine)
  -> std::vector<std::string>
{
  constexpr std::string_view kMarker = "##included_from";
  std::vector<std::string> includes;
  const auto last = std::min(beforeLine, static_cast<int>(lines.size()) - 1);
  for (int i = 0; i <= last; ++i)
  {
    const auto trimmed = trim(lines[static_cast<std::size_t>(i)]);
    if (trimmed.substr(0, kMarker.size()) != kMarker)
    {
      continue;
    }
    // /##included_from\s+(.+?)$/i on the trimmed line.
    auto q = kMarker.size();
    if (q >= trimmed.size() || !isJsSpace(trimmed[q]))
    {
      continue;
    }
    q = skipSpaces(trimmed, q);
    if (q >= trimmed.size())
    {
      continue;
    }
    includes.emplace_back(trim(trimmed.substr(q)));
  }
  return includes;
}

auto location(const lsp::Uri& uri, Target target, std::size_t nameLength)
  -> lsp::TextDocumentDefinitionResult
{
  const auto start = lsp::Position{
    .line = static_cast<lsp::Uint>(target.line),
    .character = static_cast<lsp::Uint>(target.character),
  };
  const auto end = lsp::Position{
    .line = start.line,
    .character = static_cast<lsp::Uint>(target.character + static_cast<int>(nameLength)),
  };
  return lsp::Definition{lsp::Location{.uri = uri, .range = {.start = start, .end = end}}};
}

} // namespace

namespace navigation {

bool isJsWordChar(char ch)
{
  return isWordChar(ch);
}

bool isJsBoundary(std::string_view line, std::size_t at)
{
  return isBoundary(line, at);
}

bool icaseAt(std::string_view line, std::size_t at, std::string_view name)
{
  return vertexadt::icaseAt(line, at, name);
}

auto utf16Offset(std::string_view line, std::size_t byteIndex) -> int
{
  return vertexadt::utf16Offset(line, byteIndex);
}

auto splitJsLines(std::string_view text) -> std::vector<std::string_view>
{
  return vertexadt::splitJsLines(text);
}

} // namespace navigation

auto definitionWordAtPosition(std::string_view lineText, int character)
  -> std::optional<std::string>
{
  const auto n = lineText.size();
  std::size_t pos = 0;
  while (pos < n)
  {
    // Find the next match start: a letter at a \b.
    std::size_t start = pos;
    std::size_t end = 0;
    bool found = false;
    for (; start < n; ++start)
    {
      if (!isLetter(lineText[start]) || !isBoundary(lineText, start))
      {
        continue;
      }
      auto greedy = start + 1;
      while (greedy < n && isNameTail(lineText[greedy]))
      {
        ++greedy;
      }
      // \$? then \b: `$` is non-word, so the char after it must be a word char.
      if (greedy < n && lineText[greedy] == '$' && greedy + 1 < n
          && isWordChar(lineText[greedy + 1]))
      {
        end = greedy + 1;
        found = true;
        break;
      }
      for (auto k = greedy; k > start; --k)
      {
        if (isBoundary(lineText, k))
        {
          end = k;
          found = true;
          break;
        }
      }
      if (found)
      {
        break;
      }
    }
    if (!found)
    {
      return std::nullopt;
    }
    const auto startUnits = utf16Offset(lineText, start);
    const auto endUnits = utf16Offset(lineText, end);
    if (character >= startUnits && character <= endUnits)
    {
      if (isInsideQuotes(lineText, start))
      {
        return std::nullopt;
      }
      return std::string(lineText.substr(start, end - start));
    }
    pos = end;
  }
  return std::nullopt;
}

bool isValidDefinitionTarget(std::string_view word)
{
  const auto lookup = stripDollar(word);
  if (catalog::isKeywordLabel(lookup) || catalog::isKeywordLabel(word))
  {
    return false;
  }
  return catalog::findCommand(lookup) == nullptr;
}

auto locateDefinition(
  const Snapshot& snapshot,
  std::string_view word,
  int cursorLine,
  const LibraryLayout& layout,
  const LinkedTextReader& textForPath,
  const WorkspaceIndex* index) -> std::optional<DefinitionSite>
{
  const auto lines = splitJsLines(snapshot.text);
  if (cursorLine < 0 || cursorLine >= static_cast<int>(lines.size()))
  {
    return std::nullopt;
  }

  // 1. Debug dump: most-recent assignment on lines 0…cursor. Node's `$` retry
  // runs extractBaseVariableName, which keeps a trailing `$`, so it repeats
  // the same lookup; copied as-is.
  if (isDebugDump(snapshot.text))
  {
    if (const auto assignment = findMostRecentDebugValue(snapshot.text, cursorLine, word))
    {
      const auto line = assignment->line;
      const auto target = targetOnLine(lines[static_cast<std::size_t>(line)], line, word);
      return DefinitionSite{
        .uri = snapshot.uri, .line = target.line, .character = target.character};
    }
  }

  // 2. Nearest prior declaration in this buffer.
  if (const auto local = declarationInLines(lines, word, cursorLine))
  {
    return DefinitionSite{
      .uri = snapshot.uri, .line = local->line, .character = local->character};
  }

  // 3. One hop of `##included_from`, last include first, buffer-wins. With
  // an index, only above the parent's earliest include of this file.
  const auto includes = includedFromBeforeLine(lines, cursorLine);
  const auto written = index
    ? resolveParentLinks(snapshot.text, pathFromUri(snapshot.uri), layout, index, false).written
    : std::vector<WrittenDirective>{};
  for (auto it = includes.rbegin(); it != includes.rend(); ++it)
  {
    const auto path = layout.findIncludedFile(*it);
    if (!path || !textForPath)
    {
      continue;
    }
    const auto text = textForPath(*path);
    if (!text)
    {
      continue;
    }
    const auto includedLines = splitJsLines(*text);
    auto searchTo = static_cast<int>(includedLines.size()) - 1;
    for (const auto& directive : written)
    {
      if (directive.rawPath == *it && directive.cutoffLine)
      {
        searchTo = std::min(searchTo, *directive.cutoffLine - 1);
        break;
      }
    }
    const auto declaration = declarationInLines(includedLines, word, searchTo);
    if (declaration)
    {
      return DefinitionSite{
        .uri = uriFromPath(*path),
        .path = *path,
        .line = declaration->line,
        .character = declaration->character,
      };
    }
  }

  return std::nullopt;
}

auto inferredDefinitionSites(
  const Snapshot& snapshot,
  std::string_view word,
  int cursorLine,
  const LibraryLayout& layout,
  const LinkedTextReader& textForPath,
  const WorkspaceIndex* index,
  bool inferParents) -> std::vector<DefinitionSite>
{
  std::vector<DefinitionSite> sites;
  if (!index || !inferParents || !textForPath)
  {
    return sites;
  }
  const auto links = resolveParentLinks(snapshot.text, pathFromUri(snapshot.uri), layout, index, true);
  const auto childLines = splitLines(snapshot.text);
  const auto section = toLowerAscii(sectionNameForLine(cursorLine, parseFileSections(childLines)));
  if (links.writtenCoversFile
      || (!section.empty()
          && std::find(links.writtenSections.begin(), links.writtenSections.end(), section)
            != links.writtenSections.end()))
  {
    return sites;
  }
  for (const auto& parent : links.inferred)
  {
    auto cutoff = parent.wholeLine;
    for (const auto& [name, line] : parent.sectionLines)
    {
      if (!section.empty() && name == section && (!cutoff || line < *cutoff))
      {
        cutoff = line;
      }
    }
    if (!cutoff)
    {
      continue;
    }
    const auto text = textForPath(parent.absPath);
    if (!text)
    {
      continue;
    }
    const auto parentLines = splitJsLines(*text);
    if (const auto declaration = declarationInLines(parentLines, word, *cutoff - 1))
    {
      sites.push_back(DefinitionSite{
        .uri = uriFromPath(parent.absPath),
        .path = parent.absPath,
        .line = declaration->line,
        .character = declaration->character,
      });
    }
  }
  return sites;
}

auto findDefinition(const DefinitionContext& context) -> lsp::TextDocumentDefinitionResult
{
  const auto& snapshot = context.snapshot;
  if (snapshot.languageId != "vertexadt")
  {
    return nullptr;
  }
  const auto lines = splitJsLines(snapshot.text);
  const auto cursorLine = static_cast<int>(context.position.line);
  if (cursorLine < 0 || cursorLine >= static_cast<int>(lines.size()))
  {
    return nullptr;
  }
  const auto word = definitionWordAtPosition(
    lines[static_cast<std::size_t>(cursorLine)],
    static_cast<int>(context.position.character));
  if (!word || !isValidDefinitionTarget(*word))
  {
    return nullptr;
  }
  const auto site = locateDefinition(
    snapshot, *word, cursorLine, context.layout, context.textForPath, context.index);
  if (!site)
  {
    const auto sites = inferredDefinitionSites(
      snapshot, *word, cursorLine, context.layout, context.textForPath, context.index,
      context.inferParents);
    if (sites.empty())
    {
      return nullptr;
    }
    if (sites.size() == 1)
    {
      return location(sites.front().uri,
        Target{.line = sites.front().line, .character = sites.front().character}, word->size());
    }
    lsp::Array<lsp::Location> locations;
    for (const auto& inferred : sites)
    {
      const auto start = lsp::Position{
        .line = static_cast<lsp::Uint>(inferred.line),
        .character = static_cast<lsp::Uint>(inferred.character),
      };
      const auto end = lsp::Position{
        .line = start.line,
        .character = static_cast<lsp::Uint>(inferred.character + static_cast<int>(word->size())),
      };
      locations.push_back(lsp::Location{.uri = inferred.uri, .range = {.start = start, .end = end}});
    }
    return lsp::Definition{std::move(locations)};
  }
  return location(site->uri, Target{.line = site->line, .character = site->character},
    word->size());
}

} // namespace vertexadt
