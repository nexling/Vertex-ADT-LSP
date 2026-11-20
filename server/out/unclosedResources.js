"use strict";

const path = require('path');

const UNCLOSED_RESOURCE_CODE = 'vertexadt/unclosed-resource';
const UNCLOSED_ALREADY_OPEN_CODE = 'vertexadt/unclosed-already-open';
const IGNORE_CLOSE_PATTERN = /\bIGNORE\s+CLOSE\b/i;
const RESOURCE_COMMAND_PATTERN = /\b(dbcreate|dbclose_by_name|dbopen|dbclose|open|close)\s*\(/gi;
const INCLUDE_CALL_SECTION_KEYWORDS = new Set(['section', 'sectionoffile']);
const VERIFICATION_IGNORED_SECTION_NAMES = new Set(['EDIT_FORM']);

function isInsideQuotes(line, position) {
    let activeQuote = null;
    for (let i = 0; i < position; i++) {
        if (activeQuote) {
            if (line[i] === activeQuote) {
                activeQuote = null;
            }
            continue;
        }
        if (line[i] === '"' || line[i] === "'") {
            activeQuote = line[i];
        }
    }
    return activeQuote !== null;
}

function findBangCommentIndex(line) {
    if (!line) {
        return -1;
    }
    for (let i = 0; i < line.length - 1; i++) {
        if (line[i] === ';' && line[i + 1] === '!' && !isInsideQuotes(line, i)) {
            return i;
        }
    }
    return -1;
}

function stripBangComment(line) {
    if (!line) {
        return '';
    }
    const commentIndex = findBangCommentIndex(line);
    return commentIndex !== -1 ? line.substring(0, commentIndex) : line;
}

function lineHasIgnoreClose(line) {
    const commentIndex = findBangCommentIndex(line);
    if (commentIndex === -1) {
        return false;
    }
    return IGNORE_CLOSE_PATTERN.test(line.substring(commentIndex));
}

function buildIgnoreCloseInsertion(lineText) {
    if (lineHasIgnoreClose(lineText)) {
        return null;
    }
    const withoutCr = String(lineText || '').replace(/\r$/, '');
    if (findBangCommentIndex(withoutCr) !== -1) {
        return { character: withoutCr.length, newText: ' IGNORE CLOSE' };
    }
    return { character: withoutCr.length, newText: ' ;!IGNORE CLOSE' };
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
        if (ch === '"' || ch === "'") {
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
    let quoteChar = null;
    const text = paramString || '';
    for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (quoteChar) {
            current += ch;
            if (ch === quoteChar) {
                quoteChar = null;
            }
            continue;
        }
        if (ch === '"' || ch === "'") {
            quoteChar = ch;
            current += ch;
            continue;
        }
        if (ch === ',') {
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

function stripPathQuotes(value) {
    const trimmed = String(value || '').trim();
    if ((trimmed.startsWith('"') && trimmed.endsWith('"')) ||
        (trimmed.startsWith("'") && trimmed.endsWith("'"))) {
        return trimmed.slice(1, -1);
    }
    return trimmed;
}

function parseStringLiteral(value) {
    const trimmed = String(value || '').trim();
    if (trimmed.length < 2) {
        return null;
    }
    const quote = trimmed[0];
    if (quote !== '"' && quote !== "'") {
        return null;
    }
    if (trimmed[trimmed.length - 1] !== quote) {
        return null;
    }
    const inner = trimmed.slice(1, -1);
    if (inner.includes(quote)) {
        return null;
    }
    return inner;
}

function normalizeResourceName(name) {
    return stripPathQuotes(name).replace(/\\/g, '/').trim();
}

function resourceNameStem(name) {
    const normalized = normalizeResourceName(name);
    const base = path.posix.basename(normalized);
    return base.replace(/\.[^.]+$/, '');
}

function namesMatch(left, right) {
    if (!left || !right) {
        return false;
    }
    const a = normalizeResourceName(left);
    const b = normalizeResourceName(right);
    if (!a || !b) {
        return false;
    }
    if (a.toLowerCase() === b.toLowerCase()) {
        return true;
    }
    const baseA = path.posix.basename(a).toLowerCase();
    const baseB = path.posix.basename(b).toLowerCase();
    if (baseA === baseB) {
        return true;
    }
    return resourceNameStem(a).toLowerCase() === resourceNameStem(b).toLowerCase();
}

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

function parseFileSections(lines) {
    const sectionMarker = /^#([A-Za-z_][A-Za-z0-9_]*)#$/;
    const open = new Map();
    const sections = [];
    for (let i = 0; i < lines.length; i++) {
        if (!(lines[i].startsWith('#'))) {
            continue;
        }
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

function isLineInVerificationIgnoredSection(lineIndex, fileSections) {
    return VERIFICATION_IGNORED_SECTION_NAMES.has(getSectionForLine(lineIndex, fileSections));
}

function hasLineContinuation(line) {
    return stripBangComment(line).trimEnd().endsWith('\\');
}

function parseFileCommonFlags(lines) {
    let commonDbases = false;
    let commonFiles = false;
    for (let i = 0; i < (lines || []).length; i++) {
        const trimmed = stripBangComment(lines[i]).trim();
        if (/^common_dbases\s+1\b/i.test(trimmed)) {
            commonDbases = true;
        }
        if (/^common_files\s+1\b/i.test(trimmed)) {
            commonFiles = true;
        }
    }
    return { commonDbases, commonFiles };
}

function tokensMatch(left, right) {
    if (left === undefined || left === null || right === undefined || right === null) {
        return false;
    }
    return String(left).trim().toLowerCase() === String(right).trim().toLowerCase();
}

function closeMatchesOpen(closeEvent, openEvent) {
    if (!closeEvent || !openEvent || closeEvent.kind !== openEvent.kind) {
        return false;
    }
    if (closeEvent.kind === 'database' && closeEvent.nameLiteral && openEvent.nameLiteral) {
        if (namesMatch(closeEvent.nameLiteral, openEvent.nameLiteral)) {
            return true;
        }
    }
    if (closeEvent.token && openEvent.token && tokensMatch(closeEvent.token, openEvent.token)) {
        return closeEvent.ns === openEvent.ns;
    }
    return false;
}

function openHasForwardClose(events, openIndex) {
    const openEvent = events[openIndex];
    for (let i = openIndex + 1; i < events.length; i++) {
        const event = events[i];
        if (event.action === 'close' && closeMatchesOpen(event, openEvent)) {
            return true;
        }
    }
    return false;
}

function collectCallTargets(codeLine) {
    const targets = [];
    const patterns = [
        /call\s*\(\s*["']([^"']+)["']/gi,
        /call\s*\(\s*(\$[^"',)]+)/gi,
        /\bcall\s+([^\s,(]+)/gi
    ];
    for (let p = 0; p < patterns.length; p++) {
        const pattern = patterns[p];
        pattern.lastIndex = 0;
        let match;
        while ((match = pattern.exec(codeLine)) !== null) {
            if (isInsideQuotes(codeLine, match.index)) {
                continue;
            }
            targets.push({
                index: match.index,
                parsed: parseIncludeCallPathAndSection(match[1].trim(), '')
            });
        }
        if (targets.length > 0) {
            break;
        }
    }
    return targets;
}

function collectResourceEventsFromLine(rawLine, lineIndex, filePath, nsByKind, fromIncludeSite) {
    const events = [];
    const ignoreClose = lineHasIgnoreClose(rawLine);
    const codeLine = stripBangComment(rawLine);
    if (!codeLine.trim() || codeLine.trim().startsWith('!')) {
        return events;
    }

    RESOURCE_COMMAND_PATTERN.lastIndex = 0;
    let match;
    while ((match = RESOURCE_COMMAND_PATTERN.exec(codeLine)) !== null) {
        if (isInsideQuotes(codeLine, match.index)) {
            continue;
        }
        const commandName = match[1].toLowerCase();
        const openParenIndex = match.index + match[0].length - 1;
        const closeParenIndex = findMatchingParenIndex(codeLine, openParenIndex);
        if (closeParenIndex === -1) {
            continue;
        }
        const insideParams = parseParameters(codeLine.substring(openParenIndex + 1, closeParenIndex));
        const afterParams = parseParameters(codeLine.substring(closeParenIndex + 1));
        const handleToken = (insideParams[0] || '').trim();
        const firstAfterLiteral = afterParams.length > 0 ? parseStringLiteral(afterParams[0]) : null;
        const nameLiteral = commandName === 'dbclose_by_name'
            ? parseStringLiteral(insideParams[0])
            : firstAfterLiteral;
        const alreadyOpen = commandName === 'dbopen' &&
            Boolean(firstAfterLiteral) &&
            firstAfterLiteral.toLowerCase() === 'already_open';

        let action = null;
        let kind = null;
        if (commandName === 'dbcreate' || commandName === 'dbopen') {
            action = 'open';
            kind = 'database';
        } else if (commandName === 'open') {
            action = 'open';
            kind = 'file';
        } else if (commandName === 'dbclose' || commandName === 'dbclose_by_name') {
            action = 'close';
            kind = 'database';
        } else if (commandName === 'close') {
            action = 'close';
            kind = 'file';
        }
        if (!action || !kind) {
            continue;
        }

        events.push({
            action,
            kind,
            token: commandName === 'dbclose_by_name' ? '' : handleToken,
            nameLiteral: kind === 'database' ? nameLiteral : null,
            alreadyOpen: Boolean(alreadyOpen),
            ignoreClose,
            filePath,
            line: lineIndex,
            startChar: match.index,
            endChar: closeParenIndex + 1,
            lineText: rawLine,
            ns: nsByKind[kind],
            fromIncludeSite
        });
    }
    return events;
}

function walkExecutionEvents(filePath, targetSection, stack, walkState) {
    const events = [];
    const normalized = walkState.normalizePath(filePath);
    if (!normalized) {
        return events;
    }
    if (stack.has(normalized)) {
        return events;
    }

    const lines = walkState.getFileLines(normalized);
    if (!lines) {
        return events;
    }

    stack.add(normalized);
    const fileSections = parseFileSections(lines);
    let skippingDbcreateContinuation = false;

    for (let i = 0; i < lines.length; i++) {
        const rawLine = lines[i];
        const codeLine = stripBangComment(rawLine);
        const trimmed = codeLine.trim();

        if (skippingDbcreateContinuation) {
            skippingDbcreateContinuation = hasLineContinuation(rawLine);
            continue;
        }
        if (!trimmed || trimmed.startsWith('!')) {
            continue;
        }
        if (isLineInVerificationIgnoredSection(i, fileSections)) {
            continue;
        }
        if (targetSection) {
            const sectionName = getSectionForLine(i, fileSections);
            if (sectionName !== targetSection) {
                continue;
            }
        }

        if (trimmed.startsWith('# include')) {
            const afterInclude = trimmed.substring('# include'.length).trim();
            const pathMatch = afterInclude.match(/^((?:"[^"]+"|'[^']+'|[^\s#;]+))\s*(.*)$/);
            const includeStart = codeLine.indexOf('# include');
            const includeSite = walkState.fromIncludeSite || {
                filePath: normalized,
                line: i,
                startChar: includeStart < 0 ? 0 : includeStart,
                endChar: codeLine.length,
                ignoreClose: lineHasIgnoreClose(rawLine),
                lineText: rawLine
            };
            if (pathMatch) {
                const parsed = parseIncludeCallPathAndSection(pathMatch[1], pathMatch[2]);
                const resolved = walkState.resolvePath(parsed.filePath);
                if (resolved) {
                    const nested = walkExecutionEvents(
                        resolved,
                        parsed.targetSection,
                        stack,
                        Object.assign({}, walkState, { fromIncludeSite: includeSite })
                    );
                    for (let n = 0; n < nested.length; n++) {
                        events.push(nested[n]);
                    }
                }
            }
            continue;
        }

        const resourceEvents = collectResourceEventsFromLine(
            rawLine,
            i,
            normalized,
            walkState.nsByKind,
            walkState.fromIncludeSite
        );
        const callTargets = collectCallTargets(codeLine);
        const steps = [];
        for (let r = 0; r < resourceEvents.length; r++) {
            steps.push({ index: resourceEvents[r].startChar, type: 'resource', event: resourceEvents[r] });
        }
        for (let c = 0; c < callTargets.length; c++) {
            steps.push({ index: callTargets[c].index, type: 'call', target: callTargets[c] });
        }
        steps.sort((a, b) => a.index - b.index);

        for (let s = 0; s < steps.length; s++) {
            const step = steps[s];
            if (step.type === 'resource') {
                events.push(step.event);
                if (step.event.action === 'open' &&
                    step.event.kind === 'database' &&
                    hasLineContinuation(rawLine) &&
                    /\bdbcreate\s*\(/i.test(codeLine)) {
                    skippingDbcreateContinuation = true;
                }
                continue;
            }
            const parsed = step.target.parsed;
            const resolved = walkState.resolvePath(parsed.filePath);
            if (!resolved) {
                continue;
            }
            const childNs = {
                database: walkState.shareDb ? walkState.nsByKind.database : walkState.freshNs(),
                file: walkState.shareFiles ? walkState.nsByKind.file : walkState.freshNs()
            };
            const nested = walkExecutionEvents(
                resolved,
                parsed.targetSection,
                stack,
                Object.assign({}, walkState, { nsByKind: childNs })
            );
            for (let n = 0; n < nested.length; n++) {
                events.push(nested[n]);
            }
        }
    }

    stack.delete(normalized);
    return events;
}

function getTransitiveIncludeParents(targetPath, getDirectIncludeParents, normalizePath) {
    const normalizedTarget = normalizePath(targetPath);
    const parents = new Set();
    const queue = [normalizedTarget];
    const seen = new Set([normalizedTarget]);
    while (queue.length > 0) {
        const current = queue.shift();
        const direct = getDirectIncludeParents(current) || [];
        for (let i = 0; i < direct.length; i++) {
            const fromPath = normalizePath(direct[i].fromPath);
            if (!fromPath || seen.has(fromPath)) {
                continue;
            }
            seen.add(fromPath);
            parents.add(fromPath);
            queue.push(fromPath);
        }
    }
    return parents;
}

function getIncludeRoots(targetPath, getDirectIncludeParents, normalizePath) {
    const parents = getTransitiveIncludeParents(targetPath, getDirectIncludeParents, normalizePath);
    const roots = [];
    parents.forEach((parentPath) => {
        const incoming = getDirectIncludeParents(parentPath) || [];
        if (incoming.length === 0) {
            roots.push(parentPath);
        }
    });
    return roots;
}

function includeReaches(fromPath, toPath, context, seen) {
    const normalizedFrom = context.normalizePath(fromPath);
    const normalizedTo = context.normalizePath(toPath);
    if (!normalizedFrom || !normalizedTo) {
        return false;
    }
    if (normalizedFrom === normalizedTo) {
        return true;
    }
    if (seen.has(normalizedFrom)) {
        return false;
    }
    seen.add(normalizedFrom);
    const outgoing = typeof context.getOutgoingIncludeLinks === 'function'
        ? (context.getOutgoingIncludeLinks(normalizedFrom) || [])
        : [];
    for (let i = 0; i < outgoing.length; i++) {
        if (includeReaches(outgoing[i].targetPath, normalizedTo, context, seen)) {
            return true;
        }
    }
    return false;
}

function findRootIncludeSite(rootPath, targetPath, context) {
    const outgoing = typeof context.getOutgoingIncludeLinks === 'function'
        ? (context.getOutgoingIncludeLinks(rootPath) || [])
        : [];
    for (let i = 0; i < outgoing.length; i++) {
        const link = outgoing[i];
        if (includeReaches(link.targetPath, targetPath, context, new Set())) {
            return link;
        }
    }
    const direct = context.getDirectIncludeParents(targetPath) || [];
    for (let i = 0; i < direct.length; i++) {
        if (context.normalizePath(direct[i].fromPath) === context.normalizePath(rootPath)) {
            return direct[i];
        }
    }
    return null;
}

function createWalkState(context, walkRootPath, fromIncludeSite) {
    const rootLines = context.getFileLines(walkRootPath) || [];
    const flags = parseFileCommonFlags(rootLines);
    let nsCounter = 0;
    return {
        getFileLines: context.getFileLines,
        resolvePath: context.resolvePath,
        normalizePath: context.normalizePath,
        shareDb: flags.commonDbases,
        shareFiles: flags.commonFiles,
        fromIncludeSite: fromIncludeSite || null,
        nsByKind: { database: 'root-db', file: 'root-file' },
        freshNs() {
            nsCounter += 1;
            return `ns-${nsCounter}`;
        }
    };
}

function buildOpenMessage(event, onIncludeLine) {
    const tokenLabel = event.token || event.nameLiteral || '?';
    if (onIncludeLine) {
        if (event.kind === 'file') {
            return `Included code opens file ${tokenLabel} which is not closed in this file`;
        }
        if (event.alreadyOpen) {
            return `Included code aliases ALREADY_OPEN database ${tokenLabel} which is not closed in this file`;
        }
        return `Included code opens database ${tokenLabel} which is not closed in this file`;
    }
    if (event.kind === 'file') {
        return `File ${tokenLabel} is opened here and not closed in this execution tree`;
    }
    if (event.alreadyOpen) {
        return `ALREADY_OPEN database ${tokenLabel} is aliased here and not closed in this execution tree`;
    }
    return `Database ${tokenLabel} is opened here and not closed in this execution tree`;
}

function relatedForFailingRoots(openEvent, failingRoots, context) {
    const related = [];
    for (let i = 0; i < failingRoots.length; i++) {
        const rootPath = failingRoots[i];
        const site = findRootIncludeSite(rootPath, openEvent.filePath, context);
        const line = site && typeof site.line === 'number' ? site.line : 0;
        const lineText = site && site.lineText ? site.lineText : '';
        related.push({
            uri: context.filePathToUri(rootPath),
            line,
            startChar: 0,
            endChar: Math.max(1, lineText.length),
            message: 'Include root does not close this handle'
        });
    }
    return related;
}

function collectUnclosedResourceIssues(context) {
    const issues = [];
    const currentFilePath = context.normalizePath(context.currentFilePath);
    if (!currentFilePath) {
        return issues;
    }

    const ownWalk = walkExecutionEvents(
        currentFilePath,
        null,
        new Set(),
        createWalkState(context, currentFilePath, null)
    );

    const includeRoots = getIncludeRoots(
        currentFilePath,
        context.getDirectIncludeParents,
        context.normalizePath
    );

    const unmatchedOwnOpens = [];
    for (let i = 0; i < ownWalk.length; i++) {
        const event = ownWalk[i];
        if (event.action !== 'open') {
            continue;
        }
        if (event.filePath !== currentFilePath) {
            continue;
        }
        if (event.ignoreClose) {
            continue;
        }
        if (openHasForwardClose(ownWalk, i)) {
            continue;
        }
        unmatchedOwnOpens.push(event);
    }

    for (let o = 0; o < unmatchedOwnOpens.length; o++) {
        const openEvent = unmatchedOwnOpens[o];
        const failingRoots = [];
        if (includeRoots.length > 0) {
            for (let r = 0; r < includeRoots.length; r++) {
                const rootPath = includeRoots[r];
                const rootWalk = walkExecutionEvents(
                    rootPath,
                    null,
                    new Set(),
                    createWalkState(context, rootPath, null)
                );
                let closedInRoot = false;
                for (let e = 0; e < rootWalk.length; e++) {
                    const event = rootWalk[e];
                    if (event.action !== 'open') {
                        continue;
                    }
                    if (event.filePath !== openEvent.filePath || event.line !== openEvent.line) {
                        continue;
                    }
                    if (event.kind !== openEvent.kind || !tokensMatch(event.token, openEvent.token)) {
                        continue;
                    }
                    if (openHasForwardClose(rootWalk, e)) {
                        closedInRoot = true;
                        break;
                    }
                }
                if (!closedInRoot) {
                    failingRoots.push(rootPath);
                }
            }
            if (failingRoots.length === 0) {
                continue;
            }
        }

        issues.push({
            line: openEvent.line,
            startChar: openEvent.startChar,
            endChar: openEvent.endChar,
            message: buildOpenMessage(openEvent, false),
            severity: openEvent.alreadyOpen ? 'information' : 'warning',
            code: openEvent.alreadyOpen ? UNCLOSED_ALREADY_OPEN_CODE : UNCLOSED_RESOURCE_CODE,
            related: includeRoots.length > 0
                ? relatedForFailingRoots(openEvent, failingRoots, context)
                : []
        });
    }

    const includeLeftoverKeys = new Set();
    for (let i = 0; i < ownWalk.length; i++) {
        const event = ownWalk[i];
        if (event.action !== 'open' || !event.fromIncludeSite) {
            continue;
        }
        if (event.fromIncludeSite.filePath !== currentFilePath) {
            continue;
        }
        if (event.fromIncludeSite.ignoreClose) {
            continue;
        }
        if (event.ignoreClose) {
            continue;
        }
        if (openHasForwardClose(ownWalk, i)) {
            continue;
        }
        const key = [
            event.fromIncludeSite.line,
            event.kind,
            String(event.token || '').toLowerCase(),
            event.alreadyOpen ? '1' : '0'
        ].join('::');
        if (includeLeftoverKeys.has(key)) {
            continue;
        }
        includeLeftoverKeys.add(key);
        issues.push({
            line: event.fromIncludeSite.line,
            startChar: event.fromIncludeSite.startChar,
            endChar: event.fromIncludeSite.endChar,
            message: buildOpenMessage(event, true),
            severity: event.alreadyOpen ? 'information' : 'warning',
            code: event.alreadyOpen ? UNCLOSED_ALREADY_OPEN_CODE : UNCLOSED_RESOURCE_CODE,
            related: [{
                uri: context.filePathToUri(event.filePath),
                line: event.line,
                startChar: event.startChar,
                endChar: event.endChar,
                message: buildOpenMessage(event, false)
            }]
        });
    }

    return issues;
}

module.exports = {
    UNCLOSED_RESOURCE_CODE,
    UNCLOSED_ALREADY_OPEN_CODE,
    lineHasIgnoreClose,
    buildIgnoreCloseInsertion,
    parseFileCommonFlags,
    namesMatch,
    collectUnclosedResourceIssues,
    walkExecutionEvents,
    createWalkState,
};
