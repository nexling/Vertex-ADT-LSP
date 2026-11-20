#include "piece_setup_sets.h"

#include "form_name_completion.h"
#include "text_util.h"
#include "vxsettings_xml.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

// ---- settings conditions -------------------------------------------------

bool iequals(std::string_view a, std::string_view b)
{
  return a.size() == b.size() && toLowerAscii(a) == toLowerAscii(b);
}

auto splitWords(std::string_view text) -> std::vector<std::string_view>
{
  std::vector<std::string_view> words;
  std::size_t i = 0;
  while (i < text.size())
  {
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
    {
      ++i;
    }
    const auto start = i;
    while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])))
    {
      ++i;
    }
    if (i > start)
    {
      words.push_back(text.substr(start, i - start));
    }
  }
  return words;
}

// ---- vxsettings model ------------------------------------------------------

struct SettingsData
{
  std::string guid;
  std::string key;
  std::string data;
  std::optional<std::string> config;
  std::optional<std::string> application;
  std::optional<std::string> units;
  std::optional<std::string> language;
  bool custom = false;
};

struct SettingsItem
{
  std::string guid;
  std::string summary;
  std::vector<SettingsData> data;
  std::vector<SettingsItem> children;
  bool custom = false;
};

auto optionalAttribute(const XmlElement& element, std::string_view key) -> std::optional<std::string>
{
  if (const auto value = element.attribute(key))
  {
    return std::string(*value);
  }
  return std::nullopt;
}

auto childText(const XmlElement& element, std::string_view name) -> std::string
{
  const auto* child = element.child(name);
  return child ? std::string(trim(child->text)) : std::string{};
}

auto toData(const XmlElement& element, bool custom) -> SettingsData
{
  return SettingsData{
    .guid = std::string(element.attribute("GUID").value_or("")),
    .key = childText(element, "Key"),
    .data = childText(element, "Data"),
    .config = optionalAttribute(element, "Config"),
    .application = optionalAttribute(element, "Application"),
    .units = optionalAttribute(element, "Units"),
    .language = optionalAttribute(element, "Language"),
    .custom = custom,
  };
}

auto toItem(const XmlElement& element, bool custom) -> SettingsItem
{
  SettingsItem item;
  item.guid = std::string(element.attribute("GUID").value_or(""));
  item.custom = custom;
  for (const auto& child : element.children)
  {
    if (child.name == "Summary")
    {
      item.summary = std::string(trim(child.text));
    }
    else if (child.name == "DataItem")
    {
      item.data.push_back(toData(child, custom));
    }
    else if (child.name == "SetupItem")
    {
      item.children.push_back(toItem(child, custom));
    }
  }
  return item;
}

bool sameGuid(std::string_view a, std::string_view b)
{
  return !a.empty() && iequals(a, b);
}

auto findPieceSetupRoot(const XmlElement& element) -> const XmlElement*
{
  for (const auto& child : element.children)
  {
    if (child.name != "SetupItem")
    {
      continue;
    }
    if (const auto* summary = child.child("Summary"); summary && trim(summary->text) == "piece.setup")
    {
      return &child;
    }
    if (const auto* found = findPieceSetupRoot(child))
    {
      return found;
    }
  }
  return nullptr;
}

auto findSetupItemByGuid(const XmlElement& element, std::string_view guid) -> const XmlElement*
{
  for (const auto& child : element.children)
  {
    if (child.name != "SetupItem")
    {
      continue;
    }
    if (sameGuid(child.attribute("GUID").value_or(""), guid))
    {
      return &child;
    }
    if (const auto* found = findSetupItemByGuid(child, guid))
    {
      return found;
    }
  }
  return nullptr;
}

// Custom overlay: matching GUIDs merge/replace (whatever the `Custom` value),
// the rest are added. An added DataItem goes before the trailing function_end.
void applyOverlay(SettingsItem& target, const XmlElement& overlay)
{
  for (const auto& child : overlay.children)
  {
    if (child.name == "DataItem")
    {
      auto data = toData(child, true);
      const auto match = std::find_if(target.data.begin(), target.data.end(), [&](const auto& existing) {
        return sameGuid(existing.guid, data.guid);
      });
      if (match != target.data.end())
      {
        *match = std::move(data);
        continue;
      }
      auto at = target.data.end();
      for (auto it = target.data.rbegin(); it != target.data.rend(); ++it)
      {
        if (iequals(it->key, "function_end"))
        {
          at = std::prev(it.base());
          break;
        }
      }
      target.data.insert(at, std::move(data));
    }
    else if (child.name == "SetupItem")
    {
      const auto guid = child.attribute("GUID").value_or("");
      const auto match = std::find_if(target.children.begin(), target.children.end(), [&](const auto& existing) {
        return sameGuid(existing.guid, guid);
      });
      if (match != target.children.end())
      {
        applyOverlay(*match, child);
      }
      else
      {
        target.children.push_back(toItem(child, true));
      }
    }
  }
}

bool isActive(const SettingsData& data, const VxSettingsConfig& config)
{
  const auto matches = [](const std::optional<std::string>& expression, const std::string& active) {
    return !expression || settingsConditionMatches(*expression, active);
  };
  return matches(data.config, config.config) && matches(data.application, config.application)
    && matches(data.units, config.units) && matches(data.language, config.language);
}

void collectFromItem(
  const SettingsItem& item,
  const VxSettingsConfig& config,
  std::vector<PieceSet>& out,
  std::unordered_set<std::string>& seen)
{
  if (trim(item.summary).starts_with("piece.setup"))
  {
    std::optional<PieceSet> open;
    bool inFragment = false;
    const auto close = [&]() {
      if (open && seen.insert(open->name).second)
      {
        out.push_back(std::move(*open));
      }
      open.reset();
    };
    for (const auto& data : item.data)
    {
      if (!isActive(data, config))
      {
        continue;
      }
      const auto key = toLowerAscii(data.key);
      if (key == "function_begin")
      {
        if (open || inFragment)
        {
          continue;
        }
        const auto words = splitWords(data.data);
        if (words.empty())
        {
          continue;
        }
        PieceSet set;
        set.name = std::string(words.front());
        const auto rest = std::string_view(data.data).substr(
          static_cast<std::size_t>(words.front().data() - data.data.data()) + words.front().size());
        // Multi-word descriptions are quoted (`FRAMING.WALL "WALL FRAMING"`);
        // the quotes delimit, they are not part of the description.
        auto description = trim(rest);
        if (description.size() >= 2 && description.front() == '"' && description.back() == '"')
        {
          description = trim(description.substr(1, description.size() - 2));
        }
        set.description = std::string(description);
        set.custom = item.custom;
        set.nameCustom = data.custom;
        open = std::move(set);
      }
      else if (key == "function_end")
      {
        close();
      }
      else if (key == "set_begin")
      {
        inFragment = true;
      }
      else if (key == "set_end")
      {
        inFragment = false;
      }
      else if (open && !inFragment && (key == "include_set" || key == "param"))
      {
        auto& rows = key == "param" ? open->params : open->includeSets;
        rows.push_back({.text = data.data, .custom = data.custom});
      }
    }
    close();
  }
  for (const auto& child : item.children)
  {
    collectFromItem(child, config, out, seen);
  }
}

// ---- disk cache ------------------------------------------------------------

struct FileStamp
{
  std::filesystem::path path;
  bool exists = false;
  std::uintmax_t size = 0;
  std::filesystem::file_time_type modified{};

  bool operator==(const FileStamp&) const = default;
};

auto stampOf(const std::filesystem::path& path) -> FileStamp
{
  FileStamp stamp{.path = path};
  if (path.empty())
  {
    return stamp;
  }
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec)
  {
    return stamp;
  }
  stamp.size = std::filesystem::file_size(path, ec);
  if (ec)
  {
    return stamp;
  }
  stamp.modified = std::filesystem::last_write_time(path, ec);
  stamp.exists = !ec;
  return stamp;
}

auto readFile(const FileStamp& stamp) -> std::string
{
  if (!stamp.exists)
  {
    return {};
  }
  std::ifstream in(stamp.path, std::ios::binary);
  if (!in)
  {
    return {};
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return std::move(buffer).str();
}

// ---- set slot sites ------------------------------------------------------

struct CommandSpec
{
  std::u16string_view name;
  PieceSetCommand command;
};

constexpr std::array<CommandSpec, 2> kCommands{{
  {u"new_piece", PieceSetCommand::NewPiece},
  {u"set_piece_set", PieceSetCommand::SetPieceSet},
}};
constexpr std::size_t kNewPieceSetArgument = 5;

bool isQuote(char16_t ch)
{
  return ch == u'"' || ch == u'\'';
}

// `!` outside quotes in [0, cursor).
bool cursorInComment(U16View line, std::size_t cursor)
{
  char16_t quote = 0;
  for (std::size_t i = 0; i < cursor; ++i)
  {
    const auto ch = line[i];
    if (quote)
    {
      if (ch == quote)
      {
        quote = 0;
      }
      continue;
    }
    if (isQuote(ch))
    {
      quote = ch;
    }
    else if (ch == u'!')
    {
      return true;
    }
  }
  return false;
}

struct CommandMatch
{
  PieceSetCommand command = PieceSetCommand::NewPiece;
  std::size_t openParen = 0;
};

// `name\s*(` at `at` for one of kCommands, word boundary before,
// case-insensitive.
auto matchCommandAt(U16View line, std::size_t at) -> std::optional<CommandMatch>
{
  if (at > 0 && isJsWord(line[at - 1]))
  {
    return std::nullopt;
  }
  for (const auto& spec : kCommands)
  {
    if (at + spec.name.size() > line.size())
    {
      continue;
    }
    bool same = true;
    for (std::size_t k = 0; k < spec.name.size() && same; ++k)
    {
      same = asciiLower(line[at + k]) == spec.name[k];
    }
    if (!same)
    {
      continue;
    }
    const auto paren = skipJsSpace(line, at + spec.name.size());
    if (paren < line.size() && line[paren] == u'(')
    {
      return CommandMatch{.command = spec.command, .openParen = paren};
    }
  }
  return std::nullopt;
}

auto lastCommandBefore(U16View line, std::size_t cursor) -> std::optional<CommandMatch>
{
  std::optional<CommandMatch> found;
  for (std::size_t at = 0; at < cursor; ++at)
  {
    if (const auto match = matchCommandAt(line, at); match && match->openParen < cursor)
    {
      found = match;
      at = match->openParen;
    }
  }
  return found;
}

struct ArgSpan
{
  std::size_t start = 0;
  std::size_t end = 0;
};

// Top-level comma split from `from`, skipping nested parens and quoted text.
// insideParens: stops at the `)` closing the call (its index in closeParen),
// else at EOL. After the parens: stops at a top-level `)` or a `!` comment.
auto splitArguments(U16View line, std::size_t from, bool insideParens, std::ptrdiff_t& closeParen)
  -> std::vector<ArgSpan>
{
  std::vector<ArgSpan> args{{.start = from, .end = from}};
  closeParen = -1;
  int depth = insideParens ? 1 : 0;
  char16_t quote = 0;
  for (std::size_t i = from; i < line.size(); ++i)
  {
    const auto ch = line[i];
    if (quote)
    {
      if (ch == quote)
      {
        quote = 0;
      }
      continue;
    }
    if (isQuote(ch))
    {
      quote = ch;
    }
    else if (ch == u'(')
    {
      ++depth;
    }
    else if (ch == u')')
    {
      if (depth == 0 || --depth == 0)
      {
        args.back().end = i;
        closeParen = static_cast<std::ptrdiff_t>(i);
        return args;
      }
    }
    else if (ch == u'!' && !insideParens && depth == 0)
    {
      args.back().end = i;
      return args;
    }
    else if (ch == u',' && depth == (insideParens ? 1 : 0))
    {
      args.back().end = i;
      args.push_back({.start = i + 1, .end = i + 1});
    }
  }
  args.back().end = line.size();
  return args;
}

bool isSpaceOrTab(char16_t ch)
{
  return ch == u' ' || ch == u'\t';
}

// The slot `arg` holds the cursor: inside its quotes, or only spaces/tabs
// before the cursor.
auto slotContext(U16View line, const ArgSpan& arg, std::size_t cursor, PieceSetCommand command)
  -> std::optional<PieceSetSite>
{
  if (cursor < arg.start || cursor > arg.end)
  {
    return std::nullopt;
  }
  auto i = arg.start;
  while (i < cursor && isSpaceOrTab(line[i]))
  {
    ++i;
  }
  if (i >= cursor)
  {
    // Right before an existing quoted string: inserting `"NAME"` would glue.
    auto next = cursor;
    while (next < arg.end && isSpaceOrTab(line[next]))
    {
      ++next;
    }
    if (next < arg.end && isQuote(line[next]))
    {
      return std::nullopt;
    }
    return PieceSetSite{
      .command = command,
      .insideQuotes = false,
      .leadingSpace = cursor > 0 && line[cursor - 1] == u')',
      .replaceStart = cursor,
      .replaceEnd = cursor,
    };
  }
  if (!isQuote(line[i]))
  {
    return std::nullopt;
  }
  const auto contentStart = i + 1;
  if (cursor < contentStart)
  {
    return std::nullopt;
  }
  const auto close = line.find(line[i], contentStart);
  if (close != U16View::npos && cursor > close)
  {
    return std::nullopt;
  }
  const auto contentEnd = close != U16View::npos ? close : line.size();
  return PieceSetSite{
    .command = command,
    .insideQuotes = true,
    .replaceStart = contentStart,
    .replaceEnd = close != U16View::npos ? close : cursor,
    .content = utf16To8(line.substr(contentStart, contentEnd - contentStart)),
  };
}

// A literal number or `+` sum of numbers (spaces/tabs allowed); else nullopt.
auto parseLiteralMode(U16View text) -> std::optional<long long>
{
  long long total = 0;
  bool any = false;
  std::size_t i = 0;
  while (true)
  {
    while (i < text.size() && isSpaceOrTab(text[i]))
    {
      ++i;
    }
    const auto start = i;
    long long value = 0;
    while (i < text.size() && text[i] >= u'0' && text[i] <= u'9')
    {
      if (i - start >= 15)
      {
        return std::nullopt;
      }
      value = value * 10 + (text[i] - u'0');
      ++i;
    }
    if (i == start)
    {
      return std::nullopt;
    }
    total += value;
    any = true;
    while (i < text.size() && isSpaceOrTab(text[i]))
    {
      ++i;
    }
    if (i >= text.size())
    {
      break;
    }
    if (text[i] != u'+')
    {
      return std::nullopt;
    }
    ++i;
  }
  return any ? std::optional<long long>(total) : std::nullopt;
}

auto locate(const Snapshot& snapshot, lsp::Position position)
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

auto lineMarkdown(const PieceSetLine& row) -> std::string
{
  return "- `" + row.text + "`" + (row.custom ? " *(custom)*" : "") + "\n";
}

} // namespace

bool settingsConditionMatches(std::string_view expression, std::string_view active)
{
  const auto words = splitWords(expression);
  if (words.empty() || trim(active).empty())
  {
    return true;
  }
  const auto activeName = trim(active);
  std::optional<bool> result;
  std::string_view pendingOperator;
  std::size_t i = 0;
  while (i < words.size())
  {
    bool negate = false;
    while (i < words.size() && iequals(words[i], "not"))
    {
      negate = !negate;
      ++i;
    }
    if (i >= words.size() || iequals(words[i], "and") || iequals(words[i], "or"))
    {
      return true;
    }
    const bool term = iequals(words[i], activeName) != negate;
    ++i;
    if (!result)
    {
      if (!pendingOperator.empty())
      {
        return true;
      }
      result = term;
    }
    else if (iequals(pendingOperator, "and"))
    {
      result = *result && term;
    }
    else
    {
      result = *result || term;
    }
    pendingOperator = {};
    if (i < words.size())
    {
      if (!iequals(words[i], "and") && !iequals(words[i], "or"))
      {
        return true;
      }
      pendingOperator = words[i];
      ++i;
      if (i >= words.size())
      {
        return true;
      }
    }
  }
  return result.value_or(true);
}

auto parseVxSettingsConfig(std::string_view xml) -> VxSettingsConfig
{
  const auto root = parseXml(xml);
  if (!root)
  {
    return {};
  }
  return VxSettingsConfig{
    .application = childText(*root, "Application"),
    .config = childText(*root, "Config"),
    .units = childText(*root, "Units"),
    .language = childText(*root, "Language"),
  };
}

auto collectPieceSets(
  std::string_view systemXml,
  std::string_view customXml,
  const VxSettingsConfig& config) -> std::vector<PieceSet>
{
  const auto system = parseXml(systemXml);
  if (!system)
  {
    return {};
  }
  const auto* rootElement = findPieceSetupRoot(*system);
  if (!rootElement)
  {
    return {};
  }
  auto root = toItem(*rootElement, false);
  if (!customXml.empty())
  {
    if (const auto custom = parseXml(customXml))
    {
      if (const auto* overlay = findSetupItemByGuid(*custom, root.guid))
      {
        applyOverlay(root, *overlay);
      }
    }
  }
  std::vector<PieceSet> sets;
  std::unordered_set<std::string> seen;
  for (const auto& child : root.children)
  {
    collectFromItem(child, config, sets, seen);
  }
  return sets;
}

auto resolvePieceSetSources(const LibraryLayout& layout) -> PieceSetSources
{
  PieceSetSources sources;
  const auto systemPrograms = resolveLibrarySubdirectory(LibraryTree::System, layout, "programs");
  const auto customPrograms = resolveLibrarySubdirectory(LibraryTree::Custom, layout, "programs");
  if (!systemPrograms.empty())
  {
    sources.system = systemPrograms / "vxsettings.xml";
    sources.config = systemPrograms / "vxsettings_config.xml";
  }
  if (!customPrograms.empty())
  {
    sources.custom = customPrograms / "vxsettings.xml";
  }
  return sources;
}

auto loadPieceSets(const PieceSetSources& sources) -> std::shared_ptr<const std::vector<PieceSet>>
{
  static std::mutex mutex;
  static std::vector<FileStamp> cachedKey;
  static std::shared_ptr<const std::vector<PieceSet>> cached;

  std::vector<FileStamp> key{stampOf(sources.system), stampOf(sources.custom), stampOf(sources.config)};
  const auto lock = std::lock_guard(mutex);
  if (cached && key == cachedKey)
  {
    return cached;
  }
  const auto config = parseVxSettingsConfig(readFile(key[2]));
  cached = std::make_shared<const std::vector<PieceSet>>(
    collectPieceSets(readFile(key[0]), readFile(key[1]), config));
  cachedKey = std::move(key);
  return cached;
}

auto analyzePieceSetSite(U16View line, std::size_t cursor) -> std::optional<PieceSetSite>
{
  cursor = std::min(cursor, line.size());
  if (cursorInComment(line, cursor))
  {
    return std::nullopt;
  }
  const auto match = lastCommandBefore(line, cursor);
  if (!match)
  {
    return std::nullopt;
  }
  std::ptrdiff_t closeParen = -1;
  const auto inside = splitArguments(line, match->openParen + 1, true, closeParen);
  if (match->command == PieceSetCommand::NewPiece)
  {
    if (closeParen >= 0 && static_cast<std::ptrdiff_t>(cursor) > closeParen)
    {
      return std::nullopt;
    }
    if (inside.size() <= kNewPieceSetArgument)
    {
      return std::nullopt;
    }
    return slotContext(line, inside[kNewPieceSetArgument], cursor, match->command);
  }
  if (closeParen < 0 || static_cast<std::ptrdiff_t>(cursor) <= closeParen)
  {
    return std::nullopt;
  }
  std::ptrdiff_t afterClose = -1;
  const auto after = splitArguments(line, static_cast<std::size_t>(closeParen) + 1, false, afterClose);
  auto site = slotContext(line, after[0], cursor, match->command);
  if (site && after.size() > 1)
  {
    site->mode = parseLiteralMode(line.substr(after[1].start, after[1].end - after[1].start));
  }
  return site;
}

auto pieceSetSite(const Snapshot& snapshot, lsp::Position position) -> std::optional<PieceSetSite>
{
  const auto located = locate(snapshot, position);
  if (!located)
  {
    return std::nullopt;
  }
  return analyzePieceSetSite(located->first, located->second);
}

auto pieceSetMarkdown(const PieceSet& set) -> std::string
{
  std::string markdown = "**piece.setup " + set.name + "**";
  if (set.custom)
  {
    markdown += " *(custom set)*";
  }
  else if (set.nameCustom)
  {
    markdown += " *(custom)*";
  }
  markdown += "\n\n";
  if (!set.description.empty())
  {
    markdown += "Description: " + set.description + "\n\n";
  }
  if (!set.includeSets.empty())
  {
    markdown += "include_set:\n";
    for (const auto& row : set.includeSets)
    {
      markdown += lineMarkdown(row);
    }
    markdown += "\n";
  }
  if (!set.params.empty())
  {
    markdown += "param:\n";
    for (const auto& row : set.params)
    {
      markdown += lineMarkdown(row);
    }
  }
  while (!markdown.empty() && markdown.back() == '\n')
  {
    markdown.pop_back();
  }
  return markdown;
}

auto pieceSetCompletionItems(
  const PieceSetSite& site,
  lsp::Position position,
  const std::vector<PieceSet>& sets) -> std::vector<lsp::CompletionItem>
{
  const lsp::Range range{
    .start = {.line = position.line, .character = static_cast<lsp::Uint>(site.replaceStart)},
    .end = {.line = position.line, .character = static_cast<lsp::Uint>(site.replaceEnd)},
  };
  std::vector<lsp::CompletionItem> items;
  items.reserve(sets.size());
  for (std::size_t index = 0; index < sets.size(); ++index)
  {
    const auto& set = sets[index];
    // File order on ties (Custom-added sets last, as merged).
    auto sortKey = std::to_string(index);
    sortKey.insert(0, 6 - std::min<std::size_t>(6, sortKey.size()), '0');
    lsp::CompletionItem item;
    item.label = set.name;
    item.kind = lsp::CompletionItemKind::Constant;
    item.detail = set.name;
    item.documentation = lsp::MarkupContent{
      .kind = lsp::MarkupKind::Markdown,
      .value = pieceSetMarkdown(set),
    };
    item.filterText = set.name;
    item.sortText = sortKey;
    const auto insert = site.insideQuotes
      ? set.name
      : std::string(site.leadingSpace ? " " : "") + "\"" + set.name + "\"";
    item.insertText = insert;
    item.textEdit = lsp::TextEdit{.range = range, .newText = insert};
    items.push_back(std::move(item));
  }
  return items;
}

auto findPieceSetForSite(const PieceSetSite& site, const std::vector<PieceSet>& sets)
  -> const PieceSet*
{
  if (!site.insideQuotes)
  {
    return nullptr;
  }
  const auto wanted = trim(site.content);
  if (wanted.empty())
  {
    return nullptr;
  }
  const auto byName = [&]() -> const PieceSet* {
    for (const auto& set : sets)
    {
      if (set.name == wanted)
      {
        return &set;
      }
    }
    return nullptr;
  };
  const auto byDescription = [&]() -> const PieceSet* {
    for (const auto& set : sets)
    {
      if (set.description == wanted)
      {
        return &set;
      }
    }
    return nullptr;
  };
  if (site.command == PieceSetCommand::NewPiece)
  {
    return byName();
  }
  // set_piece_set mode bit 1: read by piece.setup function, else by description.
  if (site.mode)
  {
    return (*site.mode & 1) ? byName() : byDescription();
  }
  if (const auto* set = byName())
  {
    return set;
  }
  return byDescription();
}

auto pieceSetHover(const Snapshot& snapshot, lsp::Position position, const LibraryLayout& layout)
  -> std::optional<std::string>
{
  const auto site = pieceSetSite(snapshot, position);
  if (!site || !site->insideQuotes || trim(site->content).empty())
  {
    return std::nullopt;
  }
  const auto sets = loadPieceSets(resolvePieceSetSources(layout));
  if (const auto* set = findPieceSetForSite(*site, *sets))
  {
    return pieceSetMarkdown(*set);
  }
  return std::nullopt;
}

} // namespace vertexadt
