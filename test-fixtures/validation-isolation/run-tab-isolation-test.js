'use strict';

/**
 * Portable simulation of per-document variable extraction before validation.
 * The real server keeps variablesSet in module scope; pull diagnostics for an
 * inactive tab must re-run extractVariables for that file or it inherits the
 * active tab's declarations and reports spurious "used before declaration" errors.
 */

const variablePattern = /\b([A-Za-z][A-Za-z0-9_-]*)\b(?=\s*=)/g;
const callArgPattern = /\bcall\s*\(([^)]*)\)/i;

function extractVariablesIntoSet(text, variablesSet) {
    variablesSet.clear();
    for (const line of text.split('\n')) {
        const trimmed = line.trim();
        if (trimmed.startsWith('!')) {
            continue;
        }
        let match;
        while ((match = variablePattern.exec(trimmed)) !== null) {
            variablesSet.add(match[1]);
        }
    }
}

function callArgumentsUsedBeforeDeclaration(codeLine, variablesSet) {
    const callMatch = codeLine.match(callArgPattern);
    if (!callMatch) {
        return [];
    }
    const errors = [];
    for (const part of callMatch[1].split(',')) {
        const variableName = part.trim();
        if (!variableName || !/^[A-Za-z]/.test(variableName)) {
            continue;
        }
        if (!variablesSet.has(variableName)) {
            errors.push(variableName);
        }
    }
    return errors;
}

function validateFileText(text, variablesSet) {
    extractVariablesIntoSet(text, variablesSet);
    const undeclared = [];
    for (const line of text.split('\n')) {
        for (const variableName of callArgumentsUsedBeforeDeclaration(line, variablesSet)) {
            undeclared.push(variableName);
        }
    }
    return undeclared;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const fileA = [
    'declaredInA = 1',
].join('\n');

const fileB = [
    'declaredInB = 1',
    'call(declaredInB)',
].join('\n');

const sharedVariablesSet = new Set();

// Buggy path: extract active file A, validate inactive file B without re-extract.
extractVariablesIntoSet(fileA, sharedVariablesSet);
const staleErrors = callArgumentsUsedBeforeDeclaration('call(declaredInB)', sharedVariablesSet);
assert(
    staleErrors.includes('declaredInB'),
    'stale simulation should show false positive when B is validated with A variable map'
);

// Fixed path: each validation rebuilds variablesSet for its own text.
const errorsForB = validateFileText(fileB, sharedVariablesSet);
assert(
    errorsForB.length === 0,
    `file B should not report used-before-declaration for its own variable; got: ${errorsForB.join(', ')}`
);

const errorsForA = validateFileText(fileA, sharedVariablesSet);
assert(
    errorsForA.length === 0,
    `file A should validate cleanly; got: ${errorsForA.join(', ')}`
);

// Mutex: only one validation at a time may mutate shared module state.
let validationQueue = Promise.resolve();
let concurrent = 0;
let maxConcurrent = 0;

function runValidationExclusive(fn) {
    const run = validationQueue.then(async () => {
        concurrent++;
        maxConcurrent = Math.max(maxConcurrent, concurrent);
        try {
            return await fn();
        } finally {
            concurrent--;
        }
    });
    validationQueue = run.catch(() => {});
    return run;
}

const runs = [
    runValidationExclusive(async () => validateFileText(fileA, sharedVariablesSet)),
    runValidationExclusive(async () => validateFileText(fileB, sharedVariablesSet)),
    runValidationExclusive(async () => validateFileText(fileA, sharedVariablesSet)),
];

Promise.all(runs).then(() => {
    assert(maxConcurrent === 1, `validation mutex should serialize runs; max concurrent was ${maxConcurrent}`);
    console.log('tab-isolation smoke test passed');
}).catch((err) => {
    console.error(err.message || err);
    process.exit(1);
});
