'use strict';

function isInsideQuotes(line, position) {
    let activeQuote = null;
    for (let i = 0; i < position; i++) {
        if (activeQuote) {
            if (line[i] === activeQuote) {
                activeQuote = null;
            }
            continue;
        }
        if (line[i] === '"' || line[i] === "'") {
            activeQuote = line[i];
        }
    }
    return activeQuote !== null;
}

function findValidationCommentIndex(line) {
    if (!line) {
        return -1;
    }
    for (let i = 0; i < line.length - 1; i++) {
        if (line[i] === ';' && line[i + 1] === '!' && !isInsideQuotes(line, i)) {
            return i;
        }
    }
    return -1;
}

function stripValidationComment(line) {
    if (!line) {
        return '';
    }
    const commentIndex = findValidationCommentIndex(line);
    return commentIndex !== -1 ? line.substring(0, commentIndex) : line;
}

function findMatchingParenIndex(text, openParenIndex) {
    if (!text || openParenIndex < 0 || text[openParenIndex] !== '(') {
        return -1;
    }

    let parenCount = 1;
    let activeQuote = null;

    for (let i = openParenIndex + 1; i < text.length; i++) {
        const char = text[i];

        if (activeQuote) {
            if (char === activeQuote) {
                activeQuote = null;
            }
            continue;
        }

        if (char === '"') {
            activeQuote = char;
            continue;
        }

        if (char === '(') {
            parenCount++;
        } else if (char === ')') {
            parenCount--;
            if (parenCount === 0) {
                return i;
            }
        }
    }

    return -1;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const findstriInString = 'semicolonindex = findstri(Line$,";!")';
const strippedFindstri = stripValidationComment(findstriInString);
assert(
    strippedFindstri === findstriInString,
    'findstri call with ;! inside string should not be truncated'
);

const openParen = strippedFindstri.indexOf('(');
const closeParen = findMatchingParenIndex(strippedFindstri, openParen);
assert(closeParen !== -1, 'findstri parentheses should balance after string-aware strip');

assert(
    stripValidationComment('X = 1 ;! comment') === 'X = 1 ',
    'real trailing ;! comment should be stripped'
);

assert(
    findValidationCommentIndex('findstri(Line$,";!")') === -1,
    ';! inside quoted string should not be treated as comment'
);

const trailingCommentIndex = findValidationCommentIndex('ok = findstri(Name$,"stack") ;! trailing comment');
assert(trailingCommentIndex !== -1, 'trailing ;! outside quotes should be found');
assert(
    stripValidationComment('ok = findstri(Name$,"stack") ;! trailing comment') === 'ok = findstri(Name$,"stack") ',
    'trailing comment should strip code only before real ;!'
);

const brokenLine = stripValidationComment('bad = findstri(Line$,";!"   ;! should still error');
assert(
    brokenLine === 'bad = findstri(Line$,";!"   ',
    'broken line should strip at real comment, preserving string literal'
);
const brokenOpenParen = brokenLine.indexOf('(');
assert(
    findMatchingParenIndex(brokenLine, brokenOpenParen) === -1,
    'line truncated at real comment should leave unclosed paren'
);

console.log('semicolon-bang-comments: all assertions passed');
