#include "include_call_completion.h"
#include "utf16_text.h"

#include "catalog/catalog.h"
#include "text_util.h"
#include "validate.h"

#include <algorithm>
#include <cstddef>
#include <system_error>

// Port of server/out/targetSectionSupport.js analyzeIncludeCallCompletion
// (matchDirective / parseDirectiveArgs / parseQuotedArg / parseSuffixSlot /
// readBarePath) plus server.js custom/getIncludeCallCompletion and the client
// host's item mapping. The line is parsed in UTF-16 code units so cursor
// columns, `\s`, and trimStart match JS.

namespace vertexadt {
namespace {

constexpr int kTriggerInvoked = 1;

bool isAsciiAlpha(char16_t ch)
{
  return (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z');
}

bool isAsciiDigit(char16_t ch)
{
  return ch >= u'0' && ch <= u'9';
}

bool isIdentStart(char16_t ch)
{
  return isAsciiAlpha(ch) || ch == u'_';
}

bool isIdentPart(char16_t ch)
{
  return isAsciiAlpha(ch) || isAsciiDigit(ch) || ch == u'_';
}

bool startsWith(U16View text, U16View prefix)
{
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

// targetSectionSupport.stripPathQuotes.
auto stripQuotes16(U16View value) -> U16
{
  const auto trimmed = jsTrim(value);
  if (trimmed.size() >= 2
      && ((trimmed.front() == u'"' && trimmed.back() == u'"')
        || (trimmed.front() == u'\'' && trimmed.back() == u'\'')))
  {
    return U16(trimmed.substr(1, trimmed.size() - 2));
  }
  if (!trimmed.empty() && (trimmed.front() == u'"' || trimmed.front() == u'\''))
  {
    return U16(trimmed.substr(1));
  }
  return U16(trimmed);
}

auto slashes16(U16 value) -> U16
{
  std::replace(value.begin(), value.end(), u'\\', u'/');
  return value;
}

enum class DirectiveKind
{
  IncludedFrom,
  Include,
  CallParen,
  CallSpace,
};

struct Directive
{
  DirectiveKind kind = DirectiveKind::Include;
  std::size_t argsStart = 0;
  bool sectionInsideQuotes = false;
};

// `call\s*\(\s*` at `at`; returns the end offset.
auto matchCallParen(U16View text, std::size_t at) -> std::optional<std::size_t>
{
  if (!startsWith(text.substr(at), u"call"))
  {
    return std::nullopt;
  }
  auto i = skipJsSpace(text, at + 4);
  if (i >= text.size() || text[i] != u'(')
  {
    return std::nullopt;
  }
  return skipJsSpace(text, i + 1);
}

// targetSectionSupport.matchDirective on line[0..cursor).
auto matchDirective(U16View prefix) -> std::optional<Directive>
{
  std::size_t leading = 0;
  while (leading < prefix.size() && isJsSpace(prefix[leading]))
  {
    ++leading;
  }
  const auto trimmed = prefix.substr(leading);

  constexpr U16View kIncludedFrom = u"##included_from";
  constexpr U16View kInclude = u"# include";
  if (startsWith(trimmed, kIncludedFrom))
  {
    return Directive{
      .kind = DirectiveKind::IncludedFrom,
      .argsStart = skipJsSpace(prefix, leading + kIncludedFrom.size()),
      .sectionInsideQuotes = false,
    };
  }
  if (startsWith(trimmed, kInclude))
  {
    return Directive{
      .kind = DirectiveKind::Include,
      .argsStart = skipJsSpace(prefix, leading + kInclude.size()),
      .sectionInsideQuotes = false,
    };
  }

  // ^[A-Za-z][A-Za-z0-9_]*\$?=call\s*\(\s*
  if (!trimmed.empty() && isAsciiAlpha(trimmed[0]))
  {
    std::size_t i = 1;
    while (i < trimmed.size() && isIdentPart(trimmed[i]))
    {
      ++i;
    }
    if (i < trimmed.size() && trimmed[i] == u'$')
    {
      ++i;
    }
    if (i < trimmed.size() && trimmed[i] == u'=')
    {
      if (const auto end = matchCallParen(trimmed, i + 1))
      {
        return Directive{
          .kind = DirectiveKind::CallParen,
          .argsStart = leading + *end,
          .sectionInsideQuotes = true,
        };
      }
    }
  }

  if (const auto end = matchCallParen(trimmed, 0))
  {
    return Directive{
      .kind = DirectiveKind::CallParen,
      .argsStart = leading + *end,
      .sectionInsideQuotes = true,
    };
  }

  // ^(call\s+)(?!\() — call\s*\( already matched above, so the backtracking
  // lookahead reduces to "one or more spaces, next char is not `(`".
  if (startsWith(trimmed, u"call") && trimmed.size() > 4 && isJsSpace(trimmed[4]))
  {
    const auto end = skipJsSpace(trimmed, 4);
    if (end >= trimmed.size() || trimmed[end] != u'(')
    {
      return Directive{
        .kind = DirectiveKind::CallSpace,
        .argsStart = leading + end,
        .sectionInsideQuotes = false,
      };
    }
  }
  return std::nullopt;
}

struct ParsedArgs
{
  bool modeNone = false;
  U16 filePath;
  bool inPath = false;
  bool atEndOfPath = false;
  bool inSuffix = false;
  bool pastSuffix = false;
  std::size_t suffixStart = 0;
  std::size_t suffixEnd = 0;
};

struct SuffixSlot
{
  bool inSuffix = false;
  bool pastSuffix = false;
  std::size_t suffixStart = 0;
  std::size_t suffixEnd = 0;
};

auto parseSuffixSlot(U16View line, std::size_t pathEnd, std::size_t cursor) -> SuffixSlot
{
  auto i = pathEnd;
  while (i < line.size() && (line[i] == u' ' || line[i] == u'\t'))
  {
    ++i;
  }
  const auto identStart = i;
  if (i >= line.size() || !isIdentStart(line[i]))
  {
    const bool inGap = cursor >= pathEnd
      && (cursor <= identStart || (identStart >= line.size() && cursor >= pathEnd));
    const bool onlyWhitespaceAfterPath = i >= line.size() || !isIdentPart(line[i]);
    if (cursor > pathEnd && onlyWhitespaceAfterPath && cursor <= i)
    {
      return {.inSuffix = true, .suffixStart = cursor, .suffixEnd = cursor};
    }
    if (inGap && cursor > pathEnd)
    {
      return {.inSuffix = true, .suffixStart = cursor, .suffixEnd = cursor};
    }
    return {
      .inSuffix = false,
      .pastSuffix = cursor > pathEnd,
      .suffixStart = cursor,
      .suffixEnd = cursor,
    };
  }

  while (i < line.size() && isIdentPart(line[i]))
  {
    ++i;
  }
  const auto identEnd = i;
  if (cursor < identStart)
  {
    return {.inSuffix = cursor > pathEnd, .suffixStart = cursor, .suffixEnd = cursor};
  }
  if (cursor <= identEnd)
  {
    return {.inSuffix = true, .suffixStart = identStart, .suffixEnd = identEnd};
  }
  return {.inSuffix = false, .pastSuffix = true, .suffixStart = identStart, .suffixEnd = identEnd};
}

auto withSuffix(U16 filePath, const SuffixSlot& suffix) -> ParsedArgs
{
  ParsedArgs parsed;
  parsed.filePath = std::move(filePath);
  parsed.inSuffix = suffix.inSuffix;
  parsed.pastSuffix = suffix.pastSuffix;
  parsed.suffixStart = suffix.suffixStart;
  parsed.suffixEnd = suffix.suffixEnd;
  return parsed;
}

auto parseQuotedArg(U16View line, std::size_t quoteIndex, std::size_t cursor, bool sectionInsideQuotes)
  -> ParsedArgs
{
  const auto quote = line[quoteIndex];
  const auto contentStart = quoteIndex + 1;
  std::optional<std::size_t> close;
  for (auto i = contentStart; i < line.size(); ++i)
  {
    if (line[i] == quote)
    {
      close = i;
      break;
    }
  }

  if (close && cursor > *close)
  {
    if (sectionInsideQuotes)
    {
      ParsedArgs parsed;
      parsed.modeNone = true;
      return parsed;
    }
    auto filePath = slashes16(stripQuotes16(line.substr(quoteIndex, *close + 1 - quoteIndex)));
    return withSuffix(std::move(filePath), parseSuffixSlot(line, *close + 1, cursor));
  }

  const auto contentEnd = close ? std::min(cursor, *close) : std::max(cursor, contentStart);
  const auto content = line.substr(contentStart, contentEnd - contentStart);
  std::size_t space = 0;
  while (space < content.size() && !isJsSpace(content[space]))
  {
    ++space;
  }
  if (space == content.size())
  {
    const auto sliceEnd = close ? *close + 1 : contentEnd;
    auto fullPath = slashes16(stripQuotes16(line.substr(quoteIndex, sliceEnd - quoteIndex)));
    ParsedArgs parsed;
    parsed.filePath = fullPath.empty() ? slashes16(U16(content)) : std::move(fullPath);
    parsed.inPath = true;
    parsed.atEndOfPath = cursor >= contentEnd && (!close || cursor <= *close);
    parsed.suffixStart = cursor;
    parsed.suffixEnd = cursor;
    return parsed;
  }

  auto filePath = slashes16(U16(content.substr(0, space)));
  auto j = space;
  while (j < content.size() && isJsSpace(content[j]))
  {
    ++j;
  }
  const auto identStart = contentStart + j;
  const auto identLimit = close ? *close : line.size();
  auto identEnd = identStart;
  while (identEnd < identLimit && isIdentPart(line[identEnd]))
  {
    ++identEnd;
  }

  ParsedArgs parsed;
  parsed.filePath = std::move(filePath);
  if (cursor < identStart)
  {
    parsed.inSuffix = true;
    parsed.suffixStart = cursor;
    parsed.suffixEnd = cursor;
    return parsed;
  }
  if (close && identEnd < *close && cursor > identEnd)
  {
    parsed.pastSuffix = true;
    parsed.suffixStart = identStart;
    parsed.suffixEnd = identEnd;
    return parsed;
  }
  parsed.inSuffix = true;
  parsed.suffixStart = identStart;
  parsed.suffixEnd = identEnd != 0 ? identEnd : cursor;
  return parsed;
}

auto emptyPathArgs(std::size_t cursor) -> ParsedArgs
{
  ParsedArgs parsed;
  parsed.inPath = true;
  parsed.atEndOfPath = true;
  parsed.suffixStart = cursor;
  parsed.suffixEnd = cursor;
  return parsed;
}

auto parseDirectiveArgs(U16View line, const Directive& directive, std::size_t cursor) -> ParsedArgs
{
  auto i = directive.argsStart;
  while (i < cursor && (line[i] == u' ' || line[i] == u'\t'))
  {
    ++i;
  }

  if (i >= cursor && i >= line.size())
  {
    return emptyPathArgs(cursor);
  }
  if (i < line.size() && (line[i] == u'"' || line[i] == u'\''))
  {
    return parseQuotedArg(line, i, cursor, directive.sectionInsideQuotes);
  }
  if (i >= cursor)
  {
    return emptyPathArgs(cursor);
  }

  // readBarePath (the quoted branch is unreachable here).
  auto end = i;
  while (end < line.size() && !isJsSpace(line[end]) && line[end] != u',' && line[end] != u';'
         && line[end] != u')')
  {
    ++end;
  }
  auto filePath = slashes16(stripQuotes16(line.substr(i, end - i)));
  if (cursor <= end)
  {
    ParsedArgs parsed;
    parsed.filePath = std::move(filePath);
    parsed.inPath = true;
    parsed.atEndOfPath = cursor == end || cursor == line.size()
      || (cursor < line.size() && isJsSpace(line[cursor]));
    parsed.suffixStart = cursor;
    parsed.suffixEnd = cursor;
    return parsed;
  }
  return withSuffix(std::move(filePath), parseSuffixSlot(line, end, cursor));
}

struct Analysis
{
  IncludeCallMode mode = IncludeCallMode::None;
  DirectiveKind directive = DirectiveKind::Include;
  std::string filePath;
  std::vector<std::string> sections;
  bool insertLeadingSpace = false;
  std::size_t replaceStart = 0;
  std::size_t replaceEnd = 0;
};

// Join like Node path.join(a, b, …): string concatenation, then normalize.
// Never lets an absolute `b` replace `a`.
auto joinLike(const std::filesystem::path& root, std::initializer_list<std::string_view> parts)
  -> std::filesystem::path
{
  auto text = root.u8string();
  for (const auto part : parts)
  {
    if (part.empty())
    {
      continue;
    }
    text += u8'/';
    text += std::u8string(reinterpret_cast<const char8_t*>(part.data()), part.size());
  }
  return std::filesystem::path(text).lexically_normal();
}

bool isDirectoryNoFollow(const std::filesystem::path& path)
{
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  return !ec && std::filesystem::is_directory(status);
}

auto pathToUtf8(const std::filesystem::path& path) -> std::string
{
  const auto text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

// server.js vertexRelativePathIsDirectory (custom name = detectCustomFolderName).
bool vertexRelativePathIsDirectory(const LibraryLayout& layout, std::string_view inputPath)
{
  const auto& root = layout.workspaceRoot();
  if (root.empty() || inputPath.empty())
  {
    return false;
  }
  auto relative = std::string(trim(replaceBackslashes(stripPathQuotes(inputPath))));
  if (relative.empty())
  {
    return false;
  }
  if (relative.front() == '$')
  {
    const auto slash = relative.find('/');
    const auto first = relative.substr(0, slash);
    if (const auto converted = layout.convertVertexPath(first))
    {
      relative = slash == std::string::npos ? *converted : *converted + relative.substr(slash);
    }
  }
  const std::filesystem::path candidates[] = {
    joinLike(root, {relative}),
    joinLike(root, {layout.customFolderName(), relative}),
    joinLike(root, {"system", relative}),
  };
  for (const auto& candidate : candidates)
  {
    if (isDirectoryNoFollow(candidate))
    {
      return true;
    }
  }
  return false;
}

// server.js getTargetSectionNamesForVertexPath.
auto targetSectionNamesForVertexPath(
  const IncludeCallCompletionRequest& request,
  std::string_view vertexPath) -> std::vector<std::string>
{
  const auto resolved = request.layout.resolveVertexFilePath(vertexPath);
  if (!resolved || !request.textForPath)
  {
    return {};
  }
  const auto text = request.textForPath(*resolved);
  if (!text || text->empty())
  {
    return {};
  }
  return uniqueTargetSectionNames(*text);
}

auto analyze(const IncludeCallCompletionRequest& request, U16View line, std::size_t cursor)
  -> Analysis
{
  Analysis analysis;
  const auto directive = matchDirective(line.substr(0, cursor));
  if (!directive)
  {
    return analysis;
  }
  analysis.directive = directive->kind;

  if (directive->kind == DirectiveKind::IncludedFrom)
  {
    auto plain = *directive;
    plain.sectionInsideQuotes = false;
    const auto parsed = parseDirectiveArgs(line, plain, cursor);
    analysis.mode = IncludeCallMode::Paths;
    analysis.filePath = utf16To8(parsed.filePath);
    return analysis;
  }

  const auto parsed = parseDirectiveArgs(line, *directive, cursor);
  if (parsed.modeNone || parsed.pastSuffix)
  {
    return analysis;
  }

  const auto filePath = utf16To8(parsed.filePath);
  bool canUseSections = false;
  if (!filePath.empty())
  {
    const bool isFile = request.layout.resolveVertexFilePath(filePath).has_value();
    canUseSections = isFile && !vertexRelativePathIsDirectory(request.layout, filePath);
  }
  auto names = canUseSections ? targetSectionNamesForVertexPath(request, filePath)
                              : std::vector<std::string>{};
  const bool invoked = request.triggerKind == kTriggerInvoked;

  if (!names.empty() && (parsed.inSuffix || (invoked && parsed.inPath && parsed.atEndOfPath)))
  {
    analysis.mode = IncludeCallMode::Sections;
    analysis.filePath = filePath;
    analysis.sections = std::move(names);
    analysis.insertLeadingSpace = parsed.inPath && !parsed.inSuffix;
    analysis.replaceStart = parsed.inSuffix ? parsed.suffixStart : cursor;
    analysis.replaceEnd = parsed.inSuffix ? parsed.suffixEnd : cursor;
    return analysis;
  }
  if (parsed.inSuffix)
  {
    return analysis;
  }
  analysis.mode = IncludeCallMode::Paths;
  analysis.filePath = filePath;
  return analysis;
}

// searchInCustomAndSystem's own Custom-folder name: first workspace-folder URI
// string that includes("custom"), last `/` segment, first `%3A` → `:`. Not
// decoded, not detectCustomFolderName.
auto listingCustomFolderName(const LibraryLayout& layout) -> std::string
{
  std::string name = "custom";
  for (const auto& folder : layout.folders())
  {
    const auto text = folder.uri.toString();
    if (text.find("custom") == std::string::npos)
    {
      continue;
    }
    const auto slash = text.find_last_of('/');
    name = slash == std::string::npos ? text : text.substr(slash + 1);
    std::replace(name.begin(), name.end(), '/', '\\');
    if (const auto at = name.find("%3A"); at != std::string::npos)
    {
      name.replace(at, 3, ":");
    }
  }
  return name;
}

bool endsWith(std::string_view text, std::string_view suffix)
{
  return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

// Approximates String.prototype.localeCompare for file names: case-folded
// first, lowercase before uppercase on ties.
bool localeLess(const std::string& a, const std::string& b)
{
  const auto la = toLowerAscii(a);
  const auto lb = toLowerAscii(b);
  if (la != lb)
  {
    return la < lb;
  }
  return a > b;
}

// server.js getDirectoryContents: directories as `name/`, files unless they
// end with an image/drawing extension or start with `D_` (case-sensitive).
auto directoryContents(const std::filesystem::path& dir) -> std::vector<std::string>
{
  std::vector<std::string> results;
  if (!isDirectoryNoFollow(dir))
  {
    return results;
  }
  std::error_code ec;
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
  {
    const auto name = pathToUtf8(it->path().filename());
    if (isDirectoryNoFollow(it->path()))
    {
      results.push_back(name + "/");
      continue;
    }
    constexpr std::string_view kSkipped[] = {
      ".vxm", ".ico", ".bmp", ".jpg", ".vxp", ".png", ".dwg", ".dxf"};
    bool skip = name.rfind("D_", 0) == 0;
    for (const auto suffix : kSkipped)
    {
      skip = skip || endsWith(name, suffix);
    }
    if (!skip)
    {
      results.push_back(name);
    }
  }
  std::sort(results.begin(), results.end(), [](const std::string& a, const std::string& b) {
    const bool aDir = endsWith(a, "/");
    const bool bDir = endsWith(b, "/");
    if (aDir != bDir)
    {
      return aDir;
    }
    return localeLess(a, b);
  });
  return results;
}

// server.js listIncludeDirectoryEntries + searchInCustomAndSystem.
auto listIncludeDirectoryEntries(const LibraryLayout& layout, std::string query)
  -> std::vector<std::string>
{
  const auto& root = layout.workspaceRoot();
  if (root.empty())
  {
    return {};
  }
  if (query.find('$') != std::string::npos)
  {
    const auto first = query.substr(0, query.find('/'));
    if (const auto converted = layout.convertVertexPath(first))
    {
      query.replace(0, first.size(), *converted);
    }
  }

  auto results = directoryContents(joinLike(root, {listingCustomFolderName(layout), query}));
  for (auto& item : directoryContents(joinLike(root, {"system", query})))
  {
    if (std::find(results.begin(), results.end(), item) == results.end())
    {
      results.push_back(std::move(item));
    }
  }
  return results;
}

// server.js joinVertexPath.
auto joinVertexPath(std::string_view directory, std::string_view name) -> std::string
{
  auto base = replaceBackslashes(directory);
  if (!base.empty() && base.back() == '/')
  {
    base.pop_back();
  }
  const auto leaf = replaceBackslashes(name);
  if (base.empty())
  {
    return leaf;
  }
  if (leaf.empty())
  {
    return base;
  }
  return base + "/" + leaf;
}

auto sectionItems(const Analysis& analysis, lsp::Position position)
  -> std::vector<lsp::CompletionItem>
{
  const lsp::Range range{
    .start = {.line = position.line, .character = static_cast<lsp::Uint>(analysis.replaceStart)},
    .end = {.line = position.line, .character = static_cast<lsp::Uint>(analysis.replaceEnd)},
  };
  std::vector<lsp::CompletionItem> items;
  items.reserve(analysis.sections.size());
  for (const auto& name : analysis.sections)
  {
    const auto insert = (analysis.insertLeadingSpace ? std::string(" ") : std::string()) + name;
    lsp::CompletionItem item;
    item.label = name;
    item.kind = lsp::CompletionItemKind::EnumMember;
    item.detail = "section";
    item.sortText = name;
    item.filterText = name;
    item.insertText = insert;
    item.textEdit = lsp::TextEdit{.range = range, .newText = insert};
    items.push_back(std::move(item));
  }
  return items;
}

auto pathItems(const IncludeCallCompletionRequest& request, const Analysis& analysis)
  -> std::vector<lsp::CompletionItem>
{
  std::vector<lsp::CompletionItem> items;
  if (analysis.filePath.empty())
  {
    for (const auto& entry : catalog::keywordCatalog())
    {
      if (entry.label.empty() || entry.label.front() != '$')
      {
        continue;
      }
      lsp::CompletionItem item;
      item.label = std::string(entry.label);
      item.kind = lsp::CompletionItemKind::Folder;
      item.sortText = "0" + item.label;
      items.push_back(std::move(item));
    }
    return items;
  }

  const bool attachSections = analysis.directive != DirectiveKind::IncludedFrom;
  for (auto& name : listIncludeDirectoryEntries(request.layout, analysis.filePath))
  {
    lsp::CompletionItem item;
    if (endsWith(name, "/"))
    {
      item.kind = lsp::CompletionItemKind::Folder;
      item.sortText = "0" + name;
      item.insertText = name;
      item.label = std::move(name);
      items.push_back(std::move(item));
      continue;
    }
    item.kind = lsp::CompletionItemKind::File;
    item.sortText = "1" + name;
    if (attachSections)
    {
      const auto names =
        targetSectionNamesForVertexPath(request, joinVertexPath(analysis.filePath, name));
      if (!names.empty())
      {
        item.detail = std::to_string(names.size()) + (names.size() == 1 ? " section" : " sections");
        std::string markdown;
        for (std::size_t i = 0; i < names.size(); ++i)
        {
          if (i)
          {
            markdown += '\n';
          }
          markdown += "- `" + names[i] + "`";
        }
        item.documentation = lsp::MarkupContent{
          .kind = lsp::MarkupKind::Markdown,
          .value = std::move(markdown),
        };
      }
    }
    item.label = std::move(name);
    items.push_back(std::move(item));
  }
  return items;
}

auto lineAndCursor(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<std::pair<U16, std::size_t>>
{
  if (snapshot.languageId != "vertexadt")
  {
    return std::nullopt;
  }
  const auto row = lineAt(snapshot.text, position.line);
  if (!row)
  {
    return std::nullopt;
  }
  auto line = utf8To16(*row);
  const auto cursor = std::min<std::size_t>(position.character, line.size());
  return std::pair{std::move(line), cursor};
}

} // namespace

bool isIncludeCallCompletionSite(const Snapshot& snapshot, lsp::Position position)
{
  const auto located = lineAndCursor(snapshot, position);
  if (!located)
  {
    return false;
  }
  const auto& [line, cursor] = *located;
  return matchDirective(U16View(line).substr(0, cursor)).has_value();
}

auto includeCallCompletionItems(const IncludeCallCompletionRequest& request)
  -> std::vector<lsp::CompletionItem>
{
  const auto located = lineAndCursor(request.snapshot, request.position);
  if (!located)
  {
    return {};
  }
  const auto& [line, cursor] = *located;
  const auto analysis = analyze(request, line, cursor);
  switch (analysis.mode)
  {
  case IncludeCallMode::Sections:
    return sectionItems(analysis, request.position);
  case IncludeCallMode::Paths:
    return pathItems(request, analysis);
  case IncludeCallMode::None:
    break;
  }
  return {};
}

} // namespace vertexadt
