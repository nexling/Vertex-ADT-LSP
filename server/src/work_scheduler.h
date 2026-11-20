#pragma once

#include <lsp/message_handler.h>
#include <lsp/uri.h>

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vertexadt {

// Product cheap/heavy pools around lsp-framework's FIFO ThreadPool.
// Cheap work is joined on the message-loop thread so its JSON-RPC response
// is never queued behind a heavy TaskFunction on that FIFO. Heavy work runs
// on this pool; the library TaskFunction only waits and polls cancel.
class WorkScheduler
{
public:
  WorkScheduler();
  ~WorkScheduler();

  WorkScheduler(const WorkScheduler&) = delete;
  WorkScheduler& operator=(const WorkScheduler&) = delete;

  template<class F>
  auto runCheap(F&& job) -> std::invoke_result_t<std::decay_t<F>>;

  template<class F>
  auto submitHeavy(const lsp::Uri& uri, F&& job)
    -> std::future<std::invoke_result_t<std::decay_t<F>>>;

  void noteEdited(const lsp::Uri& uri);
  void postCheapDetached(std::function<void()> job);
  void trackHeavy(const lsp::Uri& uri, lsp::RequestId id, int version);
  void cancelHeavyForUri(lsp::MessageHandler& handler, const lsp::Uri& uri);
  void completeHeavy(const lsp::Uri& uri, const lsp::RequestId& id);

private:
  class Pool
  {
  public:
    explicit Pool(unsigned threadCount, bool preferEdited);
    ~Pool();

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    void noteEdited(std::string uri);
    void post(std::string uri, std::function<void()> job);

  private:
    struct Job
    {
      std::string uri;
      std::function<void()> run;
    };

    void workerLoop();
    auto takeJob() -> std::optional<Job>;

    const bool preferEdited_;
    bool stop_ = false;
    std::string editedUri_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Job> jobs_;
    std::vector<std::thread> threads_;
  };

  void postCheap(std::function<void()> job);

  Pool cheap_;
  Pool heavy_;
  std::mutex heavyMutex_;
  std::unordered_map<std::string, std::vector<lsp::RequestId>> heavyByUri_;
};

template<class F>
auto WorkScheduler::runCheap(F&& job) -> std::invoke_result_t<std::decay_t<F>>
{
  using R = std::invoke_result_t<std::decay_t<F>>;
  auto promise = std::make_shared<std::promise<R>>();
  auto future = promise->get_future();
  postCheap([job = std::forward<F>(job), promise]() mutable {
    try
    {
      if constexpr (std::is_void_v<R>)
      {
        job();
        promise->set_value();
      }
      else
      {
        promise->set_value(job());
      }
    }
    catch (...)
    {
      promise->set_exception(std::current_exception());
    }
  });
  return future.get();
}

template<class F>
auto WorkScheduler::submitHeavy(const lsp::Uri& uri, F&& job)
  -> std::future<std::invoke_result_t<std::decay_t<F>>>
{
  using R = std::invoke_result_t<std::decay_t<F>>;
  auto promise = std::make_shared<std::promise<R>>();
  auto future = promise->get_future();
  heavy_.post(uri.toString(), [job = std::forward<F>(job), promise]() mutable {
    try
    {
      if constexpr (std::is_void_v<R>)
      {
        job();
        promise->set_value();
      }
      else
      {
        promise->set_value(job());
      }
    }
    catch (...)
    {
      promise->set_exception(std::current_exception());
    }
  });
  return future;
}

} // namespace vertexadt
