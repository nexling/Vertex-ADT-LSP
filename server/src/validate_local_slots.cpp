#include "validate.h"

#include "local_slot_rename.h"
#include "utf16_text.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace vertexadt {
namespace {

const std::regex& assignRe()
{
  static const std::regex re{R"re(\b[A-Za-z][A-Za-z0-9_-]*\b(?=\s*=))re"};
  return re;
}

const std::regex& assignLettersRe()
{
  static const std::regex re{R"re(\b[A-Za-z]*\b(?!$)(?=\s*=))re"};
  return re;
}

const std::regex& localSlotsLineRe()
{
  static const std::regex re{R"re(^@local_slots\s+(.+)$)re", std::regex::icase};
  return re;
}

const std::regex& localPrefixRe()
{
  static const std::regex re{R"re(^l(\d+))re"};
  return re;
}

const std::regex& localBaseRe()
{
  static const std::regex re{R"re(^l\d+([A-Za-z][A-Za-z0-9_-]*\$?)$)re"};
  return re;
}

const std::regex& simpleNameRe()
{
  static const std::regex re{R"re(^[A-Za-z][A-Za-z0-9_-]*\$?$)re"};
  return re;
}

struct ManifestSlot
{
  std::string name;
  int line = 0;
  std::string lineText;
};

struct LiveDecl
{
  std::string varName;
  int number = 0;
  int line = 0;
  std::string lineText;
  std::string sectionName;
  std::optional<std::string> variableType;
};

struct TypeWriteRow
{
  std::string name;
  std::string type;
  int line = 0;
};

bool isValueType(const std::optional<std::string>& type)
{
  return type && toLowerAscii(*type) == "value";
}

auto hoverTypeOnLine(std::string_view line) -> std::optional<std::string>
{
  const auto comment = captureHoverComment(line);
  if (!comment)
  {
    return std::nullopt;
  }
  const auto open = comment->find('[');
  const auto close = comment->find(']', open);
  if (open == std::string::npos || close == std::string::npos || close <= open)
  {
    return std::nullopt;
  }
  return toLowerAscii(trim(std::string_view(*comment).substr(open + 1, close - open - 1)));
}

auto assignmentName(std::string_view fragment) -> std::optional<std::string>
{
  std::cmatch match;
  const char* begin = fragment.data();
  const char* end = begin + fragment.size();
  if (std::regex_search(begin, end, match, assignRe()))
  {
    return match[0].str();
  }
  if (std::regex_search(begin, end, match, assignLettersRe()) && !match[0].str().empty())
  {
    return match[0].str();
  }
  return std::nullopt;
}

auto declarationNumber(std::string_view fragment) -> std::optional<int>
{
  const auto eq = fragment.find('=');
  if (eq == std::string_view::npos)
  {
    return std::nullopt;
  }
  auto value = std::string(trim(fragment.substr(eq + 1)));
  if (value.empty())
  {
    return std::nullopt;
  }
  std::size_t idx = 0;
  try
  {
    const auto number = std::stoi(value, &idx);
    if (idx == 0)
    {
      return std::nullopt;
    }
    while (idx < value.size() && std::isspace(static_cast<unsigned char>(value[idx])))
    {
      ++idx;
    }
    if (idx != value.size())
    {
      return std::nullopt;
    }
    return number;
  }
  catch (...)
  {
    return std::nullopt;
  }
}

auto collectTypeWrites(const std::vector<std::string_view>& lines) -> std::vector<TypeWriteRow>
{
  std::vector<TypeWriteRow> types;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    if (auto type = hoverTypeOnLine(lines[static_cast<std::size_t>(i)]))
    {
      static const std::regex nameRe{R"re(\b([A-Za-z][A-Za-z0-9_-]*)\s*=)re"};
      std::cmatch match;
      const auto line = lines[static_cast<std::size_t>(i)];
      if (std::regex_search(line.data(), line.data() + line.size(), match, nameRe))
      {
        types.push_back(TypeWriteRow{.name = match[1].str(), .type = *type, .line = i});
      }
    }
  }
  return types;
}

auto typeForDeclaration(
  std::string_view varName,
  int lineIndex,
  std::string_view line,
  const std::vector<TypeWriteRow>& types) -> std::optional<std::string>
{
  if (auto hover = hoverTypeOnLine(line))
  {
    return hover;
  }
  std::optional<std::string> closest;
  int closestLine = -1;
  for (const auto& write : types)
  {
    if (write.name == varName && write.line <= lineIndex && write.line >= closestLine)
    {
      closest = write.type;
      closestLine = write.line;
    }
  }
  return closest;
}

auto declarationBlocks(const std::vector<std::string_view>& lines)
  -> std::vector<std::pair<int, int>>
{
  std::vector<std::pair<int, int>> blocks;
  int open = -1;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto trimmed = trim(lines[static_cast<std::size_t>(i)]);
    if (trimmed.starts_with("@declarationnumbercheckstart"))
    {
      open = i;
    }
    else if (trimmed.starts_with("@declarationnumbercheckend") && open != -1)
    {
      blocks.emplace_back(open, i);
      open = -1;
    }
  }
  return blocks;
}

template<typename Fn>
void forEachDeclarationFragment(
  const std::vector<std::string_view>& lines,
  const std::vector<NamedSection>& sections,
  Fn&& fn)
{
  for (const auto& [start, end] : declarationBlocks(lines))
  {
    for (int i = start + 1; i < end; ++i)
    {
      const auto sectionName = sectionNameForLine(i, sections);
      if (isVerificationIgnoredSection(sectionName))
      {
        continue;
      }
      const auto line = lines[static_cast<std::size_t>(i)];
      std::size_t fragStart = 0;
      while (fragStart <= line.size())
      {
        const auto semi = line.find(';', fragStart);
        const auto fragment = semi == std::string_view::npos
          ? line.substr(fragStart)
          : line.substr(fragStart, semi - fragStart);
        fragStart = semi == std::string_view::npos ? line.size() + 1 : semi + 1;
        if (isBangComment(fragment))
        {
          continue;
        }
        fn(i, sectionName, line, fragment);
      }
    }
  }
}

auto extractDeclarationsFirstWin(
  const std::vector<std::string_view>& lines,
  const std::vector<NamedSection>& sections,
  const std::vector<TypeWriteRow>& types) -> std::vector<LiveDecl>
{
  std::vector<LiveDecl> declarations;
  std::unordered_set<std::string> seen;
  forEachDeclarationFragment(
    lines,
    sections,
    [&](int lineIndex, const std::string& sectionName, std::string_view line, std::string_view fragment) {
      const auto name = assignmentName(fragment);
      const auto key = sectionName + "::" + (name ? *name : std::string{});
      if (!name || !seen.insert(key).second)
      {
        return;
      }
      const auto number = declarationNumber(fragment);
      if (!number || *number <= 0)
      {
        return;
      }
      declarations.push_back(LiveDecl{
        .varName = *name,
        .number = *number,
        .line = lineIndex,
        .lineText = std::string(line),
        .sectionName = sectionName,
        .variableType = typeForDeclaration(*name, lineIndex, line, types),
      });
    });
  return declarations;
}

auto parseManifest(const std::vector<std::string_view>& lines, const std::vector<NamedSection>& sections)
  -> std::vector<ManifestSlot>
{
  std::vector<ManifestSlot> slots;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    if (lineInVerificationIgnoredSection(i, sections))
    {
      continue;
    }
    const auto trimmed = std::string(trim(lines[static_cast<std::size_t>(i)]));
    if (trimmed.starts_with("!"))
    {
      continue;
    }
    std::smatch match;
    if (!std::regex_match(trimmed, match, localSlotsLineRe()))
    {
      continue;
    }
    std::string rest = match[1].str();
    std::size_t start = 0;
    while (start <= rest.size())
    {
      const auto comma = rest.find(',', start);
      const auto part = comma == std::string::npos
        ? std::string_view(rest).substr(start)
        : std::string_view(rest).substr(start, comma - start);
      start = comma == std::string::npos ? rest.size() + 1 : comma + 1;
      const auto name = std::string(trim(part));
      if (isLocalSlotName(name))
      {
        slots.push_back(ManifestSlot{
          .name = name,
          .line = i,
          .lineText = std::string(lines[static_cast<std::size_t>(i)]),
        });
      }
    }
    break;
  }
  return slots;
}

auto localSlotPrefix(std::string_view name) -> std::string
{
  std::cmatch match;
  if (!std::regex_search(name.data(), name.data() + name.size(), match, localPrefixRe()))
  {
    return {};
  }
  return "l" + match[1].str();
}

auto extractBaseVariableName(std::string_view name) -> std::string
{
  const auto trimmed = std::string(trim(name));
  if (std::regex_match(trimmed, simpleNameRe()))
  {
    return trimmed;
  }
  static const std::regex mathRe{R"re(^([A-Za-z][A-Za-z0-9_-]*\$?)[\+\-\*\/]\d+)re"};
  static const std::regex indexRe{R"re(^([A-Za-z][A-Za-z0-9_-]*\$?)\[[^\]]+\])re"};
  std::smatch match;
  if (std::regex_search(trimmed, match, mathRe))
  {
    return match[1].str();
  }
  if (std::regex_search(trimmed, match, indexRe))
  {
    return match[1].str();
  }
  return trimmed;
}

auto localSlotBaseName(std::string_view name) -> std::string
{
  if (!isLocalSlotName(name))
  {
    return extractBaseVariableName(name);
  }
  std::cmatch match;
  if (std::regex_match(name.data(), name.data() + name.size(), match, localBaseRe()))
  {
    return match[1].str();
  }
  return extractBaseVariableName(name);
}

auto buildLocalSlotVariableName(std::string_view prefix, std::string_view baseName) -> std::string
{
  return std::string(prefix) + extractBaseVariableName(baseName);
}

auto canonicalPrefixFromDeclarations(const std::vector<Declaration>& declarations) -> std::string
{
  std::vector<std::pair<int, std::string>> prefixes;
  std::unordered_set<std::string> seen;
  for (const auto& decl : declarations)
  {
    if (!isLocalSlotName(decl.varName))
    {
      continue;
    }
    const auto prefix = localSlotPrefix(decl.varName);
    if (prefix.empty() || !seen.insert(prefix).second)
    {
      continue;
    }
    prefixes.emplace_back(std::stoi(prefix.substr(1)), prefix);
  }
  if (prefixes.empty())
  {
    return {};
  }
  std::sort(prefixes.begin(), prefixes.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });
  return prefixes.front().second;
}

const Declaration* firstLocalSlotDeclaration(const std::vector<Declaration>& declarations)
{
  for (const auto& decl : declarations)
  {
    if (isLocalSlotName(decl.varName))
    {
      return &decl;
    }
  }
  return nullptr;
}

bool requireLNamesInCallees(const ValidateSettings& settings)
{
  if (settings.requireLNamesInCallees.has_value())
  {
    return *settings.requireLNamesInCallees;
  }
  return settings.strictModeEnabled;
}

void markCallees(std::vector<IndexEntry>& entries)
{
  std::unordered_set<std::string> callees;
  for (const auto& entry : entries)
  {
    for (const auto& link : entry.callLinks)
    {
      callees.insert(link.target);
    }
  }
  for (auto& entry : entries)
  {
    entry.isCallCallee = callees.contains(entry.identity.key);
  }
}

void addTreeEntries(
  const ValidateContext& ctx,
  const std::filesystem::path& dir,
  std::vector<IndexEntry>& entries,
  std::unordered_set<std::string>& seen)
{
  if (!ctx.stores.layout || dir.empty())
  {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec))
  {
    return;
  }
  for (std::filesystem::recursive_directory_iterator it(dir, ec), end; it != end && !ec;
       it.increment(ec))
  {
    if (!it->is_regular_file(ec))
    {
      continue;
    }
    const auto path = it->path();
    if (!ctx.stores.layout->shouldIndex(path))
    {
      continue;
    }
    auto identity = ctx.stores.layout->overlayForPath(path);
    if (!identity || !seen.insert(identity->key).second)
    {
      continue;
    }
    std::optional<std::string> text;
    if (ctx.stores.textForPath)
    {
      text = ctx.stores.textForPath(path);
    }
    if (!text)
    {
      continue;
    }
    entries.push_back(parseIndexEntry(*text, std::move(*identity), *ctx.stores.layout));
  }
}

auto currentIndexEntry(
  const ValidateContext& ctx,
  const std::vector<std::shared_ptr<const IndexEntry>>& entries)
  -> std::shared_ptr<const IndexEntry>;

auto scanWorkspaceEntries(const ValidateContext& ctx) -> std::vector<std::shared_ptr<const IndexEntry>>
{
  std::vector<IndexEntry> parsed;
  std::unordered_set<std::string> seen;
  if (ctx.stores.layout)
  {
    const auto root = ctx.stores.layout->workspaceRoot();
    if (toLowerAscii(root.filename().string()).starts_with("custom"))
    {
      addTreeEntries(ctx, root, parsed, seen);
    }
    else
    {
      addTreeEntries(ctx, root / ctx.stores.layout->customFolderName(), parsed, seen);
      addTreeEntries(ctx, root / "system", parsed, seen);
    }
  }
  markCallees(parsed);
  std::vector<std::shared_ptr<const IndexEntry>> out;
  out.reserve(parsed.size());
  for (auto& entry : parsed)
  {
    out.push_back(std::make_shared<const IndexEntry>(std::move(entry)));
  }
  return out;
}

auto workspaceEntries(const ValidateContext& ctx) -> std::vector<std::shared_ptr<const IndexEntry>>
{
  if (ctx.stores.index)
  {
    auto snap = ctx.stores.index->snapshot();
    if (currentIndexEntry(ctx, snap))
    {
      return snap;
    }
  }
  return scanWorkspaceEntries(ctx);
}

auto currentIndexEntry(
  const ValidateContext& ctx,
  const std::vector<std::shared_ptr<const IndexEntry>>& entries)
  -> std::shared_ptr<const IndexEntry>
{
  std::string key;
  if (ctx.stores.layout)
  {
    if (auto identity = ctx.stores.layout->overlayForUri(ctx.prep->snapshot.uri))
    {
      key = identity->key;
    }
  }
  if (!key.empty())
  {
    for (const auto& entry : entries)
    {
      if (entry && entry->identity.key == key)
      {
        return entry;
      }
    }
  }
  const auto abs = normalizeFsPath(pathFromUri(ctx.prep->snapshot.uri).string());
  for (const auto& entry : entries)
  {
    if (entry && normalizeFsPath(entry->identity.absPath.string()) == abs)
    {
      return entry;
    }
  }
  return {};
}

auto liveLineText(const ValidateContext& ctx, int line, std::string_view fallback) -> std::string
{
  if (line >= 0 && line < static_cast<int>(ctx.prep->lines.size()))
  {
    return std::string(ctx.prep->lines[static_cast<std::size_t>(line)]);
  }
  return std::string(fallback);
}

auto ownerLineText(const ValidateContext& ctx, const OverlayIdentity& identity, int line, std::string_view fallback)
  -> std::string
{
  if (ctx.stores.textForPath)
  {
    if (auto text = ctx.stores.textForPath(identity.absPath))
    {
      const auto lines = splitLines(*text);
      if (line >= 0 && line < static_cast<int>(lines.size()))
      {
        return std::string(lines[static_cast<std::size_t>(line)]);
      }
    }
  }
  return std::string(fallback);
}

lsp::Range charRange(int line, int startChar, int endChar)
{
  const auto start = static_cast<lsp::Uint>(std::max(startChar, 0));
  const auto end = static_cast<lsp::Uint>(std::max(endChar, startChar));
  return {
    .start = {.line = static_cast<lsp::Uint>(std::max(line, 0)), .character = start},
    .end = {.line = static_cast<lsp::Uint>(std::max(line, 0)), .character = end},
  };
}

lsp::Range wholeLineRange(int line, std::string_view text)
{
  return charRange(line, 0, static_cast<int>(utf16Length(text)));
}

lsp::Range offsetPlusOneRange(int line, std::string_view text)
{
  const auto end = static_cast<int>(utf16Length(text));
  const auto start = end > 0 ? 1 : 0;
  return charRange(line, start, end);
}

std::size_t findDeclarationColumn(std::string_view line, std::string_view varName)
{
  const auto base = extractBaseVariableName(varName);
  auto escape = [](std::string_view text) {
    std::string out;
    for (char ch : text)
    {
      if (std::string_view("\\^$.*+?()[]{}|").find(ch) != std::string_view::npos)
      {
        out.push_back('\\');
      }
      out.push_back(ch);
    }
    return out;
  };
  const auto escaped = escape(base);
  std::string noDollar;
  if (!base.empty() && base.back() == '$')
  {
    noDollar = escape(base.substr(0, base.size() - 1));
  }
  else
  {
    noDollar = escaped;
  }
  const std::regex patterns[] = {
    std::regex("\\b" + escaped + "(?:\\([^)]*\\))?\\s*=", std::regex::icase),
    std::regex("\\bset\\s+" + noDollar + "\\b", std::regex::icase),
    std::regex("\\(\\s*" + escaped + "\\s*\\)", std::regex::icase),
  };
  for (const auto& pattern : patterns)
  {
    std::cmatch match;
    if (!std::regex_search(line.data(), line.data() + line.size(), match, pattern))
    {
      continue;
    }
    const std::regex nameRe("\\b" + escaped, std::regex::icase);
    std::cmatch nameMatch;
    if (std::regex_search(line.data(), line.data() + line.size(), nameMatch, nameRe))
    {
      return static_cast<std::size_t>(nameMatch.position());
    }
    const std::regex parenRe("\\(\\s*(" + escaped + ")\\s*\\)", std::regex::icase);
    std::cmatch parenMatch;
    if (std::regex_search(line.data(), line.data() + line.size(), parenMatch, parenRe))
    {
      return static_cast<std::size_t>(parenMatch.position(1));
    }
  }
  return 0;
}

auto nextAvailablePrefix(
  const std::vector<std::shared_ptr<const IndexEntry>>& entries,
  std::string_view excludeKey) -> std::string
{
  int maxUsed = 0;
  for (const auto& entry : entries)
  {
    if (!entry || !entry->isCallCallee || entry->identity.key == excludeKey)
    {
      continue;
    }
    const auto prefix = canonicalPrefixFromDeclarations(entry->declarations);
    if (prefix.empty())
    {
      continue;
    }
    try
    {
      const auto number = std::stoi(prefix.substr(1));
      if (number > maxUsed)
      {
        maxUsed = number;
      }
    }
    catch (...)
    {
    }
  }
  return "l" + std::to_string(maxUsed + 1);
}

auto fileLocalSlotPrefix(
  const std::vector<std::shared_ptr<const IndexEntry>>& entries,
  const IndexEntry& entry) -> std::string
{
  const auto existing = canonicalPrefixFromDeclarations(entry.declarations);
  if (!existing.empty())
  {
    return existing;
  }
  return nextAvailablePrefix(entries, entry.identity.key);
}

std::string ownerBasename(const IndexEntry& entry)
{
  return entry.identity.absPath.filename().string();
}

} // namespace

void familyLocalSlotsManifest(ValidateContext& ctx)
{
  ctx.pollCancel();
  const auto types = collectTypeWrites(ctx.prep->lines);
  const auto manifest = parseManifest(ctx.prep->lines, ctx.prep->sections);
  if (manifest.empty())
  {
    return;
  }
  const auto declarations = extractDeclarationsFirstWin(ctx.prep->lines, ctx.prep->sections, types);
  std::unordered_set<std::string> declaredNames;
  std::vector<LiveDecl> localDecls;
  for (const auto& decl : declarations)
  {
    if (isLocalSlotName(decl.varName) && !isValueType(decl.variableType))
    {
      declaredNames.insert(decl.varName);
      localDecls.push_back(decl);
    }
  }
  std::unordered_set<std::string> manifestNames;
  for (const auto& slot : manifest)
  {
    manifestNames.insert(slot.name);
  }

  for (const auto& slot : manifest)
  {
    if (declaredNames.contains(slot.name))
    {
      continue;
    }
    lsp::Diagnostic diagnostic;
    diagnostic.range = wholeLineRange(slot.line, slot.lineText);
    diagnostic.message = "@local_slots lists " + slot.name + " but no declaration for " + slot.name
      + " was found";
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.source = "ADT-Syntax";
    diagnostic.code = std::string("vertexadt/local-slots-manifest");
    ctx.append(std::move(diagnostic));
  }

  for (const auto& decl : localDecls)
  {
    if (manifestNames.contains(decl.varName))
    {
      continue;
    }
    lsp::Diagnostic diagnostic;
    diagnostic.range = offsetPlusOneRange(decl.line, decl.lineText);
    diagnostic.message = "Declared local slot " + decl.varName + " is missing from @local_slots manifest";
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.source = "ADT-Syntax";
    diagnostic.code = std::string("vertexadt/local-slots-manifest");
    ctx.append(std::move(diagnostic));
  }
}

void familyLocalSlotNameConflicts(ValidateContext& ctx)
{
  ctx.pollCancel();
  if (!ctx.settings.strictModeEnabled || !ctx.settings.validateUniqueNames)
  {
    return;
  }
  const auto entries = workspaceEntries(ctx);
  const auto current = currentIndexEntry(ctx, entries);
  if (!current)
  {
    return;
  }

  struct Owner
  {
    std::string key;
    std::string basename;
    OverlayIdentity identity;
    int line = 0;
    std::string lineText;
    std::string varName;
  };

  struct Conflict
  {
    std::string prefix;
    std::string fileKey;
    int line = 0;
    std::string lineText;
    std::string varName;
    Owner owner;
  };

  std::unordered_map<std::string, Owner> owners;
  std::vector<Conflict> conflicts;
  for (const auto& entry : entries)
  {
    if (!entry || !entry->isCallCallee)
    {
      continue;
    }
    const auto prefix = canonicalPrefixFromDeclarations(entry->declarations);
    const auto* representative = firstLocalSlotDeclaration(entry->declarations);
    if (prefix.empty() || !representative)
    {
      continue;
    }
    const auto existing = owners.find(prefix);
    if (existing == owners.end())
    {
      owners.emplace(
        prefix,
        Owner{
          .key = entry->identity.key,
          .basename = ownerBasename(*entry),
          .identity = entry->identity,
          .line = representative->line,
          .lineText = representative->lineText,
          .varName = representative->varName,
        });
      continue;
    }
    if (existing->second.key == entry->identity.key)
    {
      continue;
    }
    conflicts.push_back(Conflict{
      .prefix = prefix,
      .fileKey = entry->identity.key,
      .line = representative->line,
      .lineText = representative->lineText,
      .varName = representative->varName,
      .owner = existing->second,
    });
  }

  std::unordered_set<std::string> seen;
  for (const auto& conflict : conflicts)
  {
    if (conflict.fileKey != current->identity.key)
    {
      continue;
    }
    const auto key = conflict.prefix + "::" + conflict.owner.key;
    if (!seen.insert(key).second)
    {
      continue;
    }
    const auto lineText = liveLineText(ctx, conflict.line, conflict.lineText);
    lsp::Diagnostic diagnostic;
    diagnostic.range = offsetPlusOneRange(conflict.line, lineText);
    diagnostic.message = lineText + " has an error: Local slot prefix " + conflict.prefix
      + " is already used by callee " + conflict.owner.basename;
    diagnostic.severity = lsp::DiagnosticSeverity::Information;
    diagnostic.source = "ADT-Syntax";
    diagnostic.code = std::string("vertexadt/local-slot-name-collision");

    const auto relatedText =
      ownerLineText(ctx, conflict.owner.identity, conflict.owner.line, conflict.owner.lineText);
    lsp::DiagnosticRelatedInformation related;
    related.location.uri = conflict.owner.identity.uri;
    related.location.range = wholeLineRange(conflict.owner.line, relatedText);
    related.message =
      conflict.owner.varName + " (" + conflict.prefix + ") in " + conflict.owner.basename;
    diagnostic.relatedInformation = std::vector<lsp::DiagnosticRelatedInformation>{std::move(related)};
    ctx.append(std::move(diagnostic));
  }
}

void familyCalleeLocalNaming(ValidateContext& ctx)
{
  ctx.pollCancel();
  if (!requireLNamesInCallees(ctx.settings))
  {
    return;
  }
  const auto entries = workspaceEntries(ctx);
  const auto entry = currentIndexEntry(ctx, entries);
  if (!entry || !entry->isCallCallee)
  {
    return;
  }

  const auto filePrefix = fileLocalSlotPrefix(entries, *entry);
  const auto types = collectTypeWrites(ctx.prep->lines);

  forEachDeclarationFragment(
    ctx.prep->lines,
    ctx.prep->sections,
    [&](int lineIndex, const std::string& sectionName, std::string_view line, std::string_view fragment) {
      const auto name = assignmentName(fragment);
      if (!name)
      {
        return;
      }
      const auto number = declarationNumber(fragment);
      if (!number || *number <= 0)
      {
        return;
      }
      if (isValueType(typeForDeclaration(*name, lineIndex, line, types)))
      {
        return;
      }
      if (isLocalSlotName(*name) && localSlotPrefix(*name) == filePrefix)
      {
        return;
      }
      const auto baseName = isLocalSlotName(*name) ? localSlotBaseName(*name) : *name;
      const auto nextName = buildLocalSlotVariableName(filePrefix, baseName);
      if (*name == nextName)
      {
        return;
      }
      const auto byteColumn = findDeclarationColumn(line, *name);
      const auto column = static_cast<int>(utf16Offset(line, byteColumn));
      const auto columnEnd = static_cast<int>(utf16Offset(line, byteColumn + name->size()));
      lsp::Diagnostic diagnostic;
      diagnostic.range = charRange(lineIndex, column, columnEnd);
      diagnostic.message = "Callee locals in this file should share prefix " + filePrefix + " (e.g. "
        + nextName + ")";
      diagnostic.severity = lsp::DiagnosticSeverity::Warning;
      diagnostic.source = "ADT-Syntax";
      diagnostic.code = std::string("vertexadt/callee-local-naming");
      lsp::json::Object data;
      data["line"] = lineIndex;
      data["varName"] = *name;
      data["newName"] = nextName;
      data["sectionName"] = sectionName;
      data["fileWide"] = true;
      diagnostic.data = lsp::json::Value(std::move(data));
      ctx.append(std::move(diagnostic));
    });
}

auto localSlotNamePrefix(std::string_view name) -> std::string
{
  return localSlotPrefix(name);
}

auto localSlotNameBase(std::string_view name) -> std::string
{
  return localSlotBaseName(name);
}

auto fileLocalSlotPrefixForPath(
  const std::vector<std::shared_ptr<const IndexEntry>>& entries,
  const std::filesystem::path& absPath) -> std::string
{
  const auto target = normalizeFsPath(absPath.string());
  for (const auto& entry : entries)
  {
    if (entry && normalizeFsPath(entry->identity.absPath.string()) == target)
    {
      return fileLocalSlotPrefix(entries, *entry);
    }
  }
  // Not indexed: nothing to exclude.
  return nextAvailablePrefix(entries, {});
}

auto localSlotRenamePairs(std::string_view content, std::string_view filePrefix)
  -> std::vector<LocalSlotRenamePair>
{
  std::vector<LocalSlotRenamePair> pairs;
  std::unordered_set<std::string> seen;
  const auto addPair = [&](const std::string& oldName, std::string newName) {
    if (oldName.empty() || oldName == newName || !seen.insert(oldName).second)
    {
      return;
    }
    pairs.push_back(LocalSlotRenamePair{.oldName = oldName, .newName = std::move(newName)});
  };

  const auto lines = splitLines(content);
  const auto types = collectTypeWrites(lines);

  // findNonLocalSlotDeclarationsInContent: every declaration block, no
  // verification-ignored skip and no first-win.
  for (const auto& [start, end] : declarationBlocks(lines))
  {
    for (int i = start + 1; i < end; ++i)
    {
      const auto line = lines[static_cast<std::size_t>(i)];
      std::size_t fragStart = 0;
      while (fragStart <= line.size())
      {
        const auto semi = line.find(';', fragStart);
        const auto fragment = semi == std::string_view::npos
          ? line.substr(fragStart)
          : line.substr(fragStart, semi - fragStart);
        fragStart = semi == std::string_view::npos ? line.size() + 1 : semi + 1;
        if (isBangComment(fragment))
        {
          continue;
        }
        const auto name = assignmentName(fragment);
        if (!name || isLocalSlotName(*name))
        {
          continue;
        }
        const auto number = declarationNumber(fragment);
        if (!number || *number <= 0)
        {
          continue;
        }
        if (isValueType(typeForDeclaration(*name, i, line, types)))
        {
          continue;
        }
        addPair(*name, buildLocalSlotVariableName(filePrefix, *name));
      }
    }
  }

  // Wrong-prefix local-slot declarations (extractDeclarationNumbersFromLines:
  // first win per section, verification-ignored sections skipped).
  const auto sections = parseFileSections(lines);
  for (const auto& decl : extractDeclarationsFirstWin(lines, sections, types))
  {
    if (!isLocalSlotName(decl.varName) || isValueType(decl.variableType))
    {
      continue;
    }
    if (localSlotPrefix(decl.varName) != filePrefix)
    {
      addPair(decl.varName, buildLocalSlotVariableName(filePrefix, localSlotBaseName(decl.varName)));
    }
  }
  return pairs;
}

} // namespace vertexadt
