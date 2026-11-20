'use strict';

const fs = require('fs');
const path = require('path');

const fixtureRoot = __dirname;
const customRoot = path.join(fixtureRoot, 'custom');

const variablePattern1 = /\b[A-Za-z][A-Za-z0-9_-]*\b(?=\s*=)/g;
const variablePattern2 = /\b[A-Za-z]*\b(?!$)(?=\s*=)/g;

function normalizeFsPath(filePath) {
    return path.normalize(filePath).replace(/\\/g, '/').toLowerCase();
}

function isLocalSlotVariableName(varName) {
    return typeof varName === 'string' && /^l\d+[A-Za-z][A-Za-z0-9_-]*\$?$/.test(varName);
}

function getLocalSlotPrefix(varName) {
    const match = /^l(\d+)/.exec(varName || '');
    return match ? `l${match[1]}` : '';
}

function buildLocalSlotVariableName(slotPrefix, baseName) {
    return `${slotPrefix}${baseName}`;
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

function parseLocalSlotsManifest(lines) {
    const slots = [];
    for (let i = 0; i < lines.length; i++) {
        const trimmed = lines[i].trim();
        if (trimmed.startsWith('!')) {
            continue;
        }
        // const match = trimmed.match(/^@local_slots\s+(.+)$/i);
        // if (!match) {
        //     continue;
        // }
        const parts = match[1].split(',').map((part) => part.trim()).filter(Boolean);
        for (const name of parts) {
            if (isLocalSlotVariableName(name)) {
                slots.push({ name, line: i });
            }
        }
        break;
    }
    return slots;
}

function extractDeclarationNumbersFromLines(lines) {
    const declarations = [];
    const declarationSections = findDeclarationCheckSections(lines);
    const seenVariables = new Map();

    for (const section of declarationSections) {
        const sectionLines = lines.slice(section.start + 1, section.end);
        for (let i = 0; i < sectionLines.length; i++) {
            const line = sectionLines[i];
            const actualLineIndex = section.start + 1 + i;
            const splitlines = line.split(';');
            for (const splitline of splitlines) {
                if (splitline.trim().startsWith('!')) {
                    continue;
                }
                let match = splitline.match(variablePattern1);
                if (!match) {
                    match = splitline.match(variablePattern2);
                }
                const seenKey = `${match ? match[0] : ''}`;
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
                        });
                    }
                }
            }
        }
    }
    return declarations;
}

function collectDeclarationRedeclarations(lines) {
    const redeclarations = [];
    const declarationSections = findDeclarationCheckSections(lines);
    const varsByScope = new Map();

    for (const section of declarationSections) {
        const sectionLines = lines.slice(section.start + 1, section.end);
        for (let i = 0; i < sectionLines.length; i++) {
            const line = sectionLines[i];
            const actualLineIndex = section.start + 1 + i;
            const scopeKey = 'default';
            if (!varsByScope.has(scopeKey)) {
                varsByScope.set(scopeKey, new Map());
            }
            const varOwners = varsByScope.get(scopeKey);
            const splitlines = line.split(';');
            for (const splitline of splitlines) {
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
                const varName = match[0];
                const existing = varOwners.get(varName);
                if (existing && existing.number !== declarationNumber) {
                    redeclarations.push({ varName, previousNumber: existing.number, newNumber: declarationNumber });
                }
                varOwners.set(varName, { number: declarationNumber, line: actualLineIndex });
            }
        }
    }
    return redeclarations;
}

function stripPathQuotes(value) {
    const trimmed = value.trim();
    if ((trimmed.startsWith('"') && trimmed.endsWith('"')) ||
        (trimmed.startsWith("'") && trimmed.endsWith("'"))) {
        return trimmed.slice(1, -1);
    }
    return trimmed;
}

function resolveFixturePath(inputPath) {
    const baseName = path.basename(stripPathQuotes(inputPath));
    const customCandidate = path.join(customRoot, baseName);
    if (fs.existsSync(customCandidate)) {
        return normalizeFsPath(customCandidate);
    }
    return null;
}

function extractCallLinks(content) {
    const callLinks = [];
    const lines = content.split('\n');
    const pattern = /\bcall\s*\(\s*(?:"[^"]+"|'[^']+'|[^\s),]+)/ig;
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();
        if (line.startsWith('!')) {
            continue;
        }
        let match;
        pattern.lastIndex = 0;
        while ((match = pattern.exec(line)) !== null) {
            const raw = match[0].replace(/^call\s*\(\s*/i, '').trim();
            const resolved = resolveFixturePath(raw);
            if (resolved) {
                callLinks.push({ targetPath: resolved, line: i });
            }
        }
    }
    return callLinks;
}

function buildIndex(files) {
    const index = new Map();
    for (const filePath of files) {
        const content = fs.readFileSync(filePath, 'utf8');
        const lines = content.split('\n');
        const declarations = extractDeclarationNumbersFromLines(lines);
        const localSlotDeclarations = declarations.filter((decl) => isLocalSlotVariableName(decl.varName));
        index.set(normalizeFsPath(filePath), {
            filePath,
            declarations,
            localSlotDeclarations,
            callLinks: extractCallLinks(content),
            localSlotsManifest: parseLocalSlotsManifest(lines).map((slot) => slot.name),
        });
    }

    const calleePaths = new Set();
    index.forEach((entry) => {
        for (const link of entry.callLinks) {
            calleePaths.add(link.targetPath);
        }
    });
    index.forEach((entry, filePath) => {
        entry.isCallCallee = calleePaths.has(filePath);
    });
    return index;
}

function getDeclaredLocalSlotPrefixesFromEntry(entry) {
    const prefixes = new Set();
    for (const decl of entry.localSlotDeclarations || []) {
        const prefix = getLocalSlotPrefix(decl.varName);
        if (prefix) {
            prefixes.add(prefix);
        }
    }
    return prefixes;
}

function getCanonicalLocalSlotPrefixFromEntry(entry) {
    const prefixes = getDeclaredLocalSlotPrefixesFromEntry(entry);
    if (prefixes.size === 0) {
        return '';
    }
    return [...prefixes].sort((a, b) => Number(a.slice(1)) - Number(b.slice(1)))[0];
}

function findWorkspaceLocalSlotNameConflicts(index) {
    const conflicts = [];
    const prefixOwners = new Map();
    index.forEach((entry, filePath) => {
        if (!entry.isCallCallee) {
            return;
        }
        const prefix = getCanonicalLocalSlotPrefixFromEntry(entry);
        if (!prefix || entry.localSlotDeclarations.length === 0) {
            return;
        }
        const existing = prefixOwners.get(prefix);
        if (!existing) {
            prefixOwners.set(prefix, { filePath, prefix });
            return;
        }
        if (existing.filePath !== filePath) {
            conflicts.push({ prefix, filePath, conflictsWith: existing.filePath });
        }
    });
    return conflicts;
}

function getUsedLocalSlotNumbers(index, excludeFilePath) {
    const used = new Set();
    index.forEach((entry, filePath) => {
        if (!entry.isCallCallee || filePath === excludeFilePath) {
            return;
        }
        const prefix = getCanonicalLocalSlotPrefixFromEntry(entry);
        if (!prefix) {
            return;
        }
        used.add(Number(prefix.replace(/^l/, '')));
    });
    return used;
}

function getMaxUsedLocalSlotNumber(usedNumbers) {
    let maxUsed = 0;
    usedNumbers.forEach((slotNumber) => {
        if (slotNumber > maxUsed) {
            maxUsed = slotNumber;
        }
    });
    return maxUsed;
}

function getNextAvailableLocalSlotPrefixFromUsed(usedNumbers) {
    return `l${getMaxUsedLocalSlotNumber(usedNumbers) + 1}`;
}

function getFileLocalSlotPrefix(index, filePath) {
    const entry = index.get(filePath);
    const existing = entry ? getCanonicalLocalSlotPrefixFromEntry(entry) : '';
    if (existing) {
        return existing;
    }
    const usedNumbers = getUsedLocalSlotNumbers(index, filePath);
    return getNextAvailableLocalSlotPrefixFromUsed(usedNumbers);
}

function buildLocalSlotRenamePairs(index, content, filePath) {
    const filePrefix = getFileLocalSlotPrefix(index, filePath);
    const pairs = [];
    const seen = new Set();
    const addPair = (oldName, newName) => {
        if (!oldName || oldName === newName || seen.has(oldName)) {
            return;
        }
        seen.add(oldName);
        pairs.push({ oldName, newName });
    };

    for (const varName of findNonLocalSlotDeclarations(content)) {
        addPair(varName, buildLocalSlotVariableName(filePrefix, varName));
    }

    const declarations = extractDeclarationNumbersFromLines(content.split('\n'))
        .filter((decl) => isLocalSlotVariableName(decl.varName));
    for (const decl of declarations) {
        const currentPrefix = getLocalSlotPrefix(decl.varName);
        if (currentPrefix !== filePrefix) {
            const base = decl.varName.replace(/^l\d+/, '');
            addPair(decl.varName, buildLocalSlotVariableName(filePrefix, base));
        }
    }
    return pairs;
}

function collectManifestIssues(lines) {
    const manifest = parseLocalSlotsManifest(lines);
    if (manifest.length === 0) {
        return [];
    }
    const declared = extractDeclarationNumbersFromLines(lines)
        .filter((decl) => isLocalSlotVariableName(decl.varName))
        .map((decl) => decl.varName);
    const manifestNames = new Set(manifest.map((slot) => slot.name));
    const declaredNames = new Set(declared);
    const issues = [];
    for (const slot of manifest) {
        if (!declaredNames.has(slot.name)) {
            issues.push(`missing declaration for ${slot.name}`);
        }
    }
    for (const name of declaredNames) {
        if (!manifestNames.has(name)) {
            issues.push(`undeclared in manifest: ${name}`);
        }
    }
    return issues;
}

function findNonLocalSlotDeclarations(content) {
    const lines = content.split('\n');
    const results = [];
    const declarationSections = findDeclarationCheckSections(lines);
    for (const section of declarationSections) {
        const sectionLines = lines.slice(section.start + 1, section.end);
        for (const line of sectionLines) {
            const splitlines = line.split(';');
            for (const splitline of splitlines) {
                let match = splitline.match(variablePattern1);
                if (!match) {
                    match = splitline.match(variablePattern2);
                }
                if (!match || isLocalSlotVariableName(match[0])) {
                    continue;
                }
                const valuePart = splitline.split('=')[1];
                if (valuePart !== undefined && !isNaN(valuePart) && Number(valuePart.trim()) > 0) {
                    results.push(match[0]);
                }
            }
        }
    }
    return results;
}

function countVariableReferences(content, varName) {
    const regex = new RegExp(`\\b${varName}\\b(?!\\$)`, 'gi');
    let count = 0;
    const lines = content.split('\n');
    for (const line of lines) {
        if (line.trim().startsWith('!')) {
            continue;
        }
        const codeLine = line.includes(';!') ? line.split(';!')[0] : line;
        let match;
        regex.lastIndex = 0;
        while ((match = regex.exec(codeLine)) !== null) {
            count++;
        }
    }
    return count;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const allFiles = fs.readdirSync(customRoot)
    .filter((name) => name.endsWith('.prg'))
    .map((name) => path.join(customRoot, name));
const index = buildIndex(allFiles);

const conflicts = findWorkspaceLocalSlotNameConflicts(index);
const l1Conflict = conflicts.find((c) => c.prefix === 'l1');
assert(l1Conflict, 'callee_a and callee_b should conflict on l1 prefix');
assert(
    l1Conflict.filePath.includes('callee_b.prg') &&
    l1Conflict.conflictsWith.includes('callee_a.prg'),
    `unexpected l1 prefix conflict pairing: ${JSON.stringify(l1Conflict)}`
);

assert(!conflicts.some((c) => c.prefix === 'l2'), 'l2 prefix should not conflict');

const calleeAPrefixes = getDeclaredLocalSlotPrefixesFromEntry(index.get(normalizeFsPath(path.join(customRoot, 'callee_a.prg'))));
assert(calleeAPrefixes.size === 1 && calleeAPrefixes.has('l1'), 'callee_a locals should all use l1 prefix');

const redeclareLines = fs.readFileSync(path.join(customRoot, 'redeclare.prg'), 'utf8').split('\n');
const redeclarations = collectDeclarationRedeclarations(redeclareLines);
assert(redeclarations.length === 1, 'redeclare.prg should have one redeclaration');

const badCallee = index.get(normalizeFsPath(path.join(customRoot, 'callee_bad_names.prg')));
assert(badCallee?.isCallCallee, 'callee_bad_names should be a call target');
const badNames = findNonLocalSlotDeclarations(fs.readFileSync(path.join(customRoot, 'callee_bad_names.prg'), 'utf8'));
assert(badNames.includes('foo'), 'callee_bad_names should have foo declaration');
assert(countVariableReferences(fs.readFileSync(path.join(customRoot, 'callee_bad_names.prg'), 'utf8'), 'foo') >= 2,
    'callee_bad_names should reference foo outside declaration block');

const manifestOkIssues = collectManifestIssues(fs.readFileSync(path.join(customRoot, 'callee_manifest_ok.prg'), 'utf8').split('\n'));
assert(manifestOkIssues.length === 0, `manifest ok should have no issues: ${manifestOkIssues.join(', ')}`);

const manifestBadIssues = collectManifestIssues(fs.readFileSync(path.join(customRoot, 'callee_manifest_bad.prg'), 'utf8').split('\n'));
assert(manifestBadIssues.length >= 2, 'manifest bad should report missing l1B and extra l3C');

const badPath = normalizeFsPath(path.join(customRoot, 'callee_bad_names.prg'));
const assignPrefix = getFileLocalSlotPrefix(index, badPath);
const assignPairs = buildLocalSlotRenamePairs(index, fs.readFileSync(path.join(customRoot, 'callee_bad_names.prg'), 'utf8'), badPath);
assert(assignPrefix === 'l19', `next free file prefix for callee_bad_names should be l19, got ${assignPrefix}`);
assert(assignPairs.length === 1 && assignPairs[0].newName === 'l19foo',
    `prefixed rename should be l19foo, got ${JSON.stringify(assignPairs)}`);

assert(
    getNextAvailableLocalSlotPrefixFromUsed(new Set([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18])) === 'l19',
    'should propose max used + 1 and keep uncalled gaps like l17'
);
assert(
    getNextAvailableLocalSlotPrefixFromUsed(new Set([1, 2, 3])) === 'l4',
    'should extend from highest used slot, not fill interior gaps'
);

const l17Unused = index.get(normalizeFsPath(path.join(customRoot, 'callee_l17_unused.prg')));
assert(!l17Unused?.isCallCallee, 'uncalled l17 program should not reserve l17 for callee allocation');
const l18Callee = index.get(normalizeFsPath(path.join(customRoot, 'callee_l18.prg')));
assert(l18Callee?.isCallCallee, 'called l18 program should reserve l18');

const manifestBadPairs = buildLocalSlotRenamePairs(
    index,
    fs.readFileSync(path.join(customRoot, 'callee_manifest_bad.prg'), 'utf8'),
    normalizeFsPath(path.join(customRoot, 'callee_manifest_bad.prg'))
);
assert(
    manifestBadPairs.some((pair) => pair.oldName === 'l3C' && pair.newName === 'l1C'),
    `l3C should be renamed to l1C in manifest bad file, got ${JSON.stringify(manifestBadPairs)}`
);

assert(isLocalSlotVariableName('l1SelemP'), 'l1SelemP should be a local slot name');
assert(!isLocalSlotVariableName('SelemP'), 'SelemP should not be a local slot name');
assert(!isLocalSlotVariableName('l1'), 'bare l1 should not be a local slot name');

function applyCasePattern(sourceText, patternText) {
    if (!sourceText) {
        return sourceText;
    }
    if (!patternText) {
        return sourceText;
    }
    if (patternText === patternText.toUpperCase() && patternText !== patternText.toLowerCase()) {
        return sourceText.toUpperCase();
    }
    if (patternText === patternText.toLowerCase()) {
        return sourceText.toLowerCase();
    }
    if (patternText[0] === patternText[0].toUpperCase() &&
        patternText.slice(1) === patternText.slice(1).toLowerCase()) {
        return sourceText.charAt(0).toUpperCase() + sourceText.slice(1).toLowerCase();
    }
    let result = '';
    for (let i = 0; i < sourceText.length; i++) {
        const sourceChar = sourceText[i];
        const patternChar = patternText[i];
        if (patternChar === undefined) {
            result += sourceChar;
            continue;
        }
        if (patternChar === patternChar.toUpperCase() && patternChar !== patternChar.toLowerCase()) {
            result += sourceChar.toUpperCase();
        } else if (patternChar === patternChar.toLowerCase()) {
            result += sourceChar.toLowerCase();
        } else {
            result += sourceChar;
        }
    }
    return result;
}

assert(applyCasePattern('SelemP', 'selemp') === 'selemp', 'lowercase usage should stay lowercase');
assert(applyCasePattern('SelemP', 'SELEMP') === 'SELEMP', 'uppercase usage should stay uppercase');
assert(applyCasePattern('SelemP', 'SelemP') === 'SelemP', 'matching case should be preserved');
assert(`l2${applyCasePattern('foo', 'Foo')}` === 'l2Foo', 'title-case usage should be preserved with prefix');

console.log('local-slot-names smoke test passed');
