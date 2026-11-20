'use strict';

/**
 * Parity goldens for macro library/code completion.
 * Assertions from test-fixtures/macro-library-code-completion/TESTING.md.
 *
 * Oracle: the live Node language server (server/out/server.js --stdio)
 * answering custom/getMacroLibraries and custom/getMacroCodes, run through the
 * client host's libraryCodeCommands site + item mapping
 * (client/out/extension.js), folded into ordinary textDocument/completion.
 * Each server gets its own temp copy of the Custom libraries so each spawns
 * the Vertex Dbase Converter helper for its own stale database.
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const ranking = require('../completion-ranking');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'macro-library-code-completion');
const MACRO_SYS = path.join(ROOT, 'test-fixtures', 'vertex-dbase-converter', 'custom', 'complibs', 'macro_sys');
const NODE_SERVER = path.join(ROOT, 'server', 'out', 'server.js');
const FOLDER_KIND = 19;
const VALUE_KIND = 12;
const XML_TIMEOUT_MS = 120000;

// Copied verbatim from client/out/extension.js libraryCodeCommands.
const libraryCodeCommands = [
    { name: 'new_macro', pattern: /new_macro\(\s*(?:"([^"]*)"|'([^']*)')?(?:\s*,\s*(?:"([^"]*)"|'([^']*)')?)?\s*$/ },
    { name: 'new_djoint', pattern: /new_djoint\(\s*(?:"([^"]*)"|'([^']*)')?(?:\s*,\s*(?:"([^"]*)"|'([^']*)')?)?\s*$/ },
    { name: 'new_sheet', pattern: /new_sheet\(\s*(?:"([^"]*)"|'([^']*)')?(?:\s*,\s*(?:"([^"]*)"|'([^']*)')?)?\s*$/ },
];

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

// The Node language server, driven like the native one, answering the host's
// two custom requests.
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

    // Host provider (extension.js 1541–1592): site regex, then one RPC.
    async oracleItems(text, line, character) {
        const lineText = (text.split('\n')[line] || '').replace(/\r$/, '');
        const linePrefix = lineText.substring(0, character);
        for (const command of libraryCodeCommands) {
            const regexMatch = linePrefix.match(command.pattern);
            if (!regexMatch) {
                continue;
            }
            const isFirstParameter = regexMatch[1] !== undefined || regexMatch[2] !== undefined
                ? false
                : !linePrefix.includes(',');
            if (isFirstParameter) {
                const libraries = await this.request('custom/getMacroLibraries', {}, XML_TIMEOUT_MS);
                return { slot: 'library', items: libraries.map((lib) => ({ label: lib, kind: FOLDER_KIND, insertText: `"${lib}"` })) };
            }
            const library = regexMatch[1] || regexMatch[2] || '';
            const codes = await this.request('custom/getMacroCodes', { library }, XML_TIMEOUT_MS);
            return { slot: 'code', items: codes.map((code) => ({ label: code, kind: VALUE_KIND, insertText: `"${code}"` })) };
        }
        return { slot: null, items: [] };
    }
}

function familyItems(result, name) {
    assert(result && Array.isArray(result.items), `${name}: vertexadt completion is a CompletionList`);
    // isIncomplete: checked by completion-ranking familyItems (true with family items).
    assert(
        result.items.some((item) => item.label === 'askdim'),
        `${name}: keeps the cheap keyword-variable list`
    );
    return ranking.familyItems(result, 'macroLibrary', name)
        .filter((item) => (item.kind === FOLDER_KIND || item.kind === VALUE_KIND) && item.data === undefined)
        .map((item) => {
            assertEqual(item.sortText, item.label, `${name}: ${item.label} family key is the label`);
            // filterText: the typed text when one is typed (slot substring match).
            for (const field of ['textEdit', 'detail', 'documentation', 'insertTextFormat', 'commitCharacters', 'preselect']) {
                assert(item[field] === undefined, `${name}: ${item.label} has no ${field}`);
            }
            return { label: item.label, kind: item.kind, insertText: item.insertText };
        });
}

// Cursor `|`; without one, the end of the text.
function splitCursor(textWithCursor) {
    const at = textWithCursor.indexOf('|');
    const text = at < 0 ? textWithCursor : textWithCursor.slice(0, at) + textWithCursor.slice(at + 1);
    const before = at < 0 ? text : text.slice(0, at);
    const lines = before.split('\n');
    return { text, line: lines.length - 1, character: lines[lines.length - 1].length };
}

class Probe {
    constructor(session, uri) {
        this.session = session;
        this.uri = uri;
        this.version = 0;
    }

    sync(text) {
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({ uri: this.uri, languageId: 'vertexadt', version: 1, text });
        } else {
            this.session.didChange({ uri: this.uri, version: this.version, contentChanges: [{ text }] });
        }
    }

    async complete(text, line, character, timeoutMs = XML_TIMEOUT_MS) {
        this.sync(text);
        return this.session.request('textDocument/completion', {
            textDocument: { uri: this.uri },
            position: { line, character },
            context: { triggerKind: 1 },
        }, timeoutMs);
    }
}

function copyDir(from, to) {
    fs.mkdirSync(to, { recursive: true });
    for (const entry of fs.readdirSync(from, { withFileTypes: true })) {
        const source = path.join(from, entry.name);
        const target = path.join(to, entry.name);
        if (entry.isDirectory()) {
            copyDir(source, target);
        } else {
            fs.copyFileSync(source, target);
        }
    }
}

// Workspace folders `system` then `customa` (custom name from
// `includes("custom")`). macro_sys holds only stale databases (no XML yet);
// codes_lib's database is older than its XML; broken_lib's database is not a
// real Vertex database, so its export fails.
function makeWorkspace(root) {
    copyDir(FIXTURE, root);
    const custom = path.join(root, 'customa');
    const system = path.join(root, 'system');
    const complibs = path.join(custom, 'complibs');
    const macroSys = path.join(complibs, 'macro_sys');
    fs.mkdirSync(macroSys, { recursive: true });
    for (const name of ['d_MACRO_SYS', 'd_MACRO_SYSc', 'd_MACRO_SYSl']) {
        fs.copyFileSync(path.join(MACRO_SYS, name), path.join(macroSys, name));
    }
    const past = new Date(Date.now() - 3600 * 1000);
    fs.utimesSync(path.join(complibs, 'codes_lib', 'd_CODES_LIB'), past, past);
    fs.utimesSync(path.join(complibs, 'codes_lib', 'd_CODES_LIBc'), past, past);
    return { root, custom, system, complibs, probeUri: pathToFileURL(path.join(custom, 'probe.prg')).href };
}

function sessionOptions(workspace, extensionRoot) {
    return {
        rootUri: pathToFileURL(workspace.root).href,
        extensionUri: pathToFileURL(extensionRoot).href,
        workspaceFolders: [
            { uri: pathToFileURL(workspace.system).href, name: 'system' },
            { uri: pathToFileURL(workspace.custom).href, name: 'customa' },
        ],
    };
}

// [name, textWithCursor, expectation]: 'library' / 'code' (non-empty items in
// that slot), 'none' (no family items), or omitted (whatever Node answers).
const SITE_CASES = [
    ['library: bare', 'new_macro(|', 'library'],
    ['library: space after paren', 'new_macro( |', 'library'],
    ['library: assignment', 'M = new_macro(|', 'library'],
    ['library: indent', '  new_macro(|', 'library'],
    ['library: tab', '\tnew_macro(|', 'library'],
    ['library: mid-line after ;', 'x = 1; new_macro(|', 'library'],
    ['library: bang line', '! new_macro(|', 'library'],
    ['library: substring mynew_macro', 'mynew_macro(|', 'library'],
    ['library: new_djoint', 'J = new_djoint(|', 'library'],
    ['library: new_sheet', 'S = new_sheet(|', 'library'],
    ['library: text after cursor', 'M = new_macro(|"codes_lib")', 'library'],
    ['code: closed library, no comma', 'new_macro("codes_lib"|', 'code'],
    ['code: closed library, space', 'new_macro("codes_lib" |', 'code'],
    ['code: after comma', 'new_macro("codes_lib",|', 'code'],
    ['code: after comma and space', 'M = new_macro("codes_lib", |', 'code'],
    ['code: after closed code', 'new_macro("codes_lib","BATHZONE"|', 'code'],
    ['code: spaced comma', 'new_macro( "codes_lib" , "X"|', 'code'],
    ['code: single quotes', "new_macro('codes_lib',|", 'code'],
    ['code: library label case (Windows join)', 'new_macro("CODES_LIB",|', 'code'],
    ['code: new_djoint', 'J = new_djoint("codes_lib", |', 'code'],
    ['code: new_sheet d_FRM_SHEET fallback', 'S = new_sheet("MIXED_CASE", |', 'code'],
    ['code: other d_*.xml gives no codes', 'new_macro("OTHER_XML", |', 'none'],
    ['code: no Record has CODE', 'new_macro("EMPTY_CODES", |', 'none'],
    ['code: missing library folder', 'new_macro("no_such_lib", |', 'none'],
    ['code: library with comma', 'new_macro("A,B",|', 'none'],
    ['code: empty library', 'new_macro("",|', 'none'],
    ['code: leading comma, empty library', 'new_macro(,"CODE"|', 'none'],
    ['code: comma before the command, empty library', 'foo, new_macro(|', 'none'],
    ['code: System library is not Custom', 'new_macro("sys_lib", |', 'none'],
    ['none: unclosed quote', 'new_macro("|', 'none'],
    ['none: unclosed library', 'new_macro("codes_lib|', 'none'],
    ['none: unclosed code', 'new_macro("codes_lib","|', 'none'],
    ['none: third argument', 'new_macro("codes_lib","BATHZONE",|', 'none'],
    ['none: after closing paren', 'new_macro("codes_lib")|', 'none'],
    ['none: empty call closed', 'new_macro()|', 'none'],
    ['none: uppercase', 'NEW_MACRO(|', 'none'],
    ['none: capitalized', 'New_macro(|', 'none'],
    ['none: space before paren', 'new_macro (|', 'none'],
    ['none: new_macros', 'new_macros(|', 'none'],
    ['none: unquoted library variable', 'new_macro(Lib$,|', 'none'],
    ['none: new_piece', 'new_piece("codes_lib",|', 'none'],
    ['none: set_macro_form', 'set_macro_form("codes_lib"|', 'none'],
    ['none: change_sheet', 'change_sheet(S,"codes_lib",|', 'none'],
    ['code: later occurrence matches', 'new_macro("x") new_macro("codes_lib",|', 'code'],
    ['library: second line of a document', 'x = 1\nM = new_macro(|', 'library'],
    ['library: CRLF document', 'x = 1\r\nM = new_macro(|', 'library'],
];

const CODE_LABELS = ['BATHZONE', '  PADDED  ', '', 'SHEET1', 'BOTH_CODE', 'Tom & Jerry', 'FOO"BAR', 'Stairs, Straight', 'Åsa'];

function checkExpectation(name, items, expectation) {
    if (expectation === 'library') {
        assert(items.length > 0 && items.every((item) => item.kind === FOLDER_KIND), `${name}: adds library items`);
    } else if (expectation === 'code') {
        assert(items.length > 0 && items.every((item) => item.kind === VALUE_KIND), `${name}: adds code items`);
    } else if (expectation === 'none') {
        assertEqual(items, [], `${name}: adds no macro library/code items`);
    }
}

async function runCases(nativeProbe, node, nodeProbe, cases) {
    for (const [name, textWithCursor, expectation] of cases) {
        const { text, line, character } = splitCursor(textWithCursor);
        const actual = familyItems(await nativeProbe.complete(text, line, character), name);
        nodeProbe.sync(text);
        const expected = await node.oracleItems(text, line, character);
        assertEqual(actual, expected.items, `${name}: matches the Node host oracle`);
        if (process.env.GOLDEN_VERBOSE) {
            console.log(`${name} [${expected.slot || '-'}]: ${actual.map((item) => item.insertText).join(' ; ')}`);
        }
        checkExpectation(name, actual, expectation);
    }
}

async function runParity(temp) {
    const nativeWorkspace = makeWorkspace(path.join(temp, 'native'));
    const nodeWorkspace = makeWorkspace(path.join(temp, 'node'));
    const nativeMacroXml = path.join(nativeWorkspace.complibs, 'macro_sys', 'd_MACRO_SYS.xml');

    const native = LspSession.launch(sessionOptions(nativeWorkspace, ROOT));
    const node = NodeOracleSession.launchNode(sessionOptions(nodeWorkspace, ROOT));
    try {
        await native.initialize();
        await node.initialize();
        await native.waitQuiet();
        const nativeProbe = new Probe(native, nativeWorkspace.probeUri);
        const nodeProbe = new Probe(node, nodeWorkspace.probeUri);

        // Code slots first: they never touch macro_sys.
        await runCases(nativeProbe, node, nodeProbe, SITE_CASES.filter(([, , expectation]) => expectation !== 'library'));
        assert(!fs.existsSync(nativeMacroXml), 'code slots for other libraries do not export macro_sys');

        await runCases(nativeProbe, node, nodeProbe, SITE_CASES.filter(([, , expectation]) => expectation === 'library'));

        // Library listing exports every stale primary in every Custom library.
        assert(fs.existsSync(nativeMacroXml), 'library listing exported stale macro_sys');
        const xml = fs.readFileSync(nativeMacroXml, 'utf8');
        assert(xml.startsWith('<?xml') && xml.includes('<Table Name="MACRO_SYS"'), 'exported XML is the helper output');
        for (const companion of ['macro_sys/d_MACRO_SYSc.xml', 'macro_sys/d_MACRO_SYSl.xml', 'codes_lib/d_CODES_LIBc.xml', 'broken_lib/d_BROKEN.xml']) {
            assert(!fs.existsSync(path.join(nativeWorkspace.complibs, companion)), `${companion} is not written`);
        }

        // The library list, pinned (directory order, uppercased).
        const { text, line, character } = splitCursor('new_macro(|');
        const libraries = familyItems(await nativeProbe.complete(text, line, character), 'library list');
        assertEqual(
            libraries.map((item) => item.label),
            ['CODES_LIB', 'EMPTY_CODES', 'MACRO_SYS', 'MIXED_CASE', 'OTHER_XML', 'UPPER_EXT'],
            'library list'
        );
        assertEqual(libraries[0].insertText, '"CODES_LIB"', 'library insertText is double-quoted');

        // The code list, pinned.
        const codesAt = splitCursor('new_macro("codes_lib", |');
        const codes = familyItems(await nativeProbe.complete(codesAt.text, codesAt.line, codesAt.character), 'code list');
        assertEqual(codes.map((item) => item.label), CODE_LABELS, 'code list');
        assertEqual(codes.map((item) => item.insertText).slice(5, 7), ['"Tom & Jerry"', '"FOO"BAR"'], 'code insertText is unescaped');

        // Native-only (Node is frozen): an unquoted word typed in a slot is the
        // slot at the word's start, substring-filtered on it.
        const unquotedLibrary = splitCursor('new_macro(LIB|');
        const libraryResult = await nativeProbe.complete(unquotedLibrary.text, unquotedLibrary.line, unquotedLibrary.character);
        assertEqual(familyItems(libraryResult, 'unquoted library').map((item) => item.label), ['CODES_LIB'], 'unquoted library filters libraries');
        assert(
            libraryResult.items.filter((item) => ranking.isFamilyItem(item, 'macroLibrary')).every((item) => item.filterText === 'LIB'),
            'unquoted library: filterText is the typed word'
        );
        const unquotedCode = splitCursor('new_macro("codes_lib",CODE|');
        assertEqual(
            familyItems(await nativeProbe.complete(unquotedCode.text, unquotedCode.line, unquotedCode.character), 'unquoted code').map((item) => item.label),
            CODE_LABELS.filter((label) => label.toLowerCase().includes('code')),
            'unquoted code filters codes'
        );

        // Codes from the freshly exported macro_sys XML.
        await runCases(nativeProbe, node, nodeProbe, [
            ['code: exported macro_sys', 'M = new_macro("macro_sys", |', 'code'],
        ]);

        // Plaintext documents add nothing.
        const plainUri = pathToFileURL(path.join(nativeWorkspace.custom, 'plain.txt')).href;
        native.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: 'new_macro(' });
        const plain = await native.request('textDocument/completion', {
            textDocument: { uri: plainUri },
            position: { line: 0, character: 10 },
        });
        const plainItems = plain && Array.isArray(plain.items) ? plain.items : Array.isArray(plain) ? plain : [];
        assert(!plainItems.some((item) => item.kind === FOLDER_KIND), 'plaintext adds no library items');

        // completionItem/resolve leaves these items alone.
        const resolved = await native.completionResolve(codes[0]);
        assertEqual(resolved, codes[0], 'resolve returns code items unchanged');

        await native.shutdownAndExit();
        await node.shutdownAndExit().catch(() => node.kill());
    } catch (err) {
        native.kill();
        node.kill();
        throw err;
    }
}

// The helper is missing at the extension root: stale databases are logged and
// skipped, so macro_sys (no XML yet) is not listed; existing XML still is.
async function runMissingHelper(temp) {
    const workspace = makeWorkspace(path.join(temp, 'nohelper'));
    const extensionRoot = path.join(temp, 'no-helper-extension');
    fs.mkdirSync(extensionRoot, { recursive: true });
    const session = LspSession.launch(sessionOptions(workspace, extensionRoot));
    try {
        await session.initialize();
        await session.waitQuiet();
        const probe = new Probe(session, workspace.probeUri);
        const { text, line, character } = splitCursor('new_macro(|');
        const items = familyItems(await probe.complete(text, line, character), 'missing helper');
        assertEqual(
            items.map((item) => item.label),
            ['CODES_LIB', 'EMPTY_CODES', 'MIXED_CASE', 'OTHER_XML', 'UPPER_EXT'],
            'missing helper: only libraries with existing XML'
        );
        const codesAt = splitCursor('new_macro("macro_sys", |');
        const codes = familyItems(await probe.complete(codesAt.text, codesAt.line, codesAt.character), 'missing helper codes');
        assertEqual(codes, [], 'missing helper: no XML, no codes, no error');
        assert(!fs.existsSync(path.join(workspace.complibs, 'macro_sys', 'd_MACRO_SYS.xml')), 'missing helper: nothing exported');
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
    assert(/Vertex database converter not found at/.test(session._stderr), 'missing helper is logged');
}

async function main() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'vertex-adt-macro-library-goldens-'));
    try {
        await runParity(temp);
        await runMissingHelper(temp);
    } finally {
        fs.rmSync(temp, { recursive: true, force: true });
    }
}

if (require.main === module) {
    main()
        .then(() => {
            console.log('lsp golden: 42-macro-library-code-completion passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
