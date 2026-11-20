#include "vertex_dbase_converter.h"

#include <lsp/uri.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

namespace vertexadt {
namespace {

constexpr auto kPollInterval = std::chrono::milliseconds(50);

// One helper child at a time, process-wide.
std::timed_mutex& helperSlot()
{
  static std::timed_mutex slot;
  return slot;
}

class Handle
{
public:
  Handle() = default;
  explicit Handle(HANDLE h) : h_(h) {}
  ~Handle() { reset(); }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  Handle(Handle&& other) noexcept : h_(other.release()) {}
  Handle& operator=(Handle&& other) noexcept
  {
    if (this != &other)
    {
      reset(other.release());
    }
    return *this;
  }

  [[nodiscard]] HANDLE get() const { return h_; }
  [[nodiscard]] explicit operator bool() const { return h_ && h_ != INVALID_HANDLE_VALUE; }
  HANDLE release()
  {
    auto h = h_;
    h_ = nullptr;
    return h;
  }
  void reset(HANDLE h = nullptr)
  {
    if (*this)
    {
      ::CloseHandle(h_);
    }
    h_ = h;
  }

private:
  HANDLE h_ = nullptr;
};

// Kill-on-close Job object; never closed, so the kernel closes it (and kills
// any helper child) when the language server exits or crashes.
HANDLE helperJob()
{
  static const HANDLE job = [] {
    HANDLE created = ::CreateJobObjectW(nullptr, nullptr);
    if (created)
    {
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
      info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      if (!::SetInformationJobObject(created, JobObjectExtendedLimitInformation, &info, sizeof(info)))
      {
        ::CloseHandle(created);
        created = nullptr;
      }
    }
    return created;
  }();
  return job;
}

std::string utf8(const std::filesystem::path& path)
{
  const auto u8 = path.u8string();
  return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

std::string lastErrorText(DWORD code)
{
  return std::system_category().message(static_cast<int>(code));
}

// MSVCRT / CommandLineToArgvW quoting.
void appendQuotedArgument(std::wstring& commandLine, std::wstring_view arg)
{
  if (!commandLine.empty())
  {
    commandLine.push_back(L' ');
  }
  if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring_view::npos)
  {
    commandLine.append(arg);
    return;
  }
  commandLine.push_back(L'"');
  for (std::size_t i = 0;; ++i)
  {
    std::size_t backslashes = 0;
    while (i < arg.size() && arg[i] == L'\\')
    {
      ++i;
      ++backslashes;
    }
    if (i == arg.size())
    {
      commandLine.append(backslashes * 2, L'\\');
      break;
    }
    if (arg[i] == L'"')
    {
      commandLine.append(backslashes * 2 + 1, L'\\');
    }
    else
    {
      commandLine.append(backslashes, L'\\');
    }
    commandLine.push_back(arg[i]);
  }
  commandLine.push_back(L'"');
}

std::string trimmed(std::string text)
{
  const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!text.empty() && isSpace(text.back()))
  {
    text.pop_back();
  }
  std::size_t start = 0;
  while (start < text.size() && isSpace(text[start]))
  {
    ++start;
  }
  return text.substr(start);
}

// Non-blocking: reads only what is already in the pipe, so a grandchild that
// keeps the write end open cannot hang the worker.
void drainAvailable(HANDLE pipe, std::string& output)
{
  char buffer[4096];
  for (;;)
  {
    DWORD available = 0;
    if (!::PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available == 0)
    {
      return;
    }
    DWORD read = 0;
    const auto want = available < sizeof(buffer) ? available : static_cast<DWORD>(sizeof(buffer));
    if (!::ReadFile(pipe, buffer, want, &read, nullptr) || read == 0)
    {
      return;
    }
    output.append(buffer, read);
  }
}

bool canceled(const HelperExportRequest& request)
{
  return request.isCanceled && request.isCanceled();
}

} // namespace

auto vertexDbaseConverterPath(std::string_view extensionUri) -> std::optional<std::filesystem::path>
{
  if (extensionUri.empty())
  {
    return std::nullopt;
  }
  const auto uri = lsp::Uri::parse(extensionUri);
  if (!uri.isValid() || uri.scheme() != lsp::Uri::FileScheme)
  {
    return std::nullopt;
  }
  const auto fsPath = uri.fsPath();
  if (fsPath.empty())
  {
    return std::nullopt;
  }
  auto root = std::filesystem::path(
    std::u8string(reinterpret_cast<const char8_t*>(fsPath.data()), fsPath.size()));
  return root / kVertexDbaseConverterFileName;
}

auto exportDatabaseXml(const HelperExportRequest& request) -> HelperExportResult
{
  const auto log = [&](std::string_view message) {
    if (request.log)
    {
      request.log(message);
    }
    else
    {
      std::cerr << message << std::endl;
    }
  };

  const auto helper = vertexDbaseConverterPath(request.extensionUri);
  std::error_code ec;
  if (!helper || !std::filesystem::is_regular_file(*helper, ec))
  {
    log("Vertex database converter not found at "
      + (helper ? utf8(*helper) : std::string("<no extensionUri>")));
    return HelperExportResult::HelperMissing;
  }

  // Wait for the one helper slot, polling cancel.
  std::unique_lock slot(helperSlot(), std::defer_lock);
  while (!slot.try_lock_for(kPollInterval))
  {
    if (canceled(request))
    {
      return HelperExportResult::Canceled;
    }
  }
  if (canceled(request))
  {
    return HelperExportResult::Canceled;
  }

  const auto failed = [&](std::string_view details) {
    log("Error exporting macro XML for " + utf8(request.databasePath) + ": " + std::string(details));
    return HelperExportResult::Failed;
  };

  // One pipe for stdout+stderr; only it and NUL stdin are inherited.
  SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE rawRead = nullptr;
  HANDLE rawWrite = nullptr;
  if (!::CreatePipe(&rawRead, &rawWrite, &inheritable, 64 * 1024))
  {
    return failed("CreatePipe: " + lastErrorText(::GetLastError()));
  }
  Handle outputRead(rawRead);
  Handle outputWrite(rawWrite);
  ::SetHandleInformation(outputRead.get(), HANDLE_FLAG_INHERIT, 0);

  Handle nulInput(::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
    &inheritable, OPEN_EXISTING, 0, nullptr));
  if (!nulInput)
  {
    return failed("open NUL: " + lastErrorText(::GetLastError()));
  }

  HANDLE inherited[] = {outputWrite.get(), nulInput.get()};
  SIZE_T attributeBytes = 0;
  ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
  std::vector<unsigned char> attributeStorage(attributeBytes);
  auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
  if (!::InitializeProcThreadAttributeList(attributes, 1, 0, &attributeBytes))
  {
    return failed("InitializeProcThreadAttributeList: " + lastErrorText(::GetLastError()));
  }
  struct AttributeListGuard
  {
    LPPROC_THREAD_ATTRIBUTE_LIST list;
    ~AttributeListGuard() { ::DeleteProcThreadAttributeList(list); }
  } attributeGuard{attributes};
  if (!::UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
        sizeof(inherited), nullptr, nullptr))
  {
    return failed("UpdateProcThreadAttribute: " + lastErrorText(::GetLastError()));
  }

  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  startup.StartupInfo.wShowWindow = SW_HIDE;
  startup.StartupInfo.hStdInput = nulInput.get();
  startup.StartupInfo.hStdOutput = outputWrite.get();
  startup.StartupInfo.hStdError = outputWrite.get();
  startup.lpAttributeList = attributes;

  std::wstring commandLine;
  appendQuotedArgument(commandLine, helper->native());
  appendQuotedArgument(commandLine, request.databasePath.native());
  appendQuotedArgument(commandLine, L"--all");
  appendQuotedArgument(commandLine, L"--xml");
  appendQuotedArgument(commandLine, L"--export");
  appendQuotedArgument(commandLine, request.xmlPath.native());

  const auto workingDirectory = helper->parent_path().native();
  PROCESS_INFORMATION process{};
  if (!::CreateProcessW(helper->c_str(), commandLine.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        nullptr, workingDirectory.c_str(), &startup.StartupInfo, &process))
  {
    return failed("spawn: " + lastErrorText(::GetLastError()));
  }
  Handle child(process.hProcess);
  Handle childThread(process.hThread);
  outputWrite.reset();
  nulInput.reset();

  if (auto* job = helperJob(); !job || !::AssignProcessToJobObject(job, child.get()))
  {
    // Still run it; only the die-with-the-server guarantee is lost.
    log("Vertex database converter: could not assign child to job: "
      + lastErrorText(::GetLastError()));
  }
  ::ResumeThread(childThread.get());
  childThread.reset();

  std::string output;
  bool killed = false;
  for (;;)
  {
    drainAvailable(outputRead.get(), output);
    if (::WaitForSingleObject(child.get(), static_cast<DWORD>(kPollInterval.count())) != WAIT_TIMEOUT)
    {
      break;
    }
    if (canceled(request))
    {
      ::TerminateProcess(child.get(), 1);
      ::WaitForSingleObject(child.get(), INFINITE);
      killed = true;
      break;
    }
  }
  // Whatever the child wrote before exiting is still buffered in the pipe.
  drainAvailable(outputRead.get(), output);

  if (killed)
  {
    // Written straight to the sidecar: a killed child may leave it truncated
    // yet newer than the database, which would look fresh next time.
    std::filesystem::remove(request.xmlPath, ec);
    return HelperExportResult::Canceled;
  }

  DWORD exitCode = 0;
  ::GetExitCodeProcess(child.get(), &exitCode);
  if (exitCode != 0)
  {
    return failed("exit " + std::to_string(exitCode) + ": " + trimmed(std::move(output)));
  }
  return HelperExportResult::Exported;
}

} // namespace vertexadt
