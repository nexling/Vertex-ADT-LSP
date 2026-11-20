// Stand-in for Vertex_Dbase_Converter.exe in vertex-dbase-converter-test.
// The first word of the database file picks the behaviour:
//   ok        write the XML (argv, cwd, console state) and exit 0
//   fail      print "Error: boom" to stdout, exit 3
//   stderr    print "bad db" to stderr, exit 2
//   slow      write a partial XML, record pid in <db>.pid, then sleep 60 s
//   overlap   record start/end in <db>.trace around a 300 ms sleep, write XML, exit 0
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace fs = std::filesystem;

static std::string u8(std::wstring_view w)
{
  if (w.empty())
  {
    return {};
  }
  const int n = ::WideCharToMultiByte(
    CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<std::size_t>(n), ' ');
  ::WideCharToMultiByte(
    CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
  return out;
}

static long long nowMs()
{
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int wmain(int argc, wchar_t** argv)
{
  if (argc != 6)
  {
    std::cout << "Error: Usage: fake <path> [options]" << std::endl;
    return 1;
  }
  const fs::path database = argv[1];
  const fs::path xml = argv[5];

  std::string mode;
  {
    std::ifstream in(database);
    in >> mode;
  }

  auto writeXml = [&](bool complete) {
    std::ofstream out(xml, std::ios::binary);
    out << "<argv>";
    for (int i = 1; i < argc; ++i)
    {
      out << "[" << u8(argv[i]) << "]";
    }
    out << "</argv>" << std::endl;
    out << "<cwd>" << u8(fs::current_path().native()) << "</cwd>" << std::endl;
    out << "<console>" << (::GetConsoleWindow() ? "visible" : "none") << "</console>" << std::endl;
    if (complete)
    {
      out << "<done/>" << std::endl;
    }
  };

  if (mode == "ok")
  {
    writeXml(true);
    std::cout << "Exported output to x" << std::endl;
    return 0;
  }
  if (mode == "fail")
  {
    std::cout << "Error: boom" << std::endl;
    return 3;
  }
  if (mode == "stderr")
  {
    std::cerr << "bad db" << std::endl;
    return 2;
  }
  if (mode == "slow")
  {
    writeXml(false);
    std::ofstream(fs::path(database).concat(".pid")) << ::GetCurrentProcessId();
    std::this_thread::sleep_for(std::chrono::seconds(60));
    return 0;
  }
  if (mode == "overlap")
  {
    const auto start = nowMs();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto end = nowMs();
    std::ofstream(fs::path(database).concat(".trace")) << start << " " << end;
    writeXml(true);
    return 0;
  }
  std::cout << "Error: unknown mode " << mode << std::endl;
  return 9;
}
