#include "cross_call_types.h"

#include "command_parse.h"
#include "included_parent_cache.h"
#include "text_util.h"
#include "utf16_text.h"
#include "validate.h"
#include "workspace_index.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

// Bounds on one walk: chain depth from the analyzed file, and files visited.
// Past them a file contributes nothing (as if unresolved).
constexpr int kMaxDepth = 6;
constexpr std::size_t kMaxFiles = 300;

const std::unordered_set<std::string_view> kVectorFamily{
  "pvector", "dvector", "xvector", "yvector", "zvector", "nvector",
};

bool isIdentStart(char ch)
{
  return std::isalpha(static_cast<unsigned char>(ch)) != 0;
}

bool isIdentChar(char ch)
{
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

bool isSpace(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r';
}

bool iequals(std::string_view a, std::string_view b)
{
  if (a.size() != b.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i)
  {
    if (std::tolower(static_cast<unsigned char>(a[i]))
      != std::tolower(static_cast<unsigned char>(b[i])))
    {
      return false;
    }
  }
  return true;
}

bool containsCallToken(std::string_view text)
{
  for (std::size_t i = 0; i + 4 <= text.size(); ++i)
  {
    if (iequals(text.substr(i, 4), "call"))
    {
      return true;
    }
  }
  return false;
}

// First `[…]` of a hover comment, trimmed and lowercased.
std::string bracketType(std::string_view comment)
{
  const auto open = comment.find('[');
  if (open == std::string_view::npos)
  {
    return {};
  }
  const auto close = comment.find(']', open + 1);
  if (close == std::string_view::npos)
  {
    return {};
  }
  return toLowerAscii(trim(comment.substr(open + 1, close - open - 1)));
}

std::size_t skipSpaces(std::string_view text, std::size_t i)
{
  while (i < text.size() && isSpace(text[i]))
  {
    ++i;
  }
  return i;
}

std::size_t identEnd(std::string_view text, std::size_t i)
{
  if (i >= text.size() || !isIdentStart(text[i]))
  {
    return i;
  }
  ++i;
  while (i < text.size() && isIdentChar(text[i]))
  {
    ++i;
  }
  return i;
}

// `name` or `name$` assigned at the start of `code`.
struct Assignment
{
  std::string_view name; // without `$`
  bool isString = false;
  std::size_t nameStart = 0;
  std::size_t nameEnd = 0; // after the name, before `$`
  std::string_view rhs;    // trimmed
};

std::optional<Assignment> leadingAssignment(std::string_view code)
{
  const auto start = skipSpaces(code, 0);
  const auto end = identEnd(code, start);
  if (end == start)
  {
    return std::nullopt;
  }
  auto j = end;
  bool isString = false;
  if (j < code.size() && code[j] == '$')
  {
    isString = true;
    ++j;
  }
  j = skipSpaces(code, j);
  if (j >= code.size() || code[j] != '=' || (j + 1 < code.size() && code[j + 1] == '='))
  {
    return std::nullopt;
  }
  return Assignment{
    .name = code.substr(start, end - start),
    .isString = isString,
    .nameStart = start,
    .nameEnd = end,
    .rhs = trim(code.substr(j + 1)),
  };
}

// `get($CALLPARAMn)`, case-insensitive, whole right-hand side.
std::optional<int> callParamIndex(std::string_view rhs)
{
  static const std::regex re{R"(^get\s*\(\s*\$CALLPARAM(\d+)\s*\)$)", std::regex::icase};
  std::cmatch match;
  if (!std::regex_match(rhs.data(), rhs.data() + rhs.size(), match, re))
  {
    return std::nullopt;
  }
  return std::stoi(match[1].str());
}

// `strfield(R$, "KEY")`, case-insensitive name.
struct StrfieldUse
{
  std::string source; // `R` without `$`
  std::string key;
};

std::optional<StrfieldUse> strfieldUse(std::string_view rhs)
{
  static const std::regex re{
    R"re(^strfield\s*\(\s*([A-Za-z][A-Za-z0-9_]*)\$\s*,\s*"([^"]*)"\s*\)$)re", std::regex::icase};
  std::cmatch match;
  if (!std::regex_match(rhs.data(), rhs.data() + rhs.size(), match, re))
  {
    return std::nullopt;
  }
  return StrfieldUse{.source = match[1].str(), .key = match[2].str()};
}

// Top-level comma split of `code[open+1, close)`, quotes and brackets respected.
std::vector<CallArgFact> splitArguments(std::string_view code, std::size_t open, std::size_t close)
{
  std::vector<CallArgFact> args;
  int depth = 0;
  char quote = 0;
  std::size_t partStart = open + 1;
  auto push = [&](std::size_t end) {
    auto s = partStart;
    auto e = end;
    while (s < e && (isSpace(code[s]) || code[s] == '\n'))
    {
      ++s;
    }
    while (e > s && (isSpace(code[e - 1]) || code[e - 1] == '\n'))
    {
      --e;
    }
    args.push_back(CallArgFact{.text = std::string(code.substr(s, e - s)), .start = s, .end = e});
  };
  for (auto i = open + 1; i < close; ++i)
  {
    const auto ch = code[i];
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
    else if (ch == '(' || ch == '[')
    {
      ++depth;
    }
    else if (ch == ')' || ch == ']')
    {
      --depth;
    }
    else if (ch == ',' && depth == 0)
    {
      push(i);
      partStart = i + 1;
    }
  }
  push(close);
  return args;
}

void scanCallSites(
  std::string_view code,
  int line,
  const std::string& section,
  bool ignoreType,
  std::vector<CallSiteFact>& out)
{
  for (std::size_t i = 0; i + 4 <= code.size(); ++i)
  {
    if (!iequals(code.substr(i, 4), "call"))
    {
      continue;
    }
    if ((i > 0 && isIdentChar(code[i - 1])) || isInsideQuotes(code, i))
    {
      continue;
    }
    const auto open = skipSpaces(code, i + 4);
    if (open >= code.size() || code[open] != '(')
    {
      continue;
    }
    const auto close = findMatchingParenIndex(code, static_cast<int>(open));
    if (close < 0)
    {
      continue;
    }
    auto parts = splitArguments(code, open, static_cast<std::size_t>(close));
    if (parts.empty() || parts.front().text.empty())
    {
      continue;
    }
    // `"path" SECTION`: the suffix after a closed quote is the line remainder.
    const auto& first = parts.front().text;
    IncludeCallTarget target;
    if ((first.front() == '"' || first.front() == '\'') && first.find(first.front(), 1) != std::string::npos)
    {
      const auto closeQuote = first.find(first.front(), 1);
      target = splitIncludeCallTarget(
        std::string_view(first).substr(0, closeQuote + 1),
        std::string_view(first).substr(closeQuote + 1));
    }
    else
    {
      target = splitIncludeCallTarget(first, "");
    }
    CallSiteFact site;
    site.line = line;
    site.section = section;
    site.calleePath = target.filePath;
    site.targetSection = target.targetSection;
    site.ignoreType = ignoreType;
    site.args.assign(std::next(parts.begin()), parts.end());
    if (site.args.size() == 1 && site.args.front().text.empty())
    {
      site.args.clear();
    }
    // `R$ = call(` immediately before.
    auto k = i;
    while (k > 0 && isSpace(code[k - 1]))
    {
      --k;
    }
    if (k > 0 && code[k - 1] == '=')
    {
      auto m = k - 1;
      while (m > 0 && isSpace(code[m - 1]))
      {
        --m;
      }
      if (m > 0 && code[m - 1] == '$')
      {
        auto s = m - 1;
        while (s > 0 && isIdentChar(code[s - 1]))
        {
          --s;
        }
        if (s < m - 1 && isIdentStart(code[s]))
        {
          site.assignedTo = std::string(code.substr(s, m - 1 - s));
        }
      }
    }
    out.push_back(std::move(site));
    i = static_cast<std::size_t>(close);
  }
}

// `exit\s+(\w+\$)`, leftmost, on a line starting with `exit`.
std::optional<std::string> exitName(std::string_view trimmed)
{
  if (!trimmed.starts_with("exit"))
  {
    return std::nullopt;
  }
  for (auto p = trimmed.find("exit"); p != std::string_view::npos; p = trimmed.find("exit", p + 1))
  {
    const auto w = skipSpaces(trimmed, p + 4);
    if (w == p + 4)
    {
      continue;
    }
    auto e = w;
    while (e < trimmed.size() && isIdentChar(trimmed[e]))
    {
      ++e;
    }
    if (e > w && e < trimmed.size() && trimmed[e] == '$')
    {
      return std::string(trimmed.substr(w, e + 1 - w));
    }
  }
  return std::nullopt;
}

bool isKeyChar(char ch)
{
  return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
}

// Fields of one format line: `"KEY=" & value` and `"|KEY=" & value`.
void scanFormatLine(
  std::string_view raw,
  int line,
  const std::string& section,
  std::vector<ReturnFieldFact>& out)
{
  std::size_t cursor = 0;
  while (true)
  {
    const auto open = raw.find('"', cursor);
    if (open == std::string_view::npos)
    {
      return;
    }
    const auto close = raw.find('"', open + 1);
    if (close == std::string_view::npos)
    {
      return;
    }
    auto from = open + 1;
    if (from < close && raw[from] == '|')
    {
      ++from;
    }
    auto e = from;
    while (e < close && isKeyChar(raw[e]))
    {
      ++e;
    }
    if (e > from && e < close && raw[e] == '=')
    {
      ReturnFieldFact field;
      field.line = line;
      field.section = section;
      field.key = std::string(raw.substr(from, e - from));
      field.keyStart = from;
      field.keyEnd = e;
      auto v = skipSpaces(raw, close + 1);
      if (v < raw.size() && raw[v] == '&')
      {
        v = skipSpaces(raw, v + 1);
        auto end = v;
        if (end < raw.size() && isIdentStart(raw[end]))
        {
          end = identEnd(raw, end);
          if (end < raw.size() && raw[end] == '$')
          {
            ++end;
          }
          else if (end < raw.size() && raw[end] == '(')
          {
            const auto paren = findMatchingParenIndex(raw, static_cast<int>(end));
            if (paren > 0)
            {
              end = static_cast<std::size_t>(paren) + 1;
            }
          }
        }
        else
        {
          while (end < raw.size()
            && (std::isdigit(static_cast<unsigned char>(raw[end])) || raw[end] == '.'
              || raw[end] == '-'))
          {
            ++end;
          }
        }
        // A token followed by another operator is an expression.
        const auto after = skipSpaces(raw, end);
        if (end > v && (after >= raw.size() || raw[after] == '&' || raw[after] == ';'))
        {
          field.value = std::string(raw.substr(v, end - v));
        }
      }
      out.push_back(std::move(field));
    }
    cursor = close + 1;
  }
}

std::pair<std::size_t, std::size_t> commentSpan(std::string_view raw)
{
  const auto bang = findBangCommentIndex(raw);
  if (bang < 0)
  {
    return {0, 0};
  }
  auto end = raw.size();
  while (end > 0 && (raw[end - 1] == '\r' || raw[end - 1] == '\n'))
  {
    --end;
  }
  return {static_cast<std::size_t>(bang), end};
}

// `^[+-]?\d+(\.\d+)?$`
bool isNumberLiteral(std::string_view text)
{
  std::size_t i = 0;
  if (i < text.size() && (text[i] == '+' || text[i] == '-'))
  {
    ++i;
  }
  const auto digits = [&]() {
    const auto start = i;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])))
    {
      ++i;
    }
    return i > start;
  };
  if (!digits())
  {
    return false;
  }
  if (i < text.size() && text[i] == '.')
  {
    ++i;
    if (!digits())
    {
      return false;
    }
  }
  return i == text.size();
}

std::string writeTypeAt(const DocumentPrep& prep, std::string_view name, int line)
{
  const auto section = sectionNameForLine(line, prep.sections);
  std::string type;
  int bestLine = -1;
  const auto consider = [&](const TypeWrite& write) {
    if (write.name == name && write.line < line && write.section == section
      && write.line >= bestLine)
    {
      type = write.type;
      bestLine = write.line;
    }
  };
  if (!prep.types.byName.empty())
  {
    if (const auto it = prep.types.byName.find(std::string(name)); it != prep.types.byName.end())
    {
      for (const auto index : it->second)
      {
        consider(prep.types.writes[index]);
      }
    }
    return type;
  }
  for (const auto& write : prep.types.writes)
  {
    consider(write);
  }
  return type;
}

bool isUntyped(std::string_view type)
{
  return type.empty() || iequals(type, "generic") || iequals(type, "string")
    || iequals(type, "array") || iequals(type, "any");
}

std::size_t textHash(std::string_view text)
{
  return std::hash<std::string_view>{}(text);
}

lsp::Range rangeOn(int line, std::string_view lineText, std::size_t start, std::size_t end)
{
  return lsp::Range{
    .start = lsp::Position{
      .line = static_cast<lsp::Uint>(line),
      .character = static_cast<lsp::Uint>(utf16Offset(lineText, std::min(start, lineText.size()))),
    },
    .end = lsp::Position{
      .line = static_cast<lsp::Uint>(line),
      .character = static_cast<lsp::Uint>(utf16Offset(lineText, std::min(end, lineText.size()))),
    },
  };
}

CrossCallLocation locationOn(
  const lsp::Uri& uri,
  const std::vector<std::string_view>& lines,
  int line,
  std::size_t start,
  std::size_t end,
  std::string type)
{
  const auto text = line >= 0 && line < static_cast<int>(lines.size())
    ? lines[static_cast<std::size_t>(line)]
    : std::string_view{};
  const auto range = rangeOn(line, text, start, end);
  return CrossCallLocation{
    .uri = uri,
    .line = line,
    .startCharacter = range.start.character,
    .endCharacter = range.end.character,
    .type = std::move(type),
  };
}

bool sectionMatches(std::string_view readSection, std::string_view targetSection)
{
  return iequals(readSection, targetSection);
}

void sortLocations(std::vector<CrossCallLocation>& locations)
{
  std::stable_sort(
    locations.begin(), locations.end(), [](const CrossCallLocation& a, const CrossCallLocation& b) {
      const auto au = a.uri.toString();
      const auto bu = b.uri.toString();
      if (au != bu)
      {
        return au < bu;
      }
      return a.line < b.line;
    });
}

// Agreed type and conflict flag over typed locations.
std::pair<std::string, bool> agree(const std::vector<CrossCallLocation>& locations)
{
  if (locations.empty())
  {
    return {{}, false};
  }
  const auto& first = locations.front().type;
  for (const auto& location : locations)
  {
    if (!crossCallTypesCompatible(first, location.type))
    {
      return {{}, true};
    }
  }
  return {first, false};
}

} // namespace

// ---------------------------------------------------------------------------

auto extractCallFacts(std::string_view text) -> CallFacts
{
  CallFacts facts;
  const auto lines = splitLines(text);
  const auto sections = parseFileSections(lines);

  struct FieldVar
  {
    std::size_t site = 0;
    std::string key;
  };
  std::unordered_map<std::string, std::size_t> callResults; // R -> site
  std::unordered_map<std::string, FieldVar> fieldVars;      // X -> site/key

  std::string returnVar;
  for (const auto raw : lines)
  {
    if (const auto name = exitName(trim(raw)))
    {
      returnVar = *name;
    }
  }

  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto raw = lines[static_cast<std::size_t>(i)];
    const auto trimmed = trim(raw);
    if (trimmed.empty() || trimmed.starts_with("!"))
    {
      continue;
    }
    if (lineInVerificationIgnoredSection(i, sections))
    {
      continue;
    }
    if (trimmed.size() >= 15 && iequals(trimmed.substr(0, 15), "##included_from"))
    {
      facts.includedFrom.emplace_back(trim(trimmed.substr(15)));
      continue;
    }
    if (trimmed.starts_with("#"))
    {
      continue;
    }
    const auto section = sectionNameForLine(i, sections);
    const bool ignoreType = raw.find("IGNORE TYPE") != std::string_view::npos;
    auto code = stripValidationComment(raw);
    if (const auto bang = findInlineBangCommentIndex(code); bang >= 0)
    {
      code.resize(static_cast<std::size_t>(bang));
    }
    const auto hover = captureHoverComment(raw);
    const auto commentType = hover ? bracketType(*hover) : std::string{};
    const auto [commentStart, commentEnd] = commentSpan(raw);

    const auto firstSite = facts.calls.size();
    scanCallSites(code, i, section, ignoreType, facts.calls);

    // Format lines, mirroring call return-value completion's scan.
    if (trimmed.find('=') != std::string_view::npos && trimmed.find('"') != std::string_view::npos
      && trimmed.find('&') != std::string_view::npos
      && trimmed.find(returnVar) != std::string_view::npos)
    {
      if (const auto assignment = leadingAssignment(raw); assignment && assignment->isString)
      {
        returnVar = std::string(assignment->name) + "$";
        scanFormatLine(raw, i, section, facts.returnFields);
      }
    }

    const auto assignment = leadingAssignment(code);
    if (!assignment)
    {
      continue;
    }
    const auto name = std::string(assignment->name);
    if (assignment->isString)
    {
      bool fromCall = false;
      for (auto s = firstSite; s < facts.calls.size(); ++s)
      {
        if (facts.calls[s].assignedTo == name)
        {
          callResults[name] = s;
          fieldVars.erase(name);
          fromCall = true;
        }
      }
      if (fromCall)
      {
        continue;
      }
      if (const auto use = strfieldUse(assignment->rhs))
      {
        if (const auto it = callResults.find(use->source); it != callResults.end())
        {
          fieldVars[name] = FieldVar{.site = it->second, .key = use->key};
          callResults.erase(name);
          continue;
        }
      }
      callResults.erase(name);
      fieldVars.erase(name);
      continue;
    }

    if (const auto index = callParamIndex(assignment->rhs))
    {
      if (!ignoreType && *index > 0)
      {
        facts.params.push_back(ParamReadFact{
          .line = i,
          .section = section,
          .name = name,
          .index = *index,
          .nameStart = assignment->nameStart,
          .nameEnd = assignment->nameEnd,
          .commentType = commentType,
          .commentStart = commentStart,
          .commentEnd = commentEnd,
        });
      }
      continue;
    }

    if (ignoreType)
    {
      continue;
    }
    std::optional<FieldVar> source;
    const auto rhs = assignment->rhs;
    if (rhs.size() >= 2 && rhs.back() == '$' && identEnd(rhs, 0) == rhs.size() - 1)
    {
      if (const auto it = fieldVars.find(std::string(rhs.substr(0, rhs.size() - 1)));
        it != fieldVars.end())
      {
        source = it->second;
      }
    }
    else if (const auto use = strfieldUse(rhs))
    {
      if (const auto it = callResults.find(use->source); it != callResults.end())
      {
        source = FieldVar{.site = it->second, .key = use->key};
      }
    }
    if (source)
    {
      facts.receivers.push_back(ReceiverFact{
        .line = i,
        .section = section,
        .name = name,
        .nameStart = assignment->nameStart,
        .nameEnd = assignment->nameEnd,
        .callSite = source->site,
        .key = source->key,
        .commentType = commentType,
        .commentStart = commentStart,
        .commentEnd = commentEnd,
      });
    }
  }
  return facts;
}

bool crossCallTypesCompatible(std::string_view a, std::string_view b)
{
  if (isUntyped(a) || isUntyped(b))
  {
    return true;
  }
  const auto la = toLowerAscii(a);
  const auto lb = toLowerAscii(b);
  if (la == lb)
  {
    return true;
  }
  return kVectorFamily.contains(la) && kVectorFamily.contains(lb);
}

auto crossCallArgumentType(const DocumentPrep& prep, std::string_view argument, int line)
  -> std::string
{
  const auto text = trim(argument);
  if (text.empty() || text.find('"') != std::string_view::npos
    || text.find('\'') != std::string_view::npos || text.find('&') != std::string_view::npos)
  {
    return {};
  }
  if (isNumberLiteral(text))
  {
    return "value";
  }
  const auto end = identEnd(text, 0);
  if (end == 0)
  {
    return {};
  }
  // `name` or an element `name(…)`.
  if (end != text.size() && !(text[end] == '(' && text.back() == ')'))
  {
    return {};
  }
  const auto type = writeTypeAt(prep, text.substr(0, end), line);
  if (isUntyped(type))
  {
    return {};
  }
  return toLowerAscii(type);
}

auto CrossCallInjection::signature() const -> std::string
{
  std::map<int, const std::vector<Write>*> ordered;
  for (const auto& [line, writes] : lineWrites)
  {
    ordered.emplace(line, &writes);
  }
  std::string out;
  for (const auto& [line, writes] : ordered)
  {
    out += std::to_string(line);
    for (const auto& write : *writes)
    {
      out += ':' + write.name + '=' + write.type;
    }
    out += ';';
  }
  std::map<std::string, std::string> overrideOrder(overrides.begin(), overrides.end());
  for (const auto& [key, type] : overrideOrder)
  {
    out += key + '=' + type + ';';
  }
  return out;
}

// Per-key return-field types from a finished prep. Exposed for the family.
auto crossCallReturnFieldTypes(
  const DocumentPrep& prep,
  const CallFacts& facts,
  const lsp::Uri& uri) -> std::vector<CrossCallReturnField>
{
  std::vector<CrossCallReturnField> fields;
  for (const auto& field : facts.returnFields)
  {
    const auto type = field.value.empty()
      ? std::string{}
      : crossCallArgumentType(prep, field.value, field.line);
    auto it = std::find_if(fields.begin(), fields.end(), [&](const CrossCallReturnField& f) {
      return f.key == field.key;
    });
    if (it == fields.end())
    {
      fields.push_back(CrossCallReturnField{.key = field.key});
      it = std::prev(fields.end());
    }
    if (!type.empty())
    {
      it->lines.push_back(
        locationOn(uri, prep.lines, field.line, field.keyStart, field.keyEnd, type));
    }
  }
  for (auto& field : fields)
  {
    field.conflict = agree(field.lines).second;
  }
  return fields;
}

// ---------------------------------------------------------------------------
// The walk.

class CrossCallWalk
{
public:
  CrossCallWalk(CrossCallResolver& resolver, const CrossCallEnvironment& env)
    : resolver_(resolver)
    , env_(env)
  {
    if (env_.index)
    {
      for (const auto& entry : env_.index->snapshot())
      {
        std::unordered_set<std::string> seen;
        for (const auto& link : entry->callLinks)
        {
          if (seen.insert(link.target).second)
          {
            callersByKey_[link.target].push_back(entry);
          }
        }
      }
    }
  }

  // The root's final prep is built by the caller with the returned
  // injection, so its full stage stays in progress: a walk that comes back
  // to it for return fields sees none (a cycle).
  auto analyzeRoot(const std::filesystem::path& path, const std::string& text) -> CrossCallAnalysis
  {
    auto& node = makeNode(path, text);
    ensureParams(node, 0);
    node.full = State::InProgress;
    ensureReceivers(node, 0);
    buildExpectations(node);
    CrossCallAnalysis analysis;
    analysis.injection = injectionFor(node, true);
    analysis.report = node.report;
    if (node.upPrep
      && injectionFor(node, false).signature() == analysis.injection.signature())
    {
      analysis.sameInjectionPrep = node.upPrep;
    }
    return analysis;
  }

private:
  enum class State
  {
    None,
    InProgress,
    Done,
  };

  struct Node
  {
    std::string norm;
    std::filesystem::path path;
    lsp::Uri uri;
    std::string text;
    std::vector<std::string_view> lines;
    std::shared_ptr<const CallFacts> facts;
    std::shared_ptr<CrossCallReport> report;
    State params = State::None;
    State full = State::None;
    // Built with parameter writes only: what callees read for arguments.
    std::shared_ptr<const DocumentPrep> upPrep;
    // Built with parameter and receiver writes: what callers read for returns.
    std::shared_ptr<const DocumentPrep> prep;
    std::unordered_map<std::string, std::string> returnTypes; // key -> agreed type
  };

  struct Target
  {
    std::string norm;
    std::string key;
  };

  void poll() const
  {
    if (env_.pollCancel)
    {
      env_.pollCancel();
    }
  }

  auto factsFor(const std::string& norm, const std::string& text) -> std::shared_ptr<const CallFacts>
  {
    const auto hash = textHash(text);
    {
      const auto lock = std::lock_guard(resolver_.cacheMutex_);
      if (const auto it = resolver_.facts_.find(norm);
        it != resolver_.facts_.end() && it->second.textHash == hash)
      {
        return it->second.facts;
      }
    }
    auto facts = std::make_shared<const CallFacts>(extractCallFacts(text));
    const auto lock = std::lock_guard(resolver_.cacheMutex_);
    resolver_.facts_[norm] = CrossCallResolver::FactsEntry{.textHash = hash, .facts = facts};
    return facts;
  }

  auto makeNode(const std::filesystem::path& path, std::string text) -> Node&
  {
    auto node = std::make_unique<Node>();
    node->norm = normalizeFsPath(path.string());
    node->path = path;
    node->uri = uriFromPath(path);
    node->text = std::move(text);
    node->lines = splitLines(node->text);
    node->facts = factsFor(node->norm, node->text);
    node->report = std::make_shared<CrossCallReport>();
    node->report->facts = node->facts;
    auto& ref = *node;
    nodes_[ref.norm] = std::move(node);
    return ref;
  }

  struct Resolved
  {
    std::filesystem::path path;
    std::string norm;
  };

  // Memoized per walk: resolution stats the disk.
  auto resolveSite(const std::string& raw) -> const Resolved*
  {
    if (raw.empty() || !env_.layout)
    {
      return nullptr;
    }
    auto it = resolved_.find(raw);
    if (it == resolved_.end())
    {
      std::optional<Resolved> value;
      if (auto path = env_.layout->resolveVertexFilePath(raw))
      {
        value = Resolved{.path = *path, .norm = normalizeFsPath(path->string())};
      }
      it = resolved_.emplace(raw, std::move(value)).first;
    }
    return it->second ? &*it->second : nullptr;
  }

  auto resolvePath(std::string_view raw) -> std::optional<std::filesystem::path>
  {
    const auto* resolved = resolveSite(std::string(raw));
    return resolved ? std::optional(resolved->path) : std::nullopt;
  }

  auto keyOf(const std::filesystem::path& path) const -> std::string
  {
    if (!env_.layout)
    {
      return {};
    }
    const auto identity = env_.layout->overlayForPath(path);
    return identity ? identity->key : std::string{};
  }

  auto textOf(const std::filesystem::path& path) const -> std::optional<std::string>
  {
    if (!env_.textForPath)
    {
      return std::nullopt;
    }
    return env_.textForPath(path);
  }

  // An existing node, or a new one within the walk bounds.
  auto nodeFor(const std::filesystem::path& path, int depth) -> Node*
  {
    poll();
    const auto norm = normalizeFsPath(path.string());
    if (const auto it = nodes_.find(norm); it != nodes_.end())
    {
      return it->second.get();
    }
    if (depth > kMaxDepth || nodes_.size() >= kMaxFiles)
    {
      return nullptr;
    }
    auto text = textOf(path);
    if (!text)
    {
      return nullptr;
    }
    return &makeNode(path, std::move(*text));
  }

  // What a callee reads from this caller: parameters resolved and a prep
  // built with them. Null for a cycle or a file outside the walk.
  auto ensureUp(const std::filesystem::path& path, int depth) -> Node*
  {
    auto* node = nodeFor(path, depth);
    if (!node || node->params == State::InProgress)
    {
      return nullptr;
    }
    ensureParams(*node, depth);
    if (!node->upPrep)
    {
      node->upPrep = prepFor(*node, injectionFor(*node, false));
    }
    return node;
  }

  // What a caller reads from this callee: its return-field types.
  auto ensureFull(const std::filesystem::path& path, int depth) -> Node*
  {
    auto* node = ensureUp(path, depth);
    if (!node || node->full == State::InProgress)
    {
      return nullptr;
    }
    if (node->full == State::Done)
    {
      return node;
    }
    node->full = State::InProgress;
    ensureReceivers(*node, depth);
    node->prep = prepFor(*node, injectionFor(*node, true));
    for (const auto& field : crossCallReturnFieldTypes(*node->prep, *node->facts, node->uri))
    {
      const auto [type, conflict] = agree(field.lines);
      if (!conflict && !type.empty())
      {
        node->returnTypes[field.key] = type;
      }
    }
    node->full = State::Done;
    return node;
  }

  auto prepFor(const Node& node, const CrossCallInjection& injection)
    -> std::shared_ptr<const DocumentPrep>
  {
    const auto hash = textHash(node.text);
    const auto signature = injection.signature();
    std::vector<CrossCallResolver::PrepEntry> cached;
    {
      const auto lock = std::lock_guard(resolver_.cacheMutex_);
      if (const auto it = resolver_.preps_.find(node.norm); it != resolver_.preps_.end())
      {
        cached = it->second;
      }
    }
    for (const auto& entry : cached)
    {
      if (entry.textHash != hash || entry.injectionSignature != signature
        || entry.strict != env_.strictModeEnabled)
      {
        continue;
      }
      const bool depsHold = std::all_of(
        entry.dependencies.begin(), entry.dependencies.end(), [&](const auto& dep) {
          const auto text = textOf(std::filesystem::path(dep.first));
          return (text ? textHash(*text) : 0) == dep.second;
        });
      if (depsHold)
      {
        return entry.prep;
      }
    }

    std::vector<std::pair<std::string, std::size_t>> dependencies;
    ValidateSettings settings;
    settings.strictModeEnabled = env_.strictModeEnabled;
    ValidateStores stores;
    stores.layout = env_.layout;
    stores.includedParents = env_.includedParents;
    stores.textForPath = [this, &dependencies](const std::filesystem::path& p) {
      auto text = textOf(p);
      dependencies.emplace_back(p.string(), text ? textHash(*text) : 0);
      return text;
    };
    Snapshot snapshot{
      .uri = node.uri,
      .version = 0,
      .languageId = "vertexadt",
      .text = node.text,
    };
    auto prep = buildDocumentPrep(std::move(snapshot), settings, stores, env_.pollCancel, &injection);
    const auto lock = std::lock_guard(resolver_.cacheMutex_);
    auto& entries = resolver_.preps_[node.norm];
    entries.insert(entries.begin(), CrossCallResolver::PrepEntry{
      .textHash = hash,
      .injectionSignature = signature,
      .strict = env_.strictModeEnabled,
      .dependencies = std::move(dependencies),
      .prep = prep,
    });
    // The parameters-only and the full prep of one file.
    if (entries.size() > 2)
    {
      entries.resize(2);
    }
    return prep;
  }

  auto targetsOf(const Node& node) -> std::vector<Target>
  {
    std::vector<Target> targets{Target{.norm = node.norm, .key = keyOf(node.path)}};
    if (!env_.layout)
    {
      return targets;
    }
    for (const auto& raw : node.facts->includedFrom)
    {
      auto parent = env_.layout->findIncludedFile(raw);
      if (!parent)
      {
        parent = resolvePath(stripPathQuotes(raw));
      }
      if (parent)
      {
        targets.push_back(Target{.norm = normalizeFsPath(parent->string()), .key = keyOf(*parent)});
      }
    }
    return targets;
  }

  // Contributions of `caller`'s sites aimed at `target` to `node`'s reads.
  void collectContributions(
    const Node& node,
    const Node& caller,
    const DocumentPrep& callerPrep,
    const Target& target,
    std::vector<std::vector<CrossCallLocation>>& perRead)
  {
    for (const auto& site : caller.facts->calls)
    {
      if (site.ignoreType)
      {
        continue;
      }
      const auto* resolved = resolveSite(site.calleePath);
      if (!resolved || resolved->norm != target.norm)
      {
        continue;
      }
      for (std::size_t r = 0; r < node.facts->params.size(); ++r)
      {
        const auto& read = node.facts->params[r];
        if (!sectionMatches(read.section, site.targetSection))
        {
          continue;
        }
        const auto argIndex = static_cast<std::size_t>(read.index - 1);
        if (argIndex >= site.args.size())
        {
          continue;
        }
        const auto& arg = site.args[argIndex];
        auto type = crossCallArgumentType(callerPrep, arg.text, site.line);
        if (type.empty())
        {
          continue;
        }
        perRead[r].push_back(
          locationOn(caller.uri, caller.lines, site.line, arg.start, arg.end, std::move(type)));
      }
    }
  }

  static void buildParams(Node& node, std::vector<std::vector<CrossCallLocation>> perRead)
  {
    node.report->params.clear();
    for (std::size_t r = 0; r < perRead.size(); ++r)
    {
      if (perRead[r].empty())
      {
        continue;
      }
      sortLocations(perRead[r]);
      const auto [type, conflict] = agree(perRead[r]);
      node.report->params.push_back(CrossCallParam{
        .read = r,
        .type = type,
        .conflict = conflict,
        .contributions = std::move(perRead[r]),
      });
    }
  }

  static auto injectionFor(const Node& node, bool withReceivers) -> CrossCallInjection
  {
    CrossCallInjection injection;
    for (const auto& param : node.report->params)
    {
      if (param.conflict || param.type.empty())
      {
        continue;
      }
      const auto& read = node.facts->params[param.read];
      injection.lineWrites[read.line].push_back({read.name, param.type});
      injection.overrides[read.section + '\n' + read.name] = param.type;
    }
    if (!withReceivers)
    {
      return injection;
    }
    for (const auto& receiver : node.report->receivers)
    {
      if (receiver.type.empty())
      {
        continue;
      }
      const auto& fact = node.facts->receivers[receiver.receiver];
      injection.lineWrites[fact.line].push_back({fact.name, receiver.type});
      injection.overrides[fact.section + '\n' + fact.name] = receiver.type;
    }
    return injection;
  }

  // Parameters: every caller of this file, or of an `##included_from` parent.
  void ensureParams(Node& node, int depth)
  {
    if (node.params != State::None)
    {
      return;
    }
    node.params = State::InProgress;
    const auto& facts = *node.facts;
    std::vector<std::vector<CrossCallLocation>> perRead(facts.params.size());
    std::vector<Target> selfTargets;
    if (!facts.params.empty())
    {
      for (const auto& target : targetsOf(node))
      {
        if (target.key.empty())
        {
          continue;
        }
        const auto it = callersByKey_.find(target.key);
        if (it == callersByKey_.end())
        {
          continue;
        }
        for (const auto& entry : it->second)
        {
          poll();
          if (normalizeFsPath(entry->identity.absPath.string()) == node.norm)
          {
            selfTargets.push_back(target);
            continue;
          }
          const auto* caller = ensureUp(entry->identity.absPath, depth + 1);
          if (!caller || !caller->upPrep)
          {
            continue;
          }
          collectContributions(node, *caller, *caller->upPrep, target, perRead);
        }
      }
    }
    buildParams(node, perRead);

    // Same-file calls read this file's own types: one provisional pass.
    if (!selfTargets.empty())
    {
      const auto provisional = prepFor(node, injectionFor(node, false));
      for (const auto& target : selfTargets)
      {
        collectContributions(node, node, *provisional, target, perRead);
      }
      buildParams(node, std::move(perRead));
    }
    node.params = State::Done;
  }

  // Return-field receivers: the callee's agreed field type.
  void ensureReceivers(Node& node, int depth)
  {
    const auto& facts = *node.facts;
    node.report->receivers.clear();
    for (std::size_t r = 0; r < facts.receivers.size(); ++r)
    {
      const auto& fact = facts.receivers[r];
      const auto& site = facts.calls[fact.callSite];
      const auto* resolved = resolveSite(site.calleePath);
      if (!resolved || resolved->norm == node.norm)
      {
        continue;
      }
      const auto* callee = ensureFull(resolved->path, depth + 1);
      if (!callee || !callee->prep)
      {
        continue;
      }
      CrossCallReceiver receiver{.receiver = r};
      if (const auto found = callee->returnTypes.find(fact.key); found != callee->returnTypes.end())
      {
        receiver.type = found->second;
      }
      for (const auto& field : crossCallReturnFieldTypes(*callee->prep, *callee->facts, callee->uri))
      {
        if (field.key == fact.key)
        {
          receiver.sources = field.lines;
        }
      }
      node.report->receivers.push_back(std::move(receiver));
    }
  }

  // Caller-side expectations: callee `[type]` comments on `get($CALLPARAMn)`.
  void buildExpectations(Node& node)
  {
    node.report->argExpectations.clear();
    const auto& facts = *node.facts;
    struct Callee
    {
      std::shared_ptr<const CallFacts> facts;
      std::string text;
    };
    std::unordered_map<std::string, Callee> callees;
    for (std::size_t s = 0; s < facts.calls.size(); ++s)
    {
      const auto& site = facts.calls[s];
      if (site.args.empty())
      {
        continue;
      }
      const auto* resolvedSite = resolveSite(site.calleePath);
      if (!resolvedSite)
      {
        continue;
      }
      const auto& norm = resolvedSite->norm;
      const auto resolved = &resolvedSite->path;
      auto found = callees.find(norm);
      if (found == callees.end())
      {
        Callee callee;
        if (norm == node.norm)
        {
          callee = Callee{.facts = node.facts, .text = node.text};
        }
        else if (auto text = textOf(*resolved))
        {
          callee = Callee{.facts = factsFor(norm, *text), .text = std::move(*text)};
        }
        found = callees.emplace(norm, std::move(callee)).first;
      }
      const auto& callee = found->second;
      if (!callee.facts)
      {
        continue;
      }
      const auto calleeLines = splitLines(callee.text);
      const auto calleeUri = uriFromPath(*resolved);
      for (const auto& read : callee.facts->params)
      {
        if (read.commentType.empty() || !sectionMatches(read.section, site.targetSection))
        {
          continue;
        }
        const auto argIndex = static_cast<std::size_t>(read.index - 1);
        if (argIndex >= site.args.size())
        {
          continue;
        }
        node.report->argExpectations.push_back(CrossCallArgExpectation{
          .line = site.line,
          .site = s,
          .arg = argIndex,
          .expectedType = read.commentType,
          .callee = locationOn(
            calleeUri, calleeLines, read.line, read.commentStart, read.commentEnd, read.commentType),
        });
      }
    }
  }

  CrossCallResolver& resolver_;
  const CrossCallEnvironment& env_;
  std::unordered_map<std::string, std::vector<std::shared_ptr<const IndexEntry>>> callersByKey_;
  std::unordered_map<std::string, std::unique_ptr<Node>> nodes_;
  std::unordered_map<std::string, std::optional<Resolved>> resolved_;
};

auto CrossCallResolver::analyze(
  const std::filesystem::path& path,
  const std::string& text,
  const CrossCallEnvironment& env) -> CrossCallAnalysis
{
  // No call site, `$CALLPARAM` read or return value: nothing to resolve.
  if (!containsCallToken(text) && text.find("exit") == std::string::npos)
  {
    CrossCallAnalysis empty;
    auto report = std::make_shared<CrossCallReport>();
    report->facts = std::make_shared<const CallFacts>();
    empty.report = std::move(report);
    return empty;
  }
  const auto lock = std::lock_guard(walkMutex_);
  CrossCallWalk walk(*this, env);
  return walk.analyzeRoot(path, text);
}

void CrossCallResolver::clear()
{
  const auto lock = std::lock_guard(cacheMutex_);
  preps_.clear();
  facts_.clear();
}

} // namespace vertexadt
