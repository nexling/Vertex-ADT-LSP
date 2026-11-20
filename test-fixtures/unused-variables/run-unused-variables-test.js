'use strict';

const fs = require('fs');
const path = require('path');

const fixtureRoot = __dirname;
const customRoot = path.join(fixtureRoot, 'custom');
const keywordLabelsSet = new Set();
const variablePattern1 = /\b[A-Za-z][A-Za-z0-9_-]*\b(?=\s*=)/g;
const variablePattern2 = /\b[A-Za-z]*\b(?!$)(?=\s*=)/g;

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
    if (pathSectionMatch && !['section', 'sectionoffile'].includes(pathSectionMatch[2].toLowerCase())) {
        targetSection = pathSectionMatch[2];
        pathPart = pathSectionMatch[1].trim();
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
                        });
                    }
                }
            }
        }
    }

    return { includeLinks, callLinks, commonIndexes, commonPoints };
}

function buildIndex(fileNames) {
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

function extractVariableReferencesFromExpression(expression) {
    if (!expression) {
        return [];
    }
    let sanitizedExpression = '';
    let activeQuote = null;
    for (const char of expression) {
        if (activeQuote) {
            if (char === activeQuote) {
                activeQuote = null;
            }
            sanitizedExpression += ' ';
            continue;
        }
        if (char === '"') {
            activeQuote = char;
            sanitizedExpression += ' ';
            continue;
        }
        sanitizedExpression += char;
    }
    const references = new Set();
    const variablePattern = /[A-Za-z][A-Za-z0-9_]*\$?/g;
    let match;
    while ((match = variablePattern.exec(sanitizedExpression)) !== null) {
        references.add(match[0]);
    }
    return [...references];
}

function getCallCommandVariableReferences(line) {
    const callMatch = line.match(/call\s*\(([^)]*)\)/i);
    if (!callMatch) {
        return [];
    }
    const inner = callMatch[1];
    const parts = [];
    let current = '';
    let inQuote = false;
    for (let i = 0; i < inner.length; i++) {
        const char = inner[i];
        if (char === '"') {
            inQuote = !inQuote;
            current += char;
            continue;
        }
        if (char === ',' && !inQuote) {
            parts.push(current.trim());
            current = '';
            continue;
        }
        current += char;
    }
    if (current.trim()) {
        parts.push(current.trim());
    }
    const references = [];
    for (let i = 1; i < parts.length; i++) {
        const variableName = parts[i].replace(/^["']|["']$/g, '');
        if (/^[A-Za-z]/.test(variableName)) {
            references.push(variableName);
        }
    }
    return references;
}

function stripValidationComment(line) {
    if (!line) {
        return '';
    }
    return line.includes(';!') ? line.split(';!')[0] : line;
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

function advancePendingIfHeaderState(trimmedLine, currentPendingIfHeader) {
    if (/^\s*endif\b/i.test(trimmedLine) || /^\s*else\b/i.test(trimmedLine)) {
        return false;
    }
    const hasIf = /^\s*if\b/i.test(trimmedLine);
    const hasThen = /\bthen\b/i.test(trimmedLine);
    if (hasIf && !hasThen) {
        return true;
    }
    if (hasThen && currentPendingIfHeader) {
        return false;
    }
    if (currentPendingIfHeader) {
        if (/^\s*(and|or)\b/i.test(trimmedLine) || /^\s*(&&|\|\|)/.test(trimmedLine)) {
            return true;
        }
        return false;
    }
    return false;
}

function collectIfConditionReferences(line, isPendingIfHeader) {
    const references = new Set();
    const trimmed = stripValidationComment(line).trim();
    if (!trimmed) {
        return references;
    }
    const ifCondition = extractIfConditionFromLine(line);
    if (ifCondition) {
        for (const variableName of extractVariableReferencesFromExpression(ifCondition)) {
            references.add(variableName);
        }
        return references;
    }
    if (/^\s*if\b/i.test(trimmed) && !/\bthen\b/i.test(trimmed)) {
        const afterIfMatch = trimmed.match(/^\s*if\s+/i);
        if (afterIfMatch) {
            for (const variableName of extractVariableReferencesFromExpression(trimmed.slice(afterIfMatch[0].length))) {
                references.add(variableName);
            }
        }
        return references;
    }
    if (isPendingIfHeader && (/^\s*(and|or)\b/i.test(trimmed) || /^\s*(&&|\|\|)/.test(trimmed))) {
        for (const variableName of extractVariableReferencesFromExpression(trimmed)) {
            references.add(variableName);
        }
    }
    return references;
}

function makeUnusedVariableKey(sectionName, varName) {
    return `${sectionName || ''}::${varName}`;
}

function getIfConditionTextSpans(splitline, line, isPendingIfHeader) {
    const spans = [];
    const trimmed = stripValidationComment(line).trim();
    if (!trimmed) {
        return spans;
    }
    const ifCondition = extractIfConditionFromLine(line);
    if (ifCondition) {
        const conditionIndex = splitline.indexOf(ifCondition);
        if (conditionIndex !== -1) {
            spans.push({ start: conditionIndex, end: conditionIndex + ifCondition.length });
        }
        return spans;
    }
    if (/^\s*if\b/i.test(trimmed) && !/\bthen\b/i.test(trimmed)) {
        const afterIfMatch = trimmed.match(/^\s*if\s+/i);
        if (afterIfMatch) {
            const afterIf = trimmed.slice(afterIfMatch[0].length);
            const afterIfIndex = splitline.indexOf(afterIf);
            if (afterIfIndex !== -1) {
                spans.push({ start: afterIfIndex, end: afterIfIndex + afterIf.length });
            }
        }
        return spans;
    }
    if (isPendingIfHeader && (/^\s*(and|or)\b/i.test(trimmed) || /^\s*(&&|\|\|)/.test(trimmed))) {
        spans.push({ start: 0, end: splitline.length });
    }
    return spans;
}

function isIndexInsideIfConditionSpans(index, spans) {
    for (let i = 0; i < spans.length; i++) {
        if (index >= spans[i].start && index < spans[i].end) {
            return true;
        }
    }
    return false;
}

function parseDeclarationVariablesFromSplitLine(splitline, line, isPendingIfHeader) {
    const names = new Set();
    if (!splitline || splitline.trim().startsWith('!')) {
        return names;
    }
    const conditionSpans = getIfConditionTextSpans(splitline, line, isPendingIfHeader);
    const assignmentPattern = /\b[A-Za-z][A-Za-z0-9_-]*\b(?=\s*=(?!=))/g;
    let match;
    while ((match = assignmentPattern.exec(splitline)) !== null) {
        const varName = match[0];
        if (keywordLabelsSet.has(varName)) {
            continue;
        }
        if (isIndexInsideIfConditionSpans(match.index, conditionSpans)) {
            continue;
        }
        names.add(varName);
    }
    return names;
}

function stripCallSegmentsForUnusedReferenceScan(codeLine) {
    return codeLine.replace(/call\s*\([^)]*\)/gi, 'call()');
}

function collectReferencesFromValidationLine(codeLine, line, isPendingIfHeader) {
    const references = new Set(extractVariableReferencesFromExpression(
        stripCallSegmentsForUnusedReferenceScan(codeLine)
    ));
    const ifConditionRefs = collectIfConditionReferences(line, !!isPendingIfHeader);
    ifConditionRefs.forEach((variableName) => references.add(variableName));
    return references;
}

function loadLinkedFileText(targetPath, fileTextByPath) {
    return fileTextByPath.get(normalizeFsPath(targetPath)) || null;
}

function scanFileForVariableReferences(fileContent, targetNames, fileSections, options) {
    const found = new Set();
    if (!fileContent || !targetNames || targetNames.size === 0) {
        return found;
    }
    const restrictToSection = options && options.restrictToSection !== undefined
        ? options.restrictToSection
        : null;
    const lines = fileContent.split('\n');
    const sections = fileSections || parseFileSections(lines);
    let pendingIfHeader = false;
    for (let i = 0; i < lines.length; i++) {
        const trimmed = lines[i].trim();
        if (!trimmed || trimmed.startsWith('!') || trimmed.startsWith('##')) {
            continue;
        }
        if (restrictToSection !== null && getSectionForLine(i, sections) !== restrictToSection) {
            continue;
        }
        let line = lines[i];
        if (line.includes(';!')) {
            line = line.split(';!')[0];
        }
        const trimmedLine = stripValidationComment(line).trim();
        const refs = collectReferencesFromValidationLine(line, line, pendingIfHeader);
        refs.forEach((variableName) => {
            if (targetNames.has(variableName)) {
                found.add(variableName);
            }
        });
        pendingIfHeader = advancePendingIfHeaderState(trimmedLine, pendingIfHeader);
    }
    return found;
}

function analyzeUnusedVariables(filePath, fileTextByPath, index) {
    const normalizedPath = normalizeFsPath(filePath);
    const lines = fileTextByPath.get(normalizedPath).split('\n');
    const fileSections = parseFileSections(lines);
    const declarations = new Map();
    const usageCounts = new Map();

    function ensureDeclaration(sectionName, varName, lineIndex) {
        const key = makeUnusedVariableKey(sectionName, varName);
        if (!declarations.has(key)) {
            declarations.set(key, { sectionName: sectionName || '', varName, lineIndex });
            usageCounts.set(key, 0);
        }
    }

    function incrementUsage(sectionName, varName) {
        const key = makeUnusedVariableKey(sectionName, varName);
        if (declarations.has(key)) {
            usageCounts.set(key, (usageCounts.get(key) || 0) + 1);
        }
    }

    let pendingIfHeader = false;

    for (let i = 0; i < lines.length; i++) {
        let line = lines[i];
        const trimmed = line.trim();
        if (!trimmed || trimmed.startsWith('!') || (trimmed.startsWith('##') && !trimmed.startsWith('##included_from'))) {
            continue;
        }
        if (line.includes(';!')) {
            line = line.split(';!')[0];
        }
        const sectionName = getSectionForLine(i, fileSections);
        const trimmedLine = stripValidationComment(line).trim();
        const splitlines = line.split(';');
        const declarationNamesOnLine = new Set();
        for (const splitline of splitlines) {
            const declarationVars = parseDeclarationVariablesFromSplitLine(splitline, line, pendingIfHeader);
            declarationVars.forEach((varName) => {
                ensureDeclaration(sectionName, varName, i);
                declarationNamesOnLine.add(varName);
            });
        }
        const refs = collectReferencesFromValidationLine(line, line, pendingIfHeader);
        declarationNamesOnLine.forEach((declName) => refs.delete(declName));
        refs.forEach((varName) => incrementUsage(sectionName, varName));
        pendingIfHeader = advancePendingIfHeaderState(trimmedLine, pendingIfHeader);
    }

    const entry = index.get(normalizedPath);
    if (entry) {
        for (const link of entry.includeLinks || []) {
            const sourceSection = link.sourceSection || '';
            const targetText = loadLinkedFileText(link.targetPath, fileTextByPath);
            if (!targetText) {
                continue;
            }
            const targetSections = parseFileSections(targetText.split('\n'));
            const scanOptions = link.targetSection ? { restrictToSection: link.targetSection } : undefined;
            declarations.forEach((decl, key) => {
                if (decl.sectionName !== sourceSection) {
                    return;
                }
                const found = scanFileForVariableReferences(
                    targetText,
                    new Set([decl.varName]),
                    targetSections,
                    scanOptions
                );
                if (found.has(decl.varName)) {
                    usageCounts.set(key, (usageCounts.get(key) || 0) + 1);
                }
            });
        }

        if (entry.commonIndexes || entry.commonPoints) {
            for (const link of entry.callLinks || []) {
                const sourceSection = link.sourceSection || '';
                const callLine = lines[link.line];
                if (!callLine) {
                    continue;
                }
                let line = callLine;
                if (line.includes(';!')) {
                    line = line.split(';!')[0];
                }
                const callArgs = getCallCommandVariableReferences(line);
                const declaredArgs = callArgs.filter((argName) =>
                    declarations.has(makeUnusedVariableKey(sourceSection, argName))
                );
                const targetText = loadLinkedFileText(link.targetPath, fileTextByPath);
                if (!targetText || declaredArgs.length === 0) {
                    continue;
                }
                const foundInCallee = scanFileForVariableReferences(
                    targetText,
                    new Set(declaredArgs),
                    parseFileSections(targetText.split('\n'))
                );
                for (const argName of declaredArgs) {
                    if (foundInCallee.has(argName)) {
                        incrementUsage(sourceSection, argName);
                    }
                }
            }
        }
    }

    const unused = [];
    declarations.forEach((decl, key) => {
        if ((usageCounts.get(key) || 0) === 0) {
            unused.push(decl);
        }
    });
    return unused;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const fileNames = [
    'local_unused.prg',
    'local_used.prg',
    'caller_passes_arg.prg',
    'callee_uses_arg.prg',
    'caller_unused_arg.prg',
    'callee_no_arg.prg',
    'include_uses_var.prg',
    'included_body.prg',
    'section_isolated.prg',
    'if_condition_use.prg',
    'if_condition_spaced.prg',
    'if_condition_multiline.prg',
    'if_condition_single_eq.prg',
];

const index = buildIndex(fileNames);
const fileTextByPath = new Map();
for (const fileName of fileNames) {
    const absPath = normalizeFsPath(path.join(customRoot, fileName));
    fileTextByPath.set(absPath, fs.readFileSync(path.join(customRoot, fileName), 'utf8'));
}

const localUnused = analyzeUnusedVariables(path.join(customRoot, 'local_unused.prg'), fileTextByPath, index);
assert(localUnused.length === 2, `local_unused expected 2 unused, got ${localUnused.length}`);
assert(localUnused.some((d) => d.varName === 'foo'), 'local_unused should flag foo');
assert(localUnused.some((d) => d.varName === 'bar'), 'local_unused should flag bar');

const localUsed = analyzeUnusedVariables(path.join(customRoot, 'local_used.prg'), fileTextByPath, index);
assert(localUsed.length === 1 && localUsed[0].varName === 'bar',
    `local_used expected only bar unused, got ${localUsed.map((d) => d.varName).join(',')}`);
assert(!localUsed.some((d) => d.varName === 'foo'), 'local_used should not flag foo');

const callerPasses = analyzeUnusedVariables(path.join(customRoot, 'caller_passes_arg.prg'), fileTextByPath, index);
assert(!callerPasses.some((d) => d.varName === 'arg1'),
    `caller_passes_arg should not flag arg1; got ${callerPasses.map((d) => d.varName).join(',')}`);

const callerUnused = analyzeUnusedVariables(path.join(customRoot, 'caller_unused_arg.prg'), fileTextByPath, index);
assert(callerUnused.some((d) => d.varName === 'arg1'),
    `caller_unused_arg should flag arg1; got ${callerUnused.map((d) => d.varName).join(',')}`);

const includeUses = analyzeUnusedVariables(path.join(customRoot, 'include_uses_var.prg'), fileTextByPath, index);
assert(includeUses.length === 0, `include_uses_var expected 0 unused, got ${includeUses.map((d) => d.varName).join(',')}`);

const sectionIsolated = analyzeUnusedVariables(path.join(customRoot, 'section_isolated.prg'), fileTextByPath, index);
const unusedDupMacros2 = sectionIsolated.find((d) => d.sectionName === 'MACROS2' && d.varName === 'dup');
assert(unusedDupMacros2, 'section_isolated should flag dup in MACROS2');
assert(!sectionIsolated.some((d) => d.sectionName === 'MACROS' && d.varName === 'dup'),
    'section_isolated should not flag dup in MACROS');

const ifConditionUse = analyzeUnusedVariables(path.join(customRoot, 'if_condition_use.prg'), fileTextByPath, index);
assert(!ifConditionUse.some((d) => d.varName === 'I'),
    `if_condition_use should not flag I; got ${ifConditionUse.map((d) => d.varName).join(',')}`);

const ifConditionSpaced = analyzeUnusedVariables(path.join(customRoot, 'if_condition_spaced.prg'), fileTextByPath, index);
assert(!ifConditionSpaced.some((d) => d.varName === 'I'),
    `if_condition_spaced should not flag I; got ${ifConditionSpaced.map((d) => d.varName).join(',')}`);

const ifConditionMultiline = analyzeUnusedVariables(path.join(customRoot, 'if_condition_multiline.prg'), fileTextByPath, index);
assert(!ifConditionMultiline.some((d) => d.varName === 'I'),
    `if_condition_multiline should not flag I; got ${ifConditionMultiline.map((d) => d.varName).join(',')}`);

const ifConditionSingleEq = analyzeUnusedVariables(path.join(customRoot, 'if_condition_single_eq.prg'), fileTextByPath, index);
assert(!ifConditionSingleEq.some((d) => d.varName === 'flatblock'),
    `if_condition_single_eq should not flag flatblock; got ${ifConditionSingleEq.map((d) => d.varName).join(',')}`);

console.log('unused-variables smoke test passed');
