'use strict';

const fs = require('fs');
const path = require('path');

const fixtureRoot = __dirname;

function stripValidationComment(line) {
    if (!line) {
        return '';
    }
    return line.includes(';!') ? line.split(';!')[0] : line;
}

function stripQuotedStringsForIfCheck(text) {
    return text
        .replace(/"[^"]*"/g, '""')
        .replace(/'[^']*'/g, "''");
}

function createIfOperatorScope() {
    return { hasAnd: false, hasOr: false };
}

function recordIfLogicalOperator(scope, kind) {
    if (kind === 'and') {
        scope.hasAnd = true;
    } else {
        scope.hasOr = true;
    }
}

function validateIfOperatorScope(scope) {
    if (scope.hasAnd && scope.hasOr) {
        return 'If condition with both and and or requires parentheses to clarify precedence.';
    }
    return null;
}

function getIfConditionOperatorError(condition) {
    if (!condition) {
        return null;
    }

    const expr = stripQuotedStringsForIfCheck(condition);
    const scopeStack = [createIfOperatorScope()];
    const operators = [];

    for (let i = 0; i < expr.length; i++) {
        const ch = expr[i];

        if (ch === '(') {
            scopeStack.push(createIfOperatorScope());
            continue;
        }
        if (ch === ')') {
            if (scopeStack.length > 1) {
                const closedScopeError = validateIfOperatorScope(scopeStack[scopeStack.length - 1]);
                if (closedScopeError) {
                    return closedScopeError;
                }
                scopeStack.pop();
            }
            continue;
        }

        if (ch === '&' && expr[i + 1] === '&') {
            operators.push({ kind: 'and', style: 'symbol' });
            recordIfLogicalOperator(scopeStack[scopeStack.length - 1], 'and');
            i++;
            continue;
        }
        if (ch === '|' && expr[i + 1] === '|') {
            operators.push({ kind: 'or', style: 'symbol' });
            recordIfLogicalOperator(scopeStack[scopeStack.length - 1], 'or');
            i++;
            continue;
        }

        const remainder = expr.slice(i);
        const andMatch = remainder.match(/^\s+and\b/i);
        if (andMatch) {
            operators.push({ kind: 'and', style: 'word' });
            recordIfLogicalOperator(scopeStack[scopeStack.length - 1], 'and');
            i += andMatch[0].length - 1;
            continue;
        }

        const orMatch = remainder.match(/^\s+or\b/i);
        if (orMatch) {
            operators.push({ kind: 'or', style: 'word' });
            recordIfLogicalOperator(scopeStack[scopeStack.length - 1], 'or');
            i += orMatch[0].length - 1;
        }
    }

    if (operators.length === 0) {
        return null;
    }

    const hasWordStyle = operators.some(op => op.style === 'word');
    const hasSymbolStyle = operators.some(op => op.style === 'symbol');
    if (hasWordStyle && hasSymbolStyle) {
        return 'If condition must not mix word operators (and/or) with symbol operators (&&/||). Use one style consistently or add parentheses.';
    }

    for (let s = scopeStack.length - 1; s >= 1; s--) {
        const unclosedScopeError = validateIfOperatorScope(scopeStack[s]);
        if (unclosedScopeError) {
            return unclosedScopeError;
        }
    }

    return validateIfOperatorScope(scopeStack[0]);
}

function extractIfConditionFromLine(line) {
    const trimmed = stripValidationComment(line).trim();
    if (!/^\bif\b/i.test(trimmed)) {
        return null;
    }

    const thenMatch = trimmed.match(/\bthen\b/i);
    if (!thenMatch || typeof thenMatch.index !== 'number') {
        return null;
    }

    const ifPrefixMatch = trimmed.match(/^\s*if\s+/i);
    if (!ifPrefixMatch) {
        return null;
    }

    return trimmed.slice(ifPrefixMatch[0].length, thenMatch.index).trim();
}

function hasDanglingIfLogicalOperator(condition) {
    if (!condition) {
        return false;
    }
    return /\s+(and|or|&&|\|\|)\s*$/i.test(stripQuotedStringsForIfCheck(condition));
}

function hasLeadingIfLogicalOperator(condition) {
    if (!condition) {
        return false;
    }
    const expr = stripQuotedStringsForIfCheck(condition).trim();
    return /^(and|or)\b/i.test(expr) || /^(&&|\|\|)/.test(expr);
}

function getIfLeadingLogicalOperatorError(condition) {
    if (hasLeadingIfLogicalOperator(condition)) {
        return 'Logical operator must not appear directly after if without a preceding condition.';
    }
    return null;
}

function getIfContinuationLinePlacementError(trimmedLine) {
    if (/^\s*(and|or)\b\s*then\b/i.test(trimmedLine) || /^\s*(&&|\|\|)\s*then\b/i.test(trimmedLine)) {
        return 'Logical operator must not appear directly before then without a condition.';
    }
    if (/^\s*(and|or)\s*$/i.test(trimmedLine) || /^\s*(&&|\|\|)\s*$/i.test(trimmedLine)) {
        return 'Logical and/or continuation line must include a condition after the operator.';
    }
    if (/^\s*(and|or)\b/i.test(trimmedLine) && !/^\s*(and|or)\b\s+.+/i.test(trimmedLine)) {
        return 'Logical and/or continuation line must include a condition after the operator.';
    }
    if (/^\s*(&&|\|\|)/i.test(trimmedLine) && !/^\s*(&&|\|\|)\s*.+/i.test(trimmedLine)) {
        return 'Logical and/or continuation line must include a condition after the operator.';
    }
    return null;
}

function getIfLogicalOperatorPlacementError(line, isPendingIfHeader) {
    const trimmed = stripValidationComment(line).trim();
    if (!trimmed) {
        return null;
    }

    const ifCondition = extractIfConditionFromLine(line);
    if (ifCondition) {
        const leadingError = getIfLeadingLogicalOperatorError(ifCondition);
        if (leadingError) {
            return leadingError;
        }
        if (hasDanglingIfLogicalOperator(ifCondition)) {
            return 'Logical operator must not appear directly before then without a condition.';
        }
    }

    if (/^\s*if\b/i.test(trimmed) && !/\bthen\b/i.test(trimmed)) {
        const afterIfMatch = trimmed.match(/^\s*if\s+/i);
        if (afterIfMatch) {
            const afterIf = trimmed.slice(afterIfMatch[0].length);
            const leadingError = getIfLeadingLogicalOperatorError(afterIf);
            if (leadingError) {
                return leadingError;
            }
            if (hasDanglingIfLogicalOperator(afterIf)) {
                return 'Logical operator at end of if line must continue on the next line with and/or.';
            }
        }
    }

    if (isPendingIfHeader && (/^\s*(and|or)\b/i.test(trimmed) || /^\s*(&&|\|\|)/.test(trimmed))) {
        return getIfContinuationLinePlacementError(trimmed);
    }

    return null;
}

function advancePendingIfHeaderState(trimmedLine, currentPendingIfHeader) {
    if (/^\s*endif\b/i.test(trimmedLine) || /^\s*else\b/i.test(trimmedLine)) {
        return false;
    }

    const hasIf = /^\s*if\b/i.test(trimmedLine);
    const hasThen = /\bthen\b/i.test(trimmedLine);

    if (hasIf && !hasThen) {
        return true;
    }

    if (hasThen && currentPendingIfHeader) {
        return false;
    }

    if (currentPendingIfHeader) {
        if (/^\s*(and|or)\b/i.test(trimmedLine) || /^\s*(&&|\|\|)/.test(trimmedLine)) {
            return true;
        }
        return false;
    }

    return false;
}

function collectPlacementErrors(lines) {
    const errors = [];
    let pendingIfHeader = false;

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        const trimmedLine = stripValidationComment(line).trim();
        if (!trimmedLine || trimmedLine.startsWith('!')) {
            continue;
        }

        const placementError = getIfLogicalOperatorPlacementError(line, pendingIfHeader);
        if (placementError) {
            errors.push({ line: i, message: placementError });
        }

        if (/\bthen\b/i.test(trimmedLine)) {
            const ifCondition = extractIfConditionFromLine(line);
            const operatorError = getIfConditionOperatorError(ifCondition);
            if (operatorError) {
                errors.push({ line: i, message: operatorError });
            }
        }

        pendingIfHeader = advancePendingIfHeaderState(trimmedLine, pendingIfHeader);
    }

    return errors;
}

function readFixture(name) {
    return fs.readFileSync(path.join(fixtureRoot, name), 'utf8').split(/\r?\n/);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

assert(hasDanglingIfLogicalOperator('variable = 5 and'), 'trailing and should dangle');
assert(!hasDanglingIfLogicalOperator('A = 1 and B = 2'), 'combined condition should not dangle');
assert(
    getIfLogicalOperatorPlacementError('if variable = 5 and then', false) !== null,
    'same-line dangling and before then should error'
);
assert(
    getIfLogicalOperatorPlacementError('if A = 1 and B = 2 then C = 3', false) === null,
    'valid combined condition should pass'
);
assert(
    getIfLogicalOperatorPlacementError('if and var > 1000 then', false) !== null,
    'leading and after if should error'
);
assert(
    getIfLogicalOperatorPlacementError('if var > 1000 then', false) === null,
    'normal if condition should pass'
);

let errors = collectPlacementErrors(readFixture('leading_operator_same_line.prg'));
assert(errors.length === 1, 'leading_operator_same_line.prg should have one placement error');
assert(errors[0].line === 0, 'leading_operator_same_line error should be on line 0');

errors = collectPlacementErrors(readFixture('dangling_same_line.prg'));
assert(errors.length === 1, 'dangling_same_line.prg should have one placement error');
assert(errors[0].line === 0, 'dangling_same_line error should be on line 0');

errors = collectPlacementErrors(readFixture('dangling_or.prg'));
assert(errors.length === 1, 'dangling_or.prg should have one placement error');

errors = collectPlacementErrors(readFixture('valid_combined.prg'));
assert(errors.length === 0, 'valid_combined.prg should have no placement errors');

errors = collectPlacementErrors(readFixture('valid_multiline.prg'));
assert(errors.length === 0, 'valid_multiline.prg should have no placement errors');

errors = collectPlacementErrors(readFixture('dangling_multiline.prg'));
assert(errors.length === 1, 'dangling_multiline.prg should have one placement error');
assert(errors[0].line === 1, 'dangling_multiline error should be on continuation line');

const precedenceMessage = 'If condition with both and and or requires parentheses to clarify precedence.';

const validMixCondition1 = '(ccdist < 501) and (cmpi(dia$,prevdia$)) and (abs(prevxpos - xpos) < 1 or abs(prevypos - ypos) < 1) and (cmpi(prevcorners$,corners$)) and !(abs(prevxpos - xpos) < 1 and abs(prevypos - ypos) < 1)';
const validMixCondition2 = 'ccdist < 500 and (cmpi(dia$,prevdia$)) and (abs(prevxpos - xpos) < 1 or abs(prevypos - ypos) < 1) and !(abs(prevxpos - xpos) < 1 and abs(prevypos - ypos) < 1)';
assert(getIfConditionOperatorError(validMixCondition1) === null, 'parenthesized and/or mix should be valid');
assert(getIfConditionOperatorError(validMixCondition2) === null, 'parenthesized and/or mix without outer parens should be valid');
assert(getIfConditionOperatorError('a and b or c') === precedenceMessage, 'unparenthesized root mix should error');
assert(getIfConditionOperatorError('(a and b or c)') === precedenceMessage, 'unparenthesized inner mix should error');
assert(getIfConditionOperatorError('(Geom) and (Area) or (Geom)') === precedenceMessage, 'parenthesized terms with root and/or mix should error');

errors = collectPlacementErrors(readFixture('valid_parenthesized_mix.prg'));
assert(errors.length === 0, 'valid_parenthesized_mix.prg should have no errors');

errors = collectPlacementErrors(readFixture('invalid_root_mix.prg'));
assert(errors.length === 1, 'invalid_root_mix.prg should have one operator error');
assert(errors[0].message === precedenceMessage, 'invalid_root_mix should report precedence error');

errors = collectPlacementErrors(readFixture('invalid_inner_mix.prg'));
assert(errors.length === 1, 'invalid_inner_mix.prg should have one operator error');
assert(errors[0].message === precedenceMessage, 'invalid_inner_mix should report precedence error');

errors = collectPlacementErrors(readFixture('invalid_paren_terms_mix.prg'));
assert(errors.length === 1, 'invalid_paren_terms_mix.prg should have one operator error');
assert(errors[0].message === precedenceMessage, 'invalid_paren_terms_mix should report precedence error');

console.log('if-logical-operators smoke test passed');
