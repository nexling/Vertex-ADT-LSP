#include "signature_help.h"
#include "utf16_text.h"

#include "call_parameters.h"
#include "catalog/catalog.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

// Port of server.js's winning onSignatureHelp (getCommandContext plus the
// label assembler), with the spec's deltas: lowercase command-catalog lookup,
// labels from catalog structure (never keyword-catalog insertText, min prefix
// for unbounded maxima), and parenthesized `call(` labelled `path` plus the
// callee's remaining-param extract. The line prefix is scanned in UTF-16 code
// units so the cursor column and `\s` match JS.

namespace vertexadt {
namespace {

bool isIdentStart(char16_t ch)
{
  return (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z');
}

bool isIdentPart(char16_t ch)
{
  return isIdentStart(ch) || (ch >= u'0' && ch <= u'9') || ch == u'_';
}

auto lowerAscii(U16View text) -> std::string
{
  std::string out;
  out.reserve(text.size());
  for (const auto ch : text)
  {
    const auto lowered = (ch >= u'A' && ch <= u'Z') ? ch - u'A' + u'a' : ch;
    out.push_back(lowered < 0x80 ? static_cast<char>(lowered) : '?');
  }
  return out;
}

struct ParenIdent
{
  U16View name;
  std::size_t openParen = 0;
};

// Last non-overlapping /([A-Za-z][A-Za-z0-9_]*)\s*\(/g match.
auto lastParenIdent(U16View prefix) -> std::optional<ParenIdent>
{
  std::optional<ParenIdent> last;
  std::size_t i = 0;
  while (i < prefix.size())
  {
    if (!isIdentStart(prefix[i]))
    {
      ++i;
      continue;
    }
    auto end = i + 1;
    while (end < prefix.size() && isIdentPart(prefix[end]))
    {
      ++end;
    }
    const auto paren = skipJsSpace(prefix, end);
    if (paren < prefix.size() && prefix[paren] == u'(')
    {
      last = ParenIdent{.name = prefix.substr(i, end - i), .openParen = paren};
      i = paren + 1;
    }
    else
    {
      // Every later start inside this word fails the same way.
      i = end;
    }
  }
  return last;
}

struct CommandContext
{
  U16View name;
  bool hadParens = false;
  bool inParens = false;
  std::size_t insideCommas = 0;
  std::size_t afterCommas = 0;
};

// The comma walk from the active `(`: double-quoted strings only, depth-1
// inside commas until the matching `)`, every later comma after it.
auto walkParens(U16View prefix, const ParenIdent& ident) -> CommandContext
{
  CommandContext context{.name = ident.name, .hadParens = true};
  int depth = 1;
  bool inString = false;
  bool closed = false;
  for (auto i = ident.openParen + 1; i < prefix.size(); ++i)
  {
    const auto ch = prefix[i];
    if (inString)
    {
      if (ch == u'"')
      {
        inString = false;
      }
      continue;
    }
    if (ch == u'"')
    {
      inString = true;
    }
    else if (ch == u'(')
    {
      ++depth;
    }
    else if (ch == u')')
    {
      --depth;
      if (depth == 0)
      {
        closed = true;
      }
    }
    else if (ch == u',')
    {
      if (!closed)
      {
        if (depth == 1)
        {
          ++context.insideCommas;
        }
      }
      else
      {
        ++context.afterCommas;
      }
    }
  }
  context.inParens = !closed;
  return context;
}

// Leftmost /(?:^|\s)([A-Za-z][A-Za-z0-9_]*)\s+([^()]*)$/ match.
auto noParenContext(U16View prefix) -> std::optional<CommandContext>
{
  const auto tryAt = [prefix](std::size_t start) -> std::optional<CommandContext>
  {
    if (start >= prefix.size() || !isIdentStart(prefix[start]))
    {
      return std::nullopt;
    }
    auto end = start + 1;
    while (end < prefix.size() && isIdentPart(prefix[end]))
    {
      ++end;
    }
    if (end >= prefix.size() || !isJsSpace(prefix[end]))
    {
      return std::nullopt;
    }
    std::size_t commas = 0;
    for (auto i = end; i < prefix.size(); ++i)
    {
      if (prefix[i] == u'(' || prefix[i] == u')')
      {
        return std::nullopt;
      }
      if (prefix[i] == u',')
      {
        ++commas;
      }
    }
    return CommandContext{
      .name = prefix.substr(start, end - start),
      .afterCommas = commas,
    };
  };

  for (std::size_t p = 0; p < prefix.size(); ++p)
  {
    if (p == 0)
    {
      if (auto context = tryAt(0))
      {
        return context;
      }
    }
    if (isJsSpace(prefix[p]))
    {
      if (auto context = tryAt(p + 1))
      {
        return context;
      }
    }
  }
  return std::nullopt;
}

auto arityCount(catalog::Arity arity) -> std::optional<int>
{
  if (arity.kind == catalog::ArityKind::Finite)
  {
    return arity.value;
  }
  return std::nullopt;
}

// Inside: finite max, else min (omitted or unbounded max). After: finite max,
// else paramsOutside. Unbounded never loops: the finite prefix is emitted.
auto structureLabels(
  std::span<const catalog::StrictTypeSlot> strictSlots,
  int count) -> std::vector<std::string>
{
  std::vector<std::string> labels;
  labels.reserve(static_cast<std::size_t>(count > 0 ? count : 0));
  for (int i = 0; i < count; ++i)
  {
    std::string label = "param" + std::to_string(i + 1);
    for (const auto& slot : strictSlots)
    {
      if (slot.paramIndex == i)
      {
        if (!slot.paramName.empty())
        {
          label = std::string(slot.paramName);
        }
        break;
      }
    }
    labels.push_back(std::move(label));
  }
  return labels;
}

struct Labels
{
  std::vector<std::string> inside;
  std::vector<std::string> after;
};

auto catalogLabels(const catalog::CommandCatalogEntry& entry) -> Labels
{
  const auto& structure = entry.structure;
  const auto inside = arityCount(structure.maxParamsInside)
                        .value_or(arityCount(structure.minParamsInside).value_or(0));
  const auto after = arityCount(structure.maxParamsOutside)
                       .value_or(arityCount(structure.paramsOutside).value_or(0));
  const auto* strict = entry.strictMode;
  return {
    .inside = structureLabels(
      strict ? strict->inputTypes : std::span<const catalog::StrictTypeSlot>{}, inside),
    .after = structureLabels(
      strict ? strict->afterParamTypes : std::span<const catalog::StrictTypeSlot>{}, after),
  };
}

// `call\s*\(\s*((?:"[^"]+"|'[^']+'|[^,)]+))?` anchored at the active `(`.
auto callFirstArgument(U16View prefix, std::size_t openParen) -> std::string
{
  const auto a = skipJsSpace(prefix, openParen + 1);
  if (a >= prefix.size())
  {
    return {};
  }
  for (const char16_t quote : {u'"', u'\''})
  {
    if (prefix[a] == quote)
    {
      const auto close = prefix.find(quote, a + 1);
      if (close != U16View::npos && close >= a + 2)
      {
        return utf16To8(prefix.substr(a, close + 1 - a));
      }
    }
  }
  auto end = a;
  while (end < prefix.size() && prefix[end] != u',' && prefix[end] != u')')
  {
    ++end;
  }
  return utf16To8(prefix.substr(a, end - a));
}

auto callLabels(const SignatureHelpInput& input, U16View prefix, std::size_t openParen)
  -> Labels
{
  Labels labels;
  labels.inside.emplace_back("path");
  const auto target = callTargetFromFirstArgument(callFirstArgument(prefix, openParen));
  for (auto& parameter : calleeParameters(input.layout, input.textForPath, target))
  {
    labels.inside.push_back(std::move(parameter.variableName));
  }
  return labels;
}

auto joinLabels(const std::vector<std::string>& labels) -> std::string
{
  std::string out;
  for (std::size_t i = 0; i < labels.size(); ++i)
  {
    if (i > 0)
    {
      out += ", ";
    }
    out += labels[i];
  }
  return out;
}

auto assemble(const CommandContext& context, const Labels& labels)
  -> lsp::TextDocumentSignatureHelpResult
{
  if (labels.inside.empty() && labels.after.empty())
  {
    return nullptr;
  }
  if (!context.inParens && context.hadParens && labels.after.empty())
  {
    return nullptr;
  }
  const auto& effectiveAfter =
    context.inParens || !labels.after.empty() ? labels.after : labels.inside;

  std::vector<std::string> all;
  std::size_t active = 0;
  std::string label = utf16To8(context.name);
  if (context.inParens)
  {
    if (context.insideCommas >= labels.inside.size())
    {
      return nullptr;
    }
    active = context.insideCommas;
    all = labels.inside;
    all.insert(all.end(), labels.after.begin(), labels.after.end());
    label += "(" + joinLabels(labels.inside) + ")";
    if (!labels.after.empty())
    {
      label += " " + joinLabels(labels.after);
    }
  }
  else
  {
    if (context.afterCommas >= effectiveAfter.size())
    {
      return nullptr;
    }
    active = context.afterCommas;
    all = effectiveAfter;
    label += " " + joinLabels(effectiveAfter);
  }

  lsp::SignatureInformation signature;
  signature.label = std::move(label);
  lsp::Array<lsp::ParameterInformation> parameters;
  parameters.reserve(all.size());
  for (auto& name : all)
  {
    lsp::ParameterInformation parameter;
    parameter.label = std::move(name);
    parameters.push_back(std::move(parameter));
  }
  signature.parameters = std::move(parameters);

  lsp::SignatureHelp help;
  help.signatures.push_back(std::move(signature));
  help.activeSignature = 0;
  help.activeParameter = static_cast<lsp::Uint>(active);
  return help;
}

} // namespace

auto signatureHelpProviderOptions() -> lsp::SignatureHelpOptions
{
  return {
    .triggerCharacters = lsp::Array<lsp::String>{"(", ",", " "},
    .retriggerCharacters = lsp::Array<lsp::String>{",", " "},
  };
}

auto signatureHelp(const SignatureHelpInput& input) -> lsp::TextDocumentSignatureHelpResult
{
  if (input.snapshot.languageId != "vertexadt")
  {
    return nullptr;
  }
  const auto row = lineAt(input.snapshot.text, input.position.line);
  if (!row)
  {
    return nullptr;
  }
  const auto line = utf8To16(*row);
  const auto cursor = std::min<std::size_t>(input.position.character, line.size());
  const auto prefix = U16View(line).substr(0, cursor);

  if (const auto ident = lastParenIdent(prefix))
  {
    const auto lowered = lowerAscii(ident->name);
    if (lowered == "call")
    {
      return assemble(walkParens(prefix, *ident), callLabels(input, prefix, ident->openParen));
    }
    if (const auto* entry = catalog::findCommand(lowered))
    {
      return assemble(walkParens(prefix, *ident), catalogLabels(*entry));
    }
  }

  const auto context = noParenContext(prefix);
  if (!context)
  {
    return nullptr;
  }
  const auto* entry = catalog::findCommand(lowerAscii(context->name));
  if (!entry)
  {
    return nullptr;
  }
  return assemble(*context, catalogLabels(*entry));
}

} // namespace vertexadt
