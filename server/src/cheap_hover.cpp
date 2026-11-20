#include "cheap_hover.h"

#include "control_block_pairing.h"
#include "piece_setup_sets.h"
#include "utf16_text.h"

#include "catalog/catalog.h"
#include "debug_dump.h"
#include "dir_paths.h"
#include "library_paths.h"
#include "text_util.h"

#include <array>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 7> kHoverImages{{
  {"pnt_inside_poly", "pnt_inside_poly explainer.png"},
  {"is_point_inside", "is_point_inside explainer.png"},
  {"vec_intersect", "vec_intersect explainer.png"},
  {"vxgeom_cross_pnts", "vxgeom_cross_pnts explainer.png"},
  {"is_in_vxgeom", "is_in_vxgeom explainer.png"},
  {"point_to_vxgeom", "point_to_vxgeom explainer.png"},
  {"pnt_to_plane", "pnt_to_plane_explainer.png"},
}};

bool isJsWordChar(char16_t ch)
{
  return (ch >= 'A' && ch <= 'Z')
    || (ch >= 'a' && ch <= 'z')
    || (ch >= '0' && ch <= '9')
    || ch == '_';
}

bool isAsciiWordChar(unsigned char ch)
{
  return std::isalnum(ch) != 0 || ch == '_';
}

bool commentTruthy(const std::optional<std::string>& comment)
{
  return comment && !comment->empty();
}

const HoverTuple* firstTruthy(
  const std::vector<HoverTuple>& live,
  const std::vector<HoverTuple>* included,
  std::string_view word)
{
  auto find = [&](const std::vector<HoverTuple>& rows) -> const HoverTuple* {
    for (const auto& row : rows)
    {
      if (row.name == word && commentTruthy(row.comment))
      {
        return &row;
      }
    }
    return nullptr;
  };
  if (const auto* hit = find(live))
  {
    return hit;
  }
  if (included)
  {
    return find(*included);
  }
  return nullptr;
}

std::string keywordMarkdown(const catalog::KeywordCatalogEntry& entry)
{
  const auto docs = entry.documentation ? entry.documentation : "";
  return std::string(docs) + "\n\n---\n" + std::string(entry.detail);
}

std::string variableMarkdown(std::string_view heading, std::string_view name, std::string_view comment)
{
  std::string out;
  out += heading;
  out += ":\n```vertexadt \n";
  out += name;
  out += "\n```\n\n";
  out += comment;
  return out;
}

std::string stripHoverMarkers(std::string text)
{
  std::string withoutBrackets;
  withoutBrackets.reserve(text.size());
  for (std::size_t i = 0; i < text.size();)
  {
    if (text[i] == '[')
    {
      const auto close = text.find(']', i + 1);
      if (close != std::string::npos && text.find('\n', i + 1) > close)
      {
        i = close + 1;
        continue;
      }
    }
    withoutBrackets.push_back(text[i]);
    ++i;
  }

  std::string withoutIgnore;
  withoutIgnore.reserve(withoutBrackets.size());
  constexpr std::string_view kNeedle = "IGNORE TYPE";
  for (std::size_t i = 0; i < withoutBrackets.size();)
  {
    if (i + kNeedle.size() <= withoutBrackets.size())
    {
      bool match = true;
      for (std::size_t n = 0; n < kNeedle.size(); ++n)
      {
        const auto have = static_cast<unsigned char>(withoutBrackets[i + n]);
        const auto want = static_cast<unsigned char>(kNeedle[n]);
        if (std::toupper(have) != std::toupper(want))
        {
          match = false;
          break;
        }
      }
      if (match)
      {
        const auto beforeOk = i == 0 || !isAsciiWordChar(static_cast<unsigned char>(withoutBrackets[i - 1]));
        const auto afterIndex = i + kNeedle.size();
        const auto afterOk = afterIndex == withoutBrackets.size()
          || !isAsciiWordChar(static_cast<unsigned char>(withoutBrackets[afterIndex]));
        if (beforeOk && afterOk)
        {
          i = afterIndex;
          continue;
        }
      }
    }
    withoutIgnore.push_back(withoutBrackets[i]);
    ++i;
  }

  std::string collapsed;
  collapsed.reserve(withoutIgnore.size());
  for (std::size_t i = 0; i < withoutIgnore.size();)
  {
    const auto ch = withoutIgnore[i];
    if (ch == ' ' || ch == '\t')
    {
      std::size_t run = i;
      while (run < withoutIgnore.size()
        && (withoutIgnore[run] == ' ' || withoutIgnore[run] == '\t'))
      {
        ++run;
      }
      if (run - i >= 2)
      {
        collapsed.push_back(' ');
      }
      else
      {
        collapsed.append(withoutIgnore, i, run - i);
      }
      i = run;
      continue;
    }
    collapsed.push_back(ch);
    ++i;
  }
  return collapsed;
}

std::string hoverImageUri(std::string_view extensionUri, std::string_view filename)
{
  if (extensionUri.empty())
  {
    return std::string(filename);
  }
  auto uri = lsp::Uri::parse(extensionUri);
  if (!uri.isValid())
  {
    return std::string(filename);
  }
  auto path = std::string(uri.path());
  if (path.empty() || path.back() != '/')
  {
    path += '/';
  }
  path += "ADT-documentation/";
  path += filename;
  uri.setPath(path);
  return uri.toString();
}

std::string_view hoverImageFile(std::string_view word)
{
  for (const auto& [label, filename] : kHoverImages)
  {
    if (label == word)
    {
      return filename;
    }
  }
  return {};
}

lsp::Hover makeHover(std::string markdown)
{
  return lsp::Hover{
    .contents = lsp::MarkupContent{
      .kind = lsp::MarkupKind::Markdown,
      .value = std::move(markdown),
    },
  };
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

auto wordAtPosition(std::string_view text, lsp::Position position) -> std::optional<std::string>
{
  const auto line = lineAt(text, position.line).value_or(std::string_view{});
  if (line.empty() && position.character != 0)
  {
    return std::nullopt;
  }
  const auto utf16 = utf8To16(line);
  const auto column = static_cast<std::size_t>(position.character);
  if (column > utf16.size())
  {
    return std::nullopt;
  }

  std::size_t i = 0;
  while (i < utf16.size())
  {
    if (!isJsWordChar(utf16[i]))
    {
      ++i;
      continue;
    }
    const auto start = i;
    while (i < utf16.size() && isJsWordChar(utf16[i]))
    {
      ++i;
    }
    if (i < utf16.size() && utf16[i] == u'$')
    {
      ++i;
    }
    const auto end = i;
    if (column >= start && column < end)
    {
      return utf16To8(std::u16string_view(utf16.data() + start, end - start));
    }
  }
  return std::nullopt;
}

auto cheapHover(const CheapHoverContext& context) -> lsp::TextDocumentHoverResult
{
  if (context.snapshot.languageId != "vertexadt")
  {
    return nullptr;
  }
  const auto documentPath = context.layout ? pathFromUri(context.snapshot.uri) : std::filesystem::path{};
  if (context.layout)
  {
    if (auto dir = dirTokenHover(*context.layout, documentPath, context.snapshot.text, context.position))
    {
      std::string markdown;
      if (const auto* keyword = catalog::findKeyword(dir->token->token))
      {
        markdown = keywordMarkdown(*keyword) + "\n\n";
      }
      markdown += "→ " + dirPathMarkdown(dir->path);
      return makeHover(std::move(markdown));
    }
    // The quoted `new_piece` set string: the set's resolved vxsettings lines.
    if (auto set = pieceSetHover(context.snapshot, context.position, *context.layout))
    {
      return makeHover(std::move(*set));
    }
  }

  const auto word = wordAtPosition(context.snapshot.text, context.position);
  if (!word)
  {
    return nullptr;
  }

  if (isDebugDump(context.snapshot.text)
    && word->size() >= 2
    && !catalog::isKeywordLabelIgnoreCase(*word))
  {
    if (auto found = findMostRecentDebugValue(
          context.snapshot.text,
          static_cast<int>(context.position.line),
          *word))
    {
      std::string markdown = "**" + *word + "** = `" + found->value + "`";
      if (found->line != static_cast<int>(context.position.line))
      {
        markdown += "\n\n_Set on line " + std::to_string(found->line + 1) + "_";
      }
      return makeHover(std::move(markdown));
    }
  }

  std::string body;
  std::optional<DirPathDisplay> variablePath;
  if (const auto* keyword = catalog::findKeyword(*word))
  {
    body = keywordMarkdown(*keyword);
    // if/else/endif and loop/endloop: the partner keywords above the catalog text.
    const auto pairing = pairControlBlocks(context.snapshot.text);
    if (const auto hit = controlBlockAt(pairing, context.position, false))
    {
      // Keywords without examples already start with the `---` rule.
      const auto separator = body.starts_with("\n\n---\n") ? "" : "\n\n---\n\n";
      body = controlBlockHoverMarkdown(*hit) + separator + body;
    }
  }
  else
  {
    if (context.layout)
    {
      variablePath = dirVariableHover(
        *context.layout,
        documentPath,
        context.snapshot.text,
        context.position,
        *word);
    }
    VariableExtract fallback;
    const auto* live = liveExtract(context.snapshot, context.extracts, fallback);
    const auto neighborhood = context.extracts.neighborhood(context.snapshot.uri);
    const auto* included = neighborhood.get();
    if (const auto* row = firstTruthy(live->hoverVariables, included ? &included->hoverVariables : nullptr, *word))
    {
      body = variableMarkdown("Variable", row->name, *row->comment);
    }
    else if (const auto* row = firstTruthy(
               live->hoverStringVariables,
               included ? &included->hoverStringVariables : nullptr,
               *word))
    {
      body = variableMarkdown("String Variable", row->name, *row->comment);
    }
  }

  if (body.empty() && variablePath)
  {
    body = variableMarkdown(word->ends_with('$') ? "String Variable" : "Variable", *word, "");
  }
  if (body.empty())
  {
    return nullptr;
  }

  if (const auto filename = hoverImageFile(*word); !filename.empty())
  {
    body = "![Description](" + hoverImageUri(context.extensionUri, filename) + ")\n\n" + body;
  }
  else if (body.starts_with("Variable:") || body.starts_with("String Variable:"))
  {
    body = stripHoverMarkers(std::move(body));
  }
  if (variablePath)
  {
    // After stripHoverMarkers, which would eat the link's brackets.
    while (!body.empty() && (body.back() == '\n' || body.back() == ' '))
    {
      body.pop_back();
    }
    body += "\n\n→ " + dirPathMarkdown(*variablePath);
  }
  return makeHover(std::move(body));
}

} // namespace vertexadt
