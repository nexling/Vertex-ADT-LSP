#include "iterator_types.h"

#include "catalog/catalog.h"
#include "command_parse.h"
#include "text_util.h"

#include <cctype>
#include <optional>

namespace vertexadt {
namespace {

bool isWordChar(char ch)
{
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

bool isIdentifier(std::string_view text)
{
  if (text.empty() || !std::isalpha(static_cast<unsigned char>(text[0])))
  {
    return false;
  }
  for (const char ch : text)
  {
    if (!isWordChar(ch))
    {
      return false;
    }
  }
  return true;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
  if (a.size() != b.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i)
  {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
    {
      return false;
    }
  }
  return true;
}

struct Arg
{
  std::string_view text; // trimmed
  int start = 0;         // byte column in the line
};

// Comma-separated args of line[begin, end), split outside quotes and brackets.
auto splitArgs(std::string_view line, std::size_t begin, std::size_t end) -> std::vector<Arg>
{
  std::vector<Arg> args;
  const auto push = [&](std::size_t from, std::size_t to) {
    auto piece = line.substr(from, to - from);
    const auto trimmed = trim(piece);
    const auto offset = trimmed.empty() ? from : static_cast<std::size_t>(trimmed.data() - line.data());
    args.push_back(Arg{trimmed, static_cast<int>(offset)});
  };
  int depth = 0;
  bool inQuote = false;
  std::size_t pieceStart = begin;
  for (std::size_t i = begin; i < end; ++i)
  {
    const char ch = line[i];
    if (inQuote)
    {
      inQuote = ch != '"';
      continue;
    }
    if (ch == '"')
    {
      inQuote = true;
    }
    else if (ch == '(' || ch == '[')
    {
      ++depth;
    }
    else if ((ch == ')' || ch == ']') && depth > 0)
    {
      --depth;
    }
    else if (ch == ',' && depth == 0)
    {
      push(pieceStart, i);
      pieceStart = i + 1;
    }
  }
  if (pieceStart < end || !args.empty())
  {
    push(pieceStart, end);
  }
  return args;
}

// The value of a plain string literal (`"..."` with no other quote), or nullopt.
std::optional<std::string_view> literalValue(std::string_view arg)
{
  if (arg.size() < 2 || arg.front() != '"' || arg.back() != '"')
  {
    return std::nullopt;
  }
  const auto inner = arg.substr(1, arg.size() - 2);
  if (inner.find('"') != std::string_view::npos)
  {
    return std::nullopt;
  }
  return inner;
}

// `X =` (not `==`, `<=`, `>=`, `!=`) right before byte `at`.
std::optional<Arg> assignmentTarget(std::string_view line, std::size_t at)
{
  std::size_t i = at;
  while (i > 0 && (line[i - 1] == ' ' || line[i - 1] == '\t'))
  {
    --i;
  }
  if (i == 0 || line[i - 1] != '=')
  {
    return std::nullopt;
  }
  --i;
  if (i > 0 && (line[i - 1] == '=' || line[i - 1] == '<' || line[i - 1] == '>' || line[i - 1] == '!'))
  {
    return std::nullopt;
  }
  while (i > 0 && (line[i - 1] == ' ' || line[i - 1] == '\t'))
  {
    --i;
  }
  const auto end = i;
  while (i > 0 && isWordChar(line[i - 1]))
  {
    --i;
  }
  const auto name = line.substr(i, end - i);
  if (!isIdentifier(name))
  {
    return std::nullopt;
  }
  return Arg{name, static_cast<int>(i)};
}

struct Call
{
  std::size_t nameStart = 0;
  std::size_t open = 0;  // `(`
  std::size_t close = 0; // matching `)`
};

// Each `name(` (whole word, any case, outside quotes) with its matching `)`.
auto findCalls(std::string_view line, std::string_view name) -> std::vector<Call>
{
  std::vector<Call> calls;
  bool inQuote = false;
  for (std::size_t i = 0; i < line.size(); ++i)
  {
    const char ch = line[i];
    if (ch == '"')
    {
      inQuote = !inQuote;
      continue;
    }
    if (inQuote || !isWordChar(ch) || (i > 0 && isWordChar(line[i - 1])))
    {
      continue;
    }
    auto end = i;
    while (end < line.size() && isWordChar(line[end]))
    {
      ++end;
    }
    if (equalsIgnoreCase(line.substr(i, end - i), name))
    {
      auto open = end;
      while (open < line.size() && (line[open] == ' ' || line[open] == '\t'))
      {
        ++open;
      }
      if (open < line.size() && line[open] == '(')
      {
        const auto close = findMatchingParenIndex(line, static_cast<int>(open));
        if (close >= 0)
        {
          calls.push_back(Call{i, open, static_cast<std::size_t>(close)});
        }
      }
    }
    i = end - 1;
  }
  return calls;
}

// End of the statement that starts at `from`: the first unquoted `;`, or the line end.
std::size_t statementEnd(std::string_view line, std::size_t from)
{
  bool inQuote = false;
  for (std::size_t i = from; i < line.size(); ++i)
  {
    if (line[i] == '"')
    {
      inQuote = !inQuote;
    }
    else if (line[i] == ';' && !inQuote)
    {
      return i;
    }
  }
  return line.size();
}

std::string_view knownType(std::string_view value)
{
  for (const auto& row : catalog::iteratorObjectTypes())
  {
    if (row.name == value)
    {
      return row.type;
    }
  }
  return {};
}

bool isJointTask(std::string_view value)
{
  for (const auto task : catalog::iteratorJointTasks())
  {
    if (task == value)
    {
      return true;
    }
  }
  return false;
}

// A known first value `value` was likely meant to be: a listed typo, or a
// known (or listed typo) value that differs only in case.
std::string_view suggestionFor(std::string_view value)
{
  for (const auto& row : catalog::iteratorTypoHints())
  {
    if (row.name == value)
    {
      return row.type;
    }
  }
  for (const auto& row : catalog::iteratorObjectTypes())
  {
    if (equalsIgnoreCase(row.name, value))
    {
      return row.name;
    }
  }
  for (const auto& row : catalog::iteratorTypoHints())
  {
    if (equalsIgnoreCase(row.name, value))
    {
      return row.type;
    }
  }
  return {};
}

void classify(IteratorSite& site, const std::vector<Arg>& args)
{
  const Arg* typeArg = args.empty() ? nullptr : &args[0];
  const auto typeValue = typeArg ? literalValue(typeArg->text) : std::nullopt;
  const auto taskValue = args.size() > 2 ? literalValue(args[2].text) : std::nullopt;

  const auto flag = [&](IteratorProblem problem, std::string message) {
    site.problem = problem;
    site.message = std::move(message);
    site.problemStart = typeArg->start;
    site.problemEnd = typeArg->start + static_cast<int>(typeArg->text.size());
  };

  if (taskValue && isJointTask(*taskValue))
  {
    site.itemType = "joint";
    if (typeValue && !typeValue->empty())
    {
      const auto task = std::string(*taskValue);
      const auto type = knownType(*typeValue);
      flag(
        IteratorProblem::ConnectionsMismatch,
        type.empty()
          ? task + " iterators return joints; first value must be \"\", not \"" + std::string(*typeValue) + "\""
          : task + " iterators return joints; first value \"" + std::string(*typeValue) + "\" returns "
            + std::string(type));
    }
    return;
  }
  if (!typeValue || typeValue->empty())
  {
    return;
  }
  if (const auto type = knownType(*typeValue); !type.empty())
  {
    site.itemType = std::string(type);
    return;
  }
  auto message = "Unknown iterator object type \"" + std::string(*typeValue) + "\"; will return all objects";
  if (const auto suggestion = suggestionFor(*typeValue); !suggestion.empty())
  {
    message += " (did you mean \"" + std::string(suggestion) + "\"?)";
  }
  flag(IteratorProblem::UnknownType, std::move(message));
}

} // namespace

auto findIteratorSites(std::string_view line) -> std::vector<IteratorSite>
{
  std::vector<IteratorSite> sites;
  for (const auto& call : findCalls(line, "new_iterator"))
  {
    const auto target = assignmentTarget(line, call.nameStart);
    if (!target)
    {
      continue;
    }
    IteratorSite site;
    site.iterator = std::string(target->text);
    site.iteratorColumn = target->start;
    classify(site, splitArgs(line, call.open + 1, call.close));
    sites.push_back(std::move(site));
  }
  for (const auto& call : findCalls(line, "init_iterator"))
  {
    const auto inside = splitArgs(line, call.open + 1, call.close);
    if (inside.empty() || !isIdentifier(inside[0].text))
    {
      continue;
    }
    IteratorSite site;
    site.iterator = std::string(inside[0].text);
    site.iteratorColumn = inside[0].start;
    const auto after = call.close + 1;
    classify(site, splitArgs(line, after, statementEnd(line, after)));
    sites.push_back(std::move(site));
  }
  return sites;
}

auto findNextItems(std::string_view line) -> std::vector<NextItemSite>
{
  std::vector<NextItemSite> sites;
  for (const auto& call : findCalls(line, "next_item"))
  {
    const auto target = assignmentTarget(line, call.nameStart);
    const auto inside = splitArgs(line, call.open + 1, call.close);
    if (!target || inside.size() != 1 || !isIdentifier(inside[0].text))
    {
      continue;
    }
    sites.push_back(NextItemSite{
      .target = std::string(target->text),
      .targetColumn = target->start,
      .iterator = std::string(inside[0].text),
      .iteratorColumn = inside[0].start,
    });
  }
  return sites;
}

} // namespace vertexadt
