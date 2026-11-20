#include "extracts.h"

#include "catalog/catalog.h"
#include "regex_classes.h"
#include "text_util.h"

#include <algorithm>
#include <array>
#include <optional>

namespace vertexadt {
namespace {

// Hand matchers for the four ECMAScript `std::regex` scans Node runs per
// line. Each reproduces the regex's matches (start positions, backtracking,
// resume-after-match) exactly; classes are in regex_classes.h.
//   string assign: \b([A-Za-z][A-Za-z0-9_-]*)\$(?=\s*=)
//   assign:        \b([A-Za-z][A-Za-z0-9_-]*)\b(?:\([^)]*\))?(?!$)(?=\s*=)
//   set:           \bset\s+([A-Za-z][A-Za-z0-9_-]*)\b
//   call:          \b([A-Za-z][A-Za-z0-9_-]*)\s*\(

bool isNameChar(char ch)
{
  return isRegexWord(ch) || ch == '-';
}

// `\b[A-Za-z]` can start at `i`.
bool nameStartsAt(std::string_view line, std::size_t i)
{
  return isAsciiLetter(line[i]) && (i == 0 || !isRegexWord(line[i - 1]));
}

std::size_t nameRunEnd(std::string_view line, std::size_t i)
{
  while (i < line.size() && isNameChar(line[i]))
  {
    ++i;
  }
  return i;
}

// (?=\s*=) at `i`.
bool equalsAhead(std::string_view line, std::size_t i)
{
  i = skipRegexSpace(line, i);
  return i < line.size() && line[i] == '=';
}

struct NameMatch
{
  std::size_t nameBegin;
  std::size_t nameEnd;
  std::size_t matchEnd;
};

// Every match of `tryAt`, scanning like std::sregex_iterator: after a match,
// resume at its end; after a miss, try the next byte.
template <typename TryAt, typename OnMatch>
void forEachMatch(std::string_view line, TryAt tryAt, OnMatch onMatch)
{
  std::size_t i = 0;
  while (i < line.size())
  {
    if (const auto match = tryAt(line, i))
    {
      onMatch(*match);
      i = match->matchEnd;
    }
    else
    {
      ++i;
    }
  }
}

// A start inside a name run after `-` reaches the same run end as the run's
// own start, so it succeeds or fails the same way; checking every `\b`
// start therefore matches the regex.
std::optional<NameMatch> stringAssignAt(std::string_view line, std::size_t i)
{
  if (!nameStartsAt(line, i))
  {
    return std::nullopt;
  }
  const auto end = nameRunEnd(line, i);
  if (end >= line.size() || line[end] != '$' || !equalsAhead(line, end + 1))
  {
    return std::nullopt;
  }
  return NameMatch{i, end, end + 1};
}

// Backtracking to a shorter name always leaves a `-` next, which fails both
// `(` and `\s*=`, so only the maximal run can match, and only when it ends in
// a word character (`\b`). The optional group is tried first, then skipped;
// skipped leaves `(` next, which fails `\s*=`.
std::optional<NameMatch> assignAt(std::string_view line, std::size_t i)
{
  if (!nameStartsAt(line, i))
  {
    return std::nullopt;
  }
  const auto end = nameRunEnd(line, i);
  if (!isRegexWord(line[end - 1]))
  {
    return std::nullopt;
  }
  auto after = end;
  if (after < line.size() && line[after] == '(')
  {
    const auto close = line.find(')', after + 1);
    if (close == std::string_view::npos)
    {
      return std::nullopt;
    }
    after = close + 1;
  }
  if (after >= line.size() || !equalsAhead(line, after))
  {
    return std::nullopt;
  }
  return NameMatch{i, end, after};
}

// `\b` after the name backtracks to the run minus its trailing `-`s.
std::optional<NameMatch> setAt(std::string_view line, std::size_t i)
{
  if (line[i] != 's' || (i > 0 && isRegexWord(line[i - 1])) || line.substr(i, 3) != "set")
  {
    return std::nullopt;
  }
  const auto spaceBegin = i + 3;
  const auto nameBegin = skipRegexSpace(line, spaceBegin);
  if (nameBegin == spaceBegin || nameBegin >= line.size() || !isAsciiLetter(line[nameBegin]))
  {
    return std::nullopt;
  }
  auto end = nameRunEnd(line, nameBegin);
  while (line[end - 1] == '-')
  {
    --end;
  }
  return NameMatch{nameBegin, end, end};
}

// Backtracking to a shorter name leaves `-` next, which fails `\s*\(`.
std::optional<NameMatch> callAt(std::string_view line, std::size_t i)
{
  if (!nameStartsAt(line, i))
  {
    return std::nullopt;
  }
  const auto end = nameRunEnd(line, i);
  const auto paren = skipRegexSpace(line, end);
  if (paren >= line.size() || line[paren] != '(')
  {
    return std::nullopt;
  }
  return NameMatch{i, end, paren + 1};
}

// First `\d+` match, or empty.
std::string_view firstDigits(std::string_view line)
{
  std::size_t i = 0;
  while (i < line.size() && !isAsciiDigit(line[i]))
  {
    ++i;
  }
  auto end = i;
  while (end < line.size() && isAsciiDigit(line[end]))
  {
    ++end;
  }
  return line.substr(i, end - i);
}

// An assignment site only when the name starts a statement: at the start of
// the line, after a `;` separator (`P= 5; X= 6`), or right after `then` /
// `else` (`if c then X = 1`). `if X = 1`
// is a comparison, and a match inside a string is text.
bool startsStatement(std::string_view line, std::size_t nameBegin)
{
  auto end = nameBegin;
  while (end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\t'))
  {
    --end;
  }
  if (end == 0 || line[end - 1] == ';')
  {
    return true;
  }
  auto begin = end;
  while (begin > 0 && isRegexWord(line[begin - 1]))
  {
    --begin;
  }
  if (begin > 0 && line[begin - 1] != ' ' && line[begin - 1] != '\t')
  {
    return false;
  }
  const auto word = toLowerAscii(line.substr(begin, end - begin));
  return word == "then" || word == "else";
}

constexpr std::array<std::string_view, 28> kObjectTypes{
  "iterator", "edge", "selemobj", "profileend", "vxgeom", "profile", "sheet",
  "area", "joint", "point", "slope", "polyline", "array", "value", "wall",
  "boolean", "pvector", "xvector", "yvector", "zvector", "nvector", "line",
  "hatch", "text", "dvector", "macro", "databaselist", "database",
};

bool isKeywordLabel(std::string_view name)
{
  return catalog::isKeywordLabel(name);
}

std::string objectTypeFor(const HoverTuple& tuple)
{
  const auto comment = tuple.comment ? toLowerAscii(*tuple.comment) : std::string{};
  if (comment.find('[') != std::string::npos)
  {
    const auto open = comment.find('[');
    const auto close = comment.find(']', open);
    if (close != std::string::npos)
    {
      const auto inside = std::string(trim(std::string_view(comment).substr(
        open + 1, close - open - 1)));
      for (const auto type : kObjectTypes)
      {
        if (inside == type)
        {
          return std::string(type);
        }
      }
    }
  }
  return "generic";
}

void scanLine(VariableExtract& extract, std::string_view rawLine, bool includeHovers, int lineIndex = -1)
{
  const auto trimmedRaw = trim(rawLine);
  const auto lead = static_cast<std::size_t>(trimmedRaw.data() - rawLine.data());
  const auto recordSites = lineIndex >= 0;
  auto line = std::string(trimmedRaw);
  std::optional<std::string> hoverText;
  if (includeHovers)
  {
    hoverText = captureHoverComment(line);
  }
  line = stripValidationComment(line);
  if (trim(line).starts_with("!"))
  {
    return;
  }

  // Which matches are assignment sites (see AssignmentSite).
  enum class Sites
  {
    AtStatementStart,
    Always,
    Never,
  };
  auto scan = [&](auto tryAt, bool stringVar, bool attachHover, Sites sitePolicy) {
    forEachMatch(std::string_view(line), tryAt, [&](const NameMatch& match) {
      auto name = line.substr(match.nameBegin, match.nameEnd - match.nameBegin);
      if (isKeywordLabel(name))
      {
        return;
      }
      if (recordSites
          && (sitePolicy == Sites::Always
              || (sitePolicy == Sites::AtStatementStart && startsStatement(line, match.nameBegin))))
      {
        auto& sites = stringVar ? extract.stringAssignmentSites : extract.assignmentSites;
        sites[name].push_back(AssignmentSite{.line = lineIndex, .begin = lead + match.nameBegin});
      }
      if (stringVar)
      {
        extract.stringVariablesSet.insert(name);
        extract.stringVariables.push_back(name);
        if (includeHovers)
        {
          extract.hoverStringVariables.push_back(HoverTuple{
            .name = line.substr(match.nameBegin, match.matchEnd - match.nameBegin),
            .comment = attachHover ? hoverText : std::nullopt,
            .objectType = {},
          });
        }
      }
      else
      {
        extract.variablesSet.insert(name);
        extract.variables.push_back(name);
        if (includeHovers)
        {
          extract.hoverVariables.push_back(HoverTuple{
            .name = std::move(name),
            .comment = attachHover ? hoverText : std::nullopt,
            .objectType = {},
          });
        }
      }
    });
  };

  scan(stringAssignAt, true, true, Sites::AtStatementStart);
  scan(assignAt, false, true, Sites::AtStatementStart);
  scan(setAt, false, false, Sites::Always);
  // `name(` is a call or an array read, never an assignment.
  scan(callAt, false, false, Sites::Never);

  auto pushSynthetic = [&](char prefix) {
    std::string name(1, prefix);
    if (const auto digits = firstDigits(line); !digits.empty())
    {
      name += digits;
    }
    else
    {
      name += "null";
    }
    if (recordSites)
    {
      extract.assignmentSites[name].push_back(AssignmentSite{.line = lineIndex});
    }
    extract.variablesSet.insert(name);
    extract.variables.push_back(name);
    if (includeHovers)
    {
      extract.hoverVariables.push_back(HoverTuple{
        .name = name,
        .comment = std::nullopt,
        .objectType = {},
      });
    }
  };

  if (line.find("find_text_param") != std::string::npos)
  {
    pushSynthetic('T');
  }
  if (line.find("find_line_param") != std::string::npos)
  {
    pushSynthetic('L');
  }
  if (line.find("find_hatch_param") != std::string::npos)
  {
    pushSynthetic('H');
  }
  if (line.find("find_macro_param") != std::string::npos)
  {
    pushSynthetic('M');
  }
  if (line.find("find_hatch") != std::string::npos)
  {
    pushSynthetic('H');
  }
  if (line.find("get_hatch_line") != std::string::npos)
  {
    pushSynthetic('L');
  }
  if (line.find("point(") != std::string::npos)
  {
    pushSynthetic('P');
  }
  if (line.find("line(") != std::string::npos || line.find("circle(") != std::string::npos)
  {
    pushSynthetic('L');
  }
  if (line.find("hatch(") != std::string::npos)
  {
    pushSynthetic('H');
  }
}

void finishExtract(VariableExtract& extract)
{
  // Keeps each name's first occurrence, in order.
  auto dedupe = [](std::vector<std::string>& names) {
    std::vector<std::string> unique;
    unique.reserve(names.size());
    std::unordered_set<std::string_view> seen;
    seen.reserve(names.size());
    for (const auto& name : names)
    {
      if (seen.insert(name).second)
      {
        unique.push_back(name);
      }
    }
    names = std::move(unique);
  };
  dedupe(extract.variables);
  dedupe(extract.stringVariables);

  extract.variables.erase(
    std::remove_if(
      extract.variables.begin(),
      extract.variables.end(),
      [](const std::string& name) { return isKeywordLabel(name); }),
    extract.variables.end());

  for (auto& tuple : extract.hoverVariables)
  {
    tuple.objectType = objectTypeFor(tuple);
  }
}

void scanText(VariableExtract& extract, std::string_view text, bool includeHovers)
{
  for (const auto line : splitLines(text))
  {
    scanLine(extract, line, includeHovers);
  }
}

std::string uriKey(const lsp::Uri& uri)
{
  return uri.toString();
}

} // namespace

auto fillLiveBufferExtract(std::string_view text) -> VariableExtract
{
  VariableExtract extract;
  int lineIndex = 0;
  for (const auto line : splitLines(text))
  {
    scanLine(extract, line, true, lineIndex++);
  }
  finishExtract(extract);
  return extract;
}

auto scopeFor(const VariableExtract& neighborhood, const std::string& sectionName) -> const ScopedNames&
{
  if (!sectionName.empty())
  {
    const auto it = neighborhood.bySection.find(toLowerAscii(sectionName));
    if (it != neighborhood.bySection.end())
    {
      return it->second;
    }
  }
  return neighborhood.outsideSections;
}

void ExtractStore::putLive(const lsp::Uri& uri, int version, VariableExtract extract)
{
  const auto lock = std::lock_guard(mutex_);
  live_[uriKey(uri)] = LiveSlot{
    .version = version,
    .extract = std::make_shared<const VariableExtract>(std::move(extract)),
  };
}

void ExtractStore::eraseLive(const lsp::Uri& uri)
{
  const auto lock = std::lock_guard(mutex_);
  live_.erase(uriKey(uri));
}

auto ExtractStore::live(const lsp::Uri& uri, int version) const
  -> std::shared_ptr<const VariableExtract>
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = live_.find(uriKey(uri));
  if (it == live_.end() || it->second.version != version)
  {
    return {};
  }
  return it->second.extract;
}

auto ExtractStore::putNeighborhood(const lsp::Uri& uri, VariableExtract extract) -> std::uint64_t
{
  const auto lock = std::lock_guard(mutex_);
  const auto key = uriKey(uri);
  auto& slot = neighborhood_[key];
  for (const auto& path : slot.includedPaths)
  {
    auto reverse = reverseIncludes_.find(path);
    if (reverse != reverseIncludes_.end())
    {
      reverse->second.erase(key);
      if (reverse->second.empty())
      {
        reverseIncludes_.erase(reverse);
      }
    }
  }
  slot.includedPaths = extract.includedFromPaths;
  for (const auto& path : slot.includedPaths)
  {
    reverseIncludes_[path].insert(key);
  }
  slot.extract = std::make_shared<const VariableExtract>(std::move(extract));
  slot.dirty = false;
  slot.generation = ++nextGeneration_;
  return slot.generation;
}

void ExtractStore::markNeighborhoodDirty(const lsp::Uri& uri)
{
  const auto lock = std::lock_guard(mutex_);
  auto& slot = neighborhood_[uriKey(uri)];
  slot.dirty = true;
  slot.generation = ++nextGeneration_;
}

auto ExtractStore::neighborhoodState(const lsp::Uri& uri) const -> NeighborhoodState
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = neighborhood_.find(uriKey(uri));
  if (it == neighborhood_.end())
  {
    return {};
  }
  return NeighborhoodState{
    .dirty = it->second.dirty,
    .extract = it->second.extract,
    .generation = it->second.generation,
  };
}

auto ExtractStore::neighborhoodGeneration(const lsp::Uri& uri) const -> std::uint64_t
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = neighborhood_.find(uriKey(uri));
  return it == neighborhood_.end() ? 0 : it->second.generation;
}

bool ExtractStore::neighborhoodDirty(const lsp::Uri& uri) const
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = neighborhood_.find(uriKey(uri));
  return it == neighborhood_.end() || it->second.dirty;
}

auto ExtractStore::neighborhood(const lsp::Uri& uri) const
  -> std::shared_ptr<const VariableExtract>
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = neighborhood_.find(uriKey(uri));
  if (it == neighborhood_.end())
  {
    return {};
  }
  return it->second.extract;
}

auto ExtractStore::ownersOfIncludedPath(std::string_view normalizedPath) const
  -> std::vector<lsp::Uri>
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = reverseIncludes_.find(std::string(normalizedPath));
  if (it == reverseIncludes_.end())
  {
    return {};
  }
  std::vector<lsp::Uri> uris;
  uris.reserve(it->second.size());
  for (const auto& key : it->second)
  {
    uris.push_back(lsp::Uri::parse(key));
  }
  return uris;
}

void ExtractStore::eraseNeighborhood(const lsp::Uri& uri)
{
  const auto lock = std::lock_guard(mutex_);
  const auto key = uriKey(uri);
  auto it = neighborhood_.find(key);
  if (it == neighborhood_.end())
  {
    return;
  }
  for (const auto& path : it->second.includedPaths)
  {
    auto reverse = reverseIncludes_.find(path);
    if (reverse != reverseIncludes_.end())
    {
      reverse->second.erase(key);
      if (reverse->second.empty())
      {
        reverseIncludes_.erase(reverse);
      }
    }
  }
  neighborhood_.erase(it);
}

void ExtractStore::clearNeighborhoods()
{
  const auto lock = std::lock_guard(mutex_);
  neighborhood_.clear();
  reverseIncludes_.clear();
}

} // namespace vertexadt
