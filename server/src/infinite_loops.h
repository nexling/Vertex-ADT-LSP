#pragma once

// Infinite-loop checks for bare `loop ... endloop`
// (docs/adr/0027-native-infinite-loop-warnings.md).
//
// - Warning on the `loop` keyword when the body has no `exitif`, `break`,
//   `exit` or `quit`. An `exitif` inside an inner loop leaves only that inner
//   loop; `break` / `exit` / `quit` leave every loop.
// - Information on each `exitif` keyword when the loop has no `break` / `exit`
//   / `quit` and no exitif condition variable can change in the body.
//
// `loop N` is never checked. `# include` is inlined (through `loadInclude`);
// `call` is opaque and silences the Information check. A file with an
// unpaired `loop` / `endloop` gets no diagnostics.

#include "text_util.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

inline constexpr std::string_view kInfiniteLoopCode = "vertexadt/infinite-loop";
inline constexpr std::string_view kExitifNeverChangesCode = "vertexadt/exitif-never-changes";

enum class InfiniteLoopKind
{
  NoExit,            // Warning on `loop`
  ExitifNeverChanges // Information on `exitif`
};

struct InfiniteLoopIssue
{
  InfiniteLoopKind kind = InfiniteLoopKind::NoExit;
  int line = 0;
  // UTF-16 columns of the keyword.
  int startChar = 0;
  int endChar = 0;
  std::string message;
};

// An included file: a stable key (its resolved path) for cycle detection and
// its whole text. The walker cuts the target section out of it.
struct IncludedCode
{
  std::string key;
  std::string text;
};

// nullopt: the include cannot be resolved or read. A loop containing one gets
// no diagnostics, since the unseen code may leave or change anything.
using IncludeLoader = std::function<std::optional<IncludedCode>(const IncludeCallTarget&)>;

[[nodiscard]] auto findInfiniteLoops(std::string_view text, const IncludeLoader& loadInclude)
  -> std::vector<InfiniteLoopIssue>;

} // namespace vertexadt
