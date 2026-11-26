#pragma once

// Debug traces (*.dbg execution dumps), docs/adr/0025-debug-trace-fast-path.md.
// One shared text buffer, a line-offset table and a background assignment
// index. Nothing here reads includes, calls or the workspace.

#include "debug_dump.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace vertexadt {

// Bytes from the start of a document searched for the debug header.
inline constexpr std::size_t kDebugHeaderWindow = 4096;

// `START OF DEBUG in FILE|SECTION` within the first kDebugHeaderWindow bytes.
[[nodiscard]] bool hasDebugHeader(std::string_view text);

// A document is a debug trace when its path ends in `.dbg` (any case) or its
// head carries the debug header.
[[nodiscard]] bool isDebugTraceDocument(std::string_view path, std::string_view text);

// One parsed assignment (a name's value on one line), byte offsets into the
// trace text.
struct DebugAssignmentEntry
{
  int line = 0;
  std::uint32_t valueOffset = 0;
  std::uint32_t valueLength = 0;
};

// name → assignments in line order (one per line).
struct DebugAssignmentIndex
{
  std::unordered_map<std::string, std::vector<DebugAssignmentEntry>> byName;
};

// One assignment parsed by parseLineAssignmentSpans: name and value are
// byte ranges in the line.
struct DebugAssignmentSpan
{
  std::size_t nameBegin = 0;
  std::size_t nameEnd = 0;
  std::size_t valueBegin = 0;
  std::size_t valueEnd = 0;
};

// Hand-written parseLineAssignments (same results, no std::regex). Later
// spans for the same name win, as in the map version.
void parseLineAssignmentSpans(std::string_view line, std::vector<DebugAssignmentSpan>& out);

// The parts of one trace line, byte ranges in the line (CR excluded).
struct DebugLineParts
{
  enum class Kind
  {
    Code,       // code [codeBegin, codeEnd), optional value and marker
    Structural, // START/END OF DEBUG, END of FILE: [codeBegin, codeEnd) is the banner
  };
  Kind kind = Kind::Code;
  std::size_t codeBegin = 0;
  std::size_t codeEnd = 0;
  std::size_t valueBegin = 0; // `<...>` including the angle brackets; empty when absent
  std::size_t valueEnd = 0;
  std::size_t markerBegin = 0; // `_if_begin_` style marker; empty when absent
  std::size_t markerEnd = 0;
};

// Split from the right: optional trailing `_word_` marker, then the last
// whitespace-preceded `<...>` group, then code.
[[nodiscard]] auto splitDebugLine(std::string_view line) -> DebugLineParts;

class DebugTrace
{
public:
  // Builds the line table (fast); the index arrives later via setIndex.
  DebugTrace(std::shared_ptr<const std::string> text, int version);

  [[nodiscard]] int version() const { return version_; }
  [[nodiscard]] auto text() const -> std::string_view { return *text_; }
  [[nodiscard]] int lineCount() const { return static_cast<int>(lineStarts_.size()); }
  // Line `i` without its `\n` (a trailing `\r` is kept, as splitLines does).
  [[nodiscard]] auto line(int i) const -> std::string_view;

  // The assignment index, or null while it is still being built (or was
  // abandoned because a newer version replaced this trace).
  [[nodiscard]] auto index() const -> std::shared_ptr<const DebugAssignmentIndex>;
  // Waits for the index (null when abandoned).
  [[nodiscard]] auto waitIndex() const -> std::shared_ptr<const DebugAssignmentIndex>;
  void setIndex(std::shared_ptr<const DebugAssignmentIndex> index);

  // findMostRecentDebugValue over the index: the last assignment of
  // `name` on lines 0…hoverLine. Waits for the index.
  [[nodiscard]] auto mostRecentAssignment(int hoverLine, std::string_view name) const
    -> std::optional<DebugAssignment>;

private:
  std::shared_ptr<const std::string> text_;
  int version_ = 0;
  std::vector<std::uint32_t> lineStarts_;

  mutable std::mutex indexMutex_;
  mutable std::condition_variable indexReady_;
  bool indexSettled_ = false;
  std::shared_ptr<const DebugAssignmentIndex> index_;
};

// One pass over every line. `canceled` is polled every few thousand lines;
// returns null when it fires.
[[nodiscard]] auto buildDebugAssignmentIndex(
  const DebugTrace& trace,
  const std::function<bool()>& canceled = {}) -> std::shared_ptr<const DebugAssignmentIndex>;

// Open debug traces by URI string. update() builds the line table on the
// caller's thread and queues the index for a single background worker;
// `onIndexReady` runs on that worker when an index for the current version
// lands.
class DebugTraceStore
{
public:
  explicit DebugTraceStore(std::function<void()> onIndexReady);
  ~DebugTraceStore();

  DebugTraceStore(const DebugTraceStore&) = delete;
  DebugTraceStore& operator=(const DebugTraceStore&) = delete;

  void update(const std::string& uri, int version, std::shared_ptr<const std::string> text);
  void erase(const std::string& uri);
  [[nodiscard]] auto get(const std::string& uri) const -> std::shared_ptr<const DebugTrace>;

private:
  struct Job
  {
    std::string uri;
    std::shared_ptr<DebugTrace> trace;
    std::shared_ptr<std::atomic<bool>> canceled;
  };

  struct Entry
  {
    std::shared_ptr<DebugTrace> trace;
    std::shared_ptr<std::atomic<bool>> canceled;
  };

  void workerLoop();

  std::function<void()> onIndexReady_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;
  std::unordered_map<std::string, Entry> entries_;
  std::deque<Job> jobs_;
  std::thread worker_;
};

} // namespace vertexadt
