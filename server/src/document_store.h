#pragma once

#include <lsp/types.h>
#include <lsp/uri.h>

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

// Open documents. Updates happen on the message-loop thread; workers copy
// snapshots under a short lock that does not span a validate.
class DocumentStore
{
public:
  void open(const lsp::DidOpenTextDocumentParams& params);
  void change(const lsp::DidChangeTextDocumentParams& params);
  void close(const lsp::Uri& uri);

  [[nodiscard]] auto snapshot(const lsp::Uri& uri) const -> std::optional<Snapshot>;
  [[nodiscard]] auto snapshotByNormalizedPath(std::string_view normalizedPath) const
    -> std::optional<Snapshot>;
  [[nodiscard]] auto openUris() const -> std::vector<lsp::Uri>;
  [[nodiscard]] bool isOpen(const lsp::Uri& uri) const;
  [[nodiscard]] bool isOpenNormalizedPath(std::string_view normalizedPath) const;

private:
  struct Buffer
  {
    int version = 0;
    std::string languageId;
    std::string text;
    std::string normalizedPath;
  };

  mutable std::mutex mutex_;
  std::unordered_map<lsp::Uri, Buffer> buffers_;
};

} // namespace vertexadt
