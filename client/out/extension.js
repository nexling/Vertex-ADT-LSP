"use strict";
/* --------------------------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License. See License.txt in the project root for license information.
 * ------------------------------------------------------------------------------------------ */
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const path = require("path");
const fs = require("fs");
const vscode_1 = require("vscode");
const node_1 = require("vscode-languageclient/node");
const targetSectionSupport = require("../../server/out/targetSectionSupport");
const formNameCompletion = require("../../server/out/formNameCompletion");
let client;
// let cachedObjectTypePatterns = null;
let applyingCleanup = false;


//FUNCTIONS:-----------------------------------------------------------

// Function to get object type patterns (with caching)
// async function getObjectTypePatterns() {
//     if (!cachedObjectTypePatterns) {
//         cachedObjectTypePatterns = await client.sendRequest('custom/getObjectTypePatterns');
//     }
//     return cachedObjectTypePatterns;
// }

//helper function to extract get command info (client-side)
function extractGetCommandInfo(lineText) {
    // Match patterns for various get_* commands
    const patterns = [
        { command: 'get_obj_param', regex: /get_obj_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_material_param', regex: /get_material_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_area_param', regex: /get_area_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_pnt_param', regex: /get_pnt_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_sheet_param', regex: /get_sheet_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_joint_param', regex: /get_joint_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_section_param', regex: /get_section_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'set_obj_param', regex: /set_obj_param\s*\(\s*([^,\s)]+)\s*\)\s*$/ },
        { command: 'set_joint_param', regex: /set_joint_param\s*\(\s*([^,\s)]+)\s*\)\s*$/ },
        { command: 'set_djparam', regex: /set_djparam\s*\(\s*([^,\s)]+)\s*\)\s*$/ },
    ];
    
    for (const pattern of patterns) {
        const match = lineText.match(pattern.regex);
        if (match) {
            return {
                command: pattern.command,
                variableName: match[1].trim()
            };
        }
    }
    
    return null;
}

// Enhanced object type detection (client-side)
// function detectObjectType(variableName, allVariables) {
//     if (!variableName) return 'generic';
    
//     // Define object type keywords
//     const objectTypePatterns = getObjectTypePatterns();
    

//     // Check variable comments for additional context
//     const matchingVariables = allVariables.filter(variable => 
//         variable[0] && variable[0] === variableName
//     );

//     //make a reversed array of matchingVariables
//     const reversedMatchingVariables = matchingVariables.reverse();

//     //check if variable is already made into an object type by the last instance of the variable in the allVariables array that has a non generic type for the variable name.
//     for (const variable of reversedMatchingVariables) {
//         if (variable[2] && variable[2] !== 'generic') {
//             return variable[2];
//         }
//     }

//     //check if lowerComment contains any of the objectTypePatterns keys inside []
//     for (const variable of matchingVariables) {
//         const varComment = variable[1] ? variable[1].toLowerCase() : '';
//         if (varComment.includes('[')) {
//             let comment = varComment.split('[');
//             comment = comment[1].split(']')[0];
//             if (comment) {
//                 for (const [objectType, patterns] of Object.entries(objectTypePatterns)) {
//                     if (objectType == comment) {
//                         return objectType;
//                     }
//                 }
//             }
//         }
//     }
    
//     for (const variable of matchingVariables) {
//         const varComment = variable[1] ? variable[1].toLowerCase() : '';
//         for (const [objectType, patterns] of Object.entries(objectTypePatterns)) {
//             if (patterns.some(pattern => varComment.includes(pattern))) {
//                 return objectType;
//             }
//         }
//     }
    
//     return 'generic';
// }


function isStarBannerLine(line) {
    const trimmed = line.trimStart();
    if (!trimmed.startsWith('!')) {
        return false;
    }
    const stars = trimmed.match(/\*/g);
    return !!stars && stars.length >= 10;
}

function findRemovableHeaderBlock(lines) {
    let start = 0;
    while (start < lines.length && lines[start].trim() === '') {
        start++;
    }
    if (start >= lines.length || !isStarBannerLine(lines[start])) {
        return null;
    }

    const openerIndex = start;
    let closerIndex = -1;
    for (let i = openerIndex + 1; i < lines.length; i++) {
        if (isStarBannerLine(lines[i])) {
            closerIndex = i;
            break;
        }
    }
    if (closerIndex === -1) {
        return null;
    }

    for (let i = openerIndex + 1; i < closerIndex; i++) {
        const trimmed = lines[i].trim();
        if (trimmed === '' || !lines[i].trimStart().startsWith('!')) {
            return null;
        }
    }

    return { start: openerIndex, end: closerIndex };
}

function formatHeaderDate(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    return `${year}-${month}-${day}`;
}

function buildFileHeader(fileStem, date, username) {
    const dateText = typeof date === 'string' ? date : formatHeaderDate(date);
    const author = username || '';
    return [
        '!***************************************************************',
        `! ${fileStem} / ${dateText} / ${author}`,
        '! ',
        '!***************************************************************',
    ].join('\n');
}

function splitDocumentLines(text) {
    if (text.length === 0) {
        return [];
    }
    return text.split(/\r?\n/);
}

function detectDocumentEol(text) {
    return text.includes('\r\n') ? '\r\n' : '\n';
}

function applyHeaderToDocumentText(text, headerBlock) {
    const eol = detectDocumentEol(text);
    const lines = splitDocumentLines(text);
    const removable = findRemovableHeaderBlock(lines);

    if (removable) {
        const before = lines.slice(0, removable.start);
        const after = lines.slice(removable.end + 1);
        const parts = [];
        if (before.length > 0) {
            parts.push(before.join(eol));
        }
        parts.push(headerBlock);
        if (after.length > 0) {
            parts.push(after.join(eol));
        }
        return {
            text: parts.join(eol),
            mode: 'replaced',
        };
    }

    if (text.length === 0) {
        return {
            text: headerBlock,
            mode: 'created',
        };
    }

    return {
        text: `${headerBlock}${eol}${text}`,
        mode: 'created',
    };
}

function getFileStemFromUri(uri) {
    const baseName = path.basename(uri.fsPath, path.extname(uri.fsPath));
    return baseName.toUpperCase();
}


function getLanguageServerImplementation() {
    // Launch configs can force an implementation without touching settings.
    const override = process.env.VERTEXADT_LS_IMPLEMENTATION;
    if (override === 'native' || override === 'node') {
        return override;
    }
    return vscode_1.workspace.getConfiguration('vertexadt.languageServer').get('implementation') === 'native'
        ? 'native'
        : 'node';
}

async function sendHostCommand(commandId, customMethod, args) {
    if (!client) {
        throw new Error('The Vertex ADT language server is not running.');
    }
    if (getLanguageServerImplementation() === 'native') {
        return client.sendRequest('workspace/executeCommand', {
            command: commandId,
            arguments: args === undefined ? [] : [args]
        });
    }
    if (args === undefined) {
        return client.sendRequest(customMethod);
    }
    return client.sendRequest(customMethod, args);
}

function getExtensionSettings() {
    return {
        maxNumberOfProblems: vscode_1.workspace.getConfiguration('vertexadt.diagnostics').get('maxNumberOfProblems'),
        diagnostics: {
            exportCustomOnly: vscode_1.workspace.getConfiguration('vertexadt.diagnostics').get('exportCustomOnly'),
            warnUnclosedResources: vscode_1.workspace.getConfiguration('vertexadt.diagnostics').get('warnUnclosedResources'),
        },
        useExpandedSyntax: vscode_1.workspace.getConfiguration('vertexadt.snippets').get('useExpandedSyntax'),
        noteUnusedVariables: vscode_1.workspace.getConfiguration('vertexadt.variables').get('noteUnusedVariables'),
        warnVectorToValueTransition: vscode_1.workspace.getConfiguration('vertexadt.variables').get('warnVectorToValueTransition'),
        semanticHighlighting: vscode_1.workspace.getConfiguration('vertexadt.semanticHighlighting').get('enabled'),
        strictMode: vscode_1.workspace.getConfiguration('vertexadt.strictMode').get('enabled'),
        debugValidationCommands: vscode_1.workspace.getConfiguration('vertexadt.debug').get('validationCommands'),
        inferIncludedFrom: vscode_1.workspace.getConfiguration('vertexadt').get('inferIncludedFrom'),
        impactAnalysis: {
            enabled: vscode_1.workspace.getConfiguration('vertexadt.impactAnalysis').get('enabled'),
            minIncomingCount: vscode_1.workspace.getConfiguration('vertexadt.impactAnalysis').get('minIncomingCount'),
        },
        calls: {
            validateParameterCount: vscode_1.workspace.getConfiguration('vertexadt.calls').get('validateParameterCount'),
        },
        locals: {
            validateUniqueNames: vscode_1.workspace.getConfiguration('vertexadt.locals').get('validateUniqueNames'),
            requireLNamesInCallees: vscode_1.workspace.getConfiguration('vertexadt.locals').get('requireLNamesInCallees'),
            warnOnRedeclaration: vscode_1.workspace.getConfiguration('vertexadt.locals').get('warnOnRedeclaration'),
        },
    };
}

async function showImpactAnalysisQuickPick(payload) {
    if (!payload || !payload.analysis) {
        return;
    }

    const { uri, analysis } = payload;
    const items = [];

    if (analysis.incoming.references.length > 0) {
        items.push({
            label: 'Direct impact (calls / includes)',
            kind: vscode_1.QuickPickItemKind.Separator,
        });
        for (const ref of analysis.incoming.references) {
            const typeLabel = ref.type === 'call' ? 'call' : 'include';
            items.push({
                label: `${ref.fromDisplayPath}:${ref.line + 1} ${typeLabel}`,
                description: ref.lineText,
                detail: ref.type === 'call' ? 'Direct caller' : 'Direct includer',
                ref,
            });
        }
    }

    if (analysis.scopes.length > 0) {
        items.push({
            label: 'Blast radius by program',
            kind: vscode_1.QuickPickItemKind.Separator,
        });
        for (const scope of analysis.scopes) {
            items.push({
                label: scope.rootDisplayPath,
                description: `${scope.fileCount} file(s) in blast radius`,
                detail: 'Scoped program root',
                scope,
            });
        }
    }

    if (items.length === 0) {
        vscode_1.window.showInformationMessage('No impact or blast radius data for this file.');
        return;
    }

    const picked = await vscode_1.window.showQuickPick(items, {
        title: 'Impact and Blast Radius',
        placeHolder: 'Select a caller/include or blast-radius program scope',
        matchOnDescription: true,
        matchOnDetail: true,
    });

    if (!picked || picked.kind === vscode_1.QuickPickItemKind.Separator) {
        return;
    }

    if (picked.ref) {
        const targetUri = vscode_1.Uri.parse(picked.ref.fromUri);
        const document = await vscode_1.workspace.openTextDocument(targetUri);
        const editor = await vscode_1.window.showTextDocument(document, { preview: false });
        const position = new vscode_1.Position(picked.ref.line, 0);
        editor.selection = new vscode_1.Selection(position, position);
        editor.revealRange(new vscode_1.Range(position, position), vscode_1.TextEditorRevealType.InCenter);
        return;
    }

    if (picked.scope) {
        const scopeItems = picked.scope.files.map((file) => ({
            label: file.displayPath,
            description: file.uri,
            file,
        }));
        scopeItems.unshift({
            label: `Program: ${picked.scope.rootDisplayPath}`,
            kind: vscode_1.QuickPickItemKind.Separator,
        });

        const scopePick = await vscode_1.window.showQuickPick(scopeItems, {
            title: `Blast radius: ${picked.scope.rootDisplayPath}`,
            placeHolder: 'Open a file in this program scope',
        });

        if (!scopePick || scopePick.kind === vscode_1.QuickPickItemKind.Separator || !scopePick.file) {
            return;
        }

        const targetUri = vscode_1.Uri.parse(scopePick.file.uri);
        const document = await vscode_1.workspace.openTextDocument(targetUri);
        await vscode_1.window.showTextDocument(document, { preview: false });
    }
}

function stripOuterQuotes(value) {
    const trimmed = value.trim();
    if ((trimmed.startsWith('"') && trimmed.endsWith('"')) || (trimmed.startsWith("'") && trimmed.endsWith("'"))) {
        return trimmed.slice(1, -1);
    }
    return trimmed;
}

function getEnclosingSectionName(document, debugLine) {
    const sectionRegex = /#([A-Za-z0-9_]+)#/g;
    let previousSection = null;
    let nextSection = null;

    // Find the nearest section marker before the debug line.
    for (let line = debugLine - 1; line >= 0; line--) {
        const text = document.lineAt(line).text;
        sectionRegex.lastIndex = 0;

        let match;
        while ((match = sectionRegex.exec(text)) !== null) {
            previousSection = match[1];
        }

        if (previousSection) {
            break;
        }
    }

    // Find the nearest section marker after the debug line.
    for (let line = debugLine + 1; line < document.lineCount; line++) {
        const text = document.lineAt(line).text;
        sectionRegex.lastIndex = 0;

        const match = sectionRegex.exec(text);
        if (match) {
            nextSection = match[1];
            break;
        }
    }

    if (!previousSection || !nextSection) {
        return null;
    }

    return previousSection.toLowerCase() === nextSection.toLowerCase()
        ? previousSection
        : null;
}

async function resolveVertexPath(rawPath, document) {
    if (!rawPath) {
        return null;
    }

    const cleanedPath = stripOuterQuotes(rawPath);
    if (!cleanedPath || cleanedPath.includes('${') || cleanedPath.includes('%')) {
        return null;
    }

    if (cleanedPath.startsWith('$') && client) {
        try {
            const resolvedFromServer = await client.sendRequest('custom/resolveIncludePath', {
                filePath: cleanedPath,
                uri: document.uri.toString()
            });
            if (resolvedFromServer) {
                return vscode_1.Uri.file(resolvedFromServer);
            }
        }
        catch (error) {
            // Fall back to local path probing if server-side resolution fails.
        }
    }

    const normalizedInput = cleanedPath.replace(/\\/g, path.sep).replace(/\//g, path.sep);
    const candidatePaths = [];

    if (path.isAbsolute(normalizedInput)) {
        candidatePaths.push(path.normalize(normalizedInput));
    }

    const documentDir = path.dirname(document.uri.fsPath);
    candidatePaths.push(path.normalize(path.join(documentDir, normalizedInput)));

    if (vscode_1.workspace.workspaceFolders) {
        for (const folder of vscode_1.workspace.workspaceFolders) {
            candidatePaths.push(path.normalize(path.join(folder.uri.fsPath, normalizedInput)));
        }
    }

    for (const candidate of candidatePaths) {
        if (fs.existsSync(candidate)) {
            return vscode_1.Uri.file(candidate);
        }
    }

    return null;
}

function getUserWorkspaceFolder() {
    if (!vscode_1.workspace.workspaceFolders) {
        return null;
    }

    return vscode_1.workspace.workspaceFolders.find(folder => {
        const workspaceName = typeof folder.name === 'string' ? folder.name.trim().toLowerCase() : '';
        const baseName = path.basename(folder.uri.fsPath).trim().toLowerCase();
        return workspaceName.includes('user') || baseName.includes('user');
    }) ?? null;
}

function resolveDebugCommandPath(rawPath) {
    if (!rawPath) {
        return null;
    }

    const userWorkspaceFolder = getUserWorkspaceFolder();
    if (!userWorkspaceFolder) {
        return null;
    }

    const cleanedPath = stripOuterQuotes(rawPath);
    if (!cleanedPath || cleanedPath.includes('${') || cleanedPath.includes('%') || !cleanedPath.startsWith('./')) {
        return null;
    }

    const relativePath = cleanedPath.slice(2).trim();
    if (!relativePath) {
        return null;
    }

    const normalizedRelativePath = path.normalize(relativePath.replace(/\\/g, path.sep).replace(/\//g, path.sep));
    if (path.isAbsolute(normalizedRelativePath)) {
        return null;
    }

    const targetPath = path.normalize(path.join(userWorkspaceFolder.uri.fsPath, normalizedRelativePath));
    const relativeToRoot = path.relative(userWorkspaceFolder.uri.fsPath, targetPath);
    if (!relativeToRoot || relativeToRoot.startsWith('..') || path.isAbsolute(relativeToRoot)) {
        return null;
    }

    if (!fs.existsSync(targetPath)) {
        return null;
    }

    return vscode_1.Uri.file(targetPath);
}

    function resolveSectionTargetUri(targetUri, rawSectionName, sectionLineCache = new Map()) {
        if (!targetUri || !rawSectionName) {
            return targetUri;
        }

        const sectionName = rawSectionName.trim();
        if (!sectionName) {
            return targetUri;
        }

        const cacheKey = `${targetUri.fsPath}::${sectionName.toLowerCase()}`;
        let sectionLine = sectionLineCache.get(cacheKey);

        if (sectionLine === undefined) {
            sectionLine = null;
            try {
                const marker = `#${sectionName}#`.toLowerCase();
                const fileContent = fs.readFileSync(targetUri.fsPath, 'utf8');
                const lines = fileContent.split(/\r?\n/);
                for (let i = 0; i < lines.length; i++) {
                    if (lines[i].toLowerCase().includes(marker)) {
                        sectionLine = i;
                        break;
                    }
                }
            }
            catch (error) {
                sectionLine = null;
            }

            sectionLineCache.set(cacheKey, sectionLine);
        }

        if (sectionLine !== null && sectionLine >= 0) {
            return targetUri.with({ fragment: `L${sectionLine + 1}` });
        }

        return targetUri;
    }

async function buildPathDocumentLinks(document) {
    const links = [];
    const pathRanges = [];
    const sectionLineCache = new Map();

    const patterns = [
        /##included_from\s+((?:"[^"]+"|'[^']+'|[^\s#;]+))/g,
        /#\s*include\s+((?:"[^"]+"|'[^']+'|[^\s#;]+))/g,
        /\b\w+\$?\s*=\s*call\(\s*((?:"[^"]+"|'[^']+'|[^,\)\s]+))/g,
        /\bcall\(\s*((?:"[^"]+"|'[^']+'|[^,\)\s]+))/g,
        /\bcall\s+((?:"[^"]+"|'[^']+'|[^\s,]+))/g
    ];

    for (let lineIndex = 0; lineIndex < document.lineCount; lineIndex++) {
        const lineText = document.lineAt(lineIndex).text;

        for (const pattern of patterns) {
            pattern.lastIndex = 0;
            let match;

            while ((match = pattern.exec(lineText)) !== null) {
                const rawPath = match[1];
                const rawStart = match.index + match[0].lastIndexOf(rawPath);

                // For include directives, support optional section token after path.
                let sectionName = null;
                const afterPath = lineText.substring(rawStart + rawPath.length).trim();
                if (/^#\s*include\b/i.test(lineText.trimStart()) && afterPath.length > 0) {
                    const sectionMatch = afterPath.match(/^([A-Za-z_][A-Za-z0-9_]*)\b/);
                    if (sectionMatch) {
                        sectionName = sectionMatch[1];
                    }
                }

                pathRanges.push({ lineIndex, rawPath, rawStart, sectionName });
            }
        }
    }

    const uniquePaths = [...new Set(pathRanges.map(item => item.rawPath))];
    const resolvedByPath = new Map();

    await Promise.all(uniquePaths.map(async (rawPath) => {
        const resolvedUri = await resolveVertexPath(rawPath, document);
        if (resolvedUri) {
            resolvedByPath.set(rawPath, resolvedUri);
        }
    }));

    for (const item of pathRanges) {
        const resolvedUri = resolvedByPath.get(item.rawPath);
        if (!resolvedUri) {
            continue;
        }

        let targetUri = resolvedUri;
        if (item.sectionName) {
            targetUri = resolveSectionTargetUri(resolvedUri, item.sectionName, sectionLineCache);
        }

        const start = new vscode_1.Position(item.lineIndex, item.rawStart);
        const end = new vscode_1.Position(item.lineIndex, item.rawStart + item.rawPath.length);
        const link = new vscode_1.DocumentLink(new vscode_1.Range(start, end), targetUri);
        link.tooltip = `Open ${resolvedUri.fsPath}`;
        links.push(link);
    }

    return links;
}

async function buildDebugHeaderLinks(document) {
    if (document.lineCount === 0) {
        return [];
    }

    const firstLine = document.lineAt(0).text;
    const debugHeaderMatch = firstLine.match(/START OF DEBUG(?:\s+in\s+SECTION\s+#([^#]+)#)?\s+in\s+FILE\s+"([^"]+)"/i);
    if (!debugHeaderMatch) {
        return [];
    }

    const sectionName = debugHeaderMatch[1] ? debugHeaderMatch[1].trim() : null;
    const rawPath = debugHeaderMatch[2].trim();
    if (!rawPath) {
        return [];
    }

    let targetUri = await resolveVertexPath(rawPath, document);
    if (!targetUri && !rawPath.startsWith('$')) {
        const normalizedPath = path.normalize(rawPath.replace(/\\/g, path.sep).replace(/\//g, path.sep));
        targetUri = vscode_1.Uri.file(normalizedPath);
    }

    if (!targetUri) {
        return [];
    }

    if (sectionName) {
        targetUri = resolveSectionTargetUri(targetUri, sectionName);
    }

    const rawStart = firstLine.indexOf(rawPath);
    if (rawStart < 0) {
        return [];
    }

    const start = new vscode_1.Position(0, rawStart);
    const end = new vscode_1.Position(0, rawStart + rawPath.length);
    const link = new vscode_1.DocumentLink(new vscode_1.Range(start, end), targetUri);
    link.tooltip = `Open ${targetUri.fsPath}`;
    return [link];
}

function buildDebugCommandLinks(document) {
    const userWorkspaceFolder = getUserWorkspaceFolder();
    if (!userWorkspaceFolder) {
        return [];
    }

    const links = [];
    const debugPattern = /^\s*debug\b\s+((?:"\.\/[^\"]+"|'\.\/[^']+'|\.\/[^\s#;]+))/i;

    for (let lineIndex = 0; lineIndex < document.lineCount; lineIndex++) {
        const lineText = document.lineAt(lineIndex).text;
        const match = lineText.match(debugPattern);
        if (!match) {
            continue;
        }

        const rawPath = match[1];
        const targetUri = resolveDebugCommandPath(rawPath);
        if (!targetUri) {
            continue;
        }

        const rawStart = lineText.indexOf(rawPath, match.index);
        if (rawStart < 0) {
            continue;
        }

        const start = new vscode_1.Position(lineIndex, rawStart);
        const end = new vscode_1.Position(lineIndex, rawStart + rawPath.length);
        const link = new vscode_1.DocumentLink(new vscode_1.Range(start, end), targetUri);
        link.tooltip = `Open ${targetUri.fsPath}`;
        links.push(link);
    }

    return links;
}

async function provideAllDocumentLinks(document) {
    const [pathLinks, debugLinks, debugCommandLinks] = await Promise.all([
        buildPathDocumentLinks(document),
        buildDebugHeaderLinks(document),
        Promise.resolve(buildDebugCommandLinks(document))
    ]);

    return [...pathLinks, ...debugLinks, ...debugCommandLinks];
}

function getHoverInformation(word, keywords, hovervariables, hoverstringvariables) {
    let hoverInformation = '';
    // Loop through keywords and check if the word is a keyword by comparing it to labels in the keywords array
    for (let i = 0; i < keywords.length; i++) {
        if (word === keywords[i].label) {
            // Change the hover information to include the line breaks, do the same for documentation
            hoverInformation = `${keywords[i].documentation.replace(/\n/g, '\n')}\n\n---\n${keywords[i].detail.replace(/\n/g, '\n')}`;
            return hoverInformation;
        }
    }

    // Loop through hovervariables and check if the word matches
    for (let i = 0; i < hovervariables.length; i++) {
        if (word === hovervariables[i][0] && hovervariables[i][1]) {
            // Get the semantic type
            //turned off as not working yet due to poor server side code.const objectType = hovervariables[i][2] || 'generic';
            //turned off as not working yet due to poor server side code. const variableTypeLabel = objectType === 'generic' ? 'Variable' : `${objectType.charAt(0).toUpperCase() + objectType.slice(1)}-Variable`;
            const objectType = 'generic';
            const variableTypeLabel = objectType === 'generic' ? 'Variable' : `${objectType.charAt(0).toUpperCase() + objectType.slice(1)}-Variable`;
            hoverInformation = `${variableTypeLabel}:
\`\`\`vertexadt 
${hovervariables[i][0]}
\`\`\`\n
${hovervariables[i][1]}`;
            return hoverInformation;
        }
    }

    // Loop through hoverstringvariables and check if the word matches
    for (let i = 0; i < hoverstringvariables.length; i++) {
        if (word === hoverstringvariables[i][0] && hoverstringvariables[i][1]) {
            const objectType = hoverstringvariables[i][2] || 'generic';
            const variableTypeLabel = objectType === 'generic' ? 'String Variable' : `${objectType.charAt(0).toUpperCase() + objectType.slice(1)}-Variable`;
            hoverInformation = `${variableTypeLabel}:
\`\`\`vertexadt 
${hoverstringvariables[i][0]}
\`\`\`\n
${hoverstringvariables[i][1]}`;
            return hoverInformation;
        }
    }

    return hoverInformation;
}

function stripDbcreateComment(text) {
    if (!text) {
        return '';
    }
    return text.includes(';!') ? text.split(';!')[0] : text;
}

function isDbcreateContinuation(text) {
    return stripDbcreateComment(text).trimEnd().endsWith('\\');
}

function trimDbcreateContinuation(text) {
    return stripDbcreateComment(text).replace(/\s*\\\s*$/, '').trim();
}

function getDbcreateAfterParamsContext(afterParamsText) {
    let inString = null;
    let commaCount = 0;
    let fieldTextStart = -1;

    for (let i = 0; i < afterParamsText.length; i++) {
        const char = afterParamsText[i];
        if (inString) {
            if (char === inString) {
                inString = null;
            }
            continue;
        }

        if (char === '"' || char === "'") {
            inString = char;
            continue;
        }

        if (char === ',') {
            commaCount++;
            if (commaCount === 3) {
                fieldTextStart = i + 1;
                break;
            }
        }
    }

    return {
        commaCount,
        fieldText: fieldTextStart >= 0 ? afterParamsText.slice(fieldTextStart) : ''
    };
}

function getDbcreateCompletionContext(document, position) {
    let startLine = position.line;

    while (startLine > 0) {
        const previousLine = document.lineAt(startLine - 1).text;
        if (!isDbcreateContinuation(previousLine)) {
            break;
        }
        startLine--;
    }

    const startLineText = document.lineAt(startLine).text;
    if (!/\bdbcreate\s*\(/i.test(stripDbcreateComment(startLineText))) {
        return null;
    }

    const logicalParts = [];
    for (let lineIndex = startLine; lineIndex <= position.line; lineIndex++) {
        let lineText = document.lineAt(lineIndex).text;
        if (lineIndex === position.line) {
            lineText = lineText.substring(0, position.character);
        }

        const trimmedPart = trimDbcreateContinuation(lineText);
        if (trimmedPart) {
            logicalParts.push(trimmedPart);
        }
    }

    const logicalPrefix = logicalParts.join(' ').trim();
    const commandMatch = logicalPrefix.match(/\bdbcreate\s*\([^)]*\)\s*(.*)$/i);
    if (!commandMatch) {
        return null;
    }

    const afterParamsContext = getDbcreateAfterParamsContext(commandMatch[1] || '');
    if (afterParamsContext.commaCount < 3) {
        return null;
    }

    const normalizedFieldText = afterParamsContext.fieldText.replace(/\s*\\\s*/g, ' ');
    const hasTrailingWhitespace = /\s$/.test(normalizedFieldText);
    const trimmedFieldText = normalizedFieldText.trim();
    const fieldTokens = trimmedFieldText ? trimmedFieldText.split(/\s+/) : [];
    const currentToken = !fieldTokens.length || hasTrailingWhitespace
        ? ''
        : fieldTokens[fieldTokens.length - 1];

    const tokenSlot = !fieldTokens.length
        ? 0
        : (hasTrailingWhitespace ? fieldTokens.length % 4 : (fieldTokens.length) % 4);

    return {
        tokenSlot,
        currentToken,
        isMultiline: startLine !== position.line || isDbcreateContinuation(startLineText)
    };
}

function buildDbcreateCompletionItems(dbcreateContext, position) {
    const items = [];
    const replaceRange = new vscode_1.Range(
        new vscode_1.Position(position.line, Math.max(0, position.character - dbcreateContext.currentToken.length)),
        position
    );

    const addItem = (item) => {
        items.push(item);
    };

    if (dbcreateContext.tokenSlot === 0) {
        const fieldTypes = [
            { label: 'FIELDNAME', detail: 'wanted fielname - case sensitive, usually uppercase!'},
        ];

        fieldTypes.forEach((fieldType, index) => {
            const item = new vscode_1.CompletionItem(fieldType.label, vscode_1.CompletionItemKind.EnumMember);
            item.insertText = fieldType.label;
            item.detail = fieldType.detail;
            addItem(item);
        });
    }

    if (dbcreateContext.tokenSlot === 1) {
        const fieldTypes = [
            { label: 'A', detail: 'Alpha field type' },
            { label: 'N', detail: 'Numeric field type' },
            { label: 'B', detail: 'Binary field type' },
            { label: 'T', detail: 'Text field type' },
            { label: 'M', detail: 'Memo field type' },
            { label: 'U', detail: 'Unicode field type' }
        ];

        fieldTypes.forEach((fieldType, index) => {
            const item = new vscode_1.CompletionItem(fieldType.label, vscode_1.CompletionItemKind.EnumMember);
            item.insertText = fieldType.label;
            item.detail = fieldType.detail;
            addItem(item);
        });
    }

    if (dbcreateContext.tokenSlot === 2) {
        ['1', '8', '16', '32', '64', '128', '255'].forEach((lengthValue, index) => {
            const item = new vscode_1.CompletionItem(lengthValue, vscode_1.CompletionItemKind.Value);
            item.insertText = lengthValue;
            item.detail = 'Field length';
            addItem(item);
        });
    }

    if (dbcreateContext.tokenSlot === 3) {
        const indexTypes = [
            { label: 'x', detail: 'No index' },
            { label: 'IND', detail: 'Indexed field' },
            { label: 'UNIQUE', detail: 'Unique indexed field' }
        ];

        indexTypes.forEach((indexType, index) => {
            const item = new vscode_1.CompletionItem(indexType.label, vscode_1.CompletionItemKind.EnumMember);
            item.insertText = indexType.label;
            item.detail = indexType.detail;
            addItem(item);
        });
    }

    //if (dbcreateContext.isMultiline && dbcreateContext.tokenSlot === 0) {
    //    const continuedFieldItem = new vscode_1.CompletionItem('dbcreate field (continued)', vscode_1.CompletionItemKind.Snippet);
    //    continuedFieldItem.insertText = new vscode_1.SnippetString('${1:FIELDNAME} ${2|A,N,B,T,M,U|} ${3:64} ${4|x,IND,UNIQUE|} \\\n${5:NEXTFIELD} ${6|A,N,B,T,M,U|} ${7:64} ${8|x,IND,UNIQUE|}');
    //    continuedFieldItem.detail = 'Add another continued field definition';
    //    continuedFieldItem.documentation = new vscode_1.MarkdownString('Insert two continued `dbcreate` field definitions.');
    //    continuedFieldItem.sortText = '01';
    //    addItem(continuedFieldItem);
    //}

    return items;
}

//END OF FUNCTIONS-----------------------------------------------------------


// MAIN METHOD TO START LSP-SERVER-----------------------------------------------------------

function activate(context) {
    const nativeHost = getLanguageServerImplementation() === 'native';

    context.subscriptions.push(vscode_1.workspace.onDidChangeConfiguration(e => {
        if (e.affectsConfiguration('vertexadt.languageServer.implementation')) {
            vscode_1.window.showInformationMessage(
                'Reload the window to apply the language-server implementation setting.',
                'Reload Window'
            ).then((choice) => {
                if (choice === 'Reload Window') {
                    vscode_1.commands.executeCommand('workbench.action.reloadWindow');
                }
            });
        }
    }));

    let serverOptions;
    let clientOptions;
    if (nativeHost) {
        const command = context.asAbsolutePath(path.join('server', 'vertex-adt-ls.exe'));
        if (!fs.existsSync(command)) {
            vscode_1.window.showErrorMessage(`Vertex ADT language-server executable not found at ${command}.`);
        } else {
            serverOptions = { command };
            clientOptions = {
                documentSelector: [
                    { language: 'vertexadt' },
                    { language: 'plaintext' },
                    { language: 'log' }
                ],
                initializationOptions: {
                    extensionUri: context.extensionUri.toString()
                },
                markdown: { isTrusted: true },
                synchronize: {
                    configurationSection: 'vertexadt'
                }
            };
        }
    } else {
        const serverModule = context.asAbsolutePath(path.join('server', 'out', 'server.js'));
        serverOptions = {
            run: { module: serverModule, transport: node_1.TransportKind.ipc },
            debug: {
                module: serverModule,
                transport: node_1.TransportKind.ipc,
                options: { execArgv: ['--nolazy', '--inspect=6009'] }
            }
        };
        clientOptions = {
            documentSelector: [{ scheme: 'file', language: 'vertexadt' }],
            initializationOptions: {
                workspaceFolders: vscode_1.workspace.workspaceFolders ? vscode_1.workspace.workspaceFolders.map(folder => folder.uri.toString()) : [],
                settings: {
                    settings: getExtensionSettings()
                }
            }
        };
    }

    if (!nativeHost) {
    context.subscriptions.push(vscode_1.languages.registerDocumentLinkProvider(
        [
            { scheme: 'file', language: 'vertexadt' },
            { scheme: 'file', language: 'plaintext' },
            { scheme: 'file', language: 'log' }
        ],
        {
            provideDocumentLinks(document) {
                return provideAllDocumentLinks(document);
            }
        }
    ));

    //cleanup task:
    context.subscriptions.push(
        vscode_1.workspace.onDidChangeTextDocument(async (event) => {
            if (applyingCleanup) return;
            if (event.document.languageId !== 'vertexadt') return;
            const newlineChange = event.contentChanges.find(
                c => c.text === '\n' || c.text === '\r\n'
            );
            if (!newlineChange) return;
            const lineIndex = newlineChange.range.start.line;
            if (lineIndex < 0) return;
            const line = event.document.lineAt(lineIndex).text;
            const cleaned = await client.sendRequest('custom/cleanupLine', {
                uri: event.document.uri.toString(),
                line,
                lineNumber: lineIndex,
            });
            if (!cleaned || cleaned === line) return;
            applyingCleanup = true;
            try {
                const edit = new vscode_1.WorkspaceEdit();
                const range = event.document.lineAt(lineIndex).range;
                edit.replace(event.document.uri, range, cleaned);
                await vscode_1.workspace.applyEdit(edit);
            } finally {
                applyingCleanup = false;
            }
        })
    );


    // Watch for settings changes and notify server
    context.subscriptions.push(vscode_1.workspace.onDidChangeConfiguration(e => {
        if (e.affectsConfiguration('vertexadt')) {
            // Send updated settings to server
            client.sendNotification('custom/updateSettings', {
                maxNumberOfProblems: vscode_1.workspace.getConfiguration('vertexadt.diagnostics').get('maxNumberOfProblems'),
                diagnostics: {
                    exportCustomOnly: vscode_1.workspace.getConfiguration('vertexadt.diagnostics').get('exportCustomOnly'),
                    warnUnclosedResources: vscode_1.workspace.getConfiguration('vertexadt.diagnostics').get('warnUnclosedResources'),
                },
                useExpandedSyntax: vscode_1.workspace.getConfiguration('vertexadt.snippets').get('useExpandedSyntax'),
                noteUnusedVariables: vscode_1.workspace.getConfiguration('vertexadt.variables').get('noteUnusedVariables'),
                warnVectorToValueTransition: vscode_1.workspace.getConfiguration('vertexadt.variables').get('warnVectorToValueTransition'),
                semanticHighlighting: vscode_1.workspace.getConfiguration('vertexadt.semanticHighlighting').get('enabled'),
                strictMode: vscode_1.workspace.getConfiguration('vertexadt.strictMode').get('enabled'),
                debugValidationCommands: vscode_1.workspace.getConfiguration('vertexadt.debug').get('validationCommands'),
                impactAnalysis: {
                    enabled: vscode_1.workspace.getConfiguration('vertexadt.impactAnalysis').get('enabled'),
                    minIncomingCount: vscode_1.workspace.getConfiguration('vertexadt.impactAnalysis').get('minIncomingCount'),
                },
                calls: {
                    validateParameterCount: vscode_1.workspace.getConfiguration('vertexadt.calls').get('validateParameterCount'),
                },
                locals: {
                    validateUniqueNames: vscode_1.workspace.getConfiguration('vertexadt.locals').get('validateUniqueNames'),
                    requireLNamesInCallees: vscode_1.workspace.getConfiguration('vertexadt.locals').get('requireLNamesInCallees'),
                    warnOnRedeclaration: vscode_1.workspace.getConfiguration('vertexadt.locals').get('warnOnRedeclaration'),
                },
            });
        }
    }));
    }

    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.openSettings', () => {
        vscode_1.commands.executeCommand('workbench.action.openSettings', 'vertexadt');
    }));

    // Register settings change listener
    context.subscriptions.push(vscode_1.workspace.onDidChangeConfiguration(e => {
        if (e.affectsConfiguration('vertexadt')) {
            // Update any behavior that depends on settings
            const settings = getExtensionSettings();
            // You can pass these settings to appropriate functions or update state
        }
    }));


    // Create the language client and start the client.
    if (serverOptions && clientOptions) {
        client = new node_1.LanguageClient('languageServerExample', 'Vertex-adt LanguageServer', serverOptions, clientOptions);
    }

    if (!nativeHost && client) {
    const notifyActiveDocumentChanged = (editor) => {
        if (!editor) {
            return;
        }

        const { document } = editor;
        if (document.languageId !== 'vertexadt' || document.uri.scheme !== 'file') {
            return;
        }

        client.sendNotification('custom/activeDocumentChanged', {
            uri: document.uri.toString(),
            version: document.version
        });
    };

    context.subscriptions.push(vscode_1.window.onDidChangeActiveTextEditor(editor => {
        notifyActiveDocumentChanged(editor);
    }));

    context.subscriptions.push(vscode_1.workspace.onDidOpenTextDocument(document => {
        if (document.languageId !== 'vertexadt' || document.uri.scheme !== 'file') {
            return;
        }

        client.sendNotification('custom/activeDocumentChanged', {
            uri: document.uri.toString(),
            version: document.version
        });
    }));

    Promise.all([
        client.sendRequest('custom/getKeywords')
    ]).then(([keywords]) => {

        // Register a provider for hover
        context.subscriptions.push(vscode_1.languages.registerHoverProvider({ scheme: 'file', language: 'vertexadt' }, {
            provideHover(document, position, token) {
                const wordRange = document.getWordRangeAtPosition(position, /\w+\$?/);
                if (!wordRange) {
                    return null;
                }

                const word = document.getText(wordRange);
                const lowerWord = word.toLowerCase();
                const debugSkipKeywords = new Set(keywords.map(keyword => keyword.label.toLowerCase()));

                const tryDebugValueHover = (word.length >= 2 && !debugSkipKeywords.has(lowerWord))
                    ? client.sendRequest('custom/getDebugHoverValue', {
                        uri: document.uri.toString(),
                        line: position.line,
                        varName: word
                    })
                    : Promise.resolve(null);

                return tryDebugValueHover.then((debugHover) => {
                    if (debugHover && debugHover.value !== undefined) {
                        const md = new vscode_1.MarkdownString();
                        md.isTrusted = true;
                        if (debugHover.isSameLine) {
                            md.appendMarkdown(`**${word}** = \`${debugHover.value}\``);
                        }
                        else {
                            md.appendMarkdown(`**${word}** = \`${debugHover.value}\`\n\n`);
                            md.appendMarkdown(`_Set on line ${debugHover.line + 1}_`);
                        }
                        return new vscode_1.Hover(md, wordRange);
                    }

                    // Send the document content to the server to get hover variables and string variables
                    return client.sendRequest('custom/getextractedvariables', { text: document.getText() }).then(({ hovervariables, hoverstringvariables }) => {
                    // Implement your hover logic here
                    let hoverResult = getHoverInformation(word, keywords, hovervariables, hoverstringvariables); // Pass additional parameters
    
                    let markdownString;
    
                    switch (word) {
                        case 'pnt_inside_poly': {
                            const imageUri = vscode_1.Uri.joinPath(context.extensionUri, 'ADT-documentation', 'pnt_inside_poly explainer.png');
                            markdownString = new vscode_1.MarkdownString(`![Description](${imageUri.toString()})\n\n${hoverResult}`);
                            markdownString.isTrusted = true;
                            break;
                        }
                        case 'is_point_inside': {
                            const imageUri = vscode_1.Uri.joinPath(context.extensionUri, 'ADT-documentation', 'is_point_inside explainer.png');
                            markdownString = new vscode_1.MarkdownString(`![Description](${imageUri.toString()})\n\n${hoverResult}`);
                            markdownString.isTrusted = true;
                            break;
                        }
                        case 'vec_intersect': {
                            const imageUri = vscode_1.Uri.joinPath(context.extensionUri, 'ADT-documentation', 'vec_intersect explainer.png');
                            markdownString = new vscode_1.MarkdownString(`![Description](${imageUri.toString()})\n\n${hoverResult}`);
                            markdownString.isTrusted = true;
                            break;
                        }
                        case 'vxgeom_cross_pnts': {
                            const imageUri = vscode_1.Uri.joinPath(context.extensionUri, 'ADT-documentation', 'vxgeom_cross_pnts explainer.png');
                            markdownString = new vscode_1.MarkdownString(`![Description](${imageUri.toString()})\n\n${hoverResult}`);
                            markdownString.isTrusted = true;
                            break;
                        }
                        case 'is_in_vxgeom': {
                            const imageUri = vscode_1.Uri.joinPath(context.extensionUri, 'ADT-documentation', 'is_in_vxgeom explainer.png');
                            markdownString = new vscode_1.MarkdownString(`![Description](${imageUri.toString()})\n\n${hoverResult}`);
                            markdownString.isTrusted = true;
                            break;
                        }
                        case 'point_to_vxgeom': {
                            const imageUri = vscode_1.Uri.joinPath(context.extensionUri, 'ADT-documentation', 'point_to_vxgeom explainer.png');
                            markdownString = new vscode_1.MarkdownString(`![Description](${imageUri.toString()})\n\n${hoverResult}`);
                            markdownString.isTrusted = true;
                            break;
                        }
                        case 'pnt_to_plane': {
                            const imageUri = vscode_1.Uri.joinPath(context.extensionUri, 'ADT-documentation', 'pnt_to_plane_explainer.png');
                            markdownString = new vscode_1.MarkdownString(`![Description](${imageUri.toString()})\n\n${hoverResult}`);
                            markdownString.isTrusted = true;
                            break;
                        }
                        default:
                            //remove anything inside [] from hoverResult for variables
                            if (hoverResult.startsWith('Variable:') || hoverResult.startsWith('String Variable:')) {
                                hoverResult = hoverResult.replace(/\[.*?\]/g, '');
                                hoverResult = hoverResult.replace(/\bIGNORE TYPE\b/gi, '').replace(/[ \t]{2,}/g, ' ');
                            }
                            markdownString = new vscode_1.MarkdownString(hoverResult);
                    }
    
                    return new vscode_1.Hover(markdownString);
                    });
                });
            }
        }));

        // Register completion provider
        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                provideCompletionItems(document, position, token, context) {

                    //get settings.
                    const settings = getExtensionSettings();


                    return client.sendRequest('custom/getextractedvariables', { text: document.getText() }).then(({ hovervariables, hoverstringvariables,variables,stringvariables,keywords }) => {
                        const completionItems = [];

                        //remove duplicates from variables and stringvariables.
                        variables = [...new Set(variables)];
                        stringvariables = [...new Set(stringvariables)];
                        //and from hovervariables and hoverstringvariables
                        hovervariables = hovervariables.filter((variable, index, self) =>
                            index === self.findIndex((t) => (
                                t[0] === variable[0]
                            ))
                        );
                        hoverstringvariables = hoverstringvariables.filter((variable, index, self) =>
                            index === self.findIndex((t) => (
                                t[0] === variable[0]
                            ))
                        );

                        // Add if-then-endif snippet
                        if (settings.useExpandedSyntax) {
                        const ifThenSnippet = new vscode_1.CompletionItem('if-structure', vscode_1.CompletionItemKind.Snippet);
                        ifThenSnippet.insertText = new vscode_1.SnippetString('if ${1} then\n\t${2}\nendif');
                        ifThenSnippet.documentation = new vscode_1.MarkdownString('Creates an if-then-endif block structure');
                        ifThenSnippet.detail = 'if condition then ... endif';
                        completionItems.push(ifThenSnippet);
                        };

                        // Add for loop snippet
                        if (settings.useExpandedSyntax) {
                        const LoopSnippet = new vscode_1.CompletionItem('loop-structure', vscode_1.CompletionItemKind.Snippet);
                        LoopSnippet.insertText = new vscode_1.SnippetString('loop ${1} \n\t${2}\nendloop');
                        LoopSnippet.documentation = new vscode_1.MarkdownString('Creates a loop-endloop block structure');
                        LoopSnippet.detail = 'loop condition ... endloop';
                        completionItems.push(LoopSnippet);
                        };

                        // Add variables to completion items
                        hovervariables.forEach(variable => {
                            if (variable[0] && variable[1] && !keywords.some(keyword => keyword.label === variable[0])) {
                                const completionItem = new vscode_1.CompletionItem(variable[0], vscode_1.CompletionItemKind.Variable);
                                completionItem.documentation = new vscode_1.MarkdownString(`Variable:\n\`\`\`vertexadt\n${variable[0]}\n\`\`\`\n\n${variable[1]}`);
                                completionItems.push(completionItem);
                            } else if (variable[0] && !variable[1] && !keywords.some(keyword => keyword.label === variable[0])) {
                                const completionItem = new vscode_1.CompletionItem(variable[0], vscode_1.CompletionItemKind.Variable);
                                completionItems.push(completionItem);
                            }
                        });

                        // Add string variables to completion items
                        hoverstringvariables.forEach(variable => {
                            if (variable[0] && variable[1] && !keywords.some(keyword => keyword.label === variable[0])) {
                                const completionItem = new vscode_1.CompletionItem(variable[0], vscode_1.CompletionItemKind.Text);
                                completionItem.documentation = new vscode_1.MarkdownString(`String Variable:\n\`\`\`vertexadt\n${variable[0]}\n\`\`\`\n\n${variable[1]}`);
                                completionItems.push(completionItem);
                            } else if (variable[0] && !variable[1] && !keywords.some(keyword => keyword.label === variable[0])) {
                                const completionItem = new vscode_1.CompletionItem(variable[0], vscode_1.CompletionItemKind.Text);
                                completionItems.push(completionItem);
                            }
                        });
                        console.log('Variables for completion found:');
                        return completionItems;
                    });
                }
            }
        ));

        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                provideCompletionItems(document, position) {
                    const dbcreateContext = getDbcreateCompletionContext(document, position);
                    if (!dbcreateContext) {
                        return [];
                    }

                    return buildDbcreateCompletionItems(dbcreateContext, position);
                }
            },
            ' ', ','
        ));

        // Suggest debug output file name after typing: debug <space>
        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                provideCompletionItems(document, position) {
                    const linePrefix = document.lineAt(position).text.substring(0, position.character);
                    if (!/^\s*debug\s+$/.test(linePrefix)) {
                        return [];
                    }

                    const currentFileName = path.parse(document.fileName).base;
                    if (!currentFileName) {
                        return [];
                    }

                    const enclosingSection = getEnclosingSectionName(document, position.line);
                    const debugFileName = enclosingSection
                        ? `./_${currentFileName}_${enclosingSection}.dbg`
                        : `./_${currentFileName}.dbg`;
                    const item = new vscode_1.CompletionItem(debugFileName, vscode_1.CompletionItemKind.File);
                    item.insertText = debugFileName;
                    item.detail = enclosingSection
                        ? `Debug output file for current document and section ${enclosingSection}`
                        : 'Debug output file for current document';
                    item.sortText = '0';

                    return [item];
                }
            },
            ' '
        ));


        // Register specialized completion provider for paths in various directives
        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                provideCompletionItems(document, position, _token, completionContext) {
                    const lineText = document.lineAt(position).text;
                    const linePrefix = lineText.substring(0, position.character);
                    const trimmedPrefix = linePrefix.trimStart();
                    const inDirective = trimmedPrefix.startsWith('##included_from') ||
                        trimmedPrefix.startsWith('# include') ||
                        /^call\s*\(/.test(trimmedPrefix) ||
                        /^call\s+/.test(trimmedPrefix) ||
                        /^[A-Za-z][A-Za-z0-9_]*\$?=call\s*\(/.test(trimmedPrefix);
                    if (!inDirective) {
                        return [];
                    }
                    if (targetSectionSupport.isInParenthesizedCallArgumentList(lineText, position.character)) {
                        return [];
                    }

                    return client.sendRequest('custom/getIncludeCallCompletion', {
                        lineText,
                        cursorCharacter: position.character,
                        triggerKind: completionContext ? completionContext.triggerKind : 1,
                        triggerCharacter: completionContext ? completionContext.triggerCharacter : ''
                    }).then(result => {
                        if (!result || result.mode === 'none') {
                            return [];
                        }

                        if (result.mode === 'sections') {
                            const replaceStart = typeof result.replaceStart === 'number'
                                ? result.replaceStart
                                : position.character;
                            const replaceEnd = typeof result.replaceEnd === 'number'
                                ? result.replaceEnd
                                : position.character;
                            const replaceRange = new vscode_1.Range(
                                position.line,
                                replaceStart,
                                position.line,
                                replaceEnd
                            );
                            return (result.sections || []).map(name => {
                                const item = new vscode_1.CompletionItem(name, vscode_1.CompletionItemKind.EnumMember);
                                item.insertText = (result.insertLeadingSpace ? ' ' : '') + name;
                                item.range = replaceRange;
                                item.sortText = name;
                                item.filterText = name;
                                item.detail = 'section';
                                return item;
                            });
                        }

                        const items = [];
                        if (result.dirKeywords && result.dirKeywords.length) {
                            for (const dir of result.dirKeywords) {
                                const item = new vscode_1.CompletionItem(dir, vscode_1.CompletionItemKind.Folder);
                                item.sortText = '0' + dir;
                                items.push(item);
                            }
                            return items;
                        }

                        for (const entry of result.entries || []) {
                            const isDirectory = !!entry.isDirectory;
                            const item = new vscode_1.CompletionItem(
                                entry.name,
                                isDirectory ? vscode_1.CompletionItemKind.Folder : vscode_1.CompletionItemKind.File
                            );
                            item.sortText = isDirectory ? '0' + entry.name : '1' + entry.name;
                            if (isDirectory) {
                                item.insertText = entry.name.endsWith('/') ? entry.name : `${entry.name}/`;
                            }
                            if (!isDirectory && entry.sections && entry.sections.length) {
                                item.detail = `${entry.sections.length} section${entry.sections.length === 1 ? '' : 's'}`;
                                const documentation = new vscode_1.MarkdownString();
                                documentation.appendMarkdown(entry.sections.map(sectionName => `- \`${sectionName}\``).join('\n'));
                                item.documentation = documentation;
                            }
                            items.push(item);
                        }
                        return items;
                    });
                }
            },
            ' ', '(', '/' // Trigger completion after space or opening parenthesis
        ));

        // Register additional completion trigger for the slash character
        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                provideCompletionItems() {
                    // Path and section completion is handled by the main include/call provider.
                    return [];
                }
            },
            '/' // Trigger completion after slash in paths
        ));

        // Register additional completion provider for parameters after comma in call expressions
        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                provideCompletionItems(document, position) {
                    const linePrefix = document.lineAt(position).text.substring(0, position.character);

                    // Match call(...) with or without an assignment, including a section suffix in the first argument.
                    const callWithPathRegex = /(?:[A-Za-z][A-Za-z0-9_]*\$?\s*=\s*)?call\s*\(\s*((?:"[^"]+"|'[^']+'|[^,)]+))(?:,\s*[^,]*)*,\s*$/i;
                    const match = linePrefix.match(callWithPathRegex);

                    if (!match) {
                        return [];
                    }

                    const parsedPath = targetSectionSupport.parseIncludeCallPathAndSection(
                        match[1].trim().replace(/^["']|["']$/g, ''),
                        ''
                    );

                    // Count the number of commas to determine parameter position
                    const commaCount = (linePrefix.match(/,/g) || []).length;

                    // Request parameter information from server based on the file path
                    return client.sendRequest('custom/getCallParameters', {
                        filePath: parsedPath.filePath,
                        targetSection: parsedPath.targetSection
                    }).then(parameters => {
                        if (!parameters || !parameters.length) {
                            return [];
                        }

                        // Adjust for 0-based indexing, commaCount represents parameter position
                        const paramPosition = commaCount - 1;

                        // If we're beyond available parameters, return empty
                        if (paramPosition >= parameters.length) {
                            return [];
                        }

                        // Show all remaining parameters starting from current position
                        return parameters.slice(paramPosition).map((param, index) => {
                            const actualIndex = paramPosition + index;
                            //check if param.variableName contains $ and if so make completionitemkind a string
                            const isString = param.variableName.includes('$');
                            const kind = isString ? vscode_1.CompletionItemKind.Text : vscode_1.CompletionItemKind.Variable;
                            const item = new vscode_1.CompletionItem(
                                param.variableName,
                                kind
                            );

                            item.documentation = `Input Parameter ${actualIndex + 1}: ${param.comment || 'No comment given. See file!'}`;
                            item.detail = param.description || '';

                            // Sort parameters in order
                            item.sortText = index.toString().padStart(2, '0');

                            return item;
                        });
                    });
                }
            },
            ',' // Trigger completion after comma
        ));

        // Register completion provider for lines after a call statement
        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                provideCompletionItems(document, position) {
                    // We only want to provide completions on the line after a call statement
                    if (position.line === 0) {
                        return []; // First line can't be after a call
                    }

                    // Get the previous line's text
                    const prevLine = document.lineAt(position.line - 1).text.trim();

                    // Check if the previous line contains a call assignment like Ret$=call(...)
                    const callRegex = /(\w+\$)\s*=\s*call\(\s*("[^"]+"|'[^']+'|[^,)]+)(?:,\s*[^)]*)*\)/;
                    const match = prevLine.match(callRegex);

                    if (!match) {
                        return []; // Previous line doesn't contain a call
                    }

                    // Extract the return variable name and file path
                    const returnVar = match[1]; // This will be something like "Ret$"
                    let filePath = targetSectionSupport.lookupPathFromCallArgument(
                        match[2].replace(/^["']|["']$/g, '')
                    );

                    // Request return value information from server based on the file path
                    return client.sendRequest('custom/getCallReturnValues', { 
                        filePath: filePath 
                    }).then(returnValues => {
                        if (!returnValues || !returnValues.length) {
                            return [];
                        }

                        // Transform the server response into completion items
                        return returnValues.map(value => {
                            // Replace generic "Ret$" with the actual variable name from the call
                            let label = "return:"+ value.label.replace('Ret$', returnVar);
                            //strip away everything after = if = present
                            if (label.includes('=')) {
                                label = label.split('=')[0];
                            }
                            

                            const item = new vscode_1.CompletionItem(
                                label,
                                value.kind === "Snippet" ? vscode_1.CompletionItemKind.Snippet : vscode_1.CompletionItemKind.Value
                            );

                            item.documentation = new vscode_1.MarkdownString(value.detail);
                            //item.detail = value.detail;

                            // Insert the text as is
                            item.insertText = "\n"+value.label.replace('Ret$', returnVar);

                            return item;
                        });
                    });
                }
            }
        ));






        // Define supported commands that use the library/code pattern
        const libraryCodeCommands = [
            { name: 'new_macro', pattern: /new_macro\(\s*(?:"([^"]*)"|'([^']*)')?(?:\s*,\s*(?:"([^"]*)"|'([^']*)')?)?\s*$/ },
            { name: 'new_djoint', pattern: /new_djoint\(\s*(?:"([^"]*)"|'([^']*)')?(?:\s*,\s*(?:"([^"]*)"|'([^']*)')?)?\s*$/ },
            { name: 'new_sheet', pattern: /new_sheet\(\s*(?:"([^"]*)"|'([^']*)')?(?:\s*,\s*(?:"([^"]*)"|'([^']*)')?)?\s*$/ },
            // Add more commands with the same parameter pattern as needed
        ];

        // Register completion provider for all library/code commands
        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                provideCompletionItems(document, position) {
                    const linePrefix = document.lineAt(position).text.substring(0, position.character);

                    // Check for any matching command pattern
                    let matchedCommand = null;
                    let regexMatch = null;

                    for (const cmd of libraryCodeCommands) {
                        const match = linePrefix.match(cmd.pattern);
                        if (match) {
                            matchedCommand = cmd;
                            regexMatch = match;
                            break;
                        }
                    }
                    if (regexMatch === null) {
                        return []; // No match found
                    }
                    // Determine if we're completing the first parameter (LIBRARY) or second (CODE)
                    const isFirstParameter = regexMatch[1] !== undefined || regexMatch[2] !== undefined 
                        ? false  // Already typing in first param
                        : !linePrefix.includes(',');  // No comma yet, so first param

                    if (isFirstParameter) {
                        // Request library names from server
                        return client.sendRequest('custom/getMacroLibraries').then(libraries => {
                            return libraries.map(lib => {
                                const item = new vscode_1.CompletionItem(lib, vscode_1.CompletionItemKind.Folder);
                                item.insertText = `"${lib}"`;
                                return item;
                            });
                        });
                    } else {
                        // Get the library name to fetch available codes
                        const libraryName = regexMatch[1] || regexMatch[2] || '';

                        // Request codes for the specified library
                        return client.sendRequest('custom/getMacroCodes', { library: libraryName }).then(codes => {
                            return codes.map(code => {
                                const item = new vscode_1.CompletionItem(code, vscode_1.CompletionItemKind.Value);
                                item.insertText = `"${code}"`;
                                return item;
                            });
                        });
                    }
                }
            },
            '"', ',', '(' // Trigger completion after these characters in command context
        ));

        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                provideCompletionItems(document, position) {
                    const lineText = document.lineAt(position).text;
                    const analysis = formNameCompletion.analyzeFormNameCompletion(lineText, position.character);
                    if (!analysis) {
                        return [];
                    }

                    return client.sendRequest('custom/getFormNames').then(forms => {
                        if (!forms || !forms.length) {
                            return [];
                        }

                        const replaceRange = new vscode_1.Range(
                            position.line,
                            analysis.replaceStart,
                            position.line,
                            analysis.replaceEnd
                        );

                        return forms.map(form => {
                            const item = new vscode_1.CompletionItem(form.name, vscode_1.CompletionItemKind.File);
                            item.insertText = formNameCompletion.formInsertText(form.name, analysis.insideQuotes);
                            item.detail = form.origin;
                            item.filterText = form.name;
                            item.sortText = form.name.toLowerCase();
                            item.range = replaceRange;
                            return item;
                        });
                    });
                }
            },
            '"', ',', '(', ')', ' '
        ));

        // Enhanced completion provider for all get_* commands with object-specific parameters
        context.subscriptions.push(vscode_1.languages.registerCompletionItemProvider(
            { scheme: 'file', language: 'vertexadt' },
            {
                async provideCompletionItems(document, position) {
                    const linePrefix = document.lineAt(position).text.substring(0, position.character);
                
                    // Check for any get_* command pattern
                    const getCommandInfo = extractGetCommandInfo(linePrefix);
                    if (!getCommandInfo) {
                        return [];
                    }
                
                    const { command, variableName } = getCommandInfo;
                
                    const [objectType, genericParameters] = await Promise.all([
                        client.sendRequest('custom/getVariableObjectType', {
                            uri: document.uri.toString(),
                            variableName,
                            line: position.line
                        }),
                        client.sendRequest('custom/getObjectParameters', {
                            command: command,
                            objectType: 'generic'
                        })
                    ]);

                    const parameters = await client.sendRequest('custom/getObjectParameters', {
                        command,
                        objectType,
                        uri: document.uri.toString(),
                        variableName,
                        line: position.line
                    });

                    const finalParameters = parameters.length > 0 ? parameters : genericParameters;

                    return finalParameters.map(param => {
                        const item = new vscode_1.CompletionItem(param.label, vscode_1.CompletionItemKind.Property);
                        item.insertText = param.insertText || `"${param.label}"`;
                        item.documentation = new vscode_1.MarkdownString(param.documentation);
                        item.detail = `${command} - ${param.detail}`;
                        item.sortText = `0${param.label}`; // Ensure these appear first
                        return item;
                    });
                }
            },
            ',', ' ', ')' // Trigger characters
        ));





    });
    }
    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.showImpactAnalysis', async (payload) => {
        try {
            await showImpactAnalysisQuickPick(payload);
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode_1.window.showErrorMessage(`Failed to show impact analysis: ${message}`);
        }
    }));
    // Inferred ##included_from CodeLens (docs/adr/0015-native-inferred-included-from.md):
    // one parent opens directly, several open a quick pick.
    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.openIncludedFromParents', async (parents) => {
        try {
            const rows = Array.isArray(parents) ? parents.filter((row) => row && typeof row.uri === 'string') : [];
            if (rows.length === 0) {
                return;
            }
            let picked = rows[0];
            if (rows.length > 1) {
                picked = await vscode_1.window.showQuickPick(rows.map((row) => ({ label: row.displayPath || row.uri, row })), { placeHolder: 'Open an inferred ##included_from parent' }).then((item) => item && item.row);
            }
            if (picked) {
                await vscode_1.window.showTextDocument(vscode_1.Uri.parse(picked.uri));
            }
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode_1.window.showErrorMessage(`Failed to open parent: ${message}`);
        }
    }));
    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.exportWorkspaceReports', async () => {
        try {
            const result = await sendHostCommand('vertexadt.exportWorkspaceReports', 'custom/exportWorkspaceReports');
            if (!result || result.error) {
                vscode_1.window.showErrorMessage(result?.error || 'Failed to export workspace reports');
                return;
            }
            const { programUsagePath, declarationNumbersPath, localSlotsPath } = result;
            vscode_1.window.showInformationMessage(
                `Workspace reports written to ${programUsagePath}, ${declarationNumbersPath}, and ${localSlotsPath}`
            );
            const programUsageDoc = await vscode_1.workspace.openTextDocument(vscode_1.Uri.file(programUsagePath));
            await vscode_1.window.showTextDocument(programUsageDoc, { preview: false });
            const declarationNumbersDoc = await vscode_1.workspace.openTextDocument(vscode_1.Uri.file(declarationNumbersPath));
            await vscode_1.window.showTextDocument(declarationNumbersDoc, { preview: false });
            const localSlotsDoc = await vscode_1.workspace.openTextDocument(vscode_1.Uri.file(localSlotsPath));
            await vscode_1.window.showTextDocument(localSlotsDoc, { preview: false });
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode_1.window.showErrorMessage(`Failed to export workspace reports: ${message}`);
        }
    }));
    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.exportWorkspaceDiagnostics', async () => {
        try {
            if (!nativeHost && client) {
                client.sendNotification('custom/updateSettings', getExtensionSettings());
            }
            const result = await vscode_1.window.withProgress({
                location: vscode_1.ProgressLocation.Notification,
                title: 'Vertex ADT: Validating workspace files…',
                cancellable: false,
            }, async () => sendHostCommand('vertexadt.exportWorkspaceDiagnostics', 'custom/exportWorkspaceDiagnostics'));
            if (!result || result.error) {
                vscode_1.window.showErrorMessage(result?.error || 'Failed to export workspace diagnostics');
                return;
            }
            const {
                diagnosticsPath,
                fileCount,
                diagnosticCount,
                errorCount,
                warningCount,
                informationCount,
            } = result;
            vscode_1.window.showInformationMessage(
                `Validated ${fileCount} file(s): ${diagnosticCount} issue(s) ` +
                `(${errorCount} error(s), ${warningCount} warning(s), ${informationCount} information). ` +
                `Written to ${diagnosticsPath}`
            );
            const diagnosticsDoc = await vscode_1.workspace.openTextDocument(vscode_1.Uri.file(diagnosticsPath));
            await vscode_1.window.showTextDocument(diagnosticsDoc, { preview: false });
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode_1.window.showErrorMessage(`Failed to export workspace diagnostics: ${message}`);
        }
    }));
    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.fixMissingCallParameters', async () => {
        try {
            const result = await sendHostCommand('vertexadt.fixMissingCallParameters', 'custom/fixMissingCallParameters');
            if (!result || !result.changes || Object.keys(result.changes).length === 0) {
                vscode_1.window.showInformationMessage('No call parameter fixes to apply in the workspace.');
                return;
            }
            const workspaceEdit = new vscode_1.WorkspaceEdit();
            const uris = Object.keys(result.changes);
            for (let i = 0; i < uris.length; i++) {
                const uri = vscode_1.Uri.parse(uris[i]);
                const textEdits = result.changes[uris[i]].map((edit) => new vscode_1.TextEdit(
                    new vscode_1.Range(
                        edit.range.start.line,
                        edit.range.start.character,
                        edit.range.end.line,
                        edit.range.end.character
                    ),
                    edit.newText
                ));
                workspaceEdit.set(uri, textEdits);
            }
            const applied = await vscode_1.workspace.applyEdit(workspaceEdit);
            if (applied) {
                vscode_1.window.showInformationMessage(
                    `Fixed call parameters at ${result.fixedCount || 0} call site(s).`
                );
            } else {
                vscode_1.window.showWarningMessage('Could not apply call parameter fixes.');
            }
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode_1.window.showErrorMessage(`Failed to fix call parameters: ${message}`);
        }
    }));
    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.assignLocalSlotNames', async () => {
        const editor = vscode_1.window.activeTextEditor;
        if (!editor) {
            vscode_1.window.showInformationMessage('Open a callee program file to assign local slot names.');
            return;
        }
        try {
            const result = await sendHostCommand('vertexadt.assignLocalSlotNames', 'custom/assignLocalSlotNames', {
                uri: editor.document.uri.toString(),
            });
            if (result?.error) {
                vscode_1.window.showErrorMessage(result.error);
                return;
            }
            if (!result || !result.changes || Object.keys(result.changes).length === 0) {
                vscode_1.window.showInformationMessage('No local declarations need renaming in this file.');
                return;
            }
            const workspaceEdit = new vscode_1.WorkspaceEdit();
            const uris = Object.keys(result.changes);
            for (let i = 0; i < uris.length; i++) {
                const uri = vscode_1.Uri.parse(uris[i]);
                const textEdits = result.changes[uris[i]].map((edit) => new vscode_1.TextEdit(
                    new vscode_1.Range(
                        edit.range.start.line,
                        edit.range.start.character,
                        edit.range.end.line,
                        edit.range.end.character
                    ),
                    edit.newText
                ));
                workspaceEdit.set(uri, textEdits);
            }
            const applied = await vscode_1.workspace.applyEdit(workspaceEdit);
            if (applied) {
                vscode_1.window.showInformationMessage(
                    `Assigned local slot names at ${result.assignedCount || 0} declaration(s).`
                );
            } else {
                vscode_1.window.showWarningMessage('Could not apply local slot name assignments.');
            }
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode_1.window.showErrorMessage(`Failed to assign local slot names: ${message}`);
        }
    }));
    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.createFileHeader', async () => {
        const editor = vscode_1.window.activeTextEditor;
        if (!editor) {
            vscode_1.window.showInformationMessage('Open a Vertex ADT file to create a file header.');
            return;
        }
        if (editor.document.languageId !== 'vertexadt' || editor.document.uri.scheme !== 'file') {
            vscode_1.window.showInformationMessage('Create File Header is only available for Vertex ADT files.');
            return;
        }

        try {
            const username = vscode_1.workspace.getConfiguration('vertexadt.header').get('username') || '';
            const fileStem = getFileStemFromUri(editor.document.uri);
            const headerBlock = buildFileHeader(fileStem, new Date(), username);
            const currentText = editor.document.getText();
            const result = applyHeaderToDocumentText(currentText, headerBlock);

            if (result.text === currentText) {
                vscode_1.window.showInformationMessage('File header is already up to date.');
                return;
            }

            const fullRange = new vscode_1.Range(
                editor.document.positionAt(0),
                editor.document.positionAt(currentText.length)
            );
            const applied = await editor.edit((editBuilder) => {
                editBuilder.replace(fullRange, result.text);
            });

            if (applied) {
                const message = result.mode === 'replaced'
                    ? 'Replaced file header.'
                    : 'Created file header.';
                vscode_1.window.showInformationMessage(message);
            } else {
                vscode_1.window.showWarningMessage('Could not apply file header.');
            }
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode_1.window.showErrorMessage(`Failed to create file header: ${message}`);
        }
    }));
    // Program move (docs/adr/0017-native-program-move.md): native server only.
    const programMoveNative = getLanguageServerImplementation() === 'native';
    vscode_1.commands.executeCommand('setContext', 'vertexadt.nativeServer', programMoveNative);
    const programMoveChannel = vscode_1.window.createOutputChannel('Vertex ADT: Program Move');
    context.subscriptions.push(programMoveChannel);
    const showProgramMoveReport = (report, label, fromDrag) => {
        const updated = (report && report.updatedCount) || 0;
        const skipped = (report && report.skipped) || [];
        const warnings = (report && report.warnings) || [];
        if (skipped.length === 0 && warnings.length === 0) {
            if (!fromDrag) {
                vscode_1.window.showInformationMessage(`${label}: ${updated} reference(s) updated.`);
            }
            return;
        }
        programMoveChannel.appendLine(`[${new Date().toLocaleString()}] ${label}: ${updated} updated, ${skipped.length} skipped`);
        for (const warning of warnings) {
            programMoveChannel.appendLine(`  warning: ${warning}`);
        }
        for (const site of skipped) {
            const file = vscode_1.Uri.parse(site.uri).fsPath;
            programMoveChannel.appendLine(`  ${file}:${site.line + 1}  [${site.reason}]  ${site.text}`);
        }
        programMoveChannel.appendLine('');
        let message = `${label}: ${updated} reference(s) updated`;
        if (skipped.length > 0) {
            message += `, ${skipped.length} skipped`;
        }
        message += '.';
        if (warnings.length > 0) {
            message += ` ${warnings[0]}${warnings.length > 1 ? ` (+${warnings.length - 1} more)` : ''}`;
        }
        vscode_1.window.showWarningMessage(message, 'Show details').then((choice) => {
            if (choice === 'Show details') {
                programMoveChannel.show(true);
            }
        });
    };
    if (client && programMoveNative) {
        client.onNotification('vertexadt/programMoveReport', (report) => {
            showProgramMoveReport(report, 'Moved files', true);
        });
    }
    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.moveProgram', async (arg) => {
        if (!client || !programMoveNative) {
            vscode_1.window.showInformationMessage('Move Callable Program needs the native language server (vertexadt.languageServer.implementation = native).');
            return;
        }
        let uri;
        if (arg instanceof vscode_1.Uri) {
            uri = arg;
        } else if (arg && typeof arg.uri === 'string') {
            uri = vscode_1.Uri.parse(arg.uri);
        } else if (vscode_1.window.activeTextEditor) {
            uri = vscode_1.window.activeTextEditor.document.uri;
        }
        if (!uri || uri.scheme !== 'file') {
            vscode_1.window.showInformationMessage('Open a Vertex ADT file to move.');
            return;
        }
        const send = (args) => client.sendRequest('workspace/executeCommand', {
            command: 'vertexadt.moveProgram',
            arguments: [Object.assign({ uri: uri.toString() }, args)],
        });
        try {
            const initial = await send({ mode: 'default' });
            if (!initial || initial.error) {
                vscode_1.window.showErrorMessage((initial && initial.error) || 'Cannot move this file.');
                return;
            }
            const current = initial.target;
            const folderStart = current.indexOf('/') + 1;
            const folderEnd = Math.max(folderStart, current.lastIndexOf('/'));
            const target = await vscode_1.window.showInputBox({
                title: `Move ${path.basename(uri.fsPath)}`,
                prompt: 'New path: $DIR_* or Custom-relative. End with / to keep the file name.',
                value: current,
                valueSelection: [folderStart, folderEnd],
                validateInput: async (value) => {
                    const checked = await send({ mode: 'validate', target: value });
                    return checked && checked.error ? checked.error : null;
                },
            });
            if (target === undefined || target.trim() === current) {
                return;
            }
            const plan = (args) => vscode_1.window.withProgress({
                location: vscode_1.ProgressLocation.Notification,
                title: 'Finding references to move…',
            }, () => send(Object.assign({ mode: 'plan', target }, args)));
            let planned = await plan({});
            if (planned && planned.confirmation) {
                const choice = await vscode_1.window.showWarningMessage(planned.confirmation, { modal: true }, 'Move anyway');
                if (choice !== 'Move anyway') {
                    return;
                }
                planned = await plan({ confirmShadow: true });
            }
            if (!planned || planned.refusal || planned.error || planned.confirmation) {
                vscode_1.window.showErrorMessage((planned && (planned.refusal || planned.error || planned.confirmation)) || 'Move failed.');
                return;
            }
            const edit = await client.protocol2CodeConverter.asWorkspaceEdit(planned.edit);
            const applied = await vscode_1.workspace.applyEdit(edit);
            if (!applied) {
                vscode_1.window.showWarningMessage('The move was not applied.');
                return;
            }
            showProgramMoveReport(planned.report, `Moved ${path.basename(uri.fsPath)}`, false);
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode_1.window.showErrorMessage(`Failed to move the program: ${message}`);
        }
    }));
    // Start the client. This will also launch the server
    if (client) {
        client.start();
    }
}
exports.activate = activate;
function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
exports.deactivate = deactivate;