#include "validate.h"

#include "parent_context.h"
#include "utf16_text.h"

namespace vertexadt {

// A written `##included_from` whose path resolves to no file. It counts as
// absent for the include neighborhood, so inference still runs
// (docs/adr/0015-native-inferred-included-from.md).
void familyIncludedFromNotFound(ValidateContext& ctx)
{
  if (!ctx.stores.layout)
  {
    return;
  }
  for (const auto& directive : parseWrittenDirectives(ctx.prep->snapshot.text, *ctx.stores.layout))
  {
    if (directive.resolved)
    {
      continue;
    }
    const auto raw = ctx.prep->lines[static_cast<std::size_t>(directive.line)];
    const auto begin = raw.find(directive.rawPath);
    const auto start = begin == std::string_view::npos ? 0 : begin;
    const auto end = begin == std::string_view::npos ? raw.size() : begin + directive.rawPath.size();
    lsp::Diagnostic diagnostic;
    diagnostic.range.start.line = static_cast<std::uint32_t>(directive.line);
    diagnostic.range.start.character = static_cast<std::uint32_t>(utf16Offset(raw, start));
    diagnostic.range.end.line = static_cast<std::uint32_t>(directive.line);
    diagnostic.range.end.character = static_cast<std::uint32_t>(utf16Offset(raw, end));
    diagnostic.severity = lsp::DiagnosticSeverity::Warning;
    diagnostic.source = std::string("ADT-Syntax");
    diagnostic.code = std::string(kIncludedFromNotFoundCode);
    diagnostic.message = "##included_from file not found: " + directive.rawPath;
    ctx.append(std::move(diagnostic), true);
  }
}

} // namespace vertexadt
