// GENERATED FILE — do not hand-edit.
// Regenerated from server/catalog/*.json by: npm run generate-catalogs

#include "generated/catalog_data.h"

namespace vertexadt::catalog::generated {
namespace {

const std::string_view kStrictTypes_abs_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_abs_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_abs_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_abs{
  .enabled = true,
  .inputTypes = kStrictSlots_abs_input,
};

const std::string_view kStrictTypes_access_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_access_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_access_input_0,
    .paramName = std::string_view{"input mode"},
  },
};
const StrictMode kStrict_access{
  .enabled = true,
  .inputTypes = kStrictSlots_access_input,
};

const AssignmentCheck kAssign_access{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in access",
};
const Slot kInside_access[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in access"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in access"}},
};
const TableValidator kTable_access{
  .assignment = &kAssign_access,
  .tooFewInside = "access requires at least 1 parameter inside parentheses",
  .inside = kInside_access,
};

const std::string_view kStrictTypes_access_custom_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_access_custom_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_access_custom_input_0,
    .paramName = std::string_view{"input mode"},
  },
};
const StrictMode kStrict_access_custom{
  .enabled = true,
  .inputTypes = kStrictSlots_access_custom_input,
};

const AssignmentCheck kAssign_access_custom{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in access_custom",
};
const Slot kInside_access_custom[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in access_custom"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in access_custom"}},
};
const TableValidator kTable_access_custom{
  .assignment = &kAssign_access_custom,
  .tooFewInside = "access_custom requires at least 1 parameter inside parentheses",
  .inside = kInside_access_custom,
};

const std::string_view kStrictTypes_acos_input_0[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_acos_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_acos_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_acos{
  .enabled = true,
  .inputTypes = kStrictSlots_acos_input,
};

const AssignmentCheck kAssign_acos{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "acos requires an assignment variable",
  .invalid = "Assignment variable must be a valid variable in acos",
};
const Slot kInside_acos[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in acos"}},
};
const TableValidator kTable_acos{
  .assignment = &kAssign_acos,
  .tooFewInside = "acos requires 1 parameter inside parentheses",
  .inside = kInside_acos,
};

const std::string_view kStrictTypes_add_area_frmdata_input_0[] = {
  std::string_view{"area"},
  std::string_view{"edge"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_add_area_frmdata_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_add_area_frmdata_input_0,
    .paramName = std::string_view{"input area"},
  },
};
const StrictMode kStrict_add_area_frmdata{
  .enabled = true,
  .inputTypes = kStrictSlots_add_area_frmdata_input,
};

const Slot kInside_add_area_frmdata[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in add_area_frmdata"}},
};
const Slot kAfter_add_area_frmdata[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid string in add_area_frmdata"}},
  {.index = 1, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::AnyVariable, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable or string expression in add_area_frmdata"}},
};
const TableValidator kTable_add_area_frmdata{
  .tooFewInside = "add_area_frmdata requires 1 parameter inside parentheses",
  .tooFewAfter = "add_area_frmdata requires 2 parameters outside parentheses",
  .inside = kInside_add_area_frmdata,
  .after = kAfter_add_area_frmdata,
};

const std::string_view kStrictTypes_add_aux_task_input_0[] = {
  std::string_view{"value"},
  std::string_view{"macro"},
  std::string_view{"array"},
  std::string_view{"profile"},
  std::string_view{"area"},
  std::string_view{"slope"},
  std::string_view{"selemobj"},
  std::string_view{"sheet"},
  std::string_view{"profileend"},
};
const StrictTypeSlot kStrictSlots_add_aux_task_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_add_aux_task_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_add_aux_task{
  .enabled = true,
  .inputTypes = kStrictSlots_add_aux_task_input,
};

const Slot kInside_add_aux_task[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in add_aux_task"}},
};
const SlotTail kAfterTail_add_aux_task{.from = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .invalid = std::string_view{"Parameter {n} after parentheses must be a valid string in add_aux_task"}};
const TableValidator kTable_add_aux_task{
  .tooFewInside = "add_aux_task requires 1 parameter inside parentheses",
  .inside = kInside_add_aux_task,
  .afterTail = &kAfterTail_add_aux_task,
};

const std::string_view kStrictTypes_add_comp_member_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"macro"},
  std::string_view{"sheet"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_add_comp_member_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_add_comp_member_input_0,
    .paramName = std::string_view{"input component"},
  },
};
const StrictMode kStrict_add_comp_member{
  .enabled = true,
  .inputTypes = kStrictSlots_add_comp_member_input,
};

const Slot kInside_add_comp_member[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in add_comp_member"}},
};
const Slot kAfter_add_comp_member[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in add_comp_member"}},
};
const TableValidator kTable_add_comp_member{
  .tooFewInside = "add_comp_member requires 1 parameter inside parentheses",
  .tooFewAfter = "add_comp_member requires 1 parameter outside parentheses",
  .inside = kInside_add_comp_member,
  .after = kAfter_add_comp_member,
};

const std::string_view kStrictTypes_add_edge_input_0[] = {
  std::string_view{"area"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_add_edge_input_1[] = {
  std::string_view{"pvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
};
const std::string_view kStrictTypes_add_edge_input_2[] = {
  std::string_view{"pvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
};
const std::string_view kStrictTypes_add_edge_input_3[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_add_edge_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_add_edge_input_0,
    .paramName = std::string_view{"input area"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_add_edge_input_1,
    .paramName = std::string_view{"input start point"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_add_edge_input_2,
    .paramName = std::string_view{"input end point"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_add_edge_input_3,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_add_edge{
  .enabled = true,
  .inputTypes = kStrictSlots_add_edge_input,
};

const AssignmentCheck kAssign_add_edge{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "add_edge requires an assignment variable",
  .invalid = "Assignment variable must be a valid variable in add_edge",
};
const Slot kInside_add_edge[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in add_edge"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in add_edge"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in add_edge"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in add_edge"}},
};
const TableValidator kTable_add_edge{
  .assignment = &kAssign_add_edge,
  .tooFewInside = "add_edge requires 4 parameters inside parentheses",
  .inside = kInside_add_edge,
};

const std::string_view kStrictTypes_add_sobj_input_0[] = {
  std::string_view{"macro"},
  std::string_view{"selemobj"},
  std::string_view{"array"},
  std::string_view{"wall"},
  std::string_view{"edge"},
  std::string_view{"sheet"},
  std::string_view{"profile"},
  std::string_view{"slope"},
  std::string_view{"area"},
  std::string_view{"profileend"},
};
const StrictTypeSlot kStrictSlots_add_sobj_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_add_sobj_input_0,
    .paramName = std::string_view{"input parent"},
  },
};
const StrictMode kStrict_add_sobj{
  .enabled = true,
  .inputTypes = kStrictSlots_add_sobj_input,
};

const Slot kInside_add_sobj[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in add_sobj"}},
};
const Slot kAfter_add_sobj[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in add_sobj"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in add_sobj"}},
};
const TableValidator kTable_add_sobj{
  .tooFewInside = "add_sobj requires 1 parameter inside parentheses",
  .tooFewAfter = "add_sobj requires at least 1 parameter after parentheses",
  .inside = kInside_add_sobj,
  .after = kAfter_add_sobj,
};

const AssignmentCheck kAssign_ascii{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "ascii requires an assignment variable",
  .invalid = "Assignment variable must be a string variable in ascii",
};
const Slot kInside_ascii[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in ascii"}},
};
const TableValidator kTable_ascii{
  .assignment = &kAssign_ascii,
  .tooFewInside = "ascii requires 1 parameter inside parentheses",
  .inside = kInside_ascii,
};

const AssignmentCheck kAssign_ask{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "ask requires an assignment variable",
  .invalid = "Assignment variable must be a valid string variable in ask",
};
const Slot kInside_ask[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in ask"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in ask"}},
};
const TableValidator kTable_ask{
  .assignment = &kAssign_ask,
  .tooFewInside = "ask requires 2 parameters inside parentheses",
  .inside = kInside_ask,
};

const std::string_view kStrictTypes_askdim_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_askdim_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_askdim_input_0,
    .paramName = std::string_view{"input dimension"},
  },
};
const StrictMode kStrict_askdim{
  .enabled = true,
  .inputTypes = kStrictSlots_askdim_input,
};

const AssignmentCheck kAssign_askdim{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in askdim",
};
const Slot kInside_askdim[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in askdim"}},
  {.index = 1, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::AnyVariable, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in askdim"}},
};
const TableValidator kTable_askdim{
  .assignment = &kAssign_askdim,
  .tooFewInside = "askdim requires 2 parameters inside parentheses",
  .inside = kInside_askdim,
};

const std::string_view kStrictTypes_atan_input_0[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_atan_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_atan_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_atan{
  .enabled = true,
  .inputTypes = kStrictSlots_atan_input,
};

const AssignmentCheck kAssign_atan{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in atan",
};
const Slot kInside_atan[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in atan"}},
};
const TableValidator kTable_atan{
  .assignment = &kAssign_atan,
  .tooFewInside = "atan requires 1 parameter inside parentheses",
  .inside = kInside_atan,
};

const std::string_view kStrictTypes_atan2_input_0[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_atan2_input_1[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_atan2_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_atan2_input_0,
    .paramName = std::string_view{"input Y value"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_atan2_input_1,
    .paramName = std::string_view{"input X value"},
  },
};
const StrictMode kStrict_atan2{
  .enabled = true,
  .inputTypes = kStrictSlots_atan2_input,
};

const AssignmentCheck kAssign_atan2{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in atan2",
};
const Slot kInside_atan2[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in atan2"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in atan2"}},
};
const TableValidator kTable_atan2{
  .assignment = &kAssign_atan2,
  .tooFewInside = "atan2 requires 2 parameters inside parentheses",
  .inside = kInside_atan2,
};

const std::string_view kStrictTypes_base_to_dwg_input_0[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_base_to_dwg_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_base_to_dwg_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_base_to_dwg{
  .enabled = true,
  .inputTypes = kStrictSlots_base_to_dwg_input,
};

const AssignmentCheck kAssign_base_to_dwg{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in base_to_dwg",
};
const Slot kInside_base_to_dwg[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in base_to_dwg"}},
};
const TableValidator kTable_base_to_dwg{
  .assignment = &kAssign_base_to_dwg,
  .tooFewInside = "base_to_dwg requires 1 parameter inside parentheses",
  .inside = kInside_base_to_dwg,
};

const AssignmentCheck kAssign_build{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in build",
};
const Slot kInside_build[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in build"}},
};
const TableValidator kTable_build{
  .assignment = &kAssign_build,
  .tooFewInside = "build requires 1 parameter inside parentheses",
  .inside = kInside_build,
};

const std::string_view kStrictTypes_change_sheet_input_0[] = {
  std::string_view{"sheet"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_change_sheet_input_1[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_change_sheet_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_change_sheet_input_0,
    .paramName = std::string_view{"input sheet"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_change_sheet_input_1,
    .paramName = std::string_view{"optional thickness"},
  },
};
const StrictMode kStrict_change_sheet{
  .enabled = true,
  .inputTypes = kStrictSlots_change_sheet_input,
};

const AssignmentCheck kAssign_change_sheet{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in change_sheet",
};
const Slot kInside_change_sheet[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in change_sheet"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in change_sheet"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in change_sheet"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in change_sheet"}},
};
const TableValidator kTable_change_sheet{
  .assignment = &kAssign_change_sheet,
  .tooFewInside = "change_sheet requires at least 3 parameters inside parentheses",
  .inside = kInside_change_sheet,
};

const Slot kInside_close[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in close"}},
};
const TableValidator kTable_close{
  .tooFewInside = "close requires 1 parameter inside parentheses",
  .inside = kInside_close,
};

const AssignmentCheck kAssign_cmp{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in cmp",
};
const Slot kInside_cmp[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string in cmp"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in cmp"}},
};
const TableValidator kTable_cmp{
  .assignment = &kAssign_cmp,
  .tooFewInside = "cmp requires 2 parameters",
  .inside = kInside_cmp,
};

const AssignmentCheck kAssign_cmpi{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in cmpi",
};
const Slot kInside_cmpi[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string in cmpi"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in cmpi"}},
};
const TableValidator kTable_cmpi{
  .assignment = &kAssign_cmpi,
  .tooFewInside = "cmpi requires 2 parameters",
  .inside = kInside_cmpi,
};

const std::string_view kStrictTypes_common_dbases_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_common_dbases_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_common_dbases_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_common_dbases{
  .enabled = true,
  .inputTypes = kStrictSlots_common_dbases_input,
};

const Slot kAfter_common_dbases[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in common_dbases"}},
};
const TableValidator kTable_common_dbases{
  .tooFewAfter = "common_dbases requires 1 parameter outside parentheses",
  .after = kAfter_common_dbases,
};

const std::string_view kStrictTypes_common_elements_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_common_elements_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_common_elements_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_common_elements{
  .enabled = true,
  .inputTypes = kStrictSlots_common_elements_input,
};

const Slot kAfter_common_elements[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in common_elements"}},
};
const TableValidator kTable_common_elements{
  .tooFewAfter = "common_elements requires 1 parameter outside parentheses",
  .after = kAfter_common_elements,
};

const std::string_view kStrictTypes_common_files_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_common_files_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_common_files_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_common_files{
  .enabled = true,
  .inputTypes = kStrictSlots_common_files_input,
};

const Slot kAfter_common_files[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in common_files"}},
};
const TableValidator kTable_common_files{
  .tooFewAfter = "common_files requires 1 parameter outside parentheses",
  .after = kAfter_common_files,
};

const std::string_view kStrictTypes_common_indexes_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_common_indexes_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_common_indexes_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_common_indexes{
  .enabled = true,
  .inputTypes = kStrictSlots_common_indexes_input,
};

const Slot kAfter_common_indexes[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in common_indexes"}},
};
const TableValidator kTable_common_indexes{
  .tooFewAfter = "common_indexes requires 1 parameter outside parentheses",
  .after = kAfter_common_indexes,
};

const std::string_view kStrictTypes_common_points_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_common_points_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_common_points_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_common_points{
  .enabled = true,
  .inputTypes = kStrictSlots_common_points_input,
};

const Slot kAfter_common_points[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in common_points"}},
};
const TableValidator kTable_common_points{
  .tooFewAfter = "common_points requires 1 parameter outside parentheses",
  .after = kAfter_common_points,
};

const std::string_view kStrictTypes_connect_input_0[] = {
  std::string_view{"joint"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_connect_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_connect_input_0,
    .paramName = std::string_view{"input joint"},
  },
};
const std::string_view kStrictTypes_connect_after_0[] = {
  std::string_view{"macro"},
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"array"},
  std::string_view{"edge"},
  std::string_view{"selemobj"},
  std::string_view{"area"},
  std::string_view{"slope"},
  std::string_view{"wall"},
  std::string_view{"profileend"},
};
const std::string_view kStrictTypes_connect_after_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_connect_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_connect_after_0,
    .paramName = std::string_view{"input object"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_connect_after_1,
    .paramName = std::string_view{"input mode"},
  },
};
const StrictMode kStrict_connect{
  .enabled = true,
  .inputTypes = kStrictSlots_connect_input,
  .afterParamTypes = kStrictSlots_connect_after,
};

const Slot kInside_connect[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in connect"}},
};
const Slot kAfter_connect[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in connect"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter outside parentheses must be a valid string in connect"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter outside parentheses must be a valid variable in connect"}},
};
const TableValidator kTable_connect{
  .tooFewInside = "connect requires 1 parameter inside parentheses",
  .tooFewAfter = "connect requires at least 1 parameter outside parentheses",
  .inside = kInside_connect,
  .after = kAfter_connect,
};

const std::string_view kStrictTypes_copy_piece_pres_input_0[] = {
  std::string_view{"profile"},
};
const StrictTypeSlot kStrictSlots_copy_piece_pres_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_copy_piece_pres_input_0,
    .paramName = std::string_view{"input target profile"},
  },
};
const std::string_view kStrictTypes_copy_piece_pres_after_0[] = {
  std::string_view{"profile"},
};
const StrictTypeSlot kStrictSlots_copy_piece_pres_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_copy_piece_pres_after_0,
    .paramName = std::string_view{"input source profile"},
  },
};
const StrictMode kStrict_copy_piece_pres{
  .enabled = true,
  .inputTypes = kStrictSlots_copy_piece_pres_input,
  .afterParamTypes = kStrictSlots_copy_piece_pres_after,
};

const Slot kInside_copy_piece_pres[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in copy_piece_pres"}},
};
const Slot kAfter_copy_piece_pres[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in copy_piece_pres"}},
};
const TableValidator kTable_copy_piece_pres{
  .tooFewInside = "copy_piece_pres requires 1 parameter inside parentheses",
  .tooFewAfter = "copy_piece_pres requires 1 parameter outside parentheses",
  .inside = kInside_copy_piece_pres,
  .after = kAfter_copy_piece_pres,
};

const std::string_view kStrictTypes_cube_after_0[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_cube_after_1[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_cube_after_2[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_cube_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_cube_after_0,
    .paramName = std::string_view{"length x"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_cube_after_1,
    .paramName = std::string_view{"length y"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_cube_after_2,
    .paramName = std::string_view{"length z"},
  },
};
const StrictMode kStrict_cube{
  .enabled = true,
  .afterParamTypes = kStrictSlots_cube_after,
};

const Slot kAfter_cube[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid value in cube"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 must be a valid value in cube"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 must be a valid value in cube"}},
};
const TableValidator kTable_cube{
  .tooFewAfter = "cube requires 3 parameters",
  .after = kAfter_cube,
};

const AssignmentCheck kAssign_current_user{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "current_user requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in current_user",
};
const TableValidator kTable_current_user{
  .assignment = &kAssign_current_user,
};

const std::string_view kStrictTypes_dbadd_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbadd_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbadd_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const std::string_view kStrictTypes_dbadd_after_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dbadd_after_1[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbadd_after_2[] = {
  std::string_view{"string"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_dbadd_after_3[] = {
  std::string_view{"string"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_dbadd_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbadd_after_0,
    .paramName = std::string_view{"input database list"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbadd_after_1,
    .paramName = std::string_view{"input mode"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbadd_after_2,
    .paramName = std::string_view{"input field name(can be empty)"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_dbadd_after_3,
    .paramName = std::string_view{"input string"},
  },
};
const StrictMode kStrict_dbadd{
  .enabled = true,
  .inputTypes = kStrictSlots_dbadd_input,
  .afterParamTypes = kStrictSlots_dbadd_after,
};

const Slot kInside_dbadd[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbadd"}},
};
const Slot kAfter_dbadd[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dbadd"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dbadd"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter outside parentheses must be a valid string in dbadd"}},
  {.index = 3, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter outside parentheses must be a valid string in dbadd"}},
};
const TableValidator kTable_dbadd{
  .tooFewInside = "dbadd requires 1 parameter inside parentheses",
  .tooFewAfter = "dbadd requires at least 3 parameters outside parentheses",
  .inside = kInside_dbadd,
  .after = kAfter_dbadd,
};

const std::string_view kStrictTypes_dbaddrow_input_0[] = {
  std::string_view{"database"},
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbaddrow_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbaddrow_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbaddrow{
  .enabled = true,
  .inputTypes = kStrictSlots_dbaddrow_input,
};

const AssignmentCheck kAssign_dbaddrow{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "dbaddrow requires an assignment variable",
  .invalid = "Assignment variable must be a valid variable in dbaddrow",
};
const Slot kInside_dbaddrow[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbaddrow"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbaddrow"}},
};
const SlotTail kInsideTail_dbaddrow{.from = 2, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::NormalVariable, .invalid = std::string_view{"Parameter {n} inside parentheses must be a valid string or variable in dbaddrow"}};
const TableValidator kTable_dbaddrow{
  .assignment = &kAssign_dbaddrow,
  .tooFewInside = "dbaddrow requires 2 parameters inside parentheses",
  .inside = kInside_dbaddrow,
  .insideTail = &kInsideTail_dbaddrow,
};

const std::string_view kStrictTypes_dbalter_input_0[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbalter_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbalter_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbalter_input_0,
    .paramName = std::string_view{"input mode 1"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbalter_input_1,
    .paramName = std::string_view{"input mode 2"},
  },
};
const StrictMode kStrict_dbalter{
  .enabled = true,
  .inputTypes = kStrictSlots_dbalter_input,
};

const Slot kAfter_dbalter[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid string in dbalter"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dbalter"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter outside parentheses must be a valid variable in dbalter"}},
  {.index = 3, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter outside parentheses must be a valid string in dbalter"}},
};
const TableValidator kTable_dbalter{
  .tooFewAfter = "dbalter requires at least 4 parameters outside parentheses",
  .after = kAfter_dbalter,
};

const std::string_view kStrictTypes_dbbind_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbbind_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbbind_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const std::string_view kStrictTypes_dbbind_after_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbbind_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbbind_after_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbbind{
  .enabled = true,
  .inputTypes = kStrictSlots_dbbind_input,
  .afterParamTypes = kStrictSlots_dbbind_after,
};

const Slot kInside_dbbind[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbbind"}},
};
const Slot kAfter_dbbind[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dbbind"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid string in dbbind"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter outside parentheses must be a valid string in dbbind"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter outside parentheses must be a valid variable in dbbind"}},
};
const TableValidator kTable_dbbind{
  .tooFewInside = "dbbind requires 1 parameter inside parentheses",
  .tooFewAfter = "dbbind requires at least 3 parameters outside parentheses",
  .inside = kInside_dbbind,
  .after = kAfter_dbbind,
};

const AssignmentCheck kAssign_dbbutton_disable{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbbutton_disable",
};
const Slot kInside_dbbutton_disable[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in dbbutton_disable"}},
};
const TableValidator kTable_dbbutton_disable{
  .assignment = &kAssign_dbbutton_disable,
  .tooFewInside = "dbbutton_disable requires 1 parameter inside parentheses",
  .inside = kInside_dbbutton_disable,
};

const std::string_view kStrictTypes_dbchanged_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbchanged_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbchanged_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbchanged{
  .enabled = true,
  .inputTypes = kStrictSlots_dbchanged_input,
};

const AssignmentCheck kAssign_dbchanged{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbchanged",
};
const Slot kInside_dbchanged[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbchanged"}},
};
const TableValidator kTable_dbchanged{
  .assignment = &kAssign_dbchanged,
  .tooFewInside = "dbchanged requires 1 parameter inside parentheses",
  .inside = kInside_dbchanged,
};

const std::string_view kStrictTypes_dbclose_input_0[] = {
  std::string_view{"database"},
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbclose_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbclose_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbclose{
  .enabled = true,
  .inputTypes = kStrictSlots_dbclose_input,
};

const Slot kInside_dbclose[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbclose"}},
};
const TableValidator kTable_dbclose{
  .tooFewInside = "dbclose requires 1 parameter inside parentheses",
  .inside = kInside_dbclose,
};

const AssignmentCheck kAssign_dbclose_by_name{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbclose_by_name",
};
const Slot kInside_dbclose_by_name[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in dbclose_by_name"}},
};
const TableValidator kTable_dbclose_by_name{
  .assignment = &kAssign_dbclose_by_name,
  .tooFewInside = "dbclose_by_name requires 1 parameter inside parentheses",
  .inside = kInside_dbclose_by_name,
};

const std::string_view kStrictTypes_dbcontrol_disable_input_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbcontrol_disable_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbcontrol_disable_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbcontrol_disable_input_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbcontrol_disable_input_1,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbcontrol_disable{
  .enabled = true,
  .inputTypes = kStrictSlots_dbcontrol_disable_input,
};

const AssignmentCheck kAssign_dbcontrol_disable{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbcontrol_disable",
};
const Slot kInside_dbcontrol_disable[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbcontrol_disable"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in dbcontrol_disable"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in dbcontrol_disable"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in dbcontrol_disable"}},
};
const TableValidator kTable_dbcontrol_disable{
  .assignment = &kAssign_dbcontrol_disable,
  .tooFewInside = "dbcontrol_disable requires 4 parameters inside parentheses",
  .inside = kInside_dbcontrol_disable,
};

const std::string_view kStrictTypes_dbcontrol_hide_input_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbcontrol_hide_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbcontrol_hide_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbcontrol_hide_input_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbcontrol_hide_input_1,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbcontrol_hide{
  .enabled = true,
  .inputTypes = kStrictSlots_dbcontrol_hide_input,
};

const AssignmentCheck kAssign_dbcontrol_hide{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbcontrol_hide",
};
const Slot kInside_dbcontrol_hide[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbcontrol_hide"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in dbcontrol_hide"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in dbcontrol_hide"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in dbcontrol_hide"}},
};
const TableValidator kTable_dbcontrol_hide{
  .assignment = &kAssign_dbcontrol_hide,
  .tooFewInside = "dbcontrol_hide requires 4 parameters inside parentheses",
  .inside = kInside_dbcontrol_hide,
};

const std::string_view kStrictTypes_dbcopyrow_input_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbcopyrow_input_1[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbcopyrow_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbcopyrow_input_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbcopyrow_input_1,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbcopyrow{
  .enabled = true,
  .inputTypes = kStrictSlots_dbcopyrow_input,
};

const AssignmentCheck kAssign_dbcopyrow{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbcopyrow",
};
const Slot kInside_dbcopyrow[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbcopyrow"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbcopyrow"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in dbcopyrow"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in dbcopyrow"}},
};
const TableValidator kTable_dbcopyrow{
  .assignment = &kAssign_dbcopyrow,
  .tooFewInside = "dbcopyrow requires 4 parameters inside parentheses",
  .inside = kInside_dbcopyrow,
};

const std::string_view kStrictTypes_dbcreate_input_0[] = {
  std::string_view{"database"},
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbcreate_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbcreate_input_0,
    .paramName = std::string_view{"output database id"},
  },
};
const std::string_view kStrictTypes_dbcreate_after_0[] = {
  std::string_view{"string"},
};
const std::string_view kStrictTypes_dbcreate_after_1[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbcreate_after_2[] = {
  std::string_view{"value"},
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbcreate_after_3[] = {
  std::string_view{"string"},
};
const StrictTypeSlot kStrictSlots_dbcreate_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbcreate_after_0,
    .paramName = std::string_view{"database path"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbcreate_after_1,
    .paramName = std::string_view{"mode"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbcreate_after_2,
    .paramName = std::string_view{"mode or source database id"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_dbcreate_after_3,
    .paramName = std::string_view{"field definitions"},
  },
};
const StrictMode kStrict_dbcreate{
  .enabled = true,
  .inputTypes = kStrictSlots_dbcreate_input,
  .afterParamTypes = kStrictSlots_dbcreate_after,
};

const std::string_view kStrictTypes_dbdebug_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbdebug_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbdebug_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbdebug{
  .enabled = true,
  .inputTypes = kStrictSlots_dbdebug_input,
};

const AssignmentCheck kAssign_dbdebug{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbdebug",
};
const Slot kInside_dbdebug[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbdebug"}},
};
const TableValidator kTable_dbdebug{
  .assignment = &kAssign_dbdebug,
  .tooFewInside = "dbdebug requires 1 parameter inside parentheses",
  .inside = kInside_dbdebug,
};

const std::string_view kStrictTypes_dbdefsellist_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dbdefsellist_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbdefsellist_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const StrictMode kStrict_dbdefsellist{
  .enabled = true,
  .inputTypes = kStrictSlots_dbdefsellist_input,
};

const Slot kInside_dbdefsellist[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbdefsellist"}},
};
const TableValidator kTable_dbdefsellist{
  .tooFewInside = "dbdefsellist requires 1 parameter inside parentheses",
  .inside = kInside_dbdefsellist,
};

const Slot kAfter_dbdelete[] = {
  {.index = 0, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::AnyVariable, .required = true, .invalid = std::string_view{"Parameter must be a valid string or variable in dbdelete"}},
};
const TableValidator kTable_dbdelete{
  .tooFewAfter = "dbdelete requires 1 parameter",
  .after = kAfter_dbdelete,
};

const std::string_view kStrictTypes_dbdelrow_input_0[] = {
  std::string_view{"database"},
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbdelrow_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbdelrow_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbdelrow{
  .enabled = true,
  .inputTypes = kStrictSlots_dbdelrow_input,
};

const Slot kInside_dbdelrow[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbdelrow"}},
};
const Slot kAfter_dbdelrow[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in dbdelrow"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in dbdelrow"}},
};
const TableValidator kTable_dbdelrow{
  .tooFewInside = "dbdelrow requires 1 parameter inside parentheses",
  .tooFewAfter = "dbdelrow requires at least 1 parameter after parentheses",
  .inside = kInside_dbdelrow,
  .after = kAfter_dbdelrow,
};

const std::string_view kStrictTypes_dbdelrowsl_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbdelrowsl_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbdelrowsl_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const std::string_view kStrictTypes_dbdelrowsl_after_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dbdelrowsl_after_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbdelrowsl_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbdelrowsl_after_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbdelrowsl_after_1,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbdelrowsl{
  .enabled = true,
  .inputTypes = kStrictSlots_dbdelrowsl_input,
  .afterParamTypes = kStrictSlots_dbdelrowsl_after,
};

const Slot kInside_dbdelrowsl[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbdelrowsl"}},
};
const Slot kAfter_dbdelrowsl[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dbdelrowsl"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dbdelrowsl"}},
};
const TableValidator kTable_dbdelrowsl{
  .tooFewInside = "dbdelrowsl requires 1 parameter inside parentheses",
  .tooFewAfter = "dbdelrowsl requires 2 parameters outside parentheses",
  .inside = kInside_dbdelrowsl,
  .after = kAfter_dbdelrowsl,
};

const AssignmentCheck kAssign_dbdialog{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbdialog",
};
const Slot kInside_dbdialog[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in dbdialog"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in dbdialog"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dbdialog"}},
};
const TableValidator kTable_dbdialog{
  .assignment = &kAssign_dbdialog,
  .tooFewInside = "dbdialog requires at least 1 parameter inside parentheses",
  .inside = kInside_dbdialog,
};

const Slot kAfter_dbdump_to_xml[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid string in dbdump_to_xml"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid string in dbdump_to_xml"}},
};
const TableValidator kTable_dbdump_to_xml{
  .tooFewAfter = "dbdump_to_xml requires 2 parameters outside parentheses",
  .after = kAfter_dbdump_to_xml,
};

const std::string_view kStrictTypes_dbedit_input_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbedit_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbedit_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbedit_input_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbedit_input_1,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbedit{
  .enabled = true,
  .inputTypes = kStrictSlots_dbedit_input,
};

const AssignmentCheck kAssign_dbedit{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbedit",
};
const Slot kInside_dbedit[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbedit"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in dbedit"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in dbedit"}},
};
const TableValidator kTable_dbedit{
  .assignment = &kAssign_dbedit,
  .tooFewInside = "dbedit requires 3 parameters inside parentheses",
  .inside = kInside_dbedit,
};

const std::string_view kStrictTypes_dbeditfrm_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbeditfrm_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbeditfrm_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbeditfrm{
  .enabled = true,
  .inputTypes = kStrictSlots_dbeditfrm_input,
};

const Slot kAfter_dbeditfrm[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dbeditfrm"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid string in dbeditfrm"}},
};
const TableValidator kTable_dbeditfrm{
  .tooFewAfter = "dbeditfrm requires 2 parameters outside parentheses",
  .after = kAfter_dbeditfrm,
};

const std::string_view kStrictTypes_dbemptyf_input_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbemptyf_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbemptyf_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbemptyf_input_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbemptyf_input_1,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbemptyf{
  .enabled = true,
  .inputTypes = kStrictSlots_dbemptyf_input,
};

const AssignmentCheck kAssign_dbemptyf{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbemptyf",
};
const Slot kInside_dbemptyf[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbemptyf"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbemptyf"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dbemptyf"}},
};
const TableValidator kTable_dbemptyf{
  .assignment = &kAssign_dbemptyf,
  .tooFewInside = "dbemptyf requires 3 parameters inside parentheses",
  .inside = kInside_dbemptyf,
};

const std::string_view kStrictTypes_dbequalrows_input_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbequalrows_input_1[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbequalrows_input_2[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbequalrows_input_3[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbequalrows_input_4[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbequalrows_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbequalrows_input_0,
    .paramName = std::string_view{"input database 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbequalrows_input_1,
    .paramName = std::string_view{"input row id 1"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbequalrows_input_2,
    .paramName = std::string_view{"input database 2"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_dbequalrows_input_3,
    .paramName = std::string_view{"input row id 2"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_dbequalrows_input_4,
    .paramName = std::string_view{"input variable 1"},
  },
};
const StrictMode kStrict_dbequalrows{
  .enabled = true,
  .inputTypes = kStrictSlots_dbequalrows_input,
};

const AssignmentCheck kAssign_dbequalrows{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbequalrows",
};
const Slot kInside_dbequalrows[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbequalrows"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbequalrows"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in dbequalrows"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in dbequalrows"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fifth parameter inside parentheses must be a valid variable in dbequalrows"}},
};
const TableValidator kTable_dbequalrows{
  .assignment = &kAssign_dbequalrows,
  .tooFewInside = "dbequalrows requires 5 parameters inside parentheses",
  .inside = kInside_dbequalrows,
};

const std::string_view kStrictTypes_dbfenable_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbfenable_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbfenable_input_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbfenable{
  .enabled = true,
  .inputTypes = kStrictSlots_dbfenable_input,
};

const Slot kAfter_dbfenable[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid string in dbfenable"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dbfenable"}},
};
const TableValidator kTable_dbfenable{
  .tooFewAfter = "dbfenable requires 2 parameters outside parentheses",
  .after = kAfter_dbfenable,
};

const std::string_view kStrictTypes_dbfield_bgcolor_input_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbfield_bgcolor_input_1[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbfield_bgcolor_input_2[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbfield_bgcolor_input_3[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbfield_bgcolor_input_4[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbfield_bgcolor_input_5[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbfield_bgcolor_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbfield_bgcolor_input_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbfield_bgcolor_input_1,
    .paramName = std::string_view{"input value 1"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_dbfield_bgcolor_input_2,
    .paramName = std::string_view{"input value 2"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_dbfield_bgcolor_input_3,
    .paramName = std::string_view{"input value 3"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_dbfield_bgcolor_input_4,
    .paramName = std::string_view{"input value 4"},
  },
  {
    .paramIndex = 6,
    .allowedTypes = kStrictTypes_dbfield_bgcolor_input_5,
    .paramName = std::string_view{"input value 5"},
  },
};
const StrictMode kStrict_dbfield_bgcolor{
  .enabled = true,
  .inputTypes = kStrictSlots_dbfield_bgcolor_input,
};

const AssignmentCheck kAssign_dbfield_bgcolor{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbfield_bgcolor",
};
const Slot kInside_dbfield_bgcolor[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbfield_bgcolor"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbfield_bgcolor"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dbfield_bgcolor"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in dbfield_bgcolor"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fifth parameter inside parentheses must be a valid variable in dbfield_bgcolor"}},
  {.index = 5, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Sixth parameter inside parentheses must be a valid variable in dbfield_bgcolor"}},
  {.index = 6, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Seventh parameter inside parentheses must be a valid variable in dbfield_bgcolor"}},
};
const TableValidator kTable_dbfield_bgcolor{
  .assignment = &kAssign_dbfield_bgcolor,
  .tooFewInside = "dbfield_bgcolor requires 7 parameters inside parentheses",
  .inside = kInside_dbfield_bgcolor,
};

const std::string_view kStrictTypes_dbfind_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dbfind_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbfind_input_0,
    .paramName = std::string_view{"input database list"},
  },
};
const std::string_view kStrictTypes_dbfind_after_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbfind_after_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbfind_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbfind_after_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbfind_after_1,
    .paramName = std::string_view{"mode"},
  },
};
const StrictMode kStrict_dbfind{
  .enabled = true,
  .inputTypes = kStrictSlots_dbfind_input,
  .afterParamTypes = kStrictSlots_dbfind_after,
};

const Slot kInside_dbfind[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbfind"}},
};
const Slot kAfter_dbfind[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in dbfind"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in dbfind"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter after parentheses must be a valid string in dbfind"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter after parentheses must be a valid variable in dbfind"}},
};
const TableValidator kTable_dbfind{
  .tooFewInside = "dbfind requires 1 parameter inside parentheses",
  .tooFewAfter = "dbfind requires at least 2 parameters after parentheses",
  .inside = kInside_dbfind,
  .after = kAfter_dbfind,
};

const AssignmentCheck kAssign_dbgetactrow{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbgetactrow",
};
const Slot kInside_dbgetactrow[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in dbgetactrow"}},
};
const TableValidator kTable_dbgetactrow{
  .assignment = &kAssign_dbgetactrow,
  .tooFewInside = "dbgetactrow requires 1 parameter inside parentheses",
  .tooManyAfter = "dbgetactrow does not accept parameters outside parentheses",
  .inside = kInside_dbgetactrow,
};

const std::string_view kStrictTypes_dbgetall_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbgetall_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbgetall_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const std::string_view kStrictTypes_dbgetall_after_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbgetall_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbgetall_after_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbgetall{
  .enabled = true,
  .inputTypes = kStrictSlots_dbgetall_input,
  .afterParamTypes = kStrictSlots_dbgetall_after,
};

const Slot kInside_dbgetall[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbgetall"}},
};
const Slot kAfter_dbgetall[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in dbgetall"}},
};
const TableValidator kTable_dbgetall{
  .tooFewInside = "dbgetall requires 1 parameter inside parentheses",
  .tooFewAfter = "dbgetall requires 1 parameter outside parentheses",
  .inside = kInside_dbgetall,
  .after = kAfter_dbgetall,
};

const std::string_view kStrictTypes_dbgetf_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbgetf_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbgetf_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbgetf{
  .enabled = true,
  .inputTypes = kStrictSlots_dbgetf_input,
};

const AssignmentCheck kAssign_dbgetf{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbgetf",
};
const Slot kInside_dbgetf[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbgetf"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbgetf"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dbgetf"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in dbgetf"}},
};
const TableValidator kTable_dbgetf{
  .assignment = &kAssign_dbgetf,
  .tooFewInside = "dbgetf requires at least 3 parameters inside parentheses",
  .inside = kInside_dbgetf,
};

const std::string_view kStrictTypes_dbgetrid_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbgetrid_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbgetrid_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbgetrid{
  .enabled = true,
  .inputTypes = kStrictSlots_dbgetrid_input,
};

const AssignmentCheck kAssign_dbgetrid{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbgetrid",
};
const Slot kInside_dbgetrid[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbgetrid"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in dbgetrid"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dbgetrid"}},
};
const TableValidator kTable_dbgetrid{
  .assignment = &kAssign_dbgetrid,
  .tooFewInside = "dbgetrid requires 3 parameters inside parentheses",
  .inside = kInside_dbgetrid,
};

const std::string_view kStrictTypes_dbladd_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dbladd_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbladd_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const std::string_view kStrictTypes_dbladd_after_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbladd_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbladd_after_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbladd{
  .enabled = true,
  .inputTypes = kStrictSlots_dbladd_input,
  .afterParamTypes = kStrictSlots_dbladd_after,
};

const Slot kInside_dbladd[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbladd"}},
};
const Slot kAfter_dbladd[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in dbladd"}},
};
const TableValidator kTable_dbladd{
  .tooFewInside = "dbladd requires 1 parameter inside parentheses",
  .tooFewAfter = "dbladd requires 1 parameter outside parentheses",
  .inside = kInside_dbladd,
  .after = kAfter_dbladd,
};

const std::string_view kStrictTypes_dbladdl_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dbladdl_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbladdl_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const std::string_view kStrictTypes_dbladdl_after_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dbladdl_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbladdl_after_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const StrictMode kStrict_dbladdl{
  .enabled = true,
  .inputTypes = kStrictSlots_dbladdl_input,
  .afterParamTypes = kStrictSlots_dbladdl_after,
};

const Slot kInside_dbladdl[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbladdl"}},
};
const Slot kAfter_dbladdl[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in dbladdl"}},
};
const TableValidator kTable_dbladdl{
  .tooFewInside = "dbladdl requires 1 parameter inside parentheses",
  .tooFewAfter = "dbladdl requires 1 parameter outside parentheses",
  .inside = kInside_dbladdl,
  .after = kAfter_dbladdl,
};

const std::string_view kStrictTypes_dblalreadyopen_after_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dblalreadyopen_after_1[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblalreadyopen_after[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dblalreadyopen_after_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dblalreadyopen_after_1,
    .paramName = std::string_view{"input databaselist"},
  },
};
const StrictMode kStrict_dblalreadyopen{
  .enabled = true,
  .afterParamTypes = kStrictSlots_dblalreadyopen_after,
};

const Slot kInside_dblalreadyopen[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblalreadyopen"}},
};
const Slot kAfter_dblalreadyopen[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dblalreadyopen"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dblalreadyopen"}},
};
const TableValidator kTable_dblalreadyopen{
  .tooFewInside = "dblalreadyopen requires 1 parameter inside parentheses",
  .tooFewAfter = "dblalreadyopen requires 2 parameters outside parentheses",
  .inside = kInside_dblalreadyopen,
  .after = kAfter_dblalreadyopen,
};

const std::string_view kStrictTypes_dblclear_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblclear_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblclear_input_0,
    .paramName = std::string_view{"input database list"},
  },
};
const StrictMode kStrict_dblclear{
  .enabled = true,
  .inputTypes = kStrictSlots_dblclear_input,
};

const Slot kInside_dblclear[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblclear"}},
};
const TableValidator kTable_dblclear{
  .tooFewInside = "dblclear requires 1 parameter inside parentheses",
  .inside = kInside_dblclear,
};

const std::string_view kStrictTypes_dblclose_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblclose_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblclose_input_0,
    .paramName = std::string_view{"input database list"},
  },
};
const StrictMode kStrict_dblclose{
  .enabled = true,
  .inputTypes = kStrictSlots_dblclose_input,
};

const Slot kInside_dblclose[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblclose"}},
};
const TableValidator kTable_dblclose{
  .tooFewInside = "dblclose requires 1 parameter inside parentheses",
  .inside = kInside_dblclose,
};

const std::string_view kStrictTypes_dbldebug_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dbldebug_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbldebug_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const StrictMode kStrict_dbldebug{
  .enabled = true,
  .inputTypes = kStrictSlots_dbldebug_input,
};

const AssignmentCheck kAssign_dbldebug{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbldebug",
};
const Slot kInside_dbldebug[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbldebug"}},
};
const TableValidator kTable_dbldebug{
  .assignment = &kAssign_dbldebug,
  .tooFewInside = "dbldebug requires 1 parameter inside parentheses",
  .inside = kInside_dbldebug,
};

const std::string_view kStrictTypes_dbldell_input_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dbldell_input_1[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dbldell_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbldell_input_0,
    .paramName = std::string_view{"input databaselist 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbldell_input_1,
    .paramName = std::string_view{"input databaselist 2"},
  },
};
const StrictMode kStrict_dbldell{
  .enabled = true,
  .inputTypes = kStrictSlots_dbldell_input,
};

const AssignmentCheck kAssign_dbldell{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbldell",
};
const Slot kInside_dbldell[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbldell"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbldell"}},
};
const TableValidator kTable_dbldell{
  .assignment = &kAssign_dbldell,
  .tooFewInside = "dbldell requires 2 parameters inside parentheses",
  .inside = kInside_dbldell,
};

const std::string_view kStrictTypes_dbldelrid_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dbldelrid_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbldelrid_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const std::string_view kStrictTypes_dbldelrid_after_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbldelrid_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbldelrid_after_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbldelrid{
  .enabled = true,
  .inputTypes = kStrictSlots_dbldelrid_input,
  .afterParamTypes = kStrictSlots_dbldelrid_after,
};

const Slot kInside_dbldelrid[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbldelrid"}},
};
const Slot kAfter_dbldelrid[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in dbldelrid"}},
};
const TableValidator kTable_dbldelrid{
  .tooFewInside = "dbldelrid requires 1 parameter inside parentheses",
  .tooFewAfter = "dbldelrid requires 1 parameter outside parentheses",
  .inside = kInside_dbldelrid,
  .after = kAfter_dbldelrid,
};

const std::string_view kStrictTypes_dbledit_input_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dbledit_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbledit_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbledit_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbledit_input_1,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbledit{
  .enabled = true,
  .inputTypes = kStrictSlots_dbledit_input,
};

const AssignmentCheck kAssign_dbledit{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbledit",
};
const Slot kInside_dbledit[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbledit"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in dbledit"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in dbledit"}},
};
const TableValidator kTable_dbledit{
  .assignment = &kAssign_dbledit,
  .tooFewInside = "dbledit requires 3 parameters inside parentheses",
  .inside = kInside_dbledit,
};

const std::string_view kStrictTypes_dbledit2_input_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dbledit2_input_1[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbledit2_input_2[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbledit2_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbledit2_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbledit2_input_1,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_dbledit2_input_2,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_dbledit2{
  .enabled = true,
  .inputTypes = kStrictSlots_dbledit2_input,
};

const AssignmentCheck kAssign_dbledit2{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbledit2",
};
const Slot kInside_dbledit2[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbledit2"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbledit2"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dbledit2"}},
  {.index = 3, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid string in dbledit2"}},
  {.index = 4, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fifth parameter inside parentheses must be a valid string in dbledit2"}},
  {.index = 5, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Sixth parameter inside parentheses must be a valid variable in dbledit2"}},
};
const TableValidator kTable_dbledit2{
  .assignment = &kAssign_dbledit2,
  .tooFewInside = "dbledit2 requires 6 parameters inside parentheses",
  .inside = kInside_dbledit2,
};

const std::string_view kStrictTypes_dbledit2_unrelated_input_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dbledit2_unrelated_input_1[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dbledit2_unrelated_input_2[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbledit2_unrelated_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbledit2_unrelated_input_0,
    .paramName = std::string_view{"input databaselist 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbledit2_unrelated_input_1,
    .paramName = std::string_view{"input databaselist 2"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_dbledit2_unrelated_input_2,
    .paramName = std::string_view{"input mode"},
  },
};
const StrictMode kStrict_dbledit2_unrelated{
  .enabled = true,
  .inputTypes = kStrictSlots_dbledit2_unrelated_input,
};

const AssignmentCheck kAssign_dbledit2_unrelated{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbledit2_unrelated",
};
const Slot kInside_dbledit2_unrelated[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbledit2_unrelated"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbledit2_unrelated"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dbledit2_unrelated"}},
  {.index = 3, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid string in dbledit2_unrelated"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fifth parameter inside parentheses must be a valid variable in dbledit2_unrelated"}},
};
const TableValidator kTable_dbledit2_unrelated{
  .assignment = &kAssign_dbledit2_unrelated,
  .tooFewInside = "dbledit2_unrelated requires at least 3 parameters inside parentheses",
  .inside = kInside_dbledit2_unrelated,
};

const std::string_view kStrictTypes_dblfind_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblfind_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblfind_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const std::string_view kStrictTypes_dblfind_after_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dblfind_after_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dblfind_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblfind_after_0,
    .paramName = std::string_view{"input databaselist"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dblfind_after_1,
    .paramName = std::string_view{"input mode"},
  },
};
const StrictMode kStrict_dblfind{
  .enabled = true,
  .inputTypes = kStrictSlots_dblfind_input,
  .afterParamTypes = kStrictSlots_dblfind_after,
};

const Slot kInside_dblfind[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblfind"}},
};
const Slot kAfter_dblfind[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dblfind"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dblfind"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter outside parentheses must be a valid string in dblfind"}},
};
const TableValidator kTable_dblfind{
  .tooFewInside = "dblfind requires 1 parameter inside parentheses",
  .tooFewAfter = "dblfind requires 2 parameters outside parentheses",
  .inside = kInside_dblfind,
  .after = kAfter_dblfind,
};

const std::string_view kStrictTypes_dblgetval_input_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dblgetval_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dblgetval_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblgetval_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_dblgetval_input_1,
    .paramName = std::string_view{"input mode"},
  },
};
const StrictMode kStrict_dblgetval{
  .enabled = true,
  .inputTypes = kStrictSlots_dblgetval_input,
};

const AssignmentCheck kAssign_dblgetval{
  .presence = AssignmentPresence::Required,
  .missing = "dblgetval requires an assignment variable",
};
const Slot kInside_dblgetval[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dblgetval"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in dblgetval"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dblgetval"}},
  {.index = 3, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid string in dblgetval"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fifth parameter inside parentheses must be a valid variable in dblgetval"}},
};
const TableValidator kTable_dblgetval{
  .assignment = &kAssign_dblgetval,
  .tooFewInside = "dblgetval requires at least 4 parameters inside parentheses",
  .inside = kInside_dblgetval,
};

const std::string_view kStrictTypes_dblist_input_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dblist_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dblist_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblist_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dblist_input_1,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dblist{
  .enabled = true,
  .inputTypes = kStrictSlots_dblist_input,
};

const AssignmentCheck kAssign_dblist{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dblist",
};
const Slot kInside_dblist[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dblist"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dblist"}},
};
const TableValidator kTable_dblist{
  .assignment = &kAssign_dblist,
  .tooFewInside = "dblist requires 2 parameters inside parentheses",
  .inside = kInside_dblist,
};

const std::string_view kStrictTypes_dblistnum_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblistnum_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblistnum_input_0,
    .paramName = std::string_view{"input database list"},
  },
};
const StrictMode kStrict_dblistnum{
  .enabled = true,
  .inputTypes = kStrictSlots_dblistnum_input,
};

const AssignmentCheck kAssign_dblistnum{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dblistnum",
};
const Slot kInside_dblistnum[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblistnum"}},
};
const TableValidator kTable_dblistnum{
  .assignment = &kAssign_dblistnum,
  .tooFewInside = "dblistnum requires 1 parameter inside parentheses",
  .inside = kInside_dblistnum,
};

const std::string_view kStrictTypes_dblopen_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblopen_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblopen_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const std::string_view kStrictTypes_dblopen_after_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dblopen_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblopen_after_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dblopen{
  .enabled = true,
  .inputTypes = kStrictSlots_dblopen_input,
  .afterParamTypes = kStrictSlots_dblopen_after,
};

const Slot kInside_dblopen[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblopen"}},
};
const Slot kAfter_dblopen[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in dblopen"}},
};
const TableValidator kTable_dblopen{
  .tooFewInside = "dblopen requires 1 parameter inside parentheses",
  .tooFewAfter = "dblopen requires 1 parameter outside parentheses",
  .inside = kInside_dblopen,
  .after = kAfter_dblopen,
};

const std::string_view kStrictTypes_dblopenadd_after_0[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dblopenadd_after_1[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblopenadd_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblopenadd_after_0,
    .paramName = std::string_view{"value"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dblopenadd_after_1,
    .paramName = std::string_view{"input databaselist"},
  },
};
const StrictMode kStrict_dblopenadd{
  .enabled = true,
  .afterParamTypes = kStrictSlots_dblopenadd_after,
};

const Slot kAfter_dblopenadd[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dblopenadd"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dblopenadd"}},
};
const TableValidator kTable_dblopenadd{
  .tooFewAfter = "dblopenadd requires 2 parameters outside parentheses",
  .after = kAfter_dblopenadd,
};

const std::string_view kStrictTypes_dblopendel_after_0[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dblopendel_after_1[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblopendel_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblopendel_after_0,
    .paramName = std::string_view{"value"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dblopendel_after_1,
    .paramName = std::string_view{"input databaselist"},
  },
};
const StrictMode kStrict_dblopendel{
  .enabled = true,
  .afterParamTypes = kStrictSlots_dblopendel_after,
};

const Slot kAfter_dblopendel[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dblopendel"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dblopendel"}},
};
const TableValidator kTable_dblopendel{
  .tooFewAfter = "dblopendel requires 2 parameters outside parentheses",
  .after = kAfter_dblopendel,
};

const std::string_view kStrictTypes_dblopensort_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblopensort_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblopensort_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const StrictMode kStrict_dblopensort{
  .enabled = true,
  .inputTypes = kStrictSlots_dblopensort_input,
};

const Slot kInside_dblopensort[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblopensort"}},
};
const Slot kAfter_dblopensort[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in dblopensort"}},
};
const TableValidator kTable_dblopensort{
  .tooFewInside = "dblopensort requires 1 parameter inside parentheses",
  .tooFewAfter = "dblopensort requires 1 parameter outside parentheses",
  .inside = kInside_dblopensort,
  .after = kAfter_dblopensort,
};

const std::string_view kStrictTypes_dblprint_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblprint_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblprint_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const StrictMode kStrict_dblprint{
  .enabled = true,
  .inputTypes = kStrictSlots_dblprint_input,
};

const Slot kInside_dblprint[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblprint"}},
};
const Slot kAfter_dblprint[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 outside parentheses must be a valid variable in dblprint"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 outside parentheses must be a valid variable in dblprint"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 outside parentheses must be a valid variable in dblprint"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 outside parentheses must be a valid variable in dblprint"}},
};
const TableValidator kTable_dblprint{
  .tooFewInside = "dblprint requires 1 parameter inside parentheses",
  .tooFewAfter = "dblprint requires 4 parameters outside parentheses",
  .inside = kInside_dblprint,
  .after = kAfter_dblprint,
};

const std::string_view kStrictTypes_dblselect_input_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dblselect_input_1[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dblselect_input_2[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dblselect_input_3[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblselect_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblselect_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_dblselect_input_1,
    .paramName = std::string_view{"input variable 1"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_dblselect_input_2,
    .paramName = std::string_view{"input variable 2"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_dblselect_input_3,
    .paramName = std::string_view{"input databaselist 2"},
  },
};
const StrictMode kStrict_dblselect{
  .enabled = true,
  .inputTypes = kStrictSlots_dblselect_input,
};

const AssignmentCheck kAssign_dblselect{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dblselect",
};
const Slot kInside_dblselect[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dblselect"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in dblselect"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dblselect"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in dblselect"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fifth parameter inside parentheses must be a valid variable in dblselect"}},
  {.index = 5, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Sixth parameter inside parentheses must be a valid variable in dblselect"}},
};
const TableValidator kTable_dblselect{
  .assignment = &kAssign_dblselect,
  .tooFewInside = "dblselect requires at least 2 parameters inside parentheses",
  .inside = kInside_dblselect,
};

const AssignmentCheck kAssign_dblselectn{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dblselectn",
};
const Slot kInside_dblselectn[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dblselectn"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dblselectn"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in dblselectn"}},
  {.index = 3, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid string in dblselectn"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fifth parameter inside parentheses must be a valid variable in dblselectn"}},
  {.index = 5, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Sixth parameter inside parentheses must be a valid variable in dblselectn"}},
  {.index = 6, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Seventh parameter inside parentheses must be a valid variable in dblselectn"}},
};
const TableValidator kTable_dblselectn{
  .assignment = &kAssign_dblselectn,
  .tooFewInside = "dblselectn requires 7 parameters inside parentheses",
  .inside = kInside_dblselectn,
};

const std::string_view kStrictTypes_dblsort_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblsort_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblsort_input_0,
    .paramName = std::string_view{"input database list"},
  },
};
const StrictMode kStrict_dblsort{
  .enabled = true,
  .inputTypes = kStrictSlots_dblsort_input,
};

const Slot kInside_dblsort[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblsort"}},
};
const Slot kAfter_dblsort[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid string expression in dblsort"}},
};
const TableValidator kTable_dblsort{
  .tooFewInside = "dblsort requires 1 parameter inside parentheses",
  .tooFewAfter = "dblsort requires 1 parameter after parentheses",
  .inside = kInside_dblsort,
  .after = kAfter_dblsort,
};

const std::string_view kStrictTypes_dblstore_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblstore_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblstore_input_0,
    .paramName = std::string_view{"input database list"},
  },
};
const StrictMode kStrict_dblstore{
  .enabled = true,
  .inputTypes = kStrictSlots_dblstore_input,
};

const AssignmentCheck kAssign_dblstore{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in dblstore",
};
const Slot kInside_dblstore[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in dblstore"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in dblstore"}},
};
const TableValidator kTable_dblstore{
  .assignment = &kAssign_dblstore,
  .tooFewInside = "dblstore requires 2 parameters",
  .inside = kInside_dblstore,
};

const std::string_view kStrictTypes_dblvalid_input_0[] = {
  std::string_view{"databaselist"},
};
const StrictTypeSlot kStrictSlots_dblvalid_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dblvalid_input_0,
    .paramName = std::string_view{"input databaselist"},
  },
};
const StrictMode kStrict_dblvalid{
  .enabled = true,
  .inputTypes = kStrictSlots_dblvalid_input,
};

const AssignmentCheck kAssign_dblvalid{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dblvalid",
};
const Slot kInside_dblvalid[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dblvalid"}},
};
const TableValidator kTable_dblvalid{
  .assignment = &kAssign_dblvalid,
  .tooFewInside = "dblvalid requires 1 parameter inside parentheses",
  .inside = kInside_dblvalid,
};

const std::string_view kStrictTypes_dbname_input_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbname_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbname_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbname_input_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbname_input_1,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_dbname{
  .enabled = true,
  .inputTypes = kStrictSlots_dbname_input,
};

const AssignmentCheck kAssign_dbname{
  .presence = AssignmentPresence::Required,
  .missing = "dbname requires an assignment variable",
};
const Slot kInside_dbname[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbname"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbname"}},
};
const TableValidator kTable_dbname{
  .assignment = &kAssign_dbname,
  .tooFewInside = "dbname requires at least 1 parameter inside parentheses",
  .inside = kInside_dbname,
};

const std::string_view kStrictTypes_dbopen_input_0[] = {
  std::string_view{"database"},
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbopen_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbopen_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const std::string_view kStrictTypes_dbopen_after_0[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbopen_after_1[] = {
  std::string_view{"string"},
};
const StrictTypeSlot kStrictSlots_dbopen_after[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbopen_after_0,
    .paramName = std::string_view{"mode"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbopen_after_1,
    .paramName = std::string_view{"optional string"},
  },
};
const StrictMode kStrict_dbopen{
  .enabled = true,
  .inputTypes = kStrictSlots_dbopen_input,
  .afterParamTypes = kStrictSlots_dbopen_after,
};

const Slot kInside_dbopen[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbopen"}},
};
const Slot kAfter_dbopen[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid string in dbopen"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in dbopen"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter after parentheses must be a valid string in dbopen"}},
};
const TableValidator kTable_dbopen{
  .tooFewInside = "dbopen requires 1 parameter inside parentheses",
  .tooFewAfter = "dbopen requires at least 1 parameter after parentheses",
  .inside = kInside_dbopen,
  .after = kAfter_dbopen,
};

const std::string_view kStrictTypes_dbpage_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbpage_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbpage_input_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbpage{
  .enabled = true,
  .inputTypes = kStrictSlots_dbpage_input,
};

const Slot kInside_dbpage[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbpage"}},
};
const TableValidator kTable_dbpage{
  .tooFewInside = "dbpage requires 1 parameter inside parentheses",
  .tooManyAfter = "dbpage does not accept parameters outside parentheses",
  .inside = kInside_dbpage,
};

const std::string_view kStrictTypes_dbpage2_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbpage2_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbpage2_input_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbpage2{
  .enabled = true,
  .inputTypes = kStrictSlots_dbpage2_input,
};

const Slot kInside_dbpage2[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbpage2"}},
};
const TableValidator kTable_dbpage2{
  .tooFewInside = "dbpage2 requires 1 parameter inside parentheses",
  .tooManyAfter = "dbpage2 does not accept parameters outside parentheses",
  .inside = kInside_dbpage2,
};

const std::string_view kStrictTypes_dbpicformlist_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbpicformlist_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbpicformlist_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbpicformlist{
  .enabled = true,
  .inputTypes = kStrictSlots_dbpicformlist_input,
};

const Slot kInside_dbpicformlist[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbpicformlist"}},
};
const Slot kAfter_dbpicformlist[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dbpicformlist"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dbpicformlist"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter outside parentheses must be a valid variable in dbpicformlist"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter outside parentheses must be a valid variable in dbpicformlist"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fifth parameter outside parentheses must be a valid variable in dbpicformlist"}},
  {.index = 5, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Sixth parameter outside parentheses must be a valid string in dbpicformlist"}},
};
const TableValidator kTable_dbpicformlist{
  .tooFewInside = "dbpicformlist requires 1 parameter inside parentheses",
  .inside = kInside_dbpicformlist,
  .after = kAfter_dbpicformlist,
};

const std::string_view kStrictTypes_dbprint_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbprint_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbprint_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbprint{
  .enabled = true,
  .inputTypes = kStrictSlots_dbprint_input,
};

const Slot kInside_dbprint[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbprint"}},
};
const Slot kAfter_dbprint[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 outside parentheses must be a valid string in dbprint"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 outside parentheses must be a valid string in dbprint"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 outside parentheses must be a valid string in dbprint"}},
  {.index = 3, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 outside parentheses must be a valid string in dbprint"}},
};
const TableValidator kTable_dbprint{
  .tooFewInside = "dbprint requires 1 parameter inside parentheses",
  .tooFewAfter = "dbprint requires 4 parameters outside parentheses",
  .inside = kInside_dbprint,
  .after = kAfter_dbprint,
};

const std::string_view kStrictTypes_dbputall_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbputall_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbputall_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const std::string_view kStrictTypes_dbputall_after_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbputall_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbputall_after_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbputall{
  .enabled = true,
  .inputTypes = kStrictSlots_dbputall_input,
  .afterParamTypes = kStrictSlots_dbputall_after,
};

const Slot kInside_dbputall[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbputall"}},
};
const Slot kAfter_dbputall[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in dbputall"}},
};
const TableValidator kTable_dbputall{
  .tooFewInside = "dbputall requires 1 parameter inside parentheses",
  .tooFewAfter = "dbputall requires 1 parameter outside parentheses",
  .inside = kInside_dbputall,
  .after = kAfter_dbputall,
};

const std::string_view kStrictTypes_dbputf_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbputf_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbputf_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbputf{
  .enabled = true,
  .inputTypes = kStrictSlots_dbputf_input,
};

const Slot kInside_dbputf[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbputf"}},
};
const Slot kAfter_dbputf[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in dbputf"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid string in dbputf"}},
  {.index = 2, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::NormalVariable, .required = true, .invalid = std::string_view{"Third parameter after parentheses must be a valid string or variable in dbputf"}},
};
const TableValidator kTable_dbputf{
  .tooFewInside = "dbputf requires 1 parameter inside parentheses",
  .tooFewAfter = "dbputf requires 3 parameters after parentheses",
  .inside = kInside_dbputf,
  .after = kAfter_dbputf,
};

const std::string_view kStrictTypes_dbrollout_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbrollout_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbrollout_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const std::string_view kStrictTypes_dbrollout_after_0[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbrollout_after_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbrollout_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbrollout_after_0,
    .paramName = std::string_view{"input variable 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbrollout_after_1,
    .paramName = std::string_view{"input variable 2"},
  },
};
const StrictMode kStrict_dbrollout{
  .enabled = true,
  .inputTypes = kStrictSlots_dbrollout_input,
  .afterParamTypes = kStrictSlots_dbrollout_after,
};

const Slot kInside_dbrollout[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbrollout"}},
};
const Slot kAfter_dbrollout[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dbrollout"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dbrollout"}},
};
const TableValidator kTable_dbrollout{
  .tooFewInside = "dbrollout requires 1 parameter inside parentheses",
  .tooFewAfter = "dbrollout requires 2 parameters outside parentheses",
  .inside = kInside_dbrollout,
  .after = kAfter_dbrollout,
};

const std::string_view kStrictTypes_dbrowstatus_input_0[] = {
  std::string_view{"database"},
};
const std::string_view kStrictTypes_dbrowstatus_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbrowstatus_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbrowstatus_input_0,
    .paramName = std::string_view{"input database"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbrowstatus_input_1,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbrowstatus{
  .enabled = true,
  .inputTypes = kStrictSlots_dbrowstatus_input,
};

const AssignmentCheck kAssign_dbrowstatus{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbrowstatus",
};
const Slot kInside_dbrowstatus[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in dbrowstatus"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbrowstatus"}},
};
const TableValidator kTable_dbrowstatus{
  .assignment = &kAssign_dbrowstatus,
  .tooFewInside = "dbrowstatus requires 2 parameters inside parentheses",
  .tooManyAfter = "dbrowstatus does not accept parameters outside parentheses",
  .inside = kInside_dbrowstatus,
};

const std::string_view kStrictTypes_dbsave_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbsave_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbsave_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbsave{
  .enabled = true,
  .inputTypes = kStrictSlots_dbsave_input,
};

const AssignmentCheck kAssign_dbsave{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dbsave",
};
const Slot kInside_dbsave[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbsave"}},
};
const TableValidator kTable_dbsave{
  .assignment = &kAssign_dbsave,
  .tooFewInside = "dbsave requires 1 parameter inside parentheses",
  .tooManyAfter = "dbsave does not accept parameters outside parentheses",
  .inside = kInside_dbsave,
};

const std::string_view kStrictTypes_dbselect_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbselect_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbselect_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const std::string_view kStrictTypes_dbselect_after_0[] = {
  std::string_view{"databaselist"},
};
const std::string_view kStrictTypes_dbselect_after_1[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbselect_after_2[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_dbselect_after_3[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbselect_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbselect_after_0,
    .paramName = std::string_view{"input databaselist"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_dbselect_after_1,
    .paramName = std::string_view{"input variable 1"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_dbselect_after_2,
    .paramName = std::string_view{"input variable 2"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_dbselect_after_3,
    .paramName = std::string_view{"input variable 3"},
  },
};
const StrictMode kStrict_dbselect{
  .enabled = true,
  .inputTypes = kStrictSlots_dbselect_input,
  .afterParamTypes = kStrictSlots_dbselect_after,
};

const Slot kInside_dbselect[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbselect"}},
};
const Slot kAfter_dbselect[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid variable in dbselect"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid string in dbselect"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter outside parentheses must be a valid variable in dbselect"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter outside parentheses must be a valid variable in dbselect"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fifth parameter outside parentheses must be a valid variable in dbselect"}},
  {.index = 5, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Sixth parameter outside parentheses must be a valid string in dbselect"}},
};
const TableValidator kTable_dbselect{
  .tooFewInside = "dbselect requires 1 parameter inside parentheses",
  .tooFewAfter = "dbselect requires 6 parameters outside parentheses",
  .inside = kInside_dbselect,
  .after = kAfter_dbselect,
};

const std::string_view kStrictTypes_dbselector_inp_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbselector_inp_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbselector_inp_input_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbselector_inp{
  .enabled = true,
  .inputTypes = kStrictSlots_dbselector_inp_input,
};

const AssignmentCheck kAssign_dbselector_inp{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "dbselector_inp requires an assignment variable",
  .invalid = "Assignment variable must be a valid variable in dbselector_inp",
};
const Slot kInside_dbselector_inp[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in dbselector_inp"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in dbselector_inp"}},
};
const TableValidator kTable_dbselector_inp{
  .assignment = &kAssign_dbselector_inp,
  .tooFewInside = "dbselector_inp requires 2 parameters inside parentheses",
  .inside = kInside_dbselector_inp,
};

const std::string_view kStrictTypes_dbsetactrow_input_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbsetactrow_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_dbsetactrow_input_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbsetactrow{
  .enabled = true,
  .inputTypes = kStrictSlots_dbsetactrow_input,
};

const Slot kAfter_dbsetactrow[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid string in dbsetactrow"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in dbsetactrow"}},
};
const TableValidator kTable_dbsetactrow{
  .tooFewAfter = "dbsetactrow requires 2 parameters outside parentheses",
  .after = kAfter_dbsetactrow,
};

const std::string_view kStrictTypes_dbtranslate_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbtranslate_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbtranslate_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const std::string_view kStrictTypes_dbtranslate_after_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_dbtranslate_after[] = {
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_dbtranslate_after_0,
    .paramName = std::string_view{"input variable"},
  },
};
const StrictMode kStrict_dbtranslate{
  .enabled = true,
  .inputTypes = kStrictSlots_dbtranslate_input,
  .afterParamTypes = kStrictSlots_dbtranslate_after,
};

const Slot kInside_dbtranslate[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbtranslate"}},
};
const Slot kAfter_dbtranslate[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid string in dbtranslate"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid string in dbtranslate"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter outside parentheses must be a valid string in dbtranslate"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter outside parentheses must be a valid variable in dbtranslate"}},
};
const TableValidator kTable_dbtranslate{
  .tooFewInside = "dbtranslate requires 1 parameter inside parentheses",
  .tooFewAfter = "dbtranslate requires 4 parameters outside parentheses",
  .inside = kInside_dbtranslate,
  .after = kAfter_dbtranslate,
};

const std::string_view kStrictTypes_dbunset_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbunset_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbunset_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbunset{
  .enabled = true,
  .inputTypes = kStrictSlots_dbunset_input,
};

const Slot kInside_dbunset[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dbunset"}},
};
const TableValidator kTable_dbunset{
  .tooFewInside = "dbunset requires 1 parameter inside parentheses",
  .inside = kInside_dbunset,
};

const std::string_view kStrictTypes_dbvalid_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_dbvalid_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dbvalid_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_dbvalid{
  .enabled = true,
  .inputTypes = kStrictSlots_dbvalid_input,
};

const AssignmentCheck kAssign_dbvalid{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in dbvalid",
};
const Slot kInside_dbvalid[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in dbvalid"}},
};
const TableValidator kTable_dbvalid{
  .assignment = &kAssign_dbvalid,
  .tooFewInside = "dbvalid requires 1 parameter",
  .inside = kInside_dbvalid,
};

const Slot kAfter_debug[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid string in debug"}},
};
const TableValidator kTable_debug{
  .tooFewAfter = "debug requires 1 parameter outside parentheses",
  .after = kAfter_debug,
};

const std::string_view kStrictTypes_del_joint_det_input_0[] = {
  std::string_view{"joint"},
  std::string_view{"macro"},
  std::string_view{"array"},
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"edge"},
  std::string_view{"selemobj"},
  std::string_view{"area"},
  std::string_view{"slope"},
  std::string_view{"wall"},
};
const StrictTypeSlot kStrictSlots_del_joint_det_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_del_joint_det_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_del_joint_det{
  .enabled = true,
  .inputTypes = kStrictSlots_del_joint_det_input,
};

const Slot kInside_del_joint_det[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in del_joint_det"}},
};
const TableValidator kTable_del_joint_det{
  .tooFewInside = "del_joint_det requires 1 parameter inside parentheses",
  .inside = kInside_del_joint_det,
};

const std::string_view kStrictTypes_del_object_input_0[] = {
  std::string_view{"profileend"},
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"area"},
  std::string_view{"macro"},
  std::string_view{"wall"},
  std::string_view{"joint"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_del_object_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_del_object_input_0,
    .paramName = std::string_view{"object to delete"},
  },
};
const StrictMode kStrict_del_object{
  .enabled = true,
  .inputTypes = kStrictSlots_del_object_input,
};

const Slot kInside_del_object[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in del_object"}},
};
const TableValidator kTable_del_object{
  .tooFewInside = "del_object requires 1 parameter inside parentheses",
  .inside = kInside_del_object,
};

const std::string_view kStrictTypes_disconnect_input_0[] = {
  std::string_view{"profileend"},
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"area"},
  std::string_view{"edge"},
  std::string_view{"macro"},
  std::string_view{"wall"},
  std::string_view{"array"},
  std::string_view{"joint"},
};
const StrictTypeSlot kStrictSlots_disconnect_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_disconnect_input_0,
    .paramName = std::string_view{"object to disconnect"},
  },
};
const StrictMode kStrict_disconnect{
  .enabled = true,
  .inputTypes = kStrictSlots_disconnect_input,
};

const Slot kInside_disconnect[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in disconnect"}},
};
const TableValidator kTable_disconnect{
  .tooFewInside = "disconnect requires 1 parameter inside parentheses",
  .inside = kInside_disconnect,
};

const std::string_view kStrictTypes_drop_poly_input_0[] = {
  std::string_view{"polyline"},
};
const StrictTypeSlot kStrictSlots_drop_poly_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_drop_poly_input_0,
    .paramName = std::string_view{"input polyline"},
  },
};
const StrictMode kStrict_drop_poly{
  .enabled = true,
  .inputTypes = kStrictSlots_drop_poly_input,
};

const AssignmentCheck kAssign_drop_poly{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in drop_poly",
};
const Slot kInside_drop_poly[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in drop_poly"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in drop_poly"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in drop_poly"}},
};
const TableValidator kTable_drop_poly{
  .assignment = &kAssign_drop_poly,
  .tooFewInside = "drop_poly requires 2 parameters inside parentheses",
  .inside = kInside_drop_poly,
};

const std::string_view kStrictTypes_dwg_to_base_input_0[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_dwg_to_base_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dwg_to_base_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_dwg_to_base{
  .enabled = true,
  .inputTypes = kStrictSlots_dwg_to_base_input,
};

const AssignmentCheck kAssign_dwg_to_base{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dwg_to_base",
};
const Slot kInside_dwg_to_base[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dwg_to_base"}},
};
const TableValidator kTable_dwg_to_base{
  .assignment = &kAssign_dwg_to_base,
  .tooFewInside = "dwg_to_base requires 1 parameter inside parentheses",
  .inside = kInside_dwg_to_base,
};

const std::string_view kStrictTypes_dwg_to_mm_input_0[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_dwg_to_mm_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_dwg_to_mm_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_dwg_to_mm{
  .enabled = true,
  .inputTypes = kStrictSlots_dwg_to_mm_input,
};

const AssignmentCheck kAssign_dwg_to_mm{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in dwg_to_mm",
};
const Slot kInside_dwg_to_mm[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in dwg_to_mm"}},
};
const TableValidator kTable_dwg_to_mm{
  .assignment = &kAssign_dwg_to_mm,
  .tooFewInside = "dwg_to_mm requires 1 parameter inside parentheses",
  .inside = kInside_dwg_to_mm,
};

const Slot kInside_eof[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in eof"}},
};
const TableValidator kTable_eof{
  .tooFewInside = "eof requires 1 parameter inside parentheses",
  .inside = kInside_eof,
};

const Slot kAfter_exit[] = {
  {.index = 0, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::AnyVariable, .required = false, .invalid = std::string_view{"Parameter must be a valid string or variable in exit"}},
};
const TableValidator kTable_exit{
  .tooManyAfter = "exit takes at most 1 parameter",
  .after = kAfter_exit,
};

const std::string_view kStrictTypes_filecopy_after_0[] = {
  std::string_view{"string"},
};
const std::string_view kStrictTypes_filecopy_after_1[] = {
  std::string_view{"string"},
};
const StrictTypeSlot kStrictSlots_filecopy_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_filecopy_after_0,
    .paramName = std::string_view{"source path"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_filecopy_after_1,
    .paramName = std::string_view{"destination path"},
  },
};
const StrictMode kStrict_filecopy{
  .enabled = true,
  .afterParamTypes = kStrictSlots_filecopy_after,
};

const Slot kAfter_filecopy[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string in filecopy"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in filecopy"}},
};
const TableValidator kTable_filecopy{
  .tooFewAfter = "filecopy requires 2 parameters outside parentheses",
  .after = kAfter_filecopy,
};

const AssignmentCheck kAssign_filepath_custom{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "filepath_custom requires an assignment variable",
  .invalid = "Assignment variable must be a valid string variable in filepath_custom",
};
const Slot kInside_filepath_custom[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in filepath_custom"}},
};
const TableValidator kTable_filepath_custom{
  .assignment = &kAssign_filepath_custom,
  .tooFewInside = "filepath_custom requires 1 parameter inside parentheses",
  .inside = kInside_filepath_custom,
};

const AssignmentCheck kAssign_findstr{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in findstr",
};
const Slot kInside_findstr[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string in findstr"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in findstr"}},
};
const TableValidator kTable_findstr{
  .assignment = &kAssign_findstr,
  .tooFewInside = "findstr requires 2 parameters",
  .inside = kInside_findstr,
};

const AssignmentCheck kAssign_findstri{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in findstri",
};
const Slot kInside_findstri[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string in findstri"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in findstri"}},
};
const TableValidator kTable_findstri{
  .assignment = &kAssign_findstri,
  .tooFewInside = "findstri requires 2 parameters",
  .inside = kInside_findstri,
};

const std::string_view kStrictTypes_findstrr_input_0[] = {
  std::string_view{"string"},
};
const std::string_view kStrictTypes_findstrr_input_1[] = {
  std::string_view{"string"},
};
const StrictTypeSlot kStrictSlots_findstrr_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_findstrr_input_0,
    .paramName = std::string_view{"input string"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_findstrr_input_1,
    .paramName = std::string_view{"search string"},
  },
};
const StrictMode kStrict_findstrr{
  .enabled = true,
  .inputTypes = kStrictSlots_findstrr_input,
};

const AssignmentCheck kAssign_findstrr{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in findstrr",
};
const Slot kInside_findstrr[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in findstrr"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in findstrr"}},
};
const TableValidator kTable_findstrr{
  .assignment = &kAssign_findstrr,
  .tooFewInside = "findstrr requires 2 parameters inside parentheses",
  .inside = kInside_findstrr,
};

const std::string_view kStrictTypes_findstrri_input_0[] = {
  std::string_view{"string"},
};
const std::string_view kStrictTypes_findstrri_input_1[] = {
  std::string_view{"string"},
};
const StrictTypeSlot kStrictSlots_findstrri_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_findstrri_input_0,
    .paramName = std::string_view{"input string"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_findstrri_input_1,
    .paramName = std::string_view{"search string"},
  },
};
const StrictMode kStrict_findstrri{
  .enabled = true,
  .inputTypes = kStrictSlots_findstrri_input,
};

const AssignmentCheck kAssign_findstrri{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in findstrri",
};
const Slot kInside_findstrri[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in findstrri"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in findstrri"}},
};
const TableValidator kTable_findstrri{
  .assignment = &kAssign_findstrri,
  .tooFewInside = "findstrri requires 2 parameters inside parentheses",
  .inside = kInside_findstrri,
};

const AssignmentCheck kAssign_get{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable or string in get",
};
const Slot kInside_get[] = {
  {.index = 0, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::NormalVariable, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string or variable in get"}},
};
const TableValidator kTable_get{
  .assignment = &kAssign_get,
  .tooFewInside = "get requires 1 parameter inside parentheses",
  .inside = kInside_get,
};

const std::string_view kStrictTypes_get_actual_area_input_0[] = {
  std::string_view{"slope"},
  std::string_view{"edge"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_get_actual_area_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_actual_area_input_0,
    .paramName = std::string_view{"input slope or edge"},
  },
};
const StrictMode kStrict_get_actual_area{
  .enabled = true,
  .inputTypes = kStrictSlots_get_actual_area_input,
};

const AssignmentCheck kAssign_get_actual_area{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_actual_area requires an assignment variable",
  .invalid = "Assignment variable must be a valid variable in get_actual_area",
};
const Slot kInside_get_actual_area[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_actual_area"}},
};
const TableValidator kTable_get_actual_area{
  .assignment = &kAssign_get_actual_area,
  .tooFewInside = "get_actual_area requires 1 parameter inside parentheses",
  .inside = kInside_get_actual_area,
};

const std::string_view kStrictTypes_get_actual_pcend_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_get_actual_pcend_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_actual_pcend_input_0,
    .paramName = std::string_view{"input profile"},
  },
};
const StrictMode kStrict_get_actual_pcend{
  .enabled = true,
  .inputTypes = kStrictSlots_get_actual_pcend_input,
};

const AssignmentCheck kAssign_get_actual_pcend{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_actual_pcend requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in get_actual_pcend",
};
const Slot kInside_get_actual_pcend[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in get_actual_pcend"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid variable in get_actual_pcend"}},
};
const TableValidator kTable_get_actual_pcend{
  .assignment = &kAssign_get_actual_pcend,
  .tooFewInside = "get_actual_pcend requires 2 parameters",
  .inside = kInside_get_actual_pcend,
};

const std::string_view kStrictTypes_get_area_clips_with_edges_input_0[] = {
  std::string_view{"area"},
};
const std::string_view kStrictTypes_get_area_clips_with_edges_input_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_get_area_clips_with_edges_input_2[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const StrictTypeSlot kStrictSlots_get_area_clips_with_edges_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_area_clips_with_edges_input_0,
    .paramName = std::string_view{"input area"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_get_area_clips_with_edges_input_1,
    .paramName = std::string_view{"input position vector"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_get_area_clips_with_edges_input_2,
    .paramName = std::string_view{"input direction vector"},
  },
};
const StrictMode kStrict_get_area_clips_with_edges{
  .enabled = true,
  .inputTypes = kStrictSlots_get_area_clips_with_edges_input,
};

const AssignmentCheck kAssign_get_area_clips_with_edges{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_area_clips_with_edges requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in get_area_clips_with_edges",
};
const Slot kInside_get_area_clips_with_edges[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in get_area_clips_with_edges"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in get_area_clips_with_edges"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in get_area_clips_with_edges"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in get_area_clips_with_edges"}},
};
const TableValidator kTable_get_area_clips_with_edges{
  .assignment = &kAssign_get_area_clips_with_edges,
  .tooFewInside = "get_area_clips_with_edges requires 4 parameters inside parentheses",
  .inside = kInside_get_area_clips_with_edges,
};

const std::string_view kStrictTypes_get_area_param_input_0[] = {
  std::string_view{"area"},
  std::string_view{"array"},
  std::string_view{"edge"},
};
const StrictTypeSlot kStrictSlots_get_area_param_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_area_param_input_0,
    .paramName = std::string_view{"input area"},
  },
};
const StrictMode kStrict_get_area_param{
  .enabled = true,
  .inputTypes = kStrictSlots_get_area_param_input,
};

const std::string_view kStrictTypes_get_area_plane_input_0[] = {
  std::string_view{"area"},
};
const StrictTypeSlot kStrictSlots_get_area_plane_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_area_plane_input_0,
    .paramName = std::string_view{"input area"},
  },
};
const std::string_view kStrictTypes_get_area_plane_after_0[] = {
  std::string_view{"string"},
};
const StrictTypeSlot kStrictSlots_get_area_plane_after[] = {
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_get_area_plane_after_0,
    .paramName = std::string_view{"input string"},
  },
};
const StrictMode kStrict_get_area_plane{
  .enabled = true,
  .inputTypes = kStrictSlots_get_area_plane_input,
  .afterParamTypes = kStrictSlots_get_area_plane_after,
};

const Slot kInside_get_area_plane[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_area_plane"}},
};
const Slot kAfter_get_area_plane[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in get_area_plane"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in get_area_plane"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter after parentheses must be a valid string in get_area_plane"}},
};
const TableValidator kTable_get_area_plane{
  .tooFewInside = "get_area_plane requires 1 parameter inside parentheses",
  .tooFewAfter = "get_area_plane requires 3 parameters after parentheses",
  .inside = kInside_get_area_plane,
  .after = kAfter_get_area_plane,
};

const std::string_view kStrictTypes_get_ctr_edge_input_0[] = {
  std::string_view{"area"},
  std::string_view{"edge"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_get_ctr_edge_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_ctr_edge_input_0,
    .paramName = std::string_view{"input area or edge"},
  },
};
const StrictMode kStrict_get_ctr_edge{
  .enabled = true,
  .inputTypes = kStrictSlots_get_ctr_edge_input,
};

const AssignmentCheck kAssign_get_ctr_edge{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in get_ctr_edge",
};
const Slot kInside_get_ctr_edge[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in get_ctr_edge"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in get_ctr_edge"}},
};
const TableValidator kTable_get_ctr_edge{
  .assignment = &kAssign_get_ctr_edge,
  .tooFewInside = "get_ctr_edge requires 2 parameters inside parentheses",
  .inside = kInside_get_ctr_edge,
};

const std::string_view kStrictTypes_get_floor_level_name_input_0[] = {
  std::string_view{"area"},
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"selemobj"},
  std::string_view{"edge"},
  std::string_view{"wall"},
  std::string_view{"array"},
  std::string_view{"macro"},
};
const StrictTypeSlot kStrictSlots_get_floor_level_name_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_floor_level_name_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_get_floor_level_name{
  .enabled = true,
  .inputTypes = kStrictSlots_get_floor_level_name_input,
};

const AssignmentCheck kAssign_get_floor_level_name{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "get_floor_level_name requires an assignment variable",
  .invalid = "Assignment variable must be a string variable in get_floor_level_name",
};
const Slot kInside_get_floor_level_name[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in get_floor_level_name"}},
};
const TableValidator kTable_get_floor_level_name{
  .assignment = &kAssign_get_floor_level_name,
  .tooFewInside = "get_floor_level_name requires 1 parameter",
  .inside = kInside_get_floor_level_name,
};

const std::string_view kStrictTypes_get_joint_param_input_0[] = {
  std::string_view{"joint"},
};
const StrictTypeSlot kStrictSlots_get_joint_param_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_joint_param_input_0,
    .paramName = std::string_view{"input joint"},
  },
};
const StrictMode kStrict_get_joint_param{
  .enabled = true,
  .inputTypes = kStrictSlots_get_joint_param_input,
};

const AssignmentCheck kAssign_get_joint_param{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .missing = "get_joint_param requires an assignment variable",
  .invalid = "Assignment variable must be a valid variable in get_joint_param",
};
const Slot kInside_get_joint_param[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in get_joint_param"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in get_joint_param"}},
};
const TableValidator kTable_get_joint_param{
  .assignment = &kAssign_get_joint_param,
  .tooFewInside = "get_joint_param requires 2 parameters",
  .inside = kInside_get_joint_param,
};

const std::string_view kStrictTypes_get_macro_param_input_0[] = {
  std::string_view{"macro"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_get_macro_param_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_macro_param_input_0,
    .paramName = std::string_view{"input macro"},
  },
};
const StrictMode kStrict_get_macro_param{
  .enabled = true,
  .inputTypes = kStrictSlots_get_macro_param_input,
};

const AssignmentCheck kAssign_get_macro_param{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable or string variabble in get_macro_param",
};
const Slot kInside_get_macro_param[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in get_macro_param"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in get_macro_param"}},
};
const TableValidator kTable_get_macro_param{
  .assignment = &kAssign_get_macro_param,
  .tooFewInside = "get_macro_param requires 2 parameters inside parentheses",
  .inside = kInside_get_macro_param,
};

const std::string_view kStrictTypes_get_material_param_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_get_material_param_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_material_param_input_0,
    .paramName = std::string_view{"input profile"},
  },
};
const StrictMode kStrict_get_material_param{
  .enabled = true,
  .inputTypes = kStrictSlots_get_material_param_input,
};

const AssignmentCheck kAssign_get_material_param{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "get_material_param requires an assignment variable",
  .invalid = "Assignment variable must be a string variable in get_material_param",
};
const Slot kInside_get_material_param[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in get_material_param"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in get_material_param"}},
};
const TableValidator kTable_get_material_param{
  .assignment = &kAssign_get_material_param,
  .tooFewInside = "get_material_param requires 2 parameters",
  .inside = kInside_get_material_param,
};

const AssignmentCheck kAssign_get_obj_param{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .missing = "get_obj_param requires an assignment variable",
  .invalid = "Invalid assignment variable in get_obj_param",
};
const Slot kInside_get_obj_param[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in get_obj_param"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in get_obj_param"}},
  {.index = 2, .kind = SlotKind::DigitLiteralOrVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter must be a number or variable in get_obj_param"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter must be a valid variable in get_obj_param"}},
};
const TableValidator kTable_get_obj_param{
  .assignment = &kAssign_get_obj_param,
  .tooFewInside = "get_obj_param requires at least 2 parameters",
  .inside = kInside_get_obj_param,
};

const std::string_view kStrictTypes_get_obj_size_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"area"},
  std::string_view{"point"},
  std::string_view{"wall"},
  std::string_view{"array"},
  std::string_view{"macro"},
  std::string_view{"slope"},
};
const std::string_view kStrictTypes_get_obj_size_input_1[] = {
  std::string_view{"pvector"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_get_obj_size_input_2[] = {
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
};
const std::string_view kStrictTypes_get_obj_size_input_3[] = {
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
};
const std::string_view kStrictTypes_get_obj_size_input_4[] = {
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
};
const StrictTypeSlot kStrictSlots_get_obj_size_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_obj_size_input_0,
    .paramName = std::string_view{"input object"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_get_obj_size_input_1,
    .paramName = std::string_view{"input point vector"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_get_obj_size_input_2,
    .paramName = std::string_view{"input x vector"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_get_obj_size_input_3,
    .paramName = std::string_view{"input y vector"},
  },
  {
    .paramIndex = 6,
    .allowedTypes = kStrictTypes_get_obj_size_input_4,
    .paramName = std::string_view{"input z vector"},
  },
};
const StrictMode kStrict_get_obj_size{
  .enabled = true,
  .inputTypes = kStrictSlots_get_obj_size_input,
};

const AssignmentCheck kAssign_get_obj_size{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in get_obj_size",
};
const Slot kInside_get_obj_size[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid variable in get_obj_size"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 must be a valid variable in get_obj_size"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 must be a valid variable in get_obj_size"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 must be a valid variable in get_obj_size"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 5 must be a valid variable in get_obj_size"}},
  {.index = 5, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 6 must be a valid variable in get_obj_size"}},
  {.index = 6, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 7 must be a valid variable in get_obj_size"}},
};
const TableValidator kTable_get_obj_size{
  .assignment = &kAssign_get_obj_size,
  .tooFewInside = "get_obj_size requires 7 parameters",
  .inside = kInside_get_obj_size,
};

const AssignmentCheck kAssign_get_object_type{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a string variable in get_object_type",
};
const Slot kInside_get_object_type[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in get_object_type"}},
};
const TableValidator kTable_get_object_type{
  .assignment = &kAssign_get_object_type,
  .tooFewInside = "get_object_type requires 1 parameter",
  .inside = kInside_get_object_type,
};

const std::string_view kStrictTypes_get_origin_input_0[] = {
  std::string_view{"area"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_get_origin_input_1[] = {
  std::string_view{"string"},
};
const std::string_view kStrictTypes_get_origin_input_2[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_get_origin_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_origin_input_0,
    .paramName = std::string_view{"input area"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_get_origin_input_1,
    .paramName = std::string_view{"input string"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_get_origin_input_2,
    .paramName = std::string_view{"index"},
  },
};
const StrictMode kStrict_get_origin{
  .enabled = true,
  .inputTypes = kStrictSlots_get_origin_input,
};

const AssignmentCheck kAssign_get_origin{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_origin requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in get_origin",
};
const Slot kInside_get_origin[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in get_origin"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in get_origin"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter must be a valid variable in get_origin"}},
};
const TableValidator kTable_get_origin{
  .assignment = &kAssign_get_origin,
  .tooFewInside = "get_origin requires 3 parameters",
  .inside = kInside_get_origin,
};

const std::string_view kStrictTypes_get_origin_pnt_input_0[] = {
  std::string_view{"profileend"},
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"joint"},
  std::string_view{"selemobj"},
  std::string_view{"point"},
  std::string_view{"edge"},
  std::string_view{"wall"},
  std::string_view{"array"},
  std::string_view{"macro"},
};
const StrictTypeSlot kStrictSlots_get_origin_pnt_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_origin_pnt_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_get_origin_pnt{
  .enabled = true,
  .inputTypes = kStrictSlots_get_origin_pnt_input,
};

const Slot kInside_get_origin_pnt[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_origin_pnt"}},
};
const Slot kAfter_get_origin_pnt[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid variable in get_origin_pnt"}},
};
const TableValidator kTable_get_origin_pnt{
  .tooFewInside = "get_origin_pnt requires 1 parameter inside parentheses",
  .tooFewAfter = "get_origin_pnt requires 1 parameter after parentheses",
  .inside = kInside_get_origin_pnt,
  .after = kAfter_get_origin_pnt,
};

const std::string_view kStrictTypes_get_origin_pos_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"joint"},
  std::string_view{"selemobj"},
  std::string_view{"point"},
  std::string_view{"edge"},
  std::string_view{"wall"},
  std::string_view{"array"},
  std::string_view{"macro"},
  std::string_view{"profileend"},
};
const StrictTypeSlot kStrictSlots_get_origin_pos_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_origin_pos_input_0,
    .paramName = std::string_view{"object"},
  },
};
const StrictMode kStrict_get_origin_pos{
  .enabled = true,
  .inputTypes = kStrictSlots_get_origin_pos_input,
};

const Slot kInside_get_origin_pos[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_origin_pos"}},
};
const Slot kAfter_get_origin_pos[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 after parentheses must be a valid variable in get_origin_pos"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 after parentheses must be a valid variable in get_origin_pos"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 after parentheses must be a valid variable in get_origin_pos"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 after parentheses must be a valid variable in get_origin_pos"}},
};
const TableValidator kTable_get_origin_pos{
  .tooFewInside = "get_origin_pos requires 1 parameter inside parentheses",
  .tooFewAfter = "get_origin_pos requires 4 parameters after parentheses",
  .inside = kInside_get_origin_pos,
  .after = kAfter_get_origin_pos,
};

const std::string_view kStrictTypes_get_panel_input_0[] = {
  std::string_view{"slope"},
  std::string_view{"wall"},
  std::string_view{"area"},
};
const StrictTypeSlot kStrictSlots_get_panel_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_panel_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_get_panel{
  .enabled = true,
  .inputTypes = kStrictSlots_get_panel_input,
};

const AssignmentCheck kAssign_get_panel{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_panel requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in get_panel",
};
const Slot kInside_get_panel[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_panel"}},
};
const TableValidator kTable_get_panel{
  .assignment = &kAssign_get_panel,
  .tooFewInside = "get_panel requires 1 parameter inside parentheses",
  .inside = kInside_get_panel,
};

const std::string_view kStrictTypes_get_panel_core_input_0[] = {
  std::string_view{"selemobj"},
  std::string_view{"area"},
  std::string_view{"wall"},
  std::string_view{"slope"},
};
const StrictTypeSlot kStrictSlots_get_panel_core_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_panel_core_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_get_panel_core{
  .enabled = true,
  .inputTypes = kStrictSlots_get_panel_core_input,
};

const AssignmentCheck kAssign_get_panel_core{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_panel_core requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in get_panel_core",
};
const Slot kInside_get_panel_core[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in get_panel_core"}},
};
const TableValidator kTable_get_panel_core{
  .assignment = &kAssign_get_panel_core,
  .tooFewInside = "get_panel_core requires 1 parameter",
  .inside = kInside_get_panel_core,
};

const std::string_view kStrictTypes_get_parent_input_0[] = {
  std::string_view{"slope"},
  std::string_view{"area"},
  std::string_view{"profileend"},
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"selemobj"},
  std::string_view{"edge"},
  std::string_view{"wall"},
  std::string_view{"array"},
  std::string_view{"macro"},
};
const StrictTypeSlot kStrictSlots_get_parent_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_parent_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_get_parent{
  .enabled = true,
  .inputTypes = kStrictSlots_get_parent_input,
};

const AssignmentCheck kAssign_get_parent{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_parent requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in get_parent",
};
const Slot kInside_get_parent[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in get_parent"}},
};
const TableValidator kTable_get_parent{
  .assignment = &kAssign_get_parent,
  .tooFewInside = "get_parent requires 1 parameter",
  .inside = kInside_get_parent,
};

const std::string_view kStrictTypes_get_piece_dir_after_0[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_get_piece_dir_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_piece_dir_after_0,
    .paramName = std::string_view{"input profile"},
  },
};
const StrictMode kStrict_get_piece_dir{
  .enabled = true,
  .afterParamTypes = kStrictSlots_get_piece_dir_after,
};

const Slot kInside_get_piece_dir[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_piece_dir"}},
};
const Slot kAfter_get_piece_dir[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid variable in get_piece_dir"}},
};
const TableValidator kTable_get_piece_dir{
  .tooFewInside = "get_piece_dir requires 1 parameter inside parentheses",
  .tooFewAfter = "get_piece_dir requires 1 parameter after parentheses",
  .inside = kInside_get_piece_dir,
  .after = kAfter_get_piece_dir,
};

const std::string_view kStrictTypes_get_piece_end_trims_input_0[] = {
  std::string_view{"profileend"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_get_piece_end_trims_input_1[] = {
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_get_piece_end_trims_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_piece_end_trims_input_0,
    .paramName = std::string_view{"input profile"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_get_piece_end_trims_input_1,
    .paramName = std::string_view{"input position vector"},
  },
};
const StrictMode kStrict_get_piece_end_trims{
  .enabled = true,
  .inputTypes = kStrictSlots_get_piece_end_trims_input,
};

const AssignmentCheck kAssign_get_piece_end_trims{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_piece_end_trims requires an assignment variable",
  .invalid = "Assignment variable must be a valid variable in get_piece_end_trims",
};
const Slot kInside_get_piece_end_trims[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in get_piece_end_trims"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in get_piece_end_trims"}},
};
const TableValidator kTable_get_piece_end_trims{
  .assignment = &kAssign_get_piece_end_trims,
  .tooFewInside = "get_piece_end_trims requires 2 parameters inside parentheses",
  .inside = kInside_get_piece_end_trims,
};

const std::string_view kStrictTypes_get_piece_tan_after_0[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_get_piece_tan_after_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_get_piece_tan_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_piece_tan_after_0,
    .paramName = std::string_view{"input profile"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_get_piece_tan_after_1,
    .paramName = std::string_view{"output direction vector"},
  },
};
const StrictMode kStrict_get_piece_tan{
  .enabled = true,
  .afterParamTypes = kStrictSlots_get_piece_tan_after,
};

const Slot kInside_get_piece_tan[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in get_piece_tan"}},
};
const Slot kAfter_get_piece_tan[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"first parameter outside parentheses must be a valid variable in get_piece_tan"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"second parameter outside parentheses must be a valid variable in get_piece_tan"}},
};
const TableValidator kTable_get_piece_tan{
  .tooFewInside = "get_piece_tan requires 1 parameter inside parentheses",
  .tooManyInside = "get_piece_tan requires 1 parameter inside parentheses",
  .inside = kInside_get_piece_tan,
  .after = kAfter_get_piece_tan,
};

const std::string_view kStrictTypes_get_poly_ctr_input_0[] = {
  std::string_view{"polyline"},
};
const std::string_view kStrictTypes_get_poly_ctr_input_1[] = {
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_get_poly_ctr_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_poly_ctr_input_0,
    .paramName = std::string_view{"polyline"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_get_poly_ctr_input_1,
    .paramName = std::string_view{"pvector"},
  },
};
const StrictMode kStrict_get_poly_ctr{
  .enabled = true,
  .inputTypes = kStrictSlots_get_poly_ctr_input,
};

const AssignmentCheck kAssign_get_poly_ctr{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Invalid assignment variable in get_poly_ctr",
};
const Slot kInside_get_poly_ctr[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in get_poly_ctr"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid variable in get_poly_ctr"}},
};
const TableValidator kTable_get_poly_ctr{
  .assignment = &kAssign_get_poly_ctr,
  .tooFewInside = "get_poly_ctr requires 2 parameters",
  .inside = kInside_get_poly_ctr,
};

const std::string_view kStrictTypes_get_poly_lim_input_0[] = {
  std::string_view{"polyline"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_get_poly_lim_input_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const StrictTypeSlot kStrictSlots_get_poly_lim_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_poly_lim_input_0,
    .paramName = std::string_view{"input polygon"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_get_poly_lim_input_1,
    .paramName = std::string_view{"input direction vector"},
  },
};
const StrictMode kStrict_get_poly_lim{
  .enabled = true,
  .inputTypes = kStrictSlots_get_poly_lim_input,
};

const AssignmentCheck kAssign_get_poly_lim{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in get_poly_lim",
};
const Slot kInside_get_poly_lim[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in get_poly_lim"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in get_poly_lim"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in get_poly_lim"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in get_poly_lim"}},
};
const TableValidator kTable_get_poly_lim{
  .assignment = &kAssign_get_poly_lim,
  .tooFewInside = "get_poly_lim requires 4 parameters inside parentheses",
  .inside = kInside_get_poly_lim,
};

const std::string_view kStrictTypes_get_section_param_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_get_section_param_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_section_param_input_0,
    .paramName = std::string_view{"input profile"},
  },
};
const StrictMode kStrict_get_section_param{
  .enabled = true,
  .inputTypes = kStrictSlots_get_section_param_input,
};

const AssignmentCheck kAssign_get_section_param{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable or string variable in get_section_param",
};
const Slot kInside_get_section_param[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in get_section_param"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in get_section_param"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter must be a valid variable in get_section_param"}},
};
const TableValidator kTable_get_section_param{
  .assignment = &kAssign_get_section_param,
  .tooFewInside = "get_section_param requires at least 2 parameters",
  .inside = kInside_get_section_param,
};

const std::string_view kStrictTypes_get_sheet_ctr_input_0[] = {
  std::string_view{"sheet"},
};
const std::string_view kStrictTypes_get_sheet_ctr_input_1[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_get_sheet_ctr_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_sheet_ctr_input_0,
    .paramName = std::string_view{"input sheet"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_get_sheet_ctr_input_1,
    .paramName = std::string_view{"center type"},
  },
};
const StrictMode kStrict_get_sheet_ctr{
  .enabled = true,
  .inputTypes = kStrictSlots_get_sheet_ctr_input,
};

const AssignmentCheck kAssign_get_sheet_ctr{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_sheet_ctr requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in get_sheet_ctr",
};
const Slot kInside_get_sheet_ctr[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid variable in get_sheet_ctr"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 must be a valid variable in get_sheet_ctr"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 must be a valid variable in get_sheet_ctr"}},
};
const TableValidator kTable_get_sheet_ctr{
  .assignment = &kAssign_get_sheet_ctr,
  .tooFewInside = "get_sheet_ctr requires 3 parameters",
  .inside = kInside_get_sheet_ctr,
};

const std::string_view kStrictTypes_get_sheet_type_input_0[] = {
  std::string_view{"sheet"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_get_sheet_type_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_sheet_type_input_0,
    .paramName = std::string_view{"input sheet"},
  },
};
const StrictMode kStrict_get_sheet_type{
  .enabled = true,
  .inputTypes = kStrictSlots_get_sheet_type_input,
};

const AssignmentCheck kAssign_get_sheet_type{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in get_sheet_type",
};
const Slot kInside_get_sheet_type[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_sheet_type"}},
};
const TableValidator kTable_get_sheet_type{
  .assignment = &kAssign_get_sheet_type,
  .tooFewInside = "get_sheet_type requires 1 parameter inside parentheses",
  .inside = kInside_get_sheet_type,
};

const std::string_view kStrictTypes_get_struct_core_input_0[] = {
  std::string_view{"slope"},
  std::string_view{"selemobj"},
  std::string_view{"area"},
  std::string_view{"edge"},
};
const StrictTypeSlot kStrictSlots_get_struct_core_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_struct_core_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_get_struct_core{
  .enabled = true,
  .inputTypes = kStrictSlots_get_struct_core_input,
};

const AssignmentCheck kAssign_get_struct_core{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_struct_core requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in get_struct_core",
};
const Slot kInside_get_struct_core[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in get_struct_core"}},
};
const TableValidator kTable_get_struct_core{
  .assignment = &kAssign_get_struct_core,
  .tooFewInside = "get_struct_core requires 1 parameter",
  .inside = kInside_get_struct_core,
};

const std::string_view kStrictTypes_get_vxgeom_after_0[] = {
  std::string_view{"profile"},
  std::string_view{"wall"},
  std::string_view{"array"},
  std::string_view{"edge"},
};
const StrictTypeSlot kStrictSlots_get_vxgeom_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_vxgeom_after_0,
    .paramName = std::string_view{"object to get vxgeom off"},
  },
};
const StrictMode kStrict_get_vxgeom{
  .enabled = true,
  .afterParamTypes = kStrictSlots_get_vxgeom_after,
};

const Slot kInside_get_vxgeom[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_vxgeom"}},
};
const Slot kAfter_get_vxgeom[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid variable in get_vxgeom"}},
};
const TableValidator kTable_get_vxgeom{
  .tooFewInside = "get_vxgeom requires 1 parameter inside parentheses",
  .tooFewAfter = "get_vxgeom requires 1 parameter after parentheses",
  .inside = kInside_get_vxgeom,
  .after = kAfter_get_vxgeom,
};

const std::string_view kStrictTypes_get_vxgeom_len_input_0[] = {
  std::string_view{"vxgeom"},
};
const StrictTypeSlot kStrictSlots_get_vxgeom_len_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_vxgeom_len_input_0,
    .paramName = std::string_view{"vxgeom"},
  },
};
const StrictMode kStrict_get_vxgeom_len{
  .enabled = true,
  .inputTypes = kStrictSlots_get_vxgeom_len_input,
};

const AssignmentCheck kAssign_get_vxgeom_len{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in get_vxgeom_len",
};
const Slot kInside_get_vxgeom_len[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in get_vxgeom_len"}},
};
const TableValidator kTable_get_vxgeom_len{
  .assignment = &kAssign_get_vxgeom_len,
  .tooFewInside = "get_vxgeom_len requires 1 parameter",
  .inside = kInside_get_vxgeom_len,
};

const std::string_view kStrictTypes_get_vxgeom_point_after_0[] = {
  std::string_view{"vxgeom"},
};
const std::string_view kStrictTypes_get_vxgeom_point_after_1[] = {
  std::string_view{"string"},
};
const StrictTypeSlot kStrictSlots_get_vxgeom_point_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_vxgeom_point_after_0,
    .paramName = std::string_view{"vxgeom"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_get_vxgeom_point_after_1,
    .paramName = std::string_view{"point on vxgeom to get"},
  },
};
const StrictMode kStrict_get_vxgeom_point{
  .enabled = true,
  .afterParamTypes = kStrictSlots_get_vxgeom_point_after,
};

const Slot kInside_get_vxgeom_point[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_vxgeom_point"}},
};
const Slot kAfter_get_vxgeom_point[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in get_vxgeom_point"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid string in get_vxgeom_point"}},
};
const TableValidator kTable_get_vxgeom_point{
  .tooFewInside = "get_vxgeom_point requires 1 parameter inside parentheses",
  .tooFewAfter = "get_vxgeom_point requires 2 parameters after parentheses",
  .inside = kInside_get_vxgeom_point,
  .after = kAfter_get_vxgeom_point,
};

const std::string_view kStrictTypes_get_vxgeom_tan_after_0[] = {
  std::string_view{"vxgeom"},
};
const std::string_view kStrictTypes_get_vxgeom_tan_after_1[] = {
  std::string_view{"pvector"},
  std::string_view{"dvector"},
};
const StrictTypeSlot kStrictSlots_get_vxgeom_tan_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_vxgeom_tan_after_0,
    .paramName = std::string_view{"vxgeom"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_get_vxgeom_tan_after_1,
    .paramName = std::string_view{"point on vxgeom to get tangent at"},
  },
};
const StrictMode kStrict_get_vxgeom_tan{
  .enabled = true,
  .afterParamTypes = kStrictSlots_get_vxgeom_tan_after,
};

const Slot kInside_get_vxgeom_tan[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_vxgeom_tan"}},
};
const Slot kAfter_get_vxgeom_tan[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in get_vxgeom_tan"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in get_vxgeom_tan"}},
};
const TableValidator kTable_get_vxgeom_tan{
  .tooFewInside = "get_vxgeom_tan requires 1 parameter inside parentheses",
  .tooFewAfter = "get_vxgeom_tan requires 2 parameters after parentheses",
  .inside = kInside_get_vxgeom_tan,
  .after = kAfter_get_vxgeom_tan,
};

const std::string_view kStrictTypes_get_vxtype_main_input_0[] = {
  std::string_view{"macro"},
  std::string_view{"array"},
  std::string_view{"profile"},
  std::string_view{"area"},
  std::string_view{"slope"},
  std::string_view{"selemobj"},
  std::string_view{"sheet"},
  std::string_view{"profileend"},
  std::string_view{"joint"},
  std::string_view{"wall"},
};
const StrictTypeSlot kStrictSlots_get_vxtype_main_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_vxtype_main_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_get_vxtype_main{
  .enabled = true,
  .inputTypes = kStrictSlots_get_vxtype_main_input,
};

const AssignmentCheck kAssign_get_vxtype_main{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "get_vxtype_main requires an assignment variable",
  .invalid = "Assignment variable must be a valid string variable in get_vxtype_main",
};
const Slot kInside_get_vxtype_main[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_vxtype_main"}},
};
const TableValidator kTable_get_vxtype_main{
  .assignment = &kAssign_get_vxtype_main,
  .tooFewInside = "get_vxtype_main requires 1 parameter inside parentheses",
  .inside = kInside_get_vxtype_main,
};

const std::string_view kStrictTypes_get_wall_from_wpnl_input_0[] = {
  std::string_view{"selemobj"},
};
const StrictTypeSlot kStrictSlots_get_wall_from_wpnl_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_wall_from_wpnl_input_0,
    .paramName = std::string_view{"input wall panel"},
  },
};
const StrictMode kStrict_get_wall_from_wpnl{
  .enabled = true,
  .inputTypes = kStrictSlots_get_wall_from_wpnl_input,
};

const AssignmentCheck kAssign_get_wall_from_wpnl{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "get_wall_from_wpnl requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in get_wall_from_wpnl",
};
const Slot kInside_get_wall_from_wpnl[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_wall_from_wpnl"}},
};
const TableValidator kTable_get_wall_from_wpnl{
  .assignment = &kAssign_get_wall_from_wpnl,
  .tooFewInside = "get_wall_from_wpnl requires 1 parameter inside parentheses",
  .inside = kInside_get_wall_from_wpnl,
};

const std::string_view kStrictTypes_get_wall_type_input_0[] = {
  std::string_view{"wall"},
};
const StrictTypeSlot kStrictSlots_get_wall_type_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_get_wall_type_input_0,
    .paramName = std::string_view{"input wall"},
  },
};
const StrictMode kStrict_get_wall_type{
  .enabled = true,
  .inputTypes = kStrictSlots_get_wall_type_input,
};

const AssignmentCheck kAssign_get_wall_type{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "get_wall_type requires an assignment variable",
  .invalid = "Assignment variable must be a string variable in get_wall_type",
};
const Slot kInside_get_wall_type[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in get_wall_type"}},
};
const TableValidator kTable_get_wall_type{
  .assignment = &kAssign_get_wall_type,
  .tooFewInside = "get_wall_type requires 1 parameter inside parentheses",
  .inside = kInside_get_wall_type,
};

const std::string_view kStrictTypes_inch_to_dwg_input_0[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_inch_to_dwg_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_inch_to_dwg_input_0,
    .paramName = std::string_view{"input value"},
  },
};
const StrictMode kStrict_inch_to_dwg{
  .enabled = true,
  .inputTypes = kStrictSlots_inch_to_dwg_input,
};

const AssignmentCheck kAssign_inch_to_dwg{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in inch_to_dwg",
};
const Slot kInside_inch_to_dwg[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in inch_to_dwg"}},
};
const TableValidator kTable_inch_to_dwg{
  .assignment = &kAssign_inch_to_dwg,
  .tooFewInside = "inch_to_dwg requires 1 parameter inside parentheses",
  .inside = kInside_inch_to_dwg,
};

const std::string_view kStrictTypes_init_iterator_after_0[] = {
  std::string_view{"value"},
  std::string_view{"macro"},
  std::string_view{"selemobj"},
  std::string_view{"array"},
  std::string_view{"wall"},
  std::string_view{"edge"},
  std::string_view{"sheet"},
  std::string_view{"profile"},
  std::string_view{"slope"},
  std::string_view{"area"},
  std::string_view{"profileend"},
  std::string_view{"joint"},
};
const StrictTypeSlot kStrictSlots_init_iterator_after[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_init_iterator_after_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_init_iterator{
  .enabled = true,
  .afterParamTypes = kStrictSlots_init_iterator_after,
};

const Slot kInside_init_iterator[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in init_iterator"}},
};
const SlotTail kAfterTail_init_iterator{.from = 0, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::NormalVariable, .invalid = std::string_view{"Parameter {n} after parentheses must be a valid variable or string in init_iterator"}};
const TableValidator kTable_init_iterator{
  .tooManyInside = "init_iterator allows at most 1 parameter inside parentheses",
  .tooManyAfter = "init_iterator allows at most 4 parameters after parentheses",
  .inside = kInside_init_iterator,
  .afterTail = &kAfterTail_init_iterator,
};

const AssignmentCheck kAssign_int{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in int",
};
const TableValidator kTable_int{
  .assignment = &kAssign_int,
  .tooFewInside = "int requires 1 parameter",
};

const AssignmentCheck kAssign_int_part{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in int_part",
};
const Slot kInside_int_part[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a normal variable in int_part"}},
};
const TableValidator kTable_int_part{
  .assignment = &kAssign_int_part,
  .tooFewInside = "int_part requires 1 parameter",
  .inside = kInside_int_part,
};

const std::string_view kStrictTypes_is_adapt_act_pos_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_is_adapt_act_pos_input_1[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_is_adapt_act_pos_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_is_adapt_act_pos_input_0,
    .paramName = std::string_view{"input profile 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_is_adapt_act_pos_input_1,
    .paramName = std::string_view{"input profile 2"},
  },
};
const StrictMode kStrict_is_adapt_act_pos{
  .enabled = true,
  .inputTypes = kStrictSlots_is_adapt_act_pos_input,
};

const AssignmentCheck kAssign_is_adapt_act_pos{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in is_adapt_act_pos",
};
const Slot kInside_is_adapt_act_pos[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in is_adapt_act_pos"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in is_adapt_act_pos"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in is_adapt_act_pos"}},
};
const TableValidator kTable_is_adapt_act_pos{
  .assignment = &kAssign_is_adapt_act_pos,
  .tooFewInside = "is_adapt_act_pos requires at least 2 parameters inside parentheses",
  .inside = kInside_is_adapt_act_pos,
};

const std::string_view kStrictTypes_is_crossing_pce_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_is_crossing_pce_input_1[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_is_crossing_pce_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_is_crossing_pce_input_0,
    .paramName = std::string_view{"input profile 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_is_crossing_pce_input_1,
    .paramName = std::string_view{"input profile 2"},
  },
};
const StrictMode kStrict_is_crossing_pce{
  .enabled = true,
  .inputTypes = kStrictSlots_is_crossing_pce_input,
};

const AssignmentCheck kAssign_is_crossing_pce{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in is_crossing_pce",
};
const Slot kInside_is_crossing_pce[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in is_crossing_pce"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in is_crossing_pce"}},
};
const TableValidator kTable_is_crossing_pce{
  .assignment = &kAssign_is_crossing_pce,
  .tooFewInside = "is_crossing_pce requires 2 parameters inside parentheses",
  .inside = kInside_is_crossing_pce,
};

const std::string_view kStrictTypes_is_in_vxgeom_input_0[] = {
  std::string_view{"vxgeom"},
};
const std::string_view kStrictTypes_is_in_vxgeom_input_1[] = {
  std::string_view{"pvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
};
const std::string_view kStrictTypes_is_in_vxgeom_input_2[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_is_in_vxgeom_input_3[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_is_in_vxgeom_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_is_in_vxgeom_input_0,
    .paramName = std::string_view{"input vxgeom"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_is_in_vxgeom_input_1,
    .paramName = std::string_view{"input point vector"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_is_in_vxgeom_input_2,
    .paramName = std::string_view{"input tolerance value"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_is_in_vxgeom_input_3,
    .paramName = std::string_view{"input mode value"},
  },
};
const StrictMode kStrict_is_in_vxgeom{
  .enabled = true,
  .inputTypes = kStrictSlots_is_in_vxgeom_input,
};

const AssignmentCheck kAssign_is_in_vxgeom{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in is_in_vxgeom",
};
const Slot kInside_is_in_vxgeom[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid variable in is_in_vxgeom"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 must be a valid variable in is_in_vxgeom"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 must be a valid variable in is_in_vxgeom"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 must be a valid variable in is_in_vxgeom"}},
};
const TableValidator kTable_is_in_vxgeom{
  .assignment = &kAssign_is_in_vxgeom,
  .tooFewInside = "is_in_vxgeom requires 4 parameters",
  .inside = kInside_is_in_vxgeom,
};

const std::string_view kStrictTypes_is_point_inside_input_0[] = {
  std::string_view{"area"},
};
const std::string_view kStrictTypes_is_point_inside_input_1[] = {
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_is_point_inside_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_is_point_inside_input_0,
    .paramName = std::string_view{"input area"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_is_point_inside_input_1,
    .paramName = std::string_view{"input position vector"},
  },
};
const StrictMode kStrict_is_point_inside{
  .enabled = true,
  .inputTypes = kStrictSlots_is_point_inside_input,
};

const AssignmentCheck kAssign_is_point_inside{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in is_point_inside",
};
const Slot kInside_is_point_inside[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in is_point_inside"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid variable in is_point_inside"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter must be a valid variable in is_point_inside"}},
};
const TableValidator kTable_is_point_inside{
  .assignment = &kAssign_is_point_inside,
  .tooFewInside = "is_point_inside requires 2 parameters",
  .inside = kInside_is_point_inside,
};

const AssignmentCheck kAssign_is_struct_in_pdm{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .missing = "is_struct_in_pdm requires an assignment variable",
  .invalid = "Invalid assignment variable in is_struct_in_pdm",
};
const TableValidator kTable_is_struct_in_pdm{
  .assignment = &kAssign_is_struct_in_pdm,
};

const AssignmentCheck kAssign_len{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in len",
};
const Slot kInside_len[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid string in len"}},
};
const TableValidator kTable_len{
  .assignment = &kAssign_len,
  .tooFewInside = "len requires 1 parameter",
  .inside = kInside_len,
};

const AssignmentCheck kAssign_map_add_map{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_add_map",
};
const Slot kInside_map_add_map[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_add_map"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_add_map"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in map_add_map"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in map_add_map"}},
};
const TableValidator kTable_map_add_map{
  .assignment = &kAssign_map_add_map,
  .tooFewInside = "map_add_map requires at least 3 parameters inside parentheses",
  .inside = kInside_map_add_map,
};

const AssignmentCheck kAssign_map_clear{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_clear",
};
const Slot kInside_map_clear[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in map_clear"}},
};
const TableValidator kTable_map_clear{
  .assignment = &kAssign_map_clear,
  .tooFewInside = "map_clear requires 1 parameter inside parentheses",
  .inside = kInside_map_clear,
};

const AssignmentCheck kAssign_map_cmp{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_cmp",
};
const Slot kInside_map_cmp[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_cmp"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_cmp"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in map_cmp"}},
};
const TableValidator kTable_map_cmp{
  .assignment = &kAssign_map_cmp,
  .tooFewInside = "map_cmp requires at least 2 parameters inside parentheses",
  .inside = kInside_map_cmp,
};

const AssignmentCheck kAssign_map_del_map{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_del_map",
};
const Slot kInside_map_del_map[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_del_map"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_del_map"}},
};
const TableValidator kTable_map_del_map{
  .assignment = &kAssign_map_del_map,
  .tooFewInside = "map_del_map requires 2 parameters inside parentheses",
  .inside = kInside_map_del_map,
};

const AssignmentCheck kAssign_map_diff_keys{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "map_diff_keys requires an assignment variable",
  .invalid = "Assignment variable must be a valid string variable in map_diff_keys",
};
const Slot kInside_map_diff_keys[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_diff_keys"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_diff_keys"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in map_diff_keys"}},
};
const TableValidator kTable_map_diff_keys{
  .assignment = &kAssign_map_diff_keys,
  .tooFewInside = "map_diff_keys requires 3 parameters inside parentheses",
  .inside = kInside_map_diff_keys,
};

const AssignmentCheck kAssign_map_edit{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_edit",
};
const Slot kInside_map_edit[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in map_edit"}},
};
const TableValidator kTable_map_edit{
  .assignment = &kAssign_map_edit,
  .tooFewInside = "map_edit requires 1 parameter inside parentheses",
  .inside = kInside_map_edit,
};

const Slot kInside_map_find_key[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_find_key"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_find_key"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in map_find_key"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in map_find_key"}},
};
const TableValidator kTable_map_find_key{
  .tooFewInside = "map_find_key requires at least 3 parameters inside parentheses",
  .inside = kInside_map_find_key,
};

const Slot kInside_map_find_value[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_find_value"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_find_value"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in map_find_value"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in map_find_value"}},
};
const TableValidator kTable_map_find_value{
  .tooFewInside = "map_find_value requires at least 3 parameters inside parentheses",
  .inside = kInside_map_find_value,
};

const std::string_view kStrictTypes_map_from_dblist_input_0[] = {
  std::string_view{"dblist"},
};
const StrictTypeSlot kStrictSlots_map_from_dblist_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_map_from_dblist_input_0,
    .paramName = std::string_view{"input database list"},
  },
};
const StrictMode kStrict_map_from_dblist{
  .enabled = true,
  .inputTypes = kStrictSlots_map_from_dblist_input,
};

const AssignmentCheck kAssign_map_from_dblist{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_from_dblist",
};
const Slot kInside_map_from_dblist[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_from_dblist"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in map_from_dblist"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in map_from_dblist"}},
  {.index = 3, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid string in map_from_dblist"}},
};
const TableValidator kTable_map_from_dblist{
  .assignment = &kAssign_map_from_dblist,
  .tooFewInside = "map_from_dblist requires at least 3 parameters inside parentheses",
  .inside = kInside_map_from_dblist,
};

const std::string_view kStrictTypes_map_from_dbrow_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_map_from_dbrow_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_map_from_dbrow_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_map_from_dbrow{
  .enabled = true,
  .inputTypes = kStrictSlots_map_from_dbrow_input,
};

const AssignmentCheck kAssign_map_from_dbrow{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_from_dbrow",
};
const Slot kInside_map_from_dbrow[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_from_dbrow"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in map_from_dbrow"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in map_from_dbrow"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in map_from_dbrow"}},
};
const TableValidator kTable_map_from_dbrow{
  .assignment = &kAssign_map_from_dbrow,
  .tooFewInside = "map_from_dbrow requires at least 3 parameters inside parentheses",
  .inside = kInside_map_from_dbrow,
};

const AssignmentCheck kAssign_map_get{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "map_get requires an assignment variable",
  .invalid = "Assignment variable must be a valid string variable in map_get",
};
const Slot kInside_map_get[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_get"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_get"}},
};
const TableValidator kTable_map_get{
  .assignment = &kAssign_map_get,
  .tooFewInside = "map_get requires 2 parameters inside parentheses",
  .inside = kInside_map_get,
};

const AssignmentCheck kAssign_map_get_first{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_get_first",
};
const Slot kInside_map_get_first[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_get_first"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_get_first"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in map_get_first"}},
};
const TableValidator kTable_map_get_first{
  .assignment = &kAssign_map_get_first,
  .tooFewInside = "map_get_first requires 3 parameters inside parentheses",
  .inside = kInside_map_get_first,
};

const AssignmentCheck kAssign_map_get_next{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_get_next",
};
const Slot kInside_map_get_next[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_get_next"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_get_next"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in map_get_next"}},
};
const TableValidator kTable_map_get_next{
  .assignment = &kAssign_map_get_next,
  .tooFewInside = "map_get_next requires 3 parameters inside parentheses",
  .inside = kInside_map_get_next,
};

const AssignmentCheck kAssign_map_key_at{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "map_key_at requires an assignment variable",
  .invalid = "Assignment variable must be a valid string variable in map_key_at",
};
const Slot kInside_map_key_at[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_key_at"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in map_key_at"}},
};
const TableValidator kTable_map_key_at{
  .assignment = &kAssign_map_key_at,
  .tooFewInside = "map_key_at requires 2 parameters inside parentheses",
  .inside = kInside_map_key_at,
};

const AssignmentCheck kAssign_map_read{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_read",
};
const Slot kInside_map_read[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_read"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_read"}},
};
const TableValidator kTable_map_read{
  .assignment = &kAssign_map_read,
  .tooFewInside = "map_read requires 2 parameters inside parentheses",
  .inside = kInside_map_read,
};

const std::string_view kStrictTypes_map_save_input_0[] = {
  std::string_view{"string"},
};
const std::string_view kStrictTypes_map_save_input_1[] = {
  std::string_view{"string"},
};
const std::string_view kStrictTypes_map_save_input_2[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_map_save_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_map_save_input_0,
    .paramName = std::string_view{"map name"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_map_save_input_1,
    .paramName = std::string_view{"file path"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_map_save_input_2,
    .paramName = std::string_view{"optional variable"},
  },
};
const StrictMode kStrict_map_save{
  .enabled = true,
  .inputTypes = kStrictSlots_map_save_input,
};

const AssignmentCheck kAssign_map_save{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_save",
};
const Slot kInside_map_save[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_save"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in map_save"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in map_save"}},
};
const TableValidator kTable_map_save{
  .assignment = &kAssign_map_save,
  .tooFewInside = "map_save requires at least 2 parameters inside parentheses",
  .inside = kInside_map_save,
};

const std::string_view kStrictTypes_map_size_input_0[] = {
  std::string_view{"string"},
};
const StrictTypeSlot kStrictSlots_map_size_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_map_size_input_0,
    .paramName = std::string_view{"map name"},
  },
};
const StrictMode kStrict_map_size{
  .enabled = true,
  .inputTypes = kStrictSlots_map_size_input,
};

const AssignmentCheck kAssign_map_size{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_size",
};
const Slot kInside_map_size[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in map_size"}},
};
const TableValidator kTable_map_size{
  .assignment = &kAssign_map_size,
  .tooFewInside = "map_size requires 1 parameter inside parentheses",
  .inside = kInside_map_size,
};

const std::string_view kStrictTypes_map_to_dbrow_input_0[] = {
  std::string_view{"database"},
};
const StrictTypeSlot kStrictSlots_map_to_dbrow_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_map_to_dbrow_input_0,
    .paramName = std::string_view{"input database"},
  },
};
const StrictMode kStrict_map_to_dbrow{
  .enabled = true,
  .inputTypes = kStrictSlots_map_to_dbrow_input,
};

const AssignmentCheck kAssign_map_to_dbrow{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in map_to_dbrow",
};
const Slot kInside_map_to_dbrow[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in map_to_dbrow"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in map_to_dbrow"}},
  {.index = 2, .kind = SlotKind::AnyVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a variable or string in map_to_dbrow"}},
};
const TableValidator kTable_map_to_dbrow{
  .assignment = &kAssign_map_to_dbrow,
  .tooFewInside = "map_to_dbrow requires 3 parameters inside parentheses",
  .inside = kInside_map_to_dbrow,
};

const Slot kAfter_messagebox[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string in messagebox"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in messagebox"}},
};
const TableValidator kTable_messagebox{
  .tooFewAfter = "messagebox requires 2 parameters",
  .after = kAfter_messagebox,
};

const AssignmentCheck kAssign_milliseconds_since_1970{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::AnyVariable,
  .variableKind = VariableKind::None,
  .missing = "milliseconds_since_1970 requires an assignment variable",
  .invalid = "Invalid assignment variable in milliseconds_since_1970",
};
const TableValidator kTable_milliseconds_since_1970{
  .assignment = &kAssign_milliseconds_since_1970,
};

const AssignmentCheck kAssign_mm_to_dwg{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in mm_to_dwg",
};
const Slot kInside_mm_to_dwg[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a normal variable in mm_to_dwg"}},
};
const TableValidator kTable_mm_to_dwg{
  .assignment = &kAssign_mm_to_dwg,
  .tooFewInside = "mm_to_dwg requires 1 parameter",
  .inside = kInside_mm_to_dwg,
};

const std::string_view kStrictTypes_new_djoint_input_0[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_djoint_input_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const std::string_view kStrictTypes_new_djoint_input_2[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const std::string_view kStrictTypes_new_djoint_input_3[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const StrictTypeSlot kStrictSlots_new_djoint_input[] = {
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_new_djoint_input_0,
    .paramName = std::string_view{"input position vector"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_new_djoint_input_1,
    .paramName = std::string_view{"input direction vector 1"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_new_djoint_input_2,
    .paramName = std::string_view{"input direction vector 2"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_new_djoint_input_3,
    .paramName = std::string_view{"input direction vector 3"},
  },
};
const StrictMode kStrict_new_djoint{
  .enabled = true,
  .inputTypes = kStrictSlots_new_djoint_input,
};

const AssignmentCheck kAssign_new_djoint{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "new_djoint requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in new_djoint",
};
const Slot kInside_new_djoint[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string in new_djoint"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in new_djoint"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 must be a valid variable in new_djoint"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 must be a valid variable in new_djoint"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 5 must be a valid variable in new_djoint"}},
  {.index = 5, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 6 must be a valid variable in new_djoint"}},
  {.index = 6, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 7 must be a valid variable in new_djoint"}},
  {.index = 7, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 8 must be a valid variable in new_djoint"}},
};
const TableValidator kTable_new_djoint{
  .assignment = &kAssign_new_djoint,
  .tooFewInside = "new_djoint requires 8 parameters",
  .inside = kInside_new_djoint,
};

const AssignmentCheck kAssign_new_feature{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "new_feature requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in new_feature",
};
const Slot kInside_new_feature[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string in new_feature"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in new_feature"}},
};
const SlotTail kInsideTail_new_feature{.from = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .invalid = std::string_view{"Parameter {n} must be a valid variable in new_feature"}};
const TableValidator kTable_new_feature{
  .assignment = &kAssign_new_feature,
  .tooFewInside = "new_feature requires at least 3 parameters",
  .inside = kInside_new_feature,
  .insideTail = &kInsideTail_new_feature,
};


const std::string_view kStrictTypes_new_macro_input_0[] = {
  std::string_view{"pvector"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_new_macro_input_1[] = {
  std::string_view{"pvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const std::string_view kStrictTypes_new_macro_input_2[] = {
  std::string_view{"pvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const std::string_view kStrictTypes_new_macro_input_3[] = {
  std::string_view{"pvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const StrictTypeSlot kStrictSlots_new_macro_input[] = {
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_new_macro_input_0,
    .paramName = std::string_view{"input position vector"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_new_macro_input_1,
    .paramName = std::string_view{"input direction vector 1"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_new_macro_input_2,
    .paramName = std::string_view{"input direction vector 2"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_new_macro_input_3,
    .paramName = std::string_view{"input direction vector 3"},
  },
};
const StrictMode kStrict_new_macro{
  .enabled = true,
  .inputTypes = kStrictSlots_new_macro_input,
};

const AssignmentCheck kAssign_new_macro{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "new_macro requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in new_macro",
};
const Slot kInside_new_macro[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in new_macro"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in new_macro"}},
};
const SlotTail kInsideTail_new_macro{.from = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .invalid = std::string_view{"Parameter {n} inside parentheses must be a valid variable in new_macro"}};
const TableValidator kTable_new_macro{
  .assignment = &kAssign_new_macro,
  .tooFewInside = "new_macro requires at least 8 parameters inside parentheses",
  .tooManyInside = "new_macro requires at most 10 parameters inside parentheses",
  .inside = kInside_new_macro,
  .insideTail = &kInsideTail_new_macro,
};

const std::string_view kStrictTypes_new_ncaccessory_input_0[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_new_ncaccessory_input_1[] = {
  std::string_view{"pvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
};
const std::string_view kStrictTypes_new_ncaccessory_input_2[] = {
  std::string_view{"pvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
};
const std::string_view kStrictTypes_new_ncaccessory_input_3[] = {
  std::string_view{"pvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
};
const std::string_view kStrictTypes_new_ncaccessory_input_4[] = {
  std::string_view{"pvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
};
const std::string_view kStrictTypes_new_ncaccessory_input_5[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_new_ncaccessory_input_6[] = {
  std::string_view{"value"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_new_ncaccessory_input_7[] = {
  std::string_view{"vxgeom"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_new_ncaccessory_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_new_ncaccessory_input_0,
    .paramName = std::string_view{"input type"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_new_ncaccessory_input_1,
    .paramName = std::string_view{"input Point vector"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_new_ncaccessory_input_2,
    .paramName = std::string_view{"input X vector"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_new_ncaccessory_input_3,
    .paramName = std::string_view{"input Y vector"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_new_ncaccessory_input_4,
    .paramName = std::string_view{"input Z vector"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_new_ncaccessory_input_5,
    .paramName = std::string_view{"input Pres2d"},
  },
  {
    .paramIndex = 6,
    .allowedTypes = kStrictTypes_new_ncaccessory_input_6,
    .paramName = std::string_view{"input Pres3d"},
  },
  {
    .paramIndex = 8,
    .allowedTypes = kStrictTypes_new_ncaccessory_input_7,
    .paramName = std::string_view{"input Geom"},
  },
};
const StrictMode kStrict_new_ncaccessory{
  .enabled = true,
  .inputTypes = kStrictSlots_new_ncaccessory_input,
};

const AssignmentCheck kAssign_new_ncaccessory{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "new_ncaccessory requires an assignment variable",
  .invalid = "Assignment variable must be a valid variable in new_ncaccessory",
};
const Slot kInside_new_ncaccessory[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in new_ncaccessory"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in new_ncaccessory"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in new_ncaccessory"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter inside parentheses must be a valid variable in new_ncaccessory"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fifth parameter inside parentheses must be a valid variable in new_ncaccessory"}},
  {.index = 5, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Sixth parameter inside parentheses must be a valid variable in new_ncaccessory"}},
  {.index = 6, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Seventh parameter inside parentheses must be a valid variable in new_ncaccessory"}},
  {.index = 7, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Eighth parameter inside parentheses must be a valid string in new_ncaccessory"}},
};
const SlotTail kInsideTail_new_ncaccessory{.from = 8, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .invalid = std::string_view{"Parameter {n} inside parentheses must be a valid variable in new_ncaccessory"}};
const TableValidator kTable_new_ncaccessory{
  .assignment = &kAssign_new_ncaccessory,
  .tooFewInside = "new_ncaccessory requires at least 9 parameters inside parentheses",
  .inside = kInside_new_ncaccessory,
  .insideTail = &kInsideTail_new_ncaccessory,
};

const std::string_view kStrictTypes_new_piece_input_0[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_piece_input_1[] = {
  std::string_view{"value"},
  std::string_view{"string"},
};
const std::string_view kStrictTypes_new_piece_input_2[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_piece_input_3[] = {
  std::string_view{"value"},
  std::string_view{"string"},
};
const std::string_view kStrictTypes_new_piece_input_4[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const StrictTypeSlot kStrictSlots_new_piece_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_new_piece_input_0,
    .paramName = std::string_view{"input START position"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_new_piece_input_1,
    .paramName = std::string_view{"point index 1-9"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_new_piece_input_2,
    .paramName = std::string_view{"input END position"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_new_piece_input_3,
    .paramName = std::string_view{"point index 1-9"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_new_piece_input_4,
    .paramName = std::string_view{"input direction vector"},
  },
};
const StrictMode kStrict_new_piece{
  .enabled = true,
  .inputTypes = kStrictSlots_new_piece_input,
};

const AssignmentCheck kAssign_new_piece{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "new_piece requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in new_piece",
};
const Slot kInside_new_piece[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid variable in new_piece"}},
  {.index = 1, .kind = SlotKind::AnyVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid variable in new_piece"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter must be a valid variable in new_piece"}},
  {.index = 3, .kind = SlotKind::AnyVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fourth parameter must be a valid variable in new_piece"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Fifth parameter must be a valid variable in new_piece"}},
};
const TableValidator kTable_new_piece{
  .assignment = &kAssign_new_piece,
  .tooFewInside = "new_piece requires at least 8 parameters",
  .inside = kInside_new_piece,
};

const std::string_view kStrictTypes_new_piece2_input_0[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_piece2_input_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_piece2_input_2[] = {
  std::string_view{"profile"},
};
const StrictTypeSlot kStrictSlots_new_piece2_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_new_piece2_input_0,
    .paramName = std::string_view{"input position vector 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_new_piece2_input_1,
    .paramName = std::string_view{"input position vector 2"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_new_piece2_input_2,
    .paramName = std::string_view{"input profile"},
  },
};
const StrictMode kStrict_new_piece2{
  .enabled = true,
  .inputTypes = kStrictSlots_new_piece2_input,
};

const AssignmentCheck kAssign_new_piece2{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "new_piece2 requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in new_piece2",
};
const Slot kInside_new_piece2[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 inside parentheses must be a valid variable in new_piece2"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 inside parentheses must be a valid variable in new_piece2"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 inside parentheses must be a valid variable in new_piece2"}},
};
const TableValidator kTable_new_piece2{
  .assignment = &kAssign_new_piece2,
  .tooFewInside = "new_piece2 requires 3 parameters inside parentheses",
  .inside = kInside_new_piece2,
};

const std::string_view kStrictTypes_new_sheet_input_0[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_sheet_input_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_sheet_input_2[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_sheet_input_3[] = {
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_new_sheet_input[] = {
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_new_sheet_input_0,
    .paramName = std::string_view{"input position vector 1"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_new_sheet_input_1,
    .paramName = std::string_view{"input position vector 2"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_new_sheet_input_2,
    .paramName = std::string_view{"input position vector 3"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_new_sheet_input_3,
    .paramName = std::string_view{"input position vector 4"},
  },
};
const StrictMode kStrict_new_sheet{
  .enabled = true,
  .inputTypes = kStrictSlots_new_sheet_input,
};

const AssignmentCheck kAssign_new_sheet{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "new_sheet requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in new_sheet",
};
const Slot kInside_new_sheet[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string in new_sheet"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string in new_sheet"}},
};
const SlotTail kInsideTail_new_sheet{.from = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .invalid = std::string_view{"Parameter {n} must be a valid variable in new_sheet"}};
const TableValidator kTable_new_sheet{
  .assignment = &kAssign_new_sheet,
  .tooFewInside = "new_sheet requires at least 2 parameters",
  .inside = kInside_new_sheet,
  .insideTail = &kInsideTail_new_sheet,
};

const std::string_view kStrictTypes_new_vxgeom_after_0[] = {
  std::string_view{"string"},
};
const std::string_view kStrictTypes_new_vxgeom_after_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_vxgeom_after_2[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_new_vxgeom_after_3[] = {
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_new_vxgeom_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_new_vxgeom_after_0,
    .paramName = std::string_view{"input vxgeom shape string"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_new_vxgeom_after_1,
    .paramName = std::string_view{"point parameter 1"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_new_vxgeom_after_2,
    .paramName = std::string_view{"point parameter 2"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_new_vxgeom_after_3,
    .paramName = std::string_view{"point parameter 3"},
  },
};
const StrictMode kStrict_new_vxgeom{
  .enabled = true,
  .afterParamTypes = kStrictSlots_new_vxgeom_after,
};

const Slot kInside_new_vxgeom[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in new_vxgeom"}},
};
const Slot kAfter_new_vxgeom[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid string in new_vxgeom"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in new_vxgeom"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter after parentheses must be a valid variable in new_vxgeom"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter after parentheses must be a valid variable in new_vxgeom"}},
};
const TableValidator kTable_new_vxgeom{
  .tooFewInside = "new_vxgeom requires 1 parameter inside parentheses",
  .tooFewAfter = "new_vxgeom requires at least 3 parameters after parentheses",
  .inside = kInside_new_vxgeom,
  .after = kAfter_new_vxgeom,
};

const AssignmentCheck kAssign_next_item{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "next_item requires an assignment variable",
  .invalid = "Assignment variable must be a normal variable in next_item",
};
const Slot kInside_next_item[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in next_item"}},
};
const TableValidator kTable_next_item{
  .assignment = &kAssign_next_item,
  .tooFewInside = "next_item requires 1 parameter",
  .inside = kInside_next_item,
};

const AssignmentCheck kAssign_numeric_string{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a valid variable in numeric_string",
};
const Slot kInside_numeric_string[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in numeric_string"}},
};
const TableValidator kTable_numeric_string{
  .assignment = &kAssign_numeric_string,
  .tooFewInside = "numeric_string requires 1 parameter inside parentheses",
  .inside = kInside_numeric_string,
};

const Slot kInside_open[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in open"}},
};
const Slot kAfter_open[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"First parameter after parentheses must be a valid string in open"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter after parentheses must be a valid string in open"}},
};
const TableValidator kTable_open{
  .tooFewInside = "open requires 1 parameter inside parentheses",
  .inside = kInside_open,
  .after = kAfter_open,
};

const std::string_view kStrictTypes_pnt_inside_poly_input_0[] = {
  std::string_view{"polyline"},
};
const StrictTypeSlot kStrictSlots_pnt_inside_poly_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_pnt_inside_poly_input_0,
    .paramName = std::string_view{"input polyline"},
  },
};
const StrictMode kStrict_pnt_inside_poly{
  .enabled = true,
  .inputTypes = kStrictSlots_pnt_inside_poly_input,
};

const AssignmentCheck kAssign_pnt_inside_poly{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in pnt_inside_poly",
};
const Slot kInside_pnt_inside_poly[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in pnt_inside_poly"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid variable in pnt_inside_poly"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable in pnt_inside_poly"}},
};
const TableValidator kTable_pnt_inside_poly{
  .assignment = &kAssign_pnt_inside_poly,
  .tooFewInside = "pnt_inside_poly requires 2 parameters inside parentheses",
  .inside = kInside_pnt_inside_poly,
};

const std::string_view kStrictTypes_pnt_to_plane_input_0[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_pnt_to_plane_input_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_pnt_to_plane_input_2[] = {
  std::string_view{"pvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const std::string_view kStrictTypes_pnt_to_plane_input_3[] = {
  std::string_view{"pvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const StrictTypeSlot kStrictSlots_pnt_to_plane_input[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_pnt_to_plane_input_0,
    .paramName = std::string_view{"input pvector 1"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_pnt_to_plane_input_1,
    .paramName = std::string_view{"input pvector 2"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_pnt_to_plane_input_2,
    .paramName = std::string_view{"input direction vector 1"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_pnt_to_plane_input_3,
    .paramName = std::string_view{"input direction vector 2"},
  },
};
const StrictMode kStrict_pnt_to_plane{
  .enabled = true,
  .inputTypes = kStrictSlots_pnt_to_plane_input,
};

const std::string_view kStrictTypes_point_to_vxgeom_after_0[] = {
  std::string_view{"vxgeom"},
};
const std::string_view kStrictTypes_point_to_vxgeom_after_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_point_to_vxgeom_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_point_to_vxgeom_after_0,
    .paramName = std::string_view{"input vxgeom"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_point_to_vxgeom_after_1,
    .paramName = std::string_view{"input vector"},
  },
};
const StrictMode kStrict_point_to_vxgeom{
  .enabled = true,
  .afterParamTypes = kStrictSlots_point_to_vxgeom_after,
};

const Slot kAfter_point_to_vxgeom[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in point_to_vxgeom"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in point_to_vxgeom"}},
};
const TableValidator kTable_point_to_vxgeom{
  .tooFewInside = "point_to_vxgeom requires 1 parameter inside parentheses",
  .tooFewAfter = "point_to_vxgeom requires 2 parameters after parentheses",
  .after = kAfter_point_to_vxgeom,
};

const std::string_view kStrictTypes_points_to_poly_after_0[] = {
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_points_to_poly_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_points_to_poly_after_0,
    .paramName = std::string_view{"input position vector"},
  },
};
const StrictMode kStrict_points_to_poly{
  .enabled = true,
  .afterParamTypes = kStrictSlots_points_to_poly_after,
};

const Slot kInside_points_to_poly[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in points_to_poly"}},
};
const Slot kAfter_points_to_poly[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in points_to_poly"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in points_to_poly"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter after parentheses must be a valid variable in points_to_poly"}},
};
const TableValidator kTable_points_to_poly{
  .tooFewInside = "points_to_poly requires 1 parameter inside parentheses",
  .tooFewAfter = "points_to_poly requires 3 parameters after parentheses",
  .inside = kInside_points_to_poly,
  .after = kAfter_points_to_poly,
};

const std::string_view kStrictTypes_proj_to_piece_input_0[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_proj_to_piece_input_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_proj_to_piece_input_2[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_proj_to_piece_input_3[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_proj_to_piece_input_4[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_proj_to_piece_input[] = {
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_proj_to_piece_input_0,
    .paramName = std::string_view{"point-vector to go from"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_proj_to_piece_input_1,
    .paramName = std::string_view{"direction vector to project along"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_proj_to_piece_input_2,
    .paramName = std::string_view{"profile to project onto"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_proj_to_piece_input_3,
    .paramName = std::string_view{"offset"},
  },
  {
    .paramIndex = 6,
    .allowedTypes = kStrictTypes_proj_to_piece_input_4,
    .paramName = std::string_view{"mode"},
  },
};
const StrictMode kStrict_proj_to_piece{
  .enabled = true,
  .inputTypes = kStrictSlots_proj_to_piece_input,
};

const AssignmentCheck kAssign_proj_to_piece{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in proj_to_piece",
};
const Slot kInside_proj_to_piece[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid variable in proj_to_piece"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 must be a valid variable in proj_to_piece"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 must be a valid variable in proj_to_piece"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 must be a valid variable in proj_to_piece"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 5 must be a valid variable in proj_to_piece"}},
  {.index = 5, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 6 must be a valid variable in proj_to_piece"}},
  {.index = 6, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 7 must be a valid variable in proj_to_piece"}},
};
const TableValidator kTable_proj_to_piece{
  .assignment = &kAssign_proj_to_piece,
  .tooFewInside = "proj_to_piece requires 7 parameters",
  .inside = kInside_proj_to_piece,
};

const AssignmentCheck kAssign_read{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "read requires an assignment variable",
  .invalid = "Assignment variable must be a string variable in read",
};
const Slot kInside_read[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in read"}},
};
const TableValidator kTable_read{
  .assignment = &kAssign_read,
  .tooFewInside = "read requires 1 parameter inside parentheses",
  .inside = kInside_read,
};

const AssignmentCheck kAssign_replace{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "replace requires an assignment variable",
  .invalid = "Assignment variable must be a valid string variable in replace",
};
const Slot kInside_replace[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid string in replace"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in replace"}},
  {.index = 2, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::AnyVariable, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid variable or string expression in replace"}},
};
const TableValidator kTable_replace{
  .assignment = &kAssign_replace,
  .tooFewInside = "replace requires 3 parameters inside parentheses",
  .inside = kInside_replace,
};

const std::string_view kStrictTypes_section_to_poly_after_0[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_section_to_poly_after_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_section_to_poly_after_2[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const std::string_view kStrictTypes_section_to_poly_after_3[] = {
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_section_to_poly_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_section_to_poly_after_0,
    .paramName = std::string_view{"input profile"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_section_to_poly_after_1,
    .paramName = std::string_view{"input position vector"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_section_to_poly_after_2,
    .paramName = std::string_view{"input direction vector"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_section_to_poly_after_3,
    .paramName = std::string_view{"input optional position vector"},
  },
};
const StrictMode kStrict_section_to_poly{
  .enabled = true,
  .afterParamTypes = kStrictSlots_section_to_poly_after,
};

const Slot kAfter_section_to_poly[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in section_to_poly"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in section_to_poly"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter after parentheses must be a valid variable in section_to_poly"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter after parentheses must be a valid variable in section_to_poly"}},
  {.index = 4, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fifth parameter after parentheses must be a valid variable in section_to_poly"}},
};
const TableValidator kTable_section_to_poly{
  .tooFewInside = "section_to_poly requires exactly 1 parameter inside parentheses",
  .tooManyInside = "section_to_poly requires exactly 1 parameter inside parentheses",
  .after = kAfter_section_to_poly,
};

const Slot kAfter_set[] = {
  {.index = 0, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::AnyVariable, .required = true, .invalid = std::string_view{"First parameter must be a valid variable or string in set"}},
  {.index = 1, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::AnyVariable, .required = true, .invalid = std::string_view{"Second parameter must be a valid variable or string in set"}},
};
const TableValidator kTable_set{
  .tooFewAfter = "set requires 2 parameters",
  .after = kAfter_set,
};

const std::string_view kStrictTypes_set_base_label_input_0[] = {
  std::string_view{"macro"},
  std::string_view{"array"},
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"profileend"},
  std::string_view{"selemobj"},
};
const StrictTypeSlot kStrictSlots_set_base_label_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_base_label_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_set_base_label{
  .enabled = true,
  .inputTypes = kStrictSlots_set_base_label_input,
};

const Slot kInside_set_base_label[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_base_label"}},
};
const Slot kAfter_set_base_label[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid string in set_base_label"}},
};
const TableValidator kTable_set_base_label{
  .tooFewInside = "set_base_label requires 1 parameter inside parentheses",
  .tooFewAfter = "set_base_label requires 1 parameter after parentheses",
  .inside = kInside_set_base_label,
  .after = kAfter_set_base_label,
};

const std::string_view kStrictTypes_set_djmember_input_0[] = {
  std::string_view{"joint"},
};
const StrictTypeSlot kStrictSlots_set_djmember_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_djmember_input_0,
    .paramName = std::string_view{"input joint"},
  },
};
const std::string_view kStrictTypes_set_djmember_after_0[] = {
  std::string_view{"macro"},
  std::string_view{"selemobj"},
  std::string_view{"array"},
  std::string_view{"wall"},
  std::string_view{"edge"},
  std::string_view{"sheet"},
  std::string_view{"profile"},
  std::string_view{"slope"},
  std::string_view{"area"},
  std::string_view{"profileend"},
};
const StrictTypeSlot kStrictSlots_set_djmember_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_djmember_after_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_set_djmember{
  .enabled = true,
  .inputTypes = kStrictSlots_set_djmember_input,
  .afterParamTypes = kStrictSlots_set_djmember_after,
};

const Slot kInside_set_djmember[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_djmember"}},
};
const Slot kAfter_set_djmember[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in set_djmember"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid string in set_djmember"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter after parentheses must be a valid variable in set_djmember"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Fourth parameter after parentheses must be a valid variable in set_djmember"}},
};
const TableValidator kTable_set_djmember{
  .tooFewInside = "set_djmember requires 1 parameter inside parentheses",
  .tooFewAfter = "set_djmember requires at least 2 parameters after parentheses",
  .inside = kInside_set_djmember,
  .after = kAfter_set_djmember,
};

const std::string_view kStrictTypes_set_djparam_input_0[] = {
  std::string_view{"joint"},
};
const StrictTypeSlot kStrictSlots_set_djparam_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_djparam_input_0,
    .paramName = std::string_view{"input joint"},
  },
};
const StrictMode kStrict_set_djparam{
  .enabled = true,
  .inputTypes = kStrictSlots_set_djparam_input,
};

const CleanupSlot kCleanupSlots_set_djparam[] = {
  {.quote = QuoteRule::Never},
  {.quote = QuoteRule::BareNumericLiteral},
};
const CleanupRules kCleanup_set_djparam{
  .afterParams = kCleanupSlots_set_djparam,
};

const Slot kInside_set_djparam[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_djparam"}},
};
const Slot kAfter_set_djparam[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid string in set_djparam"}},
  {.index = 1, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::NormalVariable, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable or string in set_djparam"}},
};
const TableValidator kTable_set_djparam{
  .tooFewInside = "set_djparam requires 1 parameter inside parentheses",
  .tooFewAfter = "set_djparam requires 2 parameters after parentheses",
  .inside = kInside_set_djparam,
  .after = kAfter_set_djparam,
};

const std::string_view kStrictTypes_set_joint_param_input_0[] = {
  std::string_view{"joint"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_set_joint_param_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_joint_param_input_0,
    .paramName = std::string_view{"input joint"},
  },
};
const StrictMode kStrict_set_joint_param{
  .enabled = true,
  .inputTypes = kStrictSlots_set_joint_param_input,
};

const CleanupSlot kCleanupSlots_set_joint_param[] = {
  {.quote = QuoteRule::Never},
  {.quote = QuoteRule::BareNumericLiteral},
};
const CleanupRules kCleanup_set_joint_param{
  .afterParams = kCleanupSlots_set_joint_param,
};

const Slot kInside_set_joint_param[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_joint_param"}},
};
const Slot kAfter_set_joint_param[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid string in set_joint_param"}},
  {.index = 1, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::NormalVariable, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid string or value in set_joint_param"}},
};
const TableValidator kTable_set_joint_param{
  .tooFewInside = "set_joint_param requires 1 parameter inside parentheses",
  .tooFewAfter = "set_joint_param requires 2 parameters after parentheses",
  .inside = kInside_set_joint_param,
  .after = kAfter_set_joint_param,
};

const std::string_view kStrictTypes_set_joint_punch_input_0[] = {
  std::string_view{"joint"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_set_joint_punch_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_joint_punch_input_0,
    .paramName = std::string_view{"input joint"},
  },
};
const StrictMode kStrict_set_joint_punch{
  .enabled = true,
  .inputTypes = kStrictSlots_set_joint_punch_input,
};

const Slot kInside_set_joint_punch[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_joint_punch"}},
};
const Slot kAfter_set_joint_punch[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 after parentheses must be a valid variable in set_joint_punch"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 after parentheses must be a valid variable in set_joint_punch"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 after parentheses must be a valid variable in set_joint_punch"}},
};
const TableValidator kTable_set_joint_punch{
  .tooFewInside = "set_joint_punch requires 1 parameter inside parentheses",
  .tooFewAfter = "set_joint_punch requires 3 parameters after parentheses",
  .inside = kInside_set_joint_punch,
  .after = kAfter_set_joint_punch,
};

const std::string_view kStrictTypes_set_macro_form_input_0[] = {
  std::string_view{"macro"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_set_macro_form_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_macro_form_input_0,
    .paramName = std::string_view{"input macro"},
  },
};
const StrictMode kStrict_set_macro_form{
  .enabled = true,
  .inputTypes = kStrictSlots_set_macro_form_input,
};

const Slot kInside_set_macro_form[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_macro_form"}},
};
const Slot kAfter_set_macro_form[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid string in set_macro_form"}},
};
const TableValidator kTable_set_macro_form{
  .tooFewInside = "set_macro_form requires 1 parameter inside parentheses",
  .tooFewAfter = "set_macro_form requires 1 parameter outside parentheses",
  .inside = kInside_set_macro_form,
  .after = kAfter_set_macro_form,
};

const std::string_view kStrictTypes_set_macro_param_input_0[] = {
  std::string_view{"macro"},
};
const StrictTypeSlot kStrictSlots_set_macro_param_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_macro_param_input_0,
    .paramName = std::string_view{"input macro"},
  },
};
const StrictMode kStrict_set_macro_param{
  .enabled = true,
  .inputTypes = kStrictSlots_set_macro_param_input,
};

const Slot kInside_set_macro_param[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_macro_param"}},
};
const Slot kAfter_set_macro_param[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid string in set_macro_param"}},
  {.index = 1, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::NormalVariable, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid string or value in set_macro_param"}},
};
const TableValidator kTable_set_macro_param{
  .tooFewInside = "set_macro_param requires 1 parameter inside parentheses",
  .tooFewAfter = "set_macro_param requires at least 2 parameters after parentheses",
  .inside = kInside_set_macro_param,
  .after = kAfter_set_macro_param,
};

const CleanupSlot kCleanupSlots_set_obj_param[] = {
  {.quote = QuoteRule::Never},
  {.quote = QuoteRule::BareNumericLiteral},
  {.quote = QuoteRule::Never},
  {.quote = QuoteRule::Never},
};
const CleanupRules kCleanup_set_obj_param{
  .afterParams = kCleanupSlots_set_obj_param,
};

const Slot kInside_set_obj_param[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Object parameter must be a valid variable in set_obj_param"}},
};
const Slot kAfter_set_obj_param[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter name must be a valid string in set_obj_param"}},
  {.index = 1, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::NormalVariable, .required = true, .invalid = std::string_view{"Parameter value must be a valid string or variable in set_obj_param"}},
  {.index = 2, .kind = SlotKind::DigitLiteralOrVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Mode parameter must be a number or variable in set_obj_param"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Obj2 parameter must be a valid variable in set_obj_param"}},
};
const TableValidator kTable_set_obj_param{
  .tooFewAfter = "set_obj_param requires at least 2 parameters after parentheses",
  .inside = kInside_set_obj_param,
  .after = kAfter_set_obj_param,
};

const std::string_view kStrictTypes_set_origin_pos_input_0[] = {
  std::string_view{"macro"},
  std::string_view{"selemobj"},
  std::string_view{"array"},
  std::string_view{"wall"},
  std::string_view{"edge"},
  std::string_view{"sheet"},
  std::string_view{"profile"},
  std::string_view{"slope"},
  std::string_view{"profileend"},
  std::string_view{"joint"},
};
const std::string_view kStrictTypes_set_origin_pos_input_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_set_origin_pos_input_2[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const std::string_view kStrictTypes_set_origin_pos_input_3[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const std::string_view kStrictTypes_set_origin_pos_input_4[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const StrictTypeSlot kStrictSlots_set_origin_pos_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_origin_pos_input_0,
    .paramName = std::string_view{"input object"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_set_origin_pos_input_1,
    .paramName = std::string_view{"input position vector 1"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_set_origin_pos_input_2,
    .paramName = std::string_view{"input direction vector 1"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_set_origin_pos_input_3,
    .paramName = std::string_view{"input direction vector 2"},
  },
  {
    .paramIndex = 4,
    .allowedTypes = kStrictTypes_set_origin_pos_input_4,
    .paramName = std::string_view{"input direction vector 3"},
  },
};
const StrictMode kStrict_set_origin_pos{
  .enabled = true,
  .inputTypes = kStrictSlots_set_origin_pos_input,
};

const Slot kInside_set_origin_pos[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_origin_pos"}},
};
const Slot kAfter_set_origin_pos[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 after parentheses must be a valid variable in set_origin_pos"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 after parentheses must be a valid variable in set_origin_pos"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 after parentheses must be a valid variable in set_origin_pos"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 after parentheses must be a valid variable in set_origin_pos"}},
};
const TableValidator kTable_set_origin_pos{
  .tooFewInside = "set_origin_pos requires 1 parameter inside parentheses",
  .tooFewAfter = "set_origin_pos requires 4 parameters after parentheses",
  .inside = kInside_set_origin_pos,
  .after = kAfter_set_origin_pos,
};

const std::string_view kStrictTypes_set_piece_pos_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_set_piece_pos_input_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_set_piece_pos_input_2[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_set_piece_pos_input_3[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const StrictTypeSlot kStrictSlots_set_piece_pos_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_piece_pos_input_0,
    .paramName = std::string_view{"input profile"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_set_piece_pos_input_1,
    .paramName = std::string_view{"input start position"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_set_piece_pos_input_2,
    .paramName = std::string_view{"input end position"},
  },
  {
    .paramIndex = 5,
    .allowedTypes = kStrictTypes_set_piece_pos_input_3,
    .paramName = std::string_view{"input direction vector"},
  },
};
const StrictMode kStrict_set_piece_pos{
  .enabled = true,
  .inputTypes = kStrictSlots_set_piece_pos_input,
};

const AssignmentCheck kAssign_set_piece_pos{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in set_piece_pos",
};
const Slot kInside_set_piece_pos[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid variable in set_piece_pos"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 must be a valid variable in set_piece_pos"}},
  {.index = 2, .kind = SlotKind::AnyVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 must be a valid variable in set_piece_pos"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 must be a valid variable in set_piece_pos"}},
  {.index = 4, .kind = SlotKind::AnyVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 5 must be a valid variable in set_piece_pos"}},
  {.index = 5, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 6 must be a valid variable in set_piece_pos"}},
};
const TableValidator kTable_set_piece_pos{
  .assignment = &kAssign_set_piece_pos,
  .tooFewInside = "set_piece_pos requires 6 parameters",
  .inside = kInside_set_piece_pos,
};

const std::string_view kStrictTypes_set_piece_sect_input_0[] = {
  std::string_view{"profile"},
};
const StrictTypeSlot kStrictSlots_set_piece_sect_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_piece_sect_input_0,
    .paramName = std::string_view{"input profile"},
  },
};
const StrictMode kStrict_set_piece_sect{
  .enabled = true,
  .inputTypes = kStrictSlots_set_piece_sect_input,
};

const Slot kInside_set_piece_sect[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in set_piece_sect"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter inside parentheses must be a valid string in set_piece_sect"}},
  {.index = 2, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Third parameter inside parentheses must be a valid string in set_piece_sect"}},
};
const SlotTail kInsideTail_set_piece_sect{.from = 3, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .invalid = std::string_view{"Parameter {n} inside parentheses must be a valid string in set_piece_sect"}};
const TableValidator kTable_set_piece_sect{
  .tooFewInside = "set_piece_sect requires at least 3 parameters inside parentheses",
  .tooManyInside = "set_piece_sect requires at most 6 parameters inside parentheses",
  .inside = kInside_set_piece_sect,
  .insideTail = &kInsideTail_set_piece_sect,
};

const std::string_view kStrictTypes_set_piece_set_input_0[] = {
  std::string_view{"profile"},
};
const StrictTypeSlot kStrictSlots_set_piece_set_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_piece_set_input_0,
    .paramName = std::string_view{"input profile"},
  },
};
const StrictMode kStrict_set_piece_set{
  .enabled = true,
  .inputTypes = kStrictSlots_set_piece_set_input,
};

const Slot kInside_set_piece_set[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_piece_set"}},
};
const Slot kAfter_set_piece_set[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid variable in set_piece_set"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in set_piece_set"}},
};
const TableValidator kTable_set_piece_set{
  .tooFewInside = "set_piece_set requires 1 parameter inside parentheses",
  .tooFewAfter = "set_piece_set requires at least 1 parameter after parentheses",
  .inside = kInside_set_piece_set,
  .after = kAfter_set_piece_set,
};

const std::string_view kStrictTypes_set_pnt_param_input_0[] = {
  std::string_view{"macro"},
  std::string_view{"array"},
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"edge"},
  std::string_view{"selemobj"},
  std::string_view{"area"},
  std::string_view{"slope"},
  std::string_view{"wall"},
  std::string_view{"joint"},
};
const StrictTypeSlot kStrictSlots_set_pnt_param_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_pnt_param_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const std::string_view kStrictTypes_set_pnt_param_after_0[] = {
  std::string_view{"pvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
};
const StrictTypeSlot kStrictSlots_set_pnt_param_after[] = {
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_set_pnt_param_after_0,
    .paramName = std::string_view{"input point/vector"},
  },
};
const StrictMode kStrict_set_pnt_param{
  .enabled = true,
  .inputTypes = kStrictSlots_set_pnt_param_input,
  .afterParamTypes = kStrictSlots_set_pnt_param_after,
};

const Slot kInside_set_pnt_param[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_pnt_param"}},
};
const Slot kAfter_set_pnt_param[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter outside parentheses must be a valid string in set_pnt_param"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter outside parentheses must be a valid variable in set_pnt_param"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = false, .invalid = std::string_view{"Third parameter outside parentheses must be a valid variable in set_pnt_param"}},
};
const TableValidator kTable_set_pnt_param{
  .tooFewInside = "set_pnt_param requires 1 parameter inside parentheses",
  .tooFewAfter = "set_pnt_param requires at least 2 parameters outside parentheses",
  .inside = kInside_set_pnt_param,
  .after = kAfter_set_pnt_param,
};

const std::string_view kStrictTypes_set_poly_pleq_input_0[] = {
  std::string_view{"polyline"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_set_poly_pleq_input_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_set_poly_pleq_input_2[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
};
const StrictTypeSlot kStrictSlots_set_poly_pleq_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_poly_pleq_input_0,
    .paramName = std::string_view{"input polygon"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_set_poly_pleq_input_1,
    .paramName = std::string_view{"input position vector"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_set_poly_pleq_input_2,
    .paramName = std::string_view{"input direction vector"},
  },
};
const StrictMode kStrict_set_poly_pleq{
  .enabled = true,
  .inputTypes = kStrictSlots_set_poly_pleq_input,
};

const Slot kAfter_set_poly_pleq[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in set_poly_pleq"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in set_poly_pleq"}},
};
const TableValidator kTable_set_poly_pleq{
  .tooFewInside = "set_poly_pleq requires exactly 1 parameter inside parentheses",
  .tooManyInside = "set_poly_pleq requires exactly 1 parameter inside parentheses",
  .after = kAfter_set_poly_pleq,
};

const std::string_view kStrictTypes_set_regenerable_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"sheet"},
};
const StrictTypeSlot kStrictSlots_set_regenerable_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_regenerable_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const std::string_view kStrictTypes_set_regenerable_after_0[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_set_regenerable_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_regenerable_after_0,
    .paramName = std::string_view{"regenerable value"},
  },
};
const StrictMode kStrict_set_regenerable{
  .enabled = true,
  .inputTypes = kStrictSlots_set_regenerable_input,
  .afterParamTypes = kStrictSlots_set_regenerable_after,
};

const Slot kInside_set_regenerable[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_regenerable"}},
};
const Slot kAfter_set_regenerable[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter outside parentheses must be a valid variable in set_regenerable"}},
};
const TableValidator kTable_set_regenerable{
  .tooFewInside = "set_regenerable requires 1 parameter inside parentheses",
  .tooFewAfter = "set_regenerable requires 1 parameter outside parentheses",
  .inside = kInside_set_regenerable,
  .after = kAfter_set_regenerable,
};

const std::string_view kStrictTypes_set_same_tlink_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"area"},
  std::string_view{"macro"},
  std::string_view{"wall"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_set_same_tlink_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_same_tlink_input_0,
    .paramName = std::string_view{"object"},
  },
};
const std::string_view kStrictTypes_set_same_tlink_after_0[] = {
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"area"},
  std::string_view{"macro"},
  std::string_view{"wall"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_set_same_tlink_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_same_tlink_after_0,
    .paramName = std::string_view{"reference object"},
  },
};
const StrictMode kStrict_set_same_tlink{
  .enabled = true,
  .inputTypes = kStrictSlots_set_same_tlink_input,
  .afterParamTypes = kStrictSlots_set_same_tlink_after,
};

const Slot kInside_set_same_tlink[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in set_same_tlink"}},
};
const Slot kAfter_set_same_tlink[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in set_same_tlink"}},
};
const TableValidator kTable_set_same_tlink{
  .tooFewInside = "set_same_tlink requires 1 parameter inside parentheses",
  .tooFewAfter = "set_same_tlink requires 1 parameter after parentheses",
  .inside = kInside_set_same_tlink,
  .after = kAfter_set_same_tlink,
};

const std::string_view kStrictTypes_set_same_visib_input_0[] = {
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"area"},
  std::string_view{"macro"},
  std::string_view{"wall"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_set_same_visib_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_same_visib_input_0,
    .paramName = std::string_view{"object"},
  },
};
const std::string_view kStrictTypes_set_same_visib_after_0[] = {
  std::string_view{"profile"},
  std::string_view{"sheet"},
  std::string_view{"area"},
  std::string_view{"macro"},
  std::string_view{"wall"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_set_same_visib_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_same_visib_after_0,
    .paramName = std::string_view{"reference object"},
  },
};
const StrictMode kStrict_set_same_visib{
  .enabled = true,
  .inputTypes = kStrictSlots_set_same_visib_input,
  .afterParamTypes = kStrictSlots_set_same_visib_after,
};

const Slot kInside_set_same_visib[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in set_same_visib"}},
};
const Slot kAfter_set_same_visib[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in set_same_visib"}},
};
const TableValidator kTable_set_same_visib{
  .tooFewInside = "set_same_tlink requires 1 parameter inside parentheses",
  .tooFewAfter = "set_same_tlink requires 1 parameter after parentheses",
  .inside = kInside_set_same_visib,
  .after = kAfter_set_same_visib,
};

const std::string_view kStrictTypes_set_sheet_shape_input_0[] = {
  std::string_view{"sheet"},
};
const std::string_view kStrictTypes_set_sheet_shape_input_1[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_set_sheet_shape_input_2[] = {
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_set_sheet_shape_input_3[] = {
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_set_sheet_shape_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_sheet_shape_input_0,
    .paramName = std::string_view{"input sheet"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_set_sheet_shape_input_1,
    .paramName = std::string_view{"input position vector"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_set_sheet_shape_input_2,
    .paramName = std::string_view{"input vector 2"},
  },
  {
    .paramIndex = 3,
    .allowedTypes = kStrictTypes_set_sheet_shape_input_3,
    .paramName = std::string_view{"input vector 3"},
  },
};
const StrictMode kStrict_set_sheet_shape{
  .enabled = true,
  .inputTypes = kStrictSlots_set_sheet_shape_input,
};

const Slot kInside_set_sheet_shape[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter inside parentheses must be a valid variable in set_sheet_shape"}},
};
const SlotTail kAfterTail_set_sheet_shape{.from = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .invalid = std::string_view{"Parameter {n} outside parentheses must be a valid variable in set_sheet_shape"}};
const TableValidator kTable_set_sheet_shape{
  .tooFewInside = "set_sheet_shape requires 1 parameter inside parentheses",
  .tooFewAfter = "set_sheet_shape requires 3 parameters outside parentheses",
  .inside = kInside_set_sheet_shape,
  .afterTail = &kAfterTail_set_sheet_shape,
};

const std::string_view kStrictTypes_set_visibility_input_0[] = {
  std::string_view{"macro"},
  std::string_view{"array"},
  std::string_view{"wall"},
  std::string_view{"sheet"},
  std::string_view{"profile"},
  std::string_view{"profileend"},
};
const StrictTypeSlot kStrictSlots_set_visibility_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_set_visibility_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_set_visibility{
  .enabled = true,
  .inputTypes = kStrictSlots_set_visibility_input,
};

const Slot kInside_set_visibility[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in set_visibility"}},
};
const Slot kAfter_set_visibility[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid string in set_visibility"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in set_visibility"}},
};
const TableValidator kTable_set_visibility{
  .tooFewInside = "set_visibility requires 1 parameter inside parentheses",
  .tooFewAfter = "set_visibility requires 2 parameters after parentheses",
  .inside = kInside_set_visibility,
  .after = kAfter_set_visibility,
};

const std::string_view kStrictTypes_shrink_poly_input_0[] = {
  std::string_view{"polyline"},
};
const StrictTypeSlot kStrictSlots_shrink_poly_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_shrink_poly_input_0,
    .paramName = std::string_view{"input polyline"},
  },
};
const StrictMode kStrict_shrink_poly{
  .enabled = true,
  .inputTypes = kStrictSlots_shrink_poly_input,
};

const Slot kInside_shrink_poly[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid variable in shrink_poly"}},
};
const TableValidator kTable_shrink_poly{
  .tooFewInside = "shrink_poly requires 1 parameter inside parentheses",
  .inside = kInside_shrink_poly,
};

const AssignmentCheck kAssign_strfield{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "strfield requires an assignment variable",
  .invalid = "Assignment variable must be a string variable in strfield",
};
const Slot kInside_strfield[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter must be a valid string variable in strfield"}},
  {.index = 1, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter must be a valid string variable in strfield"}},
};
const TableValidator kTable_strfield{
  .assignment = &kAssign_strfield,
  .tooFewInside = "strfield requires 2 parameters",
  .inside = kInside_strfield,
};

const AssignmentCheck kAssign_tan{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .missing = "tan requires an assignment variable",
  .invalid = "Assignment variable must be a valid variable in tan",
};
const Slot kInside_tan[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in tan"}},
};
const TableValidator kTable_tan{
  .assignment = &kAssign_tan,
  .tooFewInside = "tan requires 1 parameter inside parentheses",
  .inside = kInside_tan,
};

const AssignmentCheck kAssign_trim{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "trim requires an assignment variable",
  .invalid = "Assignment variable must be a string variable in trim",
};
const Slot kInside_trim[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in trim"}},
};
const TableValidator kTable_trim{
  .assignment = &kAssign_trim,
  .tooFewInside = "trim requires 1 parameter inside parentheses",
  .inside = kInside_trim,
};

const Slot kAfter_unset[] = {
  {.index = 0, .kind = SlotKind::StringOrVariable, .variableKind = VariableKind::AnyVariable, .required = true, .invalid = std::string_view{"Parameter must be a valid variable or string in unset"}},
};
const TableValidator kTable_unset{
  .tooFewAfter = "unset requires 1 parameter",
  .after = kAfter_unset,
};

const std::string_view kStrictTypes_upd_object_geom_input_0[] = {
  std::string_view{"slope"},
  std::string_view{"area"},
  std::string_view{"macro"},
  std::string_view{"selemobj"},
  std::string_view{"array"},
  std::string_view{"wall"},
  std::string_view{"edge"},
  std::string_view{"sheet"},
  std::string_view{"profile"},
  std::string_view{"slope"},
  std::string_view{"macro"},
};
const StrictTypeSlot kStrictSlots_upd_object_geom_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_upd_object_geom_input_0,
    .paramName = std::string_view{"input object"},
  },
};
const StrictMode kStrict_upd_object_geom{
  .enabled = true,
  .inputTypes = kStrictSlots_upd_object_geom_input,
};

const Slot kInside_upd_object_geom[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in upd_object_geom"}},
};
const TableValidator kTable_upd_object_geom{
  .tooFewInside = "upd_object_geom requires 1 parameter inside parentheses",
  .inside = kInside_upd_object_geom,
};

const std::string_view kStrictTypes_vec_add_input_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_vec_add_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_add_input_0,
    .paramName = std::string_view{"output vector"},
  },
};
const std::string_view kStrictTypes_vec_add_after_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_vec_add_after_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_vec_add_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_add_after_0,
    .paramName = std::string_view{"input vector 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_vec_add_after_1,
    .paramName = std::string_view{"input vector 2"},
  },
};
const StrictMode kStrict_vec_add{
  .enabled = true,
  .inputTypes = kStrictSlots_vec_add_input,
  .afterParamTypes = kStrictSlots_vec_add_after,
};

const Slot kInside_vec_add[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in vec_add"}},
};
const Slot kAfter_vec_add[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in vec_add"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in vec_add"}},
};
const TableValidator kTable_vec_add{
  .tooFewInside = "vec_add requires 1 parameter inside parentheses",
  .tooFewAfter = "vec_add requires 2 parameters after parentheses",
  .inside = kInside_vec_add,
  .after = kAfter_vec_add,
};

const std::string_view kStrictTypes_vec_copy_after_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_vec_copy_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_copy_after_0,
    .paramName = std::string_view{"input vector to copy"},
  },
};
const StrictMode kStrict_vec_copy{
  .enabled = true,
  .afterParamTypes = kStrictSlots_vec_copy_after,
};

const Slot kInside_vec_copy[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in vec_copy"}},
};
const Slot kAfter_vec_copy[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid variable in vec_copy"}},
};
const TableValidator kTable_vec_copy{
  .tooFewInside = "vec_copy requires 1 parameter inside parentheses",
  .tooFewAfter = "vec_copy requires 1 parameter after parentheses",
  .inside = kInside_vec_copy,
  .after = kAfter_vec_copy,
};

const std::string_view kStrictTypes_vec_cross_prod_after_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_vec_cross_prod_after_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_vec_cross_prod_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_cross_prod_after_0,
    .paramName = std::string_view{"input vector 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_vec_cross_prod_after_1,
    .paramName = std::string_view{"input vector 2"},
  },
};
const StrictMode kStrict_vec_cross_prod{
  .enabled = true,
  .afterParamTypes = kStrictSlots_vec_cross_prod_after,
};

const Slot kInside_vec_cross_prod[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a normal variable in vec_cross_prod"}},
};
const Slot kAfter_vec_cross_prod[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 outside parentheses must be a normal variable in vec_cross_prod"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 outside parentheses must be a normal variable in vec_cross_prod"}},
};
const TableValidator kTable_vec_cross_prod{
  .tooFewInside = "vec_cross_prod requires 1 parameter inside parentheses",
  .tooFewAfter = "vec_cross_prod requires 2 parameters outside parentheses",
  .inside = kInside_vec_cross_prod,
  .after = kAfter_vec_cross_prod,
};

const std::string_view kStrictTypes_vec_dot_prod_input_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_vec_dot_prod_input_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_vec_dot_prod_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_dot_prod_input_0,
    .paramName = std::string_view{"input vector 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_vec_dot_prod_input_1,
    .paramName = std::string_view{"input vector 2"},
  },
};
const StrictMode kStrict_vec_dot_prod{
  .enabled = true,
  .inputTypes = kStrictSlots_vec_dot_prod_input,
};

const AssignmentCheck kAssign_vec_dot_prod{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in vec_dot_prod",
};
const Slot kInside_vec_dot_prod[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid variable in vec_dot_prod"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 must be a valid variable in vec_dot_prod"}},
};
const TableValidator kTable_vec_dot_prod{
  .assignment = &kAssign_vec_dot_prod,
  .tooFewInside = "vec_dot_prod requires 2 parameters",
  .inside = kInside_vec_dot_prod,
};

const std::string_view kStrictTypes_vec_get_x_input_0[] = {
  std::string_view{"pvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
};
const StrictTypeSlot kStrictSlots_vec_get_x_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_get_x_input_0,
    .paramName = std::string_view{"object"},
  },
};
const StrictMode kStrict_vec_get_x{
  .enabled = true,
  .inputTypes = kStrictSlots_vec_get_x_input,
};

const AssignmentCheck kAssign_vec_get_x{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in vec_get_x",
};
const Slot kInside_vec_get_x[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in vec_get_x"}},
};
const TableValidator kTable_vec_get_x{
  .assignment = &kAssign_vec_get_x,
  .tooFewInside = "vec_get_x requires 1 parameter",
  .inside = kInside_vec_get_x,
};

const std::string_view kStrictTypes_vec_get_y_input_0[] = {
  std::string_view{"pvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
};
const StrictTypeSlot kStrictSlots_vec_get_y_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_get_y_input_0,
    .paramName = std::string_view{"object"},
  },
};
const StrictMode kStrict_vec_get_y{
  .enabled = true,
  .inputTypes = kStrictSlots_vec_get_y_input,
};

const AssignmentCheck kAssign_vec_get_y{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in vec_get_y",
};
const Slot kInside_vec_get_y[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in vec_get_y"}},
};
const TableValidator kTable_vec_get_y{
  .assignment = &kAssign_vec_get_y,
  .tooFewInside = "vec_get_y requires 1 parameter",
  .inside = kInside_vec_get_y,
};

const std::string_view kStrictTypes_vec_get_z_input_0[] = {
  std::string_view{"pvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"dvector"},
};
const StrictTypeSlot kStrictSlots_vec_get_z_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_get_z_input_0,
    .paramName = std::string_view{"object"},
  },
};
const StrictMode kStrict_vec_get_z{
  .enabled = true,
  .inputTypes = kStrictSlots_vec_get_z_input,
};

const AssignmentCheck kAssign_vec_get_z{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in vec_get_z",
};
const Slot kInside_vec_get_z[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a valid variable in vec_get_z"}},
};
const TableValidator kTable_vec_get_z{
  .assignment = &kAssign_vec_get_z,
  .tooFewInside = "vec_get_z requires 1 parameter",
  .inside = kInside_vec_get_z,
};

const std::string_view kStrictTypes_vec_len_input_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_vec_len_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_len_input_0,
    .paramName = std::string_view{"input vector"},
  },
};
const StrictMode kStrict_vec_len{
  .enabled = true,
  .inputTypes = kStrictSlots_vec_len_input,
};

const AssignmentCheck kAssign_vec_len{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in vec_len",
};
const Slot kInside_vec_len[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter must be a normal variable in vec_len"}},
};
const TableValidator kTable_vec_len{
  .assignment = &kAssign_vec_len,
  .tooFewInside = "vec_len requires 1 parameter",
  .inside = kInside_vec_len,
};

const std::string_view kStrictTypes_vec_rev_after_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_vec_rev_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_rev_after_0,
    .paramName = std::string_view{"input vector to reverse"},
  },
};
const StrictMode kStrict_vec_rev{
  .enabled = true,
  .afterParamTypes = kStrictSlots_vec_rev_after,
};

const Slot kInside_vec_rev[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in vec_rev"}},
};
const Slot kAfter_vec_rev[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid variable in vec_rev"}},
};
const TableValidator kTable_vec_rev{
  .tooFewInside = "vec_rev requires 1 parameter inside parentheses",
  .tooFewAfter = "vec_rev requires 1 parameter after parentheses",
  .inside = kInside_vec_rev,
  .after = kAfter_vec_rev,
};

const std::string_view kStrictTypes_vec_same_input_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_vec_same_input_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_vec_same_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_same_input_0,
    .paramName = std::string_view{"input vector 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_vec_same_input_1,
    .paramName = std::string_view{"input vector 2"},
  },
};
const StrictMode kStrict_vec_same{
  .enabled = true,
  .inputTypes = kStrictSlots_vec_same_input,
};

const AssignmentCheck kAssign_vec_same{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in vec_same",
};
const Slot kInside_vec_same[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid variable in vec_same"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 must be a valid variable in vec_same"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 must be a valid variable in vec_same"}},
};
const TableValidator kTable_vec_same{
  .assignment = &kAssign_vec_same,
  .tooFewInside = "vec_same requires 3 parameters",
  .inside = kInside_vec_same,
};

const std::string_view kStrictTypes_vec_sub_after_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
  std::string_view{"array"},
};
const std::string_view kStrictTypes_vec_sub_after_1[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_vec_sub_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_sub_after_0,
    .paramName = std::string_view{"input vector 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_vec_sub_after_1,
    .paramName = std::string_view{"input vector 2"},
  },
};
const StrictMode kStrict_vec_sub{
  .enabled = true,
  .afterParamTypes = kStrictSlots_vec_sub_after,
};

const Slot kInside_vec_sub[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"vec_sub parameter inside parentheses must be a valid variable"}},
};
const Slot kAfter_vec_sub[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in vec_sub"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable in vec_sub"}},
};
const TableValidator kTable_vec_sub{
  .tooFewInside = "vec_sub requires 1 parameter inside parentheses",
  .tooFewAfter = "vec_sub requires 2 parameters after parentheses",
  .inside = kInside_vec_sub,
  .after = kAfter_vec_sub,
};

const std::string_view kStrictTypes_vec_times_scalar_input_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_vec_times_scalar_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_times_scalar_input_0,
    .paramName = std::string_view{"output vector"},
  },
};
const std::string_view kStrictTypes_vec_times_scalar_after_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const std::string_view kStrictTypes_vec_times_scalar_after_1[] = {
  std::string_view{"value"},
  std::string_view{"string"},
  std::string_view{"array"},
};
const StrictTypeSlot kStrictSlots_vec_times_scalar_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_times_scalar_after_0,
    .paramName = std::string_view{"input vector"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_vec_times_scalar_after_1,
    .paramName = std::string_view{"input scalar value"},
  },
};
const StrictMode kStrict_vec_times_scalar{
  .enabled = true,
  .inputTypes = kStrictSlots_vec_times_scalar_input,
  .afterParamTypes = kStrictSlots_vec_times_scalar_after,
};

const Slot kInside_vec_times_scalar[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in vec_times_scalar"}},
};
const Slot kAfter_vec_times_scalar[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"First parameter after parentheses must be a valid variable in vec_times_scalar"}},
  {.index = 1, .kind = SlotKind::AnyVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Second parameter after parentheses must be a valid variable or stringvariable in vec_times_scalar"}},
};
const TableValidator kTable_vec_times_scalar{
  .tooFewInside = "vec_times_scalar requires 1 parameter inside parentheses",
  .tooFewAfter = "vec_times_scalar requires 2 parameters after parentheses",
  .inside = kInside_vec_times_scalar,
  .after = kAfter_vec_times_scalar,
};

const std::string_view kStrictTypes_vec_to_after_0[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_vec_to_after_1[] = {
  std::string_view{"value"},
};
const std::string_view kStrictTypes_vec_to_after_2[] = {
  std::string_view{"value"},
};
const StrictTypeSlot kStrictSlots_vec_to_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_to_after_0,
    .paramName = std::string_view{"input value 1"},
  },
  {
    .paramIndex = 1,
    .allowedTypes = kStrictTypes_vec_to_after_1,
    .paramName = std::string_view{"input value 2"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_vec_to_after_2,
    .paramName = std::string_view{"input value 3"},
  },
};
const StrictMode kStrict_vec_to{
  .enabled = true,
  .afterParamTypes = kStrictSlots_vec_to_after,
};

const Slot kInside_vec_to[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid variable in vec_to"}},
};
const Slot kAfter_vec_to[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 after parentheses must be a valid variable in vec_to"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 after parentheses must be a valid variable in vec_to"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 after parentheses must be a valid variable in vec_to"}},
};
const TableValidator kTable_vec_to{
  .tooFewInside = "vec_to requires 1 parameter inside parentheses",
  .tooFewAfter = "vec_to requires 3 parameters after parentheses",
  .inside = kInside_vec_to,
  .after = kAfter_vec_to,
};

const std::string_view kStrictTypes_vec_unit_after_0[] = {
  std::string_view{"dvector"},
  std::string_view{"xvector"},
  std::string_view{"yvector"},
  std::string_view{"zvector"},
  std::string_view{"nvector"},
  std::string_view{"pvector"},
};
const StrictTypeSlot kStrictSlots_vec_unit_after[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vec_unit_after_0,
    .paramName = std::string_view{"input vector"},
  },
};
const StrictMode kStrict_vec_unit{
  .enabled = true,
  .afterParamTypes = kStrictSlots_vec_unit_after,
};

const Slot kInside_vec_unit[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"vec_unit parameter inside parentheses must be a valid variable"}},
};
const Slot kAfter_vec_unit[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter after parentheses must be a valid variable in vec_unit"}},
};
const TableValidator kTable_vec_unit{
  .tooFewInside = "vec_unit requires 1 parameter inside parentheses",
  .tooFewAfter = "vec_unit requires 1 parameter after parentheses",
  .inside = kInside_vec_unit,
  .after = kAfter_vec_unit,
};

const std::string_view kStrictTypes_vxgeom_cross_pnts_input_0[] = {
  std::string_view{"vxgeom"},
};
const std::string_view kStrictTypes_vxgeom_cross_pnts_input_1[] = {
  std::string_view{"vxgeom"},
};
const StrictTypeSlot kStrictSlots_vxgeom_cross_pnts_input[] = {
  {
    .paramIndex = 0,
    .allowedTypes = kStrictTypes_vxgeom_cross_pnts_input_0,
    .paramName = std::string_view{"first vxgeom"},
  },
  {
    .paramIndex = 2,
    .allowedTypes = kStrictTypes_vxgeom_cross_pnts_input_1,
    .paramName = std::string_view{"second vxgeom"},
  },
};
const StrictMode kStrict_vxgeom_cross_pnts{
  .enabled = true,
  .inputTypes = kStrictSlots_vxgeom_cross_pnts_input,
};

const AssignmentCheck kAssign_vxgeom_cross_pnts{
  .presence = AssignmentPresence::Optional,
  .hasKind = true,
  .kind = SlotKind::NormalVariable,
  .variableKind = VariableKind::None,
  .invalid = "Assignment variable must be a normal variable in vxgeom_cross_pnts",
};
const Slot kInside_vxgeom_cross_pnts[] = {
  {.index = 0, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 1 must be a valid variable in vxgeom_cross_pnts"}},
  {.index = 1, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 2 must be a valid variable in vxgeom_cross_pnts"}},
  {.index = 2, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 3 must be a valid variable in vxgeom_cross_pnts"}},
  {.index = 3, .kind = SlotKind::NormalVariable, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter 4 must be a valid variable in vxgeom_cross_pnts"}},
};
const TableValidator kTable_vxgeom_cross_pnts{
  .assignment = &kAssign_vxgeom_cross_pnts,
  .tooFewInside = "vxgeom_cross_pnts requires 4 parameters",
  .inside = kInside_vxgeom_cross_pnts,
};

const AssignmentCheck kAssign_vxsettings_key_value{
  .presence = AssignmentPresence::Required,
  .hasKind = true,
  .kind = SlotKind::StringVariable,
  .variableKind = VariableKind::None,
  .missing = "vxsettings_key_value requires an assignment variable",
  .invalid = "Assignment variable must be a valid string variable in vxsettings_key_value",
};
const Slot kInside_vxsettings_key_value[] = {
  {.index = 0, .kind = SlotKind::StringExpression, .variableKind = VariableKind::None, .required = true, .invalid = std::string_view{"Parameter inside parentheses must be a valid string in vxsettings_key_value"}},
};
const TableValidator kTable_vxsettings_key_value{
  .assignment = &kAssign_vxsettings_key_value,
  .tooFewInside = "vxsettings_key_value requires 1 parameter inside parentheses",
  .inside = kInside_vxsettings_key_value,
};

} // namespace

const CommandCatalogEntry kCommands[kCommandCount] = {
{
  .name = std::string_view{"abs"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_abs,
  .customValidator = CustomValidatorId::Abs,
},
{
  .name = std::string_view{"access"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_access,
  .tableValidator = &kTable_access,
},
{
  .name = std::string_view{"access_custom"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_access_custom,
  .tableValidator = &kTable_access_custom,
},
{
  .name = std::string_view{"acos"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_acos,
  .tableValidator = &kTable_acos,
},
{
  .name = std::string_view{"add_area_frmdata"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_add_area_frmdata,
  .tableValidator = &kTable_add_area_frmdata,
},
{
  .name = std::string_view{"add_aux_task"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::unbounded(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_add_aux_task,
  .tableValidator = &kTable_add_aux_task,
},
{
  .name = std::string_view{"add_comp_member"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_add_comp_member,
  .tableValidator = &kTable_add_comp_member,
},
{
  .name = std::string_view{"add_edge"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(4), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_add_edge,
  .tableValidator = &kTable_add_edge,
},
{
  .name = std::string_view{"add_sobj"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_add_sobj,
  .tableValidator = &kTable_add_sobj,
},
{
  .name = std::string_view{"ascii"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_ascii,
},
{
  .name = std::string_view{"ask"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_ask,
},
{
  .name = std::string_view{"askdim"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_askdim,
  .tableValidator = &kTable_askdim,
},
{
  .name = std::string_view{"atan"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_atan,
  .tableValidator = &kTable_atan,
},
{
  .name = std::string_view{"atan2"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_atan2,
  .tableValidator = &kTable_atan2,
},
{
  .name = std::string_view{"base_to_dwg"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_base_to_dwg,
  .tableValidator = &kTable_base_to_dwg,
},
{
  .name = std::string_view{"build"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_build,
},
{
  .name = std::string_view{"change_sheet"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_change_sheet,
  .tableValidator = &kTable_change_sheet,
},
{
  .name = std::string_view{"close"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_close,
},
{
  .name = std::string_view{"cmp"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_cmp,
},
{
  .name = std::string_view{"cmpi"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_cmpi,
},
{
  .name = std::string_view{"common_dbases"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .strictMode = &kStrict_common_dbases,
  .tableValidator = &kTable_common_dbases,
},
{
  .name = std::string_view{"common_elements"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .strictMode = &kStrict_common_elements,
  .tableValidator = &kTable_common_elements,
},
{
  .name = std::string_view{"common_files"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .strictMode = &kStrict_common_files,
  .tableValidator = &kTable_common_files,
},
{
  .name = std::string_view{"common_indexes"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .strictMode = &kStrict_common_indexes,
  .tableValidator = &kTable_common_indexes,
},
{
  .name = std::string_view{"common_points"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .strictMode = &kStrict_common_points,
  .tableValidator = &kTable_common_points,
},
{
  .name = std::string_view{"connect"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_connect,
  .tableValidator = &kTable_connect,
},
{
  .name = std::string_view{"copy_piece_pres"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_copy_piece_pres,
  .tableValidator = &kTable_copy_piece_pres,
},
{
  .name = std::string_view{"cube"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_cube,
  .tableValidator = &kTable_cube,
},
{
  .name = std::string_view{"current_user"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_current_user,
},
{
  .name = std::string_view{"dbadd"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbadd,
  .tableValidator = &kTable_dbadd,
},
{
  .name = std::string_view{"dbaddrow"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbaddrow,
  .tableValidator = &kTable_dbaddrow,
},
{
  .name = std::string_view{"dbalter"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::omitted(), .maxParamsInside = Arity::omitted(), .paramsOutside = Arity::finite(4), .maxParamsOutside = Arity::finite(99), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .strictMode = &kStrict_dbalter,
  .tableValidator = &kTable_dbalter,
},
{
  .name = std::string_view{"dbbind"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .strictMode = &kStrict_dbbind,
  .tableValidator = &kTable_dbbind,
},
{
  .name = std::string_view{"dbbutton_disable"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_dbbutton_disable,
},
{
  .name = std::string_view{"dbchanged"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbchanged,
  .tableValidator = &kTable_dbchanged,
},
{
  .name = std::string_view{"dbclose"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbclose,
  .tableValidator = &kTable_dbclose,
},
{
  .name = std::string_view{"dbclose_by_name"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_dbclose_by_name,
},
{
  .name = std::string_view{"dbcontrol_disable"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = false, .minParamsInside = Arity::finite(4), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbcontrol_disable,
  .tableValidator = &kTable_dbcontrol_disable,
},
{
  .name = std::string_view{"dbcontrol_hide"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = false, .minParamsInside = Arity::finite(4), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbcontrol_hide,
  .tableValidator = &kTable_dbcontrol_hide,
},
{
  .name = std::string_view{"dbcopyrow"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(4), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbcopyrow,
  .tableValidator = &kTable_dbcopyrow,
},
{
  .name = std::string_view{"dbcreate"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbcreate,
  .customValidator = CustomValidatorId::Dbcreate,
},
{
  .name = std::string_view{"dbdebug"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbdebug,
  .tableValidator = &kTable_dbdebug,
},
{
  .name = std::string_view{"dbdefsellist"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbdefsellist,
  .tableValidator = &kTable_dbdefsellist,
},
{
  .name = std::string_view{"dbdelete"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_dbdelete,
},
{
  .name = std::string_view{"dbdelrow"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbdelrow,
  .tableValidator = &kTable_dbdelrow,
},
{
  .name = std::string_view{"dbdelrowsl"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbdelrowsl,
  .tableValidator = &kTable_dbdelrowsl,
},
{
  .name = std::string_view{"dbdialog"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_dbdialog,
},
{
  .name = std::string_view{"dbdump_to_xml"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::omitted(), .maxParamsInside = Arity::omitted(), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_dbdump_to_xml,
},
{
  .name = std::string_view{"dbedit"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbedit,
  .tableValidator = &kTable_dbedit,
},
{
  .name = std::string_view{"dbeditfrm"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbeditfrm,
  .tableValidator = &kTable_dbeditfrm,
},
{
  .name = std::string_view{"dbemptyf"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbemptyf,
  .tableValidator = &kTable_dbemptyf,
},
{
  .name = std::string_view{"dbequalrows"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = false, .minParamsInside = Arity::finite(5), .maxParamsInside = Arity::finite(5), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbequalrows,
  .tableValidator = &kTable_dbequalrows,
},
{
  .name = std::string_view{"dbfenable"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::omitted(), .maxParamsInside = Arity::omitted(), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbfenable,
  .tableValidator = &kTable_dbfenable,
},
{
  .name = std::string_view{"dbfield_bgcolor"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = false, .minParamsInside = Arity::finite(7), .maxParamsInside = Arity::finite(7), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbfield_bgcolor,
  .tableValidator = &kTable_dbfield_bgcolor,
},
{
  .name = std::string_view{"dbfind"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbfind,
  .tableValidator = &kTable_dbfind,
},
{
  .name = std::string_view{"dbgetactrow"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_dbgetactrow,
},
{
  .name = std::string_view{"dbgetall"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbgetall,
  .tableValidator = &kTable_dbgetall,
},
{
  .name = std::string_view{"dbgetf"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbgetf,
  .tableValidator = &kTable_dbgetf,
},
{
  .name = std::string_view{"dbgetrid"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbgetrid,
  .tableValidator = &kTable_dbgetrid,
},
{
  .name = std::string_view{"dbladd"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbladd,
  .tableValidator = &kTable_dbladd,
},
{
  .name = std::string_view{"dbladdl"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbladdl,
  .tableValidator = &kTable_dbladdl,
},
{
  .name = std::string_view{"dblalreadyopen"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblalreadyopen,
  .tableValidator = &kTable_dblalreadyopen,
},
{
  .name = std::string_view{"dblclear"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblclear,
  .tableValidator = &kTable_dblclear,
},
{
  .name = std::string_view{"dblclose"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblclose,
  .tableValidator = &kTable_dblclose,
},
{
  .name = std::string_view{"dbldebug"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbldebug,
  .tableValidator = &kTable_dbldebug,
},
{
  .name = std::string_view{"dbldell"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbldell,
  .tableValidator = &kTable_dbldell,
},
{
  .name = std::string_view{"dbldelrid"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbldelrid,
  .tableValidator = &kTable_dbldelrid,
},
{
  .name = std::string_view{"dbledit"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbledit,
  .tableValidator = &kTable_dbledit,
},
{
  .name = std::string_view{"dbledit2"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(6), .maxParamsInside = Arity::finite(6), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbledit2,
  .tableValidator = &kTable_dbledit2,
},
{
  .name = std::string_view{"dbledit2_unrelated"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(5), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbledit2_unrelated,
  .tableValidator = &kTable_dbledit2_unrelated,
},
{
  .name = std::string_view{"dblfind"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblfind,
  .tableValidator = &kTable_dblfind,
},
{
  .name = std::string_view{"dblgetval"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(4), .maxParamsInside = Arity::finite(5), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblgetval,
  .tableValidator = &kTable_dblgetval,
},
{
  .name = std::string_view{"dblist"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblist,
  .tableValidator = &kTable_dblist,
},
{
  .name = std::string_view{"dblistnum"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblistnum,
  .tableValidator = &kTable_dblistnum,
},
{
  .name = std::string_view{"dblopen"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblopen,
  .tableValidator = &kTable_dblopen,
},
{
  .name = std::string_view{"dblopenadd"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::omitted(), .maxParamsInside = Arity::omitted(), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblopenadd,
  .tableValidator = &kTable_dblopenadd,
},
{
  .name = std::string_view{"dblopendel"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::omitted(), .maxParamsInside = Arity::omitted(), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblopendel,
  .tableValidator = &kTable_dblopendel,
},
{
  .name = std::string_view{"dblopensort"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblopensort,
  .tableValidator = &kTable_dblopensort,
},
{
  .name = std::string_view{"dblprint"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(4), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblprint,
  .tableValidator = &kTable_dblprint,
},
{
  .name = std::string_view{"dblselect"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(6), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblselect,
  .tableValidator = &kTable_dblselect,
},
{
  .name = std::string_view{"dblselectn"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(7), .maxParamsInside = Arity::finite(7), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_dblselectn,
},
{
  .name = std::string_view{"dblsort"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblsort,
  .tableValidator = &kTable_dblsort,
},
{
  .name = std::string_view{"dblstore"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblstore,
  .tableValidator = &kTable_dblstore,
},
{
  .name = std::string_view{"dblvalid"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dblvalid,
  .tableValidator = &kTable_dblvalid,
},
{
  .name = std::string_view{"dbname"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbname,
  .tableValidator = &kTable_dbname,
},
{
  .name = std::string_view{"dbopen"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbopen,
  .tableValidator = &kTable_dbopen,
},
{
  .name = std::string_view{"dbpage"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbpage,
  .tableValidator = &kTable_dbpage,
},
{
  .name = std::string_view{"dbpage2"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbpage2,
  .tableValidator = &kTable_dbpage2,
},
{
  .name = std::string_view{"dbpicformlist"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(6), .maxParamsOutside = Arity::finite(6), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbpicformlist,
  .tableValidator = &kTable_dbpicformlist,
},
{
  .name = std::string_view{"dbprint"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(4), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbprint,
  .tableValidator = &kTable_dbprint,
},
{
  .name = std::string_view{"dbputall"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbputall,
  .tableValidator = &kTable_dbputall,
},
{
  .name = std::string_view{"dbputf"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbputf,
  .tableValidator = &kTable_dbputf,
},
{
  .name = std::string_view{"dbrollout"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbrollout,
  .tableValidator = &kTable_dbrollout,
},
{
  .name = std::string_view{"dbrowstatus"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbrowstatus,
  .tableValidator = &kTable_dbrowstatus,
},
{
  .name = std::string_view{"dbsave"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbsave,
  .tableValidator = &kTable_dbsave,
},
{
  .name = std::string_view{"dbselect"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(6), .maxParamsOutside = Arity::finite(6), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbselect,
  .tableValidator = &kTable_dbselect,
},
{
  .name = std::string_view{"dbselector_inp"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbselector_inp,
  .tableValidator = &kTable_dbselector_inp,
},
{
  .name = std::string_view{"dbsetactrow"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::omitted(), .maxParamsInside = Arity::omitted(), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbsetactrow,
  .tableValidator = &kTable_dbsetactrow,
},
{
  .name = std::string_view{"dbtranslate"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(4), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbtranslate,
  .tableValidator = &kTable_dbtranslate,
},
{
  .name = std::string_view{"dbunset"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbunset,
  .tableValidator = &kTable_dbunset,
},
{
  .name = std::string_view{"dbvalid"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dbvalid,
  .tableValidator = &kTable_dbvalid,
},
{
  .name = std::string_view{"debug"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .tableValidator = &kTable_debug,
},
{
  .name = std::string_view{"del_joint_det"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_del_joint_det,
  .tableValidator = &kTable_del_joint_det,
},
{
  .name = std::string_view{"del_object"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_del_object,
  .tableValidator = &kTable_del_object,
},
{
  .name = std::string_view{"disconnect"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_disconnect,
  .tableValidator = &kTable_disconnect,
},
{
  .name = std::string_view{"drop_poly"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_drop_poly,
  .tableValidator = &kTable_drop_poly,
},
{
  .name = std::string_view{"dwg_to_base"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dwg_to_base,
  .tableValidator = &kTable_dwg_to_base,
},
{
  .name = std::string_view{"dwg_to_mm"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_dwg_to_mm,
  .tableValidator = &kTable_dwg_to_mm,
},
{
  .name = std::string_view{"eof"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_eof,
},
{
  .name = std::string_view{"exit"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .tableValidator = &kTable_exit,
},
{
  .name = std::string_view{"filecopy"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::omitted(), .maxParamsInside = Arity::omitted(), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .strictMode = &kStrict_filecopy,
  .tableValidator = &kTable_filecopy,
},
{
  .name = std::string_view{"filepath_custom"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_filepath_custom,
},
{
  .name = std::string_view{"findstr"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = true},
  .tableValidator = &kTable_findstr,
},
{
  .name = std::string_view{"findstri"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = true},
  .tableValidator = &kTable_findstri,
},
{
  .name = std::string_view{"findstrr"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = true},
  .strictMode = &kStrict_findstrr,
  .tableValidator = &kTable_findstrr,
},
{
  .name = std::string_view{"findstrri"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = true},
  .strictMode = &kStrict_findstrri,
  .tableValidator = &kTable_findstrri,
},
{
  .name = std::string_view{"get"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_get,
},
{
  .name = std::string_view{"get_actual_area"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_actual_area,
  .tableValidator = &kTable_get_actual_area,
},
{
  .name = std::string_view{"get_actual_pcend"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_actual_pcend,
  .tableValidator = &kTable_get_actual_pcend,
},
{
  .name = std::string_view{"get_area_clips_with_edges"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(4), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_area_clips_with_edges,
  .tableValidator = &kTable_get_area_clips_with_edges,
},
{
  .name = std::string_view{"get_area_param"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_area_param,
  .customValidator = CustomValidatorId::GetAreaParam,
},
{
  .name = std::string_view{"get_area_plane"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_area_plane,
  .tableValidator = &kTable_get_area_plane,
},
{
  .name = std::string_view{"get_ctr_edge"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_ctr_edge,
  .tableValidator = &kTable_get_ctr_edge,
},
{
  .name = std::string_view{"get_floor_level_name"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_floor_level_name,
  .tableValidator = &kTable_get_floor_level_name,
},
{
  .name = std::string_view{"get_joint_param"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_joint_param,
  .tableValidator = &kTable_get_joint_param,
},
{
  .name = std::string_view{"get_macro_param"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_macro_param,
  .tableValidator = &kTable_get_macro_param,
},
{
  .name = std::string_view{"get_material_param"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_material_param,
  .tableValidator = &kTable_get_material_param,
},
{
  .name = std::string_view{"get_obj_param"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_get_obj_param,
},
{
  .name = std::string_view{"get_obj_size"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(7), .maxParamsInside = Arity::finite(7), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_obj_size,
  .tableValidator = &kTable_get_obj_size,
},
{
  .name = std::string_view{"get_object_type"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_get_object_type,
},
{
  .name = std::string_view{"get_origin"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_origin,
  .tableValidator = &kTable_get_origin,
},
{
  .name = std::string_view{"get_origin_pnt"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_origin_pnt,
  .tableValidator = &kTable_get_origin_pnt,
},
{
  .name = std::string_view{"get_origin_pos"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(4), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_origin_pos,
  .tableValidator = &kTable_get_origin_pos,
},
{
  .name = std::string_view{"get_panel"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_panel,
  .tableValidator = &kTable_get_panel,
},
{
  .name = std::string_view{"get_panel_core"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_panel_core,
  .tableValidator = &kTable_get_panel_core,
},
{
  .name = std::string_view{"get_parent"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_parent,
  .tableValidator = &kTable_get_parent,
},
{
  .name = std::string_view{"get_piece_dir"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_piece_dir,
  .tableValidator = &kTable_get_piece_dir,
},
{
  .name = std::string_view{"get_piece_end_trims"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_piece_end_trims,
  .tableValidator = &kTable_get_piece_end_trims,
},
{
  .name = std::string_view{"get_piece_tan"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_piece_tan,
  .tableValidator = &kTable_get_piece_tan,
},
{
  .name = std::string_view{"get_poly_ctr"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_poly_ctr,
  .tableValidator = &kTable_get_poly_ctr,
},
{
  .name = std::string_view{"get_poly_lim"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(4), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_poly_lim,
  .tableValidator = &kTable_get_poly_lim,
},
{
  .name = std::string_view{"get_section_param"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_section_param,
  .tableValidator = &kTable_get_section_param,
},
{
  .name = std::string_view{"get_sheet_ctr"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_sheet_ctr,
  .tableValidator = &kTable_get_sheet_ctr,
},
{
  .name = std::string_view{"get_sheet_type"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_sheet_type,
  .tableValidator = &kTable_get_sheet_type,
},
{
  .name = std::string_view{"get_struct_core"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_struct_core,
  .tableValidator = &kTable_get_struct_core,
},
{
  .name = std::string_view{"get_vxgeom"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_vxgeom,
  .tableValidator = &kTable_get_vxgeom,
},
{
  .name = std::string_view{"get_vxgeom_len"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_vxgeom_len,
  .tableValidator = &kTable_get_vxgeom_len,
},
{
  .name = std::string_view{"get_vxgeom_point"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_vxgeom_point,
  .tableValidator = &kTable_get_vxgeom_point,
},
{
  .name = std::string_view{"get_vxgeom_tan"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_vxgeom_tan,
  .tableValidator = &kTable_get_vxgeom_tan,
},
{
  .name = std::string_view{"get_vxtype_main"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_vxtype_main,
  .tableValidator = &kTable_get_vxtype_main,
},
{
  .name = std::string_view{"get_wall_from_wpnl"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_wall_from_wpnl,
  .tableValidator = &kTable_get_wall_from_wpnl,
},
{
  .name = std::string_view{"get_wall_type"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_get_wall_type,
  .tableValidator = &kTable_get_wall_type,
},
{
  .name = std::string_view{"inch_to_dwg"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_inch_to_dwg,
  .tableValidator = &kTable_inch_to_dwg,
},
{
  .name = std::string_view{"init_iterator"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_init_iterator,
  .tableValidator = &kTable_init_iterator,
},
{
  .name = std::string_view{"int"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_int,
},
{
  .name = std::string_view{"int_part"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_int_part,
},
{
  .name = std::string_view{"is_adapt_act_pos"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_is_adapt_act_pos,
  .tableValidator = &kTable_is_adapt_act_pos,
},
{
  .name = std::string_view{"is_crossing_pce"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_is_crossing_pce,
  .tableValidator = &kTable_is_crossing_pce,
},
{
  .name = std::string_view{"is_in_vxgeom"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(4), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_is_in_vxgeom,
  .tableValidator = &kTable_is_in_vxgeom,
},
{
  .name = std::string_view{"is_point_inside"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_is_point_inside,
  .tableValidator = &kTable_is_point_inside,
},
{
  .name = std::string_view{"is_struct_in_pdm"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_is_struct_in_pdm,
},
{
  .name = std::string_view{"len"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_len,
},
{
  .name = std::string_view{"map_add_map"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_add_map,
},
{
  .name = std::string_view{"map_clear"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_clear,
},
{
  .name = std::string_view{"map_cmp"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_cmp,
},
{
  .name = std::string_view{"map_del_map"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_del_map,
},
{
  .name = std::string_view{"map_diff_keys"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_diff_keys,
},
{
  .name = std::string_view{"map_edit"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_edit,
},
{
  .name = std::string_view{"map_find_key"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_find_key,
},
{
  .name = std::string_view{"map_find_value"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_find_value,
},
{
  .name = std::string_view{"map_from_dblist"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_map_from_dblist,
  .tableValidator = &kTable_map_from_dblist,
},
{
  .name = std::string_view{"map_from_dbrow"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_map_from_dbrow,
  .tableValidator = &kTable_map_from_dbrow,
},
{
  .name = std::string_view{"map_get"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_get,
},
{
  .name = std::string_view{"map_get_first"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_get_first,
},
{
  .name = std::string_view{"map_get_next"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_get_next,
},
{
  .name = std::string_view{"map_key_at"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_key_at,
},
{
  .name = std::string_view{"map_read"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_map_read,
},
{
  .name = std::string_view{"map_save"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_map_save,
  .tableValidator = &kTable_map_save,
},
{
  .name = std::string_view{"map_size"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(0), .maxParamsOutside = Arity::finite(0), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_map_size,
  .tableValidator = &kTable_map_size,
},
{
  .name = std::string_view{"map_to_dbrow"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_map_to_dbrow,
  .tableValidator = &kTable_map_to_dbrow,
},
{
  .name = std::string_view{"messagebox"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_messagebox,
},
{
  .name = std::string_view{"milliseconds_since_1970"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_milliseconds_since_1970,
},
{
  .name = std::string_view{"mm_to_dwg"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_mm_to_dwg,
},
{
  .name = std::string_view{"new_djoint"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(8), .maxParamsInside = Arity::finite(8), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_new_djoint,
  .tableValidator = &kTable_new_djoint,
},
{
  .name = std::string_view{"new_feature"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::unbounded(), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_new_feature,
},
{
  .name = std::string_view{"new_iterator"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .customValidator = CustomValidatorId::NewIterator,
},
{
  .name = std::string_view{"new_macro"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(8), .maxParamsInside = Arity::finite(10), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_new_macro,
  .tableValidator = &kTable_new_macro,
},
{
  .name = std::string_view{"new_ncaccessory"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(9), .maxParamsInside = Arity::unbounded(), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_new_ncaccessory,
  .tableValidator = &kTable_new_ncaccessory,
},
{
  .name = std::string_view{"new_piece"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(8), .maxParamsInside = Arity::finite(10), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_new_piece,
  .tableValidator = &kTable_new_piece,
},
{
  .name = std::string_view{"new_piece2"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_new_piece2,
  .tableValidator = &kTable_new_piece2,
},
{
  .name = std::string_view{"new_sheet"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(6), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_new_sheet,
  .tableValidator = &kTable_new_sheet,
},
{
  .name = std::string_view{"new_vxgeom"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_new_vxgeom,
  .tableValidator = &kTable_new_vxgeom,
},
{
  .name = std::string_view{"next_item"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_next_item,
},
{
  .name = std::string_view{"numeric_string"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_numeric_string,
},
{
  .name = std::string_view{"open"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_open,
},
{
  .name = std::string_view{"pnt_inside_poly"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_pnt_inside_poly,
  .tableValidator = &kTable_pnt_inside_poly,
},
{
  .name = std::string_view{"pnt_to_plane"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(5), .maxParamsInside = Arity::finite(5), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_pnt_to_plane,
  .customValidator = CustomValidatorId::PntToPlane,
},
{
  .name = std::string_view{"point_to_vxgeom"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_point_to_vxgeom,
  .tableValidator = &kTable_point_to_vxgeom,
},
{
  .name = std::string_view{"points_to_poly"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_points_to_poly,
  .tableValidator = &kTable_points_to_poly,
},
{
  .name = std::string_view{"proj_to_piece"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(7), .maxParamsInside = Arity::finite(7), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_proj_to_piece,
  .tableValidator = &kTable_proj_to_piece,
},
{
  .name = std::string_view{"read"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_read,
},
{
  .name = std::string_view{"replace"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_replace,
},
{
  .name = std::string_view{"section_to_poly"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(5), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_section_to_poly,
  .tableValidator = &kTable_section_to_poly,
},
{
  .name = std::string_view{"set"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = true, .valueExpression = false},
  .tableValidator = &kTable_set,
},
{
  .name = std::string_view{"set_base_label"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_base_label,
  .tableValidator = &kTable_set_base_label,
},
{
  .name = std::string_view{"set_djmember"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_djmember,
  .tableValidator = &kTable_set_djmember,
},
{
  .name = std::string_view{"set_djparam"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_djparam,
  .cleanupRules = &kCleanup_set_djparam,
  .tableValidator = &kTable_set_djparam,
},
{
  .name = std::string_view{"set_joint_param"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_joint_param,
  .cleanupRules = &kCleanup_set_joint_param,
  .tableValidator = &kTable_set_joint_param,
},
{
  .name = std::string_view{"set_joint_punch"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_joint_punch,
  .tableValidator = &kTable_set_joint_punch,
},
{
  .name = std::string_view{"set_macro_form"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_macro_form,
  .tableValidator = &kTable_set_macro_form,
},
{
  .name = std::string_view{"set_macro_param"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_macro_param,
  .tableValidator = &kTable_set_macro_param,
},
{
  .name = std::string_view{"set_obj_param"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .cleanupRules = &kCleanup_set_obj_param,
  .tableValidator = &kTable_set_obj_param,
},
{
  .name = std::string_view{"set_origin_pos"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(4), .maxParamsOutside = Arity::finite(4), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_origin_pos,
  .tableValidator = &kTable_set_origin_pos,
},
{
  .name = std::string_view{"set_piece_pos"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(6), .maxParamsInside = Arity::finite(6), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_piece_pos,
  .tableValidator = &kTable_set_piece_pos,
},
{
  .name = std::string_view{"set_piece_sect"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(6), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_piece_sect,
  .tableValidator = &kTable_set_piece_sect,
},
{
  .name = std::string_view{"set_piece_set"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_piece_set,
  .tableValidator = &kTable_set_piece_set,
},
{
  .name = std::string_view{"set_pnt_param"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_pnt_param,
  .tableValidator = &kTable_set_pnt_param,
},
{
  .name = std::string_view{"set_poly_pleq"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_poly_pleq,
  .tableValidator = &kTable_set_poly_pleq,
},
{
  .name = std::string_view{"set_regenerable"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_regenerable,
  .tableValidator = &kTable_set_regenerable,
},
{
  .name = std::string_view{"set_same_tlink"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_same_tlink,
  .tableValidator = &kTable_set_same_tlink,
},
{
  .name = std::string_view{"set_same_visib"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_same_visib,
  .tableValidator = &kTable_set_same_visib,
},
{
  .name = std::string_view{"set_sheet_shape"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::unbounded(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_sheet_shape,
  .tableValidator = &kTable_set_sheet_shape,
},
{
  .name = std::string_view{"set_visibility"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_set_visibility,
  .tableValidator = &kTable_set_visibility,
},
{
  .name = std::string_view{"shrink_poly"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_shrink_poly,
  .tableValidator = &kTable_shrink_poly,
},
{
  .name = std::string_view{"strfield"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_strfield,
},
{
  .name = std::string_view{"tan"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_tan,
},
{
  .name = std::string_view{"trim"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_trim,
},
{
  .name = std::string_view{"unset"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = false, .minParamsInside = Arity::finite(0), .maxParamsInside = Arity::finite(0), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_unset,
},
{
  .name = std::string_view{"upd_object_geom"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_upd_object_geom,
  .tableValidator = &kTable_upd_object_geom,
},
{
  .name = std::string_view{"vec_add"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_add,
  .tableValidator = &kTable_vec_add,
},
{
  .name = std::string_view{"vec_copy"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_copy,
  .tableValidator = &kTable_vec_copy,
},
{
  .name = std::string_view{"vec_cross_prod"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_cross_prod,
  .tableValidator = &kTable_vec_cross_prod,
},
{
  .name = std::string_view{"vec_dot_prod"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(2), .maxParamsInside = Arity::finite(2), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_dot_prod,
  .tableValidator = &kTable_vec_dot_prod,
},
{
  .name = std::string_view{"vec_get_x"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_get_x,
  .tableValidator = &kTable_vec_get_x,
},
{
  .name = std::string_view{"vec_get_y"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_get_y,
  .tableValidator = &kTable_vec_get_y,
},
{
  .name = std::string_view{"vec_get_z"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_get_z,
  .tableValidator = &kTable_vec_get_z,
},
{
  .name = std::string_view{"vec_len"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_len,
  .tableValidator = &kTable_vec_len,
},
{
  .name = std::string_view{"vec_rev"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_rev,
  .tableValidator = &kTable_vec_rev,
},
{
  .name = std::string_view{"vec_same"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(3), .maxParamsInside = Arity::finite(3), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_same,
  .tableValidator = &kTable_vec_same,
},
{
  .name = std::string_view{"vec_sub"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_sub,
  .tableValidator = &kTable_vec_sub,
},
{
  .name = std::string_view{"vec_times_scalar"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(2), .maxParamsOutside = Arity::finite(2), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_times_scalar,
  .tableValidator = &kTable_vec_times_scalar,
},
{
  .name = std::string_view{"vec_to"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(3), .maxParamsOutside = Arity::finite(3), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_to,
  .tableValidator = &kTable_vec_to,
},
{
  .name = std::string_view{"vec_unit"},
  .structure = {.hasAssignment = AssignmentPresence::Forbidden, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::finite(1), .maxParamsOutside = Arity::finite(1), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vec_unit,
  .tableValidator = &kTable_vec_unit,
},
{
  .name = std::string_view{"vxgeom_cross_pnts"},
  .structure = {.hasAssignment = AssignmentPresence::Optional, .hasParentheses = true, .minParamsInside = Arity::finite(4), .maxParamsInside = Arity::finite(4), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .strictMode = &kStrict_vxgeom_cross_pnts,
  .tableValidator = &kTable_vxgeom_cross_pnts,
},
{
  .name = std::string_view{"vxsettings_key_value"},
  .structure = {.hasAssignment = AssignmentPresence::Required, .hasParentheses = true, .minParamsInside = Arity::finite(1), .maxParamsInside = Arity::finite(1), .paramsOutside = Arity::omitted(), .maxParamsOutside = Arity::omitted(), .acceptSpacesAsSeparators = false, .valueExpression = false},
  .tableValidator = &kTable_vxsettings_key_value,
},
};

} // namespace vertexadt::catalog::generated
