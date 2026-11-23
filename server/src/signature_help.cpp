#include "signature_help.h"
#include "utf16_text.h"

#include "bracket_balance.h"
#include "call_parameters.h"
#include "catalog/catalog.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

// Labels follow server.js's onSignatureHelp with lowercase command-catalog
// lookup, labels from catalog structure (never keyword-catalog insertText, min
// prefix for unbounded maxima), and parenthesized `call(` labelled `path` plus
// the callee's remaining-param extract. The active command comes from a bracket
// walk of the logical line (docs/adr/0022-signature-box-follows-the-cursor.md).
// Text is scanned in UTF-16 code units so the cursor column and `\s` match JS.

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

struct CommandContext
{
  U16View name;
  // In the logical line's text.
  std::size_t nameStart = 0;
  std::size_t openParen = 0;
  bool hadParens = false;
  bool inParens = false;
  std::size_t insideCommas = 0;
  std::size_t afterCommas = 0;
};

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
  // Parallel to `inside` for `call(`, else empty. "" is no documentation.
  std::vector<std::string> insideDocs;
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
// Reads past the cursor, so a cursor inside the path sees the whole path.
auto callFirstArgument(U16View text, std::size_t openParen) -> std::string
{
  const auto a = skipJsSpace(text, openParen + 1);
  if (a >= text.size())
  {
    return {};
  }
  for (const char16_t quote : {u'"', u'\''})
  {
    if (text[a] == quote)
    {
      const auto close = text.find(quote, a + 1);
      if (close != U16View::npos && close >= a + 2)
      {
        return utf16To8(text.substr(a, close + 1 - a));
      }
    }
  }
  auto end = a;
  while (end < text.size() && text[end] != u',' && text[end] != u')')
  {
    ++end;
  }
  return utf16To8(text.substr(a, end - a));
}

auto callLabels(const SignatureHelpInput& input, U16View text, std::size_t openParen)
  -> Labels
{
  Labels labels;
  const auto target = callTargetFromFirstArgument(callFirstArgument(text, openParen));
  auto parameters = calleeParameters(input.layout, input.textForPath, target);
  if (parameters.empty())
  {
    // No callee or no inputs: nothing to show.
    return labels;
  }
  labels.inside.emplace_back("path");
  labels.insideDocs.emplace_back();
  for (auto& parameter : parameters)
  {
    const auto index = labels.inside.size();
    labels.insideDocs.push_back(
      parameter.comment ? "Input Parameter " + std::to_string(index) + ": " + *parameter.comment
                        : std::string{});
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
  for (std::size_t i = 0; i < all.size(); ++i)
  {
    lsp::ParameterInformation parameter;
    parameter.label = std::move(all[i]);
    if (context.inParens && i < labels.insideDocs.size() && !labels.insideDocs[i].empty())
    {
      parameter.documentation = labels.insideDocs[i];
    }
    parameters.push_back(std::move(parameter));
  }
  signature.parameters = std::move(parameters);

  lsp::SignatureHelp help;
  help.signatures.push_back(std::move(signature));
  help.activeSignature = 0;
  help.activeParameter = static_cast<lsp::Uint>(active);
  return help;
}

// The cursor's statement text: its line and the lines before it joined by a
// trailing `\`.
struct LogicalLine
{
  // `;!` comments and `\` dropped on the earlier lines; the cursor's line whole.
  U16 text;
  // Where the cursor is in `text`.
  std::size_t cursor = 0;
  // Start of each physical line in `text`, and the first one's number.
  std::vector<std::size_t> starts;
  std::size_t firstLine = 0;

  [[nodiscard]] auto positionOf(std::size_t at) const -> lsp::Position
  {
    const auto next = std::upper_bound(starts.begin(), starts.end(), at);
    const auto row = static_cast<std::size_t>(next - starts.begin()) - 1;
    return {
      .line = static_cast<lsp::Uint>(firstLine + row),
      .character = static_cast<lsp::Uint>(at - starts[row]),
    };
  }
};

auto logicalLine(const Snapshot& snapshot, lsp::Position position) -> std::optional<LogicalLine>
{
  const auto row = lineAt(snapshot.text, position.line);
  // A blank line can still continue the one before it.
  if (!row || (isNonCodeLine(*row) && !jsTrim(utf8To16(*row)).empty()))
  {
    return std::nullopt;
  }
  std::vector<std::string_view> before;
  for (auto line = static_cast<std::size_t>(position.line); line > 0; --line)
  {
    const auto previous = lineAt(snapshot.text, line - 1);
    if (!previous || isNonCodeLine(*previous))
    {
      break;
    }
    const auto span = codeOf(*previous);
    if (!span.continued)
    {
      break;
    }
    before.push_back(span.code);
  }
  LogicalLine out;
  out.firstLine = position.line - before.size();
  for (auto it = before.rbegin(); it != before.rend(); ++it)
  {
    out.starts.push_back(out.text.size());
    out.text += utf8To16(*it);
    out.text += u' ';
  }
  out.starts.push_back(out.text.size());
  const auto current = utf8To16(*row);
  out.cursor = out.text.size() + std::min<std::size_t>(position.character, current.size());
  out.text += current;
  return out;
}

// `[A-Za-z][A-Za-z0-9_]*` with optional spaces, ending at `end`.
auto nameEndingAt(U16View text, std::size_t end) -> std::optional<std::pair<std::size_t, U16View>>
{
  while (end > 0 && isJsSpace(text[end - 1]))
  {
    --end;
  }
  auto start = end;
  while (start > 0 && isIdentPart(text[start - 1]))
  {
    --start;
  }
  while (start < end && !isIdentStart(text[start]))
  {
    ++start;
  }
  if (start == end)
  {
    return std::nullopt;
  }
  return std::pair{start, text.substr(start, end - start)};
}

bool takesAfterParams(U16View name)
{
  const auto* entry = catalog::findCommand(lowerAscii(name));
  return entry && !catalogLabels(*entry).after.empty();
}

// The commands whose arguments hold the cursor, innermost first: the open
// parentheses, then the command whose after-paren or unparenthesized arguments
// are at the top level. Commas in quotes or nested brackets belong to what
// encloses them. Empty in a `;!` comment.
auto enclosingCommands(const LogicalLine& line) -> std::vector<CommandContext>
{
  const auto text = U16View(line.text).substr(0, line.cursor);
  struct Frame
  {
    char16_t kind = u'(';
    CommandContext context;
  };
  std::vector<Frame> stack;
  std::optional<CommandContext> top;
  bool statementStart = true;
  char16_t quote = 0;
  std::size_t nextStart = 1;
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    if (nextStart < line.starts.size() && i == line.starts[nextStart])
    {
      quote = 0; // A string never spans physical lines.
      ++nextStart;
    }
    const auto ch = text[i];
    if (quote)
    {
      if (ch == quote)
      {
        quote = 0;
      }
      continue;
    }
    if (isJsSpace(ch))
    {
      continue;
    }
    if (ch == u';')
    {
      if (i + 1 < text.size() && text[i + 1] == u'!')
      {
        return {};
      }
      if (stack.empty())
      {
        top.reset();
        statementStart = true;
        continue;
      }
    }
    if (isIdentStart(ch) && (i == 0 || !isJsWord(text[i - 1])))
    {
      auto end = i + 1;
      while (end < text.size() && isIdentPart(text[end]))
      {
        ++end;
      }
      const auto word = text.substr(i, end - i);
      if (stack.empty())
      {
        const auto lowered = lowerAscii(word);
        if (lowered == "then" || lowered == "else")
        {
          top.reset();
          statementStart = true;
        }
        else
        {
          const auto next = skipJsSpace(line.text, end);
          if (statementStart && end < text.size() && isJsSpace(text[end])
              && (next >= line.text.size() || line.text[next] != u'='))
          {
            top = CommandContext{.name = word, .nameStart = i};
          }
          statementStart = false;
        }
      }
      i = end - 1;
      continue;
    }
    statementStart = false;
    if (ch == u'"' || ch == u'\'')
    {
      quote = ch;
    }
    else if (ch == u'(' || ch == u'[')
    {
      Frame frame{.kind = ch, .context = {.openParen = i, .hadParens = true, .inParens = true}};
      if (const auto name = ch == u'(' ? nameEndingAt(text, i) : std::nullopt)
      {
        frame.context.nameStart = name->first;
        frame.context.name = name->second;
        if (stack.empty() && top && !top->hadParens && top->nameStart == name->first)
        {
          top.reset(); // `cube (`: parenthesized after all.
        }
      }
      stack.push_back(frame);
    }
    else if (ch == u']')
    {
      if (!stack.empty() && stack.back().kind == u'[')
      {
        stack.pop_back();
      }
    }
    else if (ch == u')')
    {
      while (!stack.empty() && stack.back().kind == u'[')
      {
        stack.pop_back();
      }
      if (stack.empty())
      {
        continue;
      }
      auto closed = stack.back().context;
      stack.pop_back();
      if (stack.empty() && takesAfterParams(closed.name))
      {
        closed.inParens = false;
        top = closed;
      }
    }
    else if (ch == u',')
    {
      if (!stack.empty())
      {
        ++stack.back().context.insideCommas;
      }
      else if (top)
      {
        ++top->afterCommas;
      }
    }
  }
  std::vector<CommandContext> out;
  for (auto it = stack.rbegin(); it != stack.rend(); ++it)
  {
    if (!it->context.name.empty())
    {
      out.push_back(it->context);
    }
  }
  if (top)
  {
    out.push_back(*top);
  }
  return out;
}

auto helpFor(const SignatureHelpInput& input, const LogicalLine& line, const CommandContext& context)
  -> lsp::TextDocumentSignatureHelpResult
{
  const auto lowered = lowerAscii(context.name);
  if (lowered == "call" && context.hadParens)
  {
    return assemble(context, callLabels(input, line.text, context.openParen));
  }
  const auto* entry = catalog::findCommand(lowered);
  if (!entry)
  {
    return nullptr;
  }
  return assemble(context, catalogLabels(*entry));
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
  const auto line = logicalLine(input.snapshot, input.position);
  if (!line)
  {
    return nullptr;
  }
  for (const auto& context : enclosingCommands(*line))
  {
    auto help = helpFor(input, *line, context);
    if (!help.isNull())
    {
      return help;
    }
  }
  return nullptr;
}

auto signatureHelpSites(const SignatureHelpInput& input) -> std::vector<lsp::Position>
{
  std::vector<lsp::Position> sites;
  if (input.snapshot.languageId != "vertexadt")
  {
    return sites;
  }
  const auto line = logicalLine(input.snapshot, input.position);
  if (!line)
  {
    return sites;
  }
  for (const auto& context : enclosingCommands(*line))
  {
    if (!helpFor(input, *line, context).isNull())
    {
      sites.push_back(line->positionOf(context.nameStart));
    }
  }
  return sites;
}

} // namespace vertexadt
