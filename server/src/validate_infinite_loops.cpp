#include "infinite_loops.h"
#include "validate.h"

#include <filesystem>

namespace vertexadt {

void familyInfiniteLoops(ValidateContext& ctx)
{
  if (!ctx.settings.warnInfiniteLoops || ctx.atCap())
  {
    return;
  }
  const auto& text = ctx.prep->snapshot.text;
  // Cheap gate: most files have no bare loop at all.
  if (text.find("loop") == std::string::npos)
  {
    return;
  }

  const IncludeLoader loadInclude = [&ctx](const IncludeCallTarget& target) -> std::optional<IncludedCode> {
    if (!ctx.stores.layout || !ctx.stores.textForPath || target.filePath.empty())
    {
      return std::nullopt;
    }
    const auto resolved = ctx.stores.layout->resolveVertexFilePath(target.filePath);
    if (!resolved)
    {
      return std::nullopt;
    }
    auto content = ctx.stores.textForPath(*resolved);
    if (!content)
    {
      return std::nullopt;
    }
    return IncludedCode{.key = normalizeFsPath(resolved->string()), .text = std::move(*content)};
  };

  for (const auto& issue : findInfiniteLoops(text, loadInclude))
  {
    ctx.pollCancel();
    if (ctx.atCap())
    {
      break;
    }
    lsp::Diagnostic diagnostic;
    diagnostic.range = lsp::Range{
      .start = {static_cast<lsp::Uint>(issue.line), static_cast<lsp::Uint>(issue.startChar)},
      .end = {static_cast<lsp::Uint>(issue.line), static_cast<lsp::Uint>(issue.endChar)},
    };
    diagnostic.message = issue.message;
    diagnostic.source = "ADT-Syntax";
    const bool noExit = issue.kind == InfiniteLoopKind::NoExit;
    diagnostic.severity = noExit ? lsp::DiagnosticSeverity::Warning : lsp::DiagnosticSeverity::Information;
    diagnostic.code = std::string(noExit ? kInfiniteLoopCode : kExitifNeverChangesCode);
    ctx.append(std::move(diagnostic));
  }
}

} // namespace vertexadt
