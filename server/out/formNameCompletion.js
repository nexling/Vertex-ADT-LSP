'use strict';

const fs = require('fs');
const path = require('path');

const INSIDE_PAREN_FORM_COMMANDS = new Set(['edit_obj_data', 'edit_jnt_data', 'dbedit']);
const FORM_COMMAND_PATTERN = /\b(edit_obj_data|edit_jnt_data|dbedit|set_macro_form)\s*\(/gi;

function analyzeFormNameCompletion(lineText, cursorCharacter) {
    const line = String(lineText || '');
    const cursor = clampCursor(cursorCharacter, line.length);
    if (isCursorInComment(line, cursor)) {
        return null;
    }

    const command = findLastCommandBeforeCursor(line, cursor);
    if (!command) {
        return null;
    }

    const parsed = parseCallArguments(line, command.openParen);
    if (command.name === 'set_macro_form') {
        if (parsed.closeParen < 0 || cursor <= parsed.closeParen) {
            return null;
        }
        return stringLiteralContext(line, parsed.closeParen + 1, cursor);
    }

    if (!INSIDE_PAREN_FORM_COMMANDS.has(command.name)) {
        return null;
    }
    if (parsed.closeParen >= 0 && cursor > parsed.closeParen) {
        return null;
    }

    const argIndex = argumentIndexAtCursor(parsed.args, cursor, parsed.closeParen);
    if (argIndex !== 1) {
        return null;
    }

    const arg = parsed.args[1];
    return stringLiteralContext(line, arg.start, cursor);
}

function mergeFormNameEntries(customNames, systemNames) {
    const seen = new Set();
    const merged = [];

    appendUniqueFormNames(merged, seen, customNames, 'custom/forms');
    appendUniqueFormNames(merged, seen, systemNames, 'system/forms');

    merged.sort((a, b) => a.name.localeCompare(b.name, undefined, { sensitivity: 'base' }));
    return merged;
}

function listTopLevelFileNames(dirPath, fsLike) {
    const io = fsLike || fs;
    if (!dirPath) {
        return [];
    }

    try {
        if (!io.existsSync(dirPath)) {
            return [];
        }
        const stat = io.statSync(dirPath);
        if (!stat || !stat.isDirectory()) {
            return [];
        }

        const entries = io.readdirSync(dirPath, { withFileTypes: true });
        const names = [];
        for (let i = 0; i < entries.length; i++) {
            const entry = entries[i];
            if (entry && typeof entry.isFile === 'function' && entry.isFile()) {
                names.push(entry.name);
            }
        }
        return names;
    } catch (_error) {
        return [];
    }
}

function collectFormNameEntries(customFormsDir, systemFormsDir, fsLike) {
    return mergeFormNameEntries(
        listTopLevelFileNames(customFormsDir, fsLike),
        listTopLevelFileNames(systemFormsDir, fsLike)
    );
}

function formInsertText(fileName, insideQuotes) {
    return insideQuotes ? fileName : `"${fileName}"`;
}

function clampCursor(cursorCharacter, lineLength) {
    const cursor = typeof cursorCharacter === 'number' && isFinite(cursorCharacter)
        ? cursorCharacter
        : lineLength;
    if (cursor < 0) {
        return 0;
    }
    if (cursor > lineLength) {
        return lineLength;
    }
    return cursor;
}

function isCursorInComment(line, cursor) {
    let quote = null;
    for (let i = 0; i < cursor; i++) {
        const ch = line[i];
        if (quote) {
            if (ch === quote) {
                quote = null;
            }
            continue;
        }
        if (ch === '"' || ch === "'") {
            quote = ch;
            continue;
        }
        if (ch === ';') {
            return true;
        }
    }
    return false;
}

function findLastCommandBeforeCursor(line, cursor) {
    FORM_COMMAND_PATTERN.lastIndex = 0;
    let found = null;
    let match = FORM_COMMAND_PATTERN.exec(line);
    while (match) {
        const openParen = match.index + match[0].length - 1;
        if (match.index < cursor && openParen < cursor) {
            found = {
                name: match[1].toLowerCase(),
                start: match.index,
                openParen
            };
        }
        if (match.index >= cursor) {
            break;
        }
        match = FORM_COMMAND_PATTERN.exec(line);
    }
    return found;
}

function parseCallArguments(line, openParenIndex) {
    const args = [{
        start: openParenIndex + 1,
        end: openParenIndex + 1
    }];
    let current = args[0];
    let depth = 1;
    let quote = null;
    let i = openParenIndex + 1;

    while (i < line.length && depth > 0) {
        const ch = line[i];
        if (quote) {
            if (ch === quote) {
                quote = null;
            }
            i += 1;
            continue;
        }
        if (ch === '"' || ch === "'") {
            quote = ch;
            i += 1;
            continue;
        }
        if (ch === '(') {
            depth += 1;
            i += 1;
            continue;
        }
        if (ch === ')') {
            depth -= 1;
            if (depth === 0) {
                current.end = i;
                return {
                    args,
                    closeParen: i
                };
            }
            i += 1;
            continue;
        }
        if (ch === ',' && depth === 1) {
            current.end = i;
            current = {
                start: i + 1,
                end: i + 1
            };
            args.push(current);
            i += 1;
            continue;
        }
        i += 1;
    }

    current.end = line.length;
    return {
        args,
        closeParen: -1
    };
}

function argumentIndexAtCursor(args, cursor, closeParen) {
    if (closeParen >= 0 && cursor > closeParen) {
        return -1;
    }
    for (let i = 0; i < args.length; i++) {
        const start = args[i].start;
        const end = args[i].end;
        if (cursor >= start && cursor <= end) {
            return i;
        }
    }
    return -1;
}

function stringLiteralContext(line, regionStart, cursor) {
    let i = regionStart;
    while (i < cursor && isWhitespace(line[i])) {
        i += 1;
    }

    if (i >= cursor) {
        return {
            insideQuotes: false,
            quoteChar: null,
            replaceStart: cursor,
            replaceEnd: cursor
        };
    }

    const quoteChar = line[i];
    if (quoteChar !== '"' && quoteChar !== "'") {
        return null;
    }

    const contentStart = i + 1;
    let close = -1;
    for (let j = contentStart; j < line.length; j++) {
        if (line[j] === quoteChar) {
            close = j;
            break;
        }
    }

    if (close >= 0 && cursor > close) {
        return null;
    }

    return {
        insideQuotes: true,
        quoteChar,
        replaceStart: contentStart,
        replaceEnd: close >= 0 ? close : Math.max(cursor, contentStart)
    };
}

function isWhitespace(ch) {
    return ch === ' ' || ch === '\t';
}

function appendUniqueFormNames(merged, seen, names, origin) {
    const list = Array.isArray(names) ? names : [];
    for (let i = 0; i < list.length; i++) {
        const name = list[i];
        if (!name) {
            continue;
        }
        const key = String(name).toLowerCase();
        if (seen.has(key)) {
            continue;
        }
        seen.add(key);
        merged.push({
            name,
            origin
        });
    }
}

function resolveFormsDirectory(kind, options) {
    const workspaceRoot = options && options.workspaceRoot;
    const customName = (options && options.customName) || 'custom';
    const workspaceFolders = (options && options.workspaceFolders) || [];
    const candidates = [];

    if (kind === 'custom') {
        for (let i = 0; i < workspaceFolders.length; i++) {
            const folderPath = workspaceFolders[i];
            if (folderPath && /^custom/i.test(path.basename(folderPath))) {
                candidates.push(path.join(folderPath, 'forms'));
            }
        }
        if (workspaceRoot) {
            if (/^custom/i.test(path.basename(workspaceRoot))) {
                candidates.push(path.join(workspaceRoot, 'forms'));
            } else {
                candidates.push(path.join(workspaceRoot, customName, 'forms'));
            }
        }
    } else {
        for (let i = 0; i < workspaceFolders.length; i++) {
            const folderPath = workspaceFolders[i];
            if (folderPath && /^system$/i.test(path.basename(folderPath))) {
                candidates.push(path.join(folderPath, 'forms'));
            }
        }
        if (workspaceRoot) {
            candidates.push(path.join(workspaceRoot, 'system', 'forms'));
            if (/^custom/i.test(path.basename(workspaceRoot))) {
                candidates.push(path.join(path.dirname(workspaceRoot), 'system', 'forms'));
            }
        }
    }

    return firstExistingDirectory(candidates, options && options.fs);
}

function firstExistingDirectory(candidates, fsLike) {
    const io = fsLike || fs;
    const seen = new Set();
    for (let i = 0; i < candidates.length; i++) {
        const dirPath = candidates[i];
        if (!dirPath) {
            continue;
        }
        const key = path.normalize(dirPath).replace(/\\/g, '/').toLowerCase();
        if (seen.has(key)) {
            continue;
        }
        seen.add(key);
        try {
            if (io.existsSync(dirPath) && io.statSync(dirPath).isDirectory()) {
                return dirPath;
            }
        } catch (_error) {
            // Try the next candidate.
        }
    }
    return null;
}

function collectWorkspaceFormNames(options) {
    const customDir = resolveFormsDirectory('custom', options);
    const systemDir = resolveFormsDirectory('system', options);
    return collectFormNameEntries(customDir, systemDir, options && options.fs);
}

module.exports = {
    analyzeFormNameCompletion,
    mergeFormNameEntries,
    listTopLevelFileNames,
    collectFormNameEntries,
    collectWorkspaceFormNames,
    resolveFormsDirectory,
    formInsertText
};
