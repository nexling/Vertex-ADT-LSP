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

const parserSrc = sliceBetween(
    serverSrc,
    'function findMatchingParenIndex(text, openParenIndex) {',
    '\nfunction hasOwnPropertySafe'
);
const structureSrc = sliceBetween(
    serverSrc,
    'function parameterOrdinalName(index) {',
    '\nfunction getVariableType'
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
${parserSrc}
${structureSrc}
return { parseCommand, parseParameters, validateCommandStructure };
`)();

const parseCommand = helpers.parseCommand;
const parseParameters = helpers.parseParameters;
const validateCommandStructure = helpers.validateCommandStructure;

const VEC_TIMES_SCALAR = {
    hasAssignment: false,
    hasParentheses: true,
    minParamsInside: 1,
    maxParamsInside: 1,
    paramsOutside: 2,
    maxParamsOutside: 2
};

const DBSAVE = {
    hasAssignment: 'optional',
    hasParentheses: true,
    minParamsInside: 1,
    maxParamsInside: 1,
    paramsOutside: 0,
    maxParamsOutside: 0
};

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    if (actualJson !== expectedJson) {
        throw new Error(`${message}\n  expected: ${expectedJson}\n  actual:   ${actualJson}`);
    }
}

function parseLine(line) {
    return parseCommand(line, line, false);
}

assertEqual(parseParameters(''), [], 'empty string is zero params');
assertEqual(parseParameters('   '), [], 'whitespace-only is zero params');
assertEqual(parseParameters(',shtlaytopN,jointholestart'), ['', 'shtlaytopN', 'jointholestart'], 'leading comma keeps empty first slot');
assertEqual(parseParameters('a,,b'), ['a', '', 'b'], 'doubled comma keeps empty middle slot');
assertEqual(parseParameters('a,b,'), ['a', 'b', ''], 'trailing comma keeps empty last slot');
assertEqual(parseParameters(','), ['', ''], 'lone comma is two empty slots');
assertEqual(parseParameters('a, b'), ['a', 'b'], 'normal two params still split');

const userExample = parseLine('vec_times_scalar(joinstartP),shtlaytopN,jointholestart');
assertEqual(userExample.insideParams, ['joinstartP'], 'user example inside param');
assertEqual(
    userExample.afterParams,
    ['', 'shtlaytopN', 'jointholestart'],
    'user example keeps empty first after-paren slot'
);
assertEqual(
    validateCommandStructure(userExample, VEC_TIMES_SCALAR, 'vec_times_scalar'),
    [
        'First parameter after parentheses is missing in vec_times_scalar',
        'vec_times_scalar accepts at most 2 parameters after parentheses'
    ],
    'user example reports missing first and at-most-2'
);

const validCall = parseLine('vec_times_scalar(NewVector) Vector, ScaleFactor');
assertEqual(validCall.afterParams, ['Vector', 'ScaleFactor'], 'valid after params');
assertEqual(
    validateCommandStructure(validCall, VEC_TIMES_SCALAR, 'vec_times_scalar'),
    [],
    'valid vec_times_scalar has no structure errors'
);

const emptyParens = parseLine('vec_times_scalar() a, b');
assertEqual(emptyParens.insideParams, [], '() is zero inside params, not one empty slot');
assertEqual(
    validateCommandStructure(emptyParens, VEC_TIMES_SCALAR, 'vec_times_scalar'),
    ['vec_times_scalar requires at least 1 parameters inside parentheses'],
    '() reports too-few inside only'
);

const trailingExtraEmpty = parseLine('vec_times_scalar(out) a, b,');
assertEqual(trailingExtraEmpty.afterParams, ['a', 'b', ''], 'trailing comma after two args');
assertEqual(
    validateCommandStructure(trailingExtraEmpty, VEC_TIMES_SCALAR, 'vec_times_scalar'),
    ['vec_times_scalar accepts at most 2 parameters after parentheses'],
    'empty slot past arity is too-many, not missing third'
);

const emptyInside = parseLine('vec_times_scalar(,x) a, b');
assertEqual(emptyInside.insideParams, ['', 'x'], 'empty first inside slot is kept');
assertEqual(
    validateCommandStructure(emptyInside, VEC_TIMES_SCALAR, 'vec_times_scalar'),
    [
        'First parameter inside parentheses is missing in vec_times_scalar',
        'vec_times_scalar accepts at most 1 parameters inside parentheses'
    ],
    'empty inside slot reports missing first then too-many inside'
);

const dbsaveTrailing = parseLine('dbsave(database),');
assertEqual(dbsaveTrailing.afterParams, ['', ''], 'comma after zero-outside command is two empty slots');
assertEqual(
    validateCommandStructure(dbsaveTrailing, DBSAVE, 'dbsave'),
    ['dbsave accepts at most 0 parameters after parentheses'],
    'maxParamsOutside 0 does not report missing after-paren slots'
);

console.log('empty-comma-slots tests passed');
