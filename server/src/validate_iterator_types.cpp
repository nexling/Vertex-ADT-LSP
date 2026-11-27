#include "iterator_types.h"
#include "utf16_text.h"
#include "validate.h"

namespace vertexadt {

// Iterator first-value checks (docs/adr/0030-typed-iterator-items.md): a
// CONNECTIONS / CONNECTIONS_HC iterator with a first value other than `""` is
// an error; an unknown first value is information. Always on.
void familyIteratorTypes(ValidateContext& ctx)
{
  if (ctx.atCap())
  {
    return;
  }
  const auto& text = ctx.prep->snapshot.text;
  // Cheap gate: most files have no iterator at all.
  if (text.find("_iterator") == std::string::npos && text.find("_ITERATOR") == std::string::npos
    && text.find("_Iterator") == std::string::npos)
  {
    return;
  }
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    ctx.pollCancel();
    if (ctx.atCap())
    {
      return;
    }
    const auto raw = ctx.prep->lines[static_cast<std::size_t>(i)];
    if (isSharedPerLineSkip(raw))
    {
      continue;
    }
    const auto line = stripValidationComment(raw);
    for (const auto& site : findIteratorSites(line))
    {
      if (site.problem == IteratorProblem::None)
      {
        continue;
      }
      const bool mismatch = site.problem == IteratorProblem::ConnectionsMismatch;
      lsp::Diagnostic diagnostic;
      diagnostic.range = lsp::Range{
        .start = {static_cast<lsp::Uint>(i),
          static_cast<lsp::Uint>(utf16Offset(raw, static_cast<std::size_t>(site.problemStart)))},
        .end = {static_cast<lsp::Uint>(i),
          static_cast<lsp::Uint>(utf16Offset(raw, static_cast<std::size_t>(site.problemEnd)))},
      };
      diagnostic.message = site.message;
      diagnostic.source = "ADT-Syntax";
      diagnostic.severity = mismatch ? lsp::DiagnosticSeverity::Error : lsp::DiagnosticSeverity::Information;
      diagnostic.code = std::string(mismatch ? kIteratorConnectionsTypeCode : kIteratorUnknownTypeCode);
      ctx.append(std::move(diagnostic));
    }
  }
}

} // namespace vertexadt
