'use strict';

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
    for (let i = 0; i < fileSections.length; i++) {
        const section = fileSections[i];
        if (lineIndex >= section.bodyStart && lineIndex <= section.bodyEnd) {
            return section.name;
        }
    }
    return '';
}

function buildVariableTypesByName(entries) {
    const map = new Map();
    for (const entry of entries) {
        if (!map.has(entry.name)) {
            map.set(entry.name, []);
        }
        map.get(entry.name).push(entry);
    }
    for (const typeInfos of map.values()) {
        typeInfos.sort((a, b) => a.line - b.line);
    }
    return map;
}

function getCurrentVariableType(varName, currentLine, fileSections, variableTypesByName) {
    const sectionName = fileSections
        ? getSectionForLine(currentLine, fileSections)
        : '';

    const typeInfos = variableTypesByName.get(varName);
    let closestTypeInfo = null;

    if (typeInfos) {
        for (let i = typeInfos.length - 1; i >= 0; i--) {
            if (typeInfos[i].line <= currentLine &&
                (typeInfos[i].sectionName ?? '') === sectionName) {
                closestTypeInfo = typeInfos[i];
                break;
            }
        }
    }

    return closestTypeInfo ? closestTypeInfo.type : null;
}

function getVariableType2(variableName, lineNumber, sectionName, variableTypes) {
    const targetSection = sectionName ?? '';
    let mostRecentType = 'generic';
    let mostRecentLine = -1;

    for (const varType of variableTypes) {
        if (varType.name === variableName &&
            varType.line < lineNumber &&
            (varType.sectionName ?? '') === targetSection &&
            varType.line >= mostRecentLine) {
            mostRecentType = varType.type;
            mostRecentLine = varType.line;
        }
    }

    return mostRecentType;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const sampleLines = [
    'MyP = vec_to(1,2,3) ;! [pvector]',          // 0 file-level
    '#MACROS#',                                    // 1
    'MyP = vec_to(1,2,3) ;! [pvector]',          // 2 MACROS bodyStart
    'Other = MyP',                                 // 3
    'UseP = MyP',                                  // 4
    '#MACROS#',                                    // 5
    '#MACROS2#',                                   // 6
    'UseP2 = MyP',                                  // 7 MACROS2 bodyStart
    '#MACROS2#',                                   // 8
    '#JOINT#',                                     // 9
    '##included_from callee.prg',                  // 10 JOINT bodyStart
    'Ref = IncludedVar',                           // 11
    '#JOINT#',                                     // 12
];

const fileSections = parseFileSections(sampleLines);

const timeline = [
    { name: 'MyP', type: 'pvector', line: 0, sectionName: '' },
    { name: 'MyP', type: 'pvector', line: 2, sectionName: 'MACROS' },
    { name: 'Other', type: 'pvector', line: 3, sectionName: 'MACROS' },
    { name: 'UseP', type: 'pvector', line: 4, sectionName: 'MACROS' },
    { name: 'IncludedVar', type: 'plane', line: 10, sectionName: 'JOINT' },
];

const typesByName = buildVariableTypesByName(timeline);

assert(
    getCurrentVariableType('MyP', 4, fileSections, typesByName) === 'pvector',
    'MyP inside MACROS should resolve to pvector'
);

assert(
    getCurrentVariableType('MyP', 7, fileSections, typesByName) === null,
    'MyP inside MACROS2 without local typing should not inherit MACROS type'
);

assert(
    getCurrentVariableType('MyP', 0, fileSections, typesByName) === 'pvector',
    'file-level MyP should resolve independently from MACROS'
);

assert(
    getCurrentVariableType('IncludedVar', 11, fileSections, typesByName) === 'plane',
    'included_from type should resolve inside JOINT section'
);

assert(
    getCurrentVariableType('IncludedVar', 0, fileSections, typesByName) === null,
    'included_from type should not resolve outside JOINT section'
);

const copied = getVariableType2('MyP', 4, 'MACROS', timeline);
assert(copied === 'pvector', 'getVariableType2 should copy type within same section');

const copiedWrongSection = getVariableType2('MyP', 8, 'MACROS2', timeline);
assert(
    copiedWrongSection === 'generic',
    'getVariableType2 should not see MACROS type from MACROS2 section'
);

console.log('section-aware type lookup tests passed');
