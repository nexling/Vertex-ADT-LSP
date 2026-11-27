// GENERATED FILE — do not hand-edit.
// Regenerated from server/catalog/*.json by: npm run generate-catalogs

#include "generated/catalog_data.h"

namespace vertexadt::catalog::generated {

const NameType kIteratorObjectTypes[kIteratorObjectTypeCount] = {
  {std::string_view{"ANOTCHOBJ"}, std::string_view{"macro"}},
  {std::string_view{"AREAOBJ"}, std::string_view{"area"}},
  {std::string_view{"CUTFEATOBJ"}, std::string_view{"macro"}},
  {std::string_view{"DIMPLEOBJ"}, std::string_view{"macro"}},
  {std::string_view{"EDGEOBJ"}, std::string_view{"edge"}},
  {std::string_view{"FEATURE"}, std::string_view{"macro"}},
  {std::string_view{"FLANGECUT"}, std::string_view{"macro"}},
  {std::string_view{"LNOTCHOBJ"}, std::string_view{"macro"}},
  {std::string_view{"MACROBJ"}, std::string_view{"macro"}},
  {std::string_view{"NOTCHOBJ"}, std::string_view{"macro"}},
  {std::string_view{"PROFEND"}, std::string_view{"profileend"}},
  {std::string_view{"PROFOBJ"}, std::string_view{"profile"}},
  {std::string_view{"SELEMOBJ"}, std::string_view{"selemobj"}},
  {std::string_view{"SHEETOBJ"}, std::string_view{"sheet"}},
  {std::string_view{"SLOPEOBJ"}, std::string_view{"slope"}},
  {std::string_view{"TRUSSOBJ"}, std::string_view{"slope"}},
  {std::string_view{"WALLOBJ"}, std::string_view{"wall"}},
};

const std::string_view kIteratorJointTasks[kIteratorJointTaskCount] = {
  std::string_view{"CONNECTIONS"},
  std::string_view{"CONNECTIONS_HC"},
};

const NameType kIteratorTypoHints[kIteratorTypoHintCount] = {
  {std::string_view{"MACOBJ"}, std::string_view{"MACROBJ"}},
  {std::string_view{"MACROOBJ"}, std::string_view{"MACROBJ"}},
};

} // namespace vertexadt::catalog::generated
