#include "validate.h"
#include "utf16_text.h"

#include <algorithm>
#include <filesystem>
#include <cctype>
#include <iterator>
#include <regex>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

constexpr std::string_view kUnclosedResourceCode = "vertexadt/unclosed-resource";
constexpr std::string_view kCloseAlreadyOpenCode = "vertexadt/close-already-open";

enum class ResourceKind
{
  Database,
  File,
  // A `dwgopen_bg` port: global for the session, closed by `dwgclose`.
  Drawing,
};

enum class ResourceAction
{
  Open,
  Close,
};

struct IncludeSite
{
  std::string filePath;
  int line = 0;
  int startChar = 0;
  int endChar = 0;
  bool ignoreClose = false;
  std::string lineText;
};

struct ResourceEvent
{
  ResourceAction action = ResourceAction::Open;
  ResourceKind kind = ResourceKind::Database;
  std::string token;
  std::optional<std::string> nameLiteral;
  bool alreadyOpen = false;
  bool ignoreClose = false;
  std::string filePath;
  int line = 0;
  int startChar = 0;
  int endChar = 0;
  std::string lineText;
  std::string ns;
  std::optional<IncludeSite> fromIncludeSite;
};

struct ParsedPath
{
  std::string filePath;
  std::string targetSection;
};

struct CallTarget
{
  int index = 0;
  ParsedPath parsed;
};

struct IncludeLinkInfo
{
  std::string fromPath;
  std::string targetPath;
  int line = 0;
  std::string lineText;
};

struct WalkState
{
  bool shareDb = false;
  bool shareFiles = false;
  std::optional<IncludeSite> fromIncludeSite;
  std::string dbNs = "root-db";
  std::string fileNs = "root-file";
  std::string drawingNs = "drawing";
  int* nsCounter = nullptr;

  [[nodiscard]] auto nsByKind(ResourceKind kind) const -> const std::string&
  {
    if (kind == ResourceKind::Drawing)
    {
      return drawingNs;
    }
    return kind == ResourceKind::Database ? dbNs : fileNs;
  }

  [[nodiscard]] auto freshNs() const -> std::string
  {
    return "ns-" + std::to_string(++(*nsCounter));
  }
};

struct RelatedIssue
{
  lsp::Uri uri;
  int line = 0;
  int startChar = 0;
  int endChar = 0;
  std::string message;
};

struct UnclosedIssue
{
  int line = 0;
  int startChar = 0;
  int endChar = 0;
  std::string message;
  // A `dbclose` of an ALREADY_OPEN alias, not an unclosed open.
  bool closesAlias = false;
  std::vector<RelatedIssue> related;
};

[[nodiscard]] bool hasLineContinuation(std::string_view line)
{
  const auto stripped = stripValidationComment(line);
  return trim(stripped).ends_with('\\');
}

[[nodiscard]] bool isSectionKeyword(std::string_view token)
{
  const auto lower = toLowerAscii(token);
  return lower == "section" || lower == "sectionoffile";
}

ParsedPath parseIncludeCallPathAndSection(std::string_view rawPath, std::string_view remainder)
{
  auto pathPart = replaceBackslashes(stripPathQuotes(rawPath));
  pathPart = std::string(trim(pathPart));
  std::string targetSection;
  static const std::regex pathSection{R"re(^(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$)re"};
  std::smatch match;
  if (std::regex_match(pathPart, match, pathSection))
  {
    const auto token = match[2].str();
    if (!isSectionKeyword(token))
    {
      targetSection = token;
      pathPart = std::string(trim(match[1].str()));
    }
    else
    {
      static const std::regex stripSection{R"re(\s+SECTION(?:OFFILE)?$)re", std::regex::icase};
      pathPart = std::regex_replace(pathPart, stripSection, "");
      pathPart = std::string(trim(pathPart));
    }
  }
  const auto rem = trim(remainder);
  if (!rem.empty())
  {
    static const std::regex remRe{R"re(^([A-Za-z_][A-Za-z0-9_]*)\b)re"};
    std::cmatch remMatch;
    if (std::regex_search(rem.data(), rem.data() + rem.size(), remMatch, remRe)
        && !isSectionKeyword(remMatch[1].str()))
    {
      targetSection = remMatch[1].str();
    }
  }
  return ParsedPath{.filePath = std::move(pathPart), .targetSection = std::move(targetSection)};
}

[[nodiscard]] auto parseStringLiteral(std::string_view value) -> std::optional<std::string>
{
  const auto trimmed = trim(value);
  if (trimmed.size() < 2)
  {
    return std::nullopt;
  }
  const auto quote = trimmed.front();
  if (quote != '"' && quote != '\'')
  {
    return std::nullopt;
  }
  if (trimmed.back() != quote)
  {
    return std::nullopt;
  }
  const auto inner = trimmed.substr(1, trimmed.size() - 2);
  if (inner.find(quote) != std::string_view::npos)
  {
    return std::nullopt;
  }
  return std::string(inner);
}

[[nodiscard]] auto findMatchingParenIndex(std::string_view text, int openParenIndex) -> int
{
  if (openParenIndex < 0
      || static_cast<std::size_t>(openParenIndex) >= text.size()
      || text[static_cast<std::size_t>(openParenIndex)] != '(')
  {
    return -1;
  }
  int parenCount = 1;
  char activeQuote = 0;
  for (std::size_t i = static_cast<std::size_t>(openParenIndex) + 1; i < text.size(); ++i)
  {
    const auto ch = text[i];
    if (activeQuote)
    {
      if (ch == activeQuote)
      {
        activeQuote = 0;
      }
      continue;
    }
    if (ch == '"' || ch == '\'')
    {
      activeQuote = ch;
      continue;
    }
    if (ch == '(')
    {
      ++parenCount;
    }
    else if (ch == ')')
    {
      --parenCount;
      if (parenCount == 0)
      {
        return static_cast<int>(i);
      }
    }
  }
  return -1;
}

[[nodiscard]] auto parseParameters(std::string_view paramString) -> std::vector<std::string>
{
  std::vector<std::string> params;
  std::string current;
  char quoteChar = 0;
  for (const char ch : paramString)
  {
    if (quoteChar)
    {
      current.push_back(ch);
      if (ch == quoteChar)
      {
        quoteChar = 0;
      }
      continue;
    }
    if (ch == '"' || ch == '\'')
    {
      quoteChar = ch;
      current.push_back(ch);
      continue;
    }
    if (ch == ',')
    {
      params.push_back(std::string(trim(current)));
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  if (!trim(current).empty())
  {
    params.push_back(std::string(trim(current)));
  }
  return params;
}

[[nodiscard]] auto normalizeResourceName(std::string_view name) -> std::string
{
  return std::string(trim(replaceBackslashes(stripPathQuotes(name))));
}

[[nodiscard]] auto posixBasename(std::string_view name) -> std::string
{
  const auto slash = name.find_last_of('/');
  return std::string(slash == std::string_view::npos ? name : name.substr(slash + 1));
}

[[nodiscard]] auto resourceNameStem(std::string_view name) -> std::string
{
  const auto normalized = normalizeResourceName(name);
  auto base = posixBasename(normalized);
  static const std::regex extRe{R"re(\.[^.]+$)re"};
  return std::regex_replace(base, extRe, "");
}

[[nodiscard]] bool namesMatch(std::string_view left, std::string_view right)
{
  if (left.empty() || right.empty())
  {
    return false;
  }
  const auto a = normalizeResourceName(left);
  const auto b = normalizeResourceName(right);
  if (a.empty() || b.empty())
  {
    return false;
  }
  if (toLowerAscii(a) == toLowerAscii(b))
  {
    return true;
  }
  const auto baseA = toLowerAscii(posixBasename(a));
  const auto baseB = toLowerAscii(posixBasename(b));
  if (baseA == baseB)
  {
    return true;
  }
  return toLowerAscii(resourceNameStem(a)) == toLowerAscii(resourceNameStem(b));
}

// A drawing port expression as its handle text: spaces outside quotes removed.
// Case is folded later, with every other handle token.
[[nodiscard]] auto normalizeDrawingPort(std::string_view expression) -> std::string
{
  std::string out;
  char quote = 0;
  for (const char ch : trim(expression))
  {
    if (quote)
    {
      out.push_back(ch);
      if (ch == quote)
      {
        quote = 0;
      }
      continue;
    }
    if (ch == '"' || ch == '\'')
    {
      quote = ch;
      out.push_back(ch);
      continue;
    }
    if (!std::isspace(static_cast<unsigned char>(ch)))
    {
      out.push_back(ch);
    }
  }
  return out;
}

// The text after `dwgclose` up to the end of its `;` statement.
[[nodiscard]] auto statementTail(std::string_view text) -> std::string_view
{
  char quote = 0;
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    const auto ch = text[i];
    if (quote)
    {
      if (ch == quote)
      {
        quote = 0;
      }
      continue;
    }
    if (ch == '"' || ch == '\'')
    {
      quote = ch;
    }
    else if (ch == ';')
    {
      return text.substr(0, i);
    }
  }
  return text;
}

[[nodiscard]] bool tokensMatch(std::string_view left, std::string_view right)
{
  return toLowerAscii(trim(left)) == toLowerAscii(trim(right));
}

[[nodiscard]] auto collectCallTargets(std::string_view codeLine) -> std::vector<CallTarget>
{
  const std::string text(codeLine);
  static const std::regex patterns[] = {
    std::regex(R"re(call\s*\(\s*["']([^"']+)["'])re", std::regex::icase),
    std::regex(R"re(call\s*\(\s*(\$[^"',)]+))re", std::regex::icase),
    std::regex(R"re(\bcall\s+([^\s,(]+))re", std::regex::icase),
  };
  for (const auto& pattern : patterns)
  {
    std::vector<CallTarget> targets;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), last; it != last; ++it)
    {
      const auto index = static_cast<int>(it->position());
      if (isInsideQuotes(text, static_cast<std::size_t>(index)))
      {
        continue;
      }
      targets.push_back(CallTarget{
        .index = index,
        .parsed = parseIncludeCallPathAndSection((*it)[1].str(), {}),
      });
    }
    if (!targets.empty())
    {
      return targets;
    }
  }
  return {};
}

[[nodiscard]] auto collectResourceEventsFromLine(
  std::string_view rawLine,
  int lineIndex,
  const std::string& filePath,
  const WalkState& walkState) -> std::vector<ResourceEvent>
{
  std::vector<ResourceEvent> events;
  const auto ignoreClose = lineHasIgnoreClose(rawLine);
  const auto codeLine = stripValidationComment(rawLine);
  const auto trimmed = trim(codeLine);
  if (trimmed.empty() || trimmed.starts_with('!'))
  {
    return events;
  }

  static const std::regex resourceRe{
    R"re(\b(dbcreate|dbclose_by_name|dbopen|dbclose|open|close|dwgopen_bg)\s*\()re",
    std::regex::icase};
  for (std::sregex_iterator it(codeLine.begin(), codeLine.end(), resourceRe), last; it != last;
       ++it)
  {
    const auto index = static_cast<std::size_t>(it->position());
    if (isInsideQuotes(codeLine, index))
    {
      continue;
    }
    const auto commandName = toLowerAscii((*it)[1].str());
    const auto openParenIndex = static_cast<int>(index + it->length() - 1);
    const auto closeParenIndex = findMatchingParenIndex(codeLine, openParenIndex);
    if (closeParenIndex < 0)
    {
      continue;
    }
    const auto insideParams =
      parseParameters(codeLine.substr(static_cast<std::size_t>(openParenIndex + 1),
        static_cast<std::size_t>(closeParenIndex - openParenIndex - 1)));
    const auto afterParams =
      parseParameters(codeLine.substr(static_cast<std::size_t>(closeParenIndex + 1)));
    auto handleToken = insideParams.empty() ? std::string{} : std::string(trim(insideParams[0]));
    const auto firstAfterLiteral =
      afterParams.empty() ? std::optional<std::string>{} : parseStringLiteral(afterParams[0]);
    std::optional<std::string> nameLiteral = commandName == "dbclose_by_name"
      ? parseStringLiteral(insideParams.empty() ? std::string_view{} : insideParams[0])
      : firstAfterLiteral;
    // `dbopen(Id) "ALREADY_OPEN",Cid` or the function form `Cid = dbopen(Id,"ALREADY_OPEN",Cid)`.
    const auto isAlreadyOpen = [](const std::optional<std::string>& literal) {
      return literal && toLowerAscii(*literal) == "already_open";
    };
    const auto alreadyOpen = commandName == "dbopen"
      && (isAlreadyOpen(firstAfterLiteral)
        || (insideParams.size() >= 2 && isAlreadyOpen(parseStringLiteral(insideParams[1]))));

    ResourceAction action = ResourceAction::Open;
    ResourceKind kind = ResourceKind::Database;
    if (commandName == "dbcreate" || commandName == "dbopen")
    {
      action = ResourceAction::Open;
      kind = ResourceKind::Database;
    }
    else if (commandName == "open")
    {
      action = ResourceAction::Open;
      kind = ResourceKind::File;
    }
    else if (commandName == "dbclose" || commandName == "dbclose_by_name")
    {
      action = ResourceAction::Close;
      kind = ResourceKind::Database;
    }
    else if (commandName == "close")
    {
      action = ResourceAction::Close;
      kind = ResourceKind::File;
    }
    else if (commandName == "dwgopen_bg")
    {
      action = ResourceAction::Open;
      kind = ResourceKind::Drawing;
      handleToken = normalizeDrawingPort(handleToken);
    }
    else
    {
      continue;
    }

    if (kind != ResourceKind::Database)
    {
      nameLiteral.reset();
    }

    events.push_back(ResourceEvent{
      .action = action,
      .kind = kind,
      .token = commandName == "dbclose_by_name" ? std::string{} : handleToken,
      .nameLiteral = std::move(nameLiteral),
      .alreadyOpen = alreadyOpen,
      .ignoreClose = ignoreClose,
      .filePath = filePath,
      .line = lineIndex,
      .startChar = static_cast<int>(index),
      .endChar = closeParenIndex + 1,
      .lineText = std::string(rawLine),
      .ns = walkState.nsByKind(kind),
      .fromIncludeSite = walkState.fromIncludeSite,
    });
  }

  // `dwgclose Port$` / `dwgclose(Port$)`: a statement, so the port is the rest
  // of its `;` statement.
  static const std::regex dwgcloseRe{R"re(\bdwgclose\b)re", std::regex::icase};
  for (std::sregex_iterator it(codeLine.begin(), codeLine.end(), dwgcloseRe), last; it != last; ++it)
  {
    const auto index = static_cast<std::size_t>(it->position());
    if (isInsideQuotes(codeLine, index))
    {
      continue;
    }
    const auto afterIndex = index + static_cast<std::size_t>(it->length());
    const auto rawTail = statementTail(std::string_view(codeLine).substr(afterIndex));
    auto tail = trim(rawTail);
    if (tail.empty() || tail.front() == '=')
    {
      continue;
    }
    if (tail.front() == '(' && findMatchingParenIndex(tail, 0) == static_cast<int>(tail.size()) - 1)
    {
      tail = trim(tail.substr(1, tail.size() - 2));
    }
    const auto params = parseParameters(tail);
    if (params.empty())
    {
      continue;
    }
    events.push_back(ResourceEvent{
      .action = ResourceAction::Close,
      .kind = ResourceKind::Drawing,
      .token = normalizeDrawingPort(params[0]),
      .ignoreClose = ignoreClose,
      .filePath = filePath,
      .line = lineIndex,
      .startChar = static_cast<int>(index),
      .endChar = static_cast<int>(afterIndex + rawTail.find_last_not_of(" \t\r") + 1),
      .lineText = std::string(rawLine),
      .ns = walkState.nsByKind(ResourceKind::Drawing),
      .fromIncludeSite = walkState.fromIncludeSite,
    });
  }
  std::stable_sort(events.begin(), events.end(), [](const ResourceEvent& a, const ResourceEvent& b) {
    return a.startChar < b.startChar;
  });
  return events;
}

[[nodiscard]] auto parseFileCommonFlags(const std::vector<std::string>& lines)
  -> std::pair<bool, bool>
{
  bool commonDbases = false;
  bool commonFiles = false;
  static const std::regex dbRe{R"re(^common_dbases\s+1\b)re", std::regex::icase};
  static const std::regex fileRe{R"re(^common_files\s+1\b)re", std::regex::icase};
  for (const auto& line : lines)
  {
    const auto trimmed = std::string(trim(stripValidationComment(line)));
    if (std::regex_search(trimmed, dbRe))
    {
      commonDbases = true;
    }
    if (std::regex_search(trimmed, fileRe))
    {
      commonFiles = true;
    }
  }
  return {commonDbases, commonFiles};
}

[[nodiscard]] auto splitToOwnedLines(std::string_view text) -> std::vector<std::string>
{
  std::vector<std::string> owned;
  for (const auto line : splitLines(text))
  {
    owned.emplace_back(line);
  }
  return owned;
}

[[nodiscard]] auto tokenLabel(const ResourceEvent& event) -> std::string
{
  if (!event.token.empty())
  {
    return event.token;
  }
  if (event.nameLiteral && !event.nameLiteral->empty())
  {
    return *event.nameLiteral;
  }
  return "?";
}

[[nodiscard]] auto buildOpenMessage(const ResourceEvent& event, bool onIncludeLine) -> std::string
{
  const auto label = tokenLabel(event);
  if (onIncludeLine)
  {
    if (event.kind == ResourceKind::File)
    {
      return "Included code opens file " + label + " which is not closed in this file";
    }
    if (event.kind == ResourceKind::Drawing)
    {
      return "Included code opens drawing " + label + " which is not closed in this file";
    }
    return "Included code opens database " + label + " which is not closed in this file";
  }
  if (event.kind == ResourceKind::File)
  {
    return "File " + label + " is opened here and not closed in this execution tree";
  }
  if (event.kind == ResourceKind::Drawing)
  {
    return "Drawing " + label + " is opened here and not closed in this execution tree";
  }
  return "Database " + label + " is opened here and not closed in this execution tree";
}

// Byte columns into `lineText` → UTF-16 LSP columns (events keep byte
// indexes so they sort against call targets on the same line).
[[nodiscard]] auto u16Columns(std::string_view lineText, int startByte, int endByte) -> std::pair<int, int>
{
  return {
    static_cast<int>(utf16Offset(lineText, static_cast<std::size_t>(std::max(0, startByte)))),
    static_cast<int>(utf16Offset(lineText, static_cast<std::size_t>(std::max(0, endByte)))),
  };
}

[[nodiscard]] auto makeRange(int line, int startChar, int endChar) -> lsp::Range
{
  const auto start = static_cast<lsp::Uint>(std::max(0, startChar));
  const auto end = static_cast<lsp::Uint>(std::max(static_cast<int>(start) + 1, endChar));
  return lsp::Range{
    .start = {.line = static_cast<lsp::Uint>(std::max(0, line)), .character = start},
    .end = {.line = static_cast<lsp::Uint>(std::max(0, line)), .character = end},
  };
}

// ---------------------------------------------------------------------------
// Line parsing
// ---------------------------------------------------------------------------

// Control-flow role of a line for the path-aware walk.
enum class Control
{
  None,
  IfBlock,
  IfInline,
  Else,
  EndIf,
  Loop,
  EndLoop,
  ExitIf,
  Exit,
  Break,
};

// A condition the walk can narrow on: `ok` / `!ok` / `ok = 0` where `ok` holds
// a `dbvalid(...)` result, or `dbvalid(h)` / `!dbvalid(h)` directly.
struct Condition
{
  bool negated = false;
  std::string var;
  std::string validToken;
};

// One walk step on a line: a resource event (without walk stamps) or a call
// target with its arguments, already in stable index order.
struct LineStep
{
  bool isResource = false;
  ResourceEvent event;
  CallTarget target;
  std::vector<std::string> args;
  int callEndChar = 0;
};

// Everything the walk reads from one line that depends only on the line's
// text and the file's sections, so it is parsed once per file per pass.
struct LineInfo
{
  bool continuation = false;
  bool blank = false;
  bool ignoredSection = false;
  std::string sectionName;
  bool isInclude = false;
  std::optional<ParsedPath> includeTarget;
  int includeStartChar = 0;
  int includeEndChar = 0;
  bool ignoreClose = false;
  bool dbcreateContinues = false;
  std::vector<LineStep> steps;
  Control control = Control::None;
  bool hasThen = false;
  std::optional<Condition> condition;
  Control inlineTail = Control::None;
  std::string assignVar;
  std::string dbvalidToken;
  // Port of `ok = dwgopen_bg(port, ...)`: `ok` then tells whether it opened.
  std::string openResultToken;
  int paramIndex = 0;
};

struct FileInfo
{
  std::vector<std::string> lines;
  std::optional<std::vector<LineInfo>> parsed;
};

[[nodiscard]] auto leadingWord(std::string_view lowerTrimmed) -> std::string_view
{
  std::size_t end = 0;
  while (end < lowerTrimmed.size()
         && (std::isalnum(static_cast<unsigned char>(lowerTrimmed[end])) || lowerTrimmed[end] == '_'))
  {
    ++end;
  }
  return lowerTrimmed.substr(0, end);
}

// A keyword at the start of the line, not an assignment to a same-named variable.
[[nodiscard]] bool isKeywordLine(std::string_view lowerTrimmed, std::string_view word)
{
  if (leadingWord(lowerTrimmed) != word)
  {
    return false;
  }
  const auto rest = trim(lowerTrimmed.substr(word.size()));
  return rest.empty() || rest.front() != '=';
}

[[nodiscard]] auto stripOuterParens(std::string text) -> std::string
{
  while (text.size() >= 2 && text.front() == '('
         && findMatchingParenIndex(text, 0) == static_cast<int>(text.size()) - 1)
  {
    text = std::string(trim(std::string_view(text).substr(1, text.size() - 2)));
  }
  return text;
}

[[nodiscard]] auto parseCondition(std::string_view raw) -> std::optional<Condition>
{
  Condition condition;
  auto text = stripOuterParens(std::string(trim(raw)));
  if (!text.empty() && text.front() == '!')
  {
    condition.negated = true;
    text = stripOuterParens(std::string(trim(std::string_view(text).substr(1))));
  }
  else if (toLowerAscii(text).starts_with("not ") || toLowerAscii(text).starts_with("not("))
  {
    condition.negated = true;
    text = stripOuterParens(std::string(trim(std::string_view(text).substr(3))));
  }
  static const std::regex eqZero{R"re(^(.+?)\s*==?\s*0$)re"};
  std::smatch match;
  if (std::regex_match(text, match, eqZero))
  {
    condition.negated = !condition.negated;
    text = stripOuterParens(std::string(trim(match[1].str())));
  }
  static const std::regex ident{R"re(^[A-Za-z_][A-Za-z0-9_]*$)re"};
  static const std::regex validCall{R"re(^dbvalid\s*\(\s*([^()]+?)\s*\)$)re", std::regex::icase};
  if (std::regex_match(text, ident))
  {
    condition.var = toLowerAscii(text);
    return condition;
  }
  if (std::regex_match(text, match, validCall))
  {
    condition.validToken = std::string(trim(match[1].str()));
    return condition;
  }
  return std::nullopt;
}

// `call(` arguments after the path, and the byte end of the call expression.
[[nodiscard]] auto callArguments(std::string_view code, int index, int& endChar) -> std::vector<std::string>
{
  auto pos = static_cast<std::size_t>(index) + 4;
  while (pos < code.size() && std::isspace(static_cast<unsigned char>(code[pos])))
  {
    ++pos;
  }
  if (pos < code.size() && code[pos] == '(')
  {
    const auto close = findMatchingParenIndex(code, static_cast<int>(pos));
    if (close < 0)
    {
      endChar = static_cast<int>(code.size());
      return {};
    }
    endChar = close + 1;
    auto params = parseParameters(code.substr(pos + 1, static_cast<std::size_t>(close) - pos - 1));
    if (!params.empty())
    {
      params.erase(params.begin());
    }
    return params;
  }
  endChar = static_cast<int>(trim(code).size() + (code.size() - trim(code).size()));
  return {};
}

void classifyControl(LineInfo& info, std::string_view trimmed)
{
  const auto lower = toLowerAscii(trimmed);
  static const std::regex thenRe{R"re(\bthen\b)re", std::regex::icase};
  static const std::regex ifHeadRe{R"re(^if\b\s*(.*?)\s*\bthen\b(.*)$)re", std::regex::icase};
  info.hasThen = lower.find("then") != std::string::npos && std::regex_search(lower, thenRe);

  if (isKeywordLine(lower, "if"))
  {
    info.control = Control::IfBlock;
    std::smatch match;
    const std::string text(trimmed);
    if (info.hasThen && std::regex_match(text, match, ifHeadRe))
    {
      info.condition = parseCondition(match[1].str());
      const auto tail = trim(match[2].str());
      if (!tail.empty())
      {
        info.control = Control::IfInline;
        const auto tailLower = toLowerAscii(tail);
        if (isKeywordLine(tailLower, "exit"))
        {
          info.inlineTail = Control::Exit;
        }
        else if (isKeywordLine(tailLower, "break"))
        {
          info.inlineTail = Control::Break;
        }
      }
    }
    return;
  }
  if (isKeywordLine(lower, "else"))
  {
    info.control = Control::Else;
  }
  else if (isKeywordLine(lower, "endif"))
  {
    info.control = Control::EndIf;
  }
  else if (isKeywordLine(lower, "loop"))
  {
    info.control = Control::Loop;
  }
  else if (isKeywordLine(lower, "endloop"))
  {
    info.control = Control::EndLoop;
  }
  else if (isKeywordLine(lower, "exitif"))
  {
    info.control = Control::ExitIf;
  }
  else if (isKeywordLine(lower, "exit"))
  {
    info.control = Control::Exit;
  }
  else if (isKeywordLine(lower, "break"))
  {
    info.control = Control::Break;
  }
  if (info.control != Control::None)
  {
    return;
  }

  static const std::regex assignRe{R"re(^([A-Za-z_][A-Za-z0-9_]*\$?)\s*=([^=].*|)$)re"};
  static const std::regex validRe{R"re(^\s*dbvalid\s*\(\s*([^()]+?)\s*\)\s*$)re", std::regex::icase};
  static const std::regex paramRe{R"re(^\s*get\s*\(\s*\$CALLPARAM(\d+)\s*\)\s*$)re", std::regex::icase};
  std::smatch match;
  const std::string text(trimmed);
  if (!std::regex_match(text, match, assignRe))
  {
    return;
  }
  info.assignVar = toLowerAscii(match[1].str());
  const auto rhs = match[2].str();
  std::smatch rhsMatch;
  if (std::regex_match(rhs, rhsMatch, validRe))
  {
    info.dbvalidToken = std::string(trim(rhsMatch[1].str()));
  }
  else if (std::regex_match(rhs, rhsMatch, paramRe))
  {
    info.paramIndex = std::stoi(rhsMatch[1].str());
  }
  else
  {
    static const std::regex dwgOpenRe{R"re(^\s*dwgopen_bg\s*\()re", std::regex::icase};
    if (std::regex_search(rhs, rhsMatch, dwgOpenRe))
    {
      const auto open = static_cast<int>(rhsMatch.length(0)) - 1;
      const auto close = findMatchingParenIndex(rhs, open);
      if (close > open)
      {
        const auto params = parseParameters(
          std::string_view(rhs).substr(static_cast<std::size_t>(open + 1), static_cast<std::size_t>(close - open - 1)));
        if (!params.empty())
        {
          info.openResultToken = normalizeDrawingPort(params[0]);
        }
      }
    }
  }
}

[[nodiscard]] auto parseLineInfos(const std::vector<std::string>& lines, const std::string& filePath)
  -> std::vector<LineInfo>
{
  std::vector<std::string_view> views;
  views.reserve(lines.size());
  for (const auto& line : lines)
  {
    views.emplace_back(line);
  }
  const auto fileSections = parseFileSections(views);
  const WalkState noStamps{};
  static const std::regex pathRe{R"re(^((?:"[^"]+"|'[^']+'|[^\s#;]+))\s*(.*)$)re"};
  static const std::regex dbcreateRe{R"re(\bdbcreate\s*\()re", std::regex::icase};

  std::vector<LineInfo> infos(lines.size());
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto& rawLine = lines[static_cast<std::size_t>(i)];
    auto& info = infos[static_cast<std::size_t>(i)];
    const auto codeLine = stripValidationComment(rawLine);
    const auto trimmed = trim(codeLine);
    info.continuation = hasLineContinuation(rawLine);
    info.blank = trimmed.empty() || trimmed.starts_with('!');
    if (info.blank)
    {
      continue;
    }
    info.ignoredSection = lineInVerificationIgnoredSection(i, fileSections);
    if (info.ignoredSection)
    {
      continue;
    }
    info.sectionName = sectionNameForLine(i, fileSections);
    info.ignoreClose = lineHasIgnoreClose(rawLine);

    if (trimmed.starts_with("# include"))
    {
      info.isInclude = true;
      const auto includeStart = codeLine.find("# include");
      info.includeStartChar = includeStart == std::string::npos ? 0 : static_cast<int>(includeStart);
      info.includeEndChar = static_cast<int>(codeLine.size());
      const auto afterInclude = trim(trimmed.substr(std::string_view("# include").size()));
      std::cmatch pathMatch;
      if (!afterInclude.empty() && std::regex_match(
            afterInclude.data(), afterInclude.data() + afterInclude.size(), pathMatch, pathRe))
      {
        info.includeTarget = parseIncludeCallPathAndSection(pathMatch[1].str(), pathMatch[2].str());
      }
      continue;
    }

    classifyControl(info, trimmed);

    auto resourceEvents = collectResourceEventsFromLine(rawLine, i, filePath, noStamps);
    auto callTargets = collectCallTargets(codeLine);
    struct Indexed
    {
      int index = 0;
      LineStep step;
    };
    std::vector<Indexed> indexed;
    for (auto& event : resourceEvents)
    {
      const auto index = event.startChar;
      indexed.push_back(Indexed{.index = index, .step = LineStep{.isResource = true, .event = std::move(event)}});
    }
    for (auto& target : callTargets)
    {
      const auto index = target.index;
      int endChar = 0;
      auto args = callArguments(codeLine, index, endChar);
      indexed.push_back(Indexed{
        .index = index,
        .step = LineStep{
          .isResource = false,
          .target = std::move(target),
          .args = std::move(args),
          .callEndChar = endChar,
        },
      });
    }
    std::stable_sort(indexed.begin(), indexed.end(), [](const Indexed& a, const Indexed& b) {
      return a.index < b.index;
    });
    info.steps.reserve(indexed.size());
    for (auto& item : indexed)
    {
      info.steps.push_back(std::move(item.step));
    }
    info.dbcreateContinues = info.continuation && std::regex_search(codeLine, dbcreateRe);
  }
  return infos;
}

// ---------------------------------------------------------------------------
// Regions and blocks
// ---------------------------------------------------------------------------

enum class StmtKind
{
  Resource,
  Call,
  Include,
  If,
  Loop,
  ExitIf,
  Exit,
  Break,
  DbValid,
  // `ok = dwgopen_bg(port, ...)`: like DbValid, `ok` tells whether `port` is open.
  OpenResult,
  Assign,
};

struct Stmt
{
  StmtKind kind = StmtKind::Resource;
  int line = 0;
  int step = -1;
  std::optional<Condition> condition;
  bool ignoreClose = false;
  std::string var;
  std::string token;
  std::vector<Stmt> thenBlock;
  std::vector<Stmt> elseBlock;
};

using Block = std::vector<Stmt>;

// One named section (or the lines outside every section, name "") of a file,
// as a structured statement block.
struct Region
{
  Block block;
  std::unordered_map<std::string, int> paramVars;
  int lastLine = 0;
  bool exists = false;
};

constexpr int kMaxIfHeaderLines = 20;

class BlockParser
{
public:
  BlockParser(const std::vector<LineInfo>& infos, const std::vector<int>& lines)
    : infos_(infos), lines_(lines)
  {
  }

  auto parseAll() -> Block
  {
    std::vector<char> nesting;
    End ended = End::Eof;
    Block block;
    while (true)
    {
      auto part = parse(nesting, ended);
      block.insert(block.end(), std::make_move_iterator(part.begin()), std::make_move_iterator(part.end()));
      if (ended == End::Eof)
      {
        break;
      }
      ++pos_;
    }
    return block;
  }

private:
  enum class End
  {
    Eof,
    Else,
    EndIf,
    EndLoop,
  };

  const std::vector<LineInfo>& infos_;
  const std::vector<int>& lines_;
  std::size_t pos_ = 0;

  [[nodiscard]] auto infoAt(std::size_t pos) const -> const LineInfo&
  {
    return infos_[static_cast<std::size_t>(lines_[pos])];
  }

  void appendLine(Block& block, int line, const LineInfo& info) const
  {
    if (info.isInclude)
    {
      block.push_back(Stmt{.kind = StmtKind::Include, .line = line, .ignoreClose = info.ignoreClose});
      return;
    }
    Block steps;
    for (int s = 0; s < static_cast<int>(info.steps.size()); ++s)
    {
      steps.push_back(Stmt{
        .kind = info.steps[static_cast<std::size_t>(s)].isResource ? StmtKind::Resource : StmtKind::Call,
        .line = line,
        .step = s,
        .ignoreClose = info.ignoreClose,
      });
    }
    if (info.control == Control::IfInline)
    {
      Stmt stmt{.kind = StmtKind::If, .line = line, .condition = info.condition, .ignoreClose = info.ignoreClose};
      stmt.thenBlock = std::move(steps);
      if (info.inlineTail == Control::Exit)
      {
        stmt.thenBlock.push_back(Stmt{.kind = StmtKind::Exit, .line = line, .ignoreClose = info.ignoreClose});
      }
      else if (info.inlineTail == Control::Break)
      {
        stmt.thenBlock.push_back(Stmt{.kind = StmtKind::Break, .line = line, .ignoreClose = info.ignoreClose});
      }
      block.push_back(std::move(stmt));
      return;
    }
    block.insert(block.end(), std::make_move_iterator(steps.begin()), std::make_move_iterator(steps.end()));
    switch (info.control)
    {
    case Control::ExitIf:
      block.push_back(Stmt{.kind = StmtKind::ExitIf, .line = line, .ignoreClose = info.ignoreClose});
      return;
    case Control::Exit:
      block.push_back(Stmt{.kind = StmtKind::Exit, .line = line, .ignoreClose = info.ignoreClose});
      return;
    case Control::Break:
      block.push_back(Stmt{.kind = StmtKind::Break, .line = line, .ignoreClose = info.ignoreClose});
      return;
    default:
      break;
    }
    if (!info.assignVar.empty())
    {
      if (!info.dbvalidToken.empty())
      {
        block.push_back(Stmt{
          .kind = StmtKind::DbValid,
          .line = line,
          .var = info.assignVar,
          .token = info.dbvalidToken,
        });
      }
      else if (!info.openResultToken.empty())
      {
        block.push_back(Stmt{
          .kind = StmtKind::OpenResult,
          .line = line,
          .var = info.assignVar,
          .token = info.openResultToken,
        });
      }
      else
      {
        block.push_back(Stmt{.kind = StmtKind::Assign, .line = line, .var = info.assignVar});
      }
    }
  }

  auto parse(std::vector<char>& nesting, End& ended) -> Block
  {
    Block block;
    while (pos_ < lines_.size())
    {
      const auto line = lines_[pos_];
      const auto& info = infoAt(pos_);
      switch (info.control)
      {
      case Control::Else:
        if (!nesting.empty() && nesting.back() == 'i')
        {
          ended = End::Else;
          return block;
        }
        ++pos_;
        continue;
      case Control::EndIf:
        if (!nesting.empty() && nesting.back() == 'i')
        {
          ended = End::EndIf;
          return block;
        }
        ++pos_;
        continue;
      case Control::EndLoop:
        if (!nesting.empty() && nesting.back() == 'l')
        {
          ended = End::EndLoop;
          return block;
        }
        ++pos_;
        continue;
      case Control::IfBlock:
      {
        Stmt stmt{.kind = StmtKind::If, .line = line, .condition = info.condition};
        ++pos_;
        if (!info.hasThen)
        {
          // Multi-line header: `and`/`or` continuation lines up to `then`.
          stmt.condition.reset();
          const auto start = pos_;
          while (pos_ < lines_.size() && pos_ - start < kMaxIfHeaderLines && !infoAt(pos_).hasThen)
          {
            ++pos_;
          }
          if (pos_ < lines_.size() && infoAt(pos_).hasThen)
          {
            ++pos_;
          }
          else
          {
            pos_ = start;
          }
        }
        nesting.push_back('i');
        End inner = End::Eof;
        stmt.thenBlock = parse(nesting, inner);
        while (inner == End::Else)
        {
          ++pos_;
          auto more = parse(nesting, inner);
          stmt.elseBlock.insert(
            stmt.elseBlock.end(), std::make_move_iterator(more.begin()), std::make_move_iterator(more.end()));
        }
        if (inner == End::EndIf)
        {
          ++pos_;
        }
        nesting.pop_back();
        block.push_back(std::move(stmt));
        continue;
      }
      case Control::Loop:
      {
        Stmt stmt{.kind = StmtKind::Loop, .line = line};
        ++pos_;
        nesting.push_back('l');
        End inner = End::Eof;
        stmt.thenBlock = parse(nesting, inner);
        if (inner == End::EndLoop)
        {
          ++pos_;
        }
        nesting.pop_back();
        block.push_back(std::move(stmt));
        continue;
      }
      default:
        appendLine(block, line, info);
        ++pos_;
        continue;
      }
    }
    ended = End::Eof;
    return block;
  }
};

[[nodiscard]] auto buildRegion(const std::vector<LineInfo>& infos, const std::string& section) -> Region
{
  Region region;
  std::vector<int> lines;
  bool skippingDbcreateContinuation = false;
  for (int i = 0; i < static_cast<int>(infos.size()); ++i)
  {
    const auto& info = infos[static_cast<std::size_t>(i)];
    if (skippingDbcreateContinuation)
    {
      skippingDbcreateContinuation = info.continuation;
      continue;
    }
    if (info.blank || info.ignoredSection || info.sectionName != section)
    {
      continue;
    }
    lines.push_back(i);
    region.lastLine = i;
    region.exists = true;
    if (info.dbcreateContinues)
    {
      skippingDbcreateContinuation = true;
    }
    if (info.paramIndex > 0 && !info.assignVar.empty())
    {
      region.paramVars.emplace(info.assignVar, info.paramIndex);
    }
  }
  BlockParser parser(infos, lines);
  region.block = parser.parseAll();
  return region;
}

// ---------------------------------------------------------------------------
// Path-aware walk
// ---------------------------------------------------------------------------

struct CallSiteRef
{
  std::string filePath;
  int line = 0;
  int startChar = 0;
  int endChar = 0;
  std::string lineText;
  bool ignoreClose = false;
};

// A handle resolved in the frame that names it.
struct Resolved
{
  std::string token;
  std::string label;
  // Frame index the handle belongs to; -1 is an unbound $CALLPARAM of the root.
  int home = 0;
  // Frame index that owns the duty to close it, for reporting.
  int attr = 0;
  // Call in the `attr` frame that leads down to where the handle was named, or -1.
  int callsite = -1;
  ResourceKind kind = ResourceKind::Database;

  bool operator==(const Resolved&) const = default;
};

// One possibly-open handle on a path.
struct Fact
{
  const ResourceEvent* origin = nullptr;
  ResourceKind kind = ResourceKind::Database;
  std::string token;
  std::string label;
  int ns = 0;
  int home = 0;
  int attr = 0;
  int callsite = -1;
  int includeSite = -1;

  bool operator==(const Fact&) const = default;
};

struct PathState
{
  std::vector<Fact> facts;
  // Handles whose latest dbopen on this path was `"ALREADY_OPEN"`: the source
  // program owns them, so they are never open facts, but closing one is wrong.
  std::vector<Fact> aliases;
  std::vector<std::pair<std::string, Resolved>> valid;
};

void addUnique(std::vector<Fact>& list, const Fact& fact)
{
  if (std::find(list.begin(), list.end(), fact) == list.end())
  {
    list.push_back(fact);
  }
}

void addFact(PathState& state, Fact fact)
{
  if (std::find(state.facts.begin(), state.facts.end(), fact) == state.facts.end())
  {
    state.facts.push_back(std::move(fact));
  }
}

void joinInto(std::optional<PathState>& acc, const PathState& other)
{
  if (!acc)
  {
    acc = other;
    return;
  }
  for (const auto& fact : other.facts)
  {
    addFact(*acc, fact);
  }
  for (const auto& alias : other.aliases)
  {
    addUnique(acc->aliases, alias);
  }
  std::erase_if(acc->valid, [&](const auto& entry) {
    return std::find(other.valid.begin(), other.valid.end(), entry) == other.valid.end();
  });
}

[[nodiscard]] bool sameFactList(const std::vector<Fact>& a, const std::vector<Fact>& b)
{
  if (a.size() != b.size())
  {
    return false;
  }
  return std::all_of(a.begin(), a.end(), [&](const Fact& fact) {
    return std::find(b.begin(), b.end(), fact) != b.end();
  });
}

[[nodiscard]] bool sameFacts(const PathState& a, const PathState& b)
{
  return sameFactList(a.facts, b.facts) && sameFactList(a.aliases, b.aliases);
}

struct Flow
{
  std::optional<PathState> next;
  std::vector<PathState> exits;
  std::vector<PathState> loopExits;
};

enum class LeakReason
{
  End,
  Exit,
  Break,
};

struct Leak
{
  Fact fact;
  LeakReason reason = LeakReason::End;
  std::string reasonFile;
  int reasonLine = 0;
};

// A `dbclose` reached while its handle is an ALREADY_OPEN alias on the path.
struct AliasClose
{
  const ResourceEvent* close = nullptr;
  const ResourceEvent* alias = nullptr;

  bool operator==(const AliasClose&) const = default;
};

struct RootResult
{
  std::vector<Leak> leaks;
  std::vector<AliasClose> aliasCloses;
  // `dbclose` lines that matched no open or alias on some path: the handle may
  // come from a caller or an include root.
  std::unordered_set<const ResourceEvent*> unmatchedCloses;
  // Handles bound to the root's own $CALLPARAM that are still open at its end:
  // its callers must close them.
  std::vector<Fact> escapes;
  std::unordered_set<const ResourceEvent*> reached;
  bool truncated = false;
};

struct Frame
{
  std::string filePath;
  // File whose lines are running: `filePath`, or an included file.
  std::string codeFile;
  const Region* region = nullptr;
  const std::vector<LineInfo>* infos = nullptr;
  std::unordered_map<std::string, Resolved> binding;
  int dbNs = 0;
  int fileNs = 0;
  int callsite = -1;
  bool shareDb = false;
  bool shareFiles = false;

  [[nodiscard]] int nsFor(ResourceKind kind) const
  {
    if (kind == ResourceKind::Drawing)
    {
      // Drawing ports are global for the session: every frame shares them.
      return kDrawingNs;
    }
    return kind == ResourceKind::Database ? dbNs : fileNs;
  }

  static constexpr int kDrawingNs = -1;
};

constexpr int kMaxWalkSteps = 400000;
constexpr int kMaxFrameDepth = 48;
constexpr int kMaxLoopPasses = 4;
// Caller and include-root regions walked to find where a closed alias came from.
constexpr std::size_t kMaxAliasRegions = 64;

[[nodiscard]] bool isNumericToken(std::string_view token)
{
  return !token.empty()
    && std::all_of(token.begin(), token.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
}

// A whole quoted string such as `"planlay"`: a literal drawing port.
[[nodiscard]] bool isStringLiteralToken(std::string_view token)
{
  return parseStringLiteral(token).has_value();
}

[[nodiscard]] bool isSimpleHandleArg(std::string_view arg)
{
  // `$` names and string literals carry drawing ports.
  static const std::regex ident{R"re(^[A-Za-z_][A-Za-z0-9_]*\$?$)re"};
  const std::string text(trim(arg));
  return isNumericToken(text) || std::regex_match(text, ident) || isStringLiteralToken(text);
}

[[nodiscard]] auto regionKey(std::string_view filePath, std::string_view section) -> std::string
{
  return std::string(filePath) + "::" + std::string(section);
}

struct ResourceWalker;

// One root walk: a region entered as the top of its call chain.
struct RootRun
{
  ResourceWalker& walker;
  RootResult& result;
  std::vector<Frame> frames;
  std::unordered_set<std::string> stack;
  int nsCounter = 0;
  int includeSite = -1;
  int steps = 0;
  std::string rootSection;
  mutable std::optional<bool> rootCallers;

  auto execBlock(const Block& block, PathState state, int loopDepth) -> Flow;

  // Whether some section calls the root region (looked up on first need).
  [[nodiscard]] bool rootHasCallers() const;

  [[nodiscard]] auto resolve(int frameIndex, std::string_view raw, ResourceKind kind) const -> Resolved
  {
    auto resolved = resolveUntyped(frameIndex, raw, kind);
    resolved.kind = kind;
    return resolved;
  }

  [[nodiscard]] auto resolveUntyped(int frameIndex, std::string_view raw, ResourceKind kind) const -> Resolved
  {
    const auto label = std::string(trim(raw));
    const auto token = toLowerAscii(label);
    const auto& frame = frames[static_cast<std::size_t>(frameIndex)];
    if (kind == ResourceKind::Drawing && isStringLiteralToken(token))
    {
      // A literal port is one drawing in every frame. A root that is called
      // hands it to its callers, like a $CALLPARAM handle.
      const auto home = rootHasCallers() ? -1 : 0;
      return Resolved{
        .token = token,
        .label = label,
        .home = home,
        .attr = 0,
        .callsite = frameIndex > 0 ? frames[1].callsite : -1,
      };
    }
    if (isNumericToken(token))
    {
      // A literal handle is the same handle in every frame sharing the namespace.
      int home = frameIndex;
      for (int k = 0; k < frameIndex; ++k)
      {
        if (frames[static_cast<std::size_t>(k)].nsFor(kind) == frame.nsFor(kind))
        {
          home = k;
          break;
        }
      }
      return Resolved{
        .token = token,
        .label = label,
        .home = home,
        .attr = home,
        .callsite = home < frameIndex ? frames[static_cast<std::size_t>(home + 1)].callsite : -1,
      };
    }
    if (const auto it = frame.binding.find(token); it != frame.binding.end())
    {
      const auto ownerIndex = std::max(it->second.home, 0);
      if (frames[static_cast<std::size_t>(ownerIndex)].nsFor(kind) == frame.nsFor(kind))
      {
        return it->second;
      }
      // The caller does not share this namespace: the handle cannot go back up.
      return Resolved{
        .token = token,
        .label = label,
        .home = frameIndex,
        .attr = frameIndex - 1,
        .callsite = frame.callsite,
      };
    }
    if (frameIndex == 0 && frame.region && frame.region->paramVars.contains(token))
    {
      return Resolved{.token = token, .label = label, .home = -1, .attr = 0, .callsite = -1};
    }
    return Resolved{.token = token, .label = label, .home = frameIndex, .attr = frameIndex, .callsite = -1};
  }

  // Returns whether any open fact was closed.
  bool removeMatching(PathState& state, ResourceKind kind, const Resolved& handle, int ns,
    const std::optional<std::string>& nameLiteral) const
  {
    return std::erase_if(state.facts, [&](const Fact& fact) {
      if (fact.kind != kind)
      {
        return false;
      }
      if (kind == ResourceKind::Database && nameLiteral && fact.origin->nameLiteral
          && !nameLiteral->empty() && !fact.origin->nameLiteral->empty()
          && namesMatch(*nameLiteral, *fact.origin->nameLiteral))
      {
        return true;
      }
      return !handle.token.empty() && fact.token == handle.token && fact.ns == ns;
    }) > 0;
  }

  // Drops the aliases of `handle`, returning the ones dropped.
  [[nodiscard]] static auto takeAliases(PathState& state, const Resolved& handle, int ns) -> std::vector<Fact>
  {
    std::vector<Fact> taken;
    if (handle.token.empty())
    {
      return taken;
    }
    std::erase_if(state.aliases, [&](const Fact& alias) {
      if (alias.token != handle.token || alias.ns != ns)
      {
        return false;
      }
      taken.push_back(alias);
      return true;
    });
    return taken;
  }

  void narrow(PathState& state, const Condition& condition, bool thenBranch) const
  {
    if (condition.negated != thenBranch)
    {
      return;
    }
    const auto frameIndex = static_cast<int>(frames.size()) - 1;
    Resolved handle;
    if (!condition.var.empty())
    {
      const auto it = std::find_if(state.valid.begin(), state.valid.end(), [&](const auto& entry) {
        return entry.first == condition.var;
      });
      if (it == state.valid.end())
      {
        return;
      }
      handle = it->second;
    }
    else
    {
      handle = resolve(frameIndex, condition.validToken, ResourceKind::Database);
    }
    removeMatching(state, handle.kind, handle, frames.back().nsFor(handle.kind), std::nullopt);
  }

  void leakAll(PathState& state, LeakReason reason, const std::string& file, int line)
  {
    for (auto& fact : state.facts)
    {
      result.leaks.push_back(Leak{.fact = fact, .reason = reason, .reasonFile = file, .reasonLine = line});
    }
    state.facts.clear();
  }

  // Handles that die with `frameIndex` when it ends at `exit` (`line` >= 0)
  // or by running off its last line.
  void endFrame(PathState& state, int frameIndex, const std::string& exitFile = {}, int exitLine = -1)
  {
    const auto& frame = frames[static_cast<std::size_t>(frameIndex)];
    std::erase_if(state.aliases, [&](const Fact& alias) { return alias.home == frameIndex; });
    std::erase_if(state.facts, [&](const Fact& fact) {
      if (fact.home != frameIndex)
      {
        return false;
      }
      result.leaks.push_back(Leak{
        .fact = fact,
        .reason = exitLine >= 0 ? LeakReason::Exit : LeakReason::End,
        .reasonFile = exitLine >= 0 ? exitFile : frame.filePath,
        .reasonLine = exitLine >= 0 ? exitLine : (frame.region ? frame.region->lastLine : 0),
      });
      return true;
    });
  }

  auto execCall(const Stmt& stmt, PathState state) -> std::optional<PathState>;
  auto execInclude(const Stmt& stmt, PathState state, int loopDepth) -> Flow;
};

// ---------------------------------------------------------------------------
// Walker: file cache, resolution, index lookups, root memo, reporting
// ---------------------------------------------------------------------------

struct ChainOutcome
{
  std::vector<std::pair<int, std::string>> failingCallsites;
  std::vector<std::pair<int, std::string>> noCallerCallsites;
  bool failing = false;
  bool noCaller = false;
};

struct ResourceWalker
{
  ValidateContext& ctx;
  std::string currentFilePath;
  std::unordered_map<std::string, FileInfo> fileCache;
  std::unordered_map<std::string, std::string> resolveMemo;
  std::optional<std::unordered_map<std::string, std::vector<IncludeLinkInfo>>> parentsByTarget;
  std::unordered_map<std::string, Region> regionCache;
  std::unordered_map<std::string, RootResult> rootCache;
  std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> callerCache;
  std::unordered_map<std::string, std::pair<bool, bool>> commonFlagCache;
  std::vector<CallSiteRef> callSites;
  std::unordered_map<std::string, int> callSiteIds;
  std::vector<IncludeSite> includeSites;
  std::unordered_map<std::string, int> includeSiteIds;

  [[nodiscard]] auto getFileInfo(const std::string& normalized) -> FileInfo*
  {
    if (normalized.empty())
    {
      return nullptr;
    }
    if (const auto it = fileCache.find(normalized); it != fileCache.end())
    {
      return &it->second;
    }

    std::optional<std::string> text;
    if (normalized == currentFilePath)
    {
      text = ctx.prep->snapshot.text;
    }
    else if (ctx.stores.textForPath)
    {
      text = ctx.stores.textForPath(std::filesystem::path(normalized));
    }
    if (!text && ctx.stores.linkedFiles)
    {
      if (auto snap = ctx.stores.linkedFiles->get(normalized))
      {
        text = snap->text;
      }
    }
    if (!text)
    {
      return nullptr;
    }
    const auto [it, _] = fileCache.emplace(normalized, FileInfo{.lines = splitToOwnedLines(*text)});
    return &it->second;
  }

  [[nodiscard]] auto getParsed(const std::string& normalized) -> const std::vector<LineInfo>*
  {
    auto* file = getFileInfo(normalized);
    if (!file)
    {
      return nullptr;
    }
    if (!file->parsed)
    {
      file->parsed = parseLineInfos(file->lines, normalized);
    }
    return &*file->parsed;
  }

  [[nodiscard]] auto getRegion(const std::string& normalized, const std::string& section) -> const Region*
  {
    const auto key = regionKey(normalized, section);
    if (const auto it = regionCache.find(key); it != regionCache.end())
    {
      return &it->second;
    }
    const auto* infos = getParsed(normalized);
    if (!infos)
    {
      return nullptr;
    }
    const auto [it, _] = regionCache.emplace(key, buildRegion(*infos, section));
    return &it->second;
  }

  [[nodiscard]] auto commonFlags(const std::string& normalized) -> std::pair<bool, bool>
  {
    if (const auto it = commonFlagCache.find(normalized); it != commonFlagCache.end())
    {
      return it->second;
    }
    std::pair<bool, bool> flags{false, false};
    if (auto* file = getFileInfo(normalized))
    {
      flags = parseFileCommonFlags(file->lines);
    }
    commonFlagCache.emplace(normalized, flags);
    return flags;
  }

  // Resolution probes the disk, and the same raw targets recur, so each is
  // resolved once per pass.
  [[nodiscard]] auto resolvePath(std::string_view raw) -> std::string
  {
    if (!ctx.stores.layout || raw.empty())
    {
      return {};
    }
    std::string key(raw);
    if (const auto it = resolveMemo.find(key); it != resolveMemo.end())
    {
      return it->second;
    }
    const auto resolved = ctx.stores.layout->resolveVertexFilePath(raw);
    auto normalized = resolved ? normalizeFsPath(resolved->string()) : std::string{};
    resolveMemo.emplace(std::move(key), normalized);
    return normalized;
  }

  [[nodiscard]] auto filePathToUri(const std::string& normalized) const -> lsp::Uri
  {
    if (normalized == currentFilePath)
    {
      return ctx.prep->snapshot.uri;
    }
    if (ctx.stores.index)
    {
      if (auto entry = ctx.stores.index->entryByAbsPath(std::filesystem::path(normalized)))
      {
        return entry->identity.uri;
      }
    }
    if (ctx.stores.layout)
    {
      if (auto identity = ctx.stores.layout->overlayForPath(std::filesystem::path(normalized)))
      {
        return identity->uri;
      }
    }
    return uriFromPath(std::filesystem::path(normalized));
  }

  auto registerCallSite(const std::string& filePath, int line, const LineStep& step, bool ignoreClose,
    const std::string& lineText) -> int
  {
    const auto key = filePath + "|" + std::to_string(line) + "|" + std::to_string(step.target.index);
    if (const auto it = callSiteIds.find(key); it != callSiteIds.end())
    {
      return it->second;
    }
    const auto id = static_cast<int>(callSites.size());
    callSites.push_back(CallSiteRef{
      .filePath = filePath,
      .line = line,
      .startChar = step.target.index,
      .endChar = step.callEndChar,
      .lineText = lineText,
      .ignoreClose = ignoreClose,
    });
    callSiteIds.emplace(key, id);
    return id;
  }

  auto registerIncludeSite(const std::string& filePath, int line, const LineInfo& info,
    const std::string& lineText) -> int
  {
    const auto key = filePath + "|" + std::to_string(line);
    if (const auto it = includeSiteIds.find(key); it != includeSiteIds.end())
    {
      return it->second;
    }
    const auto id = static_cast<int>(includeSites.size());
    includeSites.push_back(IncludeSite{
      .filePath = filePath,
      .line = line,
      .startChar = info.includeStartChar,
      .endChar = info.includeEndChar,
      .ignoreClose = info.ignoreClose,
      .lineText = lineText,
    });
    includeSiteIds.emplace(key, id);
    return id;
  }

  [[nodiscard]] auto lineText(const std::string& filePath, int line) -> std::string
  {
    auto* file = getFileInfo(filePath);
    if (!file || line < 0 || line >= static_cast<int>(file->lines.size()))
    {
      return {};
    }
    return file->lines[static_cast<std::size_t>(line)];
  }

  // Named sections (and "" for lines outside every section) that hold code.
  [[nodiscard]] auto regionsOf(const std::string& filePath) -> std::vector<std::string>
  {
    std::vector<std::string> names;
    const auto* infos = getParsed(filePath);
    if (!infos)
    {
      return names;
    }
    std::unordered_set<std::string> seen;
    for (const auto& info : *infos)
    {
      if (info.blank || info.ignoredSection)
      {
        continue;
      }
      if (seen.insert(info.sectionName).second)
      {
        names.push_back(info.sectionName);
      }
    }
    return names;
  }

  auto analyzeRoot(const std::string& filePath, const std::string& section) -> const RootResult&
  {
    const auto key = regionKey(filePath, section);
    if (const auto it = rootCache.find(key); it != rootCache.end())
    {
      return it->second;
    }
    auto& result = rootCache[key];
    const auto* region = getRegion(filePath, section);
    const auto* infos = getParsed(filePath);
    if (!region || !infos || !region->exists)
    {
      return result;
    }
    const auto flags = commonFlags(filePath);
    RootRun run{.walker = *this, .result = result, .rootSection = section};
    run.frames.push_back(Frame{
      .filePath = filePath,
      .codeFile = filePath,
      .region = region,
      .infos = infos,
      .shareDb = flags.first,
      .shareFiles = flags.second,
    });
    run.stack.insert(key);
    auto flow = run.execBlock(region->block, PathState{}, 0);
    std::optional<PathState> out = std::move(flow.next);
    for (const auto& state : flow.exits)
    {
      joinInto(out, state);
    }
    for (const auto& state : flow.loopExits)
    {
      joinInto(out, state);
    }
    if (out)
    {
      for (const auto& fact : out->facts)
      {
        if (fact.home < 0)
        {
          result.escapes.push_back(fact);
        }
        else
        {
          result.leaks.push_back(Leak{
            .fact = fact,
            .reason = LeakReason::End,
            .reasonFile = filePath,
            .reasonLine = region->lastLine,
          });
        }
      }
    }
    return result;
  }

  // Every region (file, section) holding a `call` to `filePath` `section`.
  [[nodiscard]] auto findCallers(const std::string& filePath, const std::string& section)
    -> const std::vector<std::pair<std::string, std::string>>&
  {
    const auto key = regionKey(filePath, section);
    if (const auto it = callerCache.find(key); it != callerCache.end())
    {
      return it->second;
    }
    std::vector<std::string> candidates{currentFilePath};
    if (filePath != currentFilePath)
    {
      candidates.push_back(filePath);
    }
    if (ctx.stores.index)
    {
      for (const auto& entry : ctx.stores.index->snapshot())
      {
        const auto fromPath = normalizeFsPath(entry->identity.absPath.string());
        if (std::find(candidates.begin(), candidates.end(), fromPath) != candidates.end())
        {
          continue;
        }
        for (const auto& link : entry->callLinks)
        {
          if (resolvePath(link.target) == filePath)
          {
            candidates.push_back(fromPath);
            break;
          }
        }
      }
    }
    std::vector<std::pair<std::string, std::string>> callers;
    for (const auto& candidate : candidates)
    {
      ctx.pollCancel();
      const auto* infos = getParsed(candidate);
      if (!infos)
      {
        continue;
      }
      for (const auto& info : *infos)
      {
        if (info.blank || info.ignoredSection)
        {
          continue;
        }
        for (const auto& step : info.steps)
        {
          if (step.isResource || step.target.parsed.targetSection != section)
          {
            continue;
          }
          if (resolvePath(step.target.parsed.filePath) != filePath)
          {
            continue;
          }
          std::pair<std::string, std::string> caller{candidate, info.sectionName};
          if (caller.first == filePath && caller.second == section)
          {
            continue;
          }
          if (std::find(callers.begin(), callers.end(), caller) == callers.end())
          {
            callers.push_back(std::move(caller));
          }
        }
      }
    }
    return callerCache.emplace(key, std::move(callers)).first->second;
  }

  // Walks up the callers of a region whose $CALLPARAM handle `origin` is still
  // open at its end, until each chain closes it, fails, or runs out of callers.
  void walkCallers(const std::string& filePath, const std::string& section, const ResourceEvent* origin,
    const std::vector<Fact>& escapesHere, std::unordered_set<std::string>& visited, ChainOutcome& outcome)
  {
    if (!visited.insert(regionKey(filePath, section)).second)
    {
      return;
    }
    const auto& callers = findCallers(filePath, section);
    if (callers.empty())
    {
      outcome.noCaller = true;
      for (const auto& fact : escapesHere)
      {
        outcome.noCallerCallsites.emplace_back(fact.callsite, fact.label);
      }
      return;
    }
    for (const auto& [callerFile, callerSection] : callers)
    {
      const auto& result = analyzeRoot(callerFile, callerSection);
      if (result.truncated)
      {
        continue;
      }
      std::vector<Fact> escapes;
      for (const auto& leak : result.leaks)
      {
        if (leak.fact.origin != origin)
        {
          continue;
        }
        outcome.failing = true;
        if (leak.fact.callsite >= 0)
        {
          outcome.failingCallsites.emplace_back(leak.fact.callsite, leak.fact.label);
        }
      }
      for (const auto& fact : result.escapes)
      {
        if (fact.origin == origin)
        {
          escapes.push_back(fact);
        }
      }
      if (!escapes.empty())
      {
        walkCallers(callerFile, callerSection, origin, escapes, visited, outcome);
      }
    }
  }

  [[nodiscard]] auto getDirectIncludeParents(const std::string& targetNormalized)
    -> std::vector<IncludeLinkInfo>
  {
    if (!ctx.stores.index || targetNormalized.empty())
    {
      return {};
    }
    // One index scan per pass builds every target's parents, in the same
    // entry-then-link order a per-target scan would visit them.
    if (!parentsByTarget)
    {
      parentsByTarget.emplace();
      for (const auto& entry : ctx.stores.index->snapshot())
      {
        const auto fromPath = normalizeFsPath(entry->identity.absPath.string());
        for (const auto& link : entry->includeLinks)
        {
          auto resolved = resolvePath(link.target);
          if (resolved.empty())
          {
            continue;
          }
          (*parentsByTarget)[resolved].push_back(IncludeLinkInfo{
            .fromPath = fromPath,
            .targetPath = resolved,
            .line = link.line,
            .lineText = link.lineText,
          });
        }
      }
    }
    const auto it = parentsByTarget->find(targetNormalized);
    return it == parentsByTarget->end() ? std::vector<IncludeLinkInfo>{} : it->second;
  }

  [[nodiscard]] auto getOutgoingIncludeLinks(const std::string& fromNormalized)
    -> std::vector<IncludeLinkInfo>
  {
    std::vector<IncludeLinkInfo> outgoing;
    if (!ctx.stores.index || fromNormalized.empty())
    {
      return outgoing;
    }
    auto entry = ctx.stores.index->entryByAbsPath(std::filesystem::path(fromNormalized));
    if (!entry)
    {
      return outgoing;
    }
    for (const auto& link : entry->includeLinks)
    {
      const auto resolved = resolvePath(link.target);
      if (resolved.empty())
      {
        continue;
      }
      outgoing.push_back(IncludeLinkInfo{
        .fromPath = fromNormalized,
        .targetPath = resolved,
        .line = link.line,
        .lineText = link.lineText,
      });
    }
    return outgoing;
  }

  [[nodiscard]] auto getIncludeRoots(const std::string& targetPath) -> std::vector<std::string>
  {
    std::unordered_set<std::string> parents;
    std::vector<std::string> queue{targetPath};
    std::unordered_set<std::string> seen{targetPath};
    while (!queue.empty())
    {
      const auto current = std::move(queue.front());
      queue.erase(queue.begin());
      for (const auto& parent : getDirectIncludeParents(current))
      {
        if (parent.fromPath.empty() || !seen.insert(parent.fromPath).second)
        {
          continue;
        }
        parents.insert(parent.fromPath);
        queue.push_back(parent.fromPath);
      }
    }
    std::vector<std::string> roots;
    for (const auto& parentPath : parents)
    {
      if (getDirectIncludeParents(parentPath).empty())
      {
        roots.push_back(parentPath);
      }
    }
    return roots;
  }

  bool includeReaches(
    const std::string& fromPath,
    const std::string& toPath,
    std::unordered_set<std::string>& seen)
  {
    if (fromPath.empty() || toPath.empty())
    {
      return false;
    }
    if (fromPath == toPath)
    {
      return true;
    }
    if (!seen.insert(fromPath).second)
    {
      return false;
    }
    for (const auto& link : getOutgoingIncludeLinks(fromPath))
    {
      if (includeReaches(link.targetPath, toPath, seen))
      {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] auto findRootIncludeSite(const std::string& rootPath, const std::string& targetPath)
    -> std::optional<IncludeLinkInfo>
  {
    for (const auto& link : getOutgoingIncludeLinks(rootPath))
    {
      std::unordered_set<std::string> seen;
      if (includeReaches(link.targetPath, targetPath, seen))
      {
        return link;
      }
    }
    for (const auto& parent : getDirectIncludeParents(targetPath))
    {
      if (parent.fromPath == rootPath)
      {
        return parent;
      }
    }
    return std::nullopt;
  }

  // Does include root `rootPath` close `origin` on every path that reaches it?
  [[nodiscard]] bool includeRootCloses(const std::string& rootPath, const ResourceEvent* origin)
  {
    bool found = false;
    for (const auto& section : regionsOf(rootPath))
    {
      const auto& result = analyzeRoot(rootPath, section);
      if (result.truncated)
      {
        return true;
      }
      if (!result.reached.contains(origin))
      {
        continue;
      }
      found = true;
      for (const auto& leak : result.leaks)
      {
        if (leak.fact.origin == origin)
        {
          return false;
        }
      }
      for (const auto& fact : result.escapes)
      {
        if (fact.origin == origin)
        {
          return false;
        }
      }
    }
    return found;
  }

  [[nodiscard]] auto relatedAt(const std::string& filePath, int line, int startByte, int endByte,
    const std::string& text, std::string message) const -> RelatedIssue
  {
    const auto [start, end] = u16Columns(text, startByte, endByte);
    return RelatedIssue{
      .uri = filePathToUri(filePath),
      .line = line,
      .startChar = start,
      .endChar = end,
      .message = std::move(message),
    };
  }

  [[nodiscard]] auto relatedForCallsite(int id, std::string message) const -> RelatedIssue
  {
    const auto& site = callSites[static_cast<std::size_t>(id)];
    return relatedAt(site.filePath, site.line, site.startChar, site.endChar, site.lineText, std::move(message));
  }

  [[nodiscard]] auto relatedForLine(const std::string& filePath, int line, std::string message) -> RelatedIssue
  {
    const auto text = lineText(filePath, line);
    const auto start = text.find_first_not_of(" \t");
    const auto startByte = start == std::string::npos ? 0 : static_cast<int>(start);
    return relatedAt(filePath, line, startByte, static_cast<int>(text.size()), text, std::move(message));
  }

  [[nodiscard]] auto relatedForIncludeRoots(
    const ResourceEvent& openEvent,
    const std::vector<std::string>& failingRoots) -> std::vector<RelatedIssue>
  {
    std::vector<RelatedIssue> related;
    for (const auto& rootPath : failingRoots)
    {
      const auto site = findRootIncludeSite(rootPath, openEvent.filePath);
      const auto line = site ? site->line : 0;
      const auto text = site ? site->lineText : std::string{};
      related.push_back(RelatedIssue{
        .uri = filePathToUri(rootPath),
        .line = line,
        .startChar = 0,
        .endChar = std::max(1, static_cast<int>(utf16Length(text))),
        .message = "Include root does not close this handle",
      });
    }
    return related;
  }

  [[nodiscard]] auto collectIssues() -> std::vector<UnclosedIssue>;
  void appendAliasCloseIssues(const std::vector<std::string>& regions,
    std::optional<std::vector<std::string>>& includeRoots, std::vector<UnclosedIssue>& issues);
};

// ---------------------------------------------------------------------------
// RootRun statement execution
// ---------------------------------------------------------------------------

bool RootRun::rootHasCallers() const
{
  if (!rootCallers)
  {
    rootCallers = !walker.findCallers(frames.front().filePath, rootSection).empty();
  }
  return *rootCallers;
}

auto RootRun::execBlock(const Block& block, PathState state, int loopDepth) -> Flow
{
  Flow flow;
  const auto frameIndex = static_cast<int>(frames.size()) - 1;
  for (const auto& stmt : block)
  {
    walker.ctx.pollCancel();
    if (++steps > kMaxWalkSteps)
    {
      result.truncated = true;
      flow.next = std::move(state);
      return flow;
    }
    const auto& frame = frames.back();
    const auto& info = (*frame.infos)[static_cast<std::size_t>(stmt.line)];
    switch (stmt.kind)
    {
    case StmtKind::Resource:
    {
      const auto& event = info.steps[static_cast<std::size_t>(stmt.step)].event;
      const auto handle = resolve(frameIndex, event.token, event.kind);
      const auto ns = frame.nsFor(event.kind);
      if (event.action == ResourceAction::Close)
      {
        const auto closed = removeMatching(state, event.kind, handle, ns, event.nameLiteral);
        const auto aliases = event.kind == ResourceKind::Database
          ? takeAliases(state, handle, ns)
          : std::vector<Fact>{};
        if (!event.ignoreClose)
        {
          for (const auto& alias : aliases)
          {
            const AliasClose hit{.close = &event, .alias = alias.origin};
            if (std::find(result.aliasCloses.begin(), result.aliasCloses.end(), hit) == result.aliasCloses.end())
            {
              result.aliasCloses.push_back(hit);
            }
          }
        }
        if (!closed && aliases.empty() && !event.token.empty() && event.kind == ResourceKind::Database)
        {
          result.unmatchedCloses.insert(&event);
        }
        break;
      }
      if (event.kind == ResourceKind::Database)
      {
        // The latest dbopen of a handle decides whether it is an alias.
        (void)takeAliases(state, handle, ns);
      }
      if (event.alreadyOpen)
      {
        // Owned and closed by the source program: never an open to close.
        addUnique(state.aliases, Fact{
          .origin = &event,
          .kind = event.kind,
          .token = handle.token,
          .label = handle.label,
          .ns = ns,
          .home = handle.home,
          .attr = handle.attr,
          .callsite = handle.callsite,
          .includeSite = includeSite,
        });
        break;
      }
      result.reached.insert(&event);
      if (event.ignoreClose)
      {
        break;
      }
      addFact(state, Fact{
        .origin = &event,
        .kind = event.kind,
        .token = handle.token,
        .label = handle.label,
        .ns = ns,
        .home = handle.home,
        .attr = handle.attr,
        .callsite = handle.callsite,
        .includeSite = includeSite,
      });
      break;
    }
    case StmtKind::Call:
    {
      auto next = execCall(stmt, std::move(state));
      if (!next)
      {
        return flow;
      }
      state = std::move(*next);
      break;
    }
    case StmtKind::Include:
    {
      auto inner = execInclude(stmt, std::move(state), loopDepth);
      flow.exits.insert(flow.exits.end(), inner.exits.begin(), inner.exits.end());
      flow.loopExits.insert(flow.loopExits.end(), inner.loopExits.begin(), inner.loopExits.end());
      if (!inner.next)
      {
        return flow;
      }
      state = std::move(*inner.next);
      break;
    }
    case StmtKind::If:
    {
      auto thenState = state;
      auto elseState = state;
      if (stmt.condition)
      {
        narrow(thenState, *stmt.condition, true);
        narrow(elseState, *stmt.condition, false);
      }
      auto thenFlow = execBlock(stmt.thenBlock, std::move(thenState), loopDepth);
      auto elseFlow = execBlock(stmt.elseBlock, std::move(elseState), loopDepth);
      for (auto* part : {&thenFlow, &elseFlow})
      {
        flow.exits.insert(flow.exits.end(), part->exits.begin(), part->exits.end());
        flow.loopExits.insert(flow.loopExits.end(), part->loopExits.begin(), part->loopExits.end());
      }
      std::optional<PathState> joined;
      if (thenFlow.next)
      {
        joinInto(joined, *thenFlow.next);
      }
      if (elseFlow.next)
      {
        joinInto(joined, *elseFlow.next);
      }
      if (!joined)
      {
        return flow;
      }
      state = std::move(*joined);
      break;
    }
    case StmtKind::Loop:
    {
      // The body may run zero or more times.
      PathState entry = state;
      std::vector<PathState> loopExits;
      for (int pass = 0; pass < kMaxLoopPasses; ++pass)
      {
        auto body = execBlock(stmt.thenBlock, entry, loopDepth + 1);
        flow.exits.insert(flow.exits.end(), body.exits.begin(), body.exits.end());
        loopExits.insert(loopExits.end(), body.loopExits.begin(), body.loopExits.end());
        std::optional<PathState> merged = entry;
        if (body.next)
        {
          joinInto(merged, *body.next);
        }
        if (sameFacts(*merged, entry))
        {
          break;
        }
        entry = std::move(*merged);
      }
      std::optional<PathState> after = entry;
      for (const auto& exitState : loopExits)
      {
        joinInto(after, exitState);
      }
      state = std::move(*after);
      break;
    }
    case StmtKind::ExitIf:
      if (loopDepth > 0)
      {
        flow.loopExits.push_back(state);
      }
      else
      {
        // Outside a loop, `exitif` leaves the program level.
        auto exitState = state;
        if (!stmt.ignoreClose)
        {
          endFrame(exitState, frameIndex, frame.codeFile, stmt.line);
        }
        flow.exits.push_back(std::move(exitState));
      }
      break;
    case StmtKind::Exit:
      if (stmt.ignoreClose)
      {
        std::erase_if(state.facts, [&](const Fact& fact) { return fact.home == frameIndex; });
        std::erase_if(state.aliases, [&](const Fact& alias) { return alias.home == frameIndex; });
      }
      else
      {
        endFrame(state, frameIndex, frame.codeFile, stmt.line);
      }
      flow.exits.push_back(std::move(state));
      return flow;
    case StmtKind::Break:
      if (!stmt.ignoreClose)
      {
        leakAll(state, LeakReason::Break, frame.codeFile, stmt.line);
      }
      return flow;
    case StmtKind::DbValid:
    {
      auto handle = resolve(frameIndex, stmt.token, ResourceKind::Database);
      std::erase_if(state.valid, [&](const auto& entry) { return entry.first == stmt.var; });
      state.valid.emplace_back(stmt.var, std::move(handle));
      break;
    }
    case StmtKind::OpenResult:
    {
      auto handle = resolve(frameIndex, stmt.token, ResourceKind::Drawing);
      std::erase_if(state.valid, [&](const auto& entry) { return entry.first == stmt.var; });
      state.valid.emplace_back(stmt.var, std::move(handle));
      break;
    }
    case StmtKind::Assign:
      std::erase_if(state.valid, [&](const auto& entry) { return entry.first == stmt.var; });
      break;
    }
  }
  flow.next = std::move(state);
  return flow;
}

auto RootRun::execCall(const Stmt& stmt, PathState state) -> std::optional<PathState>
{
  const auto callerIndex = static_cast<int>(frames.size()) - 1;
  const auto& caller = frames.back();
  const auto& info = (*caller.infos)[static_cast<std::size_t>(stmt.line)];
  const auto& step = info.steps[static_cast<std::size_t>(stmt.step)];
  const auto target = walker.resolvePath(step.target.parsed.filePath);
  if (target.empty() || static_cast<int>(frames.size()) >= kMaxFrameDepth)
  {
    return state;
  }
  const auto& section = step.target.parsed.targetSection;
  const auto key = regionKey(target, section);
  if (stack.contains(key))
  {
    return state;
  }
  const auto* region = walker.getRegion(target, section);
  const auto* infos = walker.getParsed(target);
  if (!region || !infos || !region->exists)
  {
    return state;
  }

  const auto callsite = walker.registerCallSite(
    caller.codeFile, stmt.line, step, info.ignoreClose, walker.lineText(caller.codeFile, stmt.line));
  const auto flags = walker.commonFlags(target);
  Frame callee{
    .filePath = target,
    .codeFile = target,
    .region = region,
    .infos = infos,
    .dbNs = caller.shareDb ? caller.dbNs : ++nsCounter,
    .fileNs = caller.shareFiles ? caller.fileNs : ++nsCounter,
    .callsite = callsite,
    .shareDb = flags.first,
    .shareFiles = flags.second,
  };
  for (const auto& [var, index] : region->paramVars)
  {
    if (index < 1 || index > static_cast<int>(step.args.size()))
    {
      continue;
    }
    const auto& arg = step.args[static_cast<std::size_t>(index - 1)];
    if (!isSimpleHandleArg(arg))
    {
      continue;
    }
    auto handle = resolve(callerIndex, arg,
      isStringLiteralToken(trim(arg)) ? ResourceKind::Drawing : ResourceKind::Database);
    if (handle.attr == callerIndex)
    {
      handle.callsite = callsite;
    }
    callee.binding.emplace(var, std::move(handle));
  }

  frames.push_back(std::move(callee));
  stack.insert(key);
  auto body = execBlock(region->block, std::move(state), 0);
  std::optional<PathState> out = std::move(body.next);
  for (const auto& exitState : body.exits)
  {
    joinInto(out, exitState);
  }
  for (const auto& exitState : body.loopExits)
  {
    joinInto(out, exitState);
  }
  if (out)
  {
    endFrame(*out, static_cast<int>(frames.size()) - 1);
  }
  stack.erase(key);
  frames.pop_back();
  return out;
}

auto RootRun::execInclude(const Stmt& stmt, PathState state, int loopDepth) -> Flow
{
  Flow flow;
  const auto& frame = frames.back();
  const auto& info = (*frame.infos)[static_cast<std::size_t>(stmt.line)];
  if (!info.includeTarget)
  {
    flow.next = std::move(state);
    return flow;
  }
  const auto target = walker.resolvePath(info.includeTarget->filePath);
  const auto& section = info.includeTarget->targetSection;
  const auto key = regionKey(target, section);
  if (target.empty() || stack.contains(key))
  {
    flow.next = std::move(state);
    return flow;
  }
  const auto* region = walker.getRegion(target, section);
  const auto* infos = walker.getParsed(target);
  if (!region || !infos || !region->exists)
  {
    flow.next = std::move(state);
    return flow;
  }
  const auto savedSite = includeSite;
  if (includeSite < 0)
  {
    includeSite = walker.registerIncludeSite(
      frame.codeFile, stmt.line, info, walker.lineText(frame.codeFile, stmt.line));
  }
  // Included code runs in this frame; only its lines come from another file.
  const auto* savedInfos = frames.back().infos;
  auto savedCodeFile = frames.back().codeFile;
  frames.back().infos = infos;
  frames.back().codeFile = target;
  stack.insert(key);
  flow = execBlock(region->block, std::move(state), loopDepth);
  stack.erase(key);
  frames.back().infos = savedInfos;
  frames.back().codeFile = std::move(savedCodeFile);
  includeSite = savedSite;
  return flow;
}

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------

[[nodiscard]] auto handleNoun(const ResourceEvent& event) -> std::string
{
  if (event.kind == ResourceKind::File)
  {
    return "File";
  }
  if (event.kind == ResourceKind::Drawing)
  {
    return "Drawing";
  }
  return "Database";
}

[[nodiscard]] auto kindKey(ResourceKind kind) -> std::string
{
  switch (kind)
  {
  case ResourceKind::File:
    return "file";
  case ResourceKind::Drawing:
    return "drawing";
  case ResourceKind::Database:
    break;
  }
  return "database";
}

[[nodiscard]] auto openedVerb(const ResourceEvent& /*event*/) -> std::string
{
  return "opened";
}

[[nodiscard]] auto lowerNoun(const std::string& noun) -> std::string
{
  return toLowerAscii(noun);
}

// "break (line 20)", or "exit (other.prg line 20)" when the stop is in another file.
[[nodiscard]] auto stopPlace(const Leak& leak, const std::string& fromFile) -> std::string
{
  std::string place = leak.reason == LeakReason::Break ? "break (" : "exit (";
  if (leak.reasonFile != fromFile)
  {
    place += posixBasename(replaceBackslashes(leak.reasonFile)) + " ";
  }
  return place + "line " + std::to_string(leak.reasonLine + 1) + ")";
}

auto ResourceWalker::collectIssues() -> std::vector<UnclosedIssue>
{
  std::vector<UnclosedIssue> issues;
  if (currentFilePath.empty())
  {
    return issues;
  }
  const auto* infos = getParsed(currentFilePath);
  if (!infos)
  {
    return issues;
  }
  const auto regions = regionsOf(currentFilePath);
  for (const auto& section : regions)
  {
    analyzeRoot(currentFilePath, section);
  }

  std::optional<std::vector<std::string>> includeRoots;

  // Opens in this file.
  for (int i = 0; i < static_cast<int>(infos->size()); ++i)
  {
    const auto& info = (*infos)[static_cast<std::size_t>(i)];
    if (info.blank || info.ignoredSection)
    {
      continue;
    }
    for (const auto& step : info.steps)
    {
      if (!step.isResource || step.event.action != ResourceAction::Open || step.event.ignoreClose)
      {
        continue;
      }
      const auto* origin = &step.event;
      const auto& own = analyzeRoot(currentFilePath, info.sectionName);
      if (own.truncated || !own.reached.contains(origin))
      {
        continue;
      }
      bool directEnd = false;
      // `break` / `exit` points reached with this handle open, breaks first.
      std::vector<Leak> stops;
      ChainOutcome chain;
      for (const auto& leak : own.leaks)
      {
        if (leak.fact.origin != origin)
        {
          continue;
        }
        if (leak.fact.callsite >= 0)
        {
          chain.failing = true;
          chain.failingCallsites.emplace_back(leak.fact.callsite, leak.fact.label);
        }
        else if (leak.reason == LeakReason::End)
        {
          directEnd = true;
        }
        else
        {
          stops.push_back(leak);
        }
      }
      std::stable_sort(stops.begin(), stops.end(), [](const Leak& a, const Leak& b) {
        return a.reason == LeakReason::Break && b.reason != LeakReason::Break;
      });
      const auto* directStop = stops.empty() ? nullptr : &stops.front();
      std::vector<Fact> escapes;
      for (const auto& fact : own.escapes)
      {
        if (fact.origin == origin)
        {
          escapes.push_back(fact);
        }
      }
      if (!escapes.empty())
      {
        std::unordered_set<std::string> visited;
        walkCallers(currentFilePath, info.sectionName, origin, escapes, visited, chain);
      }

      std::vector<RelatedIssue> related;
      if (directEnd && !directStop)
      {
        if (!includeRoots)
        {
          includeRoots = getIncludeRoots(currentFilePath);
        }
        if (!includeRoots->empty())
        {
          std::vector<std::string> failingRoots;
          for (const auto& rootPath : *includeRoots)
          {
            if (!includeRootCloses(rootPath, origin))
            {
              failingRoots.push_back(rootPath);
            }
          }
          if (failingRoots.empty())
          {
            directEnd = false;
          }
          else
          {
            related = relatedForIncludeRoots(*origin, failingRoots);
          }
        }
      }
      if (!directEnd && !directStop && !chain.failing && !chain.noCaller)
      {
        continue;
      }

      const auto noun = handleNoun(*origin);
      const auto label = tokenLabel(*origin);
      std::string message;
      if (directStop)
      {
        message = noun + " " + label + " is " + openedVerb(*origin) + " here and still open at "
          + stopPlace(*directStop, origin->filePath);
        std::unordered_set<std::string> seenStops;
        for (const auto& stop : stops)
        {
          if (!seenStops.insert(stop.reasonFile + "|" + std::to_string(stop.reasonLine)).second)
          {
            continue;
          }
          related.push_back(relatedForLine(stop.reasonFile, stop.reasonLine,
            std::string(stop.reason == LeakReason::Break ? "Execution stops here" : "Execution leaves here")
              + " with " + lowerNoun(noun) + " " + label + " open"));
        }
      }
      else if (directEnd)
      {
        message = buildOpenMessage(*origin, false);
      }
      else if (chain.failing)
      {
        message = noun + " " + label + " is " + openedVerb(*origin) + " here and not closed on every caller path";
      }
      else
      {
        message = noun + " " + label + " is " + openedVerb(*origin) + " here for the caller, but no caller closes it";
      }
      std::unordered_set<int> seenSites;
      for (const auto& [id, callerLabel] : chain.failingCallsites)
      {
        if (seenSites.insert(id).second)
        {
          related.push_back(relatedForCallsite(id, "This call path does not close " + callerLabel));
        }
      }
      for (const auto& [id, callerLabel] : chain.noCallerCallsites)
      {
        if (id >= 0 && seenSites.insert(id).second)
        {
          related.push_back(relatedForCallsite(id, "No caller of this section closes " + callerLabel));
        }
      }
      const auto [openStart, openEnd] = u16Columns(origin->lineText, origin->startChar, origin->endChar);
      issues.push_back(UnclosedIssue{
        .line = origin->line,
        .startChar = openStart,
        .endChar = openEnd,
        .message = std::move(message),
        .related = std::move(related),
      });
    }
  }

  // Call lines in this file whose called code leaves a handle this section
  // must close, and include lines whose included code does.
  // Call-line issue index per (line, call, kind, handle), and the opens it already links to.
  std::unordered_map<std::string, std::size_t> callLineIssues;
  std::unordered_set<std::string> callLineOrigins;
  std::unordered_set<std::string> includeLeftoverKeys;
  for (const auto& section : regions)
  {
    const auto& result = analyzeRoot(currentFilePath, section);
    if (result.truncated)
    {
      continue;
    }
    auto reportCallLine = [&](const Fact& fact, std::string suffix) {
      const auto& site = callSites[static_cast<std::size_t>(fact.callsite)];
      if (site.filePath != currentFilePath || site.ignoreClose)
      {
        return;
      }
      const auto key = std::to_string(site.line) + "::" + std::to_string(site.startChar) + "::"
        + kindKey(fact.kind) + "::" + fact.token;
      const auto noun = handleNoun(*fact.origin);
      auto openedHere = relatedAt(fact.origin->filePath, fact.origin->line, fact.origin->startChar,
        fact.origin->endChar, fact.origin->lineText,
        noun + " " + tokenLabel(*fact.origin) + " is " + openedVerb(*fact.origin) + " here");
      const auto originKey = key + "::" + fact.origin->filePath + "|" + std::to_string(fact.origin->line) + "|"
        + std::to_string(fact.origin->startChar);
      if (const auto it = callLineIssues.find(key); it != callLineIssues.end())
      {
        if (callLineOrigins.insert(originKey).second)
        {
          issues[it->second].related.push_back(std::move(openedHere));
        }
        return;
      }
      callLineOrigins.insert(originKey);
      const auto [start, end] = u16Columns(site.lineText, site.startChar, site.endChar);
      UnclosedIssue issue{
        .line = site.line,
        .startChar = start,
        .endChar = end,
        .message = noun + " " + fact.label + " " + openedVerb(*fact.origin) + " by the called code " + suffix,
      };
      issue.related.push_back(std::move(openedHere));
      callLineIssues.emplace(key, issues.size());
      issues.push_back(std::move(issue));
    };

    for (const auto& leak : result.leaks)
    {
      const auto& fact = leak.fact;
      if (fact.attr == 0 && fact.callsite >= 0)
      {
        reportCallLine(fact, leak.reason == LeakReason::End
          ? "is not closed on every path after this call"
          : "is still open at " + stopPlace(leak, currentFilePath));
      }
      if (fact.includeSite < 0)
      {
        continue;
      }
      const auto& site = includeSites[static_cast<std::size_t>(fact.includeSite)];
      if (site.filePath != currentFilePath || site.ignoreClose || fact.origin->ignoreClose)
      {
        continue;
      }
      const auto& event = *fact.origin;
      const auto key = std::to_string(site.line) + "::"
        + kindKey(event.kind) + "::" + toLowerAscii(event.token);
      if (!includeLeftoverKeys.insert(key).second)
      {
        continue;
      }
      const auto [siteStart, siteEnd] = u16Columns(site.lineText, site.startChar, site.endChar);
      UnclosedIssue issue{
        .line = site.line,
        .startChar = siteStart,
        .endChar = siteEnd,
        .message = buildOpenMessage(event, true),
      };
      issue.related.push_back(relatedAt(event.filePath, event.line, event.startChar, event.endChar,
        event.lineText, buildOpenMessage(event, false)));
      issues.push_back(std::move(issue));
    }
    if (!result.escapes.empty() && findCallers(currentFilePath, section).empty())
    {
      for (const auto& fact : result.escapes)
      {
        if (fact.callsite >= 0)
        {
          reportCallLine(fact, "is passed up to the caller, but no caller of this section closes it");
        }
      }
    }
  }
  appendAliasCloseIssues(regions, includeRoots, issues);
  return issues;
}

// `dbclose` lines in this file that close a handle aliased with ALREADY_OPEN.
// The alias may come from a caller or an include root; those are walked only
// when a close here matched nothing in this file's own roots.
void ResourceWalker::appendAliasCloseIssues(const std::vector<std::string>& regions,
  std::optional<std::vector<std::string>>& includeRoots, std::vector<UnclosedIssue>& issues)
{
  std::vector<const RootResult*> roots;
  std::vector<std::pair<std::string, std::string>> queue;
  std::unordered_set<std::string> visited;
  bool closeFromOutside = false;
  for (const auto& section : regions)
  {
    visited.insert(regionKey(currentFilePath, section));
    queue.emplace_back(currentFilePath, section);
    const auto& result = analyzeRoot(currentFilePath, section);
    if (result.truncated)
    {
      continue;
    }
    roots.push_back(&result);
    closeFromOutside = closeFromOutside
      || std::any_of(result.unmatchedCloses.begin(), result.unmatchedCloses.end(),
        [&](const ResourceEvent* close) { return close->filePath == currentFilePath; });
  }
  if (closeFromOutside)
  {
    auto visit = [&](const std::string& filePath, const std::string& section) {
      if (visited.size() >= kMaxAliasRegions || !visited.insert(regionKey(filePath, section)).second)
      {
        return;
      }
      queue.emplace_back(filePath, section);
      const auto& result = analyzeRoot(filePath, section);
      if (!result.truncated)
      {
        roots.push_back(&result);
      }
    };
    if (!includeRoots)
    {
      includeRoots = getIncludeRoots(currentFilePath);
    }
    for (const auto& rootPath : *includeRoots)
    {
      for (const auto& section : regionsOf(rootPath))
      {
        visit(rootPath, section);
      }
    }
    for (std::size_t i = 0; i < queue.size() && visited.size() < kMaxAliasRegions; ++i)
    {
      const auto [filePath, section] = queue[i];
      for (const auto& [callerFile, callerSection] : findCallers(filePath, section))
      {
        visit(callerFile, callerSection);
      }
    }
  }

  std::vector<const ResourceEvent*> closes;
  std::unordered_map<const ResourceEvent*, std::vector<const ResourceEvent*>> aliasesByClose;
  for (const auto* result : roots)
  {
    for (const auto& hit : result->aliasCloses)
    {
      if (hit.close->filePath != currentFilePath)
      {
        continue;
      }
      auto& aliases = aliasesByClose[hit.close];
      if (aliases.empty())
      {
        closes.push_back(hit.close);
      }
      if (std::find(aliases.begin(), aliases.end(), hit.alias) == aliases.end())
      {
        aliases.push_back(hit.alias);
      }
    }
  }
  std::sort(closes.begin(), closes.end(), [](const ResourceEvent* a, const ResourceEvent* b) {
    return std::pair(a->line, a->startChar) < std::pair(b->line, b->startChar);
  });
  for (const auto* close : closes)
  {
    const auto& aliases = aliasesByClose[close];
    const auto* first = aliases.front();
    std::string place = "line " + std::to_string(first->line + 1);
    if (first->filePath != currentFilePath)
    {
      place = posixBasename(replaceBackslashes(first->filePath)) + " " + place;
    }
    UnclosedIssue issue;
    issue.line = close->line;
    std::tie(issue.startChar, issue.endChar) = u16Columns(close->lineText, close->startChar, close->endChar);
    issue.message = "Database " + tokenLabel(*close) + " was aliased with ALREADY_OPEN (" + place
      + "); dbclose closes the source program's database";
    issue.closesAlias = true;
    for (const auto* alias : aliases)
    {
      issue.related.push_back(relatedAt(alias->filePath, alias->line, alias->startChar, alias->endChar,
        alias->lineText, "Database " + tokenLabel(*alias) + " is aliased with ALREADY_OPEN here"));
    }
    issues.push_back(std::move(issue));
  }
}

lsp::Diagnostic toDiagnostic(const UnclosedIssue& issue)
{
  lsp::Diagnostic diagnostic;
  diagnostic.range = makeRange(issue.line, issue.startChar, issue.endChar);
  diagnostic.message = issue.message;
  diagnostic.source = "ADT-Syntax";
  diagnostic.severity = lsp::DiagnosticSeverity::Warning;
  diagnostic.code = std::string(issue.closesAlias ? kCloseAlreadyOpenCode : kUnclosedResourceCode);
  lsp::json::Object data;
  data["line"] = issue.line;
  diagnostic.data = lsp::json::Value(std::move(data));
  if (!issue.related.empty())
  {
    std::vector<lsp::DiagnosticRelatedInformation> related;
    related.reserve(issue.related.size());
    for (const auto& item : issue.related)
    {
      lsp::DiagnosticRelatedInformation info;
      info.location.uri = item.uri;
      info.location.range = makeRange(item.line, item.startChar, item.endChar);
      info.message = item.message;
      related.push_back(std::move(info));
    }
    diagnostic.relatedInformation = std::move(related);
  }
  return diagnostic;
}

} // namespace

void familyUnclosedResources(ValidateContext& ctx)
{
  if (!ctx.settings.warnUnclosedResources || ctx.atCap())
  {
    return;
  }

  ResourceWalker walker{
    .ctx = ctx,
    .currentFilePath = normalizeFsPath(pathFromUri(ctx.prep->snapshot.uri).string()),
  };
  for (const auto& issue : walker.collectIssues())
  {
    ctx.pollCancel();
    if (ctx.atCap())
    {
      break;
    }
    ctx.append(toDiagnostic(issue));
  }
}

} // namespace vertexadt
