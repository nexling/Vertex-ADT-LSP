#include "linked_files.h"

#include "text_util.h"

namespace vertexadt {
namespace {

bool containsToken(std::string_view text, std::string_view token)
{
  return text.find(token) != std::string_view::npos;
}

} // namespace

void LinkedFileStore::put(std::string normalizedPath, std::string text, bool fromBuffer)
{
  const auto lock = std::lock_guard(mutex_);
  snapshots_[normalizedPath] = std::make_shared<const LinkedFileSnapshot>(LinkedFileSnapshot{
    .normalizedPath = normalizedPath,
    .text = std::move(text),
    .fromBuffer = fromBuffer,
  });
}

void LinkedFileStore::drop(std::string_view normalizedPath)
{
  const auto lock = std::lock_guard(mutex_);
  snapshots_.erase(std::string(normalizedPath));
}

void LinkedFileStore::dropAll()
{
  const auto lock = std::lock_guard(mutex_);
  snapshots_.clear();
}

auto LinkedFileStore::get(std::string_view normalizedPath) const
  -> std::shared_ptr<const LinkedFileSnapshot>
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = snapshots_.find(std::string(normalizedPath));
  if (it == snapshots_.end())
  {
    return {};
  }
  return it->second;
}

auto LinkedFileStore::expectedCallParamCount(
  std::string_view normalizedPath,
  std::optional<std::string_view> targetSection,
  const WorkspaceIndex& index) const -> int
{
  auto snapshot = get(normalizedPath);
  const auto named = targetSection && !targetSection->empty();
  if (snapshot && !snapshot->text.empty())
  {
    return expectedCallParamCountFromText(snapshot->text, targetSection);
  }
  if (!named)
  {
    if (auto entry = index.entry(toLowerAscii(normalizedPath)))
    {
      return entry->expectedCallParamCount;
    }
    if (auto entry = index.entryByAbsPath(std::filesystem::path(std::string(normalizedPath))))
    {
      return entry->expectedCallParamCount;
    }
  }
  return 0;
}

bool isCrossFileCapable(std::string_view text)
{
  return containsToken(text, "# include")
    || containsToken(text, "##included_from")
    || containsToken(text, "call(")
    || containsToken(text, "$CALLPARAM")
    || containsToken(text, "@local_slots")
    || containsToken(text, "@declarationnumbercheck")
    || containsToken(text, "common_indexes")
    || containsToken(text, "common_dbases")
    || containsToken(text, "common_points");
}

auto includeFromGraph(std::string_view text) -> std::vector<std::string>
{
  std::vector<std::string> paths;
  constexpr std::string_view kPrefix = "##included_from";
  for (const auto line : splitLines(text))
  {
    const auto trimmed = trim(line);
    if (trimmed.size() >= kPrefix.size() && trimmed.substr(0, kPrefix.size()) == kPrefix)
    {
      paths.emplace_back(trim(trimmed.substr(kPrefix.size())));
    }
  }
  return paths;
}

} // namespace vertexadt
