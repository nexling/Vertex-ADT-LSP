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
const slotSuggest_1 = require("./slotSuggest");
const signatureFollow_1 = require("./signatureFollow");
let client;

//FUNCTIONS:-----------------------------------------------------------

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


async function sendHostCommand(commandId, args) {
    if (!client) {
        throw new Error('The Vertex ADT language server is not running.');
    }
    return client.sendRequest('workspace/executeCommand', {
        command: commandId,
        arguments: args === undefined ? [] : [args]
    });
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


    // Debug traces over this size get range tokens only
    // (docs/adr/0025-debug-trace-fast-path.md).
    const DEBUG_TRACE_FULL_TOKENS_MAX_CHARS = 2 * 1024 * 1024;

    function isDebugTraceDocument(document) {
        if (/\.dbg$/i.test(document.fileName || '')) {
            return true;
        }
        if (document.lineCount === 0) {
            return false;
        }
        return /START OF DEBUG in (FILE|SECTION)/.test(document.lineAt(0).text);
    }

    // A full-document token request on a big debug trace would hand VS Code
    // millions of tokens, and a full result (even null) stops the viewport
    // range requests. Cancelling instead leaves the document without
    // complete tokens, so VS Code keeps asking for the visible range.
    function provideDebugAwareDocumentSemanticTokens(document, token, next) {
        if (isDebugTraceDocument(document)) {
            const chars = document.offsetAt(new vscode_1.Position(document.lineCount, 0));
            if (chars > DEBUG_TRACE_FULL_TOKENS_MAX_CHARS) {
                throw vscode_1.CancellationError ? new vscode_1.CancellationError() : new Error('busy');
            }
        }
        return next(document, token);
    }


//END OF FUNCTIONS-----------------------------------------------------------


// MAIN METHOD TO START LSP-SERVER-----------------------------------------------------------

function activate(context) {
    let serverOptions;
    let clientOptions;
    const command = context.asAbsolutePath(path.join('server', 'vertex-adt-ls.exe'));
    if (!fs.existsSync(command)) {
        vscode_1.window.showErrorMessage(
            `Vertex ADT language-server executable not found at ${command}. ` +
            `The language server ships for Windows x64 only (this is ${process.platform}-${process.arch}).`
        );
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
            middleware: {
                provideCompletionItem: slotSuggest_1.provideCompletionItem,
                provideDocumentSemanticTokens: provideDebugAwareDocumentSemanticTokens
            },
            synchronize: {
                configurationSection: 'vertexadt'
            }
        };
    }

    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.openSettings', () => {
        vscode_1.commands.executeCommand('workbench.action.openSettings', 'vertexadt');
    }));


    // Create the language client and start the client.
    if (serverOptions && clientOptions) {
        client = new node_1.LanguageClient('languageServerExample', 'Vertex-adt LanguageServer', serverOptions, clientOptions);
        slotSuggest_1.register(context, client);
        signatureFollow_1.register(context, client);
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
            const result = await sendHostCommand('vertexadt.exportWorkspaceReports');
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
            const result = await vscode_1.window.withProgress({
                location: vscode_1.ProgressLocation.Notification,
                title: 'Vertex ADT: Validating workspace files…',
                cancellable: false,
            }, async () => sendHostCommand('vertexadt.exportWorkspaceDiagnostics'));
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
            const result = await sendHostCommand('vertexadt.fixMissingCallParameters');
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
            const result = await sendHostCommand('vertexadt.assignLocalSlotNames', {
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
    // Program move (docs/adr/0017-native-program-move.md).
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
    if (client) {
        client.onNotification('vertexadt/programMoveReport', (report) => {
            showProgramMoveReport(report, 'Moved files', true);
        });
    }
    context.subscriptions.push(vscode_1.commands.registerCommand('vertexadt.moveProgram', async (arg) => {
        if (!client) {
            vscode_1.window.showInformationMessage('Move Callable Program needs the Vertex ADT language server, which is not running.');
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