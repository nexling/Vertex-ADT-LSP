#pragma once

#include "cross_call_types.h"
#include "document_store.h"
#include "extracts.h"
#include "library_paths.h"
#include "linked_files.h"
#include "macro_library_xml.h"

#include <lsp/types.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt {

// Object-parameter completion. Ordinary textDocument/completion adds property
// names for the current name's object type on ten `get_*` / `set_*` sites.
// Heavy: collects the type timeline, and for macro/joint objects may export and
// read Custom `complibs/` XML through the Vertex Dbase Converter helper.

struct ObjectParameterSite
{
  // One of the ten site commands, lowercase.
  std::string command;
  // The captured first inside argument, trimmed (UTF-8).
  std::string variableName;
  int line = 0;
  // Byte column of `variableName` on `line`; -1 unknown.
  int column = -1;
};

// client extractGetCommandInfo on the line prefix `[0, character)`: ten
// lowercase unanchored patterns, first match wins.
[[nodiscard]] auto extractGetCommandInfo(std::u16string_view prefix)
  -> std::optional<ObjectParameterSite>;

// Cheap, on the message loop: vertexadt and extractGetCommandInfo on the
// cursor's line prefix.
[[nodiscard]] auto objectParameterSite(const Snapshot& snapshot, lsp::Position position)
  -> std::optional<ObjectParameterSite>;

// server.js extractBaseVariableName.
[[nodiscard]] auto extractBaseVariableName(std::string_view variableName) -> std::string;

// One parameter row: static parameterConfigurations or an XML field.
struct ObjectParameter
{
  std::string label;
  std::optional<std::string> documentation;
  std::optional<std::string> detail;
  std::optional<std::string> insertText;
};

// parameterConfigurations[command][objectType], else 'any', else 'generic'.
// Present empty lists win. Unknown command → empty.
[[nodiscard]] auto staticObjectParameters(std::string_view command, std::string_view objectType)
  -> std::vector<ObjectParameter>;

// Macro/joint record fields, flattened: `PARAMETERS` split on `|` and `=`,
// excludeList dropped. `joint` false matches CODE or SHTCODE.
[[nodiscard]] auto flattenRecordFields(
  const std::vector<MacroLibraryRecord>& records,
  std::string_view library,
  std::string_view code,
  bool joint) -> std::vector<ObjectParameter>;

struct ObjectParameterRequest
{
  const Snapshot& snapshot;
  const ObjectParameterSite& site;
  std::string_view extensionUri;
  bool strictModeEnabled = false;
  ExtractStore* extracts = nullptr;
  LinkedFileStore* linkedFiles = nullptr;
  const LibraryLayout* layout = nullptr;
  TextLookup textForPath;
  // Cross-call types: callers' argument types and callees' return fields.
  WorkspaceIndex* index = nullptr;
  CrossCallResolver* crossCall = nullptr;
  // Throws RequestCancelled; polled by the type timeline.
  std::function<void()> throwIfCanceled;
  // Polled by the helper export.
  std::function<bool()> isCanceled;
  std::function<void(std::string_view)> log;
};

// The family's items (never the cheap list). Nullopt when the helper export
// was canceled.
[[nodiscard]] auto objectParameterCompletionItems(const ObjectParameterRequest& request)
  -> std::optional<std::vector<lsp::CompletionItem>>;

} // namespace vertexadt
