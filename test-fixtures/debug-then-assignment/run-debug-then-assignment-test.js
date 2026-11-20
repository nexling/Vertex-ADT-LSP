'use strict';

const fs = require('fs');
const path = require('path');

const serverPath = path.join(__dirname, '..', '..', 'server', 'out', 'server.js');
const serverSrc = fs.readFileSync(serverPath, 'utf8');
const start = serverSrc.indexOf('function parseLineAssignments(lineText)');
const end = serverSrc.indexOf('\nfunction getDocumentAssignments', start);
if (start < 0 || end < 0) {
    throw new Error('Could not extract parseLineAssignments from server.js');
}
const parseLineAssignments = new Function(`${serverSrc.slice(start, end)}; return parseLineAssignments;`)();

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    if (actualJson !== expectedJson) {
        throw new Error(`${message}\n  expected: ${expectedJson}\n  actual:   ${actualJson}`);
    }
}

assertEqual(
    parseLineAssignments('CTopZ = -1000000    <-1e+06>'),
    { CTopZ: '-1e+06' },
    'standalone assignment still traces'
);

assertEqual(
    parseLineAssignments('if TZ > CTopZ then CTopZ = TZ    <5127.43>'),
    { CTopZ: '5127.43' },
    'inline then-assignment traces numeric value'
);

assertEqual(
    parseLineAssignments('if TZ > CTopZ then CTopZ = TZ    <true> _if_begin_'),
    {},
    'inline then with if-result true is skipped'
);

assertEqual(
    parseLineAssignments('if TZ > CTopZ then CTopZ = TZ    <false>'),
    {},
    'inline then with if-result false is skipped'
);

assertEqual(
    parseLineAssignments('if TZ > CTopZ then CTopZ = TZ    <true> _if_begin_ <5127.43>'),
    { CTopZ: '5127.43' },
    'last non-boolean <...> on then-line is the write'
);

assertEqual(
    parseLineAssignments('if A = B then C = D    <100>'),
    { C: '100' },
    'condition = is not an assignment'
);

assertEqual(
    parseLineAssignments('else CTopZ = TZ    <5127.43>'),
    {},
    'else VAR = without then is not traced'
);

assertEqual(
    parseLineAssignments('else if TZ > CTopZ then CTopZ = TZ    <5127.43>'),
    { CTopZ: '5127.43' },
    'else if ... then VAR = traces'
);

assertEqual(
    parseLineAssignments('else if TZ > CTopZ then CTopZ = TZ    <false>'),
    {},
    'else if then with false is skipped'
);

assertEqual(
    parseLineAssignments('Flag = TRUE    <true>'),
    { Flag: 'true' },
    'standalone boolean assignment still traces'
);

assertEqual(
    parseLineAssignments('IF TZ > CTopZ THEN CTopZ = TZ    <5127.43>'),
    { CTopZ: '5127.43' },
    'if/then is case-insensitive'
);

assertEqual(
    parseLineAssignments('if X then Name$ = "A"    <"A">'),
    { 'Name$': '"A"' },
    'string then-assignment traces'
);

assertEqual(
    parseLineAssignments('vec_sub(NSP) SidPceSP,WP                     <-3300,254,2259.5>'),
    { NSP: '-3300,254,2259.5' },
    'function output-in-parens still traces'
);

assertEqual(
    parseLineAssignments('if CenterIdx then    <true> _if_begin_'),
    {},
    'if then without assignment is not a write'
);

assertEqual(
    parseLineAssignments('if a then if b then X = Y    <9>'),
    { X: '9' },
    'last then VAR = on the line is the write'
);

console.log('debug-then-assignment tests passed');
