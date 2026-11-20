#include "call_return_value_completion.h"
#include "utf16_text.h"

#include "call_parameters.h"
#include "extracts.h"

#include <cstddef>

// Port of the client host's call return-value provider (client/out/extension.js
// callRegex + item mapping) over server.js custom/getCallReturnValues /
// extractReturnValues, reading the buffer-wins linked-file snapshot instead of
// Node's disk-only read. Lines are scanned in UTF-16 code units so `\s` and
// String.prototype.trim match JS. The regexes are matched by hand: std::regex
// backtracking recursion is unsafe on long lines.

namespace vertexadt {
namespace {

bool isAsciiLetter(char16_t ch)
{
  return (ch >= u'a' && ch <= u'z') || (ch >= u'A' && ch <= u'Z');
}

// `[A-Z0-9_]`
bool isKeyChar(char16_t ch)
{
  return (ch >= u'A' && ch <= u'Z') || (ch >= u'0' && ch <= u'9') || ch == u'_';
}

// Narrow a code-unit run that is known to be ASCII.
auto asciiFrom16(U16View text) -> std::string
{
  std::string out;
  out.reserve(text.size());
  for (const auto ch : text)
  {
    out.push_back(static_cast<char>(ch));
  }
  return out;
}

// Tail `(?:,\s*[^)]*)*\)` at `e`: a `)` there, or a comma with a `)` after it.
bool tailMatchesAt(U16View line, std::size_t e)
{
  if (e >= line.size())
  {
    return false;
  }
  if (line[e] == u')')
  {
    return true;
  }
  return line[e] == u',' && line.find(u')', e + 1) != U16View::npos;
}

// Group 2 `("[^"]+"|'[^']+'|[^,)]+)` at `a`, followed by the tail.
auto matchPathAt(U16View line, std::size_t a) -> std::optional<U16View>
{
  const auto n = line.size();
  if (a >= n)
  {
    return std::nullopt;
  }
  for (const char16_t quote : {u'"', u'\''})
  {
    if (line[a] == quote)
    {
      const auto close = line.find(quote, a + 1);
      if (close != U16View::npos && close >= a + 2 && tailMatchesAt(line, close + 1))
      {
        return line.substr(a, close + 1 - a);
      }
    }
  }
  // `[^,)]+` can only end where the tail can start: at the first `,` or `)`.
  auto m = a;
  while (m < n && line[m] != u',' && line[m] != u')')
  {
    ++m;
  }
  if (m > a && tailMatchesAt(line, m))
  {
    return line.substr(a, m - a);
  }
  return std::nullopt;
}

struct SiteMatch
{
  U16View callerName;
  U16View rawPath;
};

// The host regex, leftmost match.
auto matchSiteRegex(U16View line) -> std::optional<SiteMatch>
{
  constexpr U16View kCallParen = u"call(";
  for (std::size_t d = 0; d < line.size(); ++d)
  {
    if (line[d] != u'$')
    {
      continue;
    }
    // `\w+\$`: every start inside the word run ending here gives the same
    // suffix, so the leftmost start is the run's start.
    auto r = d;
    while (r > 0 && isJsWord(line[r - 1]))
    {
      --r;
    }
    if (r == d)
    {
      continue;
    }
    auto i = skipJsSpace(line, d + 1);
    if (i >= line.size() || line[i] != u'=')
    {
      continue;
    }
    i = skipJsSpace(line, i + 1);
    if (line.substr(i, kCallParen.size()) != kCallParen)
    {
      continue;
    }
    const auto open = i + kCallParen.size();
    const auto a = skipJsSpace(line, open);
    const auto callerName = line.substr(r, d + 1 - r);
    if (const auto path = matchPathAt(line, a))
    {
      return SiteMatch{.callerName = callerName, .rawPath = *path};
    }
    // `\s*` backtracks one space into `[^,)]+` when the tail starts directly.
    if (a > open && tailMatchesAt(line, a))
    {
      return SiteMatch{.callerName = callerName, .rawPath = line.substr(a - 1, 1)};
    }
  }
  return std::nullopt;
}

// `/exit\s+(\w+\$)/` group 1, leftmost.
auto matchExitName(U16View line) -> std::optional<U16View>
{
  constexpr U16View kExit = u"exit";
  for (auto p = line.find(kExit); p != U16View::npos; p = line.find(kExit, p + 1))
  {
    const auto w = skipJsSpace(line, p + kExit.size());
    if (w == p + kExit.size())
    {
      continue;
    }
    auto e = w;
    while (e < line.size() && isJsWord(line[e]))
    {
      ++e;
    }
    if (e > w && e < line.size() && line[e] == u'$')
    {
      return line.substr(w, e + 1 - w);
    }
  }
  return std::nullopt;
}

// `/^\s*([a-zA-Z][a-zA-Z0-9_]*\$)\s*=/` group 1.
auto matchStringAssignment(U16View line) -> std::optional<U16View>
{
  const auto start = skipJsSpace(line, 0);
  if (start >= line.size() || !isAsciiLetter(line[start]))
  {
    return std::nullopt;
  }
  auto j = start + 1;
  while (j < line.size() && isJsWord(line[j]))
  {
    ++j;
  }
  if (j >= line.size() || line[j] != u'$')
  {
    return std::nullopt;
  }
  const auto k = skipJsSpace(line, j + 1);
  if (k >= line.size() || line[k] != u'=')
  {
    return std::nullopt;
  }
  return line.substr(start, j + 1 - start);
}

// `/"([A-Z0-9_]+)=|"\|([A-Z0-9_]+)=/` on one `"…"` part. Inside a part the
// only quotes are its two ends, so a match can only start at the opening one.
auto matchFieldKey(U16View inner) -> std::optional<U16View>
{
  auto keyAt = [inner](std::size_t from) -> std::optional<U16View>
  {
    auto e = from;
    while (e < inner.size() && isKeyChar(inner[e]))
    {
      ++e;
    }
    if (e > from && e < inner.size() && inner[e] == u'=')
    {
      return inner.substr(from, e - from);
    }
    return std::nullopt;
  };
  if (const auto key = keyAt(0))
  {
    return key;
  }
  if (!inner.empty() && inner[0] == u'|')
  {
    return keyAt(1);
  }
  return std::nullopt;
}

auto splitOnNewline(U16View text) -> std::vector<U16View>
{
  std::vector<U16View> lines;
  std::size_t start = 0;
  while (true)
  {
    const auto next = text.find(u'\n', start);
    if (next == U16View::npos)
    {
      lines.push_back(text.substr(start));
      return lines;
    }
    lines.push_back(text.substr(start, next - start));
    start = next + 1;
  }
}

auto lowerAscii(std::string text) -> std::string
{
  for (auto& ch : text)
  {
    if (ch >= 'A' && ch <= 'Z')
    {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  return text;
}

// First non-string hover tuple named exactly `key` with a non-empty comment.
auto fieldComment(const VariableExtract& extract, const std::string& key)
  -> std::optional<std::string>
{
  for (const auto& tuple : extract.hoverVariables)
  {
    if (tuple.name == key && tuple.comment && !tuple.comment->empty())
    {
      return tuple.comment;
    }
  }
  return std::nullopt;
}

} // namespace

auto callReturnValueSite(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<CallReturnValueSite>
{
  if (snapshot.languageId != "vertexadt" || position.line == 0)
  {
    return std::nullopt;
  }
  const auto row = lineAt(snapshot.text, position.line - 1);
  if (!row)
  {
    return std::nullopt;
  }
  const auto line = utf8To16(*row);
  const auto match = matchSiteRegex(jsTrim(line));
  if (!match)
  {
    return std::nullopt;
  }
  return CallReturnValueSite{
    .callerName = asciiFrom16(match->callerName),
    .rawPath = utf16To8(match->rawPath),
  };
}

auto extractReturnFields(std::string_view calleeText) -> std::vector<std::string>
{
  const auto text = utf8To16(calleeText);
  const auto lines = splitOnNewline(text);

  // First loop: the last `exit Name$` names the return variable.
  U16View returnVar;
  for (const auto raw : lines)
  {
    const auto line = jsTrim(raw);
    if (line.starts_with(u"exit"))
    {
      if (const auto name = matchExitName(line))
      {
        returnVar = *name;
      }
    }
  }

  // Second loop: every format line appends its fields. An empty name is a
  // substring of every line (`includes('')`).
  std::vector<std::string> fields;
  for (const auto raw : lines)
  {
    const auto line = jsTrim(raw);
    if (line.find(u'=') == U16View::npos || line.find(u'"') == U16View::npos
        || line.find(u'&') == U16View::npos || line.find(returnVar) == U16View::npos)
    {
      continue;
    }
    const auto lhs = matchStringAssignment(line);
    if (!lhs)
    {
      continue;
    }
    returnVar = *lhs;
    std::size_t cursor = 0;
    while (true)
    {
      const auto open = line.find(u'"', cursor);
      if (open == U16View::npos)
      {
        break;
      }
      const auto close = line.find(u'"', open + 1);
      if (close == U16View::npos)
      {
        break;
      }
      if (const auto key = matchFieldKey(line.substr(open + 1, close - open - 1)))
      {
        fields.push_back(asciiFrom16(*key));
      }
      cursor = close + 1;
    }
  }
  return fields;
}

auto callReturnValueCompletionItems(
  const CallReturnValueSite& site,
  const LibraryLayout& layout,
  const std::function<std::optional<std::string>(const std::filesystem::path&)>& textForPath)
  -> std::vector<lsp::CompletionItem>
{
  std::vector<lsp::CompletionItem> items;
  // Named-section suffix is stripped for the path; the scan stays whole-file.
  const auto target = callTargetFromUntrimmedArgument(site.rawPath);
  if (target.filePath.empty() || !textForPath)
  {
    return items;
  }
  const auto resolved = layout.resolveVertexFilePath(target.filePath);
  if (!resolved)
  {
    return items;
  }
  const auto text = textForPath(*resolved);
  if (!text || text->empty())
  {
    return items;
  }
  const auto fields = extractReturnFields(*text);
  if (fields.empty())
  {
    return items;
  }

  // Ticket 21's scan on the callee snapshot, once; never the caller's extract.
  const auto calleeExtract = fillLiveBufferExtract(*text);
  items.reserve(fields.size());
  for (const auto& key : fields)
  {
    const auto lower = lowerAscii(key);
    std::string documentation = "```vertexadt\n\"Extract\" " + key + " \"from return string\"\n";
    if (const auto comment = fieldComment(calleeExtract, key))
    {
      documentation += "!Variableinfo: " + *comment + "\n";
    }
    documentation += "```";

    lsp::CompletionItem item;
    item.label = "return:ret" + lower + "$ ";
    item.kind = lsp::CompletionItemKind::Snippet;
    item.insertText = "\nret" + lower + "$ = strfield(" + site.callerName + ",\"" + key + "\") \nret"
      + lower + " = ret" + lower + "$ ";
    item.documentation = lsp::MarkupContent{
      .kind = lsp::MarkupKind::Markdown,
      .value = std::move(documentation),
    };
    items.push_back(std::move(item));
  }
  return items;
}

} // namespace vertexadt
