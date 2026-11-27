#include "infinite_loops.h"

#include "catalog/catalog.h"
#include "utf16_text.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

constexpr int kMaxIncludeDepth = 16;

bool isIdentStart(char ch)
{
  const auto c = static_cast<unsigned char>(ch);
  return std::isalpha(c) != 0 || c == '_';
}

bool isIdentChar(char ch)
{
  const auto c = static_cast<unsigned char>(ch);
  return std::isalnum(c) != 0 || c == '_';
}

// `text` starts with `word` as a whole, case-sensitive word (`$` counts as a
// word character, so `loop$` is a string variable, not the keyword).
bool startsWithKeyword(std::string_view text, std::string_view word)
{
  if (!text.starts_with(word))
  {
    return false;
  }
  return text.size() == word.size() || (!isIdentChar(text[word.size()]) && text[word.size()] != '$');
}

bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
    return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
  });
}

// Byte index of the first whole-word, case-insensitive `word` outside double
// quotes at or after `from`, or npos.
std::size_t findWordIgnoreCase(std::string_view text, std::string_view word, std::size_t from = 0)
{
  bool inQuotes = false;
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    if (text[i] == '"')
    {
      inQuotes = !inQuotes;
      continue;
    }
    if (inQuotes || i < from || i + word.size() > text.size())
    {
      continue;
    }
    if (!equalsIgnoreCase(text.substr(i, word.size()), word))
    {
      continue;
    }
    const bool leftOk = i == 0 || !isIdentChar(text[i - 1]);
    const bool rightOk = i + word.size() == text.size() || !isIdentChar(text[i + word.size()]);
    if (leftOk && rightOk)
    {
      return i;
    }
  }
  return std::string_view::npos;
}

// Splits on `separator` at bracket depth 0, outside double quotes.
std::vector<std::string_view> splitTopLevel(std::string_view text, char separator)
{
  std::vector<std::string_view> out;
  int depth = 0;
  bool inQuotes = false;
  std::size_t start = 0;
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    const char ch = text[i];
    if (ch == '"')
    {
      inQuotes = !inQuotes;
    }
    else if (inQuotes)
    {
      continue;
    }
    else if (ch == '(' || ch == '[')
    {
      ++depth;
    }
    else if ((ch == ')' || ch == ']') && depth > 0)
    {
      --depth;
    }
    else if (ch == separator && depth == 0)
    {
      out.push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  out.push_back(text.substr(start));
  return out;
}

// Index of the `)` matching the `(` at `open`, or npos.
std::size_t matchingParen(std::string_view text, std::size_t open)
{
  int depth = 0;
  bool inQuotes = false;
  for (std::size_t i = open; i < text.size(); ++i)
  {
    const char ch = text[i];
    if (ch == '"')
    {
      inQuotes = !inQuotes;
    }
    else if (inQuotes)
    {
      continue;
    }
    else if (ch == '(')
    {
      ++depth;
    }
    else if (ch == ')' && --depth == 0)
    {
      return i;
    }
  }
  return std::string_view::npos;
}

struct Ident
{
  std::string_view name; // includes a trailing `$`
  bool call = false;     // followed by `(`
};

// Identifiers outside double quotes. Words glued to a digit or `.` on the left
// (number exponents, file extensions) are skipped.
std::vector<Ident> identifiers(std::string_view text)
{
  std::vector<Ident> out;
  bool inQuotes = false;
  std::size_t i = 0;
  while (i < text.size())
  {
    const char ch = text[i];
    if (ch == '"')
    {
      inQuotes = !inQuotes;
      ++i;
      continue;
    }
    if (inQuotes || !isIdentStart(ch)
      || (i > 0 && (std::isdigit(static_cast<unsigned char>(text[i - 1])) != 0 || text[i - 1] == '.')))
    {
      ++i;
      continue;
    }
    const auto start = i;
    while (i < text.size() && isIdentChar(text[i]))
    {
      ++i;
    }
    if (i < text.size() && text[i] == '$')
    {
      ++i;
    }
    auto j = i;
    while (j < text.size() && (text[j] == ' ' || text[j] == '\t'))
    {
      ++j;
    }
    out.push_back(Ident{.name = text.substr(start, i - start), .call = j < text.size() && text[j] == '('});
  }
  return out;
}

bool isOperatorWord(std::string_view name)
{
  return equalsIgnoreCase(name, "and") || equalsIgnoreCase(name, "or") || equalsIgnoreCase(name, "not")
    || equalsIgnoreCase(name, "then");
}

const catalog::KeywordCatalogEntry* keywordFor(std::string_view name)
{
  if (const auto* entry = catalog::findKeyword(name))
  {
    return entry;
  }
  return catalog::findKeyword(toLowerAscii(name));
}

// A built-in command or function: in the command catalog, or a keyword with
// an insert text (control keywords have none).
bool isCatalogedCommand(std::string_view name)
{
  if (catalog::findCommand(toLowerAscii(name)))
  {
    return true;
  }
  const auto* keyword = keywordFor(name);
  return keyword && keyword->insertText;
}

enum class SlotRole
{
  In,
  Out,
  Unknown,
};

// `in_X` / `inX` / `InX` read; `out_X` / `outX` write; anything else may write.
SlotRole roleOfLabel(std::string_view label)
{
  const auto prefixed = [&](std::string_view prefix) {
    if (label.size() <= prefix.size() || !equalsIgnoreCase(label.substr(0, prefix.size()), prefix))
    {
      return false;
    }
    const char next = label[prefix.size()];
    return next == '_' || std::isupper(static_cast<unsigned char>(next)) != 0;
  };
  if (prefixed("out"))
  {
    return SlotRole::Out;
  }
  if (prefixed("in"))
  {
    return SlotRole::In;
  }
  return SlotRole::Unknown;
}

std::vector<SlotRole> rolesOf(std::string_view segment)
{
  static const std::regex placeholder{R"(\$\{\d+:([^}]*)\})"};
  std::vector<SlotRole> roles;
  for (const auto piece : splitTopLevel(segment, ','))
  {
    if (piece.find('\\') != std::string_view::npos)
    {
      break; // a continued template: later slots stay Unknown
    }
    bool any = false;
    bool unknown = false;
    bool out = false;
    for (std::cregex_iterator it(piece.data(), piece.data() + piece.size(), placeholder), end; it != end; ++it)
    {
      any = true;
      const auto role = roleOfLabel(std::string_view((*it)[1].first, static_cast<std::size_t>((*it)[1].length())));
      unknown = unknown || role == SlotRole::Unknown;
      out = out || role == SlotRole::Out;
    }
    roles.push_back(!any || unknown ? SlotRole::Unknown : (out ? SlotRole::Out : SlotRole::In));
  }
  return roles;
}

// An invocation `name(inside) after` split into its two argument lists.
struct Invocation
{
  std::vector<std::string_view> inside;
  std::vector<std::string_view> after;
};

Invocation splitInvocation(std::string_view rest)
{
  Invocation out;
  rest = trim(rest);
  if (rest.starts_with('('))
  {
    const auto close = matchingParen(rest, 0);
    const auto inside = close == std::string_view::npos ? rest.substr(1) : rest.substr(1, close - 1);
    if (!trim(inside).empty())
    {
      out.inside = splitTopLevel(inside, ',');
    }
    rest = close == std::string_view::npos ? std::string_view{} : trim(rest.substr(close + 1));
  }
  if (!rest.empty())
  {
    out.after = splitTopLevel(rest, ',');
  }
  return out;
}

struct Template
{
  bool known = false;
  std::vector<SlotRole> inside;
  std::vector<SlotRole> after;
};

Template templateFor(std::string_view name)
{
  Template out;
  const auto* keyword = keywordFor(name);
  if (!keyword || !keyword->insertText)
  {
    return out;
  }
  const std::string_view text(keyword->insertText);
  // Skip a leading `${1:out} = ` and find the command name itself.
  std::size_t at = std::string_view::npos;
  for (std::size_t i = 0; i + name.size() <= text.size(); ++i)
  {
    if (equalsIgnoreCase(text.substr(i, name.size()), name) && (i == 0 || !isIdentChar(text[i - 1]))
      && (i + name.size() == text.size() || !isIdentChar(text[i + name.size()])))
    {
      at = i;
      break;
    }
  }
  if (at == std::string_view::npos)
  {
    return out;
  }
  out.known = true;
  auto rest = text.substr(at + name.size());
  if (rest.starts_with('('))
  {
    const auto close = matchingParen(rest, 0);
    if (close == std::string_view::npos)
    {
      out.inside = rolesOf(rest.substr(1));
      return out;
    }
    out.inside = rolesOf(rest.substr(1, close - 1));
    rest = rest.substr(close + 1);
  }
  rest = trim(rest);
  if (!rest.empty())
  {
    out.after = rolesOf(rest);
  }
  return out;
}

struct ExitifSite
{
  int line = 0;
  int startChar = 0;
  int endChar = 0;
  bool ignored = false;
  bool opaque = false; // the condition calls a command or reads no variable
  std::vector<std::string> vars;    // lowercase
  std::vector<std::string> display; // as written, unique
};

struct Frame
{
  bool bare = false;
  bool fromMain = false;
  bool ignored = false;
  int line = 0;
  int startChar = 0;
  int endChar = 0;
  bool hasExit = false;     // any exitif / break / exit / quit
  bool hasHardExit = false; // break / exit / quit
  bool opaque = false;      // a `call`: anything may change
  bool unknownInclude = false;
  bool exitifFromInclude = false;
  std::vector<ExitifSite> exitifs;
  std::unordered_set<std::string> changed;
};

// Where a statement came from, for keyword positions.
struct LineRef
{
  bool isMain = false;
  bool ignored = false;
  int line = 0;
  std::string_view raw;
};

class Walker
{
public:
  explicit Walker(const IncludeLoader& loadInclude) : loadInclude_(loadInclude) {}

  auto run(std::string_view text) -> std::vector<InfiniteLoopIssue>
  {
    const auto lines = splitLines(text);
    const auto sections = parseFileSections(lines);
    runLines(lines, 0, static_cast<int>(lines.size()), true, &sections, 0);
    if (!balanced_ || !stack_.empty())
    {
      return {};
    }
    std::stable_sort(issues_.begin(), issues_.end(), [](const auto& a, const auto& b) {
      return a.line < b.line;
    });
    return std::move(issues_);
  }

private:
  const IncludeLoader& loadInclude_;
  std::vector<Frame> stack_;
  std::vector<InfiniteLoopIssue> issues_;
  std::unordered_set<std::string> includeStack_;
  bool balanced_ = true;

  void runLines(
    const std::vector<std::string_view>& lines,
    int first,
    int last,
    bool isMain,
    const std::vector<NamedSection>* sections,
    int depth)
  {
    const auto base = stack_.size();
    for (int i = first; i < last; ++i)
    {
      const auto raw = lines[static_cast<std::size_t>(i)];
      if (isSharedPerLineSkip(raw))
      {
        continue;
      }
      auto code = stripValidationComment(raw);
      // Join `\` continuations into one logical statement.
      const int startLine = i;
      while (true)
      {
        auto view = std::string_view(code);
        while (!view.empty() && std::isspace(static_cast<unsigned char>(view.back())) != 0)
        {
          view.remove_suffix(1);
        }
        if (!view.ends_with('\\') || i + 1 >= last)
        {
          break;
        }
        code = std::string(view.substr(0, view.size() - 1)) + " "
          + std::string(trim(stripValidationComment(lines[static_cast<std::size_t>(i + 1)])));
        ++i;
      }
      const auto trimmed = trim(code);
      if (trimmed.empty())
      {
        continue;
      }
      const LineRef ref{
        .isMain = isMain,
        .ignored = isMain && sections && lineInVerificationIgnoredSection(startLine, *sections),
        .line = startLine,
        .raw = raw,
      };

      if (trimmed.starts_with("# include"))
      {
        if (!stack_.empty())
        {
          runInclude(trim(trimmed.substr(std::string_view("# include").size())), depth);
        }
        continue;
      }
      if (startsWithKeyword(trimmed, "loop"))
      {
        Frame frame;
        frame.bare = trim(trimmed.substr(4)).empty();
        frame.fromMain = isMain;
        frame.ignored = ref.ignored;
        frame.line = startLine;
        std::tie(frame.startChar, frame.endChar) = keywordColumns(ref, trimmed, 4);
        stack_.push_back(std::move(frame));
        continue;
      }
      if (startsWithKeyword(trimmed, "endloop"))
      {
        if (stack_.size() <= base)
        {
          if (isMain)
          {
            balanced_ = false;
          }
          continue;
        }
        auto frame = std::move(stack_.back());
        stack_.pop_back();
        evaluate(frame);
        continue;
      }
      handleStatements(trimmed, ref);
    }
    // Included code that leaves loops open: drop them silently.
    while (!isMain && stack_.size() > base)
    {
      stack_.pop_back();
    }
  }

  void runInclude(std::string_view afterInclude, int depth)
  {
    static const std::regex pathRe{R"re(^((?:"[^"]+"|'[^']+'|[^\s#;]+))\s*(.*)$)re"};
    std::cmatch match;
    std::optional<IncludedCode> code;
    IncludeCallTarget target;
    if (!afterInclude.empty()
      && std::regex_match(afterInclude.data(), afterInclude.data() + afterInclude.size(), match, pathRe))
    {
      target = splitIncludeCallTarget(match[1].str(), match[2].str());
      code = loadInclude_(target);
    }
    if (!code)
    {
      markUnknownInclude();
      return;
    }
    const auto key = code->key + "|" + target.targetSection;
    if (depth >= kMaxIncludeDepth || includeStack_.contains(key))
    {
      markUnknownInclude();
      return;
    }
    const auto lines = splitLines(code->text);
    int first = 0;
    int last = static_cast<int>(lines.size());
    if (!target.targetSection.empty())
    {
      const auto sections = parseFileSections(lines);
      const NamedSection* found = nullptr;
      for (const auto& section : sections)
      {
        if (section.name == target.targetSection)
        {
          found = &section;
          break;
        }
      }
      for (const auto& section : sections)
      {
        if (!found && equalsIgnoreCase(section.name, target.targetSection))
        {
          found = &section;
        }
      }
      if (!found)
      {
        markUnknownInclude();
        return;
      }
      first = found->openLine + 1;
      last = found->closeLine;
    }
    includeStack_.insert(key);
    runLines(lines, first, last, false, nullptr, depth + 1);
    includeStack_.erase(key);
  }

  void markUnknownInclude()
  {
    for (auto& frame : stack_)
    {
      frame.unknownInclude = true;
    }
  }

  static auto keywordColumns(const LineRef& ref, std::string_view trimmedStatement, std::size_t length)
    -> std::pair<int, int>
  {
    if (!ref.isMain)
    {
      return {0, 0};
    }
    // `trimmedStatement` views a copy of the line; find the keyword in the raw line.
    const auto raw = ref.raw;
    std::size_t at = raw.find(trimmedStatement.substr(0, length));
    if (at == std::string_view::npos)
    {
      at = 0;
    }
    const auto start = static_cast<int>(utf16Offset(raw, at));
    return {start, start + static_cast<int>(length)};
  }

  void markChanged(std::string_view name)
  {
    const auto key = toLowerAscii(name);
    for (auto& frame : stack_)
    {
      frame.changed.insert(key);
    }
  }

  void markAllIdentifiers(std::string_view text)
  {
    for (const auto& ident : identifiers(text))
    {
      markChanged(ident.name);
    }
  }

  // The leading identifier of an output argument (`A(i)` writes `A`).
  void markOutput(std::string_view arg)
  {
    const auto ids = identifiers(trim(arg));
    if (!ids.empty())
    {
      markChanged(ids.front().name);
    }
  }

  void applyInvocation(std::string_view name, std::string_view rest)
  {
    const auto call = splitInvocation(rest);
    const auto tmpl = templateFor(name);
    const auto apply = [&](const std::vector<std::string_view>& args, const std::vector<SlotRole>& roles) {
      for (std::size_t k = 0; k < args.size(); ++k)
      {
        const auto role = tmpl.known && k < roles.size() ? roles[k] : SlotRole::Unknown;
        if (role == SlotRole::Out)
        {
          markOutput(args[k]);
        }
        else if (role == SlotRole::Unknown)
        {
          markAllIdentifiers(args[k]);
        }
      }
    };
    apply(call.inside, tmpl.inside);
    apply(call.after, tmpl.after);
  }

  void handleStatements(std::string_view text, const LineRef& ref)
  {
    for (const auto piece : splitTopLevel(text, ';'))
    {
      handleStatement(trim(piece), ref);
    }
  }

  void handleStatement(std::string_view s, const LineRef& ref)
  {
    if (s.empty() || stack_.empty())
    {
      return;
    }
    if (startsWithKeyword(s, "if"))
    {
      if (!isInlineIfThen(s))
      {
        return; // block if header: reads only
      }
      const auto then = findWordIgnoreCase(s, "then");
      if (then == std::string_view::npos)
      {
        return;
      }
      auto tail = s.substr(then + 4);
      const auto elseAt = findWordIgnoreCase(tail, "else");
      if (elseAt != std::string_view::npos)
      {
        handleStatements(trim(tail.substr(0, elseAt)), ref);
        handleStatements(trim(tail.substr(elseAt + 4)), ref);
        return;
      }
      handleStatements(trim(tail), ref);
      return;
    }
    if (startsWithKeyword(s, "else") || startsWithKeyword(s, "endif") || startsWithKeyword(s, "then")
      || startsWithKeyword(s, "return"))
    {
      return;
    }
    if (startsWithKeyword(s, "exitif"))
    {
      recordExitif(s, ref);
      return;
    }
    if (startsWithKeyword(s, "break") || startsWithKeyword(s, "exit") || startsWithKeyword(s, "quit"))
    {
      for (auto& frame : stack_)
      {
        frame.hasExit = true;
        frame.hasHardExit = true;
      }
      return;
    }
    if (startsWithKeyword(s, "call"))
    {
      for (auto& frame : stack_)
      {
        frame.opaque = true;
      }
      return;
    }
    if (startsWithKeyword(s, "set"))
    {
      markOutput(s.substr(3));
      return;
    }

    // Assignment: `X = ...`, `X$ = ...`, `X(i) = ...`.
    static const std::regex assignRe{R"(^([A-Za-z_][A-Za-z0-9_]*\$?)\s*(\([^=]*\))?\s*=(?!=))"};
    std::cmatch match;
    if (std::regex_search(s.data(), s.data() + s.size(), match, assignRe))
    {
      markChanged(std::string_view(match[1].first, static_cast<std::size_t>(match[1].length())));
      const auto rhs = trim(s.substr(static_cast<std::size_t>(match.length(0))));
      const auto ids = identifiers(rhs);
      // `X = cmd(...) ...`: the command may write its output arguments.
      if (!ids.empty() && ids.front().name.data() == rhs.data() && isCatalogedCommand(ids.front().name))
      {
        applyInvocation(ids.front().name, rhs.substr(ids.front().name.size()));
      }
      return;
    }

    // A statement-level command.
    const auto ids = identifiers(s);
    if (ids.empty() || ids.front().name.data() != s.data())
    {
      return;
    }
    const auto name = ids.front().name;
    const auto rest = s.substr(name.size());
    if (isCatalogedCommand(name) || keywordFor(name))
    {
      applyInvocation(name, rest);
      return;
    }
    // Not in the catalog: any argument may be written.
    markAllIdentifiers(rest);
  }

  void recordExitif(std::string_view s, const LineRef& ref)
  {
    auto& top = stack_.back();
    top.hasExit = true;
    if (!ref.isMain)
    {
      top.exitifFromInclude = true;
      return;
    }
    ExitifSite site;
    site.line = ref.line;
    site.ignored = ref.ignored;
    std::tie(site.startChar, site.endChar) = keywordColumns(ref, s, 6);
    std::unordered_set<std::string> seen;
    for (const auto& ident : identifiers(s.substr(6)))
    {
      if (isOperatorWord(ident.name))
      {
        continue;
      }
      if (ident.call && isCatalogedCommand(ident.name))
      {
        site.opaque = true;
        continue;
      }
      auto key = toLowerAscii(ident.name);
      if (seen.insert(key).second)
      {
        site.vars.push_back(std::move(key));
        site.display.emplace_back(ident.name);
      }
    }
    if (site.vars.empty())
    {
      site.opaque = true;
    }
    top.exitifs.push_back(std::move(site));
  }

  void evaluate(const Frame& frame)
  {
    if (!frame.bare || !frame.fromMain || frame.ignored || frame.unknownInclude)
    {
      return;
    }
    if (!frame.hasExit)
    {
      issues_.push_back(InfiniteLoopIssue{
        .kind = InfiniteLoopKind::NoExit,
        .line = frame.line,
        .startChar = frame.startChar,
        .endChar = frame.endChar,
        .message = "Infinite loop: no exitif/break/exit inside loop ... endloop.",
      });
      return;
    }
    if (frame.hasHardExit || frame.opaque || frame.exitifFromInclude || frame.exitifs.empty())
    {
      return;
    }
    for (const auto& site : frame.exitifs)
    {
      if (site.opaque)
      {
        return;
      }
      for (const auto& var : site.vars)
      {
        if (frame.changed.contains(var))
        {
          return;
        }
      }
    }
    for (const auto& site : frame.exitifs)
    {
      if (site.ignored)
      {
        continue;
      }
      std::string names;
      for (std::size_t k = 0; k < site.display.size(); ++k)
      {
        names += (k == 0 ? "`" : ", `") + site.display[k] + "`";
      }
      issues_.push_back(InfiniteLoopIssue{
        .kind = InfiniteLoopKind::ExitifNeverChanges,
        .line = site.line,
        .startChar = site.startChar,
        .endChar = site.endChar,
        .message = "exitif condition can never become true inside this loop: " + names
          + (site.display.size() == 1 ? " is" : " are") + " never changed in the loop body.",
      });
    }
  }
};

} // namespace

auto findInfiniteLoops(std::string_view text, const IncludeLoader& loadInclude)
  -> std::vector<InfiniteLoopIssue>
{
  Walker walker(loadInclude);
  return walker.run(text);
}

} // namespace vertexadt
