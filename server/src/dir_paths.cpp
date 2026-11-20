#include "dir_paths.h"

#include "text_util.h"
#include "utf16_text.h"

#include <algorithm>
#include <array>
#include <system_error>
#include <vector>

namespace vertexadt {
namespace {

using enum DirTokenKind;
using enum DirTree;

constexpr std::string_view kTokenPrefix = "$DIR_";
// Ends a token inside a traced value, so `$DIR_MACROS` followed by `DOOR`
// stays a token and displays glued (`custom/macrosDOOR`). Never shown.
constexpr char kTokenEnd = '\x01';

auto withoutTokenEnds(std::string_view text) -> std::string
{
  std::string out;
  out.reserve(text.size());
  for (const auto ch : text)
  {
    if (ch != kTokenEnd)
    {
      out += ch;
    }
  }
  return out;
}

constexpr std::array<DirToken, 32> kDirTokens{{
  {"$DIR_BEND", "bend", Fallback},
  {"$DIR_COMPLIBS", "complibs", Fallback},
  {"$DIR_DBASES", "dbases", Fallback},
  {"$DIR_DETAILS", "details", Fallback},
  {"$DIR_DETBOOK", "detbook", Fallback},
  {"$DIR_FONTS", "fonts", Fallback},
  {"$DIR_FORMS", "forms", Fallback},
  {"$DIR_HELP", "help", Fallback},
  {"$DIR_INTERNET", "internet", Fallback},
  {"$DIR_JOINTLIB", "jointlib", Fallback},
  {"$DIR_LIGHTWORKS", "lw", Fallback},
  {"$DIR_MACBASE", "macbase", Fallback},
  {"$DIR_MACROS", "macros", Fallback},
  {"$DIR_PICTS", "picts", Fallback},
  {"$DIR_PIPELIB", "pipelibrary", Fallback},
  {"$DIR_PROGRAMS", "programs", Fallback},
  {"$DIR_RAILING", "railing", Fallback},
  {"$DIR_RTF", "rtf", Fallback},
  {"$DIR_SETUP", "setup", Fallback},
  {"$DIR_SYMBOLS", "symbols", Fallback},
  {"$DIR_SYSBASE", "sysbase", Fallback},
  {"$DIR_SYSPROGS", "sysprogs", Fallback},
  {"$DIR_TEXTS", "texts", Fallback},
  {"$DIR_TIFF", "tiff", Fallback},
  {"$DIR_SYMBOLS_SYSTEM", "symbols", Pinned, System},
  {"$DIR_PICTS_SHARED", "picts", Pinned, Shared},
  {"$DIR_SYSPROGS_SHARED", "sysprogs", Pinned, Shared},
  {"$DIR_CUSTOM", "", Pinned, Custom},
  {"$DIR_SYSTEM", "", Pinned, System},
  {"$DIR_SHARED", "", Pinned, Shared},
  {"$DIR_THIS", "", This},
  {"$DIR_ACTPROJ", "", ActProj},
}};

bool isWordByte(char ch)
{
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
}

bool isIdentifier(std::string_view text)
{
  if (text.empty() || !(isWordByte(text.front()) && !(text.front() >= '0' && text.front() <= '9')))
  {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    if (!isWordByte(text[i]) && !(text[i] == '$' && i + 1 == text.size()))
    {
      return false;
    }
  }
  return true;
}

bool looksLikeCustomName(std::string_view name)
{
  return toLowerAscii(name.substr(0, 6)) == "custom";
}

bool pathExists(const std::filesystem::path& path)
{
  std::error_code ec;
  return std::filesystem::exists(path, ec);
}

bool rootIsCustom(const LibraryLayout& layout)
{
  return looksLikeCustomName(layout.workspaceRoot().filename().string());
}

auto treeRoot(const LibraryLayout& layout, DirTree tree) -> std::filesystem::path
{
  const auto& root = layout.workspaceRoot();
  const bool custom = rootIsCustom(layout);
  switch (tree)
  {
  case Custom:
    return custom ? root : root / layout.customFolderName();
  case System:
    return custom ? root.parent_path() / "system" : root / "system";
  case Shared:
    return custom ? root.parent_path() / "shared" : root / "shared";
  }
  return root;
}

auto treeLabel(const LibraryLayout& layout, DirTree tree) -> std::string
{
  switch (tree)
  {
  case Custom:
    return rootIsCustom(layout) ? layout.workspaceRoot().filename().string() : layout.customFolderName();
  case System:
    return "system";
  case Shared:
    return "shared";
  }
  return {};
}

auto joinRelative(std::string_view base, std::string_view folder, std::string_view remainder) -> std::string
{
  std::string out(base);
  if (!folder.empty())
  {
    if (!out.empty())
    {
      out += '/';
    }
    out += folder;
  }
  out += remainder;
  return out;
}

auto joinFs(const std::filesystem::path& root, std::string_view folder, std::string_view remainder)
  -> std::filesystem::path
{
  auto path = std::filesystem::path(joinRelative(root.generic_string(), folder, remainder)).lexically_normal();
  // A value built as a folder ends in `/`. Keep it in the text, not the link.
  if (!path.has_filename() && path.has_relative_path())
  {
    path = path.parent_path();
  }
  return path;
}

// ---- value tracing --------------------------------------------------------

class Tracer
{
public:
  explicit Tracer(std::string_view text)
  {
    std::size_t start = 0;
    while (start <= text.size())
    {
      auto end = text.find('\n', start);
      if (end == std::string_view::npos)
      {
        end = text.size();
      }
      auto line = text.substr(start, end - start);
      if (!line.empty() && line.back() == '\r')
      {
        line.remove_suffix(1);
      }
      lines_.push_back(line);
      start = end + 1;
    }
  }

  // Value of `name` as assigned at or above `upper`.
  auto variable(std::string_view name, int upper, int hops) const -> std::optional<std::string>
  {
    if (hops > kDirTraceMaxHops)
    {
      return std::nullopt;
    }
    for (int l = std::min(upper, static_cast<int>(lines_.size()) - 1); l >= 0; --l)
    {
      if (const auto found = assignment(lines_[static_cast<std::size_t>(l)], name))
      {
        return expression(found->value, l - 1, hops);
      }
    }
    return std::nullopt;
  }

  // `#..#` interpolations in string-literal text.
  auto literal(std::string_view body, int upper, int hops) const -> std::string
  {
    std::string out;
    for (std::size_t i = 0; i < body.size();)
    {
      if (body[i] == '$')
      {
        if (const auto span = findDirTokenSpan(body, i); span && span->begin == i)
        {
          out += body.substr(i, span->end - i);
          out += kTokenEnd;
          i = span->end;
          continue;
        }
      }
      if (body[i] != '#')
      {
        out += body[i++];
        continue;
      }
      const auto close = body.find('#', i + 1);
      if (close == std::string_view::npos)
      {
        out += body.substr(i);
        break;
      }
      const auto inner = body.substr(i + 1, close - i - 1);
      if (inner.starts_with(kTokenPrefix))
      {
        out += inner;
        out += kTokenEnd;
      }
      else if (isIdentifier(inner))
      {
        out += placeholderOr(variable(inner, upper, hops + 1), inner);
      }
      else
      {
        out += body.substr(i, close - i + 1);
      }
      i = close + 1;
    }
    return out;
  }

private:
  std::vector<std::string_view> lines_;

  static auto placeholderOr(std::optional<std::string> value, std::string_view name) -> std::string
  {
    return value ? std::move(*value) : "<" + std::string(name) + ">";
  }

public:
  struct Assignment
  {
    std::size_t equals = 0; // byte index of `=` in the line
    std::string_view value;
  };

  // `name = value` on this line (comment stripped), or nullopt.
  static auto assignment(std::string_view line, std::string_view name) -> std::optional<Assignment>
  {
    if (const auto comment = findInlineBangCommentIndex(line); comment >= 0)
    {
      line = line.substr(0, static_cast<std::size_t>(comment));
    }
    auto code = trim(line);
    if (!code.starts_with(name))
    {
      return std::nullopt;
    }
    code.remove_prefix(name.size());
    if (!code.empty() && (isWordByte(code.front()) || code.front() == '$'))
    {
      return std::nullopt;
    }
    code = trim(code);
    if (code.empty() || code.front() != '=' || (code.size() > 1 && code[1] == '='))
    {
      return std::nullopt;
    }
    return Assignment{
      .equals = static_cast<std::size_t>(code.data() - line.data()),
      .value = trim(code.substr(1)),
    };
  }

private:

  auto expression(std::string_view expr, int upper, int hops) const -> std::string
  {
    std::string out;
    std::size_t start = 0;
    bool quoted = false;
    int depth = 0;
    for (std::size_t i = 0; i <= expr.size(); ++i)
    {
      const auto ch = i < expr.size() ? expr[i] : '&';
      if (i < expr.size() && ch == '"')
      {
        quoted = !quoted;
      }
      else if (!quoted && ch == '(')
      {
        ++depth;
      }
      else if (!quoted && ch == ')')
      {
        --depth;
      }
      else if ((i == expr.size()) || (!quoted && depth == 0 && ch == '&'))
      {
        out += term(trim(expr.substr(start, i - start)), upper, hops);
        start = i + 1;
      }
    }
    return out;
  }

  auto term(std::string_view t, int upper, int hops) const -> std::string
  {
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"'
        && t.substr(1, t.size() - 2).find('"') == std::string_view::npos)
    {
      return literal(t.substr(1, t.size() - 2), upper, hops);
    }
    if (t.starts_with(kTokenPrefix) && isIdentifier(t.substr(1)))
    {
      return std::string(t) + kTokenEnd;
    }
    if (toLowerAscii(t.substr(0, 3)) == "get")
    {
      auto rest = trim(t.substr(3));
      if (rest.size() >= 2 && rest.front() == '(' && rest.back() == ')')
      {
        const auto arg = trim(rest.substr(1, rest.size() - 2));
        if (arg.starts_with(kTokenPrefix) && isIdentifier(arg.substr(1)))
        {
          // Vertex returns the folder with a trailing separator.
          return std::string(arg) + kTokenEnd + "/";
        }
      }
    }
    if (isIdentifier(t))
    {
      return placeholderOr(variable(t, upper, hops + 1), t);
    }
    return "<expr>";
  }
};

bool isPathTerminator(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '"' || ch == '\'' || ch == ',' || ch == '(' || ch == ')'
    || ch == '&' || ch == ';';
}

} // namespace

auto dirTokens() -> std::span<const DirToken>
{
  return kDirTokens;
}

auto findDirToken(std::string_view token) -> const DirToken*
{
  for (const auto& row : dirTokens())
  {
    if (row.token == token)
    {
      return &row;
    }
  }
  return nullptr;
}

auto findDirTokenSpan(std::string_view text, std::size_t from) -> std::optional<DirTokenSpan>
{
  for (auto pos = text.find(kTokenPrefix, from); pos != std::string_view::npos;
       pos = text.find(kTokenPrefix, pos + 1))
  {
    auto end = pos + kTokenPrefix.size();
    while (end < text.size() && isWordByte(text[end]))
    {
      ++end;
    }
    if (const auto* row = findDirToken(text.substr(pos, end - pos)))
    {
      return DirTokenSpan{.token = row, .begin = pos, .end = end};
    }
  }
  return std::nullopt;
}

auto displayDirPath(
  const LibraryLayout& layout,
  const std::filesystem::path& documentPath,
  const DirToken& token,
  std::string_view remainder) -> DirPathDisplay
{
  const auto rest = replaceBackslashes(withoutTokenEnds(remainder));
  const bool placeholder = rest.find('<') != std::string::npos;
  switch (token.kind)
  {
  case Fallback:
  {
    for (const auto tree : {Custom, System})
    {
      const auto root = treeRoot(layout, tree);
      const auto probe = placeholder ? joinFs(root, token.folder, "") : joinFs(root, token.folder, rest);
      if (!layout.workspaceRoot().empty() && pathExists(probe))
      {
        return DirPathDisplay{
          .text = joinRelative(treeLabel(layout, tree), token.folder, rest),
          .target = placeholder ? std::nullopt : std::optional(probe),
        };
      }
    }
    return DirPathDisplay{.text = joinRelative("none", token.folder, rest)};
  }
  case Pinned:
  {
    const auto probe = joinFs(treeRoot(layout, token.tree), token.folder, rest);
    const bool exists = !placeholder && !layout.workspaceRoot().empty() && pathExists(probe);
    return DirPathDisplay{
      .text = joinRelative(treeLabel(layout, token.tree), token.folder, rest),
      .target = exists ? std::optional(probe) : std::nullopt,
    };
  }
  case This:
  {
    const auto folder = documentPath.parent_path();
    const auto& root = layout.workspaceRoot();
    auto relative = root.empty() ? std::string{} : folder.lexically_relative(root).generic_string();
    if (relative == ".")
    {
      relative.clear();
    }
    std::string base;
    if (root.empty() || relative.starts_with(".."))
    {
      base = folder.generic_string();
    }
    else
    {
      base = joinRelative(rootIsCustom(layout) ? root.filename().string() : "", relative, "");
    }
    const auto probe = joinFs(folder, "", rest);
    const bool exists = !placeholder && !documentPath.empty() && pathExists(probe);
    return DirPathDisplay{
      .text = base + rest,
      .target = exists ? std::optional(probe) : std::nullopt,
    };
  }
  case ActProj:
    return DirPathDisplay{.text = "shared/projects/<actproj>" + rest};
  }
  return {};
}

auto displayDirValue(
  const LibraryLayout& layout,
  const std::filesystem::path& documentPath,
  std::string_view value) -> std::optional<DirPathDisplay>
{
  const auto span = findDirTokenSpan(value);
  if (!span)
  {
    return std::nullopt;
  }
  auto shown = displayDirPath(layout, documentPath, *span->token, value.substr(span->end));
  if (span->begin > 0)
  {
    shown.text = withoutTokenEnds(value.substr(0, span->begin)) + shown.text;
    shown.target.reset();
  }
  return shown;
}

auto traceVariableValue(std::string_view text, int line, std::string_view name)
  -> std::optional<std::string>
{
  if (name.empty())
  {
    return std::nullopt;
  }
  auto value = Tracer(text).variable(name, line, 0);
  return value ? std::optional(withoutTokenEnds(*value)) : std::nullopt;
}

auto dirTokenHover(
  const LibraryLayout& layout,
  const std::filesystem::path& documentPath,
  std::string_view text,
  lsp::Position position) -> std::optional<DirTokenHover>
{
  const auto line = lineAt(text, position.line);
  if (!line)
  {
    return std::nullopt;
  }
  const auto column = static_cast<std::size_t>(position.character);
  for (auto span = findDirTokenSpan(*line); span; span = findDirTokenSpan(*line, span->end))
  {
    if (column < utf16Offset(*line, span->begin) || column >= utf16Offset(*line, span->end))
    {
      continue;
    }
    auto pos = span->end;
    if (pos < line->size() && (*line)[pos] == '#' && span->begin > 0 && (*line)[span->begin - 1] == '#')
    {
      ++pos;
    }
    auto end = pos;
    while (end < line->size() && !isPathTerminator((*line)[end]))
    {
      ++end;
    }
    const auto upper = static_cast<int>(position.line) - 1;
    const auto rest = Tracer(text).literal(line->substr(pos, end - pos), upper, 0);
    return DirTokenHover{
      .token = span->token,
      .path = displayDirPath(layout, documentPath, *span->token, rest),
    };
  }
  return std::nullopt;
}

auto dirVariableHover(
  const LibraryLayout& layout,
  const std::filesystem::path& documentPath,
  std::string_view text,
  lsp::Position position,
  std::string_view name) -> std::optional<DirPathDisplay>
{
  if (name.empty())
  {
    return std::nullopt;
  }
  auto upper = static_cast<int>(position.line);
  if (const auto line = lineAt(text, position.line))
  {
    // Right of `=` on its own assignment line: the value before this line.
    if (const auto found = Tracer::assignment(*line, name);
        found && position.character > utf16Offset(*line, found->equals))
    {
      --upper;
    }
  }
  const auto value = Tracer(text).variable(name, upper, 0);
  if (!value)
  {
    return std::nullopt;
  }
  return displayDirValue(layout, documentPath, *value);
}

auto dirPathMarkdown(const DirPathDisplay& path) -> std::string
{
  const auto code = "`" + path.text + "`";
  if (!path.target)
  {
    return code;
  }
  std::string uri;
  for (const auto ch : uriFromPath(*path.target).toString())
  {
    if (ch == '(')
    {
      uri += "%28";
    }
    else if (ch == ')')
    {
      uri += "%29";
    }
    else
    {
      uri += ch;
    }
  }
  return "[" + code + "](" + uri + ")";
}

} // namespace vertexadt
