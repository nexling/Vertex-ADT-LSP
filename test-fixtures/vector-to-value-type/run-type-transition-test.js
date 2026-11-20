'use strict';

const VECTOR_VARIABLE_TYPES = new Set([
    'pvector', 'dvector', 'xvector', 'yvector', 'zvector', 'nvector'
]);

function isVectorVariableType(type) {
    return typeof type === 'string' && VECTOR_VARIABLE_TYPES.has(type.toLowerCase());
}

function isValueVariableType(type) {
    return typeof type === 'string' && type.toLowerCase() === 'value';
}

function findVectorToValueTypeTransitionWarnings(variableTypesByName, resolveSection) {
    const warnings = [];
    const getSection = resolveSection || (() => '');

    for (const [varName, typeInfos] of variableTypesByName) {
        const scopeState = new Map();

        for (const typeInfo of typeInfos) {
            const sectionName = getSection(typeInfo.line, typeInfo);

            if (!scopeState.has(sectionName)) {
                scopeState.set(sectionName, {
                    hadVectorType: false,
                    firstVectorEntry: null
                });
            }
            const state = scopeState.get(sectionName);

            if (isVectorVariableType(typeInfo.type)) {
                if (!state.hadVectorType) {
                    state.hadVectorType = true;
                    state.firstVectorEntry = typeInfo;
                }
            }
            else if (state.hadVectorType && isValueVariableType(typeInfo.type)) {
                warnings.push({
                    varName,
                    line: typeInfo.line,
                    sectionName,
                    firstVectorEntry: state.firstVectorEntry
                });
            }
        }
    }

    return warnings;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function buildTimeline(entries) {
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

const resolveSectionFromEntry = (_line, entry) => entry.sectionName || '';

const vectorThenValue = buildTimeline([
    { name: 'MyP', type: 'pvector', line: 1, sectionName: '' },
    { name: 'MyP', type: 'value', line: 5, sectionName: '' }
]);
let warnings = findVectorToValueTypeTransitionWarnings(vectorThenValue, resolveSectionFromEntry);
assert(warnings.length === 1, 'pvector then value should warn once');
assert(warnings[0].varName === 'MyP' && warnings[0].line === 5, 'warning should be on value line');

const vectorOnly = buildTimeline([
    { name: 'MyP', type: 'dvector', line: 2, sectionName: '' }
]);
warnings = findVectorToValueTypeTransitionWarnings(vectorOnly, resolveSectionFromEntry);
assert(warnings.length === 0, 'vector only should not warn');

const valueThenVector = buildTimeline([
    { name: 'MyV', type: 'value', line: 1, sectionName: '' },
    { name: 'MyV', type: 'pvector', line: 3, sectionName: '' }
]);
warnings = findVectorToValueTypeTransitionWarnings(valueThenVector, resolveSectionFromEntry);
assert(warnings.length === 0, 'value before vector should not warn');

const vectorVectorValue = buildTimeline([
    { name: 'MyP', type: 'pvector', line: 1, sectionName: 'MACROS' },
    { name: 'MyP', type: 'dvector', line: 3, sectionName: 'MACROS' },
    { name: 'MyP', type: 'value', line: 7, sectionName: 'MACROS' }
]);
warnings = findVectorToValueTypeTransitionWarnings(vectorVectorValue, resolveSectionFromEntry);
assert(warnings.length === 1, 'pvector to dvector to value should warn on value line');
assert(warnings[0].firstVectorEntry.type === 'pvector', 'first vector entry should be remembered');

const valueVectorValue = buildTimeline([
    { name: 'MyP', type: 'value', line: 1, sectionName: 'MACROS' },
    { name: 'MyP', type: 'pvector', line: 3, sectionName: 'MACROS' },
    { name: 'MyP', type: 'value', line: 7, sectionName: 'MACROS' }
]);
warnings = findVectorToValueTypeTransitionWarnings(valueVectorValue, resolveSectionFromEntry);
assert(warnings.length === 1, 'value after vector should warn even if value appeared earlier in same section');

const arrayThenValue = buildTimeline([
    { name: 'Arr', type: 'array', line: 1, sectionName: '' },
    { name: 'Arr', type: 'value', line: 2, sectionName: '' }
]);
warnings = findVectorToValueTypeTransitionWarnings(arrayThenValue, resolveSectionFromEntry);
assert(warnings.length === 0, 'array to value should not warn');

const differentSections = buildTimeline([
    { name: 'MyP', type: 'pvector', line: 2, sectionName: 'MACROS' },
    { name: 'MyP', type: 'value', line: 8, sectionName: 'MACROS2' }
]);
warnings = findVectorToValueTypeTransitionWarnings(differentSections, resolveSectionFromEntry);
assert(warnings.length === 0, 'vector in one section and value in another should not warn');

const sameNameDifferentSectionsBothWarn = buildTimeline([
    { name: 'MyP', type: 'pvector', line: 2, sectionName: 'MACROS' },
    { name: 'MyP', type: 'value', line: 4, sectionName: 'MACROS' },
    { name: 'MyP', type: 'pvector', line: 12, sectionName: 'MACROS2' },
    { name: 'MyP', type: 'value', line: 14, sectionName: 'MACROS2' }
]);
warnings = findVectorToValueTypeTransitionWarnings(sameNameDifferentSectionsBothWarn, resolveSectionFromEntry);
assert(warnings.length === 2, 'each section should warn independently');

console.log('vector-to-value-type smoke test passed');
