#include "semantic_tokens.h"

#include "catalog/catalog.h"
#include "definition.h"
#include "text_util.h"
#include "validate.h"

#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vertexadt {
namespace {

constexpr std::string_view kLegendTypes[] = {
  "variable",
  "iteratorVariable",
  "edgeVariable",
  "selemVariable",
  "pieceEndVariable",
  "vxgeomVariable",
  "pieceVariable",
  "sheetVariable",
  "areaVariable",
  "jointVariable",
  "pointVariable",
  "slopeVariable",
  "polylineVariable",
  "arrayVariable",
  "valueVariable",
  "wallVariable",
  "booleanVariable",
  "pvectorVariable",
  "xvectorVariable",
  "yvectorVariable",
  "zvectorVariable",
  "nvectorVariable",
  "lineVariable",
  "hatchVariable",
  "textVariable",
  "dvectorVariable",
  "macroVariable",
  "databaselistVariable",
  "databaseVariable",
  "validatedCommandVariable",
};

// objectTypePatterns key order; legend index is this index + 1.
constexpr std::string_view kObjectTypes[] = {
  "iterator",
  "edge",
  "selemobj",
  "profileend",
  "vxgeom",
  "profile",
  "sheet",
  "area",
  "joint",
  "point",
  "slope",
  "polyline",
  "array",
  "value",
  "wall",
  "boolean",
  "pvector",
  "xvector",
  "yvector",
  "zvector",
  "nvector",
  "line",
  "hatch",
  "text",
  "dvector",
  "macro",
  "databaselist",
  "database",
  "validatedCommand",
};

bool isLetter(char ch)
{
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

bool isDigit(char ch)
{
  return ch >= '0' && ch <= '9';
}

bool startsWithTrimmed(std::string_view trimmed, std::string_view prefix)
{
  return trimmed.size() >= prefix.size() && trimmed.substr(0, prefix.size()) == prefix;
}

// JS /[PXYZN](\d+)?$/.test(name)
bool isPxyznSuffix(std::string_view name)
{
  if (name.empty())
  {
    return false;
  }
  std::size_t i = name.size();
  while (i > 0 && isDigit(name[i - 1]))
  {
    --i;
  }
  if (i == 0)
  {
    return false;
  }
  const auto ch = name[i - 1];
  return ch == 'P' || ch == 'X' || ch == 'Y' || ch == 'Z' || ch == 'N';
}

int tokenTypeForObjectType(std::string_view objectType)
{
  if (objectType.empty() || objectType == "generic")
  {
    return 0;
  }
  for (std::size_t i = 0; i < std::size(kObjectTypes); ++i)
  {
    if (kObjectTypes[i] == objectType)
    {
      return static_cast<int>(i + 1);
    }
  }
  return 0;
}

auto emptyTokens(int version) -> lsp::SemanticTokens
{
  lsp::SemanticTokens tokens;
  tokens.resultId = std::to_string(version);
  return tokens;
}

auto hoverTypeFrom(const std::vector<HoverTuple>& tuples, std::string_view name)
  -> std::optional<std::string>
{
  for (const auto& tuple : tuples)
  {
    if (tuple.name != name || !tuple.comment)
    {
      continue;
    }
    const auto& comment = *tuple.comment;
    const auto open = comment.find('[');
    const auto close = comment.find(']', open == std::string::npos ? 0 : open);
    if (open == std::string::npos || close == std::string::npos || close <= open)
    {
      return std::nullopt;
    }
    return toLowerAscii(trim(std::string_view(comment).substr(open + 1, close - open - 1)));
  }
  return std::nullopt;
}

auto hoverTypeFromExtracts(const VariableExtract& live, const VariableExtract& neighborhood, std::string_view name)
  -> std::optional<std::string>
{
  if (auto found = hoverTypeFrom(live.hoverVariables, name))
  {
    return found;
  }
  if (auto found = hoverTypeFrom(live.hoverStringVariables, name))
  {
    return found;
  }
  if (auto found = hoverTypeFrom(neighborhood.hoverVariables, name))
  {
    return found;
  }
  if (auto found = hoverTypeFrom(neighborhood.hoverStringVariables, name))
  {
    return found;
  }
  return std::nullopt;
}

using WritesByName = std::unordered_map<std::string, std::vector<const TypeWrite*>>;

auto indexWrites(const TypeTimeline& timeline) -> WritesByName
{
  WritesByName byName;
  byName.reserve(timeline.writes.size());
  for (const auto& write : timeline.writes)
  {
    byName[write.name].push_back(&write);
  }
  return byName;
}

// Same named section, latest write with line <= token line. Zero rows →
// live-then-neighborhood [type] comment. Rows but none in this section → none.
auto currentVariableType(
  std::string_view name,
  int line,
  const std::string& section,
  const WritesByName& byName,
  const VariableExtract& live,
  const VariableExtract& neighborhood) -> std::optional<std::string>
{
  const auto it = byName.find(std::string(name));
  if (it == byName.end() || it->second.empty())
  {
    return hoverTypeFromExtracts(live, neighborhood, name);
  }

  const TypeWrite* closest = nullptr;
  for (const auto* write : it->second)
  {
    if (write->line <= line && write->section == section)
    {
      if (!closest || write->line >= closest->line)
      {
        closest = write;
      }
    }
  }
  if (closest)
  {
    return closest->type;
  }
  return std::nullopt;
}

struct TokenWord
{
  std::size_t start = 0;
  std::size_t end = 0;
};

// JS /\b([a-zA-Z][a-zA-Z0-9_]*)(?!\$)\b/ — no hyphen, no trailing $.
auto tokenWords(std::string_view line) -> std::vector<TokenWord>
{
  std::vector<TokenWord> words;
  const auto n = line.size();
  for (std::size_t i = 0; i < n;)
  {
    if (!isLetter(line[i]) || !navigation::isJsBoundary(line, i))
    {
      ++i;
      continue;
    }
    auto j = i + 1;
    while (j < n && navigation::isJsWordChar(line[j]))
    {
      ++j;
    }
    if (j < n && line[j] == '$')
    {
      i = j;
      continue;
    }
    if (!navigation::isJsBoundary(line, j))
    {
      ++i;
      continue;
    }
    words.push_back(TokenWord{.start = i, .end = j});
    i = j;
  }
  return words;
}

void pushToken(
  lsp::Array<lsp::Uint>& data,
  int& prevLine,
  int& prevStart,
  int line,
  int start,
  int length,
  int tokenType)
{
  const auto deltaLine = line - prevLine;
  const auto deltaStart = deltaLine == 0 ? start - prevStart : start;
  data.push_back(static_cast<lsp::Uint>(deltaLine));
  data.push_back(static_cast<lsp::Uint>(deltaStart));
  data.push_back(static_cast<lsp::Uint>(length));
  data.push_back(static_cast<lsp::Uint>(tokenType));
  data.push_back(0);
  prevLine = line;
  prevStart = start;
}

} // namespace

auto semanticTokensOptions() -> lsp::SemanticTokensOptions
{
  lsp::SemanticTokensOptions options;
  options.legend.tokenTypes.reserve(std::size(kLegendTypes));
  for (const auto type : kLegendTypes)
  {
    options.legend.tokenTypes.emplace_back(type);
  }
  options.legend.tokenModifiers = {"readonly", "declaration"};
  options.full = true;
  return options;
}

auto semanticTokensFull(const SemanticTokensRequest& request) -> lsp::SemanticTokens
{
  if (!request.semanticHighlightingEnabled
    || request.snapshot.languageId != "vertexadt")
  {
    return emptyTokens(request.snapshot.version);
  }

  ValidateContext ctx;
  ctx.settings.strictModeEnabled = request.strictModeEnabled;
  ctx.stores = ValidateStores{
    .extracts = request.extracts,
    .linkedFiles = request.linkedFiles,
    .layout = request.layout,
    .textForPath = request.textForPath,
  };
  ctx.throwIfCanceled = request.throwIfCanceled;
  ctx.prep = request.prepare
    ? request.prepare()
    : buildDocumentPrep(request.snapshot, ctx.settings, ctx.stores, ctx.throwIfCanceled);

  if (ctx.prep->types.writes.empty())
  {
    return emptyTokens(request.snapshot.version);
  }

  const auto byName = indexWrites(ctx.prep->types);
  lsp::SemanticTokens tokens;
  tokens.resultId = std::to_string(request.snapshot.version);
  int prevLine = 0;
  int prevStart = 0;
  const auto debugCommands = request.debugValidationCommands;
  const auto skipPxyzn = !request.strictModeEnabled;

  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    ctx.pollCancel();
    const auto raw = ctx.prep->lines[static_cast<std::size_t>(i)];
    if (isDebugDump(raw))
    {
      break;
    }

    const auto trimmed = trim(raw);
    if (startsWithTrimmed(trimmed, "!")
      || trimmed.empty()
      || startsWithTrimmed(trimmed, "#include")
      || startsWithTrimmed(trimmed, "##included_from")
      || startsWithTrimmed(trimmed, "#")
      || startsWithTrimmed(trimmed, "call ")
      || startsWithTrimmed(trimmed, "debug "))
    {
      continue;
    }

    const auto stripped = stripValidationComment(raw);
    if (trim(stripped).ends_with("##"))
    {
      continue;
    }

    const auto section = sectionNameForLine(i, ctx.prep->sections);
    for (const auto& word : tokenWords(stripped))
    {
      const auto name = stripped.substr(word.start, word.end - word.start);
      if (isInsideQuotes(stripped, word.start))
      {
        continue;
      }
      if (skipPxyzn && isPxyznSuffix(name))
      {
        continue;
      }
      if (debugCommands)
      {
        if (catalog::findCommand(name))
        {
          const auto start = navigation::utf16Offset(stripped, word.start);
          const auto end = navigation::utf16Offset(stripped, word.end);
          pushToken(
            tokens.data,
            prevLine,
            prevStart,
            i,
            start,
            end - start,
            tokenTypeForObjectType("validatedCommand"));
          continue;
        }
      }
      else if (catalog::isKeywordLabel(name))
      {
        continue;
      }

      const auto type = currentVariableType(
        name, i, section, byName, ctx.prep->live, ctx.prep->neighborhood);
      if (!type)
      {
        continue;
      }
      const auto tokenType = tokenTypeForObjectType(*type);
      if (tokenType == 0)
      {
        continue;
      }
      const auto start = navigation::utf16Offset(stripped, word.start);
      const auto end = navigation::utf16Offset(stripped, word.end);
      pushToken(tokens.data, prevLine, prevStart, i, start, end - start, tokenType);
    }
  }

  return tokens;
}

} // namespace vertexadt
