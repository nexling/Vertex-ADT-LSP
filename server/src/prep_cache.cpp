#include "prep_cache.h"

#include <chrono>
#include <exception>
#include <utility>

namespace vertexadt {

namespace {

// Set on a shared build whose builder threw (usually its own cancel). Waiters
// never inherit that; they retry and build themselves.
struct PrepBuildAbandoned
{
};

constexpr auto kWaitSlice = std::chrono::milliseconds(5);

} // namespace

auto PrepCache::getOrBuild(
  const std::string& uriKey,
  int version,
  bool strictModeEnabled,
  const std::function<std::uint64_t()>& neighborhoodGeneration,
  const Build& build,
  const std::function<void()>& pollCancel) -> std::shared_ptr<const DocumentPrep>
{
  for (;;)
  {
    if (pollCancel)
    {
      pollCancel();
    }

    auto lock = std::unique_lock(mutex_);
    const auto it = entries_.find(uriKey);
    if (it != entries_.end()
      && it->second.version == version
      && it->second.strictModeEnabled == strictModeEnabled
      && it->second.epoch == epoch_)
    {
      auto shared = it->second.prep;
      const auto buildId = it->second.buildId;
      lock.unlock();

      while (shared.wait_for(kWaitSlice) != std::future_status::ready)
      {
        if (pollCancel)
        {
          pollCancel();
        }
      }
      std::shared_ptr<const DocumentPrep> prep;
      try
      {
        prep = shared.get();
      }
      catch (const PrepBuildAbandoned&)
      {
        eraseIfBuild(uriKey, buildId);
        continue;
      }
      if (prep->neighborhoodGeneration == neighborhoodGeneration())
      {
        return prep;
      }
      eraseIfBuild(uriKey, buildId);
      continue;
    }

    // An older request never evicts a newer version's entry.
    if (it != entries_.end() && it->second.version > version)
    {
      lock.unlock();
      return build();
    }

    auto promise = std::make_shared<std::promise<std::shared_ptr<const DocumentPrep>>>();
    const auto buildId = ++nextBuildId_;
    entries_[uriKey] = Entry{
      .version = version,
      .strictModeEnabled = strictModeEnabled,
      .epoch = epoch_,
      .buildId = buildId,
      .prep = promise->get_future().share(),
    };
    lock.unlock();

    try
    {
      auto prep = build();
      promise->set_value(prep);
      return prep;
    }
    catch (...)
    {
      promise->set_exception(std::make_exception_ptr(PrepBuildAbandoned{}));
      eraseIfBuild(uriKey, buildId);
      throw;
    }
  }
}

void PrepCache::erase(const std::string& uriKey)
{
  const auto lock = std::lock_guard(mutex_);
  entries_.erase(uriKey);
}

void PrepCache::clear()
{
  const auto lock = std::lock_guard(mutex_);
  entries_.clear();
  ++epoch_;
}

void PrepCache::eraseIfBuild(const std::string& uriKey, std::uint64_t buildId)
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = entries_.find(uriKey);
  if (it != entries_.end() && it->second.buildId == buildId)
  {
    entries_.erase(it);
  }
}

} // namespace vertexadt
