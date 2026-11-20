#!/usr/bin/env node
'use strict';

/**
 * Parity goldens for Impact CodeLens (`textDocument/codeLens` +
 * `codeLens/resolve`).
 * Assertions from test-fixtures/workspace-declaration-index/run-impact-analysis-test.js
 * (2 incoming calls on shared_callee, two isolated programs, threshold 2 vs 3
 * and 1 vs 2 on sub_included, the commons-gated miss on callee_no_common) and
 * the spec titles from Specify Impact CodeLens:
 *   `Impact: 2 calls, 0 includes | Blast radius: 3 instances in 2 program(s)`
 *   `Impact: 0 calls, 1 includes | Blast radius: 2 instances in 1 program(s)`
 *
 * Oracle: the live Node language server (server/out/server.js --stdio). Every
 * fixture file is compared list and resolve, with no document open, so the
 * native startup index walk is what fills the index. Node keys paths by
 * lowercase absolute path; native by overlay identity (lowercase
 * library-relative). Paths and URIs are normalized before comparing. The
 * fixture is copied to a path without spaces: Node's findFile does not decode
 * `%20`.
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');
const { pathToFileURL, fileURLToPath } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const SOURCE_FIXTURE = path.join(ROOT, 'test-fixtures', 'workspace-declaration-index');
const NODE_SERVER = path.join(ROOT, 'server', 'out', 'server.js');

const COMMAND = 'vertexadt.showImpactAnalysis';
const BASE_SETTINGS = { maxNumberOfProblems: 100 };

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

function recordRefreshes(session) {
    session.refreshes = [];
    const base = session._replyToServerRequest.bind(session);
    session._replyToServerRequest = (message) => {
        if (/^workspace\/.*\/refresh$/.test(message.method || '')) {
            session.refreshes.push(message.method);
            session._write({ jsonrpc: '2.0', id: message.id, result: null });
            return;
        }
        base(message);
    };
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
                settings: { settings: BASE_SETTINGS },
            },
            workspaceFolders: this._workspaceFolders,
        });
        this.notify('initialized', {});
        return result;
    }

    _replyToServerRequest(message) {
        if (message.method === 'workspace/configuration') {
            const items = (message.params && message.params.items) || [];
            this._write({ jsonrpc: '2.0', id: message.id, result: items.map(() => BASE_SETTINGS) });
            return;
        }
        super._replyToServerRequest(message);
    }
}

function copyFixture() {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'vertexadt-46-'));
    assert(!dir.includes(' '), `temp fixture path has no spaces: ${dir}`);
    for (const tree of ['custom', 'system']) {
        fs.cpSync(path.join(SOURCE_FIXTURE, tree), path.join(dir, tree), { recursive: true });
    }
    return dir;
}

// Node abs lowercase path -> overlay identity (lowercase library-relative).
function nodePathToKey(root, nodePath) {
    const relative = path.relative(root.toLowerCase(), nodePath.toLowerCase()).replace(/\\/g, '/');
    return relative.replace(/^(custom|system)\//, '');
}

function normalizeUri(uri) {
    return path.normalize(fileURLToPath(uri)).toLowerCase();
}

function normalizeAnalysis(analysis, root, fromNode) {
    const key = (p) => (fromNode ? nodePathToKey(root, p) : p);
    return {
        incoming: {
            calls: analysis.incoming.calls,
            includes: analysis.incoming.includes,
            references: analysis.incoming.references.map((ref) => ({
                ...ref,
                fromPath: key(ref.fromPath),
                fromUri: normalizeUri(ref.fromUri),
            })),
        },
        scopes: analysis.scopes.map((scope) => ({
            ...scope,
            rootPath: key(scope.rootPath),
            rootUri: normalizeUri(scope.rootUri),
            files: scope.files.map((file) => ({
                ...file,
                path: key(file.path),
                uri: normalizeUri(file.uri),
            })),
        })),
        totalBlastRadiusFiles: analysis.totalBlastRadiusFiles,
    };
}

function normalizeLens(lens, root, fromNode) {
    const out = { range: lens.range, data: { ...lens.data, uri: normalizeUri(lens.data.uri) } };
    if (lens.command) {
        const [arg] = lens.command.arguments;
        out.command = {
            title: lens.command.title,
            command: lens.command.command,
            arguments: [{ uri: normalizeUri(arg.uri), analysis: normalizeAnalysis(arg.analysis, root, fromNode) }],
        };
    }
    return out;
}

const codeLens = (session, uri) => session.request('textDocument/codeLens', { textDocument: { uri } });
const resolve = (session, lens) => session.request('codeLens/resolve', lens);

async function listAndResolve(session, uri) {
    const lenses = await codeLens(session, uri);
    const resolved = [];
    for (const lens of lenses) {
        resolved.push(await resolve(session, lens));
    }
    return { lenses, resolved };
}

async function waitFor(predicate, label, timeoutMs = 10000) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        if (await predicate()) {
            return;
        }
        await new Promise((r) => setTimeout(r, 50));
    }
    throw new AssertionError(`timed out waiting for ${label}`);
}

async function runGoldens(native, oracle, root) {
    const custom = path.join(root, 'custom');
    const uriFor = (tree, name) => pathToFileURL(path.join(root, tree, name)).href;
    const customUri = (name) => uriFor('custom', name);
    const setNative = (impactAnalysis) =>
        native.didChangeConfiguration({ vertexadt: { ...BASE_SETTINGS, impactAnalysis } });
    const setNode = (impactAnalysis) => oracle.notify('custom/updateSettings', { impactAnalysis });

    recordRefreshes(native);
    const init = await native.initialize();
    await oracle.initialize();
    assertEqual(init.capabilities.codeLensProvider, { resolveProvider: true }, 'codeLensProvider capability');
    native.didChangeConfiguration({ vertexadt: BASE_SETTINGS });

    const sharedUri = customUri('shared_callee.prg');
    // The startup walk fills the index with nothing open.
    await waitFor(async () => native.refreshes.includes('workspace/codeLens/refresh'),
        'startup walk sends codeLens/refresh');
    assert((await codeLens(native, sharedUri)).length === 1, 'startup index walk lists shared_callee');
    await oracle.waitQuiet(300);

    // Every fixture file: list + resolve match Node at the default threshold.
    const files = [
        ...fs.readdirSync(custom).map((name) => ['custom', name]),
        ...fs.readdirSync(path.join(root, 'system')).map((name) => ['system', name]),
    ];
    const listed = [];
    for (const [tree, name] of files) {
        const uri = uriFor(tree, name);
        const ours = await listAndResolve(native, uri);
        const theirs = await listAndResolve(oracle, uri);
        assertEqual(
            ours.resolved.map((lens) => normalizeLens(lens, root, false)),
            theirs.resolved.map((lens) => normalizeLens(lens, root, true)),
            `${tree}/${name}: native vs Node`
        );
        if (ours.lenses.length > 0) {
            listed.push(`${tree}/${name}`);
        }
    }
    assert(listed.includes('custom/shared_callee.prg'), `shared_callee listed: ${listed}`);
    assert(!listed.includes('custom/sub_included.prg'), 'sub_included below default threshold 2');
    assert(!listed.includes('custom/callee_no_common.prg'), 'callee_no_common: ungated call does not count');
    assert(!listed.includes('system/custom_override.prg'), 'System twin while Custom occupies: no lens');

    // Pinned shape on shared_callee.
    {
        const [lens] = await codeLens(native, sharedUri);
        assertEqual(lens.range, { start: { line: 0, character: 0 }, end: { line: 0, character: 1 } }, 'lens range');
        assertEqual(lens.data, { kind: 'impact', uri: sharedUri }, 'lens data');
        assert(lens.command === undefined, 'unresolved lens has no command');
        const done = await resolve(native, lens);
        assert(done.command.title === 'Impact: 2 calls, 0 includes | Blast radius: 3 instances in 2 program(s)',
            `shared_callee title: ${done.command.title}`);
        assert(done.command.command === COMMAND, 'command id');
        const [arg] = done.command.arguments;
        assert(arg.uri === sharedUri, 'argument uri is the lens document');
        const callerA = arg.analysis.incoming.references[0];
        assert(normalizeUri(callerA.fromUri) === normalizeUri(customUri('caller_a.prg')), `fromUri: ${callerA.fromUri}`);
        assert(callerA.fromDisplayPath === 'caller_a.prg', 'fromDisplayPath');
        assertEqual(arg.analysis.incoming.references.map((r) => [r.fromPath, r.type]),
            [['caller_a.prg', 'call'], ['caller_b.prg', 'call']], 'incoming rows by overlay identity');
        assertEqual(arg.analysis.scopes.map((s) => s.rootDisplayPath), ['caller_a.prg', 'caller_b.prg'],
            'two isolated programs');
    }

    // Threshold 3 hides shared_callee; threshold 1 shows sub_included.
    {
        const before = native.refreshes.length;
        setNative({ enabled: true, minIncomingCount: 3 });
        await waitFor(async () => (await codeLens(native, sharedUri)).length === 0, 'threshold 3');
        assert(native.refreshes.slice(before).includes('workspace/codeLens/refresh'),
            'minIncomingCount change sends codeLens/refresh');

        setNative({ enabled: true, minIncomingCount: 1 });
        setNode({ enabled: true, minIncomingCount: 1 });
        const subUri = customUri('sub_included.prg');
        const ours = await listAndResolve(native, subUri);
        const theirs = await listAndResolve(oracle, subUri);
        assert(ours.lenses.length === 1, 'sub_included listed at threshold 1');
        assertEqual(
            ours.resolved.map((lens) => normalizeLens(lens, root, false)),
            theirs.resolved.map((lens) => normalizeLens(lens, root, true)),
            'sub_included threshold 1: native vs Node'
        );
        assert(ours.resolved[0].command.title === 'Impact: 0 calls, 1 includes | Blast radius: 2 instances in 1 program(s)',
            `sub_included title: ${ours.resolved[0].command.title}`);

        // 0 is not a valid threshold: falls back to 2.
        setNative({ enabled: true, minIncomingCount: 0 });
        await waitFor(async () => (await codeLens(native, subUri)).length === 0, 'minIncomingCount 0 -> 2');
        assert((await codeLens(native, sharedUri)).length === 1, 'shared_callee at fallback 2');
    }

    // Disabled: empty list, and the change refreshes.
    {
        const before = native.refreshes.length;
        setNative({ enabled: false, minIncomingCount: 2 });
        await waitFor(async () => (await codeLens(native, sharedUri)).length === 0, 'enabled false');
        assert(native.refreshes.slice(before).includes('workspace/codeLens/refresh'), 'disable sends codeLens/refresh');
        setNative({ minIncomingCount: 2 });
        await waitFor(async () => (await codeLens(native, sharedUri)).length === 1, 'missing enabled is on');
    }

    // Resolve: incoming 0 clears the command; non-impact data comes back unchanged.
    {
        const leafUri = customUri('caller_no_common.prg');
        const stale = {
            range: { start: { line: 0, character: 0 }, end: { line: 0, character: 1 } },
            data: { kind: 'impact', uri: leafUri },
            command: { title: 'stale', command: COMMAND },
        };
        const ours = await resolve(native, stale);
        const theirs = await resolve(oracle, stale);
        assert(ours.command === undefined, `incoming 0 clears command: ${JSON.stringify(ours)}`);
        assert(theirs.command === undefined, 'Node agrees');
        const other = { range: stale.range, data: { kind: 'other' } };
        assertEqual(await resolve(native, other), other, 'non-impact data unchanged');
    }

    // Open non-`vertexadt` document: empty.
    {
        native.didOpen({ uri: sharedUri, languageId: 'plaintext', version: 1, text: '' });
        assertEqual(await codeLens(native, sharedUri), [], 'plaintext: no lens');
        native.didClose(sharedUri);
    }

    // Open buffers win in the index: a new commons-gated caller adds a call.
    {
        const extraUri = customUri('caller_c_extra.prg');
        native.didOpen({
            uri: extraUri,
            languageId: 'vertexadt',
            version: 1,
            text: 'common_indexes 1\ncall("shared_callee.prg", "")\n',
        });
        await waitFor(async () => {
            const [lens] = await codeLens(native, sharedUri);
            const done = lens && (await resolve(native, lens));
            return done && done.command.title.startsWith('Impact: 3 calls');
        }, 'open caller indexed');
        native.didClose(extraUri);
        await waitFor(async () => {
            const [lens] = await codeLens(native, sharedUri);
            const done = lens && (await resolve(native, lens));
            return done && done.command.title.startsWith('Impact: 2 calls');
        }, 'closed caller dropped (no file on disk)');
    }

    // Workspace-folder change re-walks the tree (not just clears it).
    {
        const folder = { uri: pathToFileURL(root).href, name: path.basename(root) };
        native.notify('workspace/didChangeWorkspaceFolders', {
            event: { added: [folder], removed: [folder] },
        });
        await native.waitQuiet(700);
        await waitFor(async () => (await codeLens(native, sharedUri)).length === 1, 'folder-change re-walk');
    }
}

async function run() {
    const root = copyFixture();
    const rootUri = pathToFileURL(root).href;
    const options = {
        rootUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: path.basename(root) }],
    };
    const native = LspSession.launch(options);
    const oracle = NodeOracleSession.launchNode(options);
    try {
        await runGoldens(native, oracle, root);
        await native.shutdownAndExit();
        oracle.kill();
    } catch (err) {
        native.kill();
        oracle.kill();
        throw err;
    } finally {
        fs.rmSync(root, { recursive: true, force: true });
    }
    console.log('lsp golden: 46-impact-codelens passed');
}

module.exports = { run };

if (require.main === module) {
    run().catch((err) => {
        console.error(err);
        process.exit(1);
    });
}
