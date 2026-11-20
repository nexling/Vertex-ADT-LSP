'use strict';

const fs = require('fs');
const path = require('path');

const variablePattern1 = /\b[A-Za-z][A-Za-z0-9_-]*\b(?=\s*=)/g;
const variablePattern2 = /\b[A-Za-z]*\b(?!$)(?=\s*=)/g;

function findDeclarationCheckSections(lines) {
    const declarationSections = [];
    let currentStart = -1;
    for (let i = 0; i < lines.length; i++) {
        const trimmed = lines[i].trim();
        if (trimmed.startsWith('@declarationnumbercheckstart')) {
            currentStart = i;
        }
        else if (trimmed.startsWith('@declarationnumbercheckend') && currentStart !== -1) {
            declarationSections.push({ start: currentStart, end: i });
            currentStart = -1;
        }
    }
    return declarationSections;
}

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
                bodyEnd: i - 1
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

function findDeclarationNumberConflicts(lines) {
    const declarationSections = findDeclarationCheckSections(lines);
    const fileSections = parseFileSections(lines);
    const declarationScopeMaps = new Map();
    const conflicts = [];

    for (const section of declarationSections) {
        const linesToCheck = lines.slice(section.start + 1, section.end);
        for (let i = 0; i < linesToCheck.length; i++) {
            const line = linesToCheck[i];
            const lineIndex = section.start + 1 + i;
            const sectionScope = getSectionForLine(lineIndex, fileSections);
            if (!declarationScopeMaps.has(sectionScope)) {
                declarationScopeMaps.set(sectionScope, new Map());
            }
            const declarationNumbersForScope = declarationScopeMaps.get(sectionScope);

            const splitlines = line.split(';');
            for (let j = 0; j < splitlines.length; j++) {
                const splitline = splitlines[j];
                if (splitline.trim().startsWith('!')) {
                    continue;
                }
                let match = splitline.match(variablePattern1);
                if (!match) {
                    match = splitline.match(variablePattern2);
                }
                if (!match) {
                    continue;
                }

                const valuePart = splitline.split('=')[1];
                if (valuePart === undefined || isNaN(valuePart)) {
                    continue;
                }

                const declarationNumber = Number(valuePart.trim());
                if (declarationNumber <= 0) {
                    continue;
                }

                if (declarationNumbersForScope.has(declarationNumber)) {
                    const existingVar = declarationNumbersForScope.get(declarationNumber);
                    if (existingVar !== match[0]) {
                        conflicts.push({
                            varName: match[0],
                            existingVar,
                            declarationNumber,
                            line: lineIndex,
                            sectionName: sectionScope
                        });
                    }
                }
                declarationNumbersForScope.set(declarationNumber, match[0]);
            }
        }
    }

    return conflicts;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const fixturePath = path.join(__dirname, 'custom', 'section_multi_block_dup.prg');
const fixtureLines = fs.readFileSync(fixturePath, 'utf8').split(/\r?\n/);
const fixtureConflicts = findDeclarationNumberConflicts(fixtureLines);

assert(
    fixtureConflicts.some((c) =>
        c.varName === 'D' && c.existingVar === 'PP' && c.sectionName === 'SECTION_B'),
    'SECTION_B should report D=5 conflicting with PP=5 even when D was declared in SECTION_A'
);

assert(
    fixtureConflicts.some((c) =>
        c.varName === 'Other' && c.existingVar === 'D' && c.sectionName === 'SECTION_A'),
    'second SECTION_A block should conflict with D=5 from first block in same section'
);

assert(
    !fixtureConflicts.some((c) => c.sectionName === 'SECTION_A' && c.varName === 'PP'),
    'PP=22 in SECTION_A should not conflict with PP=5 in SECTION_B'
);

const crossSectionLines = [
    '#MACROS#',
    '@declarationnumbercheckstart',
    'var1 = 1',
    '@declarationnumbercheckend',
    '#MACROS#',
    '#MACROS2#',
    '@declarationnumbercheckstart',
    'var2 = 1',
    '@declarationnumbercheckend',
    '#MACROS2#',
];
const crossSectionConflicts = findDeclarationNumberConflicts(crossSectionLines);
assert(crossSectionConflicts.length === 0, 'same number in different #SECTION# bodies should not conflict');

const reusedNameLines = [
    '#EDIT#',
    '@declarationnumbercheckstart',
    'D = 5',
    'PP = 22',
    '@declarationnumbercheckend',
    'code',
    '@declarationnumbercheckstart',
    'PP = 5',
    'D = 5',
    '@declarationnumbercheckend',
    '#EDIT#',
];
const reusedNameConflicts = findDeclarationNumberConflicts(reusedNameLines);
assert(
    reusedNameConflicts.some((c) => c.varName === 'D' && c.existingVar === 'PP'),
    'reused variable names across blocks must still be checked'
);

console.log('declaration-number duplicate smoke test passed');
