#!/usr/bin/env node
'use strict';

/**
 * Parity goldens for call-param code actions and the
 * `vertexadt.fixMissingCallParameters` workspace command.
 * Assertions from test-fixtures/call-param-validation/TESTING.md (scenario 1
 * quick-fix text, scenario 4 workspace fix) plus the extra-removal text the
 * Node smoke runner pins.
 *
 * Oracle: the live Node language server (server/out/server.js --stdio).
 * QuickFix actions must match Node byte for byte. SourceFixAll must not copy
 * Node's `character` vs `startCharacter` range bug, so it is asserted against
 * the batched QuickFix inserts instead. The workspace command is compared to
 * Node's `custom/fixMissingCallParameters` after normalizing URIs (Node
 * lowercases paths). The fixture is copied to a path without spaces: Node's
 * findFile does not decode `%20`.
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');
const { pathToFileURL, fileURLToPath } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const SOURCE_FIXTURE = path.join(ROOT, 'test-fixtures', 'call-param-validation');
const NODE_SERVER = path.join(ROOT, 'server', 'out', 'server.js');

const MISSING = 'vertexadt/missing-call-params';
const EXTRA = 'vertexadt/extra-call-params';
const COMMAND = 'vertexadt.fixMissingCallParameters';

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
        // Node rejects unhandled refresh replies and exits; ack them.
        if (/^workspace\/.*\/refresh$/.test(message.method || '')) {
            this._write({ jsonrpc: '2.0', id: message.id, result: null });
            return;
        }
        super._replyToServerRequest(message);
    }
}

function copyFixture() {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'vertexadt-44-'));
    assert(!dir.includes(' '), `temp fixture path has no spaces: ${dir}`);
    fs.cpSync(SOURCE_FIXTURE, dir, { recursive: true });
    return dir;
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

// Applies edits in the order given (callers pass reverse-sorted edits, as the
// server returns them), the way the editor applies a non-overlapping set.
function applyEdits(text, edits) {
    const lines = text.split('\n');
    for (const { range, newText } of edits) {
        assert(range.start.line === range.end.line, 'single-line edit');
        const raw = lines[range.start.line];
        const hasCr = raw.endsWith('\r');
        const body = hasCr ? raw.slice(0, -1) : raw;
        lines[range.start.line] = body.slice(0, range.start.character) + newText
            + body.slice(range.end.character) + (hasCr ? '\r' : '');
    }
    return lines.join('\n');
}

function callParamItems(report) {
    return ((report && report.items) || []).filter((item) => item.code === MISSING || item.code === EXTRA);
}

function normalizeUriKey(uri) {
    return path.normalize(fileURLToPath(uri)).toLowerCase();
}

function normalizeChanges(changes) {
    const out = {};
    for (const uri of Object.keys(changes || {})) {
        out[normalizeUriKey(uri)] = changes[uri];
    }
    return out;
}

function missingDiag(line, insertOffset, missingCount) {
    return {
        range: { start: { line, character: 0 }, end: { line, character: 1 } },
        message: 'synthetic',
        severity: 1,
        code: MISSING,
        source: 'ADT-Syntax',
        data: { line, insertOffset, missingCount },
    };
}

function insertEdit(line, character, newText) {
    return { range: { start: { line, character }, end: { line, character } }, newText };
}

async function runGoldens(native, oracle, custom) {
    const uriFor = (name) => pathToFileURL(path.join(custom, name)).href;
    const textOf = (name) => fs.readFileSync(path.join(custom, name), 'utf8');

    await native.initialize();
    await oracle.initialize();

    assertEqual(
        native.initializeResult.capabilities.codeActionProvider,
        // `source`: pin inferred ##included_from (docs/adr/0015-native-inferred-included-from.md).
        { codeActionKinds: ['quickfix', 'source.fixAll', 'source', 'refactor.move'], resolveProvider: false },
        'codeActionProvider'
    );
    // Empty list: vscode-languageclient would register listed IDs as VS Code
    // commands and collide with the host's package.json registrations.
    assertEqual(
        native.initializeResult.capabilities.executeCommandProvider,
        { commands: [] },
        'executeCommandProvider'
    );

    // Callees open in both so expected counts resolve the same way.
    for (const name of ['callee_three_params.prg', 'sectional_callee.prg', 'callee_no_params.prg']) {
        for (const session of [native, oracle]) {
            session.didOpen({ uri: uriFor(name), languageId: 'vertexadt', version: 1, text: textOf(name) });
        }
    }
    await native.waitQuiet();

    // Scenario 1: data payload, QuickFix, SourceFixAll, applied text.
    {
        const name = 'caller_missing_one.prg';
        const uri = uriFor(name);
        const text = textOf(name);
        for (const session of [native, oracle]) {
            session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
        }
        const items = callParamItems(await native.diagnostic(uri));
        assert(items.length === 1, `scenario 1: one diagnostic, got ${items.length}`);
        assertEqual(items[0].data, {
            line: 0,
            insertOffset: 49,
            missingCount: 1,
            expected: 3,
            actual: 2,
            calleeDisplayPath: '"callee_three_params.prg"',
        }, 'scenario 1: missing data');

        const actions = await codeAction(native, uri, items);
        assertEqual(actions.map((a) => [a.title, a.kind]), [
            ['Add 1 missing call parameter(s) as ""', 'quickfix'],
            ['Add missing call parameters as "" in file (1)', 'source.fixAll'],
        ], 'scenario 1: lightbulb offers QuickFix then SourceFixAll');
        const [quickFix, fixAll] = actions;
        assertEqual(quickFix.diagnostics, items, 'QuickFix attaches the request diagnostic');
        assert(fixAll.diagnostics === undefined, 'SourceFixAll attaches no diagnostics');
        assert(quickFix.isPreferred === undefined && fixAll.isPreferred === undefined, 'isPreferred unset');
        assertEqual(quickFix.edit, { changes: { [uri]: [insertEdit(0, 49, ',""')] } }, 'QuickFix edit');
        assertEqual(fixAll.edit, quickFix.edit, 'SourceFixAll = batched QuickFix inserts');

        const expected = 'ret$=call("callee_three_params.prg",Area,Selemobj,"")';
        for (const action of actions) {
            const applied = applyEdits(text, action.edit.changes[uri]);
            assert(applied.split('\n')[0].replace(/\r$/, '') === expected,
                `scenario 1 (${action.kind}): applied ${JSON.stringify(applied.split('\n')[0])}`);
        }

        assertEqual(
            await codeAction(native, uri, items, ['quickfix']),
            await codeAction(oracle, uri, items, ['quickfix']),
            'scenario 1 QuickFix: native vs Node'
        );
        assertEqual(await codeAction(native, uri, items, ['source.fixAll']), [fixAll], 'only source.fixAll');
        assertEqual(await codeAction(native, uri, items, ['quickfix']), [quickFix], 'only quickfix');
        assertEqual(await codeAction(native, uri, items, ['refactor.extract']), [], 'only refactor.extract');

        const applied = applyEdits(text, quickFix.edit.changes[uri]);
        native.didChange({ uri, version: 2, contentChanges: [{ text: applied }] });
        assertEqual(callParamItems(await native.diagnostic(uri)), [], 'scenario 1: silenced after apply');
        native.didChange({ uri, version: 3, contentChanges: [{ text }] });
    }

    // Scenario 3: extra has no lightbulb; data carries the removal range.
    {
        const name = 'caller_extra_one.prg';
        const uri = uriFor(name);
        const text = textOf(name);
        for (const session of [native, oracle]) {
            session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
        }
        const items = callParamItems(await native.diagnostic(uri));
        assert(items.length === 1 && items[0].code === EXTRA, 'scenario 3: one extra diagnostic');
        assertEqual(items[0].data, {
            line: 0,
            extraCount: 1,
            expected: 3,
            actual: 4,
            calleeDisplayPath: '"callee_three_params.prg"',
            removeStart: 53,
            removeEnd: 61,
        }, 'scenario 3: extra data');
        assertEqual(await codeAction(native, uri, items), [], 'scenario 3: no lightbulb for extra');
        assertEqual(await codeAction(oracle, uri, items), [], 'scenario 3: Node agrees');
    }

    // Sectional: two missing sites batch into one reverse-sorted SourceFixAll.
    {
        const name = 'caller_sectional.prg';
        const uri = uriFor(name);
        const text = textOf(name);
        for (const session of [native, oracle]) {
            session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
        }
        const items = callParamItems(await native.diagnostic(uri));
        const actions = await codeAction(native, uri, items);
        assertEqual(actions.map((a) => a.title), [
            'Add 1 missing call parameter(s) as ""',
            'Add 1 missing call parameter(s) as ""',
            'Add missing call parameters as "" in file (2)',
        ], 'sectional: two QuickFixes + SourceFixAll site count');
        const fixAll = actions[2];
        assertEqual(fixAll.edit.changes[uri], [insertEdit(5, 34, ',""'), insertEdit(1, 40, ',""')],
            'sectional: SourceFixAll reverse-sorted');
        assertEqual(
            await codeAction(native, uri, items, ['quickfix']),
            await codeAction(oracle, uri, items, ['quickfix']),
            'sectional QuickFix: native vs Node'
        );
        const applied = applyEdits(text, fixAll.edit.changes[uri]).split('\n').map((l) => l.replace(/\r$/, ''));
        assert(applied[1] === 'call("sectional_callee.prg OTHER", extra,"")', `sectional line 1: ${applied[1]}`);
        assert(applied[5] === 'call("sectional_callee.prg", extra,"")', `sectional line 5: ${applied[5]}`);
    }

    // Synthetic request rows: data validation, counts, same-line sort.
    {
        const uri = uriFor('synthetic.prg');
        const text = 'call("callee_three_params.prg") call("callee_three_params.prg",a)\n';
        for (const session of [native, oracle]) {
            session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
        }
        const rows = [
            { label: 'three missing', diagnostics: [missingDiag(0, 30, 3)] },
            { label: 'no data', diagnostics: [Object.assign(missingDiag(0, 30, 1), { data: undefined })] },
            { label: 'zero missing', diagnostics: [missingDiag(0, 30, 0)] },
            { label: 'string insertOffset', diagnostics: [Object.assign(missingDiag(0, 30, 1), { data: { line: 0, insertOffset: '30', missingCount: 1 } })] },
            { label: 'two on one line', diagnostics: [missingDiag(0, 30, 3), missingDiag(0, 64, 2)] },
            { label: 'extra code ignored', diagnostics: [Object.assign(missingDiag(0, 30, 1), { code: EXTRA })] },
        ];
        for (const row of rows) {
            const diagnostics = JSON.parse(JSON.stringify(row.diagnostics));
            assertEqual(
                await codeAction(native, uri, diagnostics, ['quickfix']),
                await codeAction(oracle, uri, diagnostics, ['quickfix']),
                `${row.label}: QuickFix native vs Node`
            );
        }
        const three = await codeAction(native, uri, [missingDiag(0, 30, 3)], ['quickfix']);
        assertEqual(three[0].edit.changes[uri], [insertEdit(0, 30, ',"","",""')],
            'three missing: `,""` x 3');
        assert(three[0].title === 'Add 3 missing call parameter(s) as ""', 'three missing title');

        const both = await codeAction(native, uri, [missingDiag(0, 30, 3), missingDiag(0, 64, 2)], ['source.fixAll']);
        assertEqual(both[0].edit.changes[uri], [insertEdit(0, 64, ',"",""'), insertEdit(0, 30, ',"","",""')],
            'same line: later character first');
        assertEqual(applyEdits(text, both[0].edit.changes[uri]).split('\n')[0],
            'call("callee_three_params.prg","","","") call("callee_three_params.prg",a,"","")',
            'same line: applied text');
        assertEqual(await codeAction(native, uri, [Object.assign(missingDiag(0, 30, 1), { data: undefined })]), [],
            'no data: no SourceFixAll either');
    }

    // Scenario 4: workspace command. Index-only files (not opened) come from
    // disk; open buffers win. Compared to Node after URI normalization.
    {
        const skipsUri = uriFor('caller_skips.prg');
        // Edited buffer must win over disk for both servers.
        const skipsBuffer = 'ret$=call("callee_three_params.prg",Area)\n';
        for (const session of [native, oracle]) {
            session.didOpen({ uri: skipsUri, languageId: 'vertexadt', version: 1, text: skipsBuffer });
        }
        await native.waitQuiet();

        const result = await native.request('workspace/executeCommand', { command: COMMAND, arguments: [] });
        const nodeResult = await oracle.request('custom/fixMissingCallParameters');
        assertEqual(normalizeChanges(result.changes), normalizeChanges(nodeResult.changes),
            'scenario 4: changes native vs Node');
        assert(result.fixedCount === nodeResult.fixedCount,
            `scenario 4: fixedCount ${result.fixedCount} vs Node ${nodeResult.fixedCount}`);

        const byName = {};
        for (const uri of Object.keys(result.changes)) {
            byName[path.basename(fileURLToPath(uri))] = result.changes[uri];
        }
        const apply = (name, text) => applyEdits(text, byName[name] || []).split('\n')[0].replace(/\r$/, '');
        assert(apply('caller_missing_one.prg', textOf('caller_missing_one.prg'))
            === 'ret$=call("callee_three_params.prg",Area,Selemobj,"")', 'scenario 4: missing filled');
        assert(apply('caller_extra_one.prg', textOf('caller_extra_one.prg'))
            === 'ret$=call("callee_three_params.prg",Area,Selemobj,"1")', 'scenario 4: extra removed');
        assert(apply('caller_skips.prg', skipsBuffer)
            === 'ret$=call("callee_three_params.prg",Area,"","")', 'scenario 4: open buffer wins over disk');
        assert(!byName['caller_ok.prg'], 'scenario 4: valid call untouched');
        const fileCount = Object.values(result.changes).reduce((n, edits) => n + edits.length, 0);
        assert(fileCount === result.fixedCount, 'fixedCount = edit count');

        // The re-walk ignores the collector's #EDIT_FORM# skip (disk text).
        native.didClose(skipsUri);
        oracle.didClose(skipsUri);
        await native.waitQuiet();
        const reopened = await native.request('workspace/executeCommand', { command: COMMAND });
        const nodeReopened = await oracle.request('custom/fixMissingCallParameters');
        assertEqual(normalizeChanges(reopened.changes), normalizeChanges(nodeReopened.changes),
            'scenario 4 after close: native vs Node');
        const skipsEdits = Object.entries(reopened.changes)
            .find(([uri]) => uri.toLowerCase().endsWith('caller_skips.prg'));
        assert(skipsEdits && skipsEdits[1].some((edit) => edit.range.start.line === 4),
            'scenario 4: #EDIT_FORM# body line rewritten by the workspace walk');
    }

    // Unknown command: error, no crash.
    {
        const payload = await native.requestAllowError('workspace/executeCommand', { command: 'vertexadt.nope' });
        assert(payload.error, 'unknown command: error response');
    }

    // Non-vertexadt: empty.
    {
        const uri = uriFor('plain.txt');
        native.didOpen({ uri, languageId: 'plaintext', version: 1, text: 'call("callee_three_params.prg")\n' });
        assertEqual(await codeAction(native, uri, [missingDiag(0, 30, 3)]), [], 'plaintext: empty');
    }

    // Setting off: no call-param actions, empty command result.
    {
        native.didChangeConfiguration({ calls: { validateParameterCount: false } });
        await native.waitQuiet();
        const uri = uriFor('caller_missing_one.prg');
        assertEqual(await codeAction(native, uri, [missingDiag(0, 49, 1)]), [], 'setting off: no actions');
        assertEqual(
            await native.request('workspace/executeCommand', { command: COMMAND }),
            { changes: {}, fixedCount: 0 },
            'setting off: empty command result'
        );
    }

    await native.shutdownAndExit();
    await oracle.shutdownAndExit();
}

async function run() {
    const dir = copyFixture();
    const rootUri = pathToFileURL(dir).href;
    const options = {
        rootUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: 'call-param-validation' }],
    };
    const native = LspSession.launch(options);
    const oracle = NodeOracleSession.launchNode(options);
    try {
        await runGoldens(native, oracle, path.join(dir, 'custom'));
    } catch (err) {
        native.kill();
        oracle.kill();
        throw err;
    } finally {
        fs.rmSync(dir, { recursive: true, force: true });
    }
    console.log('lsp golden: 44-call-param-code-actions passed');
}

if (require.main === module) {
    run().catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run, runGoldens };
