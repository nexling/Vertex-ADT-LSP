#pragma once

#include "types.h"

#include <span>
#include <string_view>

namespace vertexadt::catalog {

std::span<const CommandCatalogEntry> commandCatalog();
const CommandCatalogEntry* findCommand(std::string_view lowercaseName);

std::span<const KeywordCatalogEntry> keywordCatalog();
const KeywordCatalogEntry* findKeyword(std::string_view label);
bool isKeywordLabel(std::string_view name);
bool isKeywordLabelIgnoreCase(std::string_view name);

std::span<const NameType> returnTypeMap();
std::span<const NameType> inputParamTypeMap();
std::span<const CommandSlotTypes> paramTypeMap();
std::span<const CommandSlotTypes> multiParamTypeMap();
std::span<const ObjectParameterCommand> objectParameterConfigurations();

// Typed iterator items (docs/adr/0030-typed-iterator-items.md): first value
// -> next_item type, the tasks whose items are joints, and typo hints whose
// `type` is the suggested first value. All case-sensitive.
std::span<const NameType> iteratorObjectTypes();
std::span<const std::string_view> iteratorJointTasks();
std::span<const NameType> iteratorTypoHints();

// Touches every compiled catalog so Release /OPT:REF cannot strip them.
// Does not run from initialize.
CatalogLink linkIntoExecutable();

} // namespace vertexadt::catalog
