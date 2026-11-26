#include "semantic_tokens.h"

#include "catalog/catalog.h"
#include "debug_trace.h"
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
  // Debug traces only (docs/adr/0025-debug-trace-fast-path.md). Appended so
  // the indices above never move.
  "keyword",
  "string",
  "number",
  "comment",
  "function",
  "debugValue",
  "debugMarker",
};

constexpr int kVariable = 0;
constexpr int kPvector = 17;
constexpr int kKeyword = 30;
constexpr int kString = 31;
constexpr int kNumber = 32;
constexpr int kComment = 33;
constexpr int kFunction = 34;
constexpr int kDebugValue = 35;
constexpr int kDebugMarker = 36;
static_assert(kLegendTypes[kPvector] == "pvectorVariable");
static_assert(kLegendTypes[kPvector + 4] == "nvectorVariable");
static_assert(kLegendTypes[kKeyword] == "keyword");
static_assert(kLegendTypes[kDebugMarker] == "debugMarker");
static_assert(std::size(kLegendTypes) == kDebugMarker + 1);

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

// The token at byte `column` on `line`: typeWriteAt in its named section.
// Zero rows → live-then-neighborhood [type] comment. Rows but none visible →
// none.
auto currentVariableType(
  std::string_view name,
  int line,
  int column,
  const std::string& section,
  const TypeTimeline& types,
  const VariableExtract& live,
  const VariableExtract& neighborhood) -> std::optional<std::string>
{
  const auto it = types.byName.find(std::string(name));
  if (it == types.byName.end() || it->second.empty())
  {
    return hoverTypeFromExtracts(live, neighborhood, name);
  }
  if (const auto* write = typeWriteAt(types, name, line, column, section))
  {
    return write->type;
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
  options.range = true;
  return options;
}

namespace {

// Control keywords colored as `keyword` in traces (the grammar's
// keyword.control list).
bool isControlKeyword(std::string_view name)
{
  static constexpr std::string_view kKeywords[] = {
    "if", "then", "else", "endif", "loop", "endloop", "return", "exitif", "and", "or",
    "not", "call", "goto", "debug",
  };
  if (name.size() > 7)
  {
    return false;
  }
  char lower[8] = {};
  for (std::size_t i = 0; i < name.size(); ++i)
  {
    const auto ch = name[i];
    lower[i] = (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
  }
  const std::string_view lowered(lower, name.size());
  for (const auto keyword : kKeywords)
  {
    if (keyword == lowered)
    {
      return true;
    }
  }
  return false;
}

bool isCatalogName(std::string_view name)
{
  if (catalog::isKeywordLabel(name))
  {
    return true;
  }
  std::string lower(name);
  for (auto& ch : lower)
  {
    if (ch >= 'A' && ch <= 'Z')
    {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  return catalog::findCommand(lower) != nullptr;
}

bool isIdentStart(char ch)
{
  return isLetter(ch) || ch == '_';
}

bool isIdentChar(char ch)
{
  return isLetter(ch) || isDigit(ch) || ch == '_';
}

// Byte → UTF-16 column on one line; identity for ASCII lines.
struct Utf16Columns
{
  std::string_view line;
  bool ascii = true;

  explicit Utf16Columns(std::string_view text)
    : line(text)
  {
    for (const auto ch : text)
    {
      if (static_cast<unsigned char>(ch) >= 0x80)
      {
        ascii = false;
        break;
      }
    }
  }

  [[nodiscard]] int at(std::size_t byte) const
  {
    return ascii ? static_cast<int>(byte) : navigation::utf16Offset(line, byte);
  }
};

struct TraceTokenWriter
{
  lsp::Array<lsp::Uint>& data;
  int prevLine = 0;
  int prevStart = 0;

  void push(int line, const Utf16Columns& columns, std::size_t begin, std::size_t end, int type)
  {
    if (end <= begin)
    {
      return;
    }
    const auto start = columns.at(begin);
    const auto stop = columns.at(end);
    pushToken(data, prevLine, prevStart, line, start, stop - start, type);
  }
};

int traceIdentifierType(std::string_view name, const DebugAssignmentIndex* index)
{
  if (isControlKeyword(name))
  {
    return kKeyword;
  }
  if (isCatalogName(name))
  {
    return kFunction;
  }
  if (name.ends_with('$'))
  {
    return kVariable;
  }
  if (isPxyznSuffix(name))
  {
    std::size_t i = name.size();
    while (i > 0 && isDigit(name[i - 1]))
    {
      --i;
    }
    switch (name[i - 1])
    {
      case 'P': return kPvector;
      case 'X': return kPvector + 1;
      case 'Y': return kPvector + 2;
      case 'Z': return kPvector + 3;
      default: return kPvector + 4;
    }
  }
  if (index && index->byName.contains(std::string(name)))
  {
    return kVariable;
  }
  return -1;
}

void tokenizeTraceCode(
  TraceTokenWriter& writer,
  int lineNumber,
  std::string_view line,
  const Utf16Columns& columns,
  std::size_t begin,
  std::size_t end,
  const DebugAssignmentIndex* index)
{
  if (begin < end && line[begin] == '!')
  {
    writer.push(lineNumber, columns, begin, end, kComment);
    return;
  }
  for (auto i = begin; i < end;)
  {
    const auto ch = line[i];
    if (ch == ';')
    {
      auto j = i + 1;
      while (j < end && (line[j] == ' ' || line[j] == '\t'))
      {
        ++j;
      }
      if (j < end && line[j] == '!')
      {
        writer.push(lineNumber, columns, i, end, kComment);
        return;
      }
      ++i;
      continue;
    }
    if (ch == '"')
    {
      auto j = i + 1;
      while (j < end && line[j] != '"')
      {
        ++j;
      }
      j = std::min(j + 1, end);
      writer.push(lineNumber, columns, i, j, kString);
      i = j;
      continue;
    }
    if (isDigit(ch) && (i == begin || !isIdentChar(line[i - 1])))
    {
      auto j = i;
      while (j < end && isDigit(line[j]))
      {
        ++j;
      }
      if (j + 1 < end && line[j] == '.' && isDigit(line[j + 1]))
      {
        ++j;
        while (j < end && isDigit(line[j]))
        {
          ++j;
        }
      }
      if (j < end && isIdentChar(line[j]))
      {
        // 12abc: not a number; skip the run.
        while (j < end && isIdentChar(line[j]))
        {
          ++j;
        }
        i = j;
        continue;
      }
      writer.push(lineNumber, columns, i, j, kNumber);
      i = j;
      continue;
    }
    if (isIdentStart(ch))
    {
      auto j = i + 1;
      while (j < end && isIdentChar(line[j]))
      {
        ++j;
      }
      if (j < end && line[j] == '$')
      {
        ++j;
      }
      const auto type = traceIdentifierType(line.substr(i, j - i), index);
      if (type >= 0)
      {
        writer.push(lineNumber, columns, i, j, type);
      }
      i = j;
      continue;
    }
    ++i;
  }
}

} // namespace

auto debugTraceSemanticTokens(
  const DebugTrace& trace,
  int firstLine,
  int endLine,
  const std::function<void()>& throwIfCanceled) -> lsp::SemanticTokens
{
  lsp::SemanticTokens tokens;
  tokens.resultId = std::to_string(trace.version());
  const auto indexHolder = trace.index();
  const auto* index = indexHolder.get();
  TraceTokenWriter writer{.data = tokens.data};
  firstLine = std::max(firstLine, 0);
  endLine = std::min(endLine, trace.lineCount());
  for (int i = firstLine; i < endLine; ++i)
  {
    if (throwIfCanceled && ((i - firstLine) & 0x3FF) == 0x3FF)
    {
      throwIfCanceled();
    }
    const auto line = trace.line(i);
    const auto parts = splitDebugLine(line);
    if (parts.codeBegin == parts.codeEnd && parts.valueBegin == parts.valueEnd)
    {
      continue;
    }
    const Utf16Columns columns(line);
    if (parts.kind == DebugLineParts::Kind::Structural)
    {
      writer.push(i, columns, parts.codeBegin, parts.codeEnd, kDebugMarker);
    }
    else
    {
      tokenizeTraceCode(writer, i, line, columns, parts.codeBegin, parts.codeEnd, index);
    }
    writer.push(i, columns, parts.valueBegin, parts.valueEnd, kDebugValue);
    writer.push(i, columns, parts.markerBegin, parts.markerEnd, kDebugMarker);
  }
  return tokens;
}

auto semanticTokensInRange(const lsp::SemanticTokens& full, const lsp::Range& range)
  -> lsp::SemanticTokens
{
  lsp::SemanticTokens out;
  out.resultId = full.resultId;
  const auto first = static_cast<int>(range.start.line);
  const auto last = static_cast<int>(range.end.line);
  int line = 0;
  int start = 0;
  int prevLine = 0;
  int prevStart = 0;
  for (std::size_t k = 0; k + 4 < full.data.size(); k += 5)
  {
    const auto deltaLine = static_cast<int>(full.data[k]);
    const auto deltaStart = static_cast<int>(full.data[k + 1]);
    line += deltaLine;
    start = deltaLine == 0 ? start + deltaStart : deltaStart;
    if (line < first)
    {
      continue;
    }
    if (line > last)
    {
      break;
    }
    pushToken(
      out.data,
      prevLine,
      prevStart,
      line,
      start,
      static_cast<int>(full.data[k + 2]),
      static_cast<int>(full.data[k + 3]));
    out.data[out.data.size() - 1] = full.data[k + 4];
  }
  return out;
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
        name,
        i,
        static_cast<int>(word.start),
        section,
        ctx.prep->types,
        ctx.prep->live,
        ctx.prep->neighborhood);
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
