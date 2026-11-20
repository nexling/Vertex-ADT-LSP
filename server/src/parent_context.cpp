#include "parent_context.h"

#include "text_util.h"

#include <algorithm>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace vertexadt {
namespace {

constexpr std::string_view kDirective = "##included_from";
constexpr int kMaxDepth = 8;

bool sameSection(std::string_view a, std::string_view b)
{
  return toLowerAscii(a) == toLowerAscii(b);
}

auto libraryKey(const LibraryLayout& layout, const std::filesystem::path& path)
  -> std::optional<std::string>
{
  const auto relative = layout.libraryRelativeFromAbs(path);
  if (!relative)
  {
    return std::nullopt;
  }
  return toLowerAscii(*relative);
}

// Innermost `#NAME#` block strictly containing `line`, or null (same rule as
// used-before-assignment's sectionAllows).
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
  return std::any_of(sections.begin(), sections.end(), [&](const NamedSection& section) {
    return section.name == name && section.openLine < line && line < section.closeLine;
  });
}

// Earliest line of `parent` that includes or calls `childKey`. With a
// non-empty `section`, links to that child section (or to the whole child)
// win; otherwise any link counts.
std::optional<int> earliestLinkTo(
  const IndexEntry& parent,
  const std::string& childKey,
  const std::string& section)
{
  std::optional<int> matching;
  std::optional<int> any;
  auto visit = [&](const std::vector<FileLink>& links) {
    for (const auto& link : links)
    {
      if (link.target != childKey)
      {
        continue;
      }
      if (!any || link.line < *any)
      {
        any = link.line;
      }
      if (!section.empty() && (link.targetSection.empty() || sameSection(link.targetSection, section))
          && (!matching || link.line < *matching))
      {
        matching = link.line;
      }
    }
  };
  visit(parent.includeLinks);
  visit(parent.callLinks);
  return matching ? matching : any;
}

struct Contribution
{
  ScopedNames names;
  // Names in first-seen order (keyword labels dropped), for completion.
  std::vector<std::string> orderedVariables;
  std::vector<std::string> orderedStringVariables;
  std::vector<HoverTuple> hoverVariables;
  std::vector<HoverTuple> hoverStringVariables;
};

struct FileContext
{
  ScopedNames outside;
  std::unordered_map<std::string, ScopedNames> bySection; // keyed by lowercased name
  std::vector<std::string> orderedVariables;
  std::vector<std::string> orderedStringVariables;
  std::vector<HoverTuple> hoverVariables;
  std::vector<HoverTuple> hoverStringVariables;
};

void unionInto(ScopedNames& into, const ScopedNames& from)
{
  into.variables.insert(from.variables.begin(), from.variables.end());
  into.stringVariables.insert(from.stringVariables.begin(), from.stringVariables.end());
}

void intersectInto(ScopedNames& into, const ScopedNames& from)
{
  std::erase_if(into.variables, [&](const std::string& name) { return !from.variables.contains(name); });
  std::erase_if(
    into.stringVariables,
    [&](const std::string& name) { return !from.stringVariables.contains(name); });
}

void appendHovers(std::vector<HoverTuple>& into, const std::vector<HoverTuple>& from)
{
  into.insert(into.end(), from.begin(), from.end());
}

void appendNames(std::vector<std::string>& into, const std::vector<std::string>& from)
{
  into.insert(into.end(), from.begin(), from.end());
}

template <typename T>
void appendOrdered(FileContext& context, const T& contribution)
{
  appendNames(context.orderedVariables, contribution.orderedVariables);
  appendNames(context.orderedStringVariables, contribution.orderedStringVariables);
  appendHovers(context.hoverVariables, contribution.hoverVariables);
  appendHovers(context.hoverStringVariables, contribution.hoverStringVariables);
}

const ScopedNames& scopeIn(const FileContext& context, const std::string& sectionName)
{
  if (!sectionName.empty())
  {
    const auto it = context.bySection.find(toLowerAscii(sectionName));
    if (it != context.bySection.end())
    {
      return it->second;
    }
  }
  return context.outside;
}

class Builder
{
public:
  Builder(const LibraryLayout& layout, const WorkspaceIndex* index, bool infer, const TextLookup& textForPath)
    : layout_(layout), index_(index), infer_(infer), textForPath_(textForPath)
  {
  }

  auto build(std::string_view text, const std::filesystem::path& path, int depth) -> FileContext
  {
    FileContext context;
    if (depth > kMaxDepth)
    {
      return context;
    }
    const auto links = resolveParentLinks(text, path, layout_, index_, infer_);
    const auto lines = splitLines(text);
    const auto sections = parseFileSections(lines);

    // Written parents: unioned per scope.
    ScopedNames writtenFile;
    std::map<std::string, ScopedNames> writtenBySection;
    for (const auto& directive : links.written)
    {
      if (!directive.resolved)
      {
        continue;
      }
      const auto contribution = contribute(*directive.resolved, directive.cutoffLine, depth);
      if (!contribution)
      {
        continue;
      }
      if (directive.section.empty())
      {
        unionInto(writtenFile, contribution->names);
      }
      else
      {
        unionInto(writtenBySection[toLowerAscii(directive.section)], contribution->names);
      }
      appendOrdered(context, *contribution);
    }

    // Inferred parents: intersected over the parents that reach a scope.
    auto intersectOver = [&](const std::string& section) -> ScopedNames {
      std::optional<ScopedNames> result;
      for (const auto& parent : links.inferred)
      {
        auto cutoff = parent.wholeLine;
        if (!section.empty())
        {
          for (const auto& [name, line] : parent.sectionLines)
          {
            if (name == section && (!cutoff || line < *cutoff))
            {
              cutoff = line;
            }
          }
        }
        if (!cutoff)
        {
          continue;
        }
        const auto contribution = contribute(parent.absPath, cutoff, depth);
        if (!contribution)
        {
          // An unreadable parent declares nothing.
          return {};
        }
        if (!result)
        {
          result = contribution->names;
        }
        else
        {
          intersectInto(*result, contribution->names);
        }
      }
      return result.value_or(ScopedNames{});
    };
    for (const auto& parent : links.inferred)
    {
      auto cutoff = parent.wholeLine;
      for (const auto& [_, line] : parent.sectionLines)
      {
        if (!cutoff || line < *cutoff)
        {
          cutoff = line;
        }
      }
      if (const auto contribution = contribute(parent.absPath, cutoff, depth))
      {
        appendOrdered(context, *contribution);
      }
    }

    context.outside = links.writtenCoversFile ? writtenFile : intersectOver({});

    std::unordered_set<std::string> sectionNames;
    for (const auto& section : sections)
    {
      sectionNames.insert(toLowerAscii(section.name));
    }
    for (const auto& name : links.writtenSections)
    {
      sectionNames.insert(name);
    }
    for (const auto& name : sectionNames)
    {
      ScopedNames visible;
      if (const auto written = writtenBySection.find(name); written != writtenBySection.end())
      {
        visible = written->second;
        if (links.writtenCoversFile)
        {
          unionInto(visible, writtenFile);
        }
      }
      else if (links.writtenCoversFile)
      {
        visible = writtenFile;
      }
      else
      {
        visible = intersectOver(name);
      }
      context.bySection.emplace(name, std::move(visible));
    }
    return context;
  }

  void markVisiting(const std::filesystem::path& path)
  {
    visiting_.insert(normalizeFsPath(path.string()));
  }

  [[nodiscard]] auto readPaths() const -> const std::vector<std::string>& { return readPaths_; }

private:
  const LibraryLayout& layout_;
  const WorkspaceIndex* index_;
  bool infer_;
  const TextLookup& textForPath_;
  std::vector<std::string> readPaths_;
  std::unordered_set<std::string> readSet_;
  std::unordered_map<std::string, std::shared_ptr<const FileContext>> contexts_;
  std::unordered_set<std::string> visiting_;
  std::map<std::pair<std::string, int>, std::shared_ptr<const Contribution>> contributions_;

  auto contextOf(const std::string& key, std::string_view text, const std::filesystem::path& path, int depth)
    -> std::shared_ptr<const FileContext>
  {
    if (const auto it = contexts_.find(key); it != contexts_.end())
    {
      return it->second;
    }
    if (!visiting_.insert(key).second)
    {
      return std::make_shared<const FileContext>();
    }
    auto built = std::make_shared<const FileContext>(build(text, path, depth + 1));
    visiting_.erase(key);
    contexts_.emplace(key, built);
    return built;
  }

  // What `parentPath` makes visible at `cutoff` (nullopt: the whole parent):
  // names it assigns on earlier lines in the cut-off's section (or outside
  // every section), plus its own parent context there.
  auto contribute(const std::filesystem::path& parentPath, std::optional<int> cutoff, int depth)
    -> std::shared_ptr<const Contribution>
  {
    const auto key = normalizeFsPath(parentPath.string());
    const auto memoKey = std::make_pair(key, cutoff.value_or(-1));
    if (const auto it = contributions_.find(memoKey); it != contributions_.end())
    {
      return it->second;
    }
    if (readSet_.insert(key).second)
    {
      readPaths_.push_back(key);
    }
    const auto text = textForPath_(parentPath);
    if (!text)
    {
      contributions_.emplace(memoKey, nullptr);
      return nullptr;
    }
    const auto sections = parseFileSections(splitLines(*text));
    const auto scanned = fillLiveBufferExtract(linesBeforeIncludePoint(*text, cutoff));

    auto contribution = std::make_shared<Contribution>();
    contribution->names.variables = scanned.variablesSet;
    contribution->names.stringVariables = scanned.stringVariablesSet;
    contribution->orderedVariables = scanned.variables;
    contribution->orderedStringVariables = scanned.stringVariables;
    contribution->hoverVariables = scanned.hoverVariables;
    contribution->hoverStringVariables = scanned.hoverStringVariables;

    const auto parentContext = contextOf(key, *text, parentPath, depth);
    if (cutoff)
    {
      unionInto(contribution->names, scopeIn(*parentContext, sectionNameForLine(*cutoff, sections)));
    }
    else
    {
      unionInto(contribution->names, parentContext->outside);
      for (const auto& [_, scope] : parentContext->bySection)
      {
        unionInto(contribution->names, scope);
      }
    }
    appendNames(contribution->orderedVariables, parentContext->orderedVariables);
    appendNames(contribution->orderedStringVariables, parentContext->orderedStringVariables);
    appendHovers(contribution->hoverVariables, parentContext->hoverVariables);
    appendHovers(contribution->hoverStringVariables, parentContext->hoverStringVariables);

    std::shared_ptr<const Contribution> shared = std::move(contribution);
    contributions_.emplace(memoKey, shared);
    return shared;
  }
};

void dedupeHovers(std::vector<HoverTuple>& hovers, const std::unordered_set<std::string>& visible, bool stringNames)
{
  std::vector<HoverTuple> kept;
  for (auto& tuple : hovers)
  {
    auto key = tuple.name;
    if (stringNames && key.ends_with('$'))
    {
      key.pop_back();
    }
    if (visible.contains(key) || visible.contains(tuple.name))
    {
      kept.push_back(std::move(tuple));
    }
  }
  hovers = std::move(kept);
}

// `ordered` filtered to `visible`, first occurrence only.
auto visibleInOrder(const std::vector<std::string>& ordered, const std::unordered_set<std::string>& visible)
  -> std::vector<std::string>
{
  std::vector<std::string> out;
  std::unordered_set<std::string> seen;
  for (const auto& name : ordered)
  {
    if (visible.contains(name) && seen.insert(name).second)
    {
      out.push_back(name);
    }
  }
  return out;
}

} // namespace

auto linesBeforeIncludePoint(std::string_view text, std::optional<int> cutoff) -> std::string
{
  if (!cutoff)
  {
    return std::string(text);
  }
  const auto lines = splitLines(text);
  const auto sections = parseFileSections(lines);
  std::string kept;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    if (i < *cutoff)
    {
      const auto* site = innermostSection(sections, i);
      if (!site || lineInSectionNamed(sections, site->name, *cutoff))
      {
        kept.append(lines[static_cast<std::size_t>(i)]);
      }
    }
    kept.push_back('\n');
  }
  return kept;
}

auto parseWrittenDirectives(std::string_view text, const LibraryLayout& layout)
  -> std::vector<WrittenDirective>
{
  std::vector<WrittenDirective> directives;
  if (text.find(kDirective) == std::string_view::npos)
  {
    return directives;
  }
  const auto lines = splitLines(text);
  const auto sections = parseFileSections(lines);
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto trimmed = trim(lines[static_cast<std::size_t>(i)]);
    if (!trimmed.starts_with(kDirective))
    {
      continue;
    }
    const auto rawPath = trim(trimmed.substr(kDirective.size()));
    if (rawPath.empty())
    {
      continue;
    }
    directives.push_back(WrittenDirective{
      .line = i,
      .rawPath = std::string(rawPath),
      .section = sectionNameForLine(i, sections),
      .resolved = layout.findIncludedFile(rawPath),
    });
  }
  return directives;
}

auto resolveParentLinks(
  std::string_view text,
  const std::filesystem::path& path,
  const LibraryLayout& layout,
  const WorkspaceIndex* index,
  bool inferParents) -> ParentLinks
{
  ParentLinks links;
  links.written = parseWrittenDirectives(text, layout);
  const auto childKey = libraryKey(layout, path);

  for (auto& directive : links.written)
  {
    if (!directive.resolved)
    {
      continue;
    }
    if (directive.section.empty())
    {
      links.writtenCoversFile = true;
    }
    else
    {
      links.writtenSections.push_back(toLowerAscii(directive.section));
    }
    if (!index || !childKey)
    {
      continue;
    }
    const auto parentKey = libraryKey(layout, *directive.resolved);
    if (!parentKey)
    {
      continue;
    }
    if (const auto entry = index->entry(*parentKey))
    {
      directive.cutoffLine = earliestLinkTo(*entry, *childKey, directive.section);
    }
  }

  if (!inferParents || !index || !childKey || links.writtenCoversFile)
  {
    return links;
  }

  std::map<std::string, InferredParent> byParent;
  for (const auto& incoming : index->incomingLinks(*childKey))
  {
    const auto& parent = *incoming.parent;
    if (parent.identity.key == *childKey)
    {
      continue;
    }
    if (incoming.isCall && !parent.commonIndexes && !parent.commonPoints)
    {
      continue;
    }
    auto& inferred = byParent[parent.identity.key];
    if (inferred.absPath.empty())
    {
      inferred.absPath = parent.identity.absPath;
      inferred.displayPath = parent.identity.displayPath;
    }
    const auto line = incoming.link.line;
    if (incoming.link.targetSection.empty())
    {
      if (!inferred.wholeLine || line < *inferred.wholeLine)
      {
        inferred.wholeLine = line;
      }
      continue;
    }
    const auto section = toLowerAscii(incoming.link.targetSection);
    auto found = std::find_if(inferred.sectionLines.begin(), inferred.sectionLines.end(), [&](const auto& item) {
      return item.first == section;
    });
    if (found == inferred.sectionLines.end())
    {
      inferred.sectionLines.emplace_back(section, line);
    }
    else if (line < found->second)
    {
      found->second = line;
    }
  }
  links.inferredCandidates = static_cast<int>(byParent.size());
  for (auto& [_, parent] : byParent)
  {
    links.candidates.push_back(std::move(parent));
  }
  if (links.inferredCandidates > kMaxInferredParents)
  {
    links.overCap = true;
    return links;
  }
  links.inferred = links.candidates;
  return links;
}

auto fillIncludeNeighborhoodExtract(
  std::string_view rootText,
  const std::filesystem::path& rootPath,
  const LibraryLayout& layout,
  const WorkspaceIndex* index,
  bool inferParents,
  const TextLookup& textForPath) -> VariableExtract
{
  VariableExtract extract;
  if (!inferParents && rootText.find(kDirective) == std::string_view::npos)
  {
    return extract;
  }
  Builder builder(layout, index, inferParents, textForPath);
  builder.markVisiting(rootPath);
  auto context = builder.build(rootText, rootPath, 0);

  ScopedNames all = context.outside;
  for (const auto& [_, scope] : context.bySection)
  {
    unionInto(all, scope);
  }
  extract.variables = visibleInOrder(context.orderedVariables, all.variables);
  extract.stringVariables = visibleInOrder(context.orderedStringVariables, all.stringVariables);
  extract.variablesSet = all.variables;
  extract.stringVariablesSet = all.stringVariables;
  extract.hoverVariables = std::move(context.hoverVariables);
  extract.hoverStringVariables = std::move(context.hoverStringVariables);
  dedupeHovers(extract.hoverVariables, all.variables, false);
  dedupeHovers(extract.hoverStringVariables, all.stringVariables, true);
  extract.outsideSections = std::move(context.outside);
  extract.bySection = std::move(context.bySection);
  extract.includedFromPaths = builder.readPaths();
  return extract;
}

} // namespace vertexadt
