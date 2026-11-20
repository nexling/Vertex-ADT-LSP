#!/usr/bin/env node
'use strict';

/**
 * Parity goldens for the IGNORE CLOSE code action.
 * Assertions from test-fixtures/unclosed-resources/TESTING.md (quick-fix
 * available; scenario 5: the suffix silences the warning).
 *
 * Oracle: the live Node language server (server/out/server.js --stdio)
 * answering the same textDocument/codeAction requests. Node's onCodeAction
 * consumes context.diagnostics without re-walking, so both servers get the
 * same request diagnostics and must return the same actions.
 */

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'unclosed-resources');
const CUSTOM = path.join(FIXTURE, 'custom');
const ROOT_URI = pathToFileURL(FIXTURE).href;
const NODE_SERVER = path.join(ROOT, 'server', 'out', 'server.js');

const TITLE = 'Ignore missing close (;!IGNORE CLOSE)';
const UNCLOSED = 'vertexadt/unclosed-resource';
const ALREADY_OPEN = 'vertexadt/unclosed-already-open';

class AssertionError extends Error {
    constructor(message) {
        super(message);
        this.name = 'AssertionError';
    }
}

function assert(condition, message) {
    if (!condition) {
        throw new AssertionError(message);
    }
}

function canonical(value) {
    if (Array.isArray(value)) {
        return value.map(canonical);
    }
    if (value && typeof value === 'object') {
        const out = {};
        for (const key of Object.keys(value).sort()) {
            out[key] = canonical(value[key]);
        }
        return out;
    }
    return value;
}

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(canonical(actual));
    const expectedJson = JSON.stringify(canonical(expected));
    assert(actualJson === expectedJson, `${message}: expected ${expectedJson}, got ${actualJson}`);
}

function fixtureUri(name) {
    return pathToFileURL(path.join(CUSTOM, name)).href;
}

function fixtureText(name) {
    return fs.readFileSync(path.join(CUSTOM, name), 'utf8');
}

class NodeOracleSession extends LspSession {
    static launchNode(options) {
        const child = spawn(process.execPath, [NODE_SERVER, '--stdio'], {
            cwd: ROOT,
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });
        child.stdin.setDefaultEncoding('utf8');
        return new NodeOracleSession(child, options);
    }

    async initialize() {
        const result = await this.request('initialize', {
            processId: process.pid,
            rootUri: this._rootUri,
            capabilities: { workspace: { configuration: true, workspaceFolders: true } },
            initializationOptions: {
                extensionUri: this._extensionUri,
                settings: { settings: { strictMode: false } },
            },
            workspaceFolders: this._workspaceFolders,
        });
        this.notify('initialized', {});
        return result;
    }

    _replyToServerRequest(message) {
        if (message.method === 'workspace/configuration') {
            const items = (message.params && message.params.items) || [];
            this._write({
                jsonrpc: '2.0',
                id: message.id,
                result: items.map(() => ({ strictMode: false, maxNumberOfProblems: 100 })),
            });
            return;
        }
        super._replyToServerRequest(message);
    }
}

function sessionOptions() {
    return {
        rootUri: ROOT_URI,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: ROOT_URI, name: 'unclosed-resources' }],
    };
}

function codeAction(session, uri, diagnostics, only) {
    const context = { diagnostics };
    if (only) {
        context.only = only;
    }
    return session.request('textDocument/codeAction', {
        textDocument: { uri },
        range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
        context,
    });
}

// One zero-width insert per action, applied the way the editor would.
function applyEdit(text, uri, action) {
    const edits = action.edit.changes[uri];
    assert(edits && edits.length === 1, `one edit for ${uri}`);
    const { range, newText } = edits[0];
    assertEqual(range.start, range.end, 'zero-width insert');
    const lines = text.split('\n');
    const raw = lines[range.start.line];
    const hasCr = raw.endsWith('\r');
    const body = hasCr ? raw.slice(0, -1) : raw;
    lines[range.start.line] = body.slice(0, range.start.character) + newText
        + body.slice(range.start.character) + (hasCr ? '\r' : '');
    return lines.join('\n');
}

function unclosedItems(report) {
    return ((report && report.items) || []).filter((item) => (
        item.code === UNCLOSED || item.code === ALREADY_OPEN
    ));
}

function diag(code, line, data) {
    const item = {
        range: { start: { line, character: 0 }, end: { line, character: 1 } },
        message: 'synthetic',
        severity: 2,
        code,
        source: 'ADT-Syntax',
    };
    if (data !== undefined) {
        item.data = data;
    }
    return item;
}

// Fixture lines: pull native diagnostics, take the QuickFix, apply it, and
// revalidate (TESTING.md scenario 5). The oracle gets the same request.
const FIXTURE_ROWS = [
    { name: 'unclosed_dbopen.prg', code: UNCLOSED, line0: 'dbopen(1) "SHEETLIB",1 ;!IGNORE CLOSE' },
    { name: 'already_open.prg', code: ALREADY_OPEN, line0: 'dbopen(1) "ALREADY_OPEN",OtherId ;!IGNORE CLOSE' },
    { name: 'include_opener.prg', code: UNCLOSED, line0: '# include opener.prg ;!IGNORE CLOSE' },
];

// Synthetic request rows: document text, request diagnostics, and the
// expected native insert (null = no action). Node must agree on every row.
const SYNTHETIC_ROWS = [
    { label: 'plain', text: 'dbopen(1) "A",1\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 15, ' ;!IGNORE CLOSE']] },
    { label: 'existing bang', text: 'dbopen(1) "A",1 ;!keep\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 22, ' IGNORE CLOSE']] },
    { label: 'already ignored', text: 'dbopen(1) "A",1 ;!IGNORE CLOSE\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [] },
    { label: 'lowercase ignored', text: 'dbopen(1) "A",1 ;!ignore close\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [] },
    { label: 'spaced words ignored', text: 'dbopen(1) "A",1 ;!IGNORE   CLOSE\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [] },
    { label: 'tab between words ignored', text: 'dbopen(1) "A",1 ;!IGNORE\tCLOSE\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [] },
    { label: 'NBSP between words ignored (JS \\s)', text: 'dbopen(1) "A",1 ;!IGNORE CLOSE\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [] },
    { label: 'glued word is not the marker', text: 'dbopen(1) "A",1 ;!XIGNORE CLOSE\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 31, ' IGNORE CLOSE']] },
    { label: 'CLOSED is not the marker', text: 'dbopen(1) "A",1 ;!IGNORE CLOSED\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 31, ' IGNORE CLOSE']] },
    { label: 'spaced bang is not a bang comment', text: 'dbopen(1) "A",1 ; !IGNORE CLOSE\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 31, ' ;!IGNORE CLOSE']] },
    { label: 'marker before the bang', text: 'dbopen(1) "IGNORE CLOSE",1 ;!x\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 30, ' IGNORE CLOSE']] },
    { label: 'trailing space kept', text: 'dbopen(1) "A",1 \n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 16, ' ;!IGNORE CLOSE']] },
    { label: 'CRLF', text: 'x=1\r\ndbopen(1) "A",1\r\n', diagnostics: [diag(UNCLOSED, 1, { line: 1 })], expect: [[1, 15, ' ;!IGNORE CLOSE']] },
    { label: 'quoted bang', text: 'dbopen(1) ";!",1\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 16, ' ;!IGNORE CLOSE']] },
    { label: 'single-quoted bang', text: "open(2) ';!IGNORE CLOSE',1\n", diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 26, ' ;!IGNORE CLOSE']] },
    { label: 'UTF-16 columns', text: 'dbopen(1) "Å€😀",1\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 18, ' ;!IGNORE CLOSE']] },
    { label: 'data.line wins over range', text: 'x=1\ndbopen(1) "A",1\n', diagnostics: [diag(UNCLOSED, 0, { line: 1 })], expect: [[1, 15, ' ;!IGNORE CLOSE']] },
    { label: 'no data uses range', text: 'x=1\ndbopen(1) "A",1\n', diagnostics: [diag(UNCLOSED, 1)], expect: [[1, 15, ' ;!IGNORE CLOSE']] },
    { label: 'non-number data.line uses range', text: 'x=1\ndbopen(1) "A",1\n', diagnostics: [diag(UNCLOSED, 1, { line: '0' })], expect: [[1, 15, ' ;!IGNORE CLOSE']] },
    { label: 'out-of-range line inserts at 0', text: 'dbopen(1) "A",1\n', diagnostics: [diag(UNCLOSED, 9, { line: 9 })], expect: [[9, 0, ' ;!IGNORE CLOSE']] },
    { label: 'two leftovers on one line', text: '# include opener.prg\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 }), diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 20, ' ;!IGNORE CLOSE'], [0, 20, ' ;!IGNORE CLOSE']] },
    { label: 'Information severity', text: 'dbopen(1) "ALREADY_OPEN",Id\n', diagnostics: [Object.assign(diag(ALREADY_OPEN, 0, { line: 0 }), { severity: 3 })], expect: [[0, 27, ' ;!IGNORE CLOSE']] },
    { label: 'other codes ignored', text: 'dbopen(1) "A",1\n', diagnostics: [diag('vertexadt/unused-variable', 0, { line: 0 }), diag(UNCLOSED, 0, { line: 0 })], expect: [[0, 15, ' ;!IGNORE CLOSE']] },
    { label: 'only quickfix', text: 'dbopen(1) "A",1\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], only: ['quickfix'], expect: [[0, 15, ' ;!IGNORE CLOSE']] },
    { label: 'only source.fixAll', text: 'dbopen(1) "A",1\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], only: ['source.fixAll'], expect: [] },
    { label: 'only refactor.extract', text: 'dbopen(1) "A",1\n', diagnostics: [diag(UNCLOSED, 0, { line: 0 })], only: ['refactor.extract'], expect: [] },
    { label: 'no diagnostics', text: 'dbopen(1) "A",1\n', diagnostics: [], expect: [] },
];

function expectedActions(uri, diagnostics, expect) {
    const matching = diagnostics.filter((d) => d.code === UNCLOSED || d.code === ALREADY_OPEN);
    return expect.map(([line, character, newText], i) => ({
        title: TITLE,
        kind: 'quickfix',
        diagnostics: [matching[i]],
        edit: {
            changes: {
                [uri]: [{
                    range: { start: { line, character }, end: { line, character } },
                    newText,
                }],
            },
        },
    }));
}

async function runGoldens(native, oracle) {
    await native.initialize();
    await oracle.initialize();

    // Capability frozen by Specify call-param code actions.
    assertEqual(
        native.initializeResult.capabilities.codeActionProvider,
        // `source`: pin inferred ##included_from (docs/adr/0015-native-inferred-included-from.md).
        { codeActionKinds: ['quickfix', 'source.fixAll', 'source', 'refactor.move'], resolveProvider: false },
        'codeActionProvider'
    );

    for (const row of FIXTURE_ROWS) {
        const uri = fixtureUri(row.name);
        const text = fixtureText(row.name);
        for (const session of [native, oracle]) {
            session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
        }
        const items = unclosedItems(await native.diagnostic(uri));
        assert(items.length === 1, `${row.name}: one unclosed diagnostic, got ${items.length}`);
        assert(items[0].code === row.code, `${row.name}: code ${items[0].code}`);
        assertEqual(items[0].data, { line: 0 }, `${row.name}: data`);

        const actions = await codeAction(native, uri, items);
        assertEqual(actions, await codeAction(oracle, uri, items), `${row.name}: native vs Node`);
        assert(actions.length === 1, `${row.name}: one action`);
        assert(actions[0].title === TITLE, `${row.name}: title`);
        assert(actions[0].kind === 'quickfix', `${row.name}: kind`);
        assert(actions[0].isPreferred === undefined && actions[0].command === undefined,
            `${row.name}: no isPreferred / command`);
        assertEqual(actions[0].diagnostics, items, `${row.name}: attaches the request diagnostic`);

        const applied = applyEdit(text, uri, actions[0]);
        assert(applied.split('\n')[0].replace(/\r$/, '') === row.line0,
            `${row.name}: applied line ${JSON.stringify(applied.split('\n')[0])}`);
        assertEqual(
            await codeAction(native, uri, items, ['source.fixAll']),
            [],
            `${row.name}: no SourceFixAll`
        );

        native.didChange({ uri, version: 2, contentChanges: [{ text: applied }] });
        assertEqual(unclosedItems(await native.diagnostic(uri)), [],
            `${row.name}: silenced after apply`);
        // The applied line already suppresses, so a stale diagnostic gets no action.
        assertEqual(await codeAction(native, uri, items), [], `${row.name}: no action once applied`);
    }

    // Matched open/close: nothing to fix.
    const closedUri = fixtureUri('closed_dbopen.prg');
    native.didOpen({ uri: closedUri, languageId: 'vertexadt', version: 1, text: fixtureText('closed_dbopen.prg') });
    const closedItems = unclosedItems(await native.diagnostic(closedUri));
    assertEqual(closedItems, [], 'closed_dbopen: no diagnostic');
    assertEqual(await codeAction(native, closedUri, closedItems), [], 'closed_dbopen: no action');

    for (let i = 0; i < SYNTHETIC_ROWS.length; i++) {
        const row = SYNTHETIC_ROWS[i];
        const uri = fixtureUri(`synthetic_${i}.prg`);
        for (const session of [native, oracle]) {
            session.didOpen({ uri, languageId: 'vertexadt', version: 1, text: row.text });
        }
        const actions = await codeAction(native, uri, row.diagnostics, row.only);
        assertEqual(actions, expectedActions(uri, row.diagnostics, row.expect), `${row.label}: native`);
        assertEqual(actions, await codeAction(oracle, uri, row.diagnostics, row.only),
            `${row.label}: native vs Node`);
    }

    // Missing range and data: skipped by both.
    {
        const uri = fixtureUri('synthetic_norange.prg');
        const diagnostics = [{ message: 'x', code: UNCLOSED }];
        for (const session of [native, oracle]) {
            session.didOpen({ uri, languageId: 'vertexadt', version: 1, text: 'dbopen(1) "A",1\n' });
        }
        assertEqual(await codeAction(native, uri, diagnostics), [], 'no line: native');
        assertEqual(await codeAction(oracle, uri, diagnostics), [], 'no line: Node');
    }

    // Missing context: no diagnostics, no actions.
    {
        const uri = fixtureUri('synthetic_0.prg');
        const params = { textDocument: { uri }, range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } } };
        assertEqual(await native.request('textDocument/codeAction', params), [], 'no context: native');
        assertEqual(await oracle.request('textDocument/codeAction', params), [], 'no context: Node');
    }

    // Unopened document: empty, like Node.
    {
        const uri = fixtureUri('not_open.prg');
        const diagnostics = [diag(UNCLOSED, 0, { line: 0 })];
        assertEqual(await codeAction(native, uri, diagnostics), [], 'unopened: native');
        assertEqual(await codeAction(oracle, uri, diagnostics), [], 'unopened: Node');
    }

    // Non-vertexadt: empty (Choose standard LSP methods vs custom requests).
    // Node does not filter by language; this row is native-only.
    {
        const uri = fixtureUri('plain.txt');
        native.didOpen({ uri, languageId: 'plaintext', version: 1, text: 'dbopen(1) "A",1\n' });
        assertEqual(await codeAction(native, uri, [diag(UNCLOSED, 0, { line: 0 })]), [], 'plaintext: empty');
    }

    await native.shutdownAndExit();
    await oracle.shutdownAndExit();
}

async function run() {
    const native = LspSession.launch(sessionOptions());
    const oracle = NodeOracleSession.launchNode(sessionOptions());
    try {
        await runGoldens(native, oracle);
    } catch (err) {
        native.kill();
        oracle.kill();
        throw err;
    }
    console.log('lsp golden: 43-ignore-close-code-action passed');
}

if (require.main === module) {
    run().catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run, runGoldens };
