'use strict';

const fs = require('fs');
const path = require('path');

const fixtureRoot = __dirname;
const customRoot = path.join(fixtureRoot, 'custom');

function normalizeFsPath(filePath) {
    return path.normalize(filePath).replace(/\\/g, '/').toLowerCase();
}

function stripPathQuotes(value) {
    const trimmed = value.trim();
    if ((trimmed.startsWith('"') && trimmed.endsWith('"')) ||
        (trimmed.startsWith("'") && trimmed.endsWith("'"))) {
        return trimmed.slice(1, -1);
    }
    return trimmed;
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

function resolveFixturePath(inputPath) {
    const baseName = path.basename(stripPathQuotes(inputPath));
    const customCandidate = path.join(customRoot, baseName);
    if (fs.existsSync(customCandidate)) {
        return normalizeFsPath(customCandidate);
    }
    return null;
}

function isInsideQuotes(text, index) {
    let inQuote = false;
    for (let i = 0; i < index; i++) {
        if (text[i] === '"') {
            inQuote = !inQuote;
        }
    }
    return inQuote;
}

function findMatchingParenIndex(text, openParenIndex) {
    if (!text || openParenIndex < 0 || text[openParenIndex] !== '(') {
        return -1;
    }
    let parenCount = 1;
    let activeQuote = null;
    for (let i = openParenIndex + 1; i < text.length; i++) {
        const ch = text[i];
        if (activeQuote) {
            if (ch === activeQuote) {
                activeQuote = null;
            }
            continue;
        }
        if (ch === '"') {
            activeQuote = ch;
            continue;
        }
        if (ch === '(') {
            parenCount++;
        } else if (ch === ')') {
            parenCount--;
            if (parenCount === 0) {
                return i;
            }
        }
    }
    return -1;
}

function parseParameters(paramString) {
    const params = [];
    let current = '';
    let inQuote = false;
    for (let i = 0; i < paramString.length; i++) {
        const ch = paramString[i];
        if (ch === '"') {
            inQuote = !inQuote;
            current += ch;
            continue;
        }
        if (ch === ',' && !inQuote) {
            params.push(current.trim());
            current = '';
            continue;
        }
        current += ch;
    }
    if (current.trim()) {
        params.push(current.trim());
    }
    return params;
}

function parseCommand(command) {
    command = command.trim();
    const assignmentMatch = command.match(/^([A-Za-z][A-Za-z0-9_]*\$?)\s*=\s*(.+)$/);
    let commandPart = command;
    if (assignmentMatch) {
        commandPart = assignmentMatch[2];
    }
    const funcMatch = commandPart.match(/^(\w+)\s*\(/);
    if (!funcMatch) {
        return null;
    }
    const functionName = funcMatch[1];
    const startPos = funcMatch[0].length - 1;
    const insideParensEnd = findMatchingParenIndex(commandPart, startPos);
    if (insideParensEnd === -1) {
        return null;
    }
    const insideParens = commandPart.substring(startPos + 1, insideParensEnd);
    return {
        functionName,
        insideParams: parseParameters(insideParens),
    };
}

function isValidCallPathParameter(expr) {
    const trimmed = (expr || '').trim();
    return (trimmed.startsWith('"') && trimmed.endsWith('"')) ||
        (trimmed.startsWith("'") && trimmed.endsWith("'"));
}

function extractFunctionParameters(fileContent) {
    const parameters = [];
    const lines = fileContent.split('\n');
    const regex = /(\w+)\$?\s*=\s*get\(\s*\$(\w+)\s*\)/g;
    for (const line of lines) {
        let match;
        while ((match = regex.exec(line)) !== null) {
            const variableName = match[0].split('=')[0].trim();
            parameters.push({ variableName, parameterName: match[2].trim() });
        }
    }
    return parameters;
}

function getExpectedCallParamCount(fileContent) {
    const parameters = extractFunctionParameters(fileContent);
    let maxIndex = 0;
    for (const param of parameters) {
        const paramMatch = /^CALLPARAM(\d+)$/i.exec(param.parameterName || '');
        if (paramMatch) {
            maxIndex = Math.max(maxIndex, parseInt(paramMatch[1], 10));
        }
    }
    return maxIndex;
}

function appendResolvedCallTarget(targets, rawPathToken, lineRemainder) {
    const parsed = parseIncludeCallPathAndSection(rawPathToken.trim(), lineRemainder || '');
    const resolved = resolveFixturePath(parsed.filePath);
    if (!resolved) {
        return;
    }
    targets.push({ rawPath: rawPathToken.trim(), resolvedPath: resolved });
}

function analyzeCallParamSitesOnLine(line, lineIndex, expectedByPath) {
    const sites = [];
    const commandRegex = /\bcall\s*\(/ig;
    let match;
    while ((match = commandRegex.exec(line)) !== null) {
        if (isInsideQuotes(line, match.index)) {
            continue;
        }
        const openParenIndex = match.index + match[0].length - 1;
        const closeParenIndex = findMatchingParenIndex(line, openParenIndex);
        if (closeParenIndex === -1) {
            continue;
        }
        let startIndex = match.index;
        const prefix = line.substring(0, match.index);
        const assignmentMatch = prefix.match(/([A-Za-z][A-Za-z0-9_]*\$?)\s*=\s*$/);
        if (assignmentMatch) {
            startIndex = prefix.length - assignmentMatch[0].length;
        }
        const segment = line.substring(startIndex, closeParenIndex + 1).trim();
        const parsed = parseCommand(segment);
        if (!parsed ||
            parsed.functionName.toLowerCase() !== 'call' ||
            parsed.insideParams.length < 1 ||
            !isValidCallPathParameter(parsed.insideParams[0])) {
            continue;
        }
        const targets = [];
        const remainder = parsed.insideParams.length > 1
            ? parsed.insideParams.slice(1).join(',')
            : '';
        appendResolvedCallTarget(targets, parsed.insideParams[0], remainder);
        if (targets.length === 0) {
            continue;
        }
        const argCount = parsed.insideParams.length - 1;
        const expected = expectedByPath.get(targets[0].resolvedPath) || 0;
        if (expected <= 0) {
            continue;
        }
        sites.push({
            lineIndex,
            argCount,
            expected,
            missingCount: Math.max(0, expected - argCount),
            extraCount: Math.max(0, argCount - expected),
            openParenIndex,
            closeParenIndex,
        });
    }
    return sites;
}

function buildMissingCallParamInsertText(missingCount) {
    return missingCount > 0 ? ',""'.repeat(missingCount) : '';
}

function findCallParamRangesInParen(line, openParenIndex, closeParenIndex) {
    const content = line.substring(openParenIndex + 1, closeParenIndex);
    const ranges = [];
    let i = 0;

    while (i < content.length) {
        while (i < content.length && (content[i] === ' ' || content[i] === ',')) {
            i++;
        }
        if (i >= content.length) {
            break;
        }

        const paramStart = openParenIndex + 1 + i;
        let inQuotes = false;
        let parenDepth = 0;
        while (i < content.length) {
            const ch = content[i];
            if (ch === '"' && parenDepth === 0) {
                inQuotes = !inQuotes;
            } else if (!inQuotes) {
                if (ch === '(') {
                    parenDepth++;
                } else if (ch === ')') {
                    if (parenDepth > 0) {
                        parenDepth--;
                    }
                } else if (ch === ',' && parenDepth === 0) {
                    break;
                }
            }
            i++;
        }

        ranges.push({
            start: paramStart,
            end: openParenIndex + 1 + i,
        });
    }

    return ranges;
}

function buildExtraCallParamRemovalRange(line, openParenIndex, closeParenIndex, expected, paramRanges) {
    const firstExtraIndex = expected + 1;
    if (!paramRanges || paramRanges.length <= firstExtraIndex) {
        return null;
    }

    let deleteStart = paramRanges[firstExtraIndex].start;
    while (deleteStart > openParenIndex + 1 && /\s/.test(line[deleteStart - 1])) {
        deleteStart--;
    }
    if (deleteStart > openParenIndex + 1 && line[deleteStart - 1] === ',') {
        deleteStart--;
    }
    while (deleteStart > openParenIndex + 1 && /\s/.test(line[deleteStart - 1])) {
        deleteStart--;
    }

    return {
        start: deleteStart,
        end: closeParenIndex,
    };
}

function collectCallParamIssues(filePath, expectedByPath) {
    const content = fs.readFileSync(filePath, 'utf8');
    const lines = content.split('\n');
    const issues = [];
    for (let i = 0; i < lines.length; i++) {
        const sites = analyzeCallParamSitesOnLine(lines[i], i, expectedByPath);
        for (const site of sites) {
            if (site.missingCount > 0) {
                issues.push({ type: 'missing', ...site });
            } else if (site.extraCount > 0) {
                issues.push({ type: 'extra', ...site });
            }
        }
    }
    return issues;
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

const calleePath = normalizeFsPath(path.join(customRoot, 'callee_three_params.prg'));
const calleeContent = fs.readFileSync(path.join(customRoot, 'callee_three_params.prg'), 'utf8');
const expectedByPath = new Map([[calleePath, getExpectedCallParamCount(calleeContent)]]);
assert(expectedByPath.get(calleePath) === 3, 'callee should expect 3 call parameters');

const missingIssues = collectCallParamIssues(path.join(customRoot, 'caller_missing_one.prg'), expectedByPath);
assert(missingIssues.length === 1 && missingIssues[0].type === 'missing', 'caller_missing_one should have one missing issue');
assert(missingIssues[0].missingCount === 1, 'caller_missing_one should miss one parameter');

const okIssues = collectCallParamIssues(path.join(customRoot, 'caller_ok.prg'), expectedByPath);
assert(okIssues.length === 0, 'caller_ok should have no call param issues');

const extraIssues = collectCallParamIssues(path.join(customRoot, 'caller_extra_one.prg'), expectedByPath);
assert(extraIssues.length === 1 && extraIssues[0].type === 'extra', 'caller_extra_one should have one extra issue');

const fixedLine = 'ret$=call("callee_three_params.prg",Area,Selemobj)';
const closeIdx = fixedLine.lastIndexOf(')');
const patched = fixedLine.slice(0, closeIdx) + buildMissingCallParamInsertText(1) + fixedLine.slice(closeIdx);
assert(
    patched === 'ret$=call("callee_three_params.prg",Area,Selemobj,"")',
    `insert builder failed: ${patched}`
);

const extraLine = fs.readFileSync(path.join(customRoot, 'caller_extra_one.prg'), 'utf8').split('\n')[0].replace(/\r$/, '');
const extraSites = analyzeCallParamSitesOnLine(extraLine, 0, expectedByPath);
assert(extraSites.length === 1 && extraSites[0].extraCount === 1, 'extra site should be detected');
const extraRanges = findCallParamRangesInParen(extraLine, extraSites[0].openParenIndex, extraSites[0].closeParenIndex);
const extraRemoval = buildExtraCallParamRemovalRange(
    extraLine,
    extraSites[0].openParenIndex,
    extraSites[0].closeParenIndex,
    extraSites[0].expected,
    extraRanges
);
assert(extraRemoval, 'extra removal range should be computed');
const trimmedExtra = extraLine.slice(0, extraRemoval.start) + extraLine.slice(extraRemoval.end);
assert(
    trimmedExtra === 'ret$=call("callee_three_params.prg",Area,Selemobj,"1")',
    `extra removal failed: ${trimmedExtra}`
);

console.log('call-param-validation smoke test passed');
