#pragma once

#include "table_validator.h"
#include "types.h"

#include <optional>

namespace vertexadt::catalog {

std::optional<ValidatorDiagnostic> validateAbs();
std::optional<ValidatorDiagnostic> validateAbs(const ParsedCommand& parsed);
std::optional<ValidatorDiagnostic> validateDbcreate();
std::optional<ValidatorDiagnostic> validateDbcreate(const ParsedCommand& parsed);
std::optional<ValidatorDiagnostic> validateGetAreaParam();
std::optional<ValidatorDiagnostic> validateGetAreaParam(const ParsedCommand& parsed);
std::optional<ValidatorDiagnostic> validateNewIterator();
std::optional<ValidatorDiagnostic> validateNewIterator(const ParsedCommand& parsed);
std::optional<ValidatorDiagnostic> validatePntToPlane();
std::optional<ValidatorDiagnostic> validatePntToPlane(const ParsedCommand& parsed);

std::optional<ValidatorDiagnostic> applyCustomValidator(CustomValidatorId id);
std::optional<ValidatorDiagnostic> applyCustomValidator(CustomValidatorId id, const ParsedCommand& parsed);

} // namespace vertexadt::catalog
