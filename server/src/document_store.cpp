#include "document_store.h"

#include "debug_trace.h"
#include "text_util.h"

#include <algorithm>
#include <cstdint>
#include <variant>

namespace vertexadt {
namespace {

constexpr unsigned kUtf8Invalid = 0xFFFFFFFFu;

unsigned nextUtf8(std::string_view text, std::size_t& offset)
{
  if (offset >= text.size())
  {
    return kUtf8Invalid;
  }

  const auto lead = static_cast<unsigned char>(text[offset]);
  if (lead < 0x80)
  {
    ++offset;
    return lead;
  }

  int extra = 0;
  unsigned cp = 0;
  if ((lead & 0xE0) == 0xC0)
  {
    extra = 1;
    cp = lead & 0x1F;
  }
  else if ((lead & 0xF0) == 0xE0)
  {
    extra = 2;
    cp = lead & 0x0F;
  }
  else if ((lead & 0xF8) == 0xF0)
  {
    extra = 3;
    cp = lead & 0x07;
  }
  else
  {
    ++offset;
    return 0xFFFD;
  }

  if (offset + static_cast<std::size_t>(extra) >= text.size())
  {
    offset = text.size();
    return 0xFFFD;
  }

  for (int i = 0; i < extra; ++i)
  {
    const auto cont = static_cast<unsigned char>(text[offset + 1 + i]);
    if ((cont & 0xC0) != 0x80)
    {
      ++offset;
      return 0xFFFD;
    }
    cp = (cp << 6) | (cont & 0x3F);
  }
  offset += static_cast<std::size_t>(extra) + 1;
  return cp;
}

std::size_t utf16Units(unsigned codepoint)
{
  return codepoint > 0xFFFF ? 2 : 1;
}

std::size_t lineStart(std::string_view text, unsigned line)
{
  std::size_t offset = 0;
  unsigned current = 0;
  while (current < line && offset < text.size())
  {
    const auto ch = text[offset];
    if (ch == '\n')
    {
      ++offset;
      ++current;
    }
    else if (ch == '\r')
    {
      ++offset;
      if (offset < text.size() && text[offset] == '\n')
      {
        ++offset;
      }
      ++current;
    }
    else
    {
      ++offset;
    }
  }
  return offset;
}

std::size_t positionToOffset(std::string_view text, lsp::Position position)
{
  const auto start = lineStart(text, position.line);
  auto offset = start;
  std::size_t units = 0;
  while (offset < text.size() && units < position.character)
  {
    if (text[offset] == '\n' || text[offset] == '\r')
    {
      break;
    }
    const auto codepoint = nextUtf8(text, offset);
    if (codepoint == kUtf8Invalid)
    {
      break;
    }
    units += utf16Units(codepoint);
  }
  return offset;
}

void applyPartial(std::string& text, const lsp::TextDocumentContentChangePartial& change)
{
  const auto begin = positionToOffset(text, change.range.start);
  const auto end = positionToOffset(text, change.range.end);
  const auto from = std::min(begin, text.size());
  const auto to = std::min(std::max(end, from), text.size());
  text.replace(from, to - from, change.text);
}

} // namespace

void DocumentStore::open(const lsp::DidOpenTextDocumentParams& params)
{
  auto text = std::make_shared<std::string>(params.textDocument.text);
  const auto path = params.textDocument.uri.path();
  const auto debugTrace = isDebugTraceDocument(path, *text);
  const auto lock = std::lock_guard(mutex_);
  buffers_.insert_or_assign(
    params.textDocument.uri,
    Buffer{
      .version = params.textDocument.version,
      .languageId = params.textDocument.languageId,
      .text = std::move(text),
      .normalizedPath = normalizeFsPath(params.textDocument.uri.fsPath()),
      .debugTrace = debugTrace,
    });
}

void DocumentStore::change(const lsp::DidChangeTextDocumentParams& params)
{
  const auto lock = std::lock_guard(mutex_);
  auto it = buffers_.find(params.textDocument.uri);
  if (it == buffers_.end())
  {
    return;
  }

  auto& buffer = it->second;
  for (const auto& change : params.contentChanges)
  {
    if (const auto* whole = std::get_if<lsp::TextDocumentContentChangeWholeDocument>(&change))
    {
      buffer.text = std::make_shared<std::string>(whole->text);
    }
    else if (const auto* partial = std::get_if<lsp::TextDocumentContentChangePartial>(&change))
    {
      if (buffer.text.use_count() > 1)
      {
        buffer.text = std::make_shared<std::string>(*buffer.text);
      }
      applyPartial(*buffer.text, *partial);
    }
  }
  buffer.version = params.textDocument.version;
  buffer.debugTrace = isDebugTraceDocument(params.textDocument.uri.path(), *buffer.text);
}

void DocumentStore::close(const lsp::Uri& uri)
{
  const auto lock = std::lock_guard(mutex_);
  buffers_.erase(uri);
}

auto DocumentStore::snapshot(const lsp::Uri& uri) const -> std::optional<Snapshot>
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = buffers_.find(uri);
  if (it == buffers_.end())
  {
    return std::nullopt;
  }
  return Snapshot{
    .uri = it->first,
    .version = it->second.version,
    .languageId = it->second.languageId,
    .text = *it->second.text,
  };
}

auto DocumentStore::snapshotByNormalizedPath(std::string_view normalizedPath) const
  -> std::optional<Snapshot>
{
  const auto lock = std::lock_guard(mutex_);
  for (const auto& [uri, buffer] : buffers_)
  {
    if (buffer.normalizedPath == normalizedPath && !buffer.debugTrace)
    {
      return Snapshot{
        .uri = uri,
        .version = buffer.version,
        .languageId = buffer.languageId,
        .text = *buffer.text,
      };
    }
  }
  return std::nullopt;
}

auto DocumentStore::openUris() const -> std::vector<lsp::Uri>
{
  const auto lock = std::lock_guard(mutex_);
  std::vector<lsp::Uri> uris;
  uris.reserve(buffers_.size());
  for (const auto& [uri, buffer] : buffers_)
  {
    if (!buffer.debugTrace)
    {
      uris.push_back(uri);
    }
  }
  return uris;
}

bool DocumentStore::isDebugTrace(const lsp::Uri& uri) const
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = buffers_.find(uri);
  return it != buffers_.end() && it->second.debugTrace;
}

auto DocumentStore::debugTraceText(const lsp::Uri& uri) const -> std::optional<DebugTraceText>
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = buffers_.find(uri);
  if (it == buffers_.end() || !it->second.debugTrace)
  {
    return std::nullopt;
  }
  return DebugTraceText{
    .version = it->second.version,
    .languageId = it->second.languageId,
    .text = it->second.text,
  };
}

bool DocumentStore::isOpen(const lsp::Uri& uri) const
{
  const auto lock = std::lock_guard(mutex_);
  return buffers_.contains(uri);
}

bool DocumentStore::isOpenNormalizedPath(std::string_view normalizedPath) const
{
  const auto lock = std::lock_guard(mutex_);
  for (const auto& [_, buffer] : buffers_)
  {
    if (buffer.normalizedPath == normalizedPath)
    {
      return true;
    }
  }
  return false;
}

} // namespace vertexadt
