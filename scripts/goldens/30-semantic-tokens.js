'use strict';

// Semantic-token goldens: test-fixtures/section-aware-types/TESTING.md.
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const REPO_ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(REPO_ROOT, 'test-fixtures', 'section-aware-types');

const LEGEND = [
    'variable',
    'iteratorVariable',
    'edgeVariable',
    'selemVariable',
    'pieceEndVariable',
    'vxgeomVariable',
    'pieceVariable',
    'sheetVariable',
    'areaVariable',
    'jointVariable',
    'pointVariable',
    'slopeVariable',
    'polylineVariable',
    'arrayVariable',
    'valueVariable',
    'wallVariable',
    'booleanVariable',
    'pvectorVariable',
    'xvectorVariable',
    'yvectorVariable',
    'zvectorVariable',
    'nvectorVariable',
    'lineVariable',
    'hatchVariable',
    'textVariable',
    'dvectorVariable',
    'macroVariable',
    'databaselistVariable',
    'databaseVariable',
    'validatedCommandVariable',
    // Debug traces (docs/adr/0025-debug-trace-fast-path.md), appended.
    'keyword',
    'string',
    'number',
    'comment',
    'function',
    'debugValue',
    'debugMarker',
];

const DEBUG_FIXTURE = path.join(REPO_ROOT, 'test-fixtures', 'debug-trace', 'sample.dbg');

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
    assert(actualJson === expectedJson, `${message}: expected ${expectedJson}, got ${actualJson}`);
}

function fixture(relative) {
    const filePath = path.join(FIXTURE, ...relative.split('/'));
    const text = fs.readFileSync(filePath, 'utf8');
    return { filePath, uri: pathToFileURL(filePath).href, text, lines: text.split(/\r?\n/) };
}

function lineIndex(lines, text) {
    const i = lines.indexOf(text);
    if (i < 0) {
        throw new Error(`missing fixture line ${JSON.stringify(text)}`);
    }
    return i;
}

function wordAt(lineText, word, occurrence = 1) {
    let from = 0;
    for (let n = 1; ; n++) {
        const at = lineText.indexOf(word, from);
        if (at < 0) {
            throw new Error(`missing ${JSON.stringify(word)} #${occurrence} on ${JSON.stringify(lineText)}`);
        }
        if (n === occurrence) {
            return at;
        }
        from = at + word.length;
    }
}

function decode(result) {
    const data = (result && result.data) || [];
    const tokens = [];
    let line = 0;
    let start = 0;
    for (let i = 0; i + 4 < data.length; i += 5) {
        line += data[i];
        start = data[i] === 0 ? start + data[i + 1] : data[i + 1];
        tokens.push({
            line,
            start,
            length: data[i + 2],
            type: LEGEND[data[i + 3]],
            modifiers: data[i + 4],
        });
    }
    return tokens;
}

function typeAt(tokens, line, start) {
    const hit = tokens.find((token) => token.line === line && token.start === start);
    return hit ? hit.type : null;
}

function assertType(tokens, lines, lineText, word, expected, label, occurrence = 1) {
    const line = lineIndex(lines, lineText);
    const start = wordAt(lineText, word, occurrence);
    assertEqual(typeAt(tokens, line, start), expected, label);
}

async function runMain(session) {
    const main = fixture('custom/main.prg');
    session.didOpen({ uri: main.uri, languageId: 'vertexadt', version: 1, text: main.text });
    const tokens = decode(await session.semanticTokensFull(main.uri));
    const at = (lineText, word, expected, label, occurrence) =>
        assertType(tokens, main.lines, lineText, word, expected, label, occurrence);

    at('vec_to(VecA) 1, 2, 3', 'VecA', 'pvectorVariable', 'file-level VecA');
    at('vec_to(MyP) 1, 2, 3 ;! [pvector]', 'MyP', null, 'file-level MyP is PXYZN');
    at('DbOnly = 1 ;!"[database]"', 'DbOnly', 'databaseVariable', 'DbOnly comment type');
    at('Copied = Unknown', 'Copied', null, 'generic copy is not pushed');
    at('Copied = Unknown', 'Unknown', null, 'Unknown has no type');

    at('UseA = VecA', 'UseA', 'pvectorVariable', 'MACROS UseA');
    at('UseA = VecA', 'VecA', 'pvectorVariable', 'MACROS VecA use');
    at('Other = MyP', 'Other', 'pvectorVariable', 'MACROS Other copies MyP');
    at('UseP = MyP', 'UseP', null, 'MACROS UseP is PXYZN');
    at('UseP = MyP', 'MyP', null, 'MACROS MyP is PXYZN');

    at('UseA2 = VecA', 'UseA2', null, 'MACROS2 VecA does not bleed');
    at('UseA2 = VecA', 'VecA', null, 'MACROS2 VecA use does not bleed');
    at('UseP2 = MyP', 'MyP', null, 'MACROS2 MyP does not bleed');

    at('Ref = IncludedVar', 'IncludedVar', null, 'plane is unknown');
    at('DbRef = IncludedDb', 'IncludedDb', 'databaseVariable', 'JOINT IncludedDb');
    at('OutsideInc = IncludedVar', 'IncludedVar', null, 'include write stays in JOINT');

    at('! bang VecA', 'VecA', null, 'bang line');
    at('# include skipped.prg', 'skipped', null, 'hash include');
    at('call skipped.prg', 'skipped', null, 'space-form call');
    at('debug ./x.dbg', 'debug', null, 'debug line');
    at('msg$ = "VecA"', 'VecA', null, 'quoted');
    at('if VecA then', 'if', null, 'keyword');
    at('Shown = vec_to(VecA)', 'vec_to', null, 'command keyword skipped');
    at('Shown = vec_to(VecA)', 'VecA', 'pvectorVariable', 'Shown vec_to argument');
    at('Shown = vec_to(VecA)', 'Shown', null, 'Shown assignment is not a vec_to output');
    at('Foo$ = "x"', 'Foo', null, 'string variable');
    at('foo-bar = 1', 'foo', null, 'hyphen left');
    at('foo-bar = 1', 'bar', null, 'hyphen right');
    at('KeepVec = KeepVec ;!IGNORE TYPE', 'KeepVec', 'pvectorVariable', 'IGNORE TYPE still colors', 1);
    at('KeepVec = KeepVec ;!IGNORE TYPE', 'KeepVec', 'pvectorVariable', 'IGNORE TYPE RHS', 2);
    at('end ##', 'end', null, 'trailing hash-hash');

    for (const token of tokens) {
        assertEqual(token.modifiers, 0, `modifiers on ${token.type} at ${token.line}:${token.start}`);
        assert(token.type && token.type !== 'variable', `index 0 is not pushed (${token.type})`);
    }

    const plainUri = pathToFileURL(path.join(FIXTURE, 'custom', 'main.txt')).href;
    session.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: main.text });
    assertEqual((await session.semanticTokensFull(plainUri)).data, [], 'plaintext');
    session.didClose(plainUri);

    const missingUri = pathToFileURL(path.join(FIXTURE, 'custom', 'not_open.prg')).href;
    assertEqual((await session.semanticTokensFull(missingUri)).data, [], 'unopened document');

    session.didChangeConfiguration({ semanticHighlighting: false });
    assertEqual((await session.semanticTokensFull(main.uri)).data, [], 'highlighting off');
    session.didChangeConfiguration({ semanticHighlighting: true });

    session.didChangeConfiguration({ strictMode: { enabled: true } });
    const strictTokens = decode(await session.semanticTokensFull(main.uri));
    assertType(strictTokens, main.lines, 'UseP = MyP', 'MyP', 'pvectorVariable', 'strict mode colors MACROS MyP');
    assertType(strictTokens, main.lines, 'UseP2 = MyP', 'MyP', null, 'strict mode still does not bleed');
    session.didChangeConfiguration({ strictMode: { enabled: false } });

    session.didChangeConfiguration({ debugValidationCommands: true });
    const debugTokens = decode(await session.semanticTokensFull(main.uri));
    assertType(debugTokens, main.lines, 'Shown = vec_to(VecA)', 'vec_to', 'validatedCommandVariable', 'debug colors validated commands');
    session.didChangeConfiguration({ debugValidationCommands: false });

    session.didClose(main.uri);
}

async function runDump(session) {
    // The header within the first 4 KB makes the buffer a debug trace: lexical
    // trace tokens, no type timeline (docs/adr/0025-debug-trace-fast-path.md).
    const dump = fixture('custom/dump.prg');
    session.didOpen({ uri: dump.uri, languageId: 'vertexadt', version: 1, text: dump.text });
    const tokens = decode(await session.semanticTokensFull(dump.uri));
    const header = lineIndex(dump.lines, 'START OF DEBUG in FILE');
    assertEqual(typeAt(tokens, header, 0), 'debugMarker', 'dump header is a trace banner');
    assertType(tokens, dump.lines, 'vec_to(KeepVec) 1, 2, 3', 'vec_to', 'function', 'catalog command in a trace');
    assertType(tokens, dump.lines, 'vec_to(KeepVec) 1, 2, 3', 'KeepVec', null, 'no type timeline in a trace');
    assert(!tokens.some((token) => token.type === 'pvectorVariable' && token.length === 'KeepVec'.length), 'no timeline types');
    session.didClose(dump.uri);
}

function tokensOnLine(tokens, line, lineText) {
    return tokens
        .filter((token) => token.line === line)
        .map((token) => `${lineText.slice(token.start, token.start + token.length)}:${token.type}`);
}

async function runDebugTraceRange(session) {
    const text = fs.readFileSync(DEBUG_FIXTURE, 'utf8');
    const lines = text.split('\n').map((line) => line.replace(/\r$/, ''));
    const uri = pathToFileURL(DEBUG_FIXTURE).href;
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    const range = (start, end) => session.request('textDocument/semanticTokens/range', {
        textDocument: { uri },
        range: { start: { line: start, character: 0 }, end: { line: end, character: 0 } },
    });

    // The index lands in the background; range answers never wait for it.
    let tokens = [];
    for (let attempt = 0; attempt < 20; attempt++) {
        tokens = decode(await range(0, 10));
        if (tokens.some((token) => token.type === 'variable')) {
            break;
        }
        await new Promise((resolve) => setTimeout(resolve, 50));
    }
    assert(tokens.every((token) => token.line <= 10), 'range tokens stay in the range');
    assertEqual(typeAt(tokens, 0, lines[0].indexOf('START')), 'debugMarker', 'START banner');
    const timer = lines.findIndex((line) => /^\s*timer = 1\s+<1>/.test(line));
    assertEqual(
        tokensOnLine(tokens, timer, lines[timer]),
        ['timer:variable', '1:number', '<1>:debugValue'],
        'assignment line: name, number, value'
    );
    const endif = lines.findIndex((line) => /^endif\s+<> _if_end_/.test(line));
    assertEqual(
        tokensOnLine(tokens, endif, lines[endif]),
        ['endif:keyword', '<>:debugValue', '_if_end_:debugMarker'],
        'endif line: keyword, value, marker'
    );
    const p = lines.findIndex((line) => /^P = 1\s/.test(line));
    assertEqual(typeAt(tokens, p, 0), 'pvectorVariable', 'P suffix name');

    // Full equals the union of ranges for a small trace.
    const full = decode(await session.semanticTokensFull(uri));
    const tail = decode(await range(50, lines.length));
    const expected = full.filter((token) => token.line >= 50);
    assertEqual(tail.map((t) => [t.line, t.start, t.length, t.type]), expected.map((t) => [t.line, t.start, t.length, t.type]), 'range matches full');

    // Every feature but tokens, hover and definition answers empty.
    const pull = await session.diagnostic(uri);
    assertEqual(pull && pull.items, [], 'no diagnostics on a trace');
    session.didClose(uri);
}

async function runSourceRange(session) {
    const main = fixture('custom/main.prg');
    session.didOpen({ uri: main.uri, languageId: 'vertexadt', version: 1, text: main.text });
    const full = decode(await session.semanticTokensFull(main.uri));
    const start = 2;
    const end = Math.min(12, main.lines.length - 1);
    const ranged = decode(await session.request('textDocument/semanticTokens/range', {
        textDocument: { uri: main.uri },
        range: { start: { line: start, character: 0 }, end: { line: end, character: 0 } },
    }));
    const expected = full.filter((token) => token.line >= start && token.line <= end);
    assertEqual(ranged, expected, 'source range is the full result cut to the range');
    session.didClose(main.uri);
}

async function runIncludedFromBufferEdit(session) {
    // Shared URI+version prep: tokens must follow an ##included_from parent
    // buffer edit even though main.prg's own version does not change.
    const main = fixture('custom/main.prg');
    const callee = fixture('custom/lib/callee.prg');
    const dbRef = (tokens) => assertType.bind(null, tokens, main.lines, 'DbRef = IncludedDb', 'IncludedDb');
    session.didOpen({ uri: main.uri, languageId: 'vertexadt', version: 1, text: main.text });
    const [, first] = await Promise.all([session.diagnostic(main.uri), session.semanticTokensFull(main.uri)]);
    dbRef(decode(first))('databaseVariable', 'pull+tokens together: IncludedDb');
    dbRef(decode(await session.semanticTokensFull(main.uri)))('databaseVariable', 'repeat tokens: IncludedDb');

    session.didOpen({ uri: callee.uri, languageId: 'vertexadt', version: 1, text: callee.text });
    session.didChange({
        uri: callee.uri,
        version: 2,
        contentChanges: [{ text: callee.text.replace('[database]', '[pvector]') }],
    });
    dbRef(decode(await session.semanticTokensFull(main.uri)))('pvectorVariable', 'parent buffer edit recolors IncludedDb');

    session.didClose(callee.uri);
    dbRef(decode(await session.semanticTokensFull(main.uri)))('databaseVariable', 'parent buffer close restores disk type');
    session.didClose(main.uri);
}

async function runIncludedParentPriorType(session) {
    // The ##included_from parent extraction is cached per parent text, but a
    // parent `vec_copy` reads the caller's earlier writes: an edit to the
    // caller alone must change the copied type.
    const child = fixture('custom/vec_copy_child.prg');
    const copied = (tokens) => assertType.bind(null, tokens, child.lines, 'Use = Copied', 'Copied');
    session.didOpen({ uri: child.uri, languageId: 'vertexadt', version: 1, text: child.text });
    copied(decode(await session.semanticTokensFull(child.uri)))('pvectorVariable', 'parent vec_copy reads caller pvector Src');
    session.didChange({
        uri: child.uri,
        version: 2,
        contentChanges: [{ text: child.text.replace('vec_to(Src) 1, 2, 3', 'vec_to(Src) 1, 0, 0') }],
    });
    copied(decode(await session.semanticTokensFull(child.uri)))('xvectorVariable', 'caller edit recolors copied type');
    session.didChange({ uri: child.uri, version: 3, contentChanges: [{ text: child.text }] });
    copied(decode(await session.semanticTokensFull(child.uri)))('pvectorVariable', 'caller restore recolors copied type');
    session.didClose(child.uri);
}

async function run() {
    const rootUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri,
        extensionUri: pathToFileURL(REPO_ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: 'section-aware-types' }],
    });
    try {
        await session.initialize();
        const provider = session.initializeResult.capabilities.semanticTokensProvider;
        assert(provider, 'semanticTokensProvider');
        assertEqual(provider.legend && provider.legend.tokenTypes, LEGEND, 'legend tokenTypes ABI');
        assertEqual(provider.legend && provider.legend.tokenModifiers, ['readonly', 'declaration'], 'legend tokenModifiers');
        assertEqual(provider.full, true, 'full');
        assertEqual(provider.range, true, 'range');
        await runMain(session);
        await runDump(session);
        await runDebugTraceRange(session);
        await runSourceRange(session);
        await runIncludedFromBufferEdit(session);
        await runIncludedParentPriorType(session);
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    run().then(() => {
        console.log('30-semantic-tokens goldens passed');
    }).catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run };
