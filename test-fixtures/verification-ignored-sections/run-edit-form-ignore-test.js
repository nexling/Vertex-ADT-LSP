'use strict';

const fs = require('fs');
const path = require('path');

const fixtureRoot = __dirname;
const VERIFICATION_IGNORED_SECTION_NAMES = new Set(['EDIT_FORM']);
const variablePattern1 = /\b[A-Za-z][A-Za-z0-9_-]*\b(?=\s*=)/g;
const variablePattern2 = /\b[A-Za-z]*\b(?!$)(?=\s*=)/g;

function parseFileSections(lines) {
    const sectionMarker = /^#([A-Za-z_][A-Za-z0-9_]*)#$/;
    const open = new Map();
    const sections = [];

    for (let i = 0; i < lines.length; i++) {
        const m = lines[i].trim().match(sectionMarker);
        if (!m) {
            continue;
        }
        const name = m[1];
        if (!open.has(name)) {
            open.set(name, i);
        } else {
            const start = open.get(name);
            open.delete(name);
            sections.push({
                name,
                markerStart: start,
                markerEnd: i,
                bodyStart: start + 1,
                bodyEnd: i - 1,
            });
        }
    }
    return sections;
}

function getSectionForLine(lineIndex, fileSections) {
    for (const section of fileSections) {
        if (lineIndex >= section.bodyStart && lineIndex <= section.bodyEnd) {
            return section.name;
        }
    }
    return '';
}

function isVerificationIgnoredSectionName(sectionName) {
    return typeof sectionName === 'string' &&
        VERIFICATION_IGNORED_SECTION_NAMES.has(sectionName);
}

function isLineInVerificationIgnoredSection(lineIndex, fileSections) {
    const sectionName = getSectionForLine(lineIndex, fileSections);
    return isVerificationIgnoredSectionName(sectionName);
}

function getVerificationIgnoredRanges(fileSections) {
    const ranges = [];
    for (const section of fileSections) {
        if (!isVerificationIgnoredSectionName(section.name)) {
            continue;
        }
        if (section.bodyStart <= section.bodyEnd) {
            ranges.push({
                start: section.bodyStart,
                end: section.bodyEnd,
            });
        }
    }
    return ranges;
}

function isLineInIgnoredRanges(lineIndex, ignoredRanges) {
    for (const range of ignoredRanges) {
        if (lineIndex >= range.start && lineIndex <= range.end) {
            return true;
        }
    }
    return false;
}

function findDeclarationCheckSections(lines) {
    const declarationSections = [];
    let currentStart = -1;
    for (let i = 0; i < lines.length; i++) {
        const trimmed = lines[i].trim();
        if (trimmed.startsWith('@declarationnumbercheckstart')) {
            currentStart = i;
        } else if (trimmed.startsWith('@declarationnumbercheckend') && currentStart !== -1) {
            declarationSections.push({ start: currentStart, end: i });
            currentStart = -1;
        }
    }
    return declarationSections;
}

function extractDeclarationNumbersFromLines(lines) {
    const declarations = [];
    const declarationSections = findDeclarationCheckSections(lines);
    const fileSections = parseFileSections(lines);
    const seenVariables = new Map();

    for (const section of declarationSections) {
        const sectionLines = lines.slice(section.start + 1, section.end);
        for (let i = 0; i < sectionLines.length; i++) {
            const line = sectionLines[i];
            const actualLineIndex = section.start + 1 + i;
            const sectionName = getSectionForLine(actualLineIndex, fileSections);
            if (isVerificationIgnoredSectionName(sectionName)) {
                continue;
            }
            const splitlines = line.split(';');
            for (const splitline of splitlines) {
                if (splitline.trim().startsWith('!')) {
                    continue;
                }
                let match = splitline.match(variablePattern1);
                if (!match) {
                    match = splitline.match(variablePattern2);
                }
                const seenKey = `${sectionName}::${match ? match[0] : ''}`;
                if (!match || seenVariables.has(seenKey)) {
                    continue;
                }
                seenVariables.set(seenKey, actualLineIndex);
                const value = splitline.split('=')[1];
                if (value !== undefined && !isNaN(value)) {
                    const declarationNumber = Number(value.trim());
                    if (declarationNumber > 0) {
                        declarations.push({
                            varName: match[0],
                            number: declarationNumber,
                            line: actualLineIndex,
                            sectionName,
                        });
                    }
                }
            }
        }
    }
    return declarations;
}

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

function hasDanglingIfLogicalOperator(condition) {
    if (!condition) {
        return false;
    }
    return /\s+(and|or|&&|\|\|)\s*$/i.test(stripQuotedStringsForIfCheck(condition));
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

function getIfLogicalOperatorPlacementError(line, isPendingIfHeader) {
    const trimmed = stripValidationComment(line).trim();
    if (!trimmed || trimmed.startsWith('!')) {
        return null;
    }

    if (/^\bif\b/i.test(trimmed)) {
        const condition = extractIfConditionFromLine(line);
        if (condition && hasDanglingIfLogicalOperator(condition)) {
            return 'Logical operator must not appear directly before then without a condition.';
        }
    }

    if (isPendingIfHeader) {
        if (/^\s*(and|or)\b/i.test(trimmed) || /^\s*(&&|\|\|)/.test(trimmed)) {
            return 'Logical and/or continuation line must include a condition after the operator.';
        }
    }

    return null;
}

function advancePendingIfHeaderState(trimmedLine, pendingIfHeader) {
    if (/^\bif\b/i.test(trimmedLine) && /\bthen\b/i.test(trimmedLine)) {
        return false;
    }
    if (/^\bif\b/i.test(trimmedLine)) {
        return true;
    }
    if (pendingIfHeader && (/^\s*(and|or)\b/i.test(trimmedLine) || /^\s*(&&|\|\|)/.test(trimmedLine))) {
        return true;
    }
    return pendingIfHeader;
}

function collectPlacementErrors(lines, fileSections) {
    const errors = [];
    let pendingIfHeader = false;

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        const trimmedLine = stripValidationComment(line).trim();
        if (!trimmedLine || trimmedLine.startsWith('!')) {
            continue;
        }
        if (trimmedLine.startsWith('##')) {
            continue;
        }
        if (trimmedLine.startsWith('#') && trimmedLine.endsWith('#')) {
            continue;
        }

        if (isLineInVerificationIgnoredSection(i, fileSections)) {
            pendingIfHeader = advancePendingIfHeaderState(trimmedLine, pendingIfHeader);
            continue;
        }

        const placementError = getIfLogicalOperatorPlacementError(line, pendingIfHeader);
        if (placementError) {
            errors.push({ line: i, message: placementError });
        }

        pendingIfHeader = advancePendingIfHeaderState(trimmedLine, pendingIfHeader);
    }

    return errors;
}

function readFixture(name) {
    return fs.readFileSync(path.join(fixtureRoot, 'custom', name), 'utf8').split(/\r?\n/);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const lines = readFixture('edit_form_ignored.prg');
const fileSections = parseFileSections(lines);
const ignoredRanges = getVerificationIgnoredRanges(fileSections);

assert(ignoredRanges.length === 1, 'fixture should have one EDIT_FORM body range');
assert(ignoredRanges[0].start === 5, 'EDIT_FORM body should start after opening marker');
assert(ignoredRanges[0].end === 11, 'EDIT_FORM body should end before closing marker');

const placementErrors = collectPlacementErrors(lines, fileSections);
assert(placementErrors.length === 2, 'expected two placement errors outside EDIT_FORM');
assert(placementErrors.every((err) => !isLineInIgnoredRanges(err.line, ignoredRanges)),
    'placement errors must not be inside EDIT_FORM');
assert(placementErrors.some((err) => err.line === 0), 'first dangling and should be on line 0');
assert(placementErrors.some((err) => err.line === 14), 'second dangling or should be on line 14');

const declarations = extractDeclarationNumbersFromLines(lines);
assert(declarations.length === 0, 'EDIT_FORM declarations must be excluded from index extraction');

console.log('verification-ignored-sections: all assertions passed');
