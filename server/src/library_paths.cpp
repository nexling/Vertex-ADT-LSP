#include "library_paths.h"

#include "dir_paths.h"

#include "text_util.h"

#include <algorithm>
#include <array>
#include <regex>
#include <system_error>

namespace vertexadt {
namespace {

constexpr std::array<std::string_view, 7> kAdtExtensions{
  ".prg", ".adt", ".p", ".vxt", ".3d", ".2d", ""};

constexpr std::array<std::string_view, 11> kExcludedPrefixes{
  "d_",
  "browser_macros",
  "desktop.ini",
  ".gitignore",
  "zlasersetup",
  "opngtrim",
  "mousec",
  "profprop",
  "profpar",
  "panel1",
  "panel0",
};

constexpr std::array<std::string_view, 11> kExcludedSegments{
  "/picts/",
  "/dbases/",
  "/node_modules/",
  "/.git/",
  "/lw/",
  "/tiff/",
  "/fonts/",
  "/backuper/",
  "/forms/",
  "/texts/",
  "/internet/",
};

constexpr std::string_view kDetailsSegment = "/details/";

constexpr std::array<std::string_view, 40> kExcludedIndexExtensions{
  ".exe", ".dll", ".db", ".mdb", ".accdb", ".sqlite", ".bak",
  ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".tif", ".tiff", ".webp",
  ".zip", ".7z", ".rar", ".gz", ".tar",
  ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
  ".wav", ".mp3", ".mp4", ".avi", ".wmv",
  ".bin", ".dat", ".lock", ".vxm", ".vxp", ".gitignore", ".txt",
};

bool startsWithIgnoreCase(std::string_view text, std::string_view prefix)
{
  if (text.size() < prefix.size())
  {
    return false;
  }
  for (std::size_t i = 0; i < prefix.size(); ++i)
  {
    if (toLowerAscii(text.substr(i, 1)) != toLowerAscii(prefix.substr(i, 1)))
    {
      return false;
    }
  }
  return true;
}

bool looksLikeCustomName(std::string_view name)
{
  return startsWithIgnoreCase(name, "custom");
}

bool isFile(const std::filesystem::path& path)
{
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec);
}

std::optional<std::string> lastUriSegment(const lsp::Uri& uri)
{
  auto text = uri.toString();
  while (!text.empty() && (text.back() == '/' || text.back() == '\\'))
  {
    text.pop_back();
  }
  const auto slash = text.find_last_of('/');
  if (slash == std::string::npos)
  {
    return std::nullopt;
  }
  return lsp::Uri::decode(text.substr(slash + 1));
}

} // namespace

auto pathFromUri(const lsp::Uri& uri) -> std::filesystem::path
{
  if (!uri.isValid())
  {
    return {};
  }
  return std::filesystem::path(uri.fsPath());
}

auto uriFromPath(const std::filesystem::path& path) -> lsp::Uri
{
  // lsp::Uri::fileUriFromPath keeps Windows backslashes, which encode as %5C
  // and do not open in the editor. Build file:///C:/… with forward slashes.
  auto generic = std::filesystem::absolute(path).lexically_normal().generic_u8string();
  generic.insert(generic.begin(), u8'/');
  auto uri = lsp::Uri();
  uri.setScheme(lsp::Uri::FileScheme);
  uri.setAuthority({});
  uri.setPath(std::string_view(reinterpret_cast<const char*>(generic.data()), generic.size()));
  return uri;
}

auto fileExtensionLower(const std::filesystem::path& path) -> std::string
{
  return toLowerAscii(path.extension().string());
}

auto basenameLower(const std::filesystem::path& path) -> std::string
{
  return toLowerAscii(path.filename().string());
}

void LibraryLayout::setFolders(
  std::vector<lsp::WorkspaceFolder> folders,
  std::optional<lsp::Uri> rootUri)
{
  folders_ = std::move(folders);
  if (!folders_.empty())
  {
    auto root = pathFromUri(folders_.front().uri);
    if (folders_.size() > 1)
    {
      root = root.parent_path();
    }
    workspaceRoot_ = root;
  }
  else if (rootUri)
  {
    workspaceRoot_ = pathFromUri(*rootUri);
  }
  else
  {
    workspaceRoot_.clear();
  }
  refreshCustomFolderName();
}

auto LibraryLayout::folders() const -> const std::vector<lsp::WorkspaceFolder>&
{
  return folders_;
}

auto LibraryLayout::workspaceRoot() const -> const std::filesystem::path&
{
  return workspaceRoot_;
}

auto LibraryLayout::customFolderName() const -> const std::string&
{
  return customFolderName_;
}

auto LibraryLayout::firstFolderPath() const -> std::filesystem::path
{
  if (!folders_.empty())
  {
    return pathFromUri(folders_.front().uri);
  }
  return workspaceRoot_;
}

void LibraryLayout::refreshCustomFolderName()
{
  customFolderName_ = "custom";
  for (const auto& folder : folders_)
  {
    const auto uriText = toLowerAscii(folder.uri.toString());
    if (uriText.find("custom") == std::string::npos)
    {
      continue;
    }
    auto name = lastUriSegment(folder.uri);
    if (!name || !looksLikeCustomName(*name))
    {
      continue;
    }
    const auto candidate = workspaceRoot_ / *name;
    std::error_code ec;
    if (workspaceRoot_.empty()
        || std::filesystem::exists(candidate, ec)
        || normalizeFsPath(workspaceRoot_.filename().string()) == normalizeFsPath(*name))
    {
      customFolderName_ = *name;
      return;
    }
  }

  if (workspaceRoot_.empty())
  {
    return;
  }
  const auto rootBase = workspaceRoot_.filename().string();
  if (looksLikeCustomName(rootBase))
  {
    customFolderName_ = rootBase;
    return;
  }
  std::error_code ec;
  if (!std::filesystem::exists(workspaceRoot_, ec))
  {
    return;
  }
  for (const auto& entry : std::filesystem::directory_iterator(workspaceRoot_, ec))
  {
    if (entry.is_directory(ec) && looksLikeCustomName(entry.path().filename().string()))
    {
      customFolderName_ = entry.path().filename().string();
      return;
    }
  }
}

auto LibraryLayout::libraryRelativeFromAbs(const std::filesystem::path& absPath) const
  -> std::optional<std::string>
{
  if (workspaceRoot_.empty() || absPath.empty())
  {
    return std::nullopt;
  }
  const auto normalizedFile = normalizeFsPath(absPath.string());
  const auto normalizedRoot = normalizeFsPath(workspaceRoot_.string());
  if (normalizedFile.size() < normalizedRoot.size()
      || normalizedFile.compare(0, normalizedRoot.size(), normalizedRoot) != 0)
  {
    return std::nullopt;
  }
  auto relative = absPath.lexically_relative(workspaceRoot_).generic_string();
  relative = replaceBackslashes(relative);
  if (looksLikeCustomName(workspaceRoot_.filename().string()))
  {
    return relative;
  }
  const auto slash = relative.find('/');
  const auto first = slash == std::string::npos ? relative : relative.substr(0, slash);
  if (toLowerAscii(first) == "system")
  {
    return slash == std::string::npos ? std::string{} : relative.substr(slash + 1);
  }
  if (looksLikeCustomName(first))
  {
    return slash == std::string::npos ? std::string{} : relative.substr(slash + 1);
  }
  return std::nullopt;
}

auto LibraryLayout::overlayForPath(const std::filesystem::path& absPath) const
  -> std::optional<OverlayIdentity>
{
  const auto relative = libraryRelativeFromAbs(absPath);
  if (!relative)
  {
    return std::nullopt;
  }

  LibraryTree tree = LibraryTree::System;
  if (looksLikeCustomName(workspaceRoot_.filename().string()))
  {
    tree = LibraryTree::Custom;
  }
  else
  {
    const auto fromRoot = replaceBackslashes(
      absPath.lexically_relative(workspaceRoot_).generic_string());
    const auto slash = fromRoot.find('/');
    const auto first = slash == std::string::npos ? fromRoot : fromRoot.substr(0, slash);
    if (looksLikeCustomName(first))
    {
      tree = LibraryTree::Custom;
    }
  }

  if (tree == LibraryTree::System && customOccupies(toLowerAscii(*relative)))
  {
    return std::nullopt;
  }

  OverlayIdentity identity;
  identity.key = toLowerAscii(*relative);
  identity.displayPath = *relative;
  identity.uri = uriFromPath(absPath);
  identity.absPath = absPath;
  identity.tree = tree;
  return identity;
}

auto LibraryLayout::overlayForUri(const lsp::Uri& uri) const -> std::optional<OverlayIdentity>
{
  return overlayForPath(pathFromUri(uri));
}

bool LibraryLayout::customOccupies(std::string_view libraryRelativeKey) const
{
  if (workspaceRoot_.empty() || libraryRelativeKey.empty())
  {
    return false;
  }
  auto relative = replaceBackslashes(libraryRelativeKey);
  const auto customPath = looksLikeCustomName(workspaceRoot_.filename().string())
    ? workspaceRoot_ / relative
    : workspaceRoot_ / customFolderName_ / relative;
  return isFile(customPath);
}

bool LibraryLayout::isUserTree(const std::filesystem::path& absPath) const
{
  const auto normalized = normalizeFsPath(absPath.string());
  if (normalized.find("/user") != std::string::npos)
  {
    const auto relative = libraryRelativeFromAbs(absPath);
    return !relative.has_value();
  }
  return false;
}

bool LibraryLayout::shouldIndex(const std::filesystem::path& absPath) const
{
  if (absPath.empty() || !libraryRelativeFromAbs(absPath))
  {
    return false;
  }
  const auto ext = fileExtensionLower(absPath);
  for (const auto excluded : kExcludedIndexExtensions)
  {
    if (ext == excluded)
    {
      return false;
    }
  }
  const auto base = basenameLower(absPath);
  for (const auto prefix : kExcludedPrefixes)
  {
    if (base.starts_with(prefix))
    {
      return false;
    }
  }
  bool allowed = false;
  for (const auto adt : kAdtExtensions)
  {
    if (ext == adt)
    {
      allowed = true;
      break;
    }
  }
  if (!allowed)
  {
    return false;
  }
  const auto normalized = normalizeFsPath(absPath.string());
  for (const auto segment : kExcludedSegments)
  {
    if (normalized.find(segment) != std::string::npos)
    {
      return false;
    }
  }
  if (normalized.find(kDetailsSegment) != std::string::npos)
  {
    return false;
  }
  return true;
}

auto LibraryLayout::convertVertexPath(std::string_view token) const -> std::optional<std::string>
{
  // Exact table token inside `token` (which may carry `#`/quotes around it).
  const auto span = findDirTokenSpan(token);
  if (!span)
  {
    return std::nullopt;
  }
  const auto& row = *span->token;
  const auto withFolder = [&](std::string base) {
    if (!row.folder.empty())
    {
      base += '/';
      base += row.folder;
    }
    return base;
  };
  switch (row.kind)
  {
  case DirTokenKind::Fallback:
    return std::string(row.folder);
  case DirTokenKind::Pinned:
    switch (row.tree)
    {
    case DirTree::Custom:
      return withFolder(customFolderName_);
    case DirTree::System:
      return withFolder("system");
    case DirTree::Shared:
      return withFolder("shared");
    }
    break;
  case DirTokenKind::This:
  case DirTokenKind::ActProj:
    break;
  }
  return std::nullopt;
}

auto LibraryLayout::resolveVertexFilePath(std::string_view inputPath) const
  -> std::optional<std::filesystem::path>
{
  if (workspaceRoot_.empty() || inputPath.empty())
  {
    return std::nullopt;
  }
  auto filePath = replaceBackslashes(stripPathQuotes(inputPath));
  filePath = std::string(trim(filePath));
  if (filePath.empty())
  {
    return std::nullopt;
  }
  static const std::regex sectionOff{R"re(\s+SECTION(?:OFFILE)?$)re", std::regex::icase};
  filePath = std::regex_replace(filePath, sectionOff, "");
  filePath = std::string(trim(filePath));

  if (!filePath.empty() && filePath.front() == '$')
  {
    const auto slash = filePath.find('/');
    const auto first = slash == std::string::npos ? filePath : filePath.substr(0, slash);
    const auto converted = convertVertexPath(first);
    if (!converted)
    {
      return std::nullopt;
    }
    filePath = slash == std::string::npos ? *converted : *converted + filePath.substr(slash);
  }

  std::vector<std::filesystem::path> candidates;
  const auto asPath = std::filesystem::path(filePath);
  if (asPath.is_absolute())
  {
    candidates.push_back(asPath);
  }
  candidates.push_back(workspaceRoot_ / filePath);
  candidates.push_back(workspaceRoot_ / customFolderName_ / filePath);
  candidates.push_back(workspaceRoot_ / "system" / filePath);
  if (const auto included = findIncludedFile(filePath))
  {
    candidates.push_back(*included);
  }

  for (const auto& candidate : candidates)
  {
    if (isFile(candidate))
    {
      return candidate.lexically_normal();
    }
  }

  if (filePath.find('/') == std::string::npos)
  {
    const auto customCandidate = workspaceRoot_ / customFolderName_ / filePath;
    if (isFile(customCandidate))
    {
      return customCandidate.lexically_normal();
    }
    const auto systemCandidate = workspaceRoot_ / "system" / filePath;
    if (isFile(systemCandidate))
    {
      return systemCandidate.lexically_normal();
    }
  }
  return std::nullopt;
}

auto LibraryLayout::findIncludedFile(std::string_view remainder) const
  -> std::optional<std::filesystem::path>
{
  if (workspaceRoot_.empty() || remainder.empty())
  {
    return std::nullopt;
  }
  auto fileName = replaceBackslashes(remainder);
  if (!fileName.empty() && fileName.front() == '$')
  {
    const auto slash = fileName.find('/');
    const auto first = slash == std::string::npos ? fileName : fileName.substr(0, slash);
    const auto converted = convertVertexPath(first);
    fileName = (converted ? *converted : std::string("undefined"))
      + (slash == std::string::npos ? std::string{} : fileName.substr(slash));
  }
  if (fileName.find('/') == std::string::npos)
  {
    return std::nullopt;
  }
  const auto slash = fileName.find_last_of('/');
  const auto relativeFolder = fileName.substr(0, slash);
  const auto base = fileName.substr(slash + 1);
  auto customName = customFolderName_;
  auto systemName = std::string("system");
  const auto rootText = replaceBackslashes(workspaceRoot_.string());
  if (toLowerAscii(rootText).find("custom") != std::string::npos)
  {
    customName.clear();
  }
  if (toLowerAscii(rootText).find("system") != std::string::npos)
  {
    systemName.clear();
  }
  const auto customPath = workspaceRoot_ / customName / relativeFolder / base;
  if (isFile(customPath))
  {
    return customPath.lexically_normal();
  }
  const auto systemPath = workspaceRoot_ / systemName / relativeFolder / base;
  if (isFile(systemPath))
  {
    return systemPath.lexically_normal();
  }
  return std::nullopt;
}

} // namespace vertexadt
