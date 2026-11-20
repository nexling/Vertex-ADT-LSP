#include "custom_validators.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <vector>

namespace vertexadt::catalog {
namespace {

auto toLowerAscii(std::string_view text) -> std::string
{
  std::string out(text);
  for (char& ch : out)
  {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return out;
}

bool sameVar(std::string_view left, std::string_view right)
{
  return toLowerAscii(left) == toLowerAscii(right);
}

const std::regex& basicVarRe()
{
  static const std::regex re{R"(^[a-zA-Z][a-zA-Z0-9_]*$)"};
  return re;
}

const std::regex& fieldTypeRe()
{
  static const std::regex re{R"(^(?:A|N|B|T|M|U)$)", std::regex::icase};
  return re;
}

const std::regex& indexTypeRe()
{
  static const std::regex re{R"(^(?:x|ind|unique)$)", std::regex::icase};
  return re;
}

auto splitWhitespace(std::string_view text) -> std::vector<std::string>
{
  std::vector<std::string> tokens;
  std::string current;
  for (char ch : text)
  {
    if (std::isspace(static_cast<unsigned char>(ch)))
    {
      if (!current.empty())
      {
        tokens.push_back(std::move(current));
        current.clear();
      }
    }
    else
    {
      current.push_back(ch);
    }
  }
  if (!current.empty())
  {
    tokens.push_back(std::move(current));
  }
  return tokens;
}

auto replaceContinuations(std::string text) -> std::string
{
  static const std::regex continuationRe{R"(\s*\\\s*)"};
  return std::regex_replace(text, continuationRe, " ");
}

std::optional<std::string> validateDbcreateFieldDefinitions(std::string_view fieldDefinitionText)
{
  auto normalized = replaceContinuations(std::string(fieldDefinitionText));
  std::size_t begin = 0;
  while (begin < normalized.size() && std::isspace(static_cast<unsigned char>(normalized[begin])))
  {
    ++begin;
  }
  std::size_t end = normalized.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(normalized[end - 1])))
  {
    --end;
  }
  normalized = normalized.substr(begin, end - begin);
  if (normalized.empty())
  {
    return std::nullopt;
  }

  const auto fieldTokens = splitWhitespace(normalized);
  if (fieldTokens.size() % 4 != 0)
  {
    return "dbcreate field definitions must be provided in groups of: fieldname fieldtype length indtype";
  }

  for (std::size_t i = 0; i < fieldTokens.size(); i += 4)
  {
    const auto fieldIndex = static_cast<int>(i / 4) + 1;
    const auto& fieldName = fieldTokens[i];
    const auto& fieldType = fieldTokens[i + 1];
    const auto& fieldLength = fieldTokens[i + 2];
    const auto& fieldIndexType = fieldTokens[i + 3];

    if (!std::regex_match(fieldName, basicVarRe()))
    {
      return "dbcreate field " + std::to_string(fieldIndex) + " name must be a valid identifier";
    }
    if (!std::regex_match(fieldType, fieldTypeRe()))
    {
      return "dbcreate field " + std::to_string(fieldIndex) + " type must be one of A, N, B, T, M, or U";
    }
    if (!isValidNormalVariable(fieldLength))
    {
      return "dbcreate field " + std::to_string(fieldIndex) + " length must be a valid value";
    }
    if (!std::regex_match(fieldIndexType, indexTypeRe()))
    {
      return "dbcreate field " + std::to_string(fieldIndex) + " index type must be x, IND, or UNIQUE";
    }
  }
  return std::nullopt;
}

} // namespace

std::optional<ValidatorDiagnostic> validateAbs()
{
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> validateAbs(const ParsedCommand& parsed)
{
  if (parsed.assignment && !isValidNormalVariable(*parsed.assignment))
  {
    return ValidatorDiagnostic{.message = "Assignment variable must be a normal variable in abs"};
  }
  if (parsed.insideParams.size() < 1)
  {
    return ValidatorDiagnostic{.message = "abs requires 1 parameter"};
  }
  if (!isValidVariableOrCommand(parsed.insideParams[0]))
  {
    return ValidatorDiagnostic{.message = "Parameter must be a valid variable in abs"};
  }
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> validateDbcreate()
{
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> validateDbcreate(const ParsedCommand& parsed)
{
  if (parsed.insideParams.size() < 1)
  {
    return ValidatorDiagnostic{.message = "dbcreate requires 1 parameter inside parentheses"};
  }
  if (!isValidNormalVariable(parsed.insideParams[0]))
  {
    return ValidatorDiagnostic{.message = "Parameter inside parentheses must be a valid variable in dbcreate"};
  }
  if (parsed.afterParams.size() < 3)
  {
    return ValidatorDiagnostic{.message = "dbcreate requires at least 3 parameters after parentheses"};
  }
  if (!isValidStringExpression(parsed.afterParams[0]))
  {
    return ValidatorDiagnostic{.message = "First parameter after parentheses must be a valid string in dbcreate"};
  }
  const auto limit = std::min(parsed.afterParams.size(), static_cast<std::size_t>(3));
  for (std::size_t i = 1; i < limit; ++i)
  {
    if (!isValidNormalVariable(parsed.afterParams[i]))
    {
      return ValidatorDiagnostic{
        .message = "Parameter " + std::to_string(i + 1)
          + " after parentheses must be a valid variable in dbcreate",
      };
    }
  }
  std::string fieldDefinitions;
  for (std::size_t i = 3; i < parsed.afterParams.size(); ++i)
  {
    if (i > 3)
    {
      fieldDefinitions.push_back(' ');
    }
    fieldDefinitions += parsed.afterParams[i];
  }
  std::size_t begin = 0;
  while (begin < fieldDefinitions.size()
         && std::isspace(static_cast<unsigned char>(fieldDefinitions[begin])))
  {
    ++begin;
  }
  std::size_t end = fieldDefinitions.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(fieldDefinitions[end - 1])))
  {
    --end;
  }
  if (auto error = validateDbcreateFieldDefinitions(fieldDefinitions.substr(begin, end - begin)))
  {
    return ValidatorDiagnostic{.message = std::move(*error)};
  }
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> validateGetAreaParam()
{
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> validateGetAreaParam(const ParsedCommand& parsed)
{
  if (!parsed.assignment)
  {
    return ValidatorDiagnostic{.message = "get_area_param requires an assignment variable"};
  }
  if (!isValidStringVariable(*parsed.assignment) && !isValidNormalVariable(*parsed.assignment))
  {
    return ValidatorDiagnostic{
      .message = "Assignment variable must be a string or normal variable in get_area_param",
    };
  }
  if (parsed.insideParams.size() < 2)
  {
    return ValidatorDiagnostic{.message = "get_area_param requires at least 2 parameters"};
  }
  if (!isValidNormalVariable(parsed.insideParams[0]))
  {
    return ValidatorDiagnostic{.message = "First parameter must be a valid variable in get_area_param"};
  }
  if (!isValidStringExpression(parsed.insideParams[1]))
  {
    return ValidatorDiagnostic{.message = "Second parameter must be a valid string in get_area_param"};
  }
  if (parsed.insideParams.size() >= 3 && !isValidStringExpression(parsed.insideParams[2]))
  {
    return ValidatorDiagnostic{.message = "Third parameter must be a valid string in get_area_param"};
  }
  if (parsed.insideParams.size() == 4 && !isValidNormalVariable(parsed.insideParams[3]))
  {
    return ValidatorDiagnostic{.message = "Fourth parameter must be a valid variable in get_area_param"};
  }
  if (parsed.insideParams.size() == 3)
  {
    return ValidatorDiagnostic{
      .message = "Fourth parameter is required if third parameter is provided in get_area_param",
    };
  }
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> validateNewIterator()
{
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> validateNewIterator(const ParsedCommand& parsed)
{
  if (!parsed.assignment)
  {
    return ValidatorDiagnostic{.message = "new_iterator requires an assignment variable"};
  }
  if (!isValidNormalVariable(*parsed.assignment))
  {
    return ValidatorDiagnostic{.message = "Invalid assignment variable in new_iterator"};
  }
  if (parsed.insideParams.empty())
  {
    return std::nullopt;
  }
  if (parsed.insideParams.size() == 1 && parsed.insideParams[0] == "\"\"")
  {
    return std::nullopt;
  }
  if (parsed.insideParams.size() >= 4)
  {
    if (!isValidStringExpression(parsed.insideParams[0]))
    {
      return ValidatorDiagnostic{.message = "First parameter must be a valid string in new_iterator"};
    }
    if (!isValidNormalVariable(parsed.insideParams[1]))
    {
      return ValidatorDiagnostic{.message = "Second parameter must be a valid variable in new_iterator"};
    }
    if (!isValidStringExpression(parsed.insideParams[2]))
    {
      return ValidatorDiagnostic{.message = "Third parameter must be a valid string in new_iterator"};
    }
    if (!isValidStringExpression(parsed.insideParams[3]))
    {
      return ValidatorDiagnostic{.message = "Fourth parameter must be a valid string in new_iterator"};
    }
  }
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> validatePntToPlane()
{
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> validatePntToPlane(const ParsedCommand& parsed)
{
  if (parsed.assignment && !isValidNormalVariable(*parsed.assignment))
  {
    return ValidatorDiagnostic{.message = "Assignment variable must be a normal variable in pnt_to_plane"};
  }
  if (parsed.insideParams.size() < 5)
  {
    return ValidatorDiagnostic{.message = "pnt_to_plane requires 5 parameters inside parentheses"};
  }
  for (std::size_t i = 0; i < 5; ++i)
  {
    if (!isValidNormalVariable(parsed.insideParams[i]))
    {
      return ValidatorDiagnostic{
        .message = "Parameter " + std::to_string(i + 1)
          + " inside parentheses must be a valid variable in pnt_to_plane",
      };
    }
  }
  if (sameVar(parsed.insideParams[1], parsed.insideParams[2]))
  {
    return ValidatorDiagnostic{
      .message = "pnt_to_plane: second parameter must not be the same as the third (plane point) parameter",
      .warning = true,
    };
  }
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> applyCustomValidator(CustomValidatorId id)
{
  switch (id)
  {
  case CustomValidatorId::Abs:
    return validateAbs();
  case CustomValidatorId::Dbcreate:
    return validateDbcreate();
  case CustomValidatorId::GetAreaParam:
    return validateGetAreaParam();
  case CustomValidatorId::NewIterator:
    return validateNewIterator();
  case CustomValidatorId::PntToPlane:
    return validatePntToPlane();
  case CustomValidatorId::None:
    break;
  }
  return std::nullopt;
}

std::optional<ValidatorDiagnostic> applyCustomValidator(CustomValidatorId id, const ParsedCommand& parsed)
{
  switch (id)
  {
  case CustomValidatorId::Abs:
    return validateAbs(parsed);
  case CustomValidatorId::Dbcreate:
    return validateDbcreate(parsed);
  case CustomValidatorId::GetAreaParam:
    return validateGetAreaParam(parsed);
  case CustomValidatorId::NewIterator:
    return validateNewIterator(parsed);
  case CustomValidatorId::PntToPlane:
    return validatePntToPlane(parsed);
  case CustomValidatorId::None:
    break;
  }
  return std::nullopt;
}

} // namespace vertexadt::catalog
