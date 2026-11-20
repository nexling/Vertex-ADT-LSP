#include "cheap_completion.h"

#include "catalog/catalog.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

lsp::json::Value catalogData(std::size_t index)
{
  return lsp::json::Value(static_cast<lsp::json::Integer>(index + 1));
}

std::optional<std::size_t> catalogIndexFromData(const lsp::Opt<lsp::LSPAny>& data)
{
  if (!data || !data->isNumber())
  {
    return std::nullopt;
  }
  const auto value = static_cast<int>(data->number());
  if (value <= 0)
  {
    return std::nullopt;
  }
  return static_cast<std::size_t>(value - 1);
}

std::string keywordResolveMarkdown(const catalog::KeywordCatalogEntry& entry)
{
  const auto docs = entry.documentation ? entry.documentation : "";
  return std::string(docs) + "\n\n---\n" + std::string(entry.detail);
}

lsp::CompletionItem keywordItem(const catalog::KeywordCatalogEntry& entry, std::size_t index)
{
  lsp::CompletionItem item;
  item.label = std::string(entry.label);
  item.kind = lsp::CompletionItemKind::Keyword;
  item.data = catalogData(index);
  if (entry.documentation)
  {
    item.documentation = std::string(entry.documentation);
  }
  if (entry.insertText && std::string_view(entry.insertText).find('\n') != std::string_view::npos)
  {
    item.insertText = std::string(entry.insertText);
    item.insertTextFormat = lsp::InsertTextFormat::PlainText;
  }
  return item;
}

lsp::CompletionItem snippetDuplicate(const catalog::KeywordCatalogEntry& entry, std::size_t index)
{
  lsp::CompletionItem item;
  item.label = std::string(entry.label) + " (snippet)";
  item.kind = lsp::CompletionItemKind::Snippet;
  item.data = catalogData(index);
  if (entry.documentation)
  {
    item.documentation = std::string(entry.documentation);
  }
  item.insertText = std::string(entry.insertText);
  item.insertTextFormat = lsp::InsertTextFormat::Snippet;
  return item;
}

lsp::CompletionItem structureSnippet(
  std::string label,
  std::string insertText,
  std::string detail,
  std::string documentation)
{
  lsp::CompletionItem item;
  item.label = std::move(label);
  item.kind = lsp::CompletionItemKind::Snippet;
  item.detail = std::move(detail);
  item.documentation = lsp::MarkupContent{
    .kind = lsp::MarkupKind::Markdown,
    .value = std::move(documentation),
  };
  item.insertText = std::move(insertText);
  item.insertTextFormat = lsp::InsertTextFormat::Snippet;
  return item;
}

std::string extractDocumentation(std::string_view heading, std::string_view name, std::string_view comment)
{
  std::string out;
  out += heading;
  out += ":\n```vertexadt\n";
  out += name;
  out += "\n```\n\n";
  out += comment;
  return out;
}

void appendUniqueExtractItems(
  std::vector<lsp::CompletionItem>& items,
  const std::vector<HoverTuple>& live,
  const std::vector<HoverTuple>* included,
  lsp::CompletionItemKind kind,
  std::string_view heading)
{
  std::unordered_set<std::string> seen;
  auto append = [&](const std::vector<HoverTuple>& rows) {
    for (const auto& row : rows)
    {
      if (row.name.empty() || catalog::isKeywordLabel(row.name) || !seen.insert(row.name).second)
      {
        continue;
      }
      lsp::CompletionItem item;
      item.label = row.name;
      item.kind = kind;
      if (row.comment && !row.comment->empty())
      {
        item.documentation = lsp::MarkupContent{
          .kind = lsp::MarkupKind::Markdown,
          .value = extractDocumentation(heading, row.name, *row.comment),
        };
      }
      items.push_back(std::move(item));
    }
  };
  append(live);
  if (included)
  {
    append(*included);
  }
}

const VariableExtract* liveExtract(
  const Snapshot& snapshot,
  const ExtractStore& extracts,
  VariableExtract& fallback)
{
  if (auto installed = extracts.live(snapshot.uri, snapshot.version))
  {
    return installed.get();
  }
  fallback = fillLiveBufferExtract(snapshot.text);
  return &fallback;
}

} // namespace

auto cheapCompletion(const CheapCompletionContext& context) -> lsp::TextDocumentCompletionResult
{
  if (context.snapshot.languageId != "vertexadt")
  {
    return nullptr;
  }

  std::vector<lsp::CompletionItem> items;
  const auto keywords = catalog::keywordCatalog();
  items.reserve(keywords.size() + 8);
  for (std::size_t index = 0; index < keywords.size(); ++index)
  {
    const auto& entry = keywords[index];
    items.push_back(keywordItem(entry, index));
    if (context.useExpandedSyntax && entry.insertText)
    {
      items.push_back(snippetDuplicate(entry, index));
    }
  }

  if (context.useExpandedSyntax)
  {
    items.push_back(structureSnippet(
      "if-structure",
      "if ${1} then\n\t${2}\nendif",
      "if condition then ... endif",
      "Creates an if-then-endif block structure"));
    items.push_back(structureSnippet(
      "loop-structure",
      "loop ${1} \n\t${2}\nendloop",
      "loop condition ... endloop",
      "Creates a loop-endloop block structure"));
  }

  VariableExtract fallback;
  const auto* live = liveExtract(context.snapshot, context.extracts, fallback);
  const auto neighborhood = context.extracts.neighborhood(context.snapshot.uri);
  const auto* included = neighborhood.get();
  appendUniqueExtractItems(
    items,
    live->hoverVariables,
    included ? &included->hoverVariables : nullptr,
    lsp::CompletionItemKind::Variable,
    "Variable");
  appendUniqueExtractItems(
    items,
    live->hoverStringVariables,
    included ? &included->hoverStringVariables : nullptr,
    lsp::CompletionItemKind::Text,
    "String Variable");

  return lsp::CompletionList{
    .isIncomplete = false,
    .items = std::move(items),
  };
}

auto resolveKeywordCompletion(lsp::CompletionItem item) -> lsp::CompletionItem
{
  const auto index = catalogIndexFromData(item.data);
  const auto keywords = catalog::keywordCatalog();
  if (!index || *index >= keywords.size())
  {
    return item;
  }
  item.documentation = lsp::MarkupContent{
    .kind = lsp::MarkupKind::Markdown,
    .value = keywordResolveMarkdown(keywords[*index]),
  };
  return item;
}

} // namespace vertexadt
