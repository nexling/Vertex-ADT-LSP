#include "work_scheduler.h"

#include <algorithm>

namespace vertexadt {
namespace {

unsigned cheapThreadCount()
{
  return 2;
}

unsigned heavyThreadCount()
{
  const auto hardware = std::max(2u, std::thread::hardware_concurrency());
  return std::max(2u, hardware / 2);
}

} // namespace

WorkScheduler::Pool::Pool(unsigned threadCount, bool preferEdited)
  : preferEdited_(preferEdited)
{
  threads_.reserve(threadCount);
  for (unsigned i = 0; i < threadCount; ++i)
  {
    threads_.emplace_back([this] { workerLoop(); });
  }
}

WorkScheduler::Pool::~Pool()
{
  {
    const auto lock = std::lock_guard(mutex_);
    stop_ = true;
  }
  cv_.notify_all();
  for (auto& thread : threads_)
  {
    thread.join();
  }
}

void WorkScheduler::Pool::noteEdited(std::string uri)
{
  const auto lock = std::lock_guard(mutex_);
  editedUri_ = std::move(uri);
}

void WorkScheduler::Pool::post(std::string uri, std::function<void()> job)
{
  {
    const auto lock = std::lock_guard(mutex_);
    jobs_.push(Job{.uri = std::move(uri), .run = std::move(job)});
  }
  cv_.notify_one();
}

void WorkScheduler::Pool::workerLoop()
{
  while (true)
  {
    std::optional<Job> job;
    {
      auto lock = std::unique_lock(mutex_);
      cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
      if (stop_ && jobs_.empty())
      {
        return;
      }
      job = takeJob();
    }
    if (job)
    {
      job->run();
    }
  }
}

auto WorkScheduler::Pool::takeJob() -> std::optional<Job>
{
  if (jobs_.empty())
  {
    return std::nullopt;
  }

  if (preferEdited_ && !editedUri_.empty())
  {
    const auto count = jobs_.size();
    for (std::size_t i = 0; i < count; ++i)
    {
      auto job = std::move(jobs_.front());
      jobs_.pop();
      if (job.uri == editedUri_)
      {
        return job;
      }
      jobs_.push(std::move(job));
    }
  }

  auto job = std::move(jobs_.front());
  jobs_.pop();
  return job;
}

WorkScheduler::WorkScheduler()
  : cheap_(cheapThreadCount(), false)
  , heavy_(heavyThreadCount(), true)
{
}

WorkScheduler::~WorkScheduler() = default;

void WorkScheduler::postCheap(std::function<void()> job)
{
  cheap_.post({}, std::move(job));
}

void WorkScheduler::postCheapDetached(std::function<void()> job)
{
  postCheap(std::move(job));
}

void WorkScheduler::noteEdited(const lsp::Uri& uri)
{
  heavy_.noteEdited(uri.toString());
}

void WorkScheduler::trackHeavy(const lsp::Uri& uri, lsp::RequestId id, int)
{
  const auto lock = std::lock_guard(heavyMutex_);
  heavyByUri_[uri.toString()].push_back(std::move(id));
}

void WorkScheduler::cancelHeavyForUri(lsp::MessageHandler& handler, const lsp::Uri& uri)
{
  std::vector<lsp::RequestId> ids;
  {
    const auto lock = std::lock_guard(heavyMutex_);
    auto it = heavyByUri_.find(uri.toString());
    if (it == heavyByUri_.end())
    {
      return;
    }
    ids = it->second;
  }
  for (const auto& id : ids)
  {
    handler.cancel(id);
  }
}

void WorkScheduler::completeHeavy(const lsp::Uri& uri, const lsp::RequestId& id)
{
  const auto lock = std::lock_guard(heavyMutex_);
  auto it = heavyByUri_.find(uri.toString());
  if (it == heavyByUri_.end())
  {
    return;
  }
  std::erase(it->second, id);
  if (it->second.empty())
  {
    heavyByUri_.erase(it);
  }
}

} // namespace vertexadt
