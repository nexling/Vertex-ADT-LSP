// Spawn plumbing for the Vertex Dbase Converter helper.
// Build: cmake --build build --config Release --target vertex-dbase-converter-test
// Run:   build\Release\vertex-dbase-converter-test.exe
#include "vertex_dbase_converter.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cctype>
#include <functional>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using vertexadt::HelperExportRequest;
using vertexadt::HelperExportResult;
using namespace std::chrono_literals;

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
  std::cout << (ok ? "  ok   " : "  FAIL ") << what << '\n';
  if (!ok)
  {
    ++failures;
  }
}

std::string readFile(const fs::path& path)
{
  std::ifstream in(path, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void writeFile(const fs::path& path, std::string_view text)
{
  std::ofstream(path, std::ios::binary) << text;
}

std::string utf8(const fs::path& path)
{
  auto u8 = path.u8string();
  return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

// file:///C:/a%20b/… with forward slashes, like the host's extensionUri.
std::string fileUri(const fs::path& path)
{
  std::string out = "file:///";
  for (unsigned char c : utf8(fs::absolute(path).generic_u8string()))
  {
    if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~')
    {
      out.push_back(static_cast<char>(c));
    }
    else if (c == ':' && out.size() == 9)
    {
      out += "%3A";
    }
    else
    {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

struct Logs
{
  std::mutex mutex;
  std::vector<std::string> lines;
  std::function<void(std::string_view)> sink()
  {
    return [this](std::string_view line) {
      std::lock_guard lock(mutex);
      lines.emplace_back(line);
    };
  }
  bool contains(std::string_view needle)
  {
    std::lock_guard lock(mutex);
    for (const auto& line : lines)
    {
      if (line.find(needle) != std::string::npos)
      {
        return true;
      }
    }
    return false;
  }
};

auto cancelAfter(std::chrono::milliseconds delay)
{
  auto start = std::chrono::steady_clock::now();
  return [start, delay] { return std::chrono::steady_clock::now() - start >= delay; };
}

bool processGone(DWORD pid, std::chrono::milliseconds within)
{
  HANDLE process = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (!process)
  {
    return true;
  }
  const bool gone = ::WaitForSingleObject(process, static_cast<DWORD>(within.count())) == WAIT_OBJECT_0;
  ::CloseHandle(process);
  return gone;
}

DWORD waitForPid(const fs::path& pidFile, std::chrono::milliseconds within)
{
  const auto deadline = std::chrono::steady_clock::now() + within;
  while (std::chrono::steady_clock::now() < deadline)
  {
    DWORD pid = 0;
    std::ifstream(pidFile) >> pid;
    if (pid)
    {
      return pid;
    }
    std::this_thread::sleep_for(20ms);
  }
  return 0;
}

// Child mode: start a slow export, then exit without cleanup.
int orphanMode(const std::string& extensionUri, const fs::path& database)
{
  std::thread([=] {
    (void)vertexadt::exportDatabaseXml({
      .extensionUri = extensionUri,
      .databasePath = database,
      .xmlPath = fs::path(database).concat(".xml"),
      .isCanceled = {},
      .log = [](std::string_view) {},
    });
  }).detach();
  waitForPid(fs::path(database).concat(".pid"), 5s);
  ::ExitProcess(0);
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
  if (argc == 4 && std::wstring_view(argv[1]) == L"--orphan")
  {
    return orphanMode(utf8(fs::path(argv[2])), argv[3]);
  }

  const fs::path root = fs::temp_directory_path() / L"vertex-adt dbase converter test";
  std::error_code ec;
  fs::remove_all(root, ec);
  const fs::path ext = root / L"ext \u00fc dir";
  const fs::path data = root / L"data dir";
  const fs::path empty = root / L"no helper";
  fs::create_directories(ext);
  fs::create_directories(data);
  fs::create_directories(empty);
  fs::copy_file(FAKE_CONVERTER_PATH, ext / vertexadt::kVertexDbaseConverterFileName);
  const auto extUri = fileUri(ext);

  auto database = [&](const std::wstring& name, std::string_view mode) {
    auto path = data / name;
    writeFile(path, mode);
    return path;
  };
  auto run = [&](const fs::path& db, Logs& logs, std::function<bool()> isCanceled = {},
               std::string uri = {}) {
    return vertexadt::exportDatabaseXml({
      .extensionUri = uri.empty() ? extUri : uri,
      .databasePath = db,
      .xmlPath = fs::path(db).concat(".xml"),
      .isCanceled = std::move(isCanceled),
      .log = logs.sink(),
    });
  };

  std::cout << "lookup\n";
  {
    auto path = vertexadt::vertexDbaseConverterPath(extUri);
    check(path && fs::equivalent(*path, ext / vertexadt::kVertexDbaseConverterFileName),
      "extensionUri (%20, UTF-8) joins to the helper at the extension root");
    check(!vertexadt::vertexDbaseConverterPath(""), "empty extensionUri is missing");
    check(!vertexadt::vertexDbaseConverterPath("not a uri"), "invalid extensionUri is missing");

    Logs logs;
    auto db = database(L"d_MISSING", "ok");
    check(vertexadt::exportDatabaseXml({.databasePath = db, .xmlPath = fs::path(db).concat(".xml"),
            .log = logs.sink()})
            == HelperExportResult::HelperMissing,
      "no extensionUri -> HelperMissing");
    check(run(db, logs, {}, fileUri(empty)) == HelperExportResult::HelperMissing,
      "no helper file -> HelperMissing");
    check(logs.contains("Vertex database converter not found at "), "missing helper is logged");
    check(!fs::exists(fs::path(db).concat(".xml")), "missing helper writes nothing");
  }

  std::cout << "spawn\n";
  {
    Logs logs;
    auto db = database(L"d_OK", "ok");
    check(run(db, logs) == HelperExportResult::Exported, "exit 0 -> Exported");
    const auto xml = readFile(fs::path(db).concat(".xml"));
    const auto expectedArgv = "<argv>[" + utf8(db) + "][--all][--xml][--export]["
      + utf8(fs::path(db).concat(".xml")) + "]</argv>";
    check(xml.find(expectedArgv) != std::string::npos,
      "argv is [databasePath, --all, --xml, --export, xmlPath]");
    check(xml.find("<console>none</console>") != std::string::npos, "hidden: no console window");
    check(logs.lines.empty(), "success logs nothing");

    check(xml.find("<cwd>" + utf8(ext) + "</cwd>") != std::string::npos,
      "cwd is the helper's directory");
  }

  std::cout << "failure\n";
  {
    Logs logs;
    auto db = database(L"d_FAIL", "fail");
    check(run(db, logs) == HelperExportResult::Failed, "non-zero exit -> Failed");
    check(logs.contains("Error exporting macro XML for "), "failure names the database");
    check(logs.contains("exit 3: Error: boom"), "stdout captured into the log");

    Logs errLogs;
    auto errDb = database(L"d_STDERR", "stderr");
    check(run(errDb, errLogs) == HelperExportResult::Failed, "stderr exit -> Failed");
    check(errLogs.contains("exit 2: bad db"), "stderr captured into the log");
  }

  std::cout << "cancel\n";
  {
    Logs logs;
    auto db = database(L"d_SLOW", "slow");
    const auto start = std::chrono::steady_clock::now();
    const auto result = run(db, logs, cancelAfter(400ms));
    const auto took = std::chrono::steady_clock::now() - start;
    check(result == HelperExportResult::Canceled, "cancel while running -> Canceled");
    check(took < 3s, "cancel returns promptly");
    const auto pid = waitForPid(fs::path(db).concat(".pid"), 1s);
    check(pid && processGone(pid, 2s), "cancel kills the child");
    check(!fs::exists(fs::path(db).concat(".xml")), "killed child's partial sidecar is deleted");
  }

  std::cout << "one child at a time\n";
  {
    Logs logs;
    auto a = database(L"d_A", "overlap");
    auto b = database(L"d_B", "overlap");
    HelperExportResult ra{}, rb{};
    std::thread ta([&] { ra = run(a, logs); });
    std::thread tb([&] { rb = run(b, logs); });
    ta.join();
    tb.join();
    check(ra == HelperExportResult::Exported && rb == HelperExportResult::Exported,
      "both waiting exports complete");
    long long as = 0, ae = 0, bs = 0, be = 0;
    std::ifstream(fs::path(a).concat(".trace")) >> as >> ae;
    std::ifstream(fs::path(b).concat(".trace")) >> bs >> be;
    check(as && bs && (ae <= bs || be <= as), "children never overlap");

    Logs waitLogs;
    auto slow = database(L"d_HOLD", "slow");
    auto waiter = database(L"d_WAITER", "ok");
    std::thread holder([&] { (void)run(slow, waitLogs, cancelAfter(1500ms)); });
    waitForPid(fs::path(slow).concat(".pid"), 2s);
    const auto result = run(waiter, waitLogs, cancelAfter(200ms));
    holder.join();
    check(result == HelperExportResult::Canceled, "cancel while waiting for the slot -> Canceled");
    check(!fs::exists(fs::path(waiter).concat(".xml")), "a canceled waiter never spawns");
  }

  std::cout << "dies with the language server\n";
  {
    auto db = database(L"d_ORPHAN", "slow");
    wchar_t self[MAX_PATH];
    ::GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring commandLine = L"\"" + std::wstring(self) + L"\" --orphan \""
      + fs::path(extUri).wstring() + L"\" \"" + db.wstring() + L"\"";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    const bool started = ::CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE,
      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    check(started, "orphan driver starts");
    if (started)
    {
      ::WaitForSingleObject(pi.hProcess, 10000);
      ::CloseHandle(pi.hThread);
      ::CloseHandle(pi.hProcess);
      const auto pid = waitForPid(fs::path(db).concat(".pid"), 1s);
      check(pid && processGone(pid, 3s), "helper child is killed when its parent exits");
    }
  }

  std::cout << "real helper\n";
  {
    const fs::path repo = VERTEX_ADT_REPO_ROOT;
    const fs::path fixture = repo / "test-fixtures/vertex-dbase-converter/custom/complibs/macro_sys";
    if (!fs::exists(repo / vertexadt::kVertexDbaseConverterFileName) || !fs::exists(fixture))
    {
      std::cout << "  skip (no repo-root helper or fixture)\n";
    }
    else
    {
      const fs::path lib = root / "real" / "macro_sys";
      fs::create_directories(lib);
      for (const auto* name : {"d_MACRO_SYS", "d_MACRO_SYSc", "d_MACRO_SYSl"})
      {
        fs::copy_file(fixture / name, lib / name);
      }
      Logs logs;
      const auto db = lib / "d_MACRO_SYS";
      check(run(db, logs, {}, fileUri(repo)) == HelperExportResult::Exported,
        "real helper exports the fixture database");
      const auto xml = readFile(fs::path(db).concat(".xml"));
      check(xml.rfind("<?xml", 0) == 0 && xml.find("<Table Name=\"MACRO_SYS\"") != std::string::npos,
        "real helper writes the sidecar XML");

      Logs badLogs;
      check(run(lib / "d_NOPE", badLogs, {}, fileUri(repo)) == HelperExportResult::Failed,
        "real helper on a missing database -> Failed");
      check(badLogs.contains("exit 1: Error: Path does not exist"), "real helper error is logged");
    }
  }

  fs::remove_all(root, ec);
  std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("all passed")) << '\n';
  return failures ? 1 : 0;
}
