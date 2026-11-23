'use strict';
// Reopens the signature help box when the cursor moves into a command's
// arguments (docs/adr/0022-signature-box-follows-the-cursor.md). VS Code opens
// it only when `(`, `,` or a space is typed, and a click closes it.
Object.defineProperty(exports, "__esModule", { value: true });
exports.register = void 0;
const vscode_1 = require("vscode");

const CLOSE_COMMAND = 'vertexadt.closeSignatureBox';
const DEBOUNCE_MS = 50;
// A selection change this soon after an edit comes from the edit. VS Code
// handles the box while typing.
const EDIT_MS = 50;

let client;
let timer;
// Bumped by every move, edit and Escape; a reply that is no longer current is dropped.
let token = 0;
let lastEditAt = 0;
let lastEditUri;
// The command whose box was escaped: { uri, site }, `site` a promise of its
// `line:character`.
let escaped;

function cancelPending() {
    clearTimeout(timer);
    token += 1;
}

function isEnabled(document) {
    return vscode_1.workspace.getConfiguration('editor', document).get('parameterHints.enabled') !== false;
}

// Name positions of the commands around `position` that have something to
// show, innermost first.
async function sitesAt(document, position) {
    if (!client.isRunning()) {
        return [];
    }
    try {
        const sites = await client.sendRequest('vertexadt/signatureHelpSites', {
            textDocument: { uri: client.code2ProtocolConverter.asUri(document.uri) },
            position: { line: position.line, character: position.character },
        });
        return (sites || []).map((site) => `${site.line}:${site.character}`);
    }
    catch {
        return undefined; // Canceled by a newer edit, or the server is stopping.
    }
}

function singleCursor(editor) {
    return editor.selections.length === 1 && editor.selection.isEmpty;
}

async function openBox(editor) {
    if (vscode_1.window.activeTextEditor !== editor || !singleCursor(editor) || !isEnabled(editor.document)) {
        return;
    }
    const mine = token;
    const position = editor.selection.active;
    if (escaped) {
        const current = escaped;
        if (current.uri === editor.document.uri.toString()) {
            const [site, sites] = await Promise.all([current.site, sitesAt(editor.document, position)]);
            if (mine !== token || escaped !== current || !sites) {
                return;
            }
            if (site !== undefined && sites.includes(site)) {
                return; // Still inside the escaped command's arguments.
            }
        }
        escaped = undefined;
        if (vscode_1.window.activeTextEditor !== editor || !singleCursor(editor)) {
            return;
        }
    }
    // Nothing to show closes an open box and shows no message.
    await vscode_1.commands.executeCommand('editor.action.triggerParameterHints');
}

function onSelection(event) {
    const editor = event.textEditor;
    const { document } = editor;
    if (document.languageId !== 'vertexadt' || editor !== vscode_1.window.activeTextEditor) {
        return;
    }
    cancelPending();
    if (!singleCursor(editor)
        || (lastEditUri === document.uri.toString() && Date.now() - lastEditAt < EDIT_MS)) {
        return;
    }
    timer = setTimeout(() => { void openBox(editor); }, DEBOUNCE_MS);
}

function onEdit(event) {
    const { document, contentChanges } = event;
    if (document.languageId !== 'vertexadt' || contentChanges.length === 0) {
        return;
    }
    cancelPending();
    lastEditAt = Date.now();
    lastEditUri = document.uri.toString();
    // A typed trigger character (`(` may arrive with its auto-closed `)`).
    if (contentChanges.some((change) => /^(?:[(, ]|\(\))$/.test(change.text))) {
        escaped = undefined;
    }
}

function closeBox() {
    cancelPending();
    const editor = vscode_1.window.activeTextEditor;
    if (editor) {
        const { document } = editor;
        escaped = {
            uri: document.uri.toString(),
            site: sitesAt(document, editor.selection.active).then((sites) => sites && sites[0]),
        };
    }
    return vscode_1.commands.executeCommand('closeParameterHints');
}

function register(context, languageClient) {
    client = languageClient;
    context.subscriptions.push(
        vscode_1.commands.registerCommand(CLOSE_COMMAND, closeBox),
        vscode_1.window.onDidChangeTextEditorSelection(onSelection),
        vscode_1.workspace.onDidChangeTextDocument(onEdit),
        { dispose: cancelPending }
    );
}
exports.register = register;
