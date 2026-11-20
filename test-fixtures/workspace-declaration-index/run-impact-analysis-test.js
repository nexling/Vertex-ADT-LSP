'use strict';

const fs = require('fs');
const path = require('path');

const fixtureRoot = __dirname;
const customRoot = path.join(fixtureRoot, 'custom');

function normalizeFsPath(filePath) {
    return path.normalize(filePath).replace(/\\/g, '/').toLowerCase();
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

function stripPathQuotes(value) {
    const trimmed = value.trim();
    if ((trimmed.startsWith('"') && trimmed.endsWith('"')) ||
        (trimmed.startsWith("'") && trimmed.endsWith("'"))) {
        return trimmed.slice(1, -1);
    }
    return trimmed;
}

function parseIncludeCallPathAndSection(rawPath, lineRemainder) {
    let pathPart = stripPathQuotes(rawPath).replace(/\\/g, '/').trim();
    let targetSection = null;

    const pathSectionMatch = pathPart.match(/^(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$/);
    if (pathSectionMatch) {
        const token = pathSectionMatch[2];
        if (!['section', 'sectionoffile'].includes(token.toLowerCase())) {
            targetSection = token;
            pathPart = pathSectionMatch[1].trim();
        }
    }

    if (lineRemainder && lineRemainder.trim()) {
        const remMatch = lineRemainder.trim().match(/^([A-Za-z_][A-Za-z0-9_]*)\b/);
        if (remMatch && !['section', 'sectionoffile'].includes(remMatch[1].toLowerCase())) {
            targetSection = remMatch[1];
        }
    }

    return { filePath: pathPart, targetSection };
}

function resolveFixturePath(inputPath) {
    const customCandidate = path.join(customRoot, inputPath);
    if (fs.existsSync(customCandidate)) {
        return customCandidate;
    }
    return null;
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
        const rawLine = contentLines[i];
        const trimmed = rawLine.trim();
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
                        line: i,
                        lineText: rawLine,
                    });
                }
            }
            continue;
        }

        const callPatterns = [
            /call\s*\(\s*["']([^"']+)["']/gi,
            /\bcall\s+([^\s,(]+)/gi,
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
                            line: i,
                            lineText: rawLine,
                        });
                    }
                }
            }
        }
    }

    return { includeLinks, callLinks, commonIndexes, commonPoints };
}

function buildFixtureIndex(fileNames) {
    const index = new Map();
    for (const fileName of fileNames) {
        const absPath = path.join(customRoot, fileName);
        const content = fs.readFileSync(absPath, 'utf8');
        const deps = extractFileDependencies(content, resolveFixturePath);
        index.set(normalizeFsPath(absPath), {
            relativePath: `custom/${fileName}`,
            includeLinks: deps.includeLinks,
            callLinks: deps.callLinks,
            commonIndexes: deps.commonIndexes,
            commonPoints: deps.commonPoints,
        });
    }
    return index;
}

function getForwardNamespaceClosure(rootPath, index) {
    const group = new Set();
    const queue = [normalizeFsPath(rootPath)];

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

function getIncomingReferences(targetPath, index) {
    const references = [];
    const normalizedTarget = normalizeFsPath(targetPath);
    index.forEach((otherEntry, otherPath) => {
        for (const link of otherEntry.includeLinks || []) {
            if (link.targetPath === normalizedTarget) {
                references.push({ fromPath: otherPath, type: 'include', line: link.line });
            }
        }
        if (otherEntry.commonIndexes || otherEntry.commonPoints) {
            for (const link of otherEntry.callLinks || []) {
                if (link.targetPath === normalizedTarget) {
                    references.push({ fromPath: otherPath, type: 'call', line: link.line });
                }
            }
        }
    });
    return references;
}

function analyzeFileImpact(targetPath, index) {
    const references = getIncomingReferences(targetPath, index);
    const scopes = [];
    const seenClosures = [];
    const uniqueFromPaths = new Set(references.map((ref) => ref.fromPath));

    uniqueFromPaths.forEach((fromPath) => {
        const roots = getUltimateNamespaceRoots(fromPath, index);
        roots.forEach((rootPath) => {
            const closure = getForwardNamespaceClosure(rootPath, index);
            for (const seen of seenClosures) {
                if (namespaceClosuresEqual(seen, closure)) {
                    return;
                }
            }
            seenClosures.push(closure);
            scopes.push({ rootPath, fileCount: closure.size });
        });
    });

    const blastRadiusFileSet = new Set();
    for (const scope of scopes) {
        const closure = getForwardNamespaceClosure(scope.rootPath, index);
        closure.forEach((filePath) => blastRadiusFileSet.add(filePath));
    }

    let calls = 0;
    let includes = 0;
    for (const ref of references) {
        if (ref.type === 'call') {
            calls++;
        } else {
            includes++;
        }
    }

    return {
        incoming: { calls, includes, references },
        scopes,
        totalBlastRadiusFiles: blastRadiusFileSet.size,
    };
}

function shouldShowImpactCodeLens(targetPath, index, minIncomingCount) {
    return getIncomingReferences(targetPath, index).length >= minIncomingCount;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const index = buildFixtureIndex([
    'caller_a.prg',
    'caller_b.prg',
    'shared_callee.prg',
    'main_include.prg',
    'sub_included.prg',
    'caller_no_common.prg',
]);

const sharedCallee = normalizeFsPath(path.join(customRoot, 'shared_callee.prg'));
const subIncluded = normalizeFsPath(path.join(customRoot, 'sub_included.prg'));
const callerA = normalizeFsPath(path.join(customRoot, 'caller_a.prg'));
const callerB = normalizeFsPath(path.join(customRoot, 'caller_b.prg'));
const callerNoCommon = normalizeFsPath(path.join(customRoot, 'caller_no_common.prg'));

const sharedAnalysis = analyzeFileImpact(sharedCallee, index);
assert(sharedAnalysis.incoming.calls === 2, 'shared_callee should have 2 incoming calls');
assert(sharedAnalysis.incoming.includes === 0, 'shared_callee should have no incoming includes');
assert(sharedAnalysis.scopes.length === 2, 'shared_callee should have two isolated blast-radius scopes');
assert(shouldShowImpactCodeLens(sharedCallee, index, 2), 'shared_callee should meet default threshold');
assert(!shouldShowImpactCodeLens(sharedCallee, index, 3), 'shared_callee should be below threshold of 3');

const incomingPrograms = new Set(sharedAnalysis.incoming.references.map((ref) => ref.fromPath));
assert(incomingPrograms.has(callerA) && incomingPrograms.has(callerB),
    'shared_callee incoming references should include caller_a and caller_b');

const includeAnalysis = analyzeFileImpact(subIncluded, index);
assert(includeAnalysis.incoming.includes === 1, 'sub_included should have one incoming include');
assert(!shouldShowImpactCodeLens(subIncluded, index, 2), 'sub_included should not meet threshold of 2');
assert(shouldShowImpactCodeLens(subIncluded, index, 1), 'sub_included should meet threshold of 1');

const leafAnalysis = analyzeFileImpact(callerNoCommon, index);
assert(leafAnalysis.incoming.calls === 0 && leafAnalysis.incoming.includes === 0,
    'unreferenced leaf should have no incoming references');

console.log('impact analysis smoke test passed');
