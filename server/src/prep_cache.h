#pragma once

#include "validate.h"

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace vertexadt {

// One shared prep per open URI, latest version only, shared read-only by the
// editor pull and semantic tokens. An entry is valid while its version and
// strict-mode flag match, the URI's include-neighborhood generation is the one
// the prep read, and nothing cleared the cache since it was built. The first
// request for a URI+version builds; a concurrent one waits on that build and
// builds itself if the builder is canceled.
class PrepCache
{
public:
  using Build = std::function<std::shared_ptr<const DocumentPrep>()>;

  [[nodiscard]] auto getOrBuild(
    const std::string& uriKey,
    int version,
    bool strictModeEnabled,
    const std::function<std::uint64_t()>& neighborhoodGeneration,
    const Build& build,
    const std::function<void()>& pollCancel) -> std::shared_ptr<const DocumentPrep>;

  void erase(const std::string& uriKey);
  // Settings, folders, watched files, and dropped diagnostic caches.
  void clear();

private:
  struct Entry
  {
    int version = 0;
    bool strictModeEnabled = false;
    std::uint64_t epoch = 0;
    std::uint64_t buildId = 0;
    std::shared_future<std::shared_ptr<const DocumentPrep>> prep;
  };

  void eraseIfBuild(const std::string& uriKey, std::uint64_t buildId);

  std::mutex mutex_;
  std::unordered_map<std::string, Entry> entries_;
  std::uint64_t epoch_ = 0;
  std::uint64_t nextBuildId_ = 0;
};

} // namespace vertexadt
