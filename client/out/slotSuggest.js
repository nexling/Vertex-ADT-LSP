'use strict';
// Reopens the suggestion popup in a slot after an edit
// (docs/adr/0021-native-slot-popup-reopens.md). VS Code closes the popup when
// nothing matches and never opens one on a deletion.
Object.defineProperty(exports, "__esModule", { value: true });
exports.register = exports.provideCompletionItem = void 0;
const vscode_1 = require("vscode");

const ACCEPTED_COMMAND = 'vertexadt.suggestionAccepted';
// Long enough for VS Code's own re-request, which makes ours unnecessary.
const DEBOUNCE_MS = 80;
// A completion request this soon after an edit at its position is automatic,
// not Ctrl+Space.
const AUTOMATIC_MS = 300;

let client;
let timer;
// Bumped by every edit and acceptance; a probe that is no longer current is dropped.
let token = 0;
let lastEditAt = 0;
let lastEditUri;
// `uri|version|line|character` of VS Code's latest completion request.
let lastRequest;

function positionKey(document, position) {
    return `${document.uri}|${document.version}|${position.line}|${position.character}`;
}

function openPopup() {
    // `auto`: no "No suggestions" box, and the popup closes when nothing matches.
    return vscode_1.commands.executeCommand('editor.action.triggerSuggest', { auto: true });
}

function cancelPending() {
    clearTimeout(timer);
    token += 1;
}

function opensNextLevel(item) {
    if (item.kind !== vscode_1.CompletionItemKind.Folder) {
        return false;
    }
    const text = item.textEdit ? item.textEdit.newText : item.insertText;
    return typeof text === 'string' && text.endsWith('/');
}

// Completion middleware.
// - An automatic request isn't sent as Invoked: that kind means Ctrl+Space to
//   the server and switches a complete path to its sections.
// - Every item reports its acceptance, so the edit it makes doesn't reopen the
//   popup; a folder ending in `/` opens the next level.
async function provideCompletionItem(document, position, context, cancellation, next) {
    lastRequest = positionKey(document, position);
    const automatic = context.triggerKind === vscode_1.CompletionTriggerKind.Invoke
        && lastEditUri === document.uri.toString()
        && Date.now() - lastEditAt < AUTOMATIC_MS;
    const sent = automatic
        ? { triggerKind: vscode_1.CompletionTriggerKind.TriggerCharacter, triggerCharacter: undefined }
        : context;
    const result = await next(document, position, sent, cancellation);
    const items = Array.isArray(result) ? result : result && result.items;
    for (const item of items || []) {
        if (!item.command) {
            item.command = { command: ACCEPTED_COMMAND, title: '', arguments: [opensNextLevel(item)] };
        }
    }
    return result;
}
exports.provideCompletionItem = provideCompletionItem;

// The active editor when its single cursor sits at the edit.
function editorAtEdit(document, change) {
    const editor = vscode_1.window.activeTextEditor;
    if (!editor || editor.document !== document || editor.selections.length !== 1 || !editor.selection.isEmpty) {
        return undefined;
    }
    const cursor = editor.selection.active;
    const start = change.range.start;
    return cursor.line === start.line
        && cursor.character >= start.character
        && cursor.character <= start.character + change.text.length
        ? editor
        : undefined;
}

async function probeAndOpen(document, change) {
    const editor = editorAtEdit(document, change);
    if (!editor || !client.isRunning()) {
        return;
    }
    const position = editor.selection.active;
    const key = positionKey(document, position);
    if (lastRequest === key) {
        return; // VS Code already asked here.
    }
    const mine = token;
    let result;
    try {
        result = await client.sendRequest('vertexadt/slotCompletionProbe', {
            textDocument: { uri: client.code2ProtocolConverter.asUri(document.uri) },
            position: { line: position.line, character: position.character },
            context: { triggerKind: 3 },
        });
    }
    catch {
        return; // Canceled by a newer edit, or the server is stopping.
    }
    if (mine !== token || !result || !result.items || result.items.length === 0) {
        return;
    }
    const current = vscode_1.window.activeTextEditor;
    if (current !== editor || !editor.selection.isEmpty || editor.selections.length !== 1
        || positionKey(document, editor.selection.active) !== key || lastRequest === key) {
        return;
    }
    // The popup's own request is automatic too, however long the probe took.
    lastEditAt = Date.now();
    await openPopup();
}

function onEdit(event) {
    const { document, contentChanges } = event;
    if (document.languageId !== 'vertexadt' || contentChanges.length === 0) {
        return;
    }
    cancelPending();
    lastEditAt = Date.now();
    lastEditUri = document.uri.toString();
    if (contentChanges.length !== 1 || /[\r\n]/.test(contentChanges[0].text)) {
        return;
    }
    const enabled = vscode_1.workspace.getConfiguration('editor', document).get('suggestOnTriggerCharacters');
    if (enabled === false) {
        return;
    }
    const change = contentChanges[0];
    timer = setTimeout(() => { void probeAndOpen(document, change); }, DEBOUNCE_MS);
}

function register(context, languageClient) {
    client = languageClient;
    context.subscriptions.push(
        vscode_1.commands.registerCommand(ACCEPTED_COMMAND, (nextLevel) => {
            cancelPending();
            if (nextLevel) {
                return openPopup();
            }
            return undefined;
        }),
        vscode_1.workspace.onDidChangeTextDocument(onEdit),
        { dispose: cancelPending }
    );
}
exports.register = register;
