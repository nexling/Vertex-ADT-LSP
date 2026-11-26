#pragma once

#include <lsp/types.h>
#include <lsp/uri.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vertexadt {

// Immutable copy of a live buffer for workers. The message-loop thread never
// hands out a mutable view of open text.
struct Snapshot
{
  lsp::Uri uri;
  int version = 0;
  std::string languageId;
  std::string text;
};

// A debug trace's buffer, shared instead of copied
// (docs/adr/0025-debug-trace-fast-path.md).
struct DebugTraceText
{
  int version = 0;
  std::string languageId;
  std::shared_ptr<const std::string> text;
};

// Open documents. Updates happen on the message-loop thread; workers copy
// snapshots under a short lock that does not span a validate.
class DocumentStore
{
public:
  void open(const lsp::DidOpenTextDocumentParams& params);
  void change(const lsp::DidChangeTextDocumentParams& params);
  void close(const lsp::Uri& uri);

  [[nodiscard]] auto snapshot(const lsp::Uri& uri) const -> std::optional<Snapshot>;
  // Debug traces are skipped (they are not part of the workspace).
  [[nodiscard]] auto snapshotByNormalizedPath(std::string_view normalizedPath) const
    -> std::optional<Snapshot>;
  // Debug traces are skipped (they are not part of the workspace).
  [[nodiscard]] auto openUris() const -> std::vector<lsp::Uri>;
  [[nodiscard]] bool isOpen(const lsp::Uri& uri) const;
  [[nodiscard]] bool isOpenNormalizedPath(std::string_view normalizedPath) const;

  // `.dbg` path or debug header in the head, decided on open/change.
  [[nodiscard]] bool isDebugTrace(const lsp::Uri& uri) const;
  // The shared buffer of an open debug trace; nullopt for anything else.
  [[nodiscard]] auto debugTraceText(const lsp::Uri& uri) const -> std::optional<DebugTraceText>;

private:
  struct Buffer
  {
    int version = 0;
    std::string languageId;
    // Copy-on-write: a debug trace hands this pointer out.
    std::shared_ptr<std::string> text;
    std::string normalizedPath;
    bool debugTrace = false;
  };

  mutable std::mutex mutex_;
  std::unordered_map<lsp::Uri, Buffer> buffers_;
};

} // namespace vertexadt
