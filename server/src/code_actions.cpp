#include "code_actions.h"

#include "call_param_sites.h"
#include "concat_whitespace.h"
#include "local_slot_rename.h"
#include "substring_index.h"
#include "text_util.h"
#include "utf16_text.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

constexpr std::string_view kQuickFix = "quickfix";
constexpr std::string_view kSourceFixAll = "source.fixAll";
constexpr std::string_view kUnclosedResourceCode = "vertexadt/unclosed-resource";
constexpr std::string_view kCloseAlreadyOpenCode = "vertexadt/close-already-open";
constexpr std::string_view kMissingCallParamsCode = "vertexadt/missing-call-params";
constexpr std::string_view kCalleeLocalNamingCode = "vertexadt/callee-local-naming";
constexpr std::string_view kIgnoreCloseTitle = "Ignore missing close (;!IGNORE CLOSE)";

auto field(const lsp::json::Value& value, std::string_view key) -> const lsp::json::Value*
{
  return value.isObject() ? value.object().find(key) : nullptr;
}

// JS `typeof x === 'number'`, narrowed to the integral line numbers a
// position can carry.
auto lineNumber(const lsp::json::Value* value) -> std::optional<std::int64_t>
{
  if (!value || !value->isNumber())
  {
    return std::nullopt;
  }
  const auto number = value->number();
  if (!std::isfinite(number))
  {
    return std::nullopt;
  }
  return static_cast<std::int64_t>(std::trunc(number));
}

bool wantsKind(const lsp::json::Value* only, std::string_view kind)
{
  if (!only || !only->isArray() || only->array().empty())
  {
    return true;
  }
  for (const auto& entry : only->array())
  {
    if (entry.isString() && entry.string() == kind)
    {
      return true;
    }
  }
  return false;
}

bool hasCode(const lsp::json::Value& diagnostic, std::string_view code)
{
  const auto* value = field(diagnostic, "code");
  return value && value->isString() && value->string() == code;
}

struct IgnoreCloseInsertion
{
  std::int64_t character = 0;
  std::string_view newText;
};

// Node unclosedResources `buildIgnoreCloseInsertion`. `lineText` is already
// split on `\r?\n`, so the trailing-`\r` strip is only for lone `\r` ends.
auto buildIgnoreCloseInsertion(std::string_view lineText) -> std::optional<IgnoreCloseInsertion>
{
  if (lineHasIgnoreClose(lineText))
  {
    return std::nullopt;
  }
  auto withoutCr = lineText;
  if (!withoutCr.empty() && withoutCr.back() == '\r')
  {
    withoutCr.remove_suffix(1);
  }
  const auto character = static_cast<std::int64_t>(utf8To16(withoutCr).size());
  if (findBangCommentIndex(withoutCr) >= 0)
  {
    return IgnoreCloseInsertion{character, " IGNORE CLOSE"};
  }
  return IgnoreCloseInsertion{character, " ;!IGNORE CLOSE"};
}

auto position(std::int64_t line, std::int64_t character) -> lsp::json::Value
{
  const auto number = [](std::int64_t n)
  {
    return n >= INT32_MIN && n <= INT32_MAX
      ? lsp::json::Value(static_cast<std::int32_t>(n))
      : lsp::json::Value(static_cast<double>(n));
  };
  lsp::json::Object object;
  object["line"] = number(line);
  object["character"] = number(character);
  return lsp::json::Value(std::move(object));
}

auto zeroWidthInsert(std::int64_t line, std::int64_t character, std::string newText)
  -> lsp::json::Value
{
  lsp::json::Object range;
  range["start"] = position(line, character);
  range["end"] = position(line, character);
  lsp::json::Object edit;
  edit["range"] = lsp::json::Value(std::move(range));
  edit["newText"] = lsp::json::Value(std::move(newText));
  return lsp::json::Value(std::move(edit));
}

auto singleUriEdit(const lsp::json::Value& uri, lsp::json::Array edits) -> lsp::json::Value
{
  lsp::json::Object changes;
  changes[uri.string()] = lsp::json::Value(std::move(edits));
  lsp::json::Object workspaceEdit;
  workspaceEdit["changes"] = lsp::json::Value(std::move(changes));
  return lsp::json::Value(std::move(workspaceEdit));
}

struct MissingInsert
{
  const lsp::json::Value* diagnostic = nullptr;
  std::int64_t line = 0;
  std::int64_t character = 0;
  std::int64_t missingCount = 0;
};

// `data.missingCount > 0` and numeric `data.line` / `data.insertOffset`, else
// skipped (a client that does not echo `data` gets no call-param fixes).
auto missingInserts(const lsp::json::Array& diagnostics) -> std::vector<MissingInsert>
{
  std::vector<MissingInsert> inserts;
  for (const auto& diagnostic : diagnostics)
  {
    if (!hasCode(diagnostic, kMissingCallParamsCode))
    {
      continue;
    }
    const auto* data = field(diagnostic, "data");
    const auto missingCount = data ? lineNumber(field(*data, "missingCount")) : std::nullopt;
    const auto line = data ? lineNumber(field(*data, "line")) : std::nullopt;
    const auto character = data ? lineNumber(field(*data, "insertOffset")) : std::nullopt;
    if (!missingCount || *missingCount <= 0 || !line || !character)
    {
      continue;
    }
    inserts.push_back(MissingInsert{&diagnostic, *line, *character, *missingCount});
  }
  return inserts;
}

// Node onCodeAction call-param branch: per-site QuickFix plus one SourceFixAll
// over the request's missing diagnostics. Extra diagnostics get nothing.
// SourceFixAll applies the same zero-width inserts at `insertOffset` (Node's
// `character` vs `startCharacter` mismatch is not copied).
void appendCallParamActions(
  const lsp::json::Value& uri,
  const lsp::json::Array& diagnostics,
  bool wantsQuickFix,
  bool wantsFixAll,
  lsp::json::Array& actions)
{
  auto inserts = missingInserts(diagnostics);
  if (wantsQuickFix)
  {
    for (const auto& insert : inserts)
    {
      lsp::json::Object action;
      action["title"] = lsp::json::Value(
        "Add " + std::to_string(insert.missingCount) + " missing call parameter(s) as \"\"");
      action["kind"] = lsp::json::Value(kQuickFix);
      action["diagnostics"] = lsp::json::Value(lsp::json::Array{*insert.diagnostic});
      action["edit"] = singleUriEdit(
        uri,
        lsp::json::Array{zeroWidthInsert(
          insert.line,
          insert.character,
          missingCallParamInsertText(static_cast<int>(insert.missingCount)))});
      actions.push_back(lsp::json::Value(std::move(action)));
    }
  }

  if (wantsFixAll && !inserts.empty())
  {
    std::stable_sort(inserts.begin(), inserts.end(), [](const MissingInsert& a, const MissingInsert& b) {
      if (a.line != b.line)
      {
        return a.line > b.line;
      }
      return a.character > b.character;
    });
    lsp::json::Array edits;
    for (const auto& insert : inserts)
    {
      edits.push_back(zeroWidthInsert(
        insert.line,
        insert.character,
        missingCallParamInsertText(static_cast<int>(insert.missingCount))));
    }
    lsp::json::Object action;
    action["title"] = lsp::json::Value(
      "Add missing call parameters as \"\" in file (" + std::to_string(inserts.size()) + ")");
    action["kind"] = lsp::json::Value(kSourceFixAll);
    action["edit"] = singleUriEdit(uri, std::move(edits));
    actions.push_back(lsp::json::Value(std::move(action)));
  }
}

void appendIgnoreCloseActions(
  const Snapshot& snapshot,
  const lsp::json::Value& uri,
  const lsp::json::Array& diagnostics,
  lsp::json::Array& actions)
{
  for (const auto& diagnostic : diagnostics)
  {
    if (!hasCode(diagnostic, kUnclosedResourceCode)
      && !hasCode(diagnostic, kCloseAlreadyOpenCode))
    {
      continue;
    }
    // `data.line` when it is a number, else `range.start.line`.
    const auto* data = field(diagnostic, "data");
    auto lineIndex = data ? lineNumber(field(*data, "line")) : std::nullopt;
    if (!lineIndex)
    {
      const auto* range = field(diagnostic, "range");
      const auto* start = range ? field(*range, "start") : nullptr;
      lineIndex = start ? lineNumber(field(*start, "line")) : std::nullopt;
    }
    if (!lineIndex)
    {
      continue;
    }
    std::string_view lineText;
    if (*lineIndex >= 0)
    {
      if (const auto text = lineAt(snapshot.text, static_cast<std::size_t>(*lineIndex)))
      {
        lineText = *text;
      }
    }
    const auto insertion = buildIgnoreCloseInsertion(lineText);
    if (!insertion)
    {
      continue;
    }

    lsp::json::Object range;
    range["start"] = position(*lineIndex, insertion->character);
    range["end"] = position(*lineIndex, insertion->character);
    lsp::json::Object edit;
    edit["range"] = lsp::json::Value(std::move(range));
    edit["newText"] = lsp::json::Value(insertion->newText);
    lsp::json::Object changes;
    changes[uri.string()] = lsp::json::Value(lsp::json::Array{lsp::json::Value(std::move(edit))});
    lsp::json::Object workspaceEdit;
    workspaceEdit["changes"] = lsp::json::Value(std::move(changes));

    lsp::json::Object action;
    action["title"] = lsp::json::Value(kIgnoreCloseTitle);
    action["kind"] = lsp::json::Value(kQuickFix);
    action["diagnostics"] = lsp::json::Value(lsp::json::Array{diagnostic});
    action["edit"] = lsp::json::Value(std::move(workspaceEdit));
    actions.push_back(lsp::json::Value(std::move(action)));
  }
}

// JS truthy string field (`!data.varName` skips missing and '').
auto nonEmptyString(const lsp::json::Value* data, std::string_view key) -> std::optional<std::string>
{
  const auto* value = data ? field(*data, key) : nullptr;
  if (!value || !value->isString() || value->string().empty())
  {
    return std::nullopt;
  }
  return value->string();
}

// Node onCodeAction rename branch: one QuickFix per callee-local-naming
// diagnostic with `data.varName` / `data.newName`, file-wide over this buffer.
// No SourceFixAll for this family; skipped when the rename finds no reference.
void appendLocalSlotRenameActions(
  const Snapshot& snapshot,
  const lsp::json::Value& uri,
  const lsp::json::Array& diagnostics,
  lsp::json::Array& actions)
{
  std::optional<std::vector<std::string_view>> lines;
  for (const auto& diagnostic : diagnostics)
  {
    if (!hasCode(diagnostic, kCalleeLocalNamingCode))
    {
      continue;
    }
    const auto* data = field(diagnostic, "data");
    const auto varName = nonEmptyString(data, "varName");
    const auto newName = nonEmptyString(data, "newName");
    if (!varName || !newName)
    {
      continue;
    }
    if (!lines)
    {
      lines = splitLines(snapshot.text);
    }
    const auto edits = fileWideLocalSlotRenameEdits(
      *lines, {LocalSlotRenamePair{.oldName = *varName, .newName = *newName}});
    if (edits.empty())
    {
      continue;
    }
    lsp::json::Array textEdits;
    for (const auto& edit : edits)
    {
      lsp::json::Object range;
      range["start"] = position(edit.line, edit.startCharacter);
      range["end"] = position(edit.line, edit.endCharacter);
      lsp::json::Object textEdit;
      textEdit["range"] = lsp::json::Value(std::move(range));
      textEdit["newText"] = lsp::json::Value(edit.newText);
      textEdits.push_back(lsp::json::Value(std::move(textEdit)));
    }
    lsp::json::Object action;
    action["title"] = lsp::json::Value("Rename " + *varName + " to " + *newName + " in file");
    action["kind"] = lsp::json::Value(kQuickFix);
    action["diagnostics"] = lsp::json::Value(lsp::json::Array{diagnostic});
    action["edit"] = singleUriEdit(uri, std::move(textEdits));
    actions.push_back(lsp::json::Value(std::move(action)));
  }
}

auto rangeEdit(std::int64_t line, std::int64_t start, std::int64_t end, std::string newText)
  -> lsp::json::Value
{
  lsp::json::Object range;
  range["start"] = position(line, start);
  range["end"] = position(line, end);
  lsp::json::Object edit;
  edit["range"] = lsp::json::Value(std::move(range));
  edit["newText"] = lsp::json::Value(std::move(newText));
  return lsp::json::Value(std::move(edit));
}

bool explicitlyWants(const lsp::json::Value* only, std::string_view kind)
{
  return only && only->isArray() && !only->array().empty() && wantsKind(only, kind);
}

// One QuickFix per concat-whitespace diagnostic (replace its range with `&`),
// plus a SourceFixAll that re-walks the buffer. The fix-all is offered next to
// a concat-whitespace diagnostic or when `only` asks for source.fixAll.
void appendConcatWhitespaceActions(
  const Snapshot& snapshot,
  const lsp::json::Value& uri,
  const lsp::json::Array& diagnostics,
  const lsp::json::Value* only,
  bool wantsQuickFix,
  bool wantsFixAll,
  lsp::json::Array& actions)
{
  bool sawDiagnostic = false;
  for (const auto& diagnostic : diagnostics)
  {
    if (!hasCode(diagnostic, kConcatWhitespaceCode))
    {
      continue;
    }
    sawDiagnostic = true;
    if (!wantsQuickFix)
    {
      continue;
    }
    const auto* range = field(diagnostic, "range");
    const auto* start = range ? field(*range, "start") : nullptr;
    const auto* end = range ? field(*range, "end") : nullptr;
    const auto line = start ? lineNumber(field(*start, "line")) : std::nullopt;
    const auto from = start ? lineNumber(field(*start, "character")) : std::nullopt;
    const auto to = end ? lineNumber(field(*end, "character")) : std::nullopt;
    if (!line || !from || !to)
    {
      continue;
    }
    lsp::json::Object action;
    action["title"] = lsp::json::Value("Remove spaces around '&'");
    action["kind"] = lsp::json::Value(kQuickFix);
    action["diagnostics"] = lsp::json::Value(lsp::json::Array{diagnostic});
    action["edit"] = singleUriEdit(uri, lsp::json::Array{rangeEdit(*line, *from, *to, "&")});
    action["isPreferred"] = lsp::json::Value(true);
    actions.push_back(lsp::json::Value(std::move(action)));
  }

  if (!wantsFixAll || (!sawDiagnostic && !explicitlyWants(only, kSourceFixAll))
    || isValidationExcluded(snapshot.text))
  {
    return;
  }
  const auto lines = splitLines(snapshot.text);
  const auto issues = findConcatWhitespaceIssues(lines, parseFileSections(lines));
  if (issues.empty())
  {
    return;
  }
  lsp::json::Array edits;
  for (auto it = issues.rbegin(); it != issues.rend(); ++it)
  {
    edits.push_back(rangeEdit(
      it->line,
      static_cast<std::int64_t>(it->startCharacter),
      static_cast<std::int64_t>(it->endCharacter),
      "&"));
  }
  lsp::json::Object action;
  action["title"] = lsp::json::Value(
    "Remove spaces around '&' in file (" + std::to_string(issues.size()) + ")");
  action["kind"] = lsp::json::Value(kSourceFixAll);
  action["edit"] = singleUriEdit(uri, std::move(edits));
  actions.push_back(lsp::json::Value(std::move(action)));
}

// One preferred QuickFix per substring-start-index diagnostic: replace the
// literal start index (the diagnostic's range) with `1`. Native only.
void appendSubstringStartActions(
  const lsp::json::Value& uri,
  const lsp::json::Array& diagnostics,
  lsp::json::Array& actions)
{
  for (const auto& diagnostic : diagnostics)
  {
    if (!hasCode(diagnostic, kSubstringStartIndexCode))
    {
      continue;
    }
    const auto* range = field(diagnostic, "range");
    const auto* start = range ? field(*range, "start") : nullptr;
    const auto* end = range ? field(*range, "end") : nullptr;
    const auto line = start ? lineNumber(field(*start, "line")) : std::nullopt;
    const auto from = start ? lineNumber(field(*start, "character")) : std::nullopt;
    const auto to = end ? lineNumber(field(*end, "character")) : std::nullopt;
    if (!line || !from || !to)
    {
      continue;
    }
    lsp::json::Object action;
    action["title"] = lsp::json::Value(kSubstringStartFixTitle);
    action["kind"] = lsp::json::Value(kQuickFix);
    action["diagnostics"] = lsp::json::Value(lsp::json::Array{diagnostic});
    action["edit"] = singleUriEdit(uri, lsp::json::Array{rangeEdit(*line, *from, *to, "1")});
    action["isPreferred"] = lsp::json::Value(true);
    actions.push_back(lsp::json::Value(std::move(action)));
  }
}

} // namespace

auto codeActionOptions() -> lsp::CodeActionOptions
{
  return {
    .codeActionKinds = lsp::Array<lsp::CodeActionKindEnum>{
      lsp::CodeActionKind::QuickFix,
      lsp::CodeActionKind::SourceFixAll,
      lsp::CodeActionKind::Source,
      lsp::CodeActionKind::RefactorMove,
    },
    .resolveProvider = false,
  };
}

auto codeActions(
  const std::optional<Snapshot>& snapshot,
  const lsp::json::Value& params,
  bool callParamValidationEnabled,
  const ParentUiContext* parents) -> lsp::json::Value
{
  lsp::json::Array actions;
  const auto* textDocument = field(params, "textDocument");
  const auto* uri = textDocument ? field(*textDocument, "uri") : nullptr;
  if (!snapshot || snapshot->languageId != "vertexadt" || !uri || !uri->isString())
  {
    return lsp::json::Value(std::move(actions));
  }

  const auto* context = field(params, "context");
  const auto* diagnosticsValue = context ? field(*context, "diagnostics") : nullptr;
  static const lsp::json::Array kNoDiagnostics;
  const auto& diagnostics = diagnosticsValue && diagnosticsValue->isArray()
    ? diagnosticsValue->array()
    : kNoDiagnostics;
  const auto* only = context ? field(*context, "only") : nullptr;
  const auto wantsQuickFix = wantsKind(only, kQuickFix);
  const auto wantsFixAll = wantsKind(only, kSourceFixAll);

  if (callParamValidationEnabled)
  {
    appendCallParamActions(*uri, diagnostics, wantsQuickFix, wantsFixAll, actions);
  }
  if (wantsQuickFix)
  {
    appendIgnoreCloseActions(*snapshot, *uri, diagnostics, actions);
    appendLocalSlotRenameActions(*snapshot, *uri, diagnostics, actions);
  }
  appendConcatWhitespaceActions(
    *snapshot, *uri, diagnostics, only, wantsQuickFix, wantsFixAll, actions);
  if (wantsQuickFix)
  {
    appendSubstringStartActions(*uri, diagnostics, actions);
  }
  // Pin inferred `##included_from` parents: `source` actions, only when the
  // request asks for source kinds (Source Action menu), not every lightbulb.
  if (parents && only && only->isArray() && !only->array().empty() && wantsKind(only, "source"))
  {
    for (auto& action : pinIncludedFromActions(*parents, *snapshot, *uri))
    {
      actions.push_back(std::move(action));
    }
  }
  // Program move (docs/adr/0017-native-program-move.md): the host command
  // asks for the target and applies the edit. Offered on an explicit refactor
  // request or an invoked (Ctrl+.) one, never on the automatic lightbulb.
  const auto* triggerKind = context ? field(*context, "triggerKind") : nullptr;
  const bool invoked = triggerKind && triggerKind->isNumber() && triggerKind->number() == 1;
  const bool asksRefactor = only && only->isArray() && !only->array().empty()
    && (wantsKind(only, "refactor") || wantsKind(only, "refactor.move"));
  const bool noFilter = !only || !only->isArray() || only->array().empty();
  if (uri->string().starts_with("file:") && (asksRefactor || (noFilter && invoked)))
  {
    lsp::json::Object argument;
    argument["uri"] = *uri;
    lsp::json::Object command;
    command["title"] = lsp::json::Value(std::string("Move callable program…"));
    command["command"] = lsp::json::Value(std::string("vertexadt.moveProgram"));
    command["arguments"] = lsp::json::Value(lsp::json::Array{lsp::json::Value(std::move(argument))});
    lsp::json::Object action;
    action["title"] = lsp::json::Value(std::string("Move callable program…"));
    action["kind"] = lsp::json::Value(std::string("refactor.move"));
    action["command"] = lsp::json::Value(std::move(command));
    actions.push_back(lsp::json::Value(std::move(action)));
  }
  return lsp::json::Value(std::move(actions));
}

} // namespace vertexadt
