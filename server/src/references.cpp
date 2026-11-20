#include "references.h"

#include "parent_context.h"

#include "text_util.h"

#include <lsp/serialization.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

using navigation::icaseAt;
using navigation::isJsBoundary;
using navigation::splitJsLines;
using navigation::utf16Offset;

struct SearchFile
{
  lsp::Uri uri;
  std::string identity; // normalized fs path
  std::string text;
};

struct Reference
{
  std::size_t file = 0;
  int line = 0;
  int start = 0;
  int end = 0;
};

// JS \s minus line terminators (the line is already split).
bool isInlineSpace(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r';
}

// `##included_from` remainders on every line, in source order (Node
// collectAllIncludedFiles = collectIncludedFilesBeforeLine(lines, len - 1)).
auto allIncludedFrom(const std::vector<std::string_view>& lines) -> std::vector<std::string>
{
  constexpr std::string_view kMarker = "##included_from";
  std::vector<std::string> includes;
  for (const auto raw : lines)
  {
    const auto trimmed = trim(raw);
    if (trimmed.substr(0, kMarker.size()) != kMarker)
    {
      continue;
    }
    // /##included_from\s+(.+?)$/i on the trimmed line.
    auto q = kMarker.size();
    if (q >= trimmed.size() || !isInlineSpace(trimmed[q]))
    {
      continue;
    }
    while (q < trimmed.size() && isInlineSpace(trimmed[q]))
    {
      ++q;
    }
    if (q >= trimmed.size())
    {
      continue;
    }
    includes.emplace_back(trim(trimmed.substr(q)));
  }
  return includes;
}

// Node buildVariableReferenceRegex: /\bNAME\b(?!\$)/gi, or /\bNAME\b/gi when
// NAME ends with `$`.
bool referenceMatchAt(std::string_view line, std::size_t at, std::string_view name)
{
  if (!isJsBoundary(line, at) || !icaseAt(line, at, name))
  {
    return false;
  }
  const auto end = at + name.size();
  if (!isJsBoundary(line, end))
  {
    return false;
  }
  return name.back() == '$' || end >= line.size() || line[end] != '$';
}

// Node findVariableReferencesInLines: skip trim-`!` lines, strip quote-aware
// `;!`, non-overlapping left-to-right matches, skip quoted match starts.
void scanFile(
  std::string_view text,
  std::string_view name,
  std::size_t fileIndex,
  std::vector<Reference>& out)
{
  const auto lines = splitJsLines(text);
  for (std::size_t i = 0; i < lines.size(); ++i)
  {
    const auto raw = lines[i];
    if (const auto trimmed = trim(raw); !trimmed.empty() && trimmed.front() == '!')
    {
      continue;
    }
    const auto code = stripValidationComment(raw);
    const std::string_view line = code;
    std::size_t at = 0;
    while (at + name.size() <= line.size())
    {
      if (!referenceMatchAt(line, at, name))
      {
        ++at;
        continue;
      }
      if (!isInsideQuotes(line, at))
      {
        out.push_back(Reference{
          .file = fileIndex,
          .line = static_cast<int>(i),
          .start = utf16Offset(line, at),
          .end = utf16Offset(line, at + name.size()),
        });
      }
      at += name.size();
    }
  }
}

} // namespace

auto findReferences(const ReferencesContext& context) -> lsp::TextDocumentReferencesResult
{
  lsp::Array<lsp::Location> result;
  const auto& snapshot = context.snapshot;
  if (snapshot.languageId != "vertexadt")
  {
    return result;
  }
  const auto lines = splitJsLines(snapshot.text);
  const auto cursorLine = static_cast<int>(context.position.line);
  if (cursorLine < 0 || cursorLine >= static_cast<int>(lines.size()))
  {
    return result;
  }
  const auto word = definitionWordAtPosition(
    lines[static_cast<std::size_t>(cursorLine)],
    static_cast<int>(context.position.character));
  if (!word || !isValidDefinitionTarget(*word))
  {
    return result;
  }

  // Node's Map insertion order: this buffer, the definition file, then every
  // `##included_from`. Keyed by normalized fs path so a file is scanned once.
  std::vector<SearchFile> files;
  auto indexOf = [&files](std::string_view identity) -> std::optional<std::size_t>
  {
    for (std::size_t i = 0; i < files.size(); ++i)
    {
      if (files[i].identity == identity)
      {
        return i;
      }
    }
    return std::nullopt;
  };
  files.push_back(SearchFile{
    .uri = snapshot.uri,
    .identity = normalizeFsPath(snapshot.uri.fsPath()),
    .text = snapshot.text,
  });

  auto definition = locateDefinition(
    snapshot, *word, cursorLine, context.layout, context.textForPath, context.index);
  const auto inferredSites = definition
    ? std::vector<DefinitionSite>{}
    : inferredDefinitionSites(
        snapshot, *word, cursorLine, context.layout, context.textForPath, context.index,
        context.inferParents);
  if (!definition && inferredSites.size() == 1)
  {
    definition = inferredSites.front();
  }
  std::optional<std::size_t> definitionFile;
  if (definition && !definition->path)
  {
    definitionFile = 0;
  }
  else if (definition)
  {
    const auto identity = normalizeFsPath(definition->path->string());
    definitionFile = indexOf(identity);
    if (!definitionFile && context.textForPath)
    {
      if (auto text = context.textForPath(*definition->path))
      {
        files.push_back(SearchFile{
          .uri = definition->uri, .identity = identity, .text = std::move(*text)});
        definitionFile = files.size() - 1;
      }
    }
  }

  for (const auto& include : allIncludedFrom(lines))
  {
    const auto path = context.layout.findIncludedFile(include);
    if (!path || !context.textForPath)
    {
      continue;
    }
    auto identity = normalizeFsPath(path->string());
    if (indexOf(identity))
    {
      continue;
    }
    if (auto text = context.textForPath(*path))
    {
      files.push_back(SearchFile{
        .uri = uriFromPath(*path), .identity = std::move(identity), .text = std::move(*text)});
    }
  }

  // Inferred parents (docs/adr/0015-native-inferred-included-from.md).
  if (context.index && context.inferParents && context.textForPath)
  {
    const auto links = resolveParentLinks(
      snapshot.text, pathFromUri(snapshot.uri), context.layout, context.index, true);
    for (const auto& parent : links.inferred)
    {
      auto identity = normalizeFsPath(parent.absPath.string());
      if (indexOf(identity))
      {
        continue;
      }
      if (auto text = context.textForPath(parent.absPath))
      {
        files.push_back(SearchFile{
          .uri = uriFromPath(parent.absPath), .identity = std::move(identity), .text = std::move(*text)});
      }
    }
  }

  std::vector<Reference> references;
  for (std::size_t i = 0; i < files.size(); ++i)
  {
    scanFile(files[i].text, *word, i, references);
  }

  // includeDeclaration false drops the one locator span (URI + line + start +
  // start + name length). No locator → no filter.
  const auto nameLength = utf16Offset(*word, word->size());
  for (const auto& reference : references)
  {
    if (!context.includeDeclaration && definition && definitionFile
        && reference.file == *definitionFile && reference.line == definition->line
        && reference.start == definition->character
        && reference.end == definition->character + nameLength)
    {
      continue;
    }
    const auto line = static_cast<lsp::Uint>(reference.line);
    result.push_back(lsp::Location{
      .uri = files[reference.file].uri,
      .range = {
        .start = {.line = line, .character = static_cast<lsp::Uint>(reference.start)},
        .end = {.line = line, .character = static_cast<lsp::Uint>(reference.end)},
      },
    });
  }
  return result;
}

auto parseReferenceParams(lsp::json::Value&& json) -> lsp::ReferenceParams
{
  if (json.isObject())
  {
    auto& object = json.object();
    if (!object.contains("context"))
    {
      lsp::json::Object context;
      context["includeDeclaration"] = true;
      object["context"] = std::move(context);
    }
  }
  lsp::ReferenceParams params;
  lsp::fromJson(std::move(json), params);
  return params;
}

} // namespace vertexadt
