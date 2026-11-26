#include "debug_trace.h"

#include <algorithm>
#include <cstring>

namespace vertexadt {
namespace {

// std::regex `\s` (classic ctype space).
bool isSpace(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

// std::regex `\w`.
bool isWord(char ch)
{
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')
    || ch == '_';
}

bool isDigit(char ch)
{
  return ch >= '0' && ch <= '9';
}

char lowerAscii(char ch)
{
  return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

bool icaseEquals(std::string_view text, std::string_view lowercase)
{
  if (text.size() != lowercase.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    if (lowerAscii(text[i]) != lowercase[i])
    {
      return false;
    }
  }
  return true;
}

bool icaseAt(std::string_view text, std::size_t at, std::size_t end, std::string_view lowercase)
{
  return at + lowercase.size() <= end && icaseEquals(text.substr(at, lowercase.size()), lowercase);
}

struct Range
{
  std::size_t begin = 0;
  std::size_t end = 0;
};

Range trimRange(std::string_view text, std::size_t begin, std::size_t end)
{
  while (begin < end && isSpace(text[begin]))
  {
    ++begin;
  }
  while (end > begin && isSpace(text[end - 1]))
  {
    --end;
  }
  return {begin, end};
}

std::size_t skipSpaces(std::string_view text, std::size_t at, std::size_t end)
{
  while (at < end && isSpace(text[at]))
  {
    ++at;
  }
  return at;
}

std::size_t skipWord(std::string_view text, std::size_t at, std::size_t end)
{
  while (at < end && isWord(text[at]))
  {
    ++at;
  }
  return at;
}

// `\s*(?:_\w+_)?\s*$` from `at` to `end`.
bool valueTailRestMatches(std::string_view line, std::size_t at, std::size_t end)
{
  at = skipSpaces(line, at, end);
  if (at == end)
  {
    return true;
  }
  if (line[at] != '_')
  {
    return false;
  }
  const auto runEnd = skipWord(line, at, end);
  if (runEnd - at < 3 || line[runEnd - 1] != '_')
  {
    return false;
  }
  return skipSpaces(line, runEnd, end) == end;
}

// `(\w+\$?)\s*=` at `at` (bounded by `end`): the name range, or nullopt.
std::optional<Range> assignedNameAt(std::string_view text, std::size_t at, std::size_t end)
{
  const auto wordEnd = skipWord(text, at, end);
  if (wordEnd == at)
  {
    return std::nullopt;
  }
  auto nameEnd = wordEnd;
  if (nameEnd < end && text[nameEnd] == '$')
  {
    ++nameEnd;
  }
  const auto eq = skipSpaces(text, nameEnd, end);
  if (eq >= end || text[eq] != '=')
  {
    return std::nullopt;
  }
  return Range{at, nameEnd};
}

// `^\w+\$?$` over [begin, end).
bool isVarName(std::string_view text, std::size_t begin, std::size_t end)
{
  if (begin >= end)
  {
    return false;
  }
  auto wordEnd = skipWord(text, begin, end);
  if (wordEnd == begin)
  {
    return false;
  }
  if (wordEnd < end && text[wordEnd] == '$')
  {
    ++wordEnd;
  }
  return wordEnd == end;
}

void emit(
  std::vector<DebugAssignmentSpan>& out,
  std::string_view line,
  Range name,
  Range value)
{
  // Later spans for the same name win (unordered_map assignment).
  const auto nameText = line.substr(name.begin, name.end - name.begin);
  for (auto& span : out)
  {
    if (line.substr(span.nameBegin, span.nameEnd - span.nameBegin) == nameText)
    {
      span.valueBegin = value.begin;
      span.valueEnd = value.end;
      return;
    }
  }
  out.push_back(DebugAssignmentSpan{
    .nameBegin = name.begin,
    .nameEnd = name.end,
    .valueBegin = value.begin,
    .valueEnd = value.end,
  });
}

constexpr std::string_view kStartDebugFile = "START OF DEBUG in FILE";
constexpr std::string_view kStartDebugSection = "START OF DEBUG in SECTION";

} // namespace

bool hasDebugHeader(std::string_view text)
{
  const auto head = text.substr(0, std::min(text.size(), kDebugHeaderWindow));
  return head.find(kStartDebugFile) != std::string_view::npos
    || head.find(kStartDebugSection) != std::string_view::npos;
}

bool isDebugTraceDocument(std::string_view path, std::string_view text)
{
  if (path.size() >= 4 && icaseEquals(path.substr(path.size() - 4), ".dbg"))
  {
    return true;
  }
  return hasDebugHeader(text);
}

void parseLineAssignmentSpans(std::string_view lineText, std::vector<DebugAssignmentSpan>& out)
{
  out.clear();
  auto line = lineText;
  if (!line.empty() && line.back() == '\r')
  {
    line.remove_suffix(1);
  }
  const auto n = line.size();
  const auto lastAngle = line.rfind('<');
  if (lastAngle == std::string_view::npos)
  {
    return;
  }

  // ^<(.+?)>\s*(?:_\w+_)?\s*$ — `.` stops at CR/LF.
  std::optional<std::size_t> close;
  for (auto p = lastAngle + 1; p < n; ++p)
  {
    const auto ch = line[p];
    if (ch == '\r' || ch == '\n')
    {
      break;
    }
    if (ch == '>' && p >= lastAngle + 2 && valueTailRestMatches(line, p + 1, n))
    {
      close = p;
      break;
    }
  }
  if (!close)
  {
    return;
  }

  const auto value = trimRange(line, lastAngle + 1, *close);
  if (value.begin == value.end)
  {
    return;
  }
  // ^\d+/ — a loop counter, not a value.
  {
    auto p = value.begin;
    while (p < value.end && isDigit(line[p]))
    {
      ++p;
    }
    if (p > value.begin && p < value.end && line[p] == '/')
    {
      return;
    }
  }

  const auto code = trimRange(line, 0, lastAngle);

  // ^\s*(\w+\$?)\s*=\s*
  if (const auto name = assignedNameAt(line, code.begin, code.end))
  {
    emit(out, line, *name, value);
    return;
  }

  // ^(?:else\s+)?if\b (icase)
  const auto ifAt = [&](std::size_t at) {
    return icaseAt(line, at, code.end, "if")
      && (at + 2 == code.end || !isWord(line[at + 2]));
  };
  bool ifHeader = ifAt(code.begin);
  if (!ifHeader && icaseAt(line, code.begin, code.end, "else"))
  {
    const auto afterElse = code.begin + 4;
    const auto ifStart = skipSpaces(line, afterElse, code.end);
    ifHeader = ifStart > afterElse && ifAt(ifStart);
  }
  if (ifHeader)
  {
    // \bthen\s+(\w+\$?)\s*=\s* (icase), last match.
    std::optional<Range> thenName;
    for (auto i = code.begin; i + 4 <= code.end; ++i)
    {
      if (!icaseAt(line, i, code.end, "then") || (i > code.begin && isWord(line[i - 1])))
      {
        continue;
      }
      const auto nameStart = skipSpaces(line, i + 4, code.end);
      if (nameStart == i + 4)
      {
        continue;
      }
      if (const auto name = assignedNameAt(line, nameStart, code.end))
      {
        thenName = name;
      }
    }
    if (thenName)
    {
      const auto raw = line.substr(value.begin, value.end - value.begin);
      if (icaseEquals(raw, "true") || icaseEquals(raw, "false"))
      {
        return;
      }
      emit(out, line, *thenName, value);
      return;
    }
  }

  // ^\s*(\w+)\((\w+\$?)\)\s*(.*) (whole codePart)
  const auto funcEnd = skipWord(line, code.begin, code.end);
  if (funcEnd == code.begin || funcEnd >= code.end || line[funcEnd] != '(')
  {
    return;
  }
  const auto argBegin = funcEnd + 1;
  auto argEnd = skipWord(line, argBegin, code.end);
  if (argEnd == argBegin)
  {
    return;
  }
  if (argEnd < code.end && line[argEnd] == '$')
  {
    ++argEnd;
  }
  if (argEnd >= code.end || line[argEnd] != ')')
  {
    return;
  }
  for (auto p = argEnd + 1; p < code.end; ++p)
  {
    if (line[p] == '\r' || line[p] == '\n')
    {
      return;
    }
  }
  const Range parenArg{argBegin, argEnd};
  const auto rest = trimRange(line, argEnd + 1, code.end);
  if (rest.begin < rest.end)
  {
    // splitComma(restRaw), every part ^\w+\$?$.
    std::vector<Range> parts;
    bool allNames = true;
    for (auto start = rest.begin;;)
    {
      auto comma = start;
      while (comma < rest.end && line[comma] != ',')
      {
        ++comma;
      }
      const auto part = trimRange(line, start, comma);
      if (!isVarName(line, part.begin, part.end))
      {
        allNames = false;
        break;
      }
      parts.push_back(part);
      if (comma >= rest.end)
      {
        break;
      }
      start = comma + 1;
    }
    if (allNames && parts.size() > 1)
    {
      // splitValueGroups(rawValue): " , " separated, trimmed.
      std::vector<Range> groups;
      constexpr std::string_view kSep = " , ";
      const auto raw = line.substr(value.begin, value.end - value.begin);
      std::size_t start = 0;
      while (true)
      {
        const auto found = raw.find(kSep, start);
        const auto stop = found == std::string_view::npos ? raw.size() : found;
        groups.push_back(trimRange(line, value.begin + start, value.begin + stop));
        if (found == std::string_view::npos)
        {
          break;
        }
        start = found + kSep.size();
      }
      if (groups.size() == parts.size())
      {
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
          emit(out, line, parts[i], groups[i]);
        }
        return;
      }
    }
  }
  emit(out, line, parenArg, value);
}

auto splitDebugLine(std::string_view lineText) -> DebugLineParts
{
  auto line = lineText;
  if (!line.empty() && line.back() == '\r')
  {
    line.remove_suffix(1);
  }
  DebugLineParts parts;
  const auto whole = trimRange(line, 0, line.size());
  parts.codeBegin = whole.begin;
  parts.codeEnd = whole.end;
  if (whole.begin == whole.end)
  {
    return parts;
  }

  // Real dumps open with a BOM and a stray 0x7F before ` START OF DEBUG`.
  auto bannerBegin = whole.begin;
  if (line.substr(bannerBegin).starts_with("\xEF\xBB\xBF"))
  {
    bannerBegin += 3;
  }
  while (bannerBegin < whole.end
    && (isSpace(line[bannerBegin]) || static_cast<unsigned char>(line[bannerBegin]) < 0x20
      || line[bannerBegin] == '\x7F'))
  {
    ++bannerBegin;
  }
  const auto banner = line.substr(bannerBegin, whole.end - bannerBegin);
  if (banner.starts_with("START OF DEBUG") || banner.starts_with("END OF DEBUG")
    || banner.starts_with("END of FILE"))
  {
    parts.kind = DebugLineParts::Kind::Structural;
    parts.codeBegin = bannerBegin;
  }

  auto end = whole.end;
  // Optional trailing `_word_` marker (only kept when a value precedes it).
  std::size_t markerBegin = end;
  if (line[end - 1] == '_')
  {
    auto m = end;
    while (m > whole.begin && isWord(line[m - 1]))
    {
      --m;
    }
    if (end - m >= 3 && line[m] == '_' && m > whole.begin && isSpace(line[m - 1]))
    {
      markerBegin = m;
    }
  }

  const auto findValue = [&](std::size_t stop) -> std::optional<Range> {
    const auto valueEnd = trimRange(line, whole.begin, stop).end;
    if (valueEnd <= whole.begin || line[valueEnd - 1] != '>')
    {
      return std::nullopt;
    }
    for (auto p = valueEnd - 1; p > whole.begin;)
    {
      --p;
      if (line[p] == '<' && p > whole.begin && isSpace(line[p - 1]))
      {
        return Range{p, valueEnd};
      }
    }
    return std::nullopt;
  };

  auto value = findValue(markerBegin);
  if (value && markerBegin < end)
  {
    parts.markerBegin = markerBegin;
    parts.markerEnd = end;
  }
  else if (!value && markerBegin < end)
  {
    value = findValue(end);
  }
  if (value)
  {
    parts.valueBegin = value->begin;
    parts.valueEnd = value->end;
    parts.codeEnd = trimRange(line, whole.begin, value->begin).end;
  }
  return parts;
}

DebugTrace::DebugTrace(std::shared_ptr<const std::string> text, int version)
  : text_(std::move(text))
  , version_(version)
{
  const std::string_view view = *text_;
  lineStarts_.reserve(view.size() / 64 + 1);
  lineStarts_.push_back(0);
  const char* const base = view.data();
  const char* p = base;
  const char* const last = base + view.size();
  while (p < last)
  {
    const auto* nl = static_cast<const char*>(std::memchr(p, '\n', static_cast<std::size_t>(last - p)));
    if (!nl)
    {
      break;
    }
    lineStarts_.push_back(static_cast<std::uint32_t>(nl + 1 - base));
    p = nl + 1;
  }
}

auto DebugTrace::line(int i) const -> std::string_view
{
  if (i < 0 || i >= lineCount())
  {
    return {};
  }
  const std::string_view view = *text_;
  const auto begin = static_cast<std::size_t>(lineStarts_[static_cast<std::size_t>(i)]);
  const auto next = static_cast<std::size_t>(i + 1) < lineStarts_.size()
    ? static_cast<std::size_t>(lineStarts_[static_cast<std::size_t>(i + 1)]) - 1
    : view.size();
  return view.substr(begin, next - begin);
}

auto DebugTrace::index() const -> std::shared_ptr<const DebugAssignmentIndex>
{
  const auto lock = std::lock_guard(indexMutex_);
  return index_;
}

auto DebugTrace::waitIndex() const -> std::shared_ptr<const DebugAssignmentIndex>
{
  auto lock = std::unique_lock(indexMutex_);
  indexReady_.wait(lock, [this]() { return indexSettled_; });
  return index_;
}

void DebugTrace::setIndex(std::shared_ptr<const DebugAssignmentIndex> index)
{
  {
    const auto lock = std::lock_guard(indexMutex_);
    if (indexSettled_)
    {
      return;
    }
    index_ = std::move(index);
    indexSettled_ = true;
  }
  indexReady_.notify_all();
}

auto DebugTrace::mostRecentAssignment(int hoverLine, std::string_view name) const
  -> std::optional<DebugAssignment>
{
  const auto index = waitIndex();
  if (!index || hoverLine < 0)
  {
    return std::nullopt;
  }
  const auto it = index->byName.find(std::string(name));
  if (it == index->byName.end())
  {
    return std::nullopt;
  }
  const auto& entries = it->second;
  const auto after = std::upper_bound(
    entries.begin(),
    entries.end(),
    hoverLine,
    [](int line, const DebugAssignmentEntry& entry) { return line < entry.line; });
  if (after == entries.begin())
  {
    return std::nullopt;
  }
  const auto& entry = *(after - 1);
  return DebugAssignment{
    .value = std::string(text().substr(entry.valueOffset, entry.valueLength)),
    .line = entry.line,
  };
}

auto buildDebugAssignmentIndex(const DebugTrace& trace, const std::function<bool()>& canceled)
  -> std::shared_ptr<const DebugAssignmentIndex>
{
  auto index = std::make_shared<DebugAssignmentIndex>();
  std::vector<DebugAssignmentSpan> spans;
  const auto base = trace.text().data();
  const auto count = trace.lineCount();
  std::string key;
  for (int i = 0; i < count; ++i)
  {
    if ((i & 0xFFF) == 0 && canceled && canceled())
    {
      return nullptr;
    }
    const auto line = trace.line(i);
    // Cheap reject: no `<` means no value, so no assignment.
    if (line.find('<') == std::string_view::npos)
    {
      continue;
    }
    parseLineAssignmentSpans(line, spans);
    const auto lineOffset = static_cast<std::size_t>(line.data() - base);
    for (const auto& span : spans)
    {
      key.assign(line.substr(span.nameBegin, span.nameEnd - span.nameBegin));
      index->byName[key].push_back(DebugAssignmentEntry{
        .line = i,
        .valueOffset = static_cast<std::uint32_t>(lineOffset + span.valueBegin),
        .valueLength = static_cast<std::uint32_t>(span.valueEnd - span.valueBegin),
      });
    }
  }
  return index;
}

DebugTraceStore::DebugTraceStore(std::function<void()> onIndexReady)
  : onIndexReady_(std::move(onIndexReady))
  , worker_([this]() { workerLoop(); })
{
}

DebugTraceStore::~DebugTraceStore()
{
  {
    const auto lock = std::lock_guard(mutex_);
    stop_ = true;
    for (auto& [_, entry] : entries_)
    {
      entry.canceled->store(true);
    }
  }
  cv_.notify_all();
  if (worker_.joinable())
  {
    worker_.join();
  }
  for (auto& job : jobs_)
  {
    job.trace->setIndex(nullptr);
  }
}

void DebugTraceStore::update(
  const std::string& uri,
  int version,
  std::shared_ptr<const std::string> text)
{
  {
    const auto lock = std::lock_guard(mutex_);
    if (const auto it = entries_.find(uri);
        it != entries_.end() && it->second.trace->version() == version
        && it->second.trace->text().data() == text->data())
    {
      return;
    }
  }
  auto trace = std::make_shared<DebugTrace>(std::move(text), version);
  auto canceled = std::make_shared<std::atomic<bool>>(false);
  {
    const auto lock = std::lock_guard(mutex_);
    if (const auto it = entries_.find(uri); it != entries_.end())
    {
      it->second.canceled->store(true);
    }
    entries_.insert_or_assign(uri, Entry{.trace = trace, .canceled = canceled});
    jobs_.push_back(Job{.uri = uri, .trace = trace, .canceled = canceled});
  }
  cv_.notify_one();
}

void DebugTraceStore::erase(const std::string& uri)
{
  const auto lock = std::lock_guard(mutex_);
  if (const auto it = entries_.find(uri); it != entries_.end())
  {
    it->second.canceled->store(true);
    entries_.erase(it);
  }
}

auto DebugTraceStore::get(const std::string& uri) const -> std::shared_ptr<const DebugTrace>
{
  const auto lock = std::lock_guard(mutex_);
  const auto it = entries_.find(uri);
  return it == entries_.end() ? nullptr : it->second.trace;
}

void DebugTraceStore::workerLoop()
{
  while (true)
  {
    Job job;
    {
      auto lock = std::unique_lock(mutex_);
      cv_.wait(lock, [this]() { return stop_ || !jobs_.empty(); });
      if (stop_)
      {
        return;
      }
      job = std::move(jobs_.front());
      jobs_.pop_front();
    }
    if (job.canceled->load())
    {
      job.trace->setIndex(nullptr);
      continue;
    }
    auto index = buildDebugAssignmentIndex(
      *job.trace, [&job]() { return job.canceled->load(); });
    job.trace->setIndex(index);
    if (index && !job.canceled->load() && onIndexReady_)
    {
      onIndexReady_();
    }
  }
}

} // namespace vertexadt
