// GENERATED FILE — do not hand-edit.
// Regenerated from server/catalog/*.json by: npm run generate-catalogs
#pragma once

#include "catalog/types.h"

#include <cstddef>

namespace vertexadt::catalog::generated {

inline constexpr std::size_t kCommandCount = 276;
inline constexpr std::size_t kKeywordCount = 1336;
inline constexpr std::size_t kReturnTypeCount = 360;
inline constexpr std::size_t kInputParamTypeCount = 87;
inline constexpr std::size_t kParamTypeCommandCount = 10;
inline constexpr std::size_t kMultiParamTypeCommandCount = 10;
inline constexpr std::size_t kObjectParameterCommandCount = 4;

extern const CommandCatalogEntry kCommands[kCommandCount];
extern const KeywordCatalogEntry kKeywords[kKeywordCount];
extern const NameType kReturnTypes[kReturnTypeCount];
extern const NameType kInputParamTypes[kInputParamTypeCount];
extern const CommandSlotTypes kParamTypes[kParamTypeCommandCount];
extern const CommandSlotTypes kMultiParamTypes[kMultiParamTypeCommandCount];
extern const ObjectParameterCommand kObjectParameters[kObjectParameterCommandCount];

} // namespace vertexadt::catalog::generated
