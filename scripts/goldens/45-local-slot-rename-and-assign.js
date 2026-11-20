#!/usr/bin/env node
'use strict';

/**
 * Parity goldens for the callee-local-naming rename QuickFix and the
 * `vertexadt.assignLocalSlotNames` workspace command.
 * Assertions from test-fixtures/local-slot-names/TESTING.md Scenario 6 (every
 * `foo` in callee_bad_names.prg, including `set(foo, 1)`, becomes `l19foo`)
 * plus the smoke runner's `l3C` -> `l1C` pair on a non-callee.
 *
 * Oracle: the live Node language server (server/out/server.js --stdio).
 * QuickFix actions must match Node byte for byte. The workspace command is
 * compared to Node's `custom/assignLocalSlotNames` after normalizing URIs (Node
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
const SOURCE_FIXTURE = path.join(ROOT, 'test-fixtures', 'local-slot-names');
const NODE_SERVER = path.join(ROOT, 'server', 'out', 'server.js');

const NAMING = 'vertexadt/callee-local-naming';
const COMMAND = 'vertexadt.assignLocalSlotNames';
const SETTINGS = {
    strictMode: { enabled: true },
    locals: { validateUniqueNames: true, requireLNamesInCallees: true },
    maxNumberOfProblems: 100,
};

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
                settings: { settings: SETTINGS },
            },
            workspaceFolders: this._workspaceFolders,
        });
        this.notify('initialized', {});
        return result;
    }

    _replyToServerRequest(message) {
        if (message.method === 'workspace/configuration') {
            const items = (message.params && message.params.items) || [];
            this._write({ jsonrpc: '2.0', id: message.id, result: items.map(() => SETTINGS) });
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'vertexadt-45-'));
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

// Applies reverse-sorted single-line edits (as the server returns them).
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

function namingDiag(line, varName, newName) {
    return {
        range: { start: { line, character: 0 }, end: { line, character: varName.length } },
        message: 'synthetic',
        severity: 2,
        code: NAMING,
        source: 'ADT-Syntax',
        data: { line, varName, newName, sectionName: '', fileWide: true },
    };
}

function assignNative(session, uri) {
    return session.request('workspace/executeCommand', {
        command: COMMAND,
        arguments: uri === undefined ? [] : [{ uri }],
    });
}

async function assertCommandParity(native, oracle, uri, label) {
    const ours = await assignNative(native, uri);
    const theirs = await oracle.request('custom/assignLocalSlotNames', { uri });
    assertEqual(
        { ...ours, changes: normalizeChanges(ours.changes) },
        { ...theirs, changes: normalizeChanges(theirs.changes) },
        `${label}: native vs Node`
    );
    return ours;
}

async function runGoldens(native, oracle, custom) {
    const uriFor = (name) => pathToFileURL(path.join(custom, name)).href;
    const textOf = (name) => fs.readFileSync(path.join(custom, name), 'utf8');
    const open = (name, text = textOf(name), languageId = 'vertexadt') => {
        for (const session of [native, oracle]) {
            session.didOpen({ uri: uriFor(name), languageId, version: 1, text });
        }
    };
    const close = (name) => {
        for (const session of [native, oracle]) {
            session.didClose(uriFor(name));
        }
    };

    await native.initialize();
    await oracle.initialize();
    native.didChangeConfiguration({ vertexadt: SETTINGS });

    const prgs = fs.readdirSync(custom).filter((name) => name.endsWith('.prg'));
    for (const name of prgs) {
        open(name);
    }
    await native.waitQuiet(400);
    await oracle.waitQuiet(400);

    const badName = 'callee_bad_names.prg';
    const badUri = uriFor(badName);
    const badText = textOf(badName);
    const scenario6 = badText.replace(/\bfoo\b/g, 'l19foo');
    assert(scenario6.includes('l19foo = 5') && scenario6.includes('set(l19foo, 1)'), 'Scenario 6 expected text');

    // Family 16 publishes the `data` the QuickFix consumes.
    const report = await native.diagnostic(badUri);
    const naming = (report.items || []).filter((item) => item.code === NAMING);
    assert(naming.length === 1, `Scenario 6: one naming diagnostic, got ${JSON.stringify(report.items)}`);
    assertEqual(
        naming[0].data,
        { line: 1, varName: 'foo', newName: 'l19foo', sectionName: naming[0].data.sectionName, fileWide: true },
        'Scenario 6: naming data'
    );

    // QuickFix: byte-for-byte Node; resulting text is Scenario 6.
    {
        const ours = await codeAction(native, badUri, naming);
        const theirs = await codeAction(oracle, badUri, naming);
        assertEqual(ours, theirs, 'Scenario 6 QuickFix: native vs Node');
        assert(ours.length === 1, 'Scenario 6: one rename action');
        assert(ours[0].title === 'Rename foo to l19foo in file', `title: ${ours[0].title}`);
        assert(ours[0].kind === 'quickfix', 'kind quickfix');
        assert(ours[0].isPreferred === undefined, 'isPreferred unset');
        assertEqual(ours[0].diagnostics, naming, 'diagnostic attached');
        const edits = ours[0].edit.changes[badUri];
        assert(edits.length === 2, 'declaration and set(foo, 1)');
        assert(applyEdits(badText, edits) === scenario6, 'Scenario 6 QuickFix text');
    }

    // No SourceFixAll for this family; no `data` -> no action.
    {
        assertEqual(await codeAction(native, badUri, naming, ['source.fixAll']), [], 'source.fixAll only: none');
        const bare = { ...naming[0] };
        delete bare.data;
        assertEqual(await codeAction(native, badUri, [bare]), [], 'no data: no action');
        assertEqual(await codeAction(oracle, badUri, [bare]), [], 'no data: Node agrees');
    }

    // Two declarations of one name: two identical QuickFixes.
    {
        const twice = [namingDiag(1, 'foo', 'l19foo'), namingDiag(1, 'foo', 'l19foo')];
        const ours = await codeAction(native, badUri, twice);
        assertEqual(ours, await codeAction(oracle, badUri, twice), 'duplicate diagnostics: native vs Node');
        assert(ours.length === 2 && ours[0].title === ours[1].title, 'two identical QuickFixes');
    }

    // Token rules: case per usage, quotes, bang lines, `;!` tails, `foo$`,
    // `#EDIT_FORM#` not skipped, UTF-16 columns, canonical lowercase prefix.
    {
        const probeName = 'rename_probe.prg';
        const probeText = [
            '@declarationnumbercheckstart',
            'foo = 5',
            'L3C = 7',
            '@declarationnumbercheckend',
            'Foo = FOO + fOo',
            'x$ = "foo" + \'foo\'',
            '! foo here',
            'y = foo ;! foo tail',
            'foo$ = foobar + foo_1',
            'åäö = foo; z = set(foo, 1)',
            '#EDIT_FORM#',
            'w = foo',
            '#END_EDIT_FORM#',
            'exit',
            '',
        ].join('\r\n');
        open(probeName, probeText);
        const probeUri = uriFor(probeName);
        for (const diags of [
            [namingDiag(1, 'foo', 'l19foo')],
            [namingDiag(2, 'l3C', 'l1C')],
            [namingDiag(1, 'nothere', 'l19nothere')],
        ]) {
            const ours = await codeAction(native, probeUri, diags);
            assertEqual(ours, await codeAction(oracle, probeUri, diags), `probe ${diags[0].data.varName}: native vs Node`);
        }
        const [fooAction] = await codeAction(native, probeUri, [namingDiag(1, 'foo', 'l19foo')]);
        const renamed = applyEdits(probeText, fooAction.edit.changes[probeUri]).split('\r\n');
        assert(renamed[4] === 'l19Foo = l19FOO + l19fOo', `per-usage case: ${renamed[4]}`);
        assert(renamed[5] === probeText.split('\r\n')[5], 'quoted foo kept');
        assert(renamed[6] === '! foo here', 'bang line kept');
        assert(renamed[7] === 'y = l19foo ;! foo tail', `;! tail kept: ${renamed[7]}`);
        assert(renamed[8] === 'foo$ = foobar + foo_1', 'foo$ / foobar / foo_1 kept');
        assert(renamed[9] === 'åäö = l19foo; z = set(l19foo, 1)', `UTF-16 columns: ${renamed[9]}`);
        assert(renamed[11] === 'w = l19foo', '#EDIT_FORM# body rewritten');
        const [lAction] = await codeAction(native, probeUri, [namingDiag(2, 'l3C', 'l1C')]);
        const lRenamed = applyEdits(probeText, lAction.edit.changes[probeUri]).split('\r\n');
        assert(lRenamed[2] === 'l1C = 7', `canonical lowercase prefix: ${lRenamed[2]}`);
        close(probeName);
    }

    // Workspace command, Scenario 6 on the open buffer.
    {
        const ours = await assertCommandParity(native, oracle, badUri, 'Scenario 6 command');
        assert(ours.assignedCount === 2, `assignedCount is the edit count: ${ours.assignedCount}`);
        assert(ours.error === undefined, 'no error');
        assertEqual(Object.keys(ours.changes), [badUri], 'changes keyed by the request URI');
        assert(applyEdits(badText, ours.changes[badUri]) === scenario6, 'Scenario 6 command text');
    }

    // Open buffer wins over disk.
    {
        const edited = badText.replace('foo = 5', 'foo = 5\nbar = 6');
        for (const session of [native, oracle]) {
            session.didChange({ uri: badUri, version: 2, contentChanges: [{ text: edited }] });
        }
        await native.waitQuiet();
        const ours = await assertCommandParity(native, oracle, badUri, 'buffer wins');
        const text = applyEdits(edited, ours.changes[badUri]);
        assert(text.includes('l19bar = 6') && text.includes('l19foo = 5'), `one prefix per file: ${text}`);
        for (const session of [native, oracle]) {
            session.didChange({ uri: badUri, version: 3, contentChanges: [{ text: badText }] });
        }
    }

    // Wrong-prefix local slot on a non-callee: l3C -> l1C, manifest line kept.
    {
        const name = 'callee_manifest_bad.prg';
        const ours = await assertCommandParity(native, oracle, uriFor(name), 'manifest bad');
        const text = applyEdits(textOf(name), ours.changes[uriFor(name)]);
        assert(text.includes('l1C = 3'), `l3C -> l1C: ${text}`);
        assert(text.includes('@local_slots l1A,l1B'), '@local_slots line unchanged');
    }

    // Every other fixture file, then a closed (disk) target.
    for (const name of prgs) {
        await assertCommandParity(native, oracle, uriFor(name), `command ${name}`);
    }
    close(badName);
    await native.waitQuiet();
    {
        const ours = await assertCommandParity(native, oracle, badUri, 'closed: disk text');
        assert(ours.assignedCount === 2, 'closed: two edits from disk');
    }

    // No callee / setting gate on the command.
    native.didChangeConfiguration({
        vertexadt: { ...SETTINGS, locals: { ...SETTINGS.locals, requireLNamesInCallees: false } },
    });
    await native.waitQuiet();
    {
        const ours = await assignNative(native, badUri);
        assert(ours.assignedCount === 2, 'family 16 off: command still renames');
    }

    // Errors and non-vertexadt.
    {
        assertEqual(await assignNative(native, undefined),
            { changes: {}, assignedCount: 0, error: 'No target file specified' }, 'no uri');
        assertEqual(await assignNative(native, 'untitled:Untitled-1'),
            { changes: {}, assignedCount: 0, error: 'Invalid target file URI' }, 'non-file uri');
        const missing = uriFor('does_not_exist.prg');
        assertEqual(await assignNative(native, missing),
            { changes: {}, assignedCount: 0, error: 'Could not read target file' }, 'missing file');
        assertEqual(await oracle.request('custom/assignLocalSlotNames', { uri: missing }),
            { changes: {}, assignedCount: 0, error: 'Could not read target file' }, 'missing file: Node agrees');

        const plainUri = uriFor('plain.txt');
        native.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: 'foo = 5\n' });
        assertEqual(await assignNative(native, plainUri), { changes: {}, assignedCount: 0 }, 'plaintext: empty');
        assertEqual(await codeAction(native, plainUri, [namingDiag(0, 'foo', 'l19foo')]), [], 'plaintext: no actions');
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
        workspaceFolders: [{ uri: rootUri, name: 'local-slot-names' }],
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
    console.log('lsp golden: 45-local-slot-rename-and-assign passed');
}

if (require.main === module) {
    run().catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run, runGoldens };
