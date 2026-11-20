#include "macro_library_xml.h"

#include "utf16_text.h"
#include "vertex_dbase_converter.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <system_error>

// Ports of server.js resolveMacroLibraryPath, isPrimaryMacroDatabaseFile,
// ensureMacroLibraryXmlExports and the Record/Cell walk of getMacroRecordFields.
// The helper spawn itself is vertex_dbase_converter.

namespace vertexadt {
namespace {

void logLine(const std::function<void(std::string_view)>& log, std::string_view message)
{
  if (log)
  {
    log(message);
  }
  else
  {
    std::cerr << message << std::endl;
  }
}

auto u8Path(std::string_view text) -> std::filesystem::path
{
  return std::filesystem::path(
    std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

auto pathUtf8(const std::filesystem::path& path) -> std::string
{
  const auto text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

bool endsWithCompanionLetter(std::string_view name)
{
  if (name.empty())
  {
    return false;
  }
  const auto last = name.back();
  return last == 'c' || last == 'C' || last == 'l' || last == 'L';
}

// --- XML ---------------------------------------------------------------------

void appendCodePoint(std::string& out, std::uint32_t cp)
{
  if (cp < 0x80)
  {
    out.push_back(static_cast<char>(cp));
  }
  else if (cp < 0x800)
  {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
  else if (cp < 0x10000)
  {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
  else
  {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// The five predefined entities and numeric references; anything else is kept.
auto decodeEntities(std::string_view text) -> std::string
{
  std::string out;
  out.reserve(text.size());
  std::size_t i = 0;
  while (i < text.size())
  {
    if (text[i] != '&')
    {
      out.push_back(text[i++]);
      continue;
    }
    const auto semi = text.find(';', i + 1);
    if (semi == std::string_view::npos || semi - i > 12)
    {
      out.push_back(text[i++]);
      continue;
    }
    const auto entity = text.substr(i + 1, semi - i - 1);
    bool decoded = true;
    if (entity == "lt") { out.push_back('<'); }
    else if (entity == "gt") { out.push_back('>'); }
    else if (entity == "amp") { out.push_back('&'); }
    else if (entity == "quot") { out.push_back('"'); }
    else if (entity == "apos") { out.push_back('\''); }
    else if (entity.size() > 1 && entity[0] == '#')
    {
      const bool hex = entity[1] == 'x' || entity[1] == 'X';
      const auto digits = entity.substr(hex ? 2 : 1);
      std::uint32_t cp = 0;
      bool ok = !digits.empty();
      for (const auto ch : digits)
      {
        int value = -1;
        if (ch >= '0' && ch <= '9') { value = ch - '0'; }
        else if (hex && ch >= 'a' && ch <= 'f') { value = ch - 'a' + 10; }
        else if (hex && ch >= 'A' && ch <= 'F') { value = ch - 'A' + 10; }
        if (value < 0 || cp > 0x10FFFF)
        {
          ok = false;
          break;
        }
        cp = cp * (hex ? 16 : 10) + static_cast<std::uint32_t>(value);
      }
      if (ok && cp <= 0x10FFFF)
      {
        appendCodePoint(out, cp);
      }
      else
      {
        decoded = false;
      }
    }
    else
    {
      decoded = false;
    }
    if (decoded)
    {
      i = semi + 1;
    }
    else
    {
      out.push_back(text[i++]);
    }
  }
  return out;
}

bool isXmlSpace(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

bool isNameEnd(char ch)
{
  return isXmlSpace(ch) || ch == '>' || ch == '/';
}

// `Name="…"` / `Name='…'` inside one start tag's attribute text.
auto attributeValue(std::string_view attributes, std::string_view wanted) -> std::optional<std::string>
{
  std::size_t i = 0;
  while (i < attributes.size())
  {
    while (i < attributes.size() && (isXmlSpace(attributes[i]) || attributes[i] == '/'))
    {
      ++i;
    }
    const auto nameStart = i;
    while (i < attributes.size() && !isXmlSpace(attributes[i]) && attributes[i] != '='
      && attributes[i] != '/')
    {
      ++i;
    }
    const auto name = attributes.substr(nameStart, i - nameStart);
    while (i < attributes.size() && isXmlSpace(attributes[i]))
    {
      ++i;
    }
    if (i >= attributes.size() || attributes[i] != '=')
    {
      if (name.empty())
      {
        ++i;
      }
      continue;
    }
    ++i;
    while (i < attributes.size() && isXmlSpace(attributes[i]))
    {
      ++i;
    }
    if (i >= attributes.size())
    {
      break;
    }
    const auto quote = attributes[i];
    if (quote != '"' && quote != '\'')
    {
      continue;
    }
    const auto close = attributes.find(quote, i + 1);
    if (close == std::string_view::npos)
    {
      break;
    }
    if (name == wanted)
    {
      return decodeEntities(attributes.substr(i + 1, close - i - 1));
    }
    i = close + 1;
  }
  return std::nullopt;
}

auto jsTrimUtf8(std::string_view text) -> std::string
{
  const auto wide = utf8To16(text);
  return utf16To8(jsTrim(wide));
}

// server.js customname: the last `/` segment of the last workspace folder URI
// containing `custom` (case-sensitive), `%3A` → `:`; else `custom`.
auto macroCustomName(const LibraryLayout& layout) -> std::string
{
  std::string customName = "custom";
  for (const auto& folder : layout.folders())
  {
    const auto uriText = folder.uri.toString();
    if (uriText.find("custom") == std::string::npos)
    {
      continue;
    }
    const auto slash = uriText.find_last_of('/');
    customName = slash == std::string::npos ? uriText : uriText.substr(slash + 1);
    if (const auto colon = customName.find("%3A"); colon != std::string::npos)
    {
      customName.replace(colon, 3, ":");
    }
  }
  return customName;
}

struct OpenCell
{
  std::optional<std::string> name;
  std::string text;
};

} // namespace

auto resolveMacroLibraryPath(std::string_view library, const LibraryLayout& layout)
  -> std::optional<std::filesystem::path>
{
  if (library.empty())
  {
    return std::nullopt;
  }
  const auto customName = macroCustomName(layout);
  for (const auto& folder : layout.folders())
  {
    if (folder.uri.toString().find(customName) == std::string::npos)
    {
      continue;
    }
    const auto root = pathFromUri(folder.uri);
    if (root.empty())
    {
      return std::nullopt;
    }
    return root / "complibs" / u8Path(library);
  }
  return std::nullopt;
}

auto customComplibsDirectory(const LibraryLayout& layout) -> std::optional<std::filesystem::path>
{
  const auto customName = macroCustomName(layout);
  for (const auto& folder : layout.folders())
  {
    if (folder.uri.toString().find(customName) == std::string::npos)
    {
      continue;
    }
    const auto root = pathFromUri(folder.uri);
    if (root.empty())
    {
      continue;
    }
    auto complibs = root / "complibs";
    std::error_code ec;
    if (std::filesystem::exists(complibs, ec))
    {
      return complibs;
    }
  }
  return std::nullopt;
}

auto macroLibraryXmlFiles(const std::filesystem::path& libraryPath) -> std::vector<std::string>
{
  std::vector<std::string> files;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(libraryPath, ec), end; !ec && it != end;
       it.increment(ec))
  {
    std::error_code typeEc;
    if (!it->is_regular_file(typeEc) || it->is_symlink(typeEc))
    {
      continue;
    }
    const auto name = pathUtf8(it->path().filename());
    if (!name.starts_with("d_") || name.size() < 4)
    {
      continue;
    }
    auto tail = name.substr(name.size() - 4);
    for (auto& ch : tail)
    {
      if (ch >= 'A' && ch <= 'Z')
      {
        ch = static_cast<char>(ch - 'A' + 'a');
      }
    }
    if (tail == ".xml")
    {
      files.push_back(name);
    }
  }
  return files;
}

bool isPrimaryMacroDatabaseFile(
  std::string_view fileName,
  const std::unordered_set<std::string>& allDatabaseFiles)
{
  if (!endsWithCompanionLetter(fileName))
  {
    return true;
  }
  return !allDatabaseFiles.contains(std::string(fileName.substr(0, fileName.size() - 1)));
}

auto primaryMacroDatabaseFiles(const std::filesystem::path& libraryPath)
  -> std::vector<std::filesystem::path>
{
  std::vector<std::string> candidates;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(libraryPath, ec), end; !ec && it != end;
       it.increment(ec))
  {
    std::error_code typeEc;
    if (!it->is_regular_file(typeEc) || it->is_symlink(typeEc))
    {
      continue;
    }
    const auto name = pathUtf8(it->path().filename());
    // path.extname(name) === '': no dot after the first character.
    if (!name.starts_with("d_") || name.find('.', 1) != std::string::npos)
    {
      continue;
    }
    candidates.push_back(name);
  }
  const std::unordered_set<std::string> all(candidates.begin(), candidates.end());
  std::vector<std::filesystem::path> primaries;
  for (const auto& name : candidates)
  {
    if (isPrimaryMacroDatabaseFile(name, all))
    {
      primaries.push_back(libraryPath / u8Path(name));
    }
  }
  return primaries;
}

bool macroDatabaseXmlIsStale(const std::filesystem::path& databasePath)
{
  auto xmlPath = databasePath;
  xmlPath += ".xml";
  std::error_code ec;
  if (!std::filesystem::exists(xmlPath, ec))
  {
    return true;
  }
  const auto dbTime = std::filesystem::last_write_time(databasePath, ec);
  if (ec)
  {
    return false;
  }
  const auto xmlTime = std::filesystem::last_write_time(xmlPath, ec);
  if (ec)
  {
    return false;
  }
  return dbTime > xmlTime;
}

auto ensureMacroLibraryXmlExports(const MacroLibraryExportRequest& request)
  -> MacroLibraryExportResult
{
  std::error_code ec;
  if (!std::filesystem::is_directory(request.libraryPath, ec))
  {
    return MacroLibraryExportResult::Done;
  }
  for (const auto& databasePath : primaryMacroDatabaseFiles(request.libraryPath))
  {
    if (request.isCanceled && request.isCanceled())
    {
      return MacroLibraryExportResult::Canceled;
    }
    // One helper per database at a time: the code-first background pass and a
    // foreground listing may reach the same stale database together.
    static std::mutex exportMutex;
    const auto lock = std::lock_guard(exportMutex);
    if (!macroDatabaseXmlIsStale(databasePath))
    {
      continue;
    }
    auto xmlPath = databasePath;
    xmlPath += ".xml";
    const auto result = exportDatabaseXml({
      .extensionUri = request.extensionUri,
      .databasePath = databasePath,
      .xmlPath = xmlPath,
      .isCanceled = request.isCanceled,
      .log = request.log,
    });
    if (result == HelperExportResult::Canceled)
    {
      return MacroLibraryExportResult::Canceled;
    }
    if (result == HelperExportResult::HelperMissing)
    {
      // Node checks the helper once before the loop; the helper module has
      // already logged where it looked.
      break;
    }
  }
  return MacroLibraryExportResult::Done;
}

auto parseMacroLibraryXml(std::string_view xml) -> std::vector<MacroLibraryRecord>
{
  if (xml.starts_with("\xEF\xBB\xBF"))
  {
    xml.remove_prefix(3);
  }
  std::vector<MacroLibraryRecord> records;
  std::vector<std::size_t> openRecords;
  std::vector<OpenCell> openCells;
  // Element names on the stack so an end tag closes the right Record/Cell.
  std::vector<std::string_view> stack;

  const auto appendText = [&](std::string_view text) {
    for (auto& cell : openCells)
    {
      cell.text += text;
    }
  };

  std::size_t i = 0;
  while (i < xml.size())
  {
    if (xml[i] != '<')
    {
      const auto next = xml.find('<', i);
      const auto end = next == std::string_view::npos ? xml.size() : next;
      if (!openCells.empty())
      {
        appendText(decodeEntities(xml.substr(i, end - i)));
      }
      i = end;
      continue;
    }
    if (xml.substr(i, 4) == "<!--")
    {
      const auto close = xml.find("-->", i + 4);
      i = close == std::string_view::npos ? xml.size() : close + 3;
      continue;
    }
    if (xml.substr(i, 9) == "<![CDATA[")
    {
      const auto close = xml.find("]]>", i + 9);
      const auto end = close == std::string_view::npos ? xml.size() : close;
      if (!openCells.empty())
      {
        appendText(xml.substr(i + 9, end - i - 9));
      }
      i = close == std::string_view::npos ? xml.size() : close + 3;
      continue;
    }
    if (xml.substr(i, 2) == "<?" || xml.substr(i, 2) == "<!")
    {
      const auto close = xml.find('>', i + 2);
      i = close == std::string_view::npos ? xml.size() : close + 1;
      continue;
    }

    // Find the tag end, skipping quoted attribute values.
    std::size_t j = i + 1;
    char quote = 0;
    while (j < xml.size())
    {
      const auto ch = xml[j];
      if (quote)
      {
        if (ch == quote)
        {
          quote = 0;
        }
      }
      else if (ch == '"' || ch == '\'')
      {
        quote = ch;
      }
      else if (ch == '>')
      {
        break;
      }
      ++j;
    }
    if (j >= xml.size())
    {
      break;
    }
    const auto tag = xml.substr(i + 1, j - i - 1);
    i = j + 1;

    if (tag.starts_with('/'))
    {
      auto name = tag.substr(1);
      while (!name.empty() && isXmlSpace(name.back()))
      {
        name.remove_suffix(1);
      }
      // Pop to the matching open element.
      for (auto k = stack.size(); k > 0; --k)
      {
        if (stack[k - 1] != name)
        {
          continue;
        }
        while (stack.size() >= k)
        {
          const auto top = stack.back();
          stack.pop_back();
          if (top == "Cell" && !openCells.empty())
          {
            auto cell = std::move(openCells.back());
            openCells.pop_back();
            if (cell.name && !cell.name->empty())
            {
              for (const auto index : openRecords)
              {
                records[index].push_back({*cell.name, jsTrimUtf8(cell.text), cell.text});
              }
            }
          }
          else if (top == "Record" && !openRecords.empty())
          {
            openRecords.pop_back();
          }
        }
        break;
      }
      continue;
    }

    std::size_t nameEnd = 0;
    while (nameEnd < tag.size() && !isNameEnd(tag[nameEnd]))
    {
      ++nameEnd;
    }
    const auto name = tag.substr(0, nameEnd);
    const auto attributes = tag.substr(nameEnd);
    const bool selfClosing = tag.ends_with('/');

    if (name == "Record")
    {
      records.emplace_back();
      if (!selfClosing)
      {
        openRecords.push_back(records.size() - 1);
        stack.push_back(name);
      }
      continue;
    }
    if (name == "Cell")
    {
      auto cellName = attributeValue(attributes, "Name");
      if (selfClosing)
      {
        if (cellName && !cellName->empty())
        {
          for (const auto index : openRecords)
          {
            records[index].push_back({*cellName, std::string{}, std::string{}});
          }
        }
        continue;
      }
      openCells.push_back({std::move(cellName), {}});
      stack.push_back(name);
      continue;
    }
    if (!selfClosing)
    {
      stack.push_back(name);
    }
  }
  return records;
}

auto readMacroLibraryRecords(const std::filesystem::path& libraryPath, std::string_view library)
  -> std::vector<MacroLibraryRecord>
{
  const auto read = [](const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
      return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  };
  auto text = read(libraryPath / u8Path("d_" + std::string(library) + ".xml"));
  if (!text)
  {
    text = read(libraryPath / "d_FRM_SHEET.xml");
  }
  if (!text)
  {
    return {};
  }
  return parseMacroLibraryXml(*text);
}

} // namespace vertexadt
