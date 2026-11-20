#include "validate.h"

#include "cross_call_types.h"
#include "text_util.h"
#include "utf16_text.h"

#include <algorithm>
#include <string>
#include <utility>

namespace vertexadt {
namespace {

constexpr std::string_view kSource = "ADT-Syntax";
constexpr std::string_view kArgumentTypeCode = "vertexadt/cross-call-argument-type";
constexpr std::string_view kCommentTypeCode = "vertexadt/cross-call-comment-type";
constexpr std::string_view kParamConflictCode = "vertexadt/cross-call-param-conflict";
constexpr std::string_view kReturnConflictCode = "vertexadt/cross-call-return-conflict";
constexpr std::string_view kReturnCommentTypeCode = "vertexadt/cross-call-return-comment-type";

std::string_view lineText(const ValidateContext& ctx, int line)
{
  if (line < 0 || line >= static_cast<int>(ctx.prep->lines.size()))
  {
    return {};
  }
  return ctx.prep->lines[static_cast<std::size_t>(line)];
}

lsp::Range rangeOn(const ValidateContext& ctx, int line, std::size_t start, std::size_t end)
{
  const auto text = lineText(ctx, line);
  start = std::min(start, text.size());
  end = std::max(start, std::min(end, text.size()));
  return lsp::Range{
    .start = lsp::Position{
      .line = static_cast<lsp::Uint>(line),
      .character = static_cast<lsp::Uint>(utf16Offset(text, start)),
    },
    .end = lsp::Position{
      .line = static_cast<lsp::Uint>(line),
      .character = static_cast<lsp::Uint>(utf16Offset(text, end)),
    },
  };
}

lsp::DiagnosticRelatedInformation related(const CrossCallLocation& location, std::string message)
{
  lsp::DiagnosticRelatedInformation info;
  info.location.uri = location.uri;
  info.location.range = lsp::Range{
    .start = lsp::Position{
      .line = static_cast<lsp::Uint>(location.line),
      .character = static_cast<lsp::Uint>(location.startCharacter),
    },
    .end = lsp::Position{
      .line = static_cast<lsp::Uint>(location.line),
      .character = static_cast<lsp::Uint>(location.endCharacter),
    },
  };
  info.message = std::move(message);
  return info;
}

void emit(
  ValidateContext& ctx,
  lsp::Range range,
  lsp::DiagnosticSeverity severity,
  std::string_view code,
  std::string message,
  lsp::Array<lsp::DiagnosticRelatedInformation> relatedInfo)
{
  lsp::Diagnostic diagnostic;
  diagnostic.range = range;
  diagnostic.severity = severity;
  diagnostic.source = std::string(kSource);
  diagnostic.code = std::string(code);
  diagnostic.message = std::move(message);
  if (!relatedInfo.empty())
  {
    diagnostic.relatedInformation = std::move(relatedInfo);
  }
  ctx.append(std::move(diagnostic));
}

std::string distinctTypes(const std::vector<CrossCallLocation>& locations)
{
  std::vector<std::string> types;
  for (const auto& location : locations)
  {
    if (std::find(types.begin(), types.end(), location.type) == types.end())
    {
      types.push_back(location.type);
    }
  }
  std::string out;
  for (const auto& type : types)
  {
    out += (out.empty() ? "" : ", ") + type;
  }
  return out;
}

lsp::Array<lsp::DiagnosticRelatedInformation> contributionsInfo(
  const std::vector<CrossCallLocation>& locations,
  std::string_view verb)
{
  lsp::Array<lsp::DiagnosticRelatedInformation> info;
  for (const auto& location : locations)
  {
    info.push_back(related(location, std::string(verb) + " " + location.type));
  }
  return info;
}

void callerArgumentTypes(ValidateContext& ctx, const CrossCallReport& report)
{
  const auto& facts = *report.facts;
  for (const auto& expectation : report.argExpectations)
  {
    ctx.pollCancel();
    const auto& site = facts.calls[expectation.site];
    if (site.ignoreType || expectation.arg >= site.args.size())
    {
      continue;
    }
    const auto& arg = site.args[expectation.arg];
    const auto actual = crossCallArgumentType(*ctx.prep, arg.text, site.line);
    if (actual.empty() || crossCallTypesCompatible(actual, expectation.expectedType))
    {
      continue;
    }
    emit(
      ctx,
      rangeOn(ctx, site.line, arg.start, arg.end),
      lsp::DiagnosticSeverity::Error,
      kArgumentTypeCode,
      "Argument " + std::to_string(expectation.arg + 1) + " '" + arg.text + "' is " + actual
        + " but the callee expects " + expectation.expectedType + " ($CALLPARAM"
        + std::to_string(expectation.arg + 1) + ")",
      {related(expectation.callee, "Declared as " + expectation.expectedType + " here")});
  }
}

void calleeParams(ValidateContext& ctx, const CrossCallReport& report)
{
  const auto& facts = *report.facts;
  for (const auto& param : report.params)
  {
    ctx.pollCancel();
    const auto& read = facts.params[param.read];
    const auto callParam = "$CALLPARAM" + std::to_string(read.index);
    if (param.conflict)
    {
      emit(
        ctx,
        rangeOn(ctx, read.line, read.nameStart, read.nameEnd),
        lsp::DiagnosticSeverity::Warning,
        kParamConflictCode,
        "Callers pass different types for " + read.name + " (" + callParam
          + "): " + distinctTypes(param.contributions),
        contributionsInfo(param.contributions, "Passes"));
      continue;
    }
    if (param.type.empty() || read.commentType.empty()
      || crossCallTypesCompatible(read.commentType, param.type))
    {
      continue;
    }
    emit(
      ctx,
      rangeOn(ctx, read.line, read.commentStart, read.commentEnd),
      lsp::DiagnosticSeverity::Error,
      kCommentTypeCode,
      "Comment types " + read.name + " as " + read.commentType + " but callers pass "
        + param.type + " (" + callParam + ")",
      contributionsInfo(param.contributions, "Passes"));
  }
}

void calleeReturnFields(ValidateContext& ctx, const CrossCallReport& report)
{
  const auto fields =
    crossCallReturnFieldTypes(*ctx.prep, *report.facts, ctx.prep->snapshot.uri);
  for (const auto& field : fields)
  {
    if (!field.conflict)
    {
      continue;
    }
    const auto types = distinctTypes(field.lines);
    for (const auto& line : field.lines)
    {
      ctx.pollCancel();
      lsp::Array<lsp::DiagnosticRelatedInformation> info;
      for (const auto& other : field.lines)
      {
        if (other.line != line.line)
        {
          info.push_back(related(other, "Packed as " + other.type));
        }
      }
      lsp::Range range{
        .start = lsp::Position{
          .line = static_cast<lsp::Uint>(line.line),
          .character = static_cast<lsp::Uint>(line.startCharacter),
        },
        .end = lsp::Position{
          .line = static_cast<lsp::Uint>(line.line),
          .character = static_cast<lsp::Uint>(line.endCharacter),
        },
      };
      emit(
        ctx,
        range,
        lsp::DiagnosticSeverity::Warning,
        kReturnConflictCode,
        "Return field " + field.key + " is packed with different types: " + types,
        std::move(info));
    }
  }
}

void callerReceivers(ValidateContext& ctx, const CrossCallReport& report)
{
  const auto& facts = *report.facts;
  for (const auto& receiver : report.receivers)
  {
    ctx.pollCancel();
    const auto& fact = facts.receivers[receiver.receiver];
    if (receiver.type.empty() || fact.commentType.empty()
      || crossCallTypesCompatible(fact.commentType, receiver.type))
    {
      continue;
    }
    emit(
      ctx,
      rangeOn(ctx, fact.line, fact.commentStart, fact.commentEnd),
      lsp::DiagnosticSeverity::Error,
      kReturnCommentTypeCode,
      "Comment types " + fact.name + " as " + fact.commentType + " but the callee returns "
        + receiver.type + " for " + fact.key,
      contributionsInfo(receiver.sources, "Returned as"));
  }
}

} // namespace

void familyCrossCallTypes(ValidateContext& ctx)
{
  if (!ctx.settings.strictModeEnabled || !ctx.settings.crossCallTypeChecks)
  {
    return;
  }
  const auto& report = ctx.prep->crossCall;
  if (!report || !report->facts)
  {
    return;
  }
  callerArgumentTypes(ctx, *report);
  calleeParams(ctx, *report);
  calleeReturnFields(ctx, *report);
  callerReceivers(ctx, *report);
}

} // namespace vertexadt
