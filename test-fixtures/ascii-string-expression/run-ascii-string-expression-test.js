'use strict';

const fs = require('fs');
const path = require('path');

const serverPath = path.join(__dirname, '..', '..', 'server', 'out', 'server.js');
const serverSrc = fs.readFileSync(serverPath, 'utf8');

function sliceBetween(source, startMarker, endMarker) {
    const start = source.indexOf(startMarker);
    const end = source.indexOf(endMarker, start);
    if (start < 0 || end < 0) {
        throw new Error(`Could not extract slice starting at ${startMarker}`);
    }
    return source.slice(start, end);
}

const patternSrc = sliceBetween(
    serverSrc,
    'const VARIABLE_PATTERNS = {',
    'const DBCREATE_FIELD_TYPE_PATTERN'
);
const parserSrc = sliceBetween(
    serverSrc,
    'function findMatchingParenIndex(text, openParenIndex) {',
    '\nfunction hasOwnPropertySafe'
);

const helpers = new Function(`
function isMathOperator(char) {
    return ['+', '-', '*', '/'].includes(char);
}
function isStartOfNewParam(str) {
    return /^[a-zA-Z_]|^"/.test(str);
}
function isEndOfParam(str) {
    return /[a-zA-Z0-9_)\\]"]$/.test(str);
}
${patternSrc}
${parserSrc}
return { isValidStringExpression, isAsciiStringCommand, parseCommand, isValidStringVariable, isValidAnyVariable };`)();
const isValidStringExpression = helpers.isValidStringExpression;
const isAsciiStringCommand = helpers.isAsciiStringCommand;
const parseCommand = helpers.parseCommand;
const isValidStringVariable = helpers.isValidStringVariable;
const isValidAnyVariable = helpers.isValidAnyVariable;

function assertEqual(actual, expected, message) {
    if (actual !== expected) {
        throw new Error(`${message}\n  expected: ${expected}\n  actual:   ${actual}`);
    }
}

assertEqual(
    isValidStringExpression('ascii(35)&"SAID"&ascii(35)'),
    true,
    'replace search concat with ascii(35)'
);
assertEqual(
    isValidStringExpression('ascii(35)'),
    true,
    'standalone ascii(35) is a string'
);
assertEqual(
    isValidStringExpression('ascii( 35 )'),
    true,
    'spaces inside ascii() are allowed'
);
assertEqual(
    isValidStringExpression('ascii(code)'),
    true,
    'ascii with a normal variable'
);
assertEqual(
    isValidStringExpression('ascii(code)&said$'),
    true,
    'ascii concatenated with a string variable'
);
assertEqual(
    isValidStringExpression('ASCII(35)'),
    false,
    'ASCII() is case-sensitive'
);
assertEqual(
    isValidStringExpression('ascii (35)'),
    false,
    'space before ( is not allowed'
);
assertEqual(
    isValidStringExpression('ascii()'),
    false,
    'ascii() with no argument is not a string'
);
assertEqual(
    isValidStringExpression('trim(hello)'),
    false,
    'other string commands are not accepted'
);
assertEqual(
    isAsciiStringCommand('ascii(35)'),
    true,
    'ascii matcher accepts a number'
);
assertEqual(
    isAsciiStringCommand('ascii(code)'),
    true,
    'ascii matcher accepts a normal variable'
);
assertEqual(
    isAsciiStringCommand('ascii(foo$)'),
    false,
    'ascii matcher rejects a string variable argument'
);
assertEqual(
    isAsciiStringCommand('ascii()'),
    false,
    'ascii matcher rejects an empty argument'
);
assertEqual(
    isAsciiStringCommand('ASCII(35)'),
    false,
    'ascii matcher is case-sensitive'
);
assertEqual(
    isAsciiStringCommand('ascii (35)'),
    false,
    'ascii matcher requires ( immediately after ascii'
);

const replaceLine = 'said$ = replace(said$,ascii(35)&"SAID"&ascii(35),Panel_Label$)';
const parsedReplace = parseCommand(replaceLine, replaceLine, true);
if (!parsedReplace) {
    throw new Error('failed to parse replace line with nested ascii()');
}
assertEqual(parsedReplace.functionName, 'replace', 'parsed function name');
assertEqual(parsedReplace.assignment, 'said$', 'parsed assignment');
assertEqual(parsedReplace.insideParams.length, 3, 'replace has 3 inside params');
assertEqual(
    isValidStringVariable(parsedReplace.assignment) &&
        isValidStringExpression(parsedReplace.insideParams[0]) &&
        isValidStringExpression(parsedReplace.insideParams[1]) &&
        (isValidAnyVariable(parsedReplace.insideParams[2]) || isValidStringExpression(parsedReplace.insideParams[2])),
    true,
    'replace line with ascii(35)&"SAID"&ascii(35) is valid'
);

console.log('ascii-string-expression tests passed');
