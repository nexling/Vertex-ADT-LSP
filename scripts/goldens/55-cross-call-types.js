'use strict';

/**
 * Native-only goldens for cross-call types
 * (docs/adr/0009-native-cross-call-types.md). No Node oracle: the Node server
 * is frozen and has no cross-call typing.
 *
 * Copies test-fixtures/cross-call-types/ to a temp workspace (the startup
 * index walk finds the callers), then checks:
 *   - every cross-call diagnostic per file (range, severity, message, related)
 *   - a parameter typed only by its callers gets that type's semantic token
 *   - strict mode off, or crossCallTypeChecks false, silences the family
 *   - editing an open caller re-pulls the callee with the new types
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const SOURCE_FIXTURE = path.join(ROOT, 'test-fixtures', 'cross-call-types');
const PREFIX = 'vertexadt/cross-call-';

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

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    assert(actualJson === expectedJson, `${message}:\n  expected ${expectedJson}\n  got      ${actualJson}`);
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

// [line, startChar, endChar, severity, code suffix, message, related[]]
function summarize(report) {
    return ((report && report.items) || [])
        .filter((item) => String(item.code || '').startsWith(PREFIX))
        .map((item) => [
            item.range.start.line,
            item.range.start.character,
            item.range.end.character,
            item.severity,
            item.code.slice(PREFIX.length),
            item.message,
            (item.relatedInformation || []).map((info) => `${path.basename(decodeURIComponent(info.location.uri))}:`
                + `${info.location.range.start.line}:${info.location.range.start.character} ${info.message}`),
        ]);
}

const EXPECTED = {
    'callee_db.prg': [
        [1, 25, 53, 1, 'comment-type', 'Comment types studs as profile but callers pass database ($CALLPARAM1)',
            ['caller_a.prg:3:26 Passes database']],
        [3, 0, 3, 2, 'param-conflict', 'Callers pass different types for dir ($CALLPARAM3): dvector, value',
            ['caller_a.prg:3:32 Passes dvector', 'caller_b.prg:1:33 Passes value']],
        [4, 0, 5, 2, 'param-conflict', 'Callers pass different types for mixed ($CALLPARAM4): database, profile',
            ['caller_a.prg:3:34 Passes database', 'caller_b.prg:1:35 Passes profile']],
    ],
    'caller_a.prg': [
        [3, 26, 28, 1, 'argument-type', "Argument 1 'db' is database but the callee expects profile ($CALLPARAM1)",
            ['callee_db.prg:1:25 Declared as profile here']],
        [5, 10, 37, 1, 'return-comment-type', 'Comment types back as profile but the callee returns database for DB',
            ['callee_db.prg:5:8 Returned as database']],
    ],
    // The IGNORE TYPE line passes `pc` everywhere and contributes nothing.
    'caller_b.prg': [
        [1, 33, 34, 1, 'argument-type', "Argument 3 '5' is value but the callee expects pvector ($CALLPARAM3)",
            ['callee_db.prg:3:23 Declared as pvector here']],
    ],
    'chain_top.prg': [],
    // chain_top's database reaches chain_leaf through chain_mid's parameter.
    'chain_mid.prg': [
        [1, 27, 32, 1, 'argument-type', "Argument 1 'middb' is database but the callee expects value ($CALLPARAM1)",
            ['chain_leaf.prg:0:26 Declared as value here']],
    ],
    'chain_leaf.prg': [
        [0, 26, 47, 1, 'comment-type', 'Comment types leafdb as value but callers pass database ($CALLPARAM1)',
            ['chain_mid.prg:1:27 Passes database']],
    ],
    // A call cycle with no typed origin terminates and stays quiet.
    'cycle_a.prg': [],
    'cycle_b.prg': [],
    'self_sections.prg': [
        [1, 37, 39, 1, 'argument-type', "Argument 1 'pc' is profile but the callee expects database ($CALLPARAM1)",
            ['self_sections.prg:4:22 Declared as database here']],
        [4, 22, 48, 1, 'comment-type', 'Comment types wp as database but callers pass profile ($CALLPARAM1)',
            ['self_sections.prg:1:37 Passes profile']],
    ],
    'return_conflict.prg': [
        [2, 8, 11, 2, 'return-conflict', 'Return field OBJ is packed with different types: database, profile',
            ['return_conflict.prg:3:8 Packed as profile']],
        [3, 8, 11, 2, 'return-conflict', 'Return field OBJ is packed with different types: database, profile',
            ['return_conflict.prg:2:8 Packed as database']],
    ],
    // Callers of frag_parent.prg reach the `##included_from` fragment's reads.
    'frag_part.prg': [
        [1, 22, 49, 1, 'comment-type', 'Comment types fx as profile but callers pass database ($CALLPARAM1)',
            ['frag_caller.prg:1:28 Passes database']],
    ],
    'frag_caller.prg': [],
    'frag_parent.prg': [],
};

// Decoded semantic tokens: [line, char, length, legend name].
function decodeTokens(data, legend) {
    const out = [];
    let line = 0;
    let char = 0;
    for (let i = 0; i < data.length; i += 5) {
        line += data[i];
        char = data[i] === 0 ? char + data[i + 1] : data[i + 1];
        out.push([line, char, data[i + 2], legend[data[i + 3]]]);
    }
    return out;
}

async function main() {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'vertexadt-55-'));
    fs.cpSync(SOURCE_FIXTURE, dir, { recursive: true });
    const custom = path.join(dir, 'custom');
    const uriFor = (name) => pathToFileURL(path.join(custom, name)).href;
    const textOf = (name) => fs.readFileSync(path.join(custom, name), 'utf8');
    const rootUri = pathToFileURL(dir).href;

    const session = LspSession.launch({ rootUri, workspaceFolders: [{ uri: rootUri, name: 'cross-call-types' }] });
    try {
        recordRefreshes(session);
        const init = await session.initialize();
        const legend = init.capabilities.semanticTokensProvider.legend.tokenTypes;
        session.didChangeConfiguration({ vertexadt: { strictMode: { enabled: true } } });
        await waitFor(() => session.refreshes.includes('workspace/codeLens/refresh'), 'startup index walk');

        for (const name of Object.keys(EXPECTED)) {
            session.didOpen({ uri: uriFor(name), text: textOf(name) });
        }
        for (const [name, expected] of Object.entries(EXPECTED)) {
            assertEqual(summarize(await session.diagnostic(uriFor(name))), expected, `${name} cross-call diagnostics`);
        }

        // `pce` has no comment: both callers pass a profile.
        const tokens = decodeTokens((await session.semanticTokensFull(uriFor('callee_db.prg'))).data, legend);
        assertEqual(tokens.filter(([line, char]) => line === 2 && char === 0), [[2, 0, 3, 'pieceVariable']],
            'caller-typed pce token');
        // `back` in caller_a takes the returned type over its comment.
        const callerTokens = decodeTokens((await session.semanticTokensFull(uriFor('caller_a.prg'))).data, legend);
        assertEqual(callerTokens.filter(([line, char]) => line === 5 && char === 0), [[5, 0, 4, 'databaseVariable']],
            'returned-type back token');

        // Live: caller_b stops passing a profile as $CALLPARAM4.
        const before = session.refreshes.length;
        const edited = textOf('caller_b.prg').replace('call("callee_db.prg",db2,pc,5,pc)', 'call("callee_db.prg",db2,pc,5,db2)');
        session.didChange({ uri: uriFor('caller_b.prg'), version: 2, contentChanges: [{ text: edited }] });
        await waitFor(() => session.refreshes.slice(before).includes('workspace/diagnostic/refresh'),
            'diagnostic refresh after a caller edit');
        const live = summarize(await session.diagnostic(uriFor('callee_db.prg')));
        assertEqual(live.map((row) => [row[0], row[4]]), [[1, 'comment-type'], [3, 'param-conflict']],
            'callee after the caller edit');

        // Gates: the setting, then strict mode.
        session.didChangeConfiguration({ vertexadt: { strictMode: { enabled: true, crossCallTypeChecks: false } } });
        assertEqual(summarize(await session.diagnostic(uriFor('caller_a.prg'))), [], 'crossCallTypeChecks false');
        session.didChangeConfiguration({ vertexadt: { strictMode: { enabled: false, crossCallTypeChecks: true } } });
        assertEqual(summarize(await session.diagnostic(uriFor('caller_a.prg'))), [], 'strict mode off');
        // Typing stays on without strict mode.
        const looseTokens = decodeTokens((await session.semanticTokensFull(uriFor('callee_db.prg'))).data, legend);
        assertEqual(looseTokens.filter(([line, char]) => line === 2 && char === 0), [[2, 0, 3, 'pieceVariable']],
            'typing without strict mode');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

module.exports = { run: main, main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 55-cross-call-types passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
