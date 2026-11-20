'use strict';

const fs = require('fs');
const path = require('path');

const fixtureRoot = __dirname;
const customRoot = path.join(fixtureRoot, 'custom');
const systemRoot = path.join(fixtureRoot, 'system');
const ADT_FILE_EXTENSIONS = new Set(['.prg', '.adt', '.p', '.vxt', '.3d', '.2d']);
const variablePattern1 = /\b[A-Za-z][A-Za-z0-9_-]*\b(?=\s*=)/g;
const variablePattern2 = /\b[A-Za-z]*\b(?!$)(?=\s*=)/g;

function normalizeFsPath(filePath) {
    return path.normalize(filePath).replace(/\\/g, '/').toLowerCase();
}

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

function stripPathQuotes(value) {
    const trimmed = value.trim();
    if ((trimmed.startsWith('"') && trimmed.endsWith('"')) ||
        (trimmed.startsWith("'") && trimmed.endsWith("'"))) {
        return trimmed.slice(1, -1);
    }
    return trimmed;
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

const INCLUDE_CALL_SECTION_KEYWORDS = new Set(['section', 'sectionoffile']);

function parseIncludeCallPathAndSection(rawPath, lineRemainder) {
    let pathPart = stripPathQuotes(rawPath).replace(/\\/g, '/').trim();
    let targetSection = null;

    const pathSectionMatch = pathPart.match(/^(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$/);
    if (pathSectionMatch) {
        const token = pathSectionMatch[2];
        if (!INCLUDE_CALL_SECTION_KEYWORDS.has(token.toLowerCase())) {
            targetSection = token;
            pathPart = pathSectionMatch[1].trim();
        } else {
            pathPart = pathPart.replace(/\s+SECTION(?:OFFILE)?$/i, '').trim();
        }
    }

    if (lineRemainder && lineRemainder.trim()) {
        const remMatch = lineRemainder.trim().match(/^([A-Za-z_][A-Za-z0-9_]*)\b/);
        if (remMatch && !INCLUDE_CALL_SECTION_KEYWORDS.has(remMatch[1].toLowerCase())) {
            targetSection = remMatch[1];
        }
    }

    return { filePath: pathPart, targetSection };
}

function getDeclarationVariableType(varName, lineIndex, line, fileVariableTypes) {
    const hoverPattern = /;\s*!"([^"]*)"/;
    const hoverMatch = line.match(hoverPattern);
    if (hoverMatch) {
        const bracketMatch = hoverMatch[1].match(/\[(.*?)\]/);
        if (bracketMatch) {
            return bracketMatch[1].trim().toLowerCase();
        }
    }

    let closestTypeInfo = null;
    for (const typeInfo of fileVariableTypes) {
        if (typeInfo.name === varName && typeInfo.line <= lineIndex) {
            if (!closestTypeInfo || typeInfo.line > closestTypeInfo.line) {
                closestTypeInfo = typeInfo;
            }
        }
    }

    return closestTypeInfo ? closestTypeInfo.type : null;
}

function isValueDeclarationType(variableType) {
    return typeof variableType === 'string' && variableType.toLowerCase() === 'value';
}

function buildFileVariableTypes(lines) {
    const fileVariableTypes = [];
    const hoverPattern = /;\s*!"([^"]*)"/;

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        const hoverMatch = line.match(hoverPattern);
        if (!hoverMatch) {
            continue;
        }
        const bracketMatch = hoverMatch[1].match(/\[(.*?)\]/);
        if (!bracketMatch) {
            continue;
        }
        const match = line.match(/\b([A-Za-z][A-Za-z0-9_-]*)\s*=/);
        if (match) {
            fileVariableTypes.push({
                name: match[1],
                type: bracketMatch[1].trim().toLowerCase(),
                line: i
            });
        }
    }

    return fileVariableTypes;
}

function extractDeclarationNumbersFromLines(lines, fileVariableTypes) {
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
                            lineText: line,
                            sectionName,
                            variableType: getDeclarationVariableType(
                                match[0],
                                actualLineIndex,
                                line,
                                fileVariableTypes || []
                            )
                        });
                    }
                }
            }
        }
    }

    return declarations;
}

function extractFileDependencies(content, resolvePath) {
    const includeLinks = [];
    const callLinks = [];
    let commonIndexes = false;
    let commonPoints = false;
    const seenIncludes = new Set();
    const seenCalls = new Set();
    const contentLines = content.split('\n');
    const fileSections = parseFileSections(contentLines);

    for (let i = 0; i < contentLines.length; i++) {
        const trimmed = contentLines[i].trim();
        if (trimmed.startsWith('!')) {
            continue;
        }
        if (/^common_indexes\s+1\b/i.test(trimmed)) {
            commonIndexes = true;
        }
        if (/^common_points\s+1\b/i.test(trimmed)) {
            commonPoints = true;
        }

        const sourceSection = getSectionForLine(i, fileSections);

        if (trimmed.startsWith('# include')) {
            const afterInclude = trimmed.substring('# include'.length).trim();
            const pathMatch = afterInclude.match(/^((?:"[^"]+"|'[^']+'|[^\s#;]+))\s*(.*)$/);
            if (!pathMatch) {
                continue;
            }
            const parsed = parseIncludeCallPathAndSection(pathMatch[1], pathMatch[2]);
            const resolved = resolvePath(parsed.filePath);
            if (resolved) {
                const normalized = normalizeFsPath(resolved);
                const seenKey = `${normalized}::${parsed.targetSection || ''}`;
                if (!seenIncludes.has(seenKey)) {
                    seenIncludes.add(seenKey);
                    includeLinks.push({
                        targetPath: normalized,
                        targetSection: parsed.targetSection,
                        sourceSection,
                        line: i
                    });
                }
            }
            continue;
        }
        const callPatterns = [
            /call\s*\(\s*["']([^"']+)["']/gi,
            /\bcall\s+([^\s,(]+)/gi
        ];
        for (const pattern of callPatterns) {
            pattern.lastIndex = 0;
            let match;
            while ((match = pattern.exec(trimmed)) !== null) {
                const parsed = parseIncludeCallPathAndSection(match[1].trim(), '');
                const resolved = resolvePath(parsed.filePath);
                if (resolved) {
                    const normalized = normalizeFsPath(resolved);
                    const seenKey = `${normalized}::${parsed.targetSection || ''}`;
                    if (!seenCalls.has(seenKey)) {
                        seenCalls.add(seenKey);
                        callLinks.push({
                            targetPath: normalized,
                            targetSection: parsed.targetSection,
                            sourceSection,
                            line: i
                        });
                    }
                }
            }
        }
    }

    return {
        includeLinks,
        callLinks,
        includes: includeLinks.map((link) => link.targetPath),
        calls: callLinks.map((link) => link.targetPath),
        commonIndexes,
        commonPoints
    };
}

function walkWorkspaceAdtFiles(workspaceRoot, customName) {
    const results = [];
    const seenRelative = new Set();

    function walkDir(baseDir, relativePrefix) {
        if (!fs.existsSync(baseDir)) {
            return;
        }
        for (const entry of fs.readdirSync(baseDir, { withFileTypes: true })) {
            const relPath = relativePrefix ? `${relativePrefix}/${entry.name}` : entry.name;
            const normalizedRel = relPath.replace(/\\/g, '/');
            const absPath = path.join(baseDir, entry.name);
            if (entry.isDirectory()) {
                walkDir(absPath, normalizedRel);
            }
            else if (entry.isFile()) {
                const ext = path.extname(entry.name).toLowerCase();
                if (!ADT_FILE_EXTENSIONS.has(ext) || seenRelative.has(normalizedRel)) {
                    continue;
                }
                seenRelative.add(normalizedRel);
                results.push({ absPath: path.normalize(absPath), relativePath: normalizedRel });
            }
        }
    }

    walkDir(path.join(workspaceRoot, customName), '');
    walkDir(path.join(workspaceRoot, 'system'), '');
    return results;
}

function resolveFixturePath(inputPath) {
    const customCandidate = path.join(customRoot, inputPath);
    if (fs.existsSync(customCandidate)) {
        return customCandidate;
    }
    const systemCandidate = path.join(systemRoot, inputPath);
    if (fs.existsSync(systemCandidate)) {
        return systemCandidate;
    }
    return null;
}

function buildIndex() {
    const index = new Map();
    const files = walkWorkspaceAdtFiles(fixtureRoot, 'custom');
    for (const file of files) {
        const content = fs.readFileSync(file.absPath, 'utf8');
        const lines = content.split('\n');
        const fileVariableTypes = buildFileVariableTypes(lines);
        const deps = extractFileDependencies(content, resolveFixturePath);
        index.set(normalizeFsPath(file.absPath), {
            relativePath: file.relativePath,
            declarations: extractDeclarationNumbersFromLines(lines, fileVariableTypes),
            includeLinks: deps.includeLinks,
            callLinks: deps.callLinks,
            includes: deps.includes,
            calls: deps.calls,
            commonIndexes: deps.commonIndexes,
            commonPoints: deps.commonPoints
        });
    }
    return index;
}

function getForwardNamespaceClosure(rootPath, index) {
    const group = new Set();
    const normalizedRoot = normalizeFsPath(rootPath);
    const queue = [normalizedRoot];

    while (queue.length > 0) {
        const current = queue.pop();
        if (group.has(current)) {
            continue;
        }
        group.add(current);
        const entry = index.get(current);
        if (!entry) {
            continue;
        }
        for (const link of entry.includeLinks || []) {
            queue.push(link.targetPath);
        }
        if (entry.commonIndexes || entry.commonPoints) {
            for (const link of entry.callLinks || []) {
                queue.push(link.targetPath);
            }
        }
    }

    return group;
}

function getDirectIncomingNamespaceParents(targetPath, index) {
    const parents = new Set();
    const normalizedTarget = normalizeFsPath(targetPath);
    index.forEach((otherEntry, otherPath) => {
        for (const link of otherEntry.includeLinks || []) {
            if (link.targetPath === normalizedTarget) {
                parents.add(otherPath);
            }
        }
        if (otherEntry.commonIndexes || otherEntry.commonPoints) {
            for (const link of otherEntry.callLinks || []) {
                if (link.targetPath === normalizedTarget) {
                    parents.add(otherPath);
                }
            }
        }
    });
    return parents;
}

function getUltimateNamespaceRoots(filePath, index) {
    const roots = new Set();
    const visited = new Set();

    function walkUp(node) {
        if (visited.has(node)) {
            return;
        }
        visited.add(node);
        const parents = getDirectIncomingNamespaceParents(node, index);
        if (parents.size === 0) {
            roots.add(node);
            return;
        }
        parents.forEach(walkUp);
    }

    walkUp(normalizeFsPath(filePath));
    return roots;
}

function namespaceClosuresEqual(a, b) {
    if (a.size !== b.size) {
        return false;
    }
    for (const value of a) {
        if (!b.has(value)) {
            return false;
        }
    }
    return true;
}

function getDeclarationConflictScopes(filePath, index) {
    const normalized = normalizeFsPath(filePath);
    const scopes = [];
    const seenClosures = [];

    function addScope(rootPath) {
        const closure = getForwardNamespaceClosure(rootPath, index);
        for (let i = 0; i < seenClosures.length; i++) {
            if (namespaceClosuresEqual(seenClosures[i], closure)) {
                return;
            }
        }
        seenClosures.push(closure);
        scopes.push({ root: normalizeFsPath(rootPath), files: closure });
    }

    const parents = getDirectIncomingNamespaceParents(normalized, index);
    const roots = getUltimateNamespaceRoots(normalized, index);
    if (parents.size === 0) {
        addScope(normalized);
    }
    else {
        roots.forEach((root) => addScope(root));
    }
    if (scopes.length === 0) {
        addScope(normalized);
    }
    return scopes;
}

function declarationMatchesLinkSection(declSectionName, targetSection, sourceSection) {
    if (targetSection) {
        return declSectionName === targetSection;
    }
    return declSectionName === sourceSection;
}

function buildEffectiveConflictParticipants(groupPaths, index) {
    const participants = [];

    for (const filePath of groupPaths) {
        const entry = index.get(filePath);
        if (!entry) {
            continue;
        }

        for (const decl of entry.declarations) {
            if (decl.number <= 0 || isValueDeclarationType(decl.variableType)) {
                continue;
            }
            const sectionName = decl.sectionName || '';
            participants.push({
                effectiveScopeKey: `${filePath}::${sectionName}`,
                number: decl.number,
                varName: decl.varName,
                filePath,
                sectionName,
                isVirtual: false
            });
        }

        for (const link of entry.includeLinks || []) {
            const calleeEntry = index.get(link.targetPath);
            if (!calleeEntry) {
                continue;
            }
            const sourceSection = link.sourceSection || '';
            for (const decl of calleeEntry.declarations) {
                if (decl.number <= 0 || isValueDeclarationType(decl.variableType)) {
                    continue;
                }
                const declSectionName = decl.sectionName || '';
                if (!declarationMatchesLinkSection(declSectionName, link.targetSection, sourceSection)) {
                    continue;
                }
                participants.push({
                    effectiveScopeKey: `${filePath}::${sourceSection}`,
                    number: decl.number,
                    varName: decl.varName,
                    filePath: link.targetPath,
                    sectionName: sourceSection,
                    isVirtual: true
                });
            }
        }

        if (entry.commonIndexes || entry.commonPoints) {
            for (const link of entry.callLinks || []) {
                const calleeEntry = index.get(link.targetPath);
                if (!calleeEntry) {
                    continue;
                }
                const sourceSection = link.sourceSection || '';
                for (const decl of calleeEntry.declarations) {
                    if (decl.number <= 0 || isValueDeclarationType(decl.variableType)) {
                        continue;
                    }
                    const declSectionName = decl.sectionName || '';
                    if (!declarationMatchesLinkSection(declSectionName, link.targetSection, sourceSection)) {
                        continue;
                    }
                    participants.push({
                        effectiveScopeKey: `${filePath}::${sourceSection}`,
                        number: decl.number,
                        varName: decl.varName,
                        filePath: link.targetPath,
                        sectionName: sourceSection,
                        isVirtual: true
                    });
                }
            }
        }
    }

    return participants;
}

function findCrossFileDeclarationConflicts(groupPaths, index) {
    const conflicts = [];
    const numberOwners = new Map();
    const conflictKeys = new Set();
    const participants = buildEffectiveConflictParticipants(groupPaths, index);

    for (const participant of participants) {
        const ownerKey = `${participant.effectiveScopeKey}::${participant.number}`;
        const existing = numberOwners.get(ownerKey);
        if (!existing) {
            numberOwners.set(ownerKey, participant);
            continue;
        }
        if (existing.varName === participant.varName) {
            continue;
        }
        const pairKey = `${ownerKey}::${[participant.varName, existing.varName].sort().join('::')}`;
        if (conflictKeys.has(pairKey)) {
            continue;
        }
        conflictKeys.add(pairKey);
        conflicts.push({
            number: participant.number,
            filePath: participant.filePath,
            sectionName: participant.sectionName,
            varName: participant.varName,
            other: existing.filePath,
            otherVarName: existing.varName
        });
        conflicts.push({
            number: existing.number,
            filePath: existing.filePath,
            sectionName: existing.sectionName,
            varName: existing.varName,
            other: participant.filePath,
            otherVarName: participant.varName
        });
    }

    return conflicts;
}

function resolveProgramDisplayPath(targetPath, index, workspaceRoot) {
    const normalized = normalizeFsPath(targetPath);
    const entry = index.get(normalized);
    if (entry && entry.relativePath) {
        return entry.relativePath.replace(/\\/g, '/');
    }
    if (workspaceRoot) {
        const relative = path.relative(workspaceRoot, targetPath);
        if (relative && !relative.startsWith('..') && !path.isAbsolute(relative)) {
            return relative.replace(/\\/g, '/');
        }
    }
    return path.basename(targetPath);
}

function aggregateProgramUsage(index, workspaceRoot) {
    const usage = new Map();

    index.forEach((entry) => {
        for (const link of entry.callLinks) {
            const target = normalizeFsPath(link.targetPath);
            let row = usage.get(target);
            if (!row) {
                row = {
                    callCount: 0,
                    includeCount: 0,
                    displayPath: resolveProgramDisplayPath(target, index, workspaceRoot)
                };
                usage.set(target, row);
            }
            row.callCount++;
        }

        for (const link of entry.includeLinks) {
            const target = normalizeFsPath(link.targetPath);
            let row = usage.get(target);
            if (!row) {
                row = {
                    callCount: 0,
                    includeCount: 0,
                    displayPath: resolveProgramDisplayPath(target, index, workspaceRoot)
                };
                usage.set(target, row);
            }
            row.includeCount++;
        }
    });

    const rows = [];
    usage.forEach((row) => {
        if (row.callCount > 0 || row.includeCount > 0) {
            rows.push(row);
        }
    });
    rows.sort((a, b) => a.displayPath.localeCompare(b.displayPath));
    return rows;
}

function aggregateDeclarationNumbers(index) {
    const counts = new Map();

    index.forEach((entry) => {
        for (const decl of entry.declarations) {
            if (decl.number <= 0) {
                continue;
            }
            const key = `${decl.varName}\0${decl.number}`;
            let row = counts.get(key);
            if (!row) {
                row = { varName: decl.varName, number: decl.number, count: 0 };
                counts.set(key, row);
            }
            row.count++;
        }
    });

    const rows = Array.from(counts.values());
    rows.sort((a, b) => {
        const nameCmp = a.varName.localeCompare(b.varName);
        if (nameCmp !== 0) {
            return nameCmp;
        }
        return a.number - b.number;
    });
    return rows;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function hasCrossScopeConflict(fileA, fileB, index) {
    const scopesA = getDeclarationConflictScopes(fileA, index);
    for (const scope of scopesA) {
        if (!scope.files.has(fileB)) {
            continue;
        }
        const conflicts = findCrossFileDeclarationConflicts(scope.files, index);
        if (conflicts.some((c) => c.filePath === fileA && c.other === fileB)) {
            return true;
        }
    }
    return false;
}

const index = buildIndex();
const mainInclude = normalizeFsPath(path.join(customRoot, 'main_include.prg'));
const callerNoCommon = normalizeFsPath(path.join(customRoot, 'caller_no_common.prg'));
const callerWithCommon = normalizeFsPath(path.join(customRoot, 'caller_with_common.prg'));
const customOverride = normalizeFsPath(path.join(customRoot, 'custom_override.prg'));
const callerA = normalizeFsPath(path.join(customRoot, 'caller_a.prg'));
const callerB = normalizeFsPath(path.join(customRoot, 'caller_b.prg'));
const sharedCallee = normalizeFsPath(path.join(customRoot, 'shared_callee.prg'));
const includeCallerA = normalizeFsPath(path.join(customRoot, 'include_caller_a.prg'));
const includeCallerB = normalizeFsPath(path.join(customRoot, 'include_caller_b.prg'));
const sharedInclude = normalizeFsPath(path.join(customRoot, 'shared_include.prg'));

assert(index.has(mainInclude), 'main_include.prg should be indexed');
assert(!index.has(normalizeFsPath(path.join(systemRoot, 'custom_override.prg'))),
    'system/custom_override.prg should be skipped when custom copy exists');

const includeScopes = getDeclarationConflictScopes(mainInclude, index);
const includeConflicts = findCrossFileDeclarationConflicts(includeScopes[0].files, index);
assert(includeConflicts.length > 0, 'include chain should produce a cross-file conflict');

const noCommonScopes = getDeclarationConflictScopes(callerNoCommon, index);
const noCommonConflicts = findCrossFileDeclarationConflicts(noCommonScopes[0].files, index);
assert(noCommonConflicts.length === 0, 'call without common_* should not produce cross-file conflicts');

const commonScopes = getDeclarationConflictScopes(callerWithCommon, index);
const commonConflicts = findCrossFileDeclarationConflicts(commonScopes[0].files, index);
assert(commonConflicts.length > 0, 'call with common_indexes 1 should produce cross-file conflicts');

const overrideEntry = index.get(customOverride);
assert(overrideEntry.declarations.some((d) => d.varName === 'CustomWins'),
    'custom override file should be indexed from custom folder');

const callerAScopes = getDeclarationConflictScopes(callerA, index);
assert(callerAScopes.length === 1, 'caller_a should have one scope');
assert(callerAScopes[0].files.has(sharedCallee), 'caller_a scope should include shared callee');
const callerAConflicts = findCrossFileDeclarationConflicts(callerAScopes[0].files, index);
assert(callerAConflicts.length > 0, 'caller_a and shared callee should conflict on number 10');

const callerBScopes = getDeclarationConflictScopes(callerB, index);
assert(callerBScopes.length === 1, 'caller_b should have one scope');
const callerBConflicts = findCrossFileDeclarationConflicts(callerBScopes[0].files, index);
assert(callerBConflicts.length > 0, 'caller_b and shared callee should conflict on number 10');

assert(!hasCrossScopeConflict(callerA, callerB, index),
    'caller_a and caller_b must not conflict through shared callee hub');

const sharedCalleeScopes = getDeclarationConflictScopes(sharedCallee, index);
assert(sharedCalleeScopes.length === 2, 'shared callee should have separate scopes per caller root');

const includeAScopes = getDeclarationConflictScopes(includeCallerA, index);
const includeAConflicts = findCrossFileDeclarationConflicts(includeAScopes[0].files, index);
assert(includeAConflicts.length > 0, 'include_caller_a should conflict with shared_include');

const includeBScopes = getDeclarationConflictScopes(includeCallerB, index);
const includeBConflicts = findCrossFileDeclarationConflicts(includeBScopes[0].files, index);
assert(includeBConflicts.length > 0, 'include_caller_b should conflict with shared_include');

assert(!hasCrossScopeConflict(includeCallerA, includeCallerB, index),
    'include callers must not conflict through shared include hub');

const sharedIncludeScopes = getDeclarationConflictScopes(sharedInclude, index);
assert(sharedIncludeScopes.length === 2, 'shared include should have separate scopes per includer root');

const callerValueSkip = normalizeFsPath(path.join(customRoot, 'caller_value_skip.prg'));
const sharedCalleeValue = normalizeFsPath(path.join(customRoot, 'shared_callee_value.prg'));
const valueSkipScopes = getDeclarationConflictScopes(callerValueSkip, index);
const valueSkipConflicts = findCrossFileDeclarationConflicts(valueSkipScopes[0].files, index);
assert(valueSkipConflicts.length >= 1, 'only non-value declarations should conflict');
assert(
    valueSkipConflicts.some((c) =>
        (c.filePath === callerValueSkip && c.other === sharedCalleeValue) ||
        (c.filePath === sharedCalleeValue && c.other === callerValueSkip)),
    'ObjectSlot and CalleeObject should conflict on number 30'
);

const programUsage = aggregateProgramUsage(index, fixtureRoot);
const sharedCalleeUsage = programUsage.find((row) => row.displayPath.endsWith('shared_callee.prg'));
assert(sharedCalleeUsage && sharedCalleeUsage.callCount === 2,
    'shared_callee should have global call_count 2 from caller_a and caller_b');

const sharedIncludeUsage = programUsage.find((row) => row.displayPath.endsWith('shared_include.prg'));
assert(sharedIncludeUsage && sharedIncludeUsage.includeCount === 2,
    'shared_include should have global include_count 2 from both include callers');

const declarationNumbers = aggregateDeclarationNumbers(index);
const customWinsRow = declarationNumbers.find((row) => row.varName === 'CustomWins' && row.number === 99);
assert(customWinsRow && customWinsRow.count === 1,
    'CustomWins=99 should count only the custom override file');

const sharedCalleeDecl = declarationNumbers.find((row) => row.varName === 'SharedCalleeVar' && row.number === 10);
assert(sharedCalleeDecl && sharedCalleeDecl.count === 1,
    'SharedCalleeVar=10 should appear once in declaration report');

const sectionMacros = normalizeFsPath(path.join(customRoot, 'section_macros.prg'));
const sectionMacrosEntry = index.get(sectionMacros);
assert(sectionMacrosEntry, 'section_macros.prg should be indexed');
assert(
    sectionMacrosEntry.declarations.some((d) => d.varName === 'var1' && d.sectionName === 'MACROS'),
    'var1 should be tagged with MACROS section'
);
assert(
    sectionMacrosEntry.declarations.some((d) => d.varName === 'varP' && d.sectionName === 'MACROS2'),
    'varP should be tagged with MACROS2 section'
);
const sectionMacrosScopes = getDeclarationConflictScopes(sectionMacros, index);
const sectionMacrosConflicts = findCrossFileDeclarationConflicts(sectionMacrosScopes[0].files, index);
assert(sectionMacrosConflicts.length === 0,
    'same declaration number in different #SECTION# bodies should not conflict');

const sectionIncludeCaller = normalizeFsPath(path.join(customRoot, 'section_include_caller.prg'));
const sectionIncludeCallee = normalizeFsPath(path.join(customRoot, 'section_include_callee.prg'));
const sectionIncludeScopes = getDeclarationConflictScopes(sectionIncludeCaller, index);
const sectionIncludeConflicts = findCrossFileDeclarationConflicts(sectionIncludeScopes[0].files, index);
assert(
    sectionIncludeConflicts.some((c) =>
        c.varName === 'CallerVar' && c.otherVarName === 'CalleeVar'),
    'include MACROS from #JOINT# should conflict via virtual paste'
);
assert(
    !sectionIncludeConflicts.some((c) =>
        c.varName === 'CallerOther' && c.otherVarName === 'CalleeVar'),
    'CalleeVar in MACROS should not conflict with CallerOther in #OTHER#'
);

const sectionIncludeCalleeScopes = getDeclarationConflictScopes(sectionIncludeCallee, index);
const calleeOnlyConflicts = findCrossFileDeclarationConflicts(sectionIncludeCalleeScopes[0].files, index);
assert(
    !calleeOnlyConflicts.some((c) =>
        (c.varName === 'CalleeVar' && c.otherVarName === 'OtherVar') ||
        (c.varName === 'OtherVar' && c.otherVarName === 'CalleeVar')),
    'different sections within callee file should not conflict with each other'
);

const callerSameSlot = normalizeFsPath(path.join(customRoot, 'caller_same_slot.prg'));
const calleeSameSlot = normalizeFsPath(path.join(customRoot, 'callee_same_slot.prg'));
const sameSlotScopes = getDeclarationConflictScopes(callerSameSlot, index);
const sameSlotConflicts = findCrossFileDeclarationConflicts(sameSlotScopes[0].files, index);
assert(
    sameSlotConflicts.length === 0,
    'same varName and declaration number across call-linked files should not conflict'
);
assert(
    commonConflicts.length > 0,
    'different varNames with same number should still conflict (SharedA vs SharedB)'
);

console.log('workspace-declaration-index smoke test passed');
