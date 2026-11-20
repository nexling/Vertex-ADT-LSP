#include "validate.h"

#include "concat_whitespace.h"
#include "substring_index.h"
#include "text_util.h"

#include <unordered_set>
#include <utility>
#include <variant>

namespace vertexadt {
namespace {


struct NodebugRange
{
  int start = 0;
  int end = 0;
};

bool skipSharedPerLine(ValidateContext& ctx, PerLineState& line)
{
  if (isSharedPerLineSkip(line.raw))
  {
    return true;
  }

  line.stripped = stripValidationComment(line.raw);
  line.trimmed = trim(line.stripped);
  line.lowercaseTrimmed = toLowerAscii(line.trimmed);
  const auto bang = findInlineBangCommentIndex(line.stripped);
  line.codeLine = bang < 0 ? line.stripped : line.stripped.substr(0, static_cast<std::size_t>(bang));
  line.indentation = -1;
  for (std::size_t i = 0; i < line.stripped.size(); ++i)
  {
    const auto ch = static_cast<unsigned char>(line.stripped[i]);
    if (ch != ' ' && ch != '\t' && ch != '\r')
    {
      line.indentation = static_cast<int>(i);
      break;
    }
  }

  if (lineInVerificationIgnoredSection(line.index, ctx.prep->sections))
  {
    if (!ctx.atCap())
    {
      silentIfLoopIndentStackUpdate(ctx, line);
    }
    return true;
  }
  return false;
}

void familyPerLine(ValidateContext& ctx)
{
  PerLineState line;
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    ctx.pollCancel();
    line = {};
    line.index = i;
    line.raw = ctx.prep->lines[static_cast<std::size_t>(i)];
    if (skipSharedPerLine(ctx, line))
    {
      continue;
    }
    // Spec: the loop continues at cap; inline checks and stack updates stop.
    if (ctx.atCap())
    {
      continue;
    }
    familyLeadingDigit(ctx, line);
    familyIfLogicalAndMatching(ctx, line);
    familyUsedBeforeDeclaration(ctx, line);
    familyEqualsAngle(ctx, line);
    familyCommandSyntax(ctx, line);
    familyIndentLine(ctx, line);
  }
}

void runPostLoop(ValidateContext& ctx, void (*family)(ValidateContext&), bool ignoreCap)
{
  ctx.pollCancel();
  if (!ignoreCap && ctx.atCap())
  {
    return;
  }
  family(ctx);
}

auto diagnosticMessage(const lsp::Diagnostic& diagnostic) -> std::string
{
  if (const auto* text = std::get_if<std::string>(&diagnostic.message))
  {
    return *text;
  }
  if (const auto* markup = std::get_if<lsp::MarkupContent>(&diagnostic.message))
  {
    return markup->value;
  }
  return {};
}

auto nodebugRanges(const std::vector<std::string_view>& lines) -> std::vector<NodebugRange>
{
  std::vector<NodebugRange> ranges;
  int activeStart = -1;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i)
  {
    const auto line = lines[static_cast<std::size_t>(i)];
    const auto hasStart = line.find("@nodebugstart") != std::string_view::npos;
    const auto hasEnd = line.find("@nodebugend") != std::string_view::npos;
    if (hasStart && activeStart == -1)
    {
      activeStart = i;
    }
    else if (hasEnd)
    {
      if (activeStart != -1)
      {
        ranges.push_back(NodebugRange{.start = activeStart, .end = i});
        activeStart = -1;
      }
    }
    else if (hasStart && activeStart != -1)
    {
      activeStart = i;
    }
  }
  return ranges;
}

bool inNodebug(int line, const std::vector<NodebugRange>& ranges)
{
  for (const auto& range : ranges)
  {
    if (line >= range.start && line <= range.end)
    {
      return true;
    }
  }
  return false;
}

void postFilter(ValidateContext& ctx)
{
  const auto ranges = nodebugRanges(ctx.prep->lines);
  std::vector<lsp::Diagnostic> kept;
  std::unordered_set<std::string> seen;
  kept.reserve(ctx.diagnostics.size());
  for (auto& diagnostic : ctx.diagnostics)
  {
    const auto line = static_cast<int>(diagnostic.range.start.line);
    if (lineInVerificationIgnoredSection(line, ctx.prep->sections))
    {
      continue;
    }
    if (inNodebug(line, ranges))
    {
      continue;
    }
    auto key = diagnosticMessage(diagnostic) + std::to_string(line);
    // One concat-whitespace diagnostic per `&`, and one substring diagnostic
    // per index, so several share a line.
    const auto* code = diagnostic.code ? std::get_if<std::string>(&*diagnostic.code) : nullptr;
    if (code && (*code == kConcatWhitespaceCode || isSubstringCode(*code)))
    {
      key += ":" + std::to_string(diagnostic.range.start.character);
    }
    if (!seen.insert(key).second)
    {
      continue;
    }
    kept.push_back(std::move(diagnostic));
  }
  ctx.diagnostics = std::move(kept);
}

} // namespace

bool ValidateContext::atCap() const
{
  return problems >= settings.maxNumberOfProblems;
}

void ValidateContext::pollCancel() const
{
  if (throwIfCanceled)
  {
    throwIfCanceled();
  }
}

void ValidateContext::append(
  lsp::Diagnostic diagnostic,
  bool ignoreCap,
  bool bumpProblems)
{
  if (!ignoreCap && atCap())
  {
    return;
  }
  diagnostics.push_back(std::move(diagnostic));
  if (bumpProblems)
  {
    ++problems;
  }
}

auto buildDocumentPrep(
  Snapshot snapshot,
  const ValidateSettings& settings,
  const ValidateStores& stores,
  const std::function<void()>& throwIfCanceled,
  const CrossCallInjection* injection) -> std::shared_ptr<const DocumentPrep>
{
  auto prep = std::make_shared<DocumentPrep>();
  ValidateContext ctx;
  ctx.prep = prep;
  ctx.buildingTypes = &prep->types;
  ctx.settings = settings;
  ctx.stores = stores;
  ctx.throwIfCanceled = throwIfCanceled;

  ctx.pollCancel();
  prep->snapshot = std::move(snapshot);
  prep->strictModeEnabled = settings.strictModeEnabled;
  prep->lines = splitLines(prep->snapshot.text);
  prep->sections = parseFileSections(prep->lines);
  prep->live = fillLiveBufferExtract(prep->snapshot.text);
  if (stores.extracts)
  {
    stores.extracts->putLive(prep->snapshot.uri, prep->snapshot.version, prep->live);
  }

  const auto state = stores.extracts
    ? stores.extracts->neighborhoodState(prep->snapshot.uri)
    : NeighborhoodState{};
  if (stores.extracts && !state.dirty)
  {
    prep->neighborhoodGeneration = state.generation;
    if (state.extract)
    {
      prep->neighborhood = *state.extract;
    }
  }
  else if (stores.layout && stores.textForPath)
  {
    prep->neighborhood = fillIncludeNeighborhoodExtract(
      prep->snapshot.text,
      pathFromUri(prep->snapshot.uri),
      *stores.layout,
      stores.index,
      settings.inferIncludedFrom,
      stores.textForPath);
    if (stores.extracts)
    {
      prep->neighborhoodGeneration =
        stores.extracts->putNeighborhood(prep->snapshot.uri, prep->neighborhood);
    }
  }
  else
  {
    prep->neighborhoodGeneration = state.generation;
  }

  CrossCallAnalysis analysis;
  if (injection)
  {
    ctx.injection = injection;
  }
  else if (stores.crossCall && stores.layout)
  {
    const CrossCallEnvironment env{
      .layout = stores.layout,
      .index = stores.index,
      .textForPath = stores.textForPath,
      .includedParents = stores.includedParents,
      .strictModeEnabled = settings.strictModeEnabled,
      .pollCancel = throwIfCanceled,
    };
    analysis = stores.crossCall->analyze(
      pathFromUri(prep->snapshot.uri), prep->snapshot.text, env);
    prep->crossCall = analysis.report;
    ctx.injection = &analysis.injection;
  }

  if (analysis.sameInjectionPrep)
  {
    prep->types = analysis.sameInjectionPrep->types;
  }
  else
  {
    collectTypeTimeline(ctx);
  }
  ctx.injection = nullptr;
  prep->types.byName.clear();
  for (std::size_t i = 0; i < prep->types.writes.size(); ++i)
  {
    prep->types.byName[prep->types.writes[i].name].push_back(i);
  }
  return prep;
}

auto fullDocumentValidate(
  ValidateContext ctx,
  const Snapshot& snapshot,
  const PrepProvider& prepare) -> std::vector<lsp::Diagnostic>
{
  if (snapshot.languageId != "vertexadt")
  {
    return {};
  }
  if (isValidationExcluded(snapshot.text))
  {
    return {};
  }

  ctx.prep = prepare
    ? prepare()
    : buildDocumentPrep(snapshot, ctx.settings, ctx.stores, ctx.throwIfCanceled);
  ctx.diagnostics.clear();
  ctx.problems = 0;
  ctx.pollCancel();
  familyVectorToValue(ctx);
  familyPerLine(ctx);
  runPostLoop(ctx, familyBracketBalance, false);
  runPostLoop(ctx, familyLeftoverIf, true);
  runPostLoop(ctx, familyLeftoverLoop, true);
  runPostLoop(ctx, familyLeftoverIndent, true);
  runPostLoop(ctx, familyUnusedVariables, false);
  runPostLoop(ctx, familyCallParamCount, false);
  runPostLoop(ctx, familyCrossCallTypes, false);
  runPostLoop(ctx, familyUnknownSection, false);
  runPostLoop(ctx, familyIncludedFromNotFound, true);
  runPostLoop(ctx, familyUnclosedResources, false);
  runPostLoop(ctx, familyInFileDeclarationDuplicates, true);
  runPostLoop(ctx, familyRedeclaration, false);
  runPostLoop(ctx, familyLocalSlotsManifest, false);
  runPostLoop(ctx, familyCalleeCommonRequirements, false);
  runPostLoop(ctx, familyCrossFileDeclarationConflicts, false);
  runPostLoop(ctx, familyLocalSlotNameConflicts, false);
  runPostLoop(ctx, familyCalleeLocalNaming, false);
  runPostLoop(ctx, familyJointSection, false);
  runPostLoop(ctx, familyConcatWhitespace, false);
  runPostLoop(ctx, familySubstringIndex, false);
  postFilter(ctx);
  return std::move(ctx.diagnostics);
}

} // namespace vertexadt
