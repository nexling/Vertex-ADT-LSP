#include "program_move.h"

#include "dir_paths.h"
#include "text_util.h"
#include "utf16_text.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <regex>
#include <set>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace vertexadt {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view kAnnotationId = "vertexadt.programMove";

constexpr std::array<std::string_view, 6> kMoveExtensions{".prg", ".adt", ".p", ".vxt", ".3d", ".2d"};

enum class Tree
{
  Custom,
  System,
};

// Where a path sits in the library: tree plus the library-relative path in
// its own spelling (forward slashes).
struct Place
{
  Tree tree = Tree::Custom;
  std::string rel;
};

bool iequals(std::string_view a, std::string_view b)
{
  return a.size() == b.size() && toLowerAscii(a) == toLowerAscii(b);
}

bool rootIsCustom(const LibraryLayout& layout)
{
  return toLowerAscii(layout.workspaceRoot().filename().string()).starts_with("custom");
}

auto pathKey(const fs::path& path) -> std::string
{
  return normalizeFsPath(path.lexically_normal().string());
}

auto genericString(const fs::path& path) -> std::string
{
  return replaceBackslashes(path.generic_string());
}

bool isUnder(const fs::path& child, const fs::path& dir)
{
  const auto c = pathKey(child);
  auto d = pathKey(dir);
  if (d.empty())
  {
    return false;
  }
  d += '/';
  return c.size() > d.size() && c.compare(0, d.size(), d) == 0;
}

auto splitSlash(std::string_view text) -> std::vector<std::string>
{
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= text.size())
  {
    const auto slash = text.find('/', start);
    if (slash == std::string_view::npos)
    {
      parts.emplace_back(text.substr(start));
      break;
    }
    parts.emplace_back(text.substr(start, slash - start));
    start = slash + 1;
  }
  return parts;
}

auto placeOf(const LibraryLayout& layout, const fs::path& absPath) -> std::optional<Place>
{
  const auto& root = layout.workspaceRoot();
  if (root.empty() || absPath.empty() || !isUnder(absPath, root))
  {
    return std::nullopt;
  }
  const auto fromRoot = genericString(absPath.lexically_normal().lexically_relative(root.lexically_normal()));
  if (fromRoot.empty() || fromRoot.starts_with(".."))
  {
    return std::nullopt;
  }
  if (rootIsCustom(layout))
  {
    return Place{.tree = Tree::Custom, .rel = fromRoot};
  }
  const auto slash = fromRoot.find('/');
  if (slash == std::string::npos)
  {
    return std::nullopt;
  }
  const auto first = std::string_view(fromRoot).substr(0, slash);
  auto rel = fromRoot.substr(slash + 1);
  if (iequals(first, layout.customFolderName()))
  {
    return Place{.tree = Tree::Custom, .rel = std::move(rel)};
  }
  if (iequals(first, "system"))
  {
    return Place{.tree = Tree::System, .rel = std::move(rel)};
  }
  return std::nullopt;
}

auto customRoot(const LibraryLayout& layout) -> fs::path
{
  return rootIsCustom(layout) ? layout.workspaceRoot() : layout.workspaceRoot() / layout.customFolderName();
}

// The System file at the same library-relative path, when the layout has a
// System tree and the file exists.
auto systemTwin(const LibraryLayout& layout, std::string_view rel) -> std::optional<fs::path>
{
  if (rootIsCustom(layout) || layout.workspaceRoot().empty())
  {
    return std::nullopt;
  }
  const auto candidate = (layout.workspaceRoot() / "system" / fs::path(std::string(rel))).lexically_normal();
  std::error_code ec;
  if (fs::is_regular_file(candidate, ec))
  {
    return candidate;
  }
  return std::nullopt;
}

bool isFile(const fs::path& path)
{
  std::error_code ec;
  return fs::is_regular_file(path, ec);
}

bool hasMoveExtension(const fs::path& path)
{
  const auto ext = fileExtensionLower(path);
  return std::find(kMoveExtensions.begin(), kMoveExtensions.end(), ext) != kMoveExtensions.end();
}

bool isMovableFile(const LibraryLayout& layout, const fs::path& path)
{
  return hasMoveExtension(path) || layout.shouldIndex(path);
}

// `$DIR_*/rest` (or `$DIR_CUSTOM/rel`) reaching `target`. `preferred` wins a
// tie between Fallback tokens for the same folder.
auto tokenSpelling(const LibraryLayout& layout, const fs::path& target, const DirToken* preferred)
  -> std::optional<std::string>
{
  const auto place = placeOf(layout, target);
  if (!place)
  {
    return std::nullopt;
  }
  const auto slash = place->rel.find('/');
  if (slash != std::string::npos)
  {
    const auto first = std::string_view(place->rel).substr(0, slash);
    const DirToken* chosen = nullptr;
    for (const auto& token : dirTokens())
    {
      if (token.kind != DirTokenKind::Fallback || !iequals(token.folder, first))
      {
        continue;
      }
      if (!chosen || &token == preferred)
      {
        chosen = &token;
      }
    }
    if (chosen)
    {
      return std::string(chosen->token) + place->rel.substr(slash);
    }
  }
  if (rootIsCustom(layout))
  {
    return std::nullopt;
  }
  return std::string(place->tree == Tree::Custom ? "$DIR_CUSTOM/" : "$DIR_SYSTEM/") + place->rel;
}

// Directory segments the caller already spells (any case) keep its spelling.
auto applyCaseStyle(const std::string& newText, std::string_view oldText) -> std::string
{
  auto parts = splitSlash(newText);
  const auto old = splitSlash(oldText);
  if (parts.size() < 2 || old.size() < 2)
  {
    return newText;
  }
  for (std::size_t i = 0; i + 1 < parts.size(); ++i)
  {
    if (parts[i].empty() || parts[i].front() == '$')
    {
      continue;
    }
    for (std::size_t j = 0; j + 1 < old.size(); ++j)
    {
      if (iequals(parts[i], old[j]))
      {
        parts[i] = old[j];
        break;
      }
    }
  }
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i)
  {
    if (i)
    {
      out += '/';
    }
    out += parts[i];
  }
  return out;
}

bool isSectionKeyword(std::string_view token)
{
  const auto lower = toLowerAscii(token);
  return lower == "section" || lower == "sectionoffile";
}

bool isWordChar(char ch)
{
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

// `needle` (lowercase) in `hayLower` as a whole file name: not glued to a
// word character or `.` on the left, nor a word character on the right.
bool containsName(std::string_view hayLower, std::string_view needle)
{
  std::size_t from = 0;
  while (true)
  {
    const auto at = hayLower.find(needle, from);
    if (at == std::string_view::npos)
    {
      return false;
    }
    const bool leftOk = at == 0 || (!isWordChar(hayLower[at - 1]) && hayLower[at - 1] != '.');
    const auto after = at + needle.size();
    const bool rightOk = after >= hayLower.size() || !isWordChar(hayLower[after]);
    if (leftOk && rightOk)
    {
      return true;
    }
    from = at + 1;
  }
}

auto lspPosition(int line, int character) -> lsp::json::Value
{
  lsp::json::Object position;
  position["line"] = lsp::json::Value(line);
  position["character"] = lsp::json::Value(character);
  return lsp::json::Value(std::move(position));
}

auto textEditJson(const MoveTextEdit& edit, bool annotate) -> lsp::json::Value
{
  lsp::json::Object range;
  range["start"] = lspPosition(edit.line, edit.startCharacter);
  range["end"] = lspPosition(edit.line, edit.endCharacter);
  lsp::json::Object out;
  out["range"] = lsp::json::Value(std::move(range));
  out["newText"] = lsp::json::Value(edit.newText);
  if (annotate)
  {
    out["annotationId"] = lsp::json::Value(std::string(kAnnotationId));
  }
  return lsp::json::Value(std::move(out));
}

auto uriText(const fs::path& path) -> std::string
{
  return uriFromPath(path).toString();
}

} // namespace

auto scanMoveReferences(std::string_view text) -> std::vector<MoveReference>
{
  static const std::regex includedFromRe{R"re(^\s*##included_from\s+("[^"]*"|'[^']*'|[^\s#;]+))re"};
  static const std::regex includeRe{R"re(^\s*#\s*include\s+("[^"]*"|'[^']*'|[^\s#;]+))re", std::regex::icase};
  static const std::regex callParenRe{R"re(\bcall\s*\(\s*("[^"]*"|'[^']*'|\$[^"',)\s]+))re", std::regex::icase};
  static const std::regex callSpaceRe{R"re(\bcall\s+("[^"]*"|'[^']*'|[^\s,("']+))re", std::regex::icase};
  static const std::regex sectionSuffixRe{R"re(^(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$)re"};

  std::vector<MoveReference> refs;
  const auto lines = splitLines(text);
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    auto line = lines[static_cast<std::size_t>(i)];
    if (!line.empty() && line.back() == '\r')
    {
      line.remove_suffix(1);
    }
    const auto trimmed = trim(line);
    if (trimmed.empty() || trimmed.front() == '!')
    {
      continue;
    }
    const auto bang = findBangCommentIndex(line);
    const auto codeEnd = bang >= 0 ? static_cast<std::size_t>(bang) : line.size();
    const auto lower = toLowerAscii(line);
    if (trimmed.front() != '#' && lower.find("call") == std::string::npos)
    {
      continue;
    }

    std::set<std::size_t> seen;
    auto add = [&](std::size_t tokenBegin, std::string_view token, MoveRefKind kind) {
      if (tokenBegin >= codeEnd || token.empty())
      {
        return;
      }
      const bool quoted = token.front() == '"' || token.front() == '\'';
      auto inner = quoted ? token.substr(1, token.size() >= 2 ? token.size() - 2 : 0) : token;
      auto begin = tokenBegin + (quoted ? 1 : 0);
      while (!inner.empty() && std::isspace(static_cast<unsigned char>(inner.front())))
      {
        inner.remove_prefix(1);
        ++begin;
      }
      while (!inner.empty() && std::isspace(static_cast<unsigned char>(inner.back())))
      {
        inner.remove_suffix(1);
      }
      std::string path(inner);
      std::smatch match;
      const std::string innerText(inner);
      if (std::regex_match(innerText, match, sectionSuffixRe))
      {
        path = std::string(trim(match[1].str()));
      }
      if (path.empty() || !seen.insert(begin).second)
      {
        return;
      }
      refs.push_back(MoveReference{
        .line = i,
        .begin = begin,
        .end = begin + path.size(),
        .path = std::move(path),
        .kind = kind,
      });
    };

    const std::string lineText(line);
    std::smatch match;
    if (std::regex_search(lineText, match, includedFromRe))
    {
      add(static_cast<std::size_t>(match.position(1)), match[1].str(), MoveRefKind::IncludedFrom);
      continue;
    }
    if (std::regex_search(lineText, match, includeRe))
    {
      add(static_cast<std::size_t>(match.position(1)), match[1].str(), MoveRefKind::Include);
      continue;
    }
    for (const auto* re : {&callParenRe, &callSpaceRe})
    {
      for (std::sregex_iterator it(lineText.begin(), lineText.end(), *re), last; it != last; ++it)
      {
        const auto at = static_cast<std::size_t>(it->position(0));
        if (isInsideQuotes(line, at))
        {
          continue;
        }
        const auto token = (*it)[1].str();
        // `call` + a section keyword or a bare word is a same-file section call.
        if (token.front() != '"' && token.front() != '\'' && token.find('.') == std::string::npos
            && token.front() != '$')
        {
          continue;
        }
        if (isSectionKeyword(token))
        {
          continue;
        }
        add(static_cast<std::size_t>(it->position(1)), token, MoveRefKind::Call);
      }
    }
  }
  std::sort(refs.begin(), refs.end(), [](const MoveReference& a, const MoveReference& b) {
    return a.line != b.line ? a.line < b.line : a.begin < b.begin;
  });
  return refs;
}

auto resolveMoveReference(
  const LibraryLayout& layout,
  const fs::path& documentPath,
  std::string_view path) -> std::optional<ResolvedReference>
{
  const auto text = replaceBackslashes(trim(path));
  if (text.empty())
  {
    return std::nullopt;
  }
  if (text.front() == '$')
  {
    const auto span = findDirTokenSpan(text);
    if (span && span->begin == 0 && span->token->kind == DirTokenKind::This)
    {
      auto rest = std::string_view(text).substr(span->end);
      while (!rest.empty() && rest.front() == '/')
      {
        rest.remove_prefix(1);
      }
      const auto candidate = (documentPath.parent_path() / fs::path(std::string(rest))).lexically_normal();
      if (isFile(candidate))
      {
        return ResolvedReference{.target = candidate, .documentRelative = true};
      }
      return std::nullopt;
    }
    if (const auto resolved = layout.resolveVertexFilePath(text))
    {
      return ResolvedReference{.target = resolved->lexically_normal(), .documentRelative = false};
    }
    return std::nullopt;
  }
  const auto asPath = fs::path(text);
  if (asPath.is_absolute())
  {
    if (isFile(asPath))
    {
      return ResolvedReference{.target = asPath.lexically_normal(), .documentRelative = false};
    }
    return std::nullopt;
  }
  if (!documentPath.empty())
  {
    const auto candidate = (documentPath.parent_path() / asPath).lexically_normal();
    if (isFile(candidate))
    {
      return ResolvedReference{.target = candidate, .documentRelative = true};
    }
  }
  if (const auto resolved = layout.resolveVertexFilePath(text))
  {
    return ResolvedReference{.target = resolved->lexically_normal(), .documentRelative = false};
  }
  return std::nullopt;
}

auto formatMovePath(
  const LibraryLayout& layout,
  std::string_view oldText,
  bool oldDocumentRelative,
  const fs::path& callerDir,
  const fs::path& target) -> std::optional<std::string>
{
  const auto old = replaceBackslashes(trim(oldText));
  const bool backslashes = oldText.find('\\') != std::string_view::npos
    && oldText.find('/') == std::string_view::npos;

  const DirToken* oldToken = nullptr;
  if (!old.empty() && old.front() == '$')
  {
    if (const auto span = findDirTokenSpan(old); span && span->begin == 0)
    {
      oldToken = span->token;
    }
  }
  const bool oldThis = oldToken && oldToken->kind == DirTokenKind::This;

  std::optional<std::string> out;
  if (oldThis || (!oldToken && oldDocumentRelative))
  {
    if (isUnder(target, callerDir))
    {
      const auto rel = genericString(target.lexically_normal().lexically_relative(callerDir.lexically_normal()));
      out = oldThis ? std::string(oldToken->token) + "/" + rel : rel;
    }
  }
  if (!out)
  {
    if (oldToken || oldDocumentRelative)
    {
      out = tokenSpelling(layout, target, oldToken);
    }
    else if (fs::path(old).is_absolute())
    {
      out = genericString(target.lexically_normal());
    }
    else if (const auto place = placeOf(layout, target))
    {
      // Bare library-relative path: Custom first, then System.
      out = place->rel;
    }
  }
  if (!out)
  {
    return std::nullopt;
  }
  auto styled = applyCaseStyle(*out, old);
  if (backslashes)
  {
    std::replace(styled.begin(), styled.end(), '/', '\\');
  }
  return styled;
}

auto defaultMoveTargetText(const LibraryLayout& layout, const fs::path& path) -> std::optional<std::string>
{
  return tokenSpelling(layout, path, nullptr);
}

auto resolveMoveTargetText(
  const LibraryLayout& layout,
  const fs::path& source,
  std::string_view text) -> MoveTargetResolution
{
  auto spec = replaceBackslashes(stripPathQuotes(trim(text)));
  spec = std::string(trim(spec));
  if (spec.empty())
  {
    return {.error = "Enter a target path."};
  }
  if (spec.back() == '/')
  {
    spec += source.filename().string();
  }

  fs::path candidate;
  if (spec.front() == '$')
  {
    const auto span = findDirTokenSpan(spec);
    if (!span || span->begin != 0)
    {
      return {.error = "Unknown $DIR_ token."};
    }
    auto rest = std::string_view(spec).substr(span->end);
    if (rest.empty() || rest.front() != '/')
    {
      return {.error = "Write a file path after the $DIR_ token."};
    }
    rest.remove_prefix(1);
    const auto& token = *span->token;
    switch (token.kind)
    {
    case DirTokenKind::Fallback:
      candidate = customRoot(layout) / fs::path(std::string(token.folder)) / fs::path(std::string(rest));
      break;
    case DirTokenKind::Pinned:
      if (token.tree != DirTree::Custom)
      {
        return {.error = "The target must be inside the Custom tree."};
      }
      candidate = customRoot(layout) / fs::path(std::string(rest));
      break;
    case DirTokenKind::This:
      candidate = source.parent_path() / fs::path(std::string(rest));
      break;
    case DirTokenKind::ActProj:
      return {.error = "The target must be inside the Custom tree."};
    }
  }
  else if (fs::path(spec).is_absolute())
  {
    candidate = fs::path(spec);
  }
  else
  {
    candidate = customRoot(layout) / fs::path(spec);
  }
  candidate = candidate.lexically_normal();

  const auto place = placeOf(layout, candidate);
  if (!place || place->tree != Tree::Custom)
  {
    return {.error = "The target must be inside the Custom tree."};
  }
  if (!hasMoveExtension(candidate) && fileExtensionLower(candidate) != fileExtensionLower(source))
  {
    return {.error = "Keep a Vertex ADT file extension (.prg, .adt, .p, .vxt, .3d, .2d)."};
  }
  return {.path = candidate};
}

auto planProgramMove(const ProgramMoveInput& input) -> ProgramMovePlan
{
  ProgramMovePlan plan;
  const auto& layout = input.layout;
  const bool command = input.origin == MoveOrigin::Command;
  if (layout.workspaceRoot().empty())
  {
    if (command)
    {
      plan.refusal = "No workspace root found.";
    }
    return plan;
  }

  struct Move
  {
    fs::path oldPath;
    fs::path newPath;
    bool frozen = false;
  };
  std::vector<Move> moves;
  std::unordered_map<std::string, std::size_t> byOldKey;
  auto addMove = [&](fs::path oldPath, fs::path newPath) {
    const auto key = pathKey(oldPath);
    if (byOldKey.contains(key))
    {
      return;
    }
    byOldKey.emplace(key, moves.size());
    moves.push_back(Move{.oldPath = oldPath.lexically_normal(), .newPath = newPath.lexically_normal()});
  };
  for (const auto& rename : input.renames)
  {
    std::error_code ec;
    if (fs::is_directory(rename.oldPath, ec))
    {
      for (fs::recursive_directory_iterator it(rename.oldPath, ec), end; it != end && !ec; it.increment(ec))
      {
        if (!it->is_regular_file(ec) || !isMovableFile(layout, it->path()))
        {
          continue;
        }
        addMove(it->path(), rename.newPath / it->path().lexically_relative(rename.oldPath));
      }
    }
    else if (isMovableFile(layout, rename.oldPath))
    {
      addMove(rename.oldPath, rename.newPath);
    }
  }
  if (moves.empty())
  {
    if (command)
    {
      plan.refusal = "Only Vertex ADT files can be moved.";
    }
    return plan;
  }

  const auto display = [&](const fs::path& path) {
    if (const auto place = placeOf(layout, path))
    {
      return (place->tree == Tree::Custom ? layout.customFolderName() : std::string("system")) + "/" + place->rel;
    }
    return genericString(path);
  };

  std::vector<std::string> confirmations;
  for (auto& move : moves)
  {
    const auto from = placeOf(layout, move.oldPath);
    const auto to = placeOf(layout, move.newPath);
    const auto fromTwin = from && from->tree == Tree::Custom ? systemTwin(layout, from->rel) : std::nullopt;
    const bool sameRel = from && to && iequals(from->rel, to->rel);
    const auto toTwin = to && to->tree == Tree::Custom && !sameRel ? systemTwin(layout, to->rel) : std::nullopt;
    if (command)
    {
      if (!from || from->tree != Tree::Custom)
      {
        plan.refusal = from && from->tree == Tree::System
          ? display(move.oldPath) + " is a System file; Vertex upgrades overwrite it, so it cannot be moved."
          : genericString(move.oldPath) + " is not inside the Custom tree.";
        return plan;
      }
      if (!to || to->tree != Tree::Custom)
      {
        plan.refusal = "The target must be inside the Custom tree.";
        return plan;
      }
      if (isFile(move.newPath) && pathKey(move.newPath) != pathKey(move.oldPath))
      {
        plan.refusal = display(move.newPath) + " already exists.";
        return plan;
      }
      if (toTwin)
      {
        plan.refusal = display(move.newPath) + " would shadow the System file " + display(*toTwin) + ".";
        return plan;
      }
      if (fromTwin && !sameRel)
      {
        confirmations.push_back(
          "After the move, the old path of " + display(move.oldPath) + " reaches the System file "
          + display(*fromTwin) + ". References that are not rewritten (System callers, dynamic paths) will run it.");
      }
      continue;
    }
    // willRenameFiles: the rename happens anyway; never rewrite a caller so
    // it reaches a different file.
    if (!from)
    {
      move.frozen = true;
      continue;
    }
    if (from->tree == Tree::System)
    {
      move.frozen = true;
      plan.warnings.push_back(
        "Moved the System file " + display(move.oldPath)
        + ": references keep its old path, where a Vertex reinstall puts it back.");
      continue;
    }
    if (!to || to->tree != from->tree)
    {
      move.frozen = true;
      plan.warnings.push_back(
        "Moved " + display(move.oldPath) + " out of the Custom tree: references were not updated.");
      continue;
    }
    if (toTwin)
    {
      plan.warnings.push_back(display(move.newPath) + " now shadows the System file " + display(*toTwin) + ".");
    }
    if (fromTwin && !sameRel)
    {
      plan.warnings.push_back(
        "The old path of " + display(move.oldPath) + " now reaches the System file " + display(*fromTwin)
        + "; references that were not rewritten will run it.");
    }
  }
  if (!confirmations.empty() && !input.confirmShadow)
  {
    std::string message;
    for (const auto& line : confirmations)
    {
      message += (message.empty() ? "" : "\n") + line;
    }
    plan.confirmation = std::move(message);
    return plan;
  }
  for (const auto& move : moves)
  {
    plan.fileMoves.push_back(MoveRename{.oldPath = move.oldPath, .newPath = move.newPath});
  }

  std::vector<std::string> names;
  for (const auto& move : moves)
  {
    if (!move.frozen)
    {
      names.push_back(basenameLower(move.oldPath));
    }
  }
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  if (names.empty())
  {
    return plan;
  }

  std::vector<fs::path> scan = input.scanFiles ? input.scanFiles() : std::vector<fs::path>{};
  for (const auto& move : moves)
  {
    scan.push_back(move.oldPath);
  }
  std::unordered_set<std::string> scanned;

  for (const auto& file : scan)
  {
    if (input.pollCancel)
    {
      input.pollCancel();
    }
    const auto fileKey = pathKey(file);
    if (!scanned.insert(fileKey).second)
    {
      continue;
    }
    const auto selfIt = byOldKey.find(fileKey);
    const Move* self = selfIt == byOldKey.end() ? nullptr : &moves[selfIt->second];
    if (self && self->frozen)
    {
      continue;
    }
    const auto text = input.textForPath ? input.textForPath(file) : std::nullopt;
    if (!text)
    {
      continue;
    }
    const auto lowerText = toLowerAscii(*text);
    const bool mentions = std::any_of(names.begin(), names.end(), [&](const std::string& name) {
      return lowerText.find(name) != std::string::npos;
    });
    if (!self && !mentions)
    {
      continue;
    }
    const auto filePlace = placeOf(layout, file);
    const bool systemCaller = filePlace && filePlace->tree == Tree::System;
    const auto lines = splitLines(*text);
    auto lineText = [&](int line) {
      auto raw = lines[static_cast<std::size_t>(line)];
      if (!raw.empty() && raw.back() == '\r')
      {
        raw.remove_suffix(1);
      }
      return raw;
    };

    MoveFileEdits fileEdits{.file = file};
    std::unordered_set<int> structuralLines;
    for (const auto& ref : scanMoveReferences(*text))
    {
      const auto resolved = resolveMoveReference(layout, file, ref.path);
      if (!resolved)
      {
        continue;
      }
      const auto targetIt = byOldKey.find(pathKey(resolved->target));
      const Move* target = targetIt == byOldKey.end() ? nullptr : &moves[targetIt->second];
      structuralLines.insert(ref.line);
      if (target && target->frozen)
      {
        continue;
      }
      if (!target && !(self && resolved->documentRelative))
      {
        continue;
      }
      const auto raw = lineText(ref.line);
      if (systemCaller)
      {
        plan.skipped.push_back(MoveSkippedSite{
          .file = file,
          .line = ref.line,
          .reason = "System caller (System files are never edited)",
          .text = std::string(trim(raw)),
        });
        continue;
      }
      const auto callerDir = (self ? self->newPath : file).parent_path();
      const auto newTarget = target ? target->newPath : resolved->target;
      const auto rewritten = formatMovePath(layout, ref.path, resolved->documentRelative, callerDir, newTarget);
      if (!rewritten)
      {
        plan.skipped.push_back(MoveSkippedSite{
          .file = file,
          .line = ref.line,
          .reason = "no $DIR_ path reaches the new location",
          .text = std::string(trim(raw)),
        });
        continue;
      }
      if (*rewritten == ref.path)
      {
        continue;
      }
      fileEdits.edits.push_back(MoveTextEdit{
        .line = ref.line,
        .startCharacter = static_cast<int>(utf16Offset(raw, ref.begin)),
        .endCharacter = static_cast<int>(utf16Offset(raw, ref.end)),
        .newText = *rewritten,
      });
      ++plan.updatedCount;
    }

    if (mentions)
    {
      for (int i = 0; i < static_cast<int>(lines.size()); ++i)
      {
        if (structuralLines.contains(i))
        {
          continue;
        }
        const auto raw = lineText(i);
        const auto trimmed = trim(raw);
        if (trimmed.empty() || trimmed.front() == '!')
        {
          continue;
        }
        const auto bang = findBangCommentIndex(raw);
        const auto code = toLowerAscii(bang >= 0 ? raw.substr(0, static_cast<std::size_t>(bang)) : raw);
        const bool hit = std::any_of(names.begin(), names.end(), [&](const std::string& name) {
          return containsName(code, name);
        });
        if (hit)
        {
          plan.skipped.push_back(MoveSkippedSite{
            .file = file,
            .line = i,
            .reason = "dynamic path or text mention",
            .text = std::string(trimmed),
          });
        }
      }
    }

    if (!fileEdits.edits.empty())
    {
      fileEdits.version = input.openVersion ? input.openVersion(file) : std::nullopt;
      plan.edits.push_back(std::move(fileEdits));
    }
  }
  return plan;
}

auto programMoveEditJson(const ProgramMovePlan& plan, bool withRename) -> lsp::json::Value
{
  lsp::json::Object edit;
  if (!withRename)
  {
    lsp::json::Object changes;
    for (const auto& file : plan.edits)
    {
      lsp::json::Array edits;
      for (const auto& item : file.edits)
      {
        edits.push_back(textEditJson(item, false));
      }
      changes[uriText(file.file)] = lsp::json::Value(std::move(edits));
    }
    edit["changes"] = lsp::json::Value(std::move(changes));
    return lsp::json::Value(std::move(edit));
  }

  lsp::json::Array documentChanges;
  for (const auto& file : plan.edits)
  {
    lsp::json::Object textDocument;
    textDocument["uri"] = lsp::json::Value(uriText(file.file));
    textDocument["version"] = file.version ? lsp::json::Value(*file.version) : lsp::json::Value(nullptr);
    lsp::json::Array edits;
    for (const auto& item : file.edits)
    {
      edits.push_back(textEditJson(item, true));
    }
    lsp::json::Object change;
    change["textDocument"] = lsp::json::Value(std::move(textDocument));
    change["edits"] = lsp::json::Value(std::move(edits));
    documentChanges.push_back(lsp::json::Value(std::move(change)));
  }
  for (const auto& move : plan.fileMoves)
  {
    lsp::json::Object rename;
    rename["kind"] = lsp::json::Value(std::string("rename"));
    rename["oldUri"] = lsp::json::Value(uriText(move.oldPath));
    rename["newUri"] = lsp::json::Value(uriText(move.newPath));
    rename["annotationId"] = lsp::json::Value(std::string(kAnnotationId));
    documentChanges.push_back(lsp::json::Value(std::move(rename)));
  }
  lsp::json::Object annotation;
  annotation["label"] = lsp::json::Value(std::string("Move program"));
  annotation["needsConfirmation"] = lsp::json::Value(true);
  annotation["description"] = lsp::json::Value(std::string("Rename the file and rewrite the paths that reach it"));
  lsp::json::Object annotations;
  annotations[std::string(kAnnotationId)] = lsp::json::Value(std::move(annotation));
  edit["documentChanges"] = lsp::json::Value(std::move(documentChanges));
  edit["changeAnnotations"] = lsp::json::Value(std::move(annotations));
  return lsp::json::Value(std::move(edit));
}

auto programMoveReportJson(const ProgramMovePlan& plan) -> lsp::json::Value
{
  lsp::json::Array skipped;
  for (const auto& site : plan.skipped)
  {
    lsp::json::Object item;
    item["uri"] = lsp::json::Value(uriText(site.file));
    item["line"] = lsp::json::Value(site.line);
    item["reason"] = lsp::json::Value(site.reason);
    item["text"] = lsp::json::Value(site.text);
    skipped.push_back(lsp::json::Value(std::move(item)));
  }
  lsp::json::Array warnings;
  for (const auto& warning : plan.warnings)
  {
    warnings.push_back(lsp::json::Value(warning));
  }
  lsp::json::Object report;
  report["updatedCount"] = lsp::json::Value(plan.updatedCount);
  report["skipped"] = lsp::json::Value(std::move(skipped));
  report["warnings"] = lsp::json::Value(std::move(warnings));
  return lsp::json::Value(std::move(report));
}

} // namespace vertexadt
