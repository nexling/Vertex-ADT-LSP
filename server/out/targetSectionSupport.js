'use strict';

const INCLUDE_CALL_SECTION_KEYWORDS = new Set(['section', 'sectionoffile']);
const TRIGGER_INVOKED = 1;
const SECTION_IDENT = /[A-Za-z_][A-Za-z0-9_]*/;

function stripPathQuotes(value) {
    const trimmed = String(value || '').trim();
    if ((trimmed.startsWith('"') && trimmed.endsWith('"') && trimmed.length >= 2) ||
        (trimmed.startsWith("'") && trimmed.endsWith("'") && trimmed.length >= 2)) {
        return trimmed.slice(1, -1);
    }
    if (trimmed.startsWith('"') || trimmed.startsWith("'")) {
        return trimmed.slice(1);
    }
    return trimmed;
}

function isSectionKeyword(token) {
    return typeof token === 'string' && INCLUDE_CALL_SECTION_KEYWORDS.has(token.toLowerCase());
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

function extractCallParametersFromText(fileContent, targetSection) {
    const lines = String(fileContent || '').split('\n');
    const fileSections = parseFileSections(lines);
    const parameters = [];
    const regex = /(\w+)\$?\s*=\s*get\(\s*\$(\w+)\s*\)/g;

    for (let i = 0; i < lines.length; i++) {
        if (targetSection && getSectionForLine(i, fileSections) !== targetSection) {
            continue;
        }
        const line = lines[i];
        regex.lastIndex = 0;
        let match;
        while ((match = regex.exec(line)) !== null) {
            let variableName = '';
            try {
                variableName = match[0].split('=')[0].trim();
            } catch (_error) {
                variableName = match[1].trim();
            }
            parameters.push({
                variableName,
                parameterName: match[2].trim(),
                lineIndex: i,
                sectionName: getSectionForLine(i, fileSections)
            });
        }
    }
    return parameters;
}

function getExpectedCallParamCountFromText(fileContent, targetSection) {
    const parameters = extractCallParametersFromText(fileContent, targetSection);
    let maxIndex = 0;
    for (let i = 0; i < parameters.length; i++) {
        const paramMatch = /^CALLPARAM(\d+)$/i.exec(parameters[i].parameterName || '');
        if (paramMatch) {
            maxIndex = Math.max(maxIndex, parseInt(paramMatch[1], 10));
        }
    }
    return maxIndex;
}

function listUniqueTargetSectionNames(text) {
    const sections = parseFileSections(String(text || '').split('\n'));
    const names = [];
    const seen = new Set();
    for (let i = 0; i < sections.length; i++) {
        const name = sections[i].name;
        if (isSectionKeyword(name) || seen.has(name)) {
            continue;
        }
        seen.add(name);
        names.push(name);
    }
    names.sort();
    return names;
}

function isUnknownTargetSectionName(token, sectionNames) {
    if (!token || isSectionKeyword(token)) {
        return false;
    }
    const names = sectionNames || [];
    if (names.indexOf(token) !== -1) {
        return false;
    }
    for (let i = 0; i < names.length; i++) {
        if (names[i].startsWith(token)) {
            return false;
        }
    }
    return true;
}

function parseIncludeCallPathAndSection(rawPath, lineRemainder) {
    let pathPart = stripPathQuotes(rawPath).replace(/\\/g, '/').trim();
    let targetSection = null;

    const pathSectionMatch = pathPart.match(/^(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$/);
    if (pathSectionMatch) {
        const token = pathSectionMatch[2];
        if (!isSectionKeyword(token)) {
            targetSection = token;
            pathPart = pathSectionMatch[1].trim();
        } else {
            pathPart = pathPart.replace(/\s+SECTION(?:OFFILE)?$/i, '').trim();
        }
    }

    if (lineRemainder && lineRemainder.trim()) {
        const remMatch = lineRemainder.trim().match(/^([A-Za-z_][A-Za-z0-9_]*)\b/);
        if (remMatch && !isSectionKeyword(remMatch[1])) {
            targetSection = remMatch[1];
        }
    }

    return { filePath: pathPart, targetSection };
}

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

function findInlineBangCommentIndex(line) {
    for (let i = 0; i < line.length; i++) {
        if (line[i] === '!' && !isInsideQuotes(line, i)) {
            return i;
        }
    }
    return -1;
}

function codeLinePrefix(line) {
    const bang = findInlineBangCommentIndex(line);
    return bang === -1 ? line : line.slice(0, bang);
}

function escapeRegExp(value) {
    return String(value).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function findIdentRange(line, ident, fromIndex) {
    const pattern = new RegExp(`(^|[^A-Za-z0-9_])(${escapeRegExp(ident)})(?![A-Za-z0-9_])`, 'g');
    pattern.lastIndex = fromIndex;
    const match = pattern.exec(line);
    if (!match) {
        return null;
    }
    const start = match.index + match[1].length;
    return { start, end: start + ident.length };
}

function pushSuffixSite(sites, filePath, targetSection, range, form) {
    if (!targetSection || !range) {
        return;
    }
    sites.push({
        filePath,
        targetSection,
        sectionStart: range.start,
        sectionEnd: range.end,
        form
    });
}

function collectNamedSectionSuffixes(line) {
    const sites = [];
    const code = codeLinePrefix(line);
    if (!code.trim() || code.trim().startsWith('!')) {
        return sites;
    }

    const includeMatch = code.match(/^(\s*# include\s+)((?:"[^"]+"|'[^']+'|[^\s#;]+))(\s*)(.*)$/);
    if (includeMatch) {
        const prefix = includeMatch[1];
        const rawPath = includeMatch[2];
        const remainder = includeMatch[4];
        const parsed = parseIncludeCallPathAndSection(rawPath, remainder);
        if (parsed.targetSection) {
            const pathStart = prefix.length;
            const inner = stripPathQuotes(rawPath);
            const innerSection = inner.match(/^(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$/);
            let range = null;
            if (innerSection && innerSection[2] === parsed.targetSection) {
                range = findIdentRange(code, parsed.targetSection, pathStart);
            } else {
                range = findIdentRange(code, parsed.targetSection, pathStart + rawPath.length);
            }
            pushSuffixSite(sites, parsed.filePath, parsed.targetSection, range, 'include');
        }
    }

    const callParenRe = /\b(?:[A-Za-z][A-Za-z0-9_]*\$?=)?call\s*\(\s*(["'])([^"'\\]*)\1/gi;
    let callMatch;
    while ((callMatch = callParenRe.exec(code)) !== null) {
        const quoteStart = callMatch.index + callMatch[0].indexOf(callMatch[1]);
        const inner = callMatch[2];
        const parsed = parseIncludeCallPathAndSection(inner, '');
        if (!parsed.targetSection) {
            continue;
        }
        const range = findIdentRange(code, parsed.targetSection, quoteStart);
        pushSuffixSite(sites, parsed.filePath, parsed.targetSection, range, 'call_paren');
    }

    const callSpaceRe = /\bcall\s+(?!\()((?:"[^"]+"|'[^']+'|[^\s,(]+))(\s+)([A-Za-z_][A-Za-z0-9_]*)/gi;
    let spaceMatch;
    while ((spaceMatch = callSpaceRe.exec(code)) !== null) {
        const rawPath = spaceMatch[1];
        const token = spaceMatch[3];
        const parsed = parseIncludeCallPathAndSection(rawPath, token);
        if (!parsed.targetSection) {
            continue;
        }
        const tokenStart = spaceMatch.index + spaceMatch[0].length - token.length;
        pushSuffixSite(sites, parsed.filePath, parsed.targetSection, {
            start: tokenStart,
            end: tokenStart + token.length
        }, 'call_space');
    }

    const unique = [];
    const seen = new Set();
    for (let i = 0; i < sites.length; i++) {
        const key = `${sites[i].sectionStart}:${sites[i].sectionEnd}:${sites[i].targetSection}`;
        if (seen.has(key)) {
            continue;
        }
        seen.add(key);
        unique.push(sites[i]);
    }
    return unique;
}

function matchDirective(prefix) {
    const trimmed = prefix.trimStart();
    const leading = prefix.length - trimmed.length;

    if (trimmed.startsWith('##included_from')) {
        const token = '##included_from';
        const after = prefix.slice(leading + token.length);
        const ws = after.match(/^\s*/);
        return {
            kind: 'included_from',
            argsStart: leading + token.length + (ws ? ws[0].length : 0)
        };
    }
    if (trimmed.startsWith('# include')) {
        const token = '# include';
        const after = prefix.slice(leading + token.length);
        const ws = after.match(/^\s*/);
        return {
            kind: 'include',
            argsStart: leading + token.length + (ws ? ws[0].length : 0),
            sectionInsideQuotes: false
        };
    }

    const assign = trimmed.match(/^([A-Za-z][A-Za-z0-9_]*\$?=call\s*\(\s*)/);
    if (assign) {
        return {
            kind: 'call_paren',
            argsStart: leading + assign[1].length,
            sectionInsideQuotes: true
        };
    }
    const callParen = trimmed.match(/^(call\s*\(\s*)/);
    if (callParen) {
        return {
            kind: 'call_paren',
            argsStart: leading + callParen[1].length,
            sectionInsideQuotes: true
        };
    }
    const callSpace = trimmed.match(/^(call\s+)(?!\()/);
    if (callSpace) {
        return {
            kind: 'call_space',
            argsStart: leading + callSpace[1].length,
            sectionInsideQuotes: false
        };
    }
    return null;
}

function readBarePath(line, start, endLimit) {
    let i = start;
    if (i < line.length && (line[i] === '"' || line[i] === "'")) {
        const quote = line[i];
        i++;
        while (i < line.length && line[i] !== quote) {
            i++;
        }
        if (i < line.length && line[i] === quote) {
            i++;
        }
        return { raw: line.slice(start, Math.min(i, endLimit === undefined ? i : Math.max(i, endLimit))), end: i, quoted: true, quote };
    }
    while (i < line.length && !/\s/.test(line[i]) && line[i] !== ',' && line[i] !== ';' && line[i] !== ')') {
        i++;
    }
    return { raw: line.slice(start, i), end: i, quoted: false, quote: null };
}

function parseSuffixSlot(line, pathEnd, cursor) {
    let i = pathEnd;
    while (i < line.length && (line[i] === ' ' || line[i] === '\t')) {
        i++;
    }
    const identStart = i;
    if (i >= line.length || !/[A-Za-z_]/.test(line[i])) {
        const inGap = cursor >= pathEnd && (cursor <= identStart || (identStart >= line.length && cursor >= pathEnd));
        const onlyWhitespaceAfterPath = i >= line.length || !/[A-Za-z_0-9]/.test(line[i]);
        if (cursor > pathEnd && onlyWhitespaceAfterPath && cursor <= i) {
            return {
                inSuffix: true,
                pastSuffix: false,
                sectionToken: '',
                suffixStart: cursor,
                suffixEnd: cursor
            };
        }
        if (inGap && cursor > pathEnd) {
            return {
                inSuffix: true,
                pastSuffix: false,
                sectionToken: '',
                suffixStart: cursor,
                suffixEnd: cursor
            };
        }
        return {
            inSuffix: false,
            pastSuffix: cursor > pathEnd,
            sectionToken: '',
            suffixStart: cursor,
            suffixEnd: cursor
        };
    }

    while (i < line.length && /[A-Za-z0-9_]/.test(line[i])) {
        i++;
    }
    const identEnd = i;
    const token = line.slice(identStart, identEnd);
    if (cursor < identStart) {
        return {
            inSuffix: cursor > pathEnd,
            pastSuffix: false,
            sectionToken: '',
            suffixStart: cursor,
            suffixEnd: cursor
        };
    }
    if (cursor <= identEnd) {
        return {
            inSuffix: true,
            pastSuffix: false,
            sectionToken: token,
            suffixStart: identStart,
            suffixEnd: identEnd
        };
    }
    return {
        inSuffix: false,
        pastSuffix: true,
        sectionToken: token,
        suffixStart: identStart,
        suffixEnd: identEnd
    };
}

function parseQuotedArg(line, quoteIndex, cursor, sectionInsideQuotes) {
    const quote = line[quoteIndex];
    const contentStart = quoteIndex + 1;
    let close = -1;
    for (let i = contentStart; i < line.length; i++) {
        if (line[i] === quote) {
            close = i;
            break;
        }
    }

    if (close !== -1 && cursor > close) {
        if (sectionInsideQuotes) {
            return { afterCallQuote: true, modeNone: true };
        }
        const filePath = stripPathQuotes(line.slice(quoteIndex, close + 1)).replace(/\\/g, '/');
        const suffix = parseSuffixSlot(line, close + 1, cursor);
        return {
            filePath,
            atEndOfPath: false,
            inPath: false,
            ...suffix
        };
    }

    const contentEnd = close === -1 ? Math.max(cursor, contentStart) : Math.min(cursor, close);
    const content = line.slice(contentStart, contentEnd);
    const space = content.search(/\s/);
    if (space === -1) {
        const fullContentEnd = close === -1 ? line.length : close;
        const fullPath = stripPathQuotes(line.slice(quoteIndex, close === -1 ? contentEnd : close + 1)).replace(/\\/g, '/');
        const atEndOfPath = cursor >= contentEnd && (close === -1 || cursor <= close);
        return {
            filePath: fullPath || content.replace(/\\/g, '/'),
            inPath: true,
            atEndOfPath,
            inSuffix: false,
            pastSuffix: false,
            sectionToken: '',
            suffixStart: cursor,
            suffixEnd: cursor,
            quotedContentStart: contentStart,
            quotedContentEnd: fullContentEnd
        };
    }

    const filePath = content.slice(0, space).replace(/\\/g, '/');
    const suffixStartInContent = space;
    let j = space;
    while (j < content.length && /\s/.test(content[j])) {
        j++;
    }
    const identStart = contentStart + j;
    let identEnd = identStart;
    while (identEnd < (close === -1 ? line.length : close) && /[A-Za-z0-9_]/.test(line[identEnd])) {
        identEnd++;
    }
    const token = line.slice(identStart, identEnd);
    if (cursor < identStart) {
        return {
            filePath,
            inPath: false,
            atEndOfPath: false,
            inSuffix: true,
            pastSuffix: false,
            sectionToken: '',
            suffixStart: cursor,
            suffixEnd: cursor
        };
    }
    if (close !== -1 && identEnd < close && cursor > identEnd) {
        return {
            filePath,
            inPath: false,
            atEndOfPath: false,
            inSuffix: false,
            pastSuffix: true,
            sectionToken: token,
            suffixStart: identStart,
            suffixEnd: identEnd
        };
    }
    return {
        filePath,
        inPath: false,
        atEndOfPath: false,
        inSuffix: true,
        pastSuffix: false,
        sectionToken: token,
        suffixStart: identStart,
        suffixEnd: identEnd || cursor
    };
}

function parseDirectiveArgs(line, directive, cursor) {
    let i = directive.argsStart;
    while (i < cursor && (line[i] === ' ' || line[i] === '\t')) {
        i++;
    }

    if (i >= cursor && i >= line.length) {
        return {
            filePath: '',
            inPath: true,
            atEndOfPath: true,
            inSuffix: false,
            pastSuffix: false,
            sectionToken: '',
            suffixStart: cursor,
            suffixEnd: cursor
        };
    }

    if (i < line.length && (line[i] === '"' || line[i] === "'")) {
        return parseQuotedArg(line, i, cursor, !!directive.sectionInsideQuotes);
    }

    if (i >= cursor) {
        return {
            filePath: '',
            inPath: true,
            atEndOfPath: true,
            inSuffix: false,
            pastSuffix: false,
            sectionToken: '',
            suffixStart: cursor,
            suffixEnd: cursor
        };
    }

    const path = readBarePath(line, i);
    const filePath = stripPathQuotes(path.raw).replace(/\\/g, '/');
    const atEndOfPath = cursor <= path.end && cursor >= i;
    if (cursor <= path.end) {
        return {
            filePath,
            inPath: true,
            atEndOfPath: cursor === path.end || cursor === line.length || /\s/.test(line[cursor] || ''),
            inSuffix: false,
            pastSuffix: false,
            sectionToken: '',
            suffixStart: cursor,
            suffixEnd: cursor
        };
    }

    const suffix = parseSuffixSlot(line, path.end, cursor);
    return {
        filePath,
        inPath: false,
        atEndOfPath: false,
        ...suffix
    };
}

function analyzeIncludeCallCompletion(input) {
    const lineText = String(input.lineText || '');
    const cursor = Math.max(0, Math.min(Number(input.cursorCharacter) || 0, lineText.length));
    const triggerKind = input.triggerKind;
    const pathStatus = typeof input.pathStatus === 'function'
        ? input.pathStatus
        : () => ({ isFile: false, isDirectory: false });
    const getSectionNames = typeof input.getSectionNames === 'function'
        ? input.getSectionNames
        : () => [];

    const prefix = lineText.slice(0, cursor);
    const directive = matchDirective(prefix);
    if (!directive) {
        return { mode: 'none' };
    }

    if (directive.kind === 'included_from') {
        const parsed = parseDirectiveArgs(lineText, Object.assign({}, directive, { sectionInsideQuotes: false }), cursor);
        return {
            mode: 'paths',
            directive: directive.kind,
            filePath: parsed.filePath || ''
        };
    }

    const parsed = parseDirectiveArgs(lineText, directive, cursor);
    if (parsed.modeNone || parsed.afterCallQuote || parsed.pastSuffix) {
        return { mode: 'none' };
    }

    const filePath = parsed.filePath || '';
    const status = filePath ? pathStatus(filePath) : { isFile: false, isDirectory: false };
    const canUseSections = !!(status.isFile && !status.isDirectory);
    const names = canUseSections ? (getSectionNames(filePath) || []) : [];
    const hasSections = names.length > 0;
    const invoked = triggerKind === TRIGGER_INVOKED;

    if (hasSections && (parsed.inSuffix || (invoked && parsed.inPath && parsed.atEndOfPath))) {
        return {
            mode: 'sections',
            directive: directive.kind,
            filePath,
            sections: names,
            insertLeadingSpace: !!(parsed.inPath && !parsed.inSuffix),
            replaceStart: parsed.inSuffix ? parsed.suffixStart : cursor,
            replaceEnd: parsed.inSuffix ? parsed.suffixEnd : cursor,
            sectionToken: parsed.sectionToken || ''
        };
    }

    if (parsed.inSuffix) {
        return { mode: 'none' };
    }

    return {
        mode: 'paths',
        directive: directive.kind,
        filePath
    };
}

function lookupPathFromCallArgument(rawPath) {
    return parseIncludeCallPathAndSection(rawPath, '').filePath;
}

function isInParenthesizedCallArgumentList(lineText, cursorCharacter) {
    const line = String(lineText || '');
    const cursor = Math.max(0, Math.min(Number(cursorCharacter) || 0, line.length));
    const directive = matchDirective(line.slice(0, cursor));
    if (!directive || directive.kind !== 'call_paren') {
        return false;
    }
    const parsed = parseDirectiveArgs(line, directive, cursor);
    return !!(parsed && (parsed.afterCallQuote || parsed.modeNone || parsed.pastSuffix));
}

module.exports = {
    INCLUDE_CALL_SECTION_KEYWORDS,
    TRIGGER_INVOKED,
    stripPathQuotes,
    isSectionKeyword,
    parseFileSections,
    getSectionForLine,
    extractCallParametersFromText,
    getExpectedCallParamCountFromText,
    listUniqueTargetSectionNames,
    isUnknownTargetSectionName,
    parseIncludeCallPathAndSection,
    lookupPathFromCallArgument,
    isInParenthesizedCallArgumentList,
    collectNamedSectionSuffixes,
    matchDirective,
    analyzeIncludeCallCompletion
};
