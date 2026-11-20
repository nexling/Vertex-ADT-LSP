#include "validate.h"
#include "utf16_text.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

struct DeclarationBlock
{
  int start = 0;
  int end = 0;
};

struct RedeclOccupant
{
  double number = 0;
  int line = 0;
  std::string lineText;
};

const std::regex& assignNameRe()
{
  static const std::regex re{R"re(\b[A-Za-z][A-Za-z0-9_-]*\b(?=\s*=))re"};
  return re;
}

const std::regex& assignLettersRe()
{
  static const std::regex re{R"re(\b[A-Za-z]*\b(?!$)(?=\s*=))re"};
  return re;
}

bool skipSharedMarkerLine(
  std::string_view raw,
  int index,
  const std::vector<NamedSection>& sections)
{
  const auto originalTrimmed = trim(raw);
  if (originalTrimmed.starts_with("!"))
  {
    return true;
  }
  if (originalTrimmed.starts_with("messagebox"))
  {
    return true;
  }
  if (originalTrimmed.empty() && raw == "\r")
  {
    return true;
  }
  if (originalTrimmed.starts_with("##"))
  {
    return true;
  }
  if (originalTrimmed.ends_with("##"))
  {
    return true;
  }
  if (originalTrimmed.starts_with("debug"))
  {
    return true;
  }
  if (!originalTrimmed.empty()
      && originalTrimmed.front() == '#'
      && originalTrimmed.back() == '#')
  {
    return true;
  }
  if (originalTrimmed.empty())
  {
    return true;
  }
  if (originalTrimmed.starts_with("# attributes on"))
  {
    return true;
  }
  if (lineInVerificationIgnoredSection(index, sections))
  {
    return true;
  }
  return false;
}

auto family10Blocks(const ValidateContext& ctx) -> std::vector<DeclarationBlock>
{
  std::vector<DeclarationBlock> blocks;
  int currentStart = -1;
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    const auto raw = ctx.prep->lines[static_cast<std::size_t>(i)];
    if (skipSharedMarkerLine(raw, i, ctx.prep->sections))
    {
      continue;
    }
    const auto trimmed = trim(stripValidationComment(raw));
    if (trimmed.starts_with("@declarationnumbercheckstart"))
    {
      currentStart = i;
    }
    else if (trimmed.starts_with("@declarationnumbercheckend") && currentStart != -1)
    {
      blocks.push_back(DeclarationBlock{.start = currentStart, .end = i});
      currentStart = -1;
    }
  }
  return blocks;
}

auto findDeclarationCheckSections(const ValidateContext& ctx) -> std::vector<DeclarationBlock>
{
  std::vector<DeclarationBlock> blocks;
  int currentStart = -1;
  for (int i = 0; i < static_cast<int>(ctx.prep->lines.size()); ++i)
  {
    const auto trimmed = trim(ctx.prep->lines[static_cast<std::size_t>(i)]);
    if (trimmed.starts_with("@declarationnumbercheckstart"))
    {
      currentStart = i;
    }
    else if (trimmed.starts_with("@declarationnumbercheckend") && currentStart != -1)
    {
      blocks.push_back(DeclarationBlock{.start = currentStart, .end = i});
      currentStart = -1;
    }
  }
  return blocks;
}

auto assignmentName(std::string_view fragment) -> std::optional<std::string>
{
  std::cmatch match;
  const char* begin = fragment.data();
  const char* end = begin + fragment.size();
  if (std::regex_search(begin, end, match, assignNameRe()))
  {
    return match[0].str();
  }
  if (std::regex_search(begin, end, match, assignLettersRe()) && !match[0].str().empty())
  {
    return match[0].str();
  }
  return std::nullopt;
}

auto jsNumber(std::string_view raw) -> std::optional<double>
{
  const auto trimmed = trim(raw);
  if (trimmed.empty())
  {
    return 0.0;
  }
  const std::string buf(trimmed);
  char* end = nullptr;
  const auto value = std::strtod(buf.c_str(), &end);
  if (end == buf.c_str() || end != buf.c_str() + buf.size())
  {
    return std::nullopt;
  }
  if (std::isnan(value))
  {
    return std::nullopt;
  }
  return value;
}

auto rhsNumber(std::string_view fragment) -> std::optional<double>
{
  const auto eq = fragment.find('=');
  if (eq == std::string_view::npos)
  {
    return std::nullopt;
  }
  return jsNumber(fragment.substr(eq + 1));
}

auto formatJsNumber(double value) -> std::string
{
  if (std::isfinite(value)
      && value == std::trunc(value)
      && value >= static_cast<double>(std::numeric_limits<long long>::min())
      && value <= static_cast<double>(std::numeric_limits<long long>::max()))
  {
    return std::to_string(static_cast<long long>(value));
  }
  return std::to_string(value);
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

auto declarationVariableType(
  std::string_view varName,
  int lineIndex,
  std::string_view line,
  const TypeTimeline& types) -> std::optional<std::string>
{
  if (auto hover = hoverTypeOnLine(line))
  {
    return hover;
  }
  const TypeWrite* closest = nullptr;
  for (const auto& write : types.writes)
  {
    if (write.name != varName || write.line > lineIndex)
    {
      continue;
    }
    if (!closest || write.line > closest->line)
    {
      closest = &write;
    }
  }
  if (!closest)
  {
    return std::nullopt;
  }
  return closest->type;
}

bool isValueDeclarationType(const std::optional<std::string>& variableType)
{
  return variableType && toLowerAscii(*variableType) == "value";
}

// Node reportProblem123 / buildDeclarationRedeclarationDiagnostics:
// positionAt(lineOffset + 1), so the range starts at character 1.
auto lineRange(int line, std::string_view lineText) -> lsp::Range
{
  return lsp::Range{
    .start = lsp::Position{
      .line = static_cast<lsp::Uint>(line),
      .character = 1,
    },
    .end = lsp::Position{
      .line = static_cast<lsp::Uint>(line),
      .character = static_cast<lsp::Uint>(utf16Length(lineText)),
    },
  };
}

auto wrappedWarning(
  const ValidateContext& ctx,
  int line,
  std::string_view lineText,
  const std::string& inner,
  std::optional<std::string> code,
  lsp::DiagnosticRelatedInformation related) -> lsp::Diagnostic
{
  const auto range = lineRange(line, lineText);
  lsp::Diagnostic diagnostic;
  diagnostic.range = range;
  diagnostic.message = std::string(lineText) + " has an error: " + inner;
  diagnostic.severity = lsp::DiagnosticSeverity::Warning;
  diagnostic.source = std::string("ADT-Syntax");
  if (code)
  {
    diagnostic.code = *code;
  }
  related.location.uri = ctx.prep->snapshot.uri;
  diagnostic.relatedInformation = std::vector<lsp::DiagnosticRelatedInformation>{std::move(related)};
  return diagnostic;
}

template<typename Fn>
void forEachFragment(std::string_view line, Fn&& fn)
{
  std::size_t fragStart = 0;
  while (fragStart <= line.size())
  {
    const auto semi = line.find(';', fragStart);
    const auto fragment = semi == std::string_view::npos
      ? line.substr(fragStart)
      : line.substr(fragStart, semi - fragStart);
    fn(fragment);
    fragStart = semi == std::string_view::npos ? line.size() + 1 : semi + 1;
  }
}

} // namespace

void familyInFileDeclarationDuplicates(ValidateContext& ctx)
{
  ctx.pollCancel();
  const auto blocks = family10Blocks(ctx);
  std::map<std::string, std::map<double, std::string>> occupancy;
  for (const auto& block : blocks)
  {
    ctx.pollCancel();
    for (int i = block.start + 1; i < block.end; ++i)
    {
      const auto sectionName = sectionNameForLine(i, ctx.prep->sections);
      if (isVerificationIgnoredSection(sectionName))
      {
        continue;
      }
      auto& numbers = occupancy[sectionName];
      const auto line = ctx.prep->lines[static_cast<std::size_t>(i)];
      forEachFragment(line, [&](std::string_view fragment) {
        if (isBangComment(fragment))
        {
          return;
        }
        const auto name = assignmentName(fragment);
        if (!name)
        {
          return;
        }
        const auto number = rhsNumber(fragment);
        if (!number)
        {
          return;
        }
        if (isValueDeclarationType(declarationVariableType(*name, i, line, ctx.prep->types)))
        {
          return;
        }
        const auto existing = numbers.find(*number);
        if (existing != numbers.end() && *number > 0.0 && existing->second != *name)
        {
          const auto inner =
            "Variable " + *name + " has declaration number " + formatJsNumber(*number)
            + ", which is already used by " + existing->second + " in this section.";
          auto related = lsp::DiagnosticRelatedInformation{
            .location = lsp::Location{.uri = ctx.prep->snapshot.uri, .range = lineRange(i, line)},
            .message = inner,
          };
          ctx.append(
            wrappedWarning(ctx, i, line, inner, std::nullopt, std::move(related)),
            true);
        }
        numbers[*number] = *name;
      });
    }
  }
}

void familyRedeclaration(ValidateContext& ctx)
{
  ctx.pollCancel();
  if (!ctx.settings.warnOnRedeclaration)
  {
    return;
  }
  const auto blocks = findDeclarationCheckSections(ctx);
  for (const auto& block : blocks)
  {
    ctx.pollCancel();
    std::map<std::string, std::map<std::string, RedeclOccupant>> varsByScope;
    for (int i = block.start + 1; i < block.end; ++i)
    {
      const auto sectionName = sectionNameForLine(i, ctx.prep->sections);
      if (isVerificationIgnoredSection(sectionName))
      {
        continue;
      }
      auto& owners = varsByScope[sectionName];
      const auto line = ctx.prep->lines[static_cast<std::size_t>(i)];
      forEachFragment(line, [&](std::string_view fragment) {
        if (isBangComment(fragment))
        {
          return;
        }
        const auto name = assignmentName(fragment);
        if (!name)
        {
          return;
        }
        const auto number = rhsNumber(fragment);
        if (!number || *number <= 0.0)
        {
          return;
        }
        if (isValueDeclarationType(declarationVariableType(*name, i, line, ctx.prep->types)))
        {
          return;
        }
        const auto existing = owners.find(*name);
        if (existing != owners.end() && existing->second.number != *number)
        {
          const auto previous = formatJsNumber(existing->second.number);
          const auto next = formatJsNumber(*number);
          const auto inner =
            "Variable " + *name + " was declared as " + previous + ", now redeclared as "
            + next + "; slot " + previous + " is no longer bound to " + *name;
          auto related = lsp::DiagnosticRelatedInformation{
            .location = lsp::Location{
              .uri = ctx.prep->snapshot.uri,
              .range = lineRange(existing->second.line, existing->second.lineText),
            },
            .message = "First declaration: " + *name + " = " + previous,
          };
          ctx.append(
            wrappedWarning(
              ctx,
              i,
              line,
              inner,
              std::string("vertexadt/declaration-redeclared"),
              std::move(related)));
        }
        owners[*name] = RedeclOccupant{
          .number = *number,
          .line = i,
          .lineText = std::string(line),
        };
      });
    }
  }
}

} // namespace vertexadt
