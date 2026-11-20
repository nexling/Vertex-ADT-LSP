#pragma once

#include "library_paths.h"
#include "workspace_index.h"

#include <lsp/uri.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace vertexadt {

struct LinkedFileSnapshot
{
  std::string normalizedPath;
  std::string text;
  bool fromBuffer = false;
};

class LinkedFileStore
{
public:
  void put(std::string normalizedPath, std::string text, bool fromBuffer);
  void drop(std::string_view normalizedPath);
  void dropAll();
  [[nodiscard]] auto get(std::string_view normalizedPath) const
    -> std::shared_ptr<const LinkedFileSnapshot>;

  [[nodiscard]] auto expectedCallParamCount(
    std::string_view normalizedPath,
    std::optional<std::string_view> targetSection,
    const WorkspaceIndex& index) const -> int;

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<const LinkedFileSnapshot>> snapshots_;
};

[[nodiscard]] bool isCrossFileCapable(std::string_view text);
[[nodiscard]] auto includeFromGraph(std::string_view text) -> std::vector<std::string>;

} // namespace vertexadt
