'use strict';

const fs = require('fs');
const path = require('path');

const fixtureRoot = __dirname;
const TEST_DATE = '2026-01-15';
const TEST_STEM = 'DOOR_INIT';
const TEST_USER = 'Author';

function isStarBannerLine(line) {
    const trimmed = line.trimStart();
    if (!trimmed.startsWith('!')) {
        return false;
    }
    const stars = trimmed.match(/\*/g);
    return !!stars && stars.length >= 10;
}

function findRemovableHeaderBlock(lines) {
    let start = 0;
    while (start < lines.length && lines[start].trim() === '') {
        start++;
    }
    if (start >= lines.length || !isStarBannerLine(lines[start])) {
        return null;
    }

    const openerIndex = start;
    let closerIndex = -1;
    for (let i = openerIndex + 1; i < lines.length; i++) {
        if (isStarBannerLine(lines[i])) {
            closerIndex = i;
            break;
        }
    }
    if (closerIndex === -1) {
        return null;
    }

    for (let i = openerIndex + 1; i < closerIndex; i++) {
        const trimmed = lines[i].trim();
        if (trimmed === '' || !lines[i].trimStart().startsWith('!')) {
            return null;
        }
    }

    return { start: openerIndex, end: closerIndex };
}

function buildFileHeader(fileStem, date, username) {
    const author = username || '';
    return [
        '!***************************************************************',
        `! ${fileStem} / ${date} / ${author}`,
        '! "leave empty for user manual input"',
        '!***************************************************************',
    ].join('\n');
}

function splitDocumentLines(text) {
    if (text.length === 0) {
        return [];
    }
    return text.split(/\r?\n/);
}

function detectDocumentEol(text) {
    return text.includes('\r\n') ? '\r\n' : '\n';
}

function applyHeaderToDocumentText(text, headerBlock) {
    const eol = detectDocumentEol(text);
    const lines = splitDocumentLines(text);
    const removable = findRemovableHeaderBlock(lines);

    if (removable) {
        const before = lines.slice(0, removable.start);
        const after = lines.slice(removable.end + 1);
        const parts = [];
        if (before.length > 0) {
            parts.push(before.join(eol));
        }
        parts.push(headerBlock);
        if (after.length > 0) {
            parts.push(after.join(eol));
        }
        return {
            text: parts.join(eol),
            mode: 'replaced',
        };
    }

    if (text.length === 0) {
        return {
            text: headerBlock,
            mode: 'created',
        };
    }

    return {
        text: `${headerBlock}${eol}${text}`,
        mode: 'created',
    };
}

function readFixture(name) {
    return fs.readFileSync(path.join(fixtureRoot, name), 'utf8');
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const expectedHeader = buildFileHeader(TEST_STEM, TEST_DATE, TEST_USER);
const expectedInfoLine = `! ${TEST_STEM} / ${TEST_DATE} / ${TEST_USER}`;

assert(isStarBannerLine('!***************************************************************'), 'star banner should match');
assert(!isStarBannerLine('! short *'), 'short star line should not match');
assert(!isStarBannerLine('X = 1'), 'code line should not match');

const oldHeaderText = readFixture('with_old_header.prg');
const oldHeaderResult = applyHeaderToDocumentText(oldHeaderText, expectedHeader);
assert(oldHeaderResult.mode === 'replaced', 'valid old header should be replaced');
assert(!oldHeaderResult.text.includes('DOOR_INIT / 1999-03-23'), 'old header content should be removed');
assert(!oldHeaderResult.text.includes('TopiR'), 'old header content should be removed');
assert(oldHeaderResult.text.includes(expectedInfoLine), 'new header info line should be present');
assert(oldHeaderResult.text.trimEnd().endsWith('X = 1'), 'body content should remain after header');

const invalidMiddleText = readFixture('with_invalid_middle.prg');
const invalidMiddleResult = applyHeaderToDocumentText(invalidMiddleText, expectedHeader);
assert(invalidMiddleResult.mode === 'created', 'invalid middle should prepend only');
assert(invalidMiddleResult.text.includes('OLD_HEADER / 1999-01-01'), 'old header block should be kept');
assert(invalidMiddleResult.text.startsWith('!***************************************************************'), 'new header should be prepended');
assert(invalidMiddleResult.text.includes('X = 1'), 'invalid middle line should remain in original block');
assert(invalidMiddleResult.text.includes('Y = 2'), 'body after old header should remain');

const noHeaderText = readFixture('no_header.prg');
const noHeaderResult = applyHeaderToDocumentText(noHeaderText, expectedHeader);
assert(noHeaderResult.mode === 'created', 'file without header should prepend');
assert(noHeaderResult.text.startsWith(expectedHeader), 'header should be at top');
assert(noHeaderResult.text.includes('\nX = 1'), 'body should follow header');

const removable = findRemovableHeaderBlock(splitDocumentLines(oldHeaderText));
assert(removable && removable.start === 0 && removable.end === 4, 'old header block range should match fixture');

const notRemovable = findRemovableHeaderBlock(splitDocumentLines(invalidMiddleText));
assert(notRemovable === null, 'invalid middle block should not be removable');

console.log('header-creator: all assertions passed');
