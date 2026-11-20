#include "object_parameter_completion.h"

#include "catalog/catalog.h"
#include "command_parse.h"
#include "text_util.h"
#include "utf16_text.h"
#include "validate.h"

#include <array>
#include <regex>
#include <unordered_set>

// Port of the client host's extractGetCommandInfo + object-param provider
// (client/out/extension.js) and server.js custom/getVariableObjectType,
// custom/getObjectParameters, resolveMacroIdentity / resolveJointIdentity /
// resolveStringParam and getMacroRecordFields / getJointRecordFields, folded
// into ordinary completion. Site patterns are matched by hand in UTF-16.

namespace vertexadt {
namespace {

enum class SlotShape
{
  SecondInside, // `command(arg, ` → the 2nd inside argument
  AfterParen,   // `command(arg) ` → the after-paren argument
};

struct SitePattern
{
  std::u16string_view command;
  SlotShape shape;
};

constexpr std::array<SitePattern, 10> kSitePatterns = {{
  {u"get_obj_param", SlotShape::SecondInside},
  {u"get_material_param", SlotShape::SecondInside},
  {u"get_area_param", SlotShape::SecondInside},
  {u"get_pnt_param", SlotShape::SecondInside},
  {u"get_sheet_param", SlotShape::SecondInside},
  {u"get_joint_param", SlotShape::SecondInside},
  {u"get_section_param", SlotShape::SecondInside},
  {u"set_obj_param", SlotShape::AfterParen},
  {u"set_joint_param", SlotShape::AfterParen},
  {u"set_djparam", SlotShape::AfterParen},
}};

// `\s*\(\s*([^,\s]+)\s*,\s*$` or `\s*\(\s*([^,\s)]+)\s*\)\s*$` from `i`. Every
// quantifier is followed by a character the class excludes, so one greedy
// pass decides the match.
auto matchTail(std::u16string_view text, std::size_t i, SlotShape shape)
  -> std::optional<std::u16string_view>
{
  i = skipJsSpace(text, i);
  if (i >= text.size() || text[i] != u'(')
  {
    return std::nullopt;
  }
  i = skipJsSpace(text, i + 1);
  const auto start = i;
  while (i < text.size() && text[i] != u',' && !isJsSpace(text[i])
    && !(shape == SlotShape::AfterParen && text[i] == u')'))
  {
    ++i;
  }
  if (i == start)
  {
    return std::nullopt;
  }
  const auto capture = text.substr(start, i - start);
  i = skipJsSpace(text, i);
  const auto closer = shape == SlotShape::SecondInside ? u',' : u')';
  if (i >= text.size() || text[i] != closer)
  {
    return std::nullopt;
  }
  i = skipJsSpace(text, i + 1);
  if (i != text.size())
  {
    return std::nullopt;
  }
  return capture;
}

bool isMacroXmlCommand(std::string_view command)
{
  return command == "get_obj_param" || command == "set_obj_param";
}

bool isJointXmlCommand(std::string_view command)
{
  return command == "get_obj_param" || command == "get_joint_param"
    || command == "set_joint_param" || command == "set_djparam"
    || command == "set_obj_param";
}

auto toParameter(const catalog::ObjectParameterRow& row) -> ObjectParameter
{
  const auto copy = [](const std::optional<std::string_view>& value) -> std::optional<std::string> {
    return value ? std::optional<std::string>(std::string(*value)) : std::nullopt;
  };
  return {
    .label = std::string(row.label),
    .documentation = copy(row.documentation),
    .detail = copy(row.detail),
    .insertText = copy(row.insertText),
  };
}

auto stripCr(std::string_view line) -> std::string_view
{
  if (!line.empty() && line.back() == '\r')
  {
    line.remove_suffix(1);
  }
  return line;
}

// new RegExp(source, flags); a source Node would reject makes this family add
// nothing (Node's request would throw).
auto makeRegex(const std::string& source, bool icase) -> std::optional<std::regex>
{
  try
  {
    auto flags = std::regex::ECMAScript;
    if (icase)
    {
      flags |= std::regex::icase;
    }
    return std::regex(source, flags);
  }
  catch (const std::regex_error&)
  {
    return std::nullopt;
  }
}

struct RegexFailure
{
};

// server.js resolveStringParam: a quoted literal, else the nearest `Name = "…"`
// at or above `upToLine`. Empty optional when unresolved.
auto resolveStringParam(
  const std::vector<std::string_view>& lines,
  int upToLine,
  const std::string* expr) -> std::optional<std::string>
{
  if (!expr || expr->empty())
  {
    return std::nullopt;
  }
  const auto trimmed = utf16To8(jsTrim(utf8To16(*expr)));
  static const std::regex literalRe{R"(^["'](.*)["']$)"};
  std::smatch literal;
  if (std::regex_search(trimmed, literal, literalRe))
  {
    return literal[1].str();
  }
  const auto varName = extractBaseVariableName(trimmed);
  const auto assignRe = makeRegex(R"(\b)" + varName + R"(\s*=\s*(.+)$)", false);
  if (!assignRe)
  {
    throw RegexFailure{};
  }
  static const std::regex rhsLiteralRe{R"(^["'](.*)["'])"};
  for (int i = upToLine; i >= 0; --i)
  {
    if (static_cast<std::size_t>(i) >= lines.size())
    {
      continue;
    }
    const auto line = stripValidationComment(stripCr(lines[static_cast<std::size_t>(i)]));
    std::smatch match;
    if (!std::regex_search(line, match, *assignRe))
    {
      continue;
    }
    const auto rhs = utf16To8(jsTrim(utf8To16(match[1].str())));
    std::smatch lit;
    if (std::regex_search(rhs, lit, rhsLiteralRe))
    {
      return lit[1].str();
    }
  }
  return std::nullopt;
}

struct ObjectIdentity
{
  std::string library;
  std::string code;
};

// server.js resolveMacroIdentity / resolveJointIdentity: walk up from the
// cursor line for `{base} = {creator}(` and resolve its first two arguments.
auto resolveIdentity(
  const std::vector<std::string_view>& lines,
  const std::string& variableName,
  int line,
  std::string_view creator) -> std::optional<ObjectIdentity>
{
  const auto baseName = extractBaseVariableName(variableName);
  const auto assignRe = makeRegex(
    R"(\b)" + baseName + R"(\s*=\s*)" + std::string(creator) + R"(\s*\()", true);
  if (!assignRe)
  {
    throw RegexFailure{};
  }
  for (int i = line; i >= 0; --i)
  {
    if (static_cast<std::size_t>(i) >= lines.size())
    {
      continue;
    }
    const auto lineText = stripValidationComment(stripCr(lines[static_cast<std::size_t>(i)]));
    if (!std::regex_search(lineText, *assignRe))
    {
      continue;
    }
    const auto parsed = parseCommand(lineText);
    if (!parsed || toLowerAscii(parsed->functionName) != creator)
    {
      continue;
    }
    const auto* libraryExpr = parsed->insideParams.size() > 0 ? &parsed->insideParams[0] : nullptr;
    const auto* codeExpr = parsed->insideParams.size() > 1 ? &parsed->insideParams[1] : nullptr;
    auto library = resolveStringParam(lines, i, libraryExpr);
    auto code = resolveStringParam(lines, i, codeExpr);
    if (library && !library->empty() && code && !code->empty())
    {
      return ObjectIdentity{std::move(*library), std::move(*code)};
    }
  }
  return std::nullopt;
}

// JS `toUpperCase()` equality, ASCII-folded.
bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
  return toLowerAscii(a) == toLowerAscii(b);
}

// JS `text.split(separator)[index]`, or nullopt (undefined).
auto splitPart(std::string_view text, std::string_view separator, std::size_t index)
  -> std::optional<std::string>
{
  std::size_t start = 0;
  for (std::size_t part = 0;; ++part)
  {
    const auto found = text.find(separator, start);
    const auto end = found == std::string_view::npos ? text.size() : found;
    if (part == index)
    {
      return std::string(text.substr(start, end - start));
    }
    if (found == std::string_view::npos)
    {
      return std::nullopt;
    }
    start = found + separator.size();
  }
}

auto splitAll(std::string_view text, char separator) -> std::vector<std::string>
{
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (true)
  {
    const auto found = text.find(separator, start);
    if (found == std::string_view::npos)
    {
      parts.emplace_back(text.substr(start));
      return parts;
    }
    parts.emplace_back(text.substr(start, found - start));
    start = found + 1;
  }
}

constexpr std::string_view kValuePrefix = "Value in database: ";

// server.js getCurrentVariableType (+ getVariableObjectType's `|| 'generic'`).
// `column` is the variable's byte column on `line` (typeWriteAt).
auto variableObjectType(
  const ValidateContext& ctx,
  const std::string& variableName,
  int line,
  int column) -> std::string
{
  const auto baseName = extractBaseVariableName(variableName);
  const auto section = sectionNameForLine(line, ctx.prep->sections);
  const auto& types = ctx.prep->types;
  std::string type;
  if (!types.byName.contains(baseName))
  {
    type = commentObjectType(ctx, baseName);
  }
  else if (const auto* write = typeWriteAt(types, baseName, line, column, section))
  {
    type = write->type;
  }
  return type.empty() ? std::string("generic") : type;
}

} // namespace

auto extractGetCommandInfo(std::u16string_view prefix) -> std::optional<ObjectParameterSite>
{
  for (const auto& pattern : kSitePatterns)
  {
    for (auto at = prefix.find(pattern.command); at != std::u16string_view::npos;
         at = prefix.find(pattern.command, at + 1))
    {
      if (auto capture = matchTail(prefix, at + pattern.command.size(), pattern.shape))
      {
        const auto before = prefix.substr(0, static_cast<std::size_t>(capture->data() - prefix.data()));
        return ObjectParameterSite{
          .command = utf16To8(pattern.command),
          .variableName = utf16To8(jsTrim(*capture)),
          .column = static_cast<int>(utf16To8(before).size()),
        };
      }
    }
  }
  return std::nullopt;
}

auto objectParameterSite(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<ObjectParameterSite>
{
  if (snapshot.languageId != "vertexadt")
  {
    return std::nullopt;
  }
  const auto line = lineAt(snapshot.text, position.line);
  if (!line)
  {
    return std::nullopt;
  }
  const auto wide = utf8To16(*line);
  const auto prefix = U16View(wide).substr(0, std::min<std::size_t>(position.character, wide.size()));
  auto site = extractGetCommandInfo(prefix);
  if (site)
  {
    site->line = static_cast<int>(position.line);
  }
  return site;
}

auto extractBaseVariableName(std::string_view variableName) -> std::string
{
  const auto trimmed = utf16To8(jsTrim(utf8To16(variableName)));
  const auto isAlpha = [](char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
  };
  const auto isTail = [&](char ch) {
    return isAlpha(ch) || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
  };
  // Every branch of Node's function returns `^([A-Za-z][A-Za-z0-9_-]*\$?)`
  // when it matches, else the trimmed text.
  if (trimmed.empty() || !isAlpha(trimmed[0]))
  {
    return trimmed;
  }
  std::size_t i = 1;
  while (i < trimmed.size() && isTail(trimmed[i]))
  {
    ++i;
  }
  if (i < trimmed.size() && trimmed[i] == '$')
  {
    ++i;
  }
  return trimmed.substr(0, i);
}

auto staticObjectParameters(std::string_view command, std::string_view objectType)
  -> std::vector<ObjectParameter>
{
  for (const auto& entry : catalog::objectParameterConfigurations())
  {
    if (entry.command != command)
    {
      continue;
    }
    const catalog::ObjectParameterList* chosen = nullptr;
    for (const auto key : {objectType, std::string_view("any"), std::string_view("generic")})
    {
      for (const auto& list : entry.lists)
      {
        if (list.objectType == key)
        {
          chosen = &list;
          break;
        }
      }
      if (chosen)
      {
        break;
      }
    }
    std::vector<ObjectParameter> out;
    if (chosen)
    {
      for (const auto& row : chosen->rows)
      {
        out.push_back(toParameter(row));
      }
    }
    return out;
  }
  return {};
}

auto flattenRecordFields(
  const std::vector<MacroLibraryRecord>& records,
  std::string_view library,
  std::string_view code,
  bool joint) -> std::vector<ObjectParameter>
{
  static const std::unordered_set<std::string_view> kExclude = {
    "BROWSER_PATH", "CATEGORY", "NAME_2D", "NAME_3DACC", "NAME_3DCRS", "THUMBNAIL_ID",
    "FORM_FILE", "HNDPROG", "INPUT_STYLE", "NAME_INI", "PREPROG"};
  const auto fieldDetail = std::string(joint ? "Joint field (" : "Macro field (")
    + std::string(library) + "/" + std::string(code) + ")";

  for (const auto& record : records)
  {
    bool matched = false;
    for (const auto& cell : record)
    {
      if ((cell.name == "CODE" || (!joint && cell.name == "SHTCODE"))
        && equalsIgnoreCase(cell.value, code))
      {
        matched = true;
      }
    }
    if (!matched)
    {
      continue;
    }
    std::vector<ObjectParameter> out;
    for (const auto& cell : record)
    {
      const auto documentation = std::string(kValuePrefix) + cell.value;
      if (cell.name == "PARAMETERS")
      {
        const auto value = splitPart(documentation, kValuePrefix, 1).value_or(std::string{});
        for (const auto& part : splitAll(value, '|'))
        {
          const auto key = splitPart(part, "=", 0).value_or(std::string{});
          const auto standard = splitPart(part, "=", 1).value_or(std::string("undefined"));
          out.push_back({
            .label = key,
            .documentation = "PARAMETER RECORD, standard value: " + standard,
          });
        }
        continue;
      }
      if (kExclude.contains(cell.name))
      {
        continue;
      }
      out.push_back({
        .label = cell.name,
        .documentation = documentation,
        .detail = fieldDetail,
        .insertText = "\"" + cell.name + "\"",
      });
    }
    return out;
  }
  return {};
}

auto objectParameterCompletionItems(const ObjectParameterRequest& request)
  -> std::optional<std::vector<lsp::CompletionItem>>
{
  const auto& site = request.site;
  if (request.snapshot.languageId != "vertexadt")
  {
    return std::vector<lsp::CompletionItem>{};
  }

  ValidateContext ctx;
  ctx.settings.strictModeEnabled = request.strictModeEnabled;
  ctx.stores = ValidateStores{
    .extracts = request.extracts,
    .linkedFiles = request.linkedFiles,
    .index = request.index,
    .layout = request.layout,
    .textForPath = request.textForPath,
    .crossCall = request.crossCall,
  };
  ctx.throwIfCanceled = request.throwIfCanceled;
  ctx.prep = buildDocumentPrep(request.snapshot, ctx.settings, ctx.stores, ctx.throwIfCanceled);
  const auto objectType = variableObjectType(ctx, site.variableName, site.line, site.column);

  const auto genericParameters = staticObjectParameters(site.command, "generic");

  std::optional<std::vector<ObjectParameter>> typed;
  const bool macro = objectType == "macro" && isMacroXmlCommand(site.command);
  const bool joint = objectType == "joint" && isJointXmlCommand(site.command);
  if (macro || joint)
  {
    std::optional<ObjectIdentity> identity;
    try
    {
      identity = resolveIdentity(
        ctx.prep->lines, site.variableName, site.line, macro ? "new_macro" : "new_djoint");
    }
    catch (const RegexFailure&)
    {
      return std::vector<lsp::CompletionItem>{};
    }
    if (identity)
    {
      std::vector<MacroLibraryRecord> records;
      const auto libraryPath = request.layout
        ? resolveMacroLibraryPath(identity->library, *request.layout)
        : std::nullopt;
      if (libraryPath)
      {
        const auto exported = ensureMacroLibraryXmlExports({
          .libraryPath = *libraryPath,
          .extensionUri = request.extensionUri,
          .isCanceled = request.isCanceled,
          .log = request.log,
        });
        if (exported == MacroLibraryExportResult::Canceled)
        {
          return std::nullopt;
        }
        records = readMacroLibraryRecords(*libraryPath, identity->library);
      }
      typed = flattenRecordFields(records, identity->library, identity->code, joint);
    }
  }
  if (!typed)
  {
    typed = staticObjectParameters(site.command, objectType);
  }
  const auto& finalParameters = typed->empty() ? genericParameters : *typed;

  std::vector<lsp::CompletionItem> items;
  items.reserve(finalParameters.size());
  for (const auto& param : finalParameters)
  {
    lsp::CompletionItem item;
    item.label = param.label;
    item.kind = lsp::CompletionItemKind::Property;
    item.insertText = param.insertText && !param.insertText->empty()
      ? *param.insertText
      : "\"" + param.label + "\"";
    item.documentation = lsp::MarkupContent{
      .kind = lsp::MarkupKind::Markdown,
      .value = param.documentation.value_or(std::string{}),
    };
    item.detail = site.command + " - " + param.detail.value_or(std::string("undefined"));
    item.sortText = "0" + param.label;
    items.push_back(std::move(item));
  }
  return items;
}

} // namespace vertexadt
