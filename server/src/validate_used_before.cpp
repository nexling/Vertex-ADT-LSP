#include "validate.h"

#include "catalog/catalog.h"
#include "utf16_text.h"

#include <cctype>
#include <initializer_list>
#include <regex>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

bool isAsciiWordChar(char ch)
{
  const auto c = static_cast<unsigned char>(ch);
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

bool isAsciiLetter(char ch)
{
  const auto c = static_cast<unsigned char>(ch);
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool containsAscii(std::string_view text, std::string_view needle)
{
  return text.find(needle) != std::string_view::npos;
}

bool startsWithCallAt(std::string_view text, std::size_t index)
{
  if (index + 4 > text.size())
  {
    return false;
  }
  return (text[index] == 'c' || text[index] == 'C')
      && (text[index + 1] == 'a' || text[index + 1] == 'A')
      && (text[index + 2] == 'l' || text[index + 2] == 'L')
      && (text[index + 3] == 'l' || text[index + 3] == 'L');
}

// The include-neighborhood names visible on `lineIndex`: its section's scope
// (docs/adr/0015-native-inferred-included-from.md).
const ScopedNames& neighborhoodScope(const ValidateContext& ctx, int lineIndex)
{
  return scopeFor(ctx.prep->neighborhood, sectionNameForLine(lineIndex, ctx.prep->sections));
}

bool inVariablesSet(const ValidateContext& ctx, int lineIndex, std::string_view name)
{
  const std::string key(name);
  return ctx.prep->live.variablesSet.contains(key)
    || neighborhoodScope(ctx, lineIndex).variables.contains(key);
}

bool inStringVariablesSet(const ValidateContext& ctx, int lineIndex, std::string_view name)
{
  const std::string key(name);
  return ctx.prep->live.stringVariablesSet.contains(key)
    || neighborhoodScope(ctx, lineIndex).stringVariables.contains(key);
}

bool hasLineContinuation(std::string_view stripped)
{
  auto end = stripped.size();
  while (end > 0 && (stripped[end - 1] == ' ' || stripped[end - 1] == '\t' || stripped[end - 1] == '\r'))
  {
    --end;
  }
  return end > 0 && stripped[end - 1] == '\\';
}

bool isDbcreateOpen(std::string_view stripped)
{
  static const std::regex re{R"(\bdbcreate\s*\()", std::regex::icase};
  const std::string text(stripped);
  return std::regex_search(text, re);
}

// Per-line dbcreate start/continuation flags for the whole pass, computed
// once. Same semantics as the former per-line full-file rescan.
const std::vector<DbcreateLineFlags>& dbcreateFlagsFor(ValidateContext& ctx)
{
  auto& flags = ctx.usedBeforeDbcreateFlags;
  if (flags.size() == ctx.prep->lines.size())
  {
    return flags;
  }
  const auto n = ctx.prep->lines.size();
  flags.assign(n, DbcreateLineFlags{});
  std::vector<char> continues(n, 0);
  std::vector<char> opens(n, 0);
  for (std::size_t j = 0; j < n; ++j)
  {
    const auto current = stripValidationComment(ctx.prep->lines[j]);
    continues[j] = hasLineContinuation(current) ? 1 : 0;
    opens[j] = isDbcreateOpen(current) ? 1 : 0;
  }
  for (std::size_t j = 0; j < n; ++j)
  {
    if (!opens[j])
    {
      continue;
    }
    if (continues[j])
    {
      flags[j].isStart = true;
    }
    auto continued = continues[j] != 0;
    auto next = j + 1;
    while (continued && next < n)
    {
      flags[next].isContinuation = true;
      continued = continues[next] != 0;
      ++next;
    }
  }
  return flags;
}

std::size_t findMatchingParenIndex(std::string_view text, std::size_t openParenIndex)
{
  if (openParenIndex >= text.size() || text[openParenIndex] != '(')
  {
    return std::string_view::npos;
  }
  int parenCount = 1;
  char activeQuote = 0;
  for (std::size_t i = openParenIndex + 1; i < text.size(); ++i)
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
    if (ch == '"')
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
        return i;
      }
    }
  }
  return std::string_view::npos;
}

std::optional<std::pair<std::string, std::string>> matchCommandAssignment(std::string_view text)
{
  static const std::regex re{
    R"(^([a-zA-Z][a-zA-Z0-9_]*\$?(?:\([^)]*\))?)\s*=\s*(.+)$)"};
  std::smatch match;
  const std::string owned(text);
  if (!std::regex_match(owned, match, re))
  {
    return std::nullopt;
  }
  return std::pair<std::string, std::string>{match[1].str(), match[2].str()};
}

std::vector<std::string> parseParameters(std::string_view paramString)
{
  if (trim(paramString).empty())
  {
    return {};
  }
  static const std::regex concatRe{
    R"("[^"]*"(?:\s*&\s*[a-zA-Z][a-zA-Z0-9_]*\$?)+|[a-zA-Z][a-zA-Z0-9_]*\$?(?:\s*&\s*"[^"]*")+|"[^"]*"(?:\s*&\s*"[^"]*")+)"};

  const std::string original(paramString);
  std::vector<std::string> concatenations;
  std::string processed;
  processed.reserve(original.size());
  std::size_t last = 0;
  for (std::sregex_iterator it(original.begin(), original.end(), concatRe), end; it != end; ++it)
  {
    processed.append(original, last, static_cast<std::size_t>(it->position()) - last);
    concatenations.push_back(it->str());
    processed += "__CONCAT_";
    processed += std::to_string(concatenations.size() - 1);
    processed += "__";
    last = static_cast<std::size_t>(it->position() + it->length());
  }
  processed.append(original, last, std::string::npos);

  std::vector<std::string> params;
  std::string current;
  bool inQuotes = false;
  char quoteChar = 0;
  int parenDepth = 0;
  int bracketDepth = 0;
  for (const char ch : processed)
  {
    if (!inQuotes && ch == '"')
    {
      inQuotes = true;
      quoteChar = ch;
      current += ch;
    }
    else if (inQuotes && ch == quoteChar)
    {
      inQuotes = false;
      current += ch;
    }
    else if (inQuotes)
    {
      current += ch;
    }
    else if (ch == '(')
    {
      ++parenDepth;
      current += ch;
    }
    else if (ch == ')')
    {
      --parenDepth;
      current += ch;
    }
    else if (ch == '[')
    {
      ++bracketDepth;
      current += ch;
    }
    else if (ch == ']')
    {
      --bracketDepth;
      current += ch;
    }
    else if (ch == ',' && parenDepth == 0 && bracketDepth == 0)
    {
      params.push_back(std::string(trim(current)));
      current.clear();
    }
    else
    {
      current += ch;
    }
  }
  params.push_back(std::string(trim(current)));

  static const std::regex placeholderRe{R"(__CONCAT_(\d+)__)"};
  for (auto& param : params)
  {
    std::string restored;
    std::size_t pos = 0;
    for (std::sregex_iterator it(param.begin(), param.end(), placeholderRe), end; it != end; ++it)
    {
      restored.append(param, pos, static_cast<std::size_t>(it->position()) - pos);
      const auto idx = static_cast<std::size_t>(std::stoul((*it)[1].str()));
      if (idx < concatenations.size())
      {
        restored += concatenations[idx];
      }
      pos = static_cast<std::size_t>(it->position() + it->length());
    }
    restored.append(param, pos, std::string::npos);
    param = std::move(restored);
  }
  return params;
}

struct ParsedCall
{
  std::string functionName;
  std::vector<std::string> insideParams;
};

std::optional<ParsedCall> parseCallSegment(std::string_view command)
{
  auto owned = std::string(trim(command));
  auto commandPart = owned;
  if (const auto assignment = matchCommandAssignment(owned))
  {
    commandPart = assignment->second;
  }
  static const std::regex funcRe{R"(^(\w+)\s*\()"};
  std::smatch funcMatch;
  if (!std::regex_search(commandPart, funcMatch, funcRe)
      || funcMatch.position() != 0)
  {
    commandPart = owned;
    if (!std::regex_search(commandPart, funcMatch, funcRe) || funcMatch.position() != 0)
    {
      return std::nullopt;
    }
  }
  const auto startPos = static_cast<std::size_t>(funcMatch.length() - 1);
  const auto insideParensEnd = findMatchingParenIndex(commandPart, startPos);
  if (insideParensEnd == std::string_view::npos)
  {
    return std::nullopt;
  }
  const auto insideParens = commandPart.substr(startPos + 1, insideParensEnd - startPos - 1);
  return ParsedCall{
    .functionName = funcMatch[1].str(),
    .insideParams = parseParameters(insideParens),
  };
}

std::vector<std::string> findParenthesizedCallSegments(std::string_view line)
{
  std::vector<std::string> segments;
  for (std::size_t i = 0; i < line.size();)
  {
    if (!startsWithCallAt(line, i) || (i > 0 && isAsciiWordChar(line[i - 1])))
    {
      ++i;
      continue;
    }
    auto j = i + 4;
    while (j < line.size()
           && (line[j] == ' ' || line[j] == '\t' || line[j] == '\r' || line[j] == '\n'
               || line[j] == '\v' || line[j] == '\f'))
    {
      ++j;
    }
    if (j >= line.size() || line[j] != '(')
    {
      ++i;
      continue;
    }
    if (isInsideQuotes(line, i))
    {
      ++i;
      continue;
    }
    const auto close = findMatchingParenIndex(line, j);
    if (close == std::string_view::npos)
    {
      ++i;
      continue;
    }
    auto startIndex = i;
    const auto prefix = line.substr(0, i);
    static const std::regex assignRe{R"(([A-Za-z][A-Za-z0-9_]*\$?)\s*=\s*$)"};
    std::smatch assignMatch;
    const std::string prefixOwned(prefix);
    if (std::regex_search(prefixOwned, assignMatch, assignRe)
        && static_cast<std::size_t>(assignMatch.position() + assignMatch.length()) == prefixOwned.size())
    {
      startIndex = prefixOwned.size() - static_cast<std::size_t>(assignMatch.length());
    }
    segments.push_back(std::string(trim(line.substr(startIndex, close + 1 - startIndex))));
    i = j + 1;
  }
  return segments;
}

std::string stripStringIndexSuffix(std::string expr)
{
  static const std::regex re{R"(\[[^\]]+\]$)"};
  return std::regex_replace(expr, re, "");
}

bool isValidStringVariable(std::string expr)
{
  if (expr.empty())
  {
    return false;
  }
  expr = stripStringIndexSuffix(std::move(expr));
  if (expr.empty() || expr.size() > 16)
  {
    return false;
  }
  static const std::regex variableRe{R"([a-zA-Z][a-zA-Z0-9_]*\$?(?:\([A-Za-z0-9]*\))?)"};
  static const std::regex tickerRe{R"(^([a-zA-Z][a-zA-Z0-9_]*)\((.*)\)$)"};
  static const std::regex stringVarRe{R"(^[a-zA-Z][a-zA-Z0-9_]*\$)"};
  if (std::regex_search(expr, variableRe) && expr.find('$') != std::string::npos)
  {
    return true;
  }
  std::smatch tickerMatch;
  if (std::regex_match(expr, tickerMatch, tickerRe))
  {
    const auto baseVar = tickerMatch[1].str();
    if (baseVar.empty() || baseVar.back() != '$')
    {
      return false;
    }
    if (!std::regex_match(baseVar, stringVarRe))
    {
      return false;
    }
    return isValidStringVariable(tickerMatch[2].str());
  }
  return std::regex_match(expr, stringVarRe);
}

bool isStaticString(std::string_view part)
{
  static const std::regex doubleQuote{R"("[^"]*")"};
  static const std::regex singleQuote{R"('[^']*')"};
  const std::string owned(part);
  return std::regex_search(owned, doubleQuote) || std::regex_search(owned, singleQuote);
}

bool isValidCallPathParameter(std::string_view expr)
{
  if (expr.empty())
  {
    return false;
  }
  static const std::regex delim{R"(\s*&\s*)"};
  const std::string owned(expr);
  std::vector<std::string> parts;
  std::size_t last = 0;
  for (std::sregex_iterator it(owned.begin(), owned.end(), delim), end; it != end; ++it)
  {
    parts.push_back(owned.substr(last, static_cast<std::size_t>(it->position()) - last));
    parts.push_back(it->str());
    last = static_cast<std::size_t>(it->position() + it->length());
  }
  parts.push_back(owned.substr(last));

  for (std::size_t i = 0; i < parts.size(); ++i)
  {
    const auto part = std::string(trim(parts[i]));
    if (part.empty())
    {
      continue;
    }
    if (i % 2 == 0)
    {
      if (!isStaticString(part) && !isValidStringVariable(part))
      {
        return false;
      }
    }
    else
    {
      static const std::regex concatOp{R"(\s*&\s*)"};
      if (!std::regex_match(part, concatOp))
      {
        return false;
      }
    }
  }
  return true;
}

bool isKnownFunctionToken(std::string_view token)
{
  static const auto keys = [] {
    std::unordered_set<std::string> out;
    out.emplace("call");
    for (const auto& entry : catalog::commandCatalog())
    {
      out.emplace(std::string(entry.name));
    }
    for (const auto& row : catalog::returnTypeMap())
    {
      out.emplace(std::string(row.name));
    }
    for (const auto& row : catalog::inputParamTypeMap())
    {
      out.emplace(std::string(row.name));
    }
    for (const auto& row : catalog::paramTypeMap())
    {
      out.emplace(std::string(row.command));
    }
    for (const auto& row : catalog::multiParamTypeMap())
    {
      out.emplace(std::string(row.command));
    }
    return out;
  }();
  return keys.contains(toLowerAscii(token));
}

std::vector<std::string> extractVariableReferencesFromExpression(std::string_view expression)
{
  if (expression.empty())
  {
    return {};
  }
  std::string sanitized;
  sanitized.reserve(expression.size());
  char activeQuote = 0;
  for (const char ch : expression)
  {
    if (activeQuote)
    {
      if (ch == activeQuote)
      {
        activeQuote = 0;
      }
      sanitized += ' ';
      continue;
    }
    if (ch == '"')
    {
      activeQuote = ch;
      sanitized += ' ';
      continue;
    }
    sanitized += ch;
  }

  std::vector<std::string> references;
  std::unordered_set<std::string> seen;
  static const std::regex variableRe{R"([A-Za-z][A-Za-z0-9_]*\$?)"};
  for (std::sregex_iterator it(sanitized.begin(), sanitized.end(), variableRe), end; it != end; ++it)
  {
    const auto variableName = it->str();
    const auto after = static_cast<std::size_t>(it->position() + it->length());
    auto k = after;
    while (k < sanitized.size()
           && (sanitized[k] == ' ' || sanitized[k] == '\t' || sanitized[k] == '\r' || sanitized[k] == '\n'))
    {
      ++k;
    }
    if (k < sanitized.size() && sanitized[k] == '(' && isKnownFunctionToken(variableName))
    {
      continue;
    }
    if (seen.insert(variableName).second)
    {
      references.push_back(variableName);
    }
  }
  return references;
}

std::vector<std::string> getCallCommandVariableReferences(std::string_view line)
{
  std::vector<std::string> references;
  std::unordered_set<std::string> seen;
  for (const auto& segment : findParenthesizedCallSegments(line))
  {
    const auto parsed = parseCallSegment(segment);
    if (!parsed || toLowerAscii(parsed->functionName) != "call")
    {
      continue;
    }
    for (std::size_t index = 0; index < parsed->insideParams.size(); ++index)
    {
      const auto& param = parsed->insideParams[index];
      if (index == 0 && !isValidCallPathParameter(param))
      {
        continue;
      }
      for (const auto& variableName : extractVariableReferencesFromExpression(param))
      {
        if (seen.insert(variableName).second)
        {
          references.push_back(variableName);
        }
      }
    }
  }
  return references;
}

bool quoteLookaheadOk(std::string_view codeLine, std::size_t from)
{
  int quotes = 0;
  std::size_t lastQuote = std::string_view::npos;
  for (auto i = from; i < codeLine.size(); ++i)
  {
    if (codeLine[i] == '"')
    {
      ++quotes;
      lastQuote = i;
    }
  }
  if (quotes % 2 != 0)
  {
    return false;
  }
  const auto leftover = lastQuote == std::string_view::npos
    ? codeLine.substr(from)
    : codeLine.substr(lastQuote + 1);
  return leftover.find('\'') == std::string_view::npos;
}

struct UsageMatch
{
  std::string name;
  std::size_t begin = 0;
};

std::vector<UsageMatch> genericUsageMatches(std::string_view codeLine)
{
  std::vector<UsageMatch> matches;
  for (std::size_t i = 0; i < codeLine.size();)
  {
    if (!isAsciiLetter(codeLine[i]) || (i > 0 && isAsciiWordChar(codeLine[i - 1])))
    {
      ++i;
      continue;
    }
    auto j = i + 1;
    while (j < codeLine.size() && isAsciiWordChar(codeLine[j]))
    {
      ++j;
    }
    if (codeLine.find('$', j) == std::string_view::npos && quoteLookaheadOk(codeLine, j))
    {
      matches.push_back(UsageMatch{.name = std::string(codeLine.substr(i, j - i)), .begin = i});
    }
    i = j;
  }
  return matches;
}

lsp::Diagnostic reportProblem123(
  const ValidateContext& ctx,
  const PerLineState& line,
  const std::string& inner)
{
  const auto& lineText = line.stripped;
  const auto lineNumber = static_cast<lsp::Uint>(line.index);
  const auto endChar = static_cast<lsp::Uint>(utf16Length(lineText));
  const lsp::Range range{
    .start = {.line = lineNumber, .character = 1},
    .end = {.line = lineNumber, .character = endChar},
  };
  lsp::Diagnostic diagnostic;
  diagnostic.range = range;
  diagnostic.message = lineText + " has an error: " + inner;
  diagnostic.severity = lsp::DiagnosticSeverity::Warning;
  diagnostic.source = std::string("ADT-Syntax");
  diagnostic.relatedInformation = std::vector<lsp::DiagnosticRelatedInformation>{
    lsp::DiagnosticRelatedInformation{
      .location = {.uri = ctx.prep->snapshot.uri, .range = range},
      .message = inner,
    },
  };
  return diagnostic;
}

void emitUndeclared(ValidateContext& ctx, const PerLineState& line, const std::string& variableName)
{
  ctx.append(
    reportProblem123(ctx, line, "Variable " + variableName + " used before declaration"),
    true);
}

// --- Used before assignment -------------------------------------------------
// A name that is assigned somewhere in this document must also be assigned
// before the use: on an earlier line, inside the same #SECTION# (blocks with
// the same name count as one section) or outside every section. On the
// first-assignment line only the assignment target itself counts. Names from
// the include neighborhood (written or inferred `##included_from`) visible in
// the use's section are assigned before line 1.
// Numeric uses look only at numeric sites and `X$` uses only at string sites.
// A name with no site of its kind (only compared, called, or assigned as the
// other kind) is left alone.

// Innermost `#NAME#` block strictly containing `line`, or null.
const NamedSection* innermostSection(const std::vector<NamedSection>& sections, int line)
{
  const NamedSection* best = nullptr;
  for (const auto& section : sections)
  {
    if (section.openLine < line && line < section.closeLine
        && (!best || section.closeLine - section.openLine < best->closeLine - best->openLine))
    {
      best = &section;
    }
  }
  return best;
}

bool lineInSectionNamed(const std::vector<NamedSection>& sections, std::string_view name, int line)
{
  for (const auto& section : sections)
  {
    if (section.name == name && section.openLine < line && line < section.closeLine)
    {
      return true;
    }
  }
  return false;
}

bool sectionAllows(const std::vector<NamedSection>& sections, int siteLine, int useLine)
{
  const auto* section = innermostSection(sections, siteLine);
  return !section || lineInSectionNamed(sections, section->name, useLine);
}

// On the use's own line a site counts when it is the use itself (the
// assignment target) or sits in an earlier `;` statement (`X= 6; Y= X`).
// `useBegin` npos: the use is never the target and has no position (call
// arguments, dbcreate ids), so only a synthetic same-line site counts.
bool siteReaches(
  const std::vector<NamedSection>& sections,
  const AssignmentSite& site,
  int useLine,
  std::size_t useBegin,
  std::string_view lineText)
{
  if (site.line > useLine)
  {
    return false;
  }
  // A synthetic name (begin npos) is an output of its line's own command.
  if (site.line == useLine && site.begin != std::string::npos && site.begin != useBegin)
  {
    const auto earlierStatement = useBegin != std::string::npos && site.begin < useBegin
      && useBegin <= lineText.size()
      && lineText.substr(site.begin, useBegin - site.begin).find(';') != std::string_view::npos;
    if (!earlierStatement)
    {
      return false;
    }
  }
  return sectionAllows(sections, site.line, useLine);
}

struct SiteKey
{
  const std::unordered_map<std::string, std::vector<AssignmentSite>>* sites;
  std::string name;
};

void emitUsedBeforeAssignment(
  ValidateContext& ctx,
  const PerLineState& line,
  const std::string& variableName,
  std::size_t useBegin,
  std::initializer_list<SiteKey> keys)
{
  const auto& neighborhood = neighborhoodScope(ctx, line.index);
  for (const auto& key : keys)
  {
    if (neighborhood.variables.contains(key.name) || neighborhood.stringVariables.contains(key.name))
    {
      return;
    }
  }
  const auto& sections = ctx.prep->sections;
  const AssignmentSite* later = nullptr;
  const AssignmentSite* elsewhere = nullptr;
  for (const auto& key : keys)
  {
    const auto it = key.sites->find(key.name);
    if (it == key.sites->end())
    {
      continue;
    }
    for (const auto& site : it->second)
    {
      if (siteReaches(sections, site, line.index, useBegin, line.stripped))
      {
        return;
      }
      if (site.line >= line.index && sectionAllows(sections, site.line, line.index))
      {
        if (!later || site.line < later->line)
        {
          later = &site;
        }
      }
      else if (!elsewhere || site.line < elsewhere->line)
      {
        elsewhere = &site;
      }
    }
  }
  const auto* site = later ? later : elsewhere;
  if (!site)
  {
    return;
  }
  const auto lineNumber = std::to_string(site->line + 1);
  std::string inner = "Variable " + variableName + " used before assignment";
  std::string related = variableName + " is first assigned here";
  if (later)
  {
    inner += " (first assigned on line " + lineNumber + ")";
  }
  else
  {
    const auto* section = innermostSection(sections, site->line);
    const auto sectionName = section ? section->name : std::string{};
    inner += " (only assigned in section " + sectionName + ", line " + lineNumber + ")";
    related = variableName + " is assigned here, in section " + sectionName;
  }
  auto diagnostic = reportProblem123(ctx, line, inner);
  const auto siteText = stripValidationComment(ctx.prep->lines[static_cast<std::size_t>(site->line)]);
  const auto siteLine = static_cast<lsp::Uint>(site->line);
  diagnostic.relatedInformation = std::vector<lsp::DiagnosticRelatedInformation>{
    lsp::DiagnosticRelatedInformation{
      .location = {
        .uri = ctx.prep->snapshot.uri,
        .range = {
          .start = {.line = siteLine, .character = 0},
          .end = {.line = siteLine, .character = static_cast<lsp::Uint>(utf16Length(siteText))},
        },
      },
      .message = related,
    },
  };
  ctx.append(std::move(diagnostic), true);
}

} // namespace

void familyUsedBeforeDeclaration(ValidateContext& ctx, PerLineState& line)
{
  if (ctx.atCap())
  {
    return;
  }

  const auto& codeLine = line.codeLine;
  const auto trimmedLine = line.trimmed;
  const auto dbcreate = dbcreateFlagsFor(ctx)[static_cast<std::size_t>(line.index)];
  const auto hasCall = containsAscii(codeLine, "call");
  const auto hasOpenParen = containsAscii(codeLine, "(");
  const auto hasCloseParen = containsAscii(codeLine, ")");
  const auto hasGet = containsAscii(codeLine, "get(");
  const auto hasDbalter = containsAscii(codeLine, "dbalter");
  const auto hasMessagebox = containsAscii(codeLine, "messagebox");
  const auto hasCharset = containsAscii(codeLine, "charset");
  const auto hasDbcreate =
    containsAscii(codeLine, "dbcreate") || dbcreate.isStart || dbcreate.isContinuation;
  const auto hasGetActualPcend = containsAscii(codeLine, "get_actual_pcend");
  const auto startsWithInclude = trimmedLine.starts_with("# include");
  const auto startsWithDeclarationCheckStart = trimmedLine.starts_with("@declarationnumbercheckstart");
  const auto startsWithDeclarationCheckEnd = trimmedLine.starts_with("@declarationnumbercheckend");
  const auto startsWithFunction = trimmedLine.starts_with("function");
  const auto shouldSkipGeneric = hasGet || hasDbalter || hasMessagebox || hasCharset || hasDbcreate
    || startsWithInclude || hasCall || startsWithDeclarationCheckStart
    || startsWithDeclarationCheckEnd || startsWithFunction;

  if (hasCall && hasOpenParen && hasCloseParen)
  {
    for (const auto& variableName : getCallCommandVariableReferences(codeLine))
    {
      if (catalog::isKeywordLabel(variableName))
      {
        continue;
      }
      const auto stringVariant = !variableName.empty() && variableName.back() == '$'
        ? variableName.substr(0, variableName.size() - 1)
        : variableName;
      if (!inVariablesSet(ctx, line.index, variableName) && !inStringVariablesSet(ctx, line.index, stringVariant))
      {
        emitUndeclared(ctx, line, variableName);
      }
      else if (ctx.settings.warnUsedBeforeAssignment)
      {
        emitUsedBeforeAssignment(ctx, line, variableName, std::string::npos, {
          variableName.ends_with('$')
            ? SiteKey{&ctx.prep->live.stringAssignmentSites, stringVariant}
            : SiteKey{&ctx.prep->live.assignmentSites, variableName},
        });
      }
    }
  }

  if (hasDbcreate && !dbcreate.isContinuation)
  {
    static const std::regex dbcreateOutRe{
      R"(\bdbcreate\s*\(\s*([A-Za-z][A-Za-z0-9_]*)\s*\))",
      std::regex::icase};
    std::smatch match;
    if (std::regex_search(codeLine, match, dbcreateOutRe))
    {
      const auto variableName = match[1].str();
      if (catalog::isKeywordLabel(variableName))
      {
        // Keyword labels are never variables.
      }
      else if (!inVariablesSet(ctx, line.index, variableName) && !inStringVariablesSet(ctx, line.index, variableName))
      {
        emitUndeclared(ctx, line, variableName);
      }
      else if (ctx.settings.warnUsedBeforeAssignment)
      {
        emitUsedBeforeAssignment(ctx, line, variableName, std::string::npos, {
          SiteKey{&ctx.prep->live.assignmentSites, variableName},
        });
      }
    }
  }

  if (shouldSkipGeneric)
  {
    return;
  }

  const auto& stripped = line.stripped;
  const auto hasDollar = containsAscii(stripped, "$");
  const auto hasEquals = containsAscii(stripped, "=");
  const auto firstDollarIndex = hasDollar ? stripped.find('$') : std::string::npos;
  const auto firstEqualsIndex = hasEquals ? stripped.find('=') : std::string::npos;
  const auto hasHash = containsAscii(stripped, "#");
  const auto firstHashIndex = hasHash ? stripped.find('#') : std::string::npos;
  const auto lastHashIndex = hasHash ? stripped.rfind('#') : std::string::npos;

  for (const auto& usage : genericUsageMatches(codeLine))
  {
    const auto& variableName = usage.name;
    if (variableName.find("RETURN") != std::string::npos)
    {
      continue;
    }
    if (hasGetActualPcend && (variableName == "END" || variableName == "START"))
    {
      continue;
    }
    if (hasDollar && hasEquals && firstDollarIndex < firstEqualsIndex && variableName == "N")
    {
      continue;
    }
    const auto variableIndex = stripped.find(variableName);
    if (hasHash && firstHashIndex < variableIndex && variableIndex < lastHashIndex)
    {
      continue;
    }
    if (variableName == "SHEETORIG" || variableName == "AREAORIG")
    {
      continue;
    }

    if (variableName.size() > 16 && inVariablesSet(ctx, line.index, variableName))
    {
      ctx.append(
        reportProblem123(
          ctx,
          line,
          "Keyword " + variableName + " exceeds maximum length of 16 characters"),
        true);
    }
    if (catalog::isKeywordLabel(variableName))
    {
      continue;
    }
    if (!inVariablesSet(ctx, line.index, variableName) && !inStringVariablesSet(ctx, line.index, variableName))
    {
      emitUndeclared(ctx, line, variableName);
    }
    else if (ctx.settings.warnUsedBeforeAssignment)
    {
      emitUsedBeforeAssignment(ctx, line, variableName, usage.begin, {
        SiteKey{&ctx.prep->live.assignmentSites, variableName},
      });
    }
  }
}

} // namespace vertexadt
