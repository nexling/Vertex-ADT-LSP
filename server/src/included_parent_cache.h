#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vertexadt {

struct IncludedParentVar
{
  std::string name;
  std::string type;
  std::optional<std::string> hover;
};

// One `priorType(name, line, "")` lookup the extraction made against this
// pass's type writes (a `vec_copy` line in the parent), with its answer.
struct IncludedParentQuery
{
  std::string name;
  int line = 0;
  std::string answer;
};

// The `##included_from` parent extraction before comment fill-in. It is valid
// for a pass while `text` is byte-equal to the parent text that pass reads and
// every recorded query still gives the same answer.
struct IncludedParentExtract
{
  std::string text;
  std::vector<IncludedParentVar> variables;
  std::vector<IncludedParentVar> stringVariables;
  std::vector<IncludedParentQuery> priorTypeQueries;
};

// Keyed by normalized parent path. Entries validate themselves (see above),
// so no invalidation hook evicts them; the cap only bounds memory.
class IncludedParentCache
{
public:
  [[nodiscard]] auto get(const std::string& normalizedPath) const
    -> std::shared_ptr<const IncludedParentExtract>
  {
    const auto lock = std::lock_guard(mutex_);
    const auto it = entries_.find(normalizedPath);
    return it == entries_.end() ? nullptr : it->second.extract;
  }

  void put(const std::string& normalizedPath, std::shared_ptr<const IncludedParentExtract> extract)
  {
    const auto lock = std::lock_guard(mutex_);
    if (!entries_.contains(normalizedPath) && entries_.size() >= kMaxEntries)
    {
      const auto oldest = std::min_element(entries_.begin(), entries_.end(), [](const auto& a, const auto& b) {
        return a.second.stamp < b.second.stamp;
      });
      entries_.erase(oldest);
    }
    entries_[normalizedPath] = Entry{nextStamp_++, std::move(extract)};
  }

private:
  static constexpr std::size_t kMaxEntries = 32;

  struct Entry
  {
    std::uint64_t stamp = 0;
    std::shared_ptr<const IncludedParentExtract> extract;
  };

  mutable std::mutex mutex_;
  std::unordered_map<std::string, Entry> entries_;
  std::uint64_t nextStamp_ = 0;
};

} // namespace vertexadt
