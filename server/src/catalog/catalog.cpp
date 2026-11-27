#include "catalog.h"

#include "custom_validators.h"
#include "generated/catalog_data.h"
#include "table_validator.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

namespace vertexadt::catalog {
namespace {

template <typename Entry, typename GetName>
const Entry* findSorted(std::span<const Entry> rows, std::string_view name, GetName getName)
{
  const auto it = std::lower_bound(
    rows.begin(),
    rows.end(),
    name,
    [&](const Entry& entry, std::string_view needle)
    {
      return getName(entry) < needle;
    });
  if (it == rows.end() || getName(*it) != name)
  {
    return nullptr;
  }
  return &*it;
}

} // namespace

std::span<const CommandCatalogEntry> commandCatalog()
{
  return generated::kCommands;
}

const CommandCatalogEntry* findCommand(std::string_view lowercaseName)
{
  return findSorted(commandCatalog(), lowercaseName, [](const CommandCatalogEntry& entry) {
    return entry.name;
  });
}

std::span<const KeywordCatalogEntry> keywordCatalog()
{
  return generated::kKeywords;
}

const KeywordCatalogEntry* findKeyword(std::string_view label)
{
  for (const auto& entry : keywordCatalog())
  {
    if (entry.label == label)
    {
      return &entry;
    }
  }
  return nullptr;
}

bool isKeywordLabel(std::string_view name)
{
  static const auto labels = [] {
    std::unordered_set<std::string_view> out;
    out.reserve(keywordCatalog().size());
    for (const auto& entry : keywordCatalog())
    {
      out.insert(entry.label);
    }
    return out;
  }();
  return labels.contains(name);
}

bool isKeywordLabelIgnoreCase(std::string_view name)
{
  static const auto labels = [] {
    std::unordered_set<std::string> out;
    out.reserve(keywordCatalog().size());
    for (const auto& entry : keywordCatalog())
    {
      std::string lower(entry.label);
      for (char& ch : lower)
      {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }
      out.insert(std::move(lower));
    }
    return out;
  }();
  std::string lower(name);
  for (char& ch : lower)
  {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return labels.contains(lower);
}

std::span<const NameType> returnTypeMap()
{
  return generated::kReturnTypes;
}

std::span<const NameType> inputParamTypeMap()
{
  return generated::kInputParamTypes;
}

std::span<const CommandSlotTypes> paramTypeMap()
{
  return generated::kParamTypes;
}

std::span<const CommandSlotTypes> multiParamTypeMap()
{
  return generated::kMultiParamTypes;
}

std::span<const ObjectParameterCommand> objectParameterConfigurations()
{
  return generated::kObjectParameters;
}

std::span<const NameType> iteratorObjectTypes()
{
  return generated::kIteratorObjectTypes;
}

std::span<const std::string_view> iteratorJointTasks()
{
  return generated::kIteratorJointTasks;
}

std::span<const NameType> iteratorTypoHints()
{
  return generated::kIteratorTypoHints;
}

CatalogLink linkIntoExecutable()
{
  CatalogLink link;
  link.commandCount = commandCatalog().size();
  link.keywordCount = keywordCatalog().size();
  link.returnTypeCount = returnTypeMap().size();

  std::size_t fingerprint = 0;
  for (const CommandCatalogEntry& entry : commandCatalog())
  {
    fingerprint ^= entry.name.size();
    fingerprint ^= static_cast<std::size_t>(entry.structure.hasAssignment);
    if (entry.strictMode)
    {
      fingerprint ^= entry.strictMode->enabled ? 1u : 0u;
      fingerprint += entry.strictMode->inputTypes.size();
      fingerprint += entry.strictMode->afterParamTypes.size();
    }
    if (entry.cleanupRules)
    {
      fingerprint += entry.cleanupRules->afterParams.size();
    }
    if (entry.tableValidator)
    {
      ++link.tableValidatorCount;
      fingerprint ^= tableValidatorFingerprint(*entry.tableValidator);
      (void)applyTableValidator(*entry.tableValidator);
    }
    if (entry.customValidator != CustomValidatorId::None)
    {
      ++link.customValidatorCount;
      (void)applyCustomValidator(entry.customValidator);
    }
  }

  if (const CommandCatalogEntry* cube = findCommand("cube"))
  {
    fingerprint ^= cube->name.size();
  }
  if (const NameType* mixed = findSorted(returnTypeMap(), "Xlimit_max", [](const NameType& row) {
        return row.name;
      }))
  {
    fingerprint ^= mixed->type.size();
  }
  if (!keywordCatalog().empty())
  {
    fingerprint ^= keywordCatalog().front().label.size();
  }

  (void)fingerprint;
  return link;
}

} // namespace vertexadt::catalog
