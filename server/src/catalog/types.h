#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace vertexadt::catalog {

enum class AssignmentPresence : std::uint8_t
{
  Forbidden,
  Required,
  Optional,
};

enum class ArityKind : std::uint8_t
{
  Omitted,
  Finite,
  Unbounded,
};

struct Arity
{
  ArityKind kind = ArityKind::Omitted;
  int value = 0;

  static constexpr Arity omitted()
  {
    return {ArityKind::Omitted, 0};
  }

  static constexpr Arity unbounded()
  {
    return {ArityKind::Unbounded, 0};
  }

  static constexpr Arity finite(int n)
  {
    return {ArityKind::Finite, n};
  }

  constexpr bool isOmitted() const
  {
    return kind == ArityKind::Omitted;
  }

  constexpr bool isUnbounded() const
  {
    return kind == ArityKind::Unbounded;
  }
};

enum class SlotKind : std::uint8_t
{
  NormalVariable,
  StringVariable,
  AnyVariable,
  StringExpression,
  StringOrVariable,
  NumberOrVariable,
  DigitLiteralOrVariable,
};

enum class VariableKind : std::uint8_t
{
  None,
  AnyVariable,
  NormalVariable,
};

enum class QuoteRule : std::uint8_t
{
  Never,
  BareNumericLiteral,
};

enum class CustomValidatorId : std::uint8_t
{
  None,
  Abs,
  Dbcreate,
  GetAreaParam,
  NewIterator,
  PntToPlane,
};

struct CommandStructure
{
  AssignmentPresence hasAssignment = AssignmentPresence::Forbidden;
  bool hasParentheses = false;
  Arity minParamsInside = Arity::omitted();
  Arity maxParamsInside = Arity::omitted();
  Arity paramsOutside = Arity::omitted();
  Arity maxParamsOutside = Arity::omitted();
  bool acceptSpacesAsSeparators = false;
  // Native only (docs/adr/0011): in an assignment, an operator and a math
  // expression after the call's `)` belong to the assigned value.
  bool valueExpression = false;
};

struct Slot
{
  std::uint8_t index = 0;
  SlotKind kind = SlotKind::NormalVariable;
  VariableKind variableKind = VariableKind::None;
  bool required = false;
  std::string_view invalid{};
};

struct SlotTail
{
  std::uint8_t from = 0;
  SlotKind kind = SlotKind::NormalVariable;
  VariableKind variableKind = VariableKind::None;
  std::string_view invalid{};
};

struct AssignmentCheck
{
  AssignmentPresence presence = AssignmentPresence::Required;
  bool hasKind = false;
  SlotKind kind = SlotKind::NormalVariable;
  VariableKind variableKind = VariableKind::None;
  const char* missing = nullptr;
  const char* invalid = nullptr;
};

struct TableValidator
{
  const AssignmentCheck* assignment = nullptr;
  const char* tooFewInside = nullptr;
  const char* tooManyInside = nullptr;
  const char* tooFewAfter = nullptr;
  const char* tooManyAfter = nullptr;
  std::span<const Slot> inside{};
  std::span<const Slot> after{};
  const SlotTail* insideTail = nullptr;
  const SlotTail* afterTail = nullptr;
};

struct StrictTypeSlot
{
  std::uint8_t paramIndex = 0;
  std::span<const std::string_view> allowedTypes{};
  std::string_view paramName{};
};

struct StrictMode
{
  bool enabled = false;
  std::span<const StrictTypeSlot> inputTypes{};
  std::span<const StrictTypeSlot> afterParamTypes{};
};

struct CleanupSlot
{
  QuoteRule quote = QuoteRule::Never;
};

struct CleanupRules
{
  std::span<const CleanupSlot> afterParams{};
};

struct CommandCatalogEntry
{
  std::string_view name{};
  CommandStructure structure{};
  const StrictMode* strictMode = nullptr;
  const CleanupRules* cleanupRules = nullptr;
  const TableValidator* tableValidator = nullptr;
  CustomValidatorId customValidator = CustomValidatorId::None;
};

struct KeywordCatalogEntry
{
  std::string_view label{};
  std::string_view detail{};
  const char* documentation = nullptr;
  const char* insertText = nullptr;
};

struct NameType
{
  std::string_view name{};
  std::string_view type{};
};

struct IndexedType
{
  std::uint8_t index = 0;
  std::string_view type{};
};

struct CommandSlotTypes
{
  std::string_view command{};
  std::span<const IndexedType> slots{};
};

// Node parameterConfigurations: object-parameter completion's static rows.
struct ObjectParameterRow
{
  std::string_view label{};
  std::optional<std::string_view> documentation{};
  std::optional<std::string_view> detail{};
  std::optional<std::string_view> insertText{};
};

// One objectType key. An empty `rows` is a present empty list; it still wins.
struct ObjectParameterList
{
  std::string_view objectType{};
  std::span<const ObjectParameterRow> rows{};
};

struct ObjectParameterCommand
{
  std::string_view command{};
  std::span<const ObjectParameterList> lists{};
};

struct ValidatorDiagnostic
{
  std::string message{};
  bool warning = false;
};

struct CatalogLink
{
  std::size_t commandCount = 0;
  std::size_t tableValidatorCount = 0;
  std::size_t customValidatorCount = 0;
  std::size_t keywordCount = 0;
  std::size_t returnTypeCount = 0;
};

} // namespace vertexadt::catalog
