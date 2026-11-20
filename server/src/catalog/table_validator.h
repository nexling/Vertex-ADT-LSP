#pragma once

#include "types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vertexadt::catalog {

struct ParsedCommand
{
  std::optional<std::string> assignment;
  std::string functionName;
  std::vector<std::string> insideParams;
  std::vector<std::string> afterParams;
};

[[nodiscard]] auto findMatchingParenIndex(std::string_view text, std::size_t openParenIndex)
  -> std::ptrdiff_t;

[[nodiscard]] auto parseParameters(std::string_view paramString, bool acceptSpacesAsSeparators = false)
  -> std::vector<std::string>;

[[nodiscard]] auto parseCommand(
  std::string_view command,
  std::string_view trimmedLine,
  bool mustHaveAssignment) -> std::optional<ParsedCommand>;

[[nodiscard]] auto parseCommandNoParens(
  std::string_view command,
  std::string_view trimmedLine,
  bool mustHaveAssignment,
  bool acceptSpacesAsSeparators = false) -> std::optional<ParsedCommand>;

[[nodiscard]] bool isValidNormalVariable(std::string_view expr);
[[nodiscard]] bool isValidStringVariable(std::string_view expr);
[[nodiscard]] bool isValidAnyVariable(std::string_view expr);
[[nodiscard]] bool isValidStringExpression(std::string_view expr);
[[nodiscard]] bool isAsciiStringCommand(std::string_view expr);
[[nodiscard]] bool isValidCallPathParameter(std::string_view expr);
[[nodiscard]] bool isValidCommand(std::string_view expr);
[[nodiscard]] bool isValidVariableOrCommand(std::string_view expr);
[[nodiscard]] bool isValidMathExpression(std::string_view expr);

// True when `tail` (text after a call's `)` in an assigned value) continues
// that value: `+ - * /` and a math expression (alone or inside `before`, the
// value text ahead of the call), a comparison or whole-word and/or with a
// right-hand side, a `)` closing a `(` in `before`, or a lone `;`
// (docs/adr/0011-findstr-value-expressions.md).
[[nodiscard]] bool isValueExpressionTail(std::string_view before, std::string_view tail);

[[nodiscard]] auto validateCommandStructure(
  const ParsedCommand& parsed,
  const CommandStructure& structure,
  std::string_view commandName) -> std::vector<std::string>;

[[nodiscard]] auto findParenthesizedCommandSegments(std::string_view line, std::string_view commandName)
  -> std::vector<std::string>;

[[nodiscard]] auto validateCallCommandSyntax(std::string_view line) -> std::optional<std::string>;

// Stub/link entry: reads every table-validator field so the 240 compiled rows stay live.
std::optional<ValidatorDiagnostic> applyTableValidator(const TableValidator& row);
std::optional<ValidatorDiagnostic> applyTableValidator(
  const TableValidator& row,
  const ParsedCommand& parsed);
std::size_t tableValidatorFingerprint(const TableValidator& row);

} // namespace vertexadt::catalog
