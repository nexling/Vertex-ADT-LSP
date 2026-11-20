#pragma once

#include <lsp/types.h>
#include <lsp/uri.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

enum class LibraryTree
{
  Custom,
  System,
};

struct OverlayIdentity
{
  std::string key;
  std::string displayPath;
  lsp::Uri uri;
  std::filesystem::path absPath;
  LibraryTree tree = LibraryTree::Custom;
};

class LibraryLayout
{
public:
  void setFolders(std::vector<lsp::WorkspaceFolder> folders, std::optional<lsp::Uri> rootUri);

  [[nodiscard]] auto folders() const -> const std::vector<lsp::WorkspaceFolder>&;
  [[nodiscard]] auto workspaceRoot() const -> const std::filesystem::path&;
  [[nodiscard]] auto customFolderName() const -> const std::string&;

  [[nodiscard]] auto overlayForPath(const std::filesystem::path& absPath) const
    -> std::optional<OverlayIdentity>;
  [[nodiscard]] auto overlayForUri(const lsp::Uri& uri) const -> std::optional<OverlayIdentity>;
  [[nodiscard]] bool customOccupies(std::string_view libraryRelativeKey) const;
  [[nodiscard]] bool shouldIndex(const std::filesystem::path& absPath) const;
  [[nodiscard]] bool isUserTree(const std::filesystem::path& absPath) const;

  [[nodiscard]] auto convertVertexPath(std::string_view token) const -> std::optional<std::string>;
  [[nodiscard]] auto resolveVertexFilePath(std::string_view inputPath) const
    -> std::optional<std::filesystem::path>;
  [[nodiscard]] auto findIncludedFile(std::string_view remainder) const
    -> std::optional<std::filesystem::path>;
  [[nodiscard]] auto libraryRelativeFromAbs(const std::filesystem::path& absPath) const
    -> std::optional<std::string>;

private:
  std::vector<lsp::WorkspaceFolder> folders_;
  std::filesystem::path workspaceRoot_;
  std::string customFolderName_ = "custom";

  void refreshCustomFolderName();
  [[nodiscard]] auto firstFolderPath() const -> std::filesystem::path;
};

[[nodiscard]] auto pathFromUri(const lsp::Uri& uri) -> std::filesystem::path;
[[nodiscard]] auto uriFromPath(const std::filesystem::path& path) -> lsp::Uri;
[[nodiscard]] auto fileExtensionLower(const std::filesystem::path& path) -> std::string;
[[nodiscard]] auto basenameLower(const std::filesystem::path& path) -> std::string;

} // namespace vertexadt
