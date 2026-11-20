'use strict';

/**
 * Parity goldens for object-parameter completion.
 * Assertions from test-fixtures/object-parameter-completion/TESTING.md.
 *
 * Oracle: the live Node language server (server/out/server.js --stdio)
 * answering custom/getVariableObjectType and custom/getObjectParameters, run
 * through the client host's extractGetCommandInfo + item mapping
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
const FIXTURE = path.join(ROOT, 'test-fixtures', 'object-parameter-completion');
const MACRO_SYS = path.join(ROOT, 'test-fixtures', 'vertex-dbase-converter', 'custom', 'complibs', 'macro_sys');
const NODE_SERVER = path.join(ROOT, 'server', 'out', 'server.js');
const PROPERTY_KIND = 10;
const XML_TIMEOUT_MS = 60000;

// Copied verbatim from client/out/extension.js extractGetCommandInfo.
function extractGetCommandInfo(lineText) {
    const patterns = [
        { command: 'get_obj_param', regex: /get_obj_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_material_param', regex: /get_material_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_area_param', regex: /get_area_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_pnt_param', regex: /get_pnt_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_sheet_param', regex: /get_sheet_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_joint_param', regex: /get_joint_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'get_section_param', regex: /get_section_param\s*\(\s*([^,\s]+)\s*,\s*$/ },
        { command: 'set_obj_param', regex: /set_obj_param\s*\(\s*([^,\s)]+)\s*\)\s*$/ },
        { command: 'set_joint_param', regex: /set_joint_param\s*\(\s*([^,\s)]+)\s*\)\s*$/ },
        { command: 'set_djparam', regex: /set_djparam\s*\(\s*([^,\s)]+)\s*\)\s*$/ },
    ];
    for (const pattern of patterns) {
        const match = lineText.match(pattern.regex);
        if (match) {
            return { command: pattern.command, variableName: match[1].trim() };
        }
    }
    return null;
}

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

// The host provider's CompletionItem, as it reaches the wire.
function hostItem(command, param) {
    return {
        label: param.label,
        kind: PROPERTY_KIND,
        insertText: param.insertText || `"${param.label}"`,
        documentation: { kind: 'markdown', value: param.documentation === undefined ? '' : param.documentation },
        detail: `${command} - ${param.detail}`,
        sortText: `0${param.label}`,
    };
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

    // Host provider: extractGetCommandInfo, then the two RPCs, then
    // `parameters.length > 0 ? parameters : genericParameters`.
    async oracleItems(uri, text, line, character) {
        const lineText = (text.split('\n')[line] || '').replace(/\r$/, '');
        const info = extractGetCommandInfo(lineText.substring(0, character));
        if (!info) {
            return { items: [] };
        }
        const { command, variableName } = info;
        const [objectType, genericParameters] = await Promise.all([
            this.request('custom/getVariableObjectType', { uri, variableName, line }),
            this.request('custom/getObjectParameters', { command, objectType: 'generic' }),
        ]);
        const typed = await this.requestAllowError('custom/getObjectParameters', {
            command, objectType, uri, variableName, line,
        }, XML_TIMEOUT_MS);
        const generic = genericParameters.map((param) => hostItem(command, param));
        if (typed.error) {
            return { items: generic, nodeError: typed.error.message, objectType };
        }
        const finalParameters = typed.result.length > 0 ? typed.result : genericParameters;
        return { items: finalParameters.map((param) => hostItem(command, param)), objectType };
    }
}

function familyItems(result, name) {
    assert(result && Array.isArray(result.items), `${name}: vertexadt completion is a CompletionList`);
    // isIncomplete: checked by completion-ranking familyItems (true with family items).
    assert(
        result.items.some((item) => item.label === 'askdim'),
        `${name}: keeps the cheap keyword-variable list`
    );
    return ranking.familyItems(result, 'objectParameter', name)
        .filter((item) => item.kind === PROPERTY_KIND && typeof item.sortText === 'string' && item.sortText.startsWith('0'))
        .map((item) => {
            // filterText: the typed text when one is typed (slot substring match).
            for (const field of ['textEdit', 'data', 'insertTextFormat', 'commitCharacters', 'preselect']) {
                assert(item[field] === undefined, `${name}: ${item.label} has no ${field}`);
            }
            return {
                label: item.label,
                kind: item.kind,
                insertText: item.insertText,
                documentation: item.documentation,
                detail: item.detail,
                sortText: item.sortText,
            };
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

    async complete(text, line, character, timeoutMs) {
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

// One Custom workspace: macro_sys with its database made newer than the XML
// (stale), fresh_lib with its database older than the XML, frm_lib with only
// d_FRM_SHEET.xml.
function makeWorkspace(root) {
    const custom = path.join(root, 'custom');
    const complibs = path.join(custom, 'complibs');
    copyDir(path.join(FIXTURE, 'custom', 'complibs'), complibs);
    copyDir(MACRO_SYS, path.join(complibs, 'macro_sys'));
    const past = new Date(Date.now() - 3600 * 1000);
    const future = new Date(Date.now() + 3600 * 1000);
    fs.utimesSync(path.join(complibs, 'macro_sys', 'd_MACRO_SYS.xml'), past, past);
    fs.utimesSync(path.join(complibs, 'macro_sys', 'd_MACRO_SYS'), future, future);
    fs.utimesSync(path.join(complibs, 'fresh_lib', 'd_FRESH_LIB'), past, past);
    return { root, custom, complibs, probeUri: pathToFileURL(path.join(custom, 'probe.prg')).href };
}

function sessionOptions(workspace, extensionRoot) {
    const folderUri = pathToFileURL(workspace.custom).href;
    return {
        rootUri: pathToFileURL(workspace.root).href,
        extensionUri: pathToFileURL(extensionRoot).href,
        workspaceFolders: [{ uri: folderUri, name: 'custom' }],
    };
}

const P = 'P = new_piece(1)\n';
const S = 'S = new_sheet(1)\n';
const E = 'E = get_panel(1)\n';
const A = 'A = get_panel_core(1)\n';

// [name, textWithCursor, expectation]. expectation: 'items' (non-empty),
// 'none' (empty), or omitted (whatever Node answers).
const STATIC_CASES = [
    ['profile get_obj_param', `${P}x$ = get_obj_param(P, |`, 'items'],
    ['sheet get_obj_param no space', `${S}get_obj_param(S,|`, 'items'],
    ['selemobj get_obj_param two spaces', `${E}  get_obj_param(E,  |`, 'items'],
    ['area present empty list wins', `${A}get_obj_param(A, |`, 'none'],
    ['unknown name is generic', 'get_obj_param(Unknown, |', 'none'],
    ['get_material_param any list', `${P}get_material_param(P, |`, 'items'],
    ['get_material_param generic is any', 'get_material_param(Nope, |', 'items'],
    ['get_area_param snippets', `${A}get_area_param(A, |`, 'items'],
    ['get_section_param any', 'get_section_param(Sec, |', 'items'],
    ['get_pnt_param no rows', `${P}get_pnt_param(P, |`, 'none'],
    ['get_sheet_param site, no rows', `${S}get_sheet_param(S, |`, 'none'],
    ['get_joint_param without identity', 'get_joint_param(J, |', 'none'],
    ['set_obj_param after paren', `${P}set_obj_param(P) |`, 'none'],
    ['set_obj_param inner spaces', `${P}set_obj_param( P ) |`, 'none'],
    ['uppercase is not a site', `${P}GET_OBJ_PARAM(P, |`, 'none'],
    ['third-arg comma is not a site', `${P}get_obj_param(P, "USE", |`, 'none'],
    ['get_macro_param is not a site', `${P}get_macro_param(P, |`, 'none'],
    ['documented after-paren get_pnt_param is not a site', `${P}get_pnt_param(P) |`, 'none'],
    ['set_pnt_param is not a site', `${P}set_pnt_param(P) |`, 'none'],
    ['unclosed set_obj_param is not a site', `${P}set_obj_param(P |`, 'none'],
    ['substring xget_obj_param is a site', `${P}out$ = xget_obj_param(P, |`, 'items'],
    ['bang line is a site', `${P}! get_obj_param(P,|`, 'items'],
    ['mid-line is a site', `${P}x = 1; get_obj_param(P, |`, 'items'],
    ['tab indent is a site', `${P}\tget_obj_param(P, |`, 'items'],
    ['math expression base name', `${P}get_obj_param(P+2, |`, 'items'],
    ['index expression base name', `${P}get_obj_param(P[1], |`, 'items'],
    ['quoted name is generic', `${P}get_obj_param("P", |`, 'none'],
    ['name lookup is case-sensitive', `${P}get_obj_param(p, |`, 'none'],
    ['write below the cursor does not count', `get_obj_param(P, |\n${P}`, 'none'],
    ['same-line text after cursor', `${P}get_obj_param(P, |) ! tail`, 'items'],
    ['latest write wins', `${P}${'P = new_sheet(1)\n'}get_obj_param(P, |`, 'items'],
    ['section: write in another section', `#SEC_A#\n${P}#SEC_A#\n#SEC_B#\nget_obj_param(P, |\n#SEC_B#`, 'none'],
    ['section: write in the same section', `#SEC_A#\n${P}get_obj_param(P, |\n#SEC_A#`, 'items'],
    ['section: file-level write, section cursor', `${P}#SEC_A#\nget_obj_param(P, |\n#SEC_A#`, 'none'],
    ['comment [type] fallback (PXYZN skipped by the timeline)', 'LP1 = 5 ;!"[sheet] corner"\nget_obj_param(LP1, |'],
    ['comment [type] fallback, uppercase bracket', 'MyN = 1 ;!"[ PROFILE ] bar"\nget_obj_param(MyN, |'],
    ['comment [type] on a timeline name', 'Foo = 1 ;!"[sheet] bar"\nget_obj_param(Foo, |'],
    ['CRLF document', `${P.replace('\n', '\r\n')}get_obj_param(P, |`, 'items'],
    ['cursor mid-line before site end', `${P}get_obj_param(P, |"USE")`, 'items'],
];

const XML_CASES = [
    ['macro: stale macro_sys exported then read', 'M = new_macro("macro_sys", "Stairs, Straight")\nget_obj_param(M, |', 'items'],
    ['macro: set_obj_param after paren', 'M = new_macro("macro_sys", "Stairs")\nset_obj_param(M) |', 'items'],
    ['macro: code matched case-insensitively', "M = new_macro('macro_sys', 'railings')\nget_obj_param(M, |", 'items'],
    ['macro: library and code from variables', 'Lib$ = "macro_sys"\nCode = "WEBSTIFF"\nM = new_macro(Lib$, Code)\nget_obj_param(M, |', 'none'],
    ['macro: code from a plain variable', 'Code = "WEBSTIFF"\nM = new_macro("macro_sys", Code)\nget_obj_param(M, |', 'items'],
    ['macro: fresh library, PARAMETERS split, entities, exclude list', 'M = new_macro("fresh_lib", "Plain")\nget_obj_param(M, |', 'items'],
    ['macro: SHTCODE matches for macros', 'M = new_macro("fresh_lib", "SheetOnly")\nget_obj_param(M, |', 'items'],
    ['macro: d_FRM_SHEET.xml fallback', 'M = new_macro("frm_lib", "FS1")\nget_obj_param(M, |', 'items'],
    ['macro: no matching record falls back to generic', 'M = new_macro("fresh_lib", "NoSuchCode")\nget_obj_param(M, |', 'none'],
    ['macro: unresolved code uses the static table', 'M = new_macro("fresh_lib", ascii(65))\nget_obj_param(M, |', 'none'],
    ['macro: get_material_param ignores XML', 'M = new_macro("fresh_lib", "Plain")\nget_material_param(M, |', 'items'],
    ['macro: name ending $ never resolves identity', 'M$ = new_macro("fresh_lib", "Plain")\nget_obj_param(M$, |'],
    ['macro: identity below the cursor is not used', 'get_obj_param(M, |\nM = new_macro("fresh_lib", "Plain")', 'none'],
    ['joint: CODE only', 'J = new_djoint("fresh_lib", "Plain")\nget_joint_param(J, |', 'items'],
    ['joint: SHTCODE does not match', 'J = new_djoint("fresh_lib", "SheetOnly")\nget_joint_param(J, |', 'none'],
    ['joint: set_djparam', 'J = new_djoint("fresh_lib", "Plain")\nset_djparam(J) |', 'items'],
    ['joint: set_joint_param', 'J = new_djoint("frm_lib", "FS1")\nset_joint_param(J) |', 'items'],
    ['joint: get_obj_param', 'J = new_djoint("macro_sys", "JOISTCONN")\nget_obj_param(J, |', 'items'],
];

// Node throws (uncaught readFileSync); the spec answers the generic list.
const NODE_THROWS_CASES = [
    ['macro: missing library folder', 'M = new_macro("no_such_lib", "X")\nget_obj_param(M, |', 'none'],
];

function checkExpectation(name, items, expectation) {
    if (expectation === 'items') {
        assert(items.length > 0, `${name}: adds object parameters`);
    } else if (expectation === 'none') {
        assertEqual(items, [], `${name}: adds no object parameters`);
    }
}

async function runCases(native, nativeProbe, node, nodeProbe, cases, { nodeThrows = false } = {}) {
    for (const [name, textWithCursor, expectation] of cases) {
        const { text, line, character } = splitCursor(textWithCursor);
        const actual = familyItems(await nativeProbe.complete(text, line, character, XML_TIMEOUT_MS), name);
        nodeProbe.sync(text);
        const expected = await node.oracleItems(nodeProbe.uri, text, line, character);
        if (nodeThrows) {
            assert(expected.nodeError, `${name}: Node's custom/getObjectParameters throws here`);
        } else {
            assert(!expected.nodeError, `${name}: Node error ${expected.nodeError}`);
        }
        assertEqual(actual, expected.items, `${name}: matches the Node host oracle`);
        if (process.env.GOLDEN_VERBOSE) {
            console.log(`${name} [${expected.objectType || '-'}]: ${actual.map((item) => `${item.label}|${item.insertText}|${item.detail}|${item.documentation.value}`).join(' ; ')}`);
        }
        checkExpectation(name, actual, expectation);
    }
}

async function runParity(temp) {
    const nativeWorkspace = makeWorkspace(path.join(temp, 'native'));
    const nodeWorkspace = makeWorkspace(path.join(temp, 'node'));
    const nativeXml = path.join(nativeWorkspace.complibs, 'macro_sys', 'd_MACRO_SYS.xml');
    const staleTime = fs.statSync(nativeXml).mtimeMs;

    const native = LspSession.launch(sessionOptions(nativeWorkspace, ROOT));
    const node = NodeOracleSession.launchNode(sessionOptions(nodeWorkspace, ROOT));
    try {
        await native.initialize();
        await node.initialize();
        await native.waitQuiet();
        const nativeProbe = new Probe(native, nativeWorkspace.probeUri);
        const nodeProbe = new Probe(node, nodeWorkspace.probeUri);

        await runCases(native, nativeProbe, node, nodeProbe, STATIC_CASES);
        assertEqual(fs.statSync(nativeXml).mtimeMs, staleTime, 'static cases never export');

        await runCases(native, nativeProbe, node, nodeProbe, XML_CASES);
        await runCases(native, nativeProbe, node, nodeProbe, NODE_THROWS_CASES, { nodeThrows: true });

        assert(fs.statSync(nativeXml).mtimeMs > staleTime, 'stale macro_sys XML was re-exported by the helper');
        const xml = fs.readFileSync(nativeXml, 'utf8');
        assert(xml.startsWith('<?xml') && xml.includes('<Table Name="MACRO_SYS"'), 'exported XML is the helper output');
        assert(!fs.existsSync(path.join(nativeWorkspace.complibs, 'macro_sys', 'd_MACRO_SYSc.xml')), 'c companion is not exported');
        assert(!fs.existsSync(path.join(nativeWorkspace.complibs, 'macro_sys', 'd_MACRO_SYSl.xml')), 'l companion is not exported');
        assert(!fs.existsSync(path.join(nativeWorkspace.complibs, 'fresh_lib', 'd_FRESH_LIBc.xml')), 'fresh companion is not exported');
        const freshXml = fs.readFileSync(path.join(nativeWorkspace.complibs, 'fresh_lib', 'd_FRESH_LIB.xml'), 'utf8');
        assert(freshXml.includes('Tom &amp; Jerry'), 'fresh XML (newer than its database) is not re-exported');

        // Native-only (Node is frozen): typing the field is the slot at the typed
        // word's start, substring-filtered on the typed text.
        {
            const empty = splitCursor(`${P}get_obj_param(P, |`);
            const all = familyItems(await nativeProbe.complete(empty.text, empty.line, empty.character, XML_TIMEOUT_MS), 'field slot');
            const at = splitCursor(`${P}get_obj_param(P, F|`);
            const typed = await nativeProbe.complete(at.text, at.line, at.character, XML_TIMEOUT_MS);
            const items = familyItems(typed, 'typing the field');
            assertEqual(
                items.map((item) => item.label),
                all.map((item) => item.label).filter((label) => label.toLowerCase().includes('f')),
                'typing the field filters by substring'
            );
            assert(items.length > 0, 'typing the field keeps matching parameters');
            assert(
                typed.items.filter((item) => item.kind === PROPERTY_KIND && ranking.isFamilyItem(item, 'objectParameter')).every((item) => item.filterText === 'F'),
                'typing the field: filterText is the typed text'
            );
        }

        // Plaintext documents add nothing.
        native.didOpen({ uri: pathToFileURL(path.join(nativeWorkspace.custom, 'plain.txt')).href, languageId: 'plaintext', version: 1, text: `${P}get_obj_param(P, ` });
        const plain = await native.request('textDocument/completion', {
            textDocument: { uri: pathToFileURL(path.join(nativeWorkspace.custom, 'plain.txt')).href },
            position: { line: 1, character: 15 },
        });
        const plainItems = plain && Array.isArray(plain.items) ? plain.items : Array.isArray(plain) ? plain : [];
        assert(!plainItems.some((item) => item.kind === PROPERTY_KIND), 'plaintext adds no object parameters');

        // completionItem/resolve leaves these items alone.
        const { text, line, character } = splitCursor(`${P}get_obj_param(P, |`);
        const listed = await nativeProbe.complete(text, line, character);
        const fptype = listed.items.find((item) => item.label === 'FPTYPE');
        const resolved = await native.completionResolve(fptype);
        assertEqual(resolved, fptype, 'resolve returns object-parameter items unchanged');

        await native.shutdownAndExit();
        await node.shutdownAndExit().catch(() => node.kill());
    } catch (err) {
        native.kill();
        node.kill();
        throw err;
    }
}

// The helper is missing at the extension root: the stale database is logged
// and skipped, and the existing XML is read.
async function runMissingHelper(temp) {
    const workspace = makeWorkspace(path.join(temp, 'nohelper'));
    const extensionRoot = path.join(temp, 'no-helper-extension');
    fs.mkdirSync(extensionRoot, { recursive: true });
    const xmlPath = path.join(workspace.complibs, 'macro_sys', 'd_MACRO_SYS.xml');
    const before = fs.statSync(xmlPath).mtimeMs;
    const expected = parseFixtureFields(fs.readFileSync(xmlPath, 'utf8'), 'Stairs, Straight');

    const session = LspSession.launch(sessionOptions(workspace, extensionRoot));
    try {
        await session.initialize();
        await session.waitQuiet();
        const probe = new Probe(session, workspace.probeUri);
        const { text, line, character } = splitCursor('M = new_macro("macro_sys", "Stairs, Straight")\nget_obj_param(M, |');
        const items = familyItems(await probe.complete(text, line, character, XML_TIMEOUT_MS), 'missing helper');
        assertEqual(items.map((item) => item.label), expected, 'missing helper: existing XML is read');
        assertEqual(fs.statSync(xmlPath).mtimeMs, before, 'missing helper: sidecar untouched');
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
    assert(/Vertex database converter not found at/.test(session._stderr), 'missing helper is logged');
}

// Labels for one CODE record, flattened like getObjectParameters.
function parseFixtureFields(xml, code) {
    const exclude = new Set(['BROWSER_PATH', 'CATEGORY', 'NAME_2D', 'NAME_3DACC', 'NAME_3DCRS', 'THUMBNAIL_ID', 'FORM_FILE', 'HNDPROG', 'INPUT_STYLE', 'NAME_INI', 'PREPROG']);
    for (const record of xml.split('<Record ').slice(1)) {
        const cells = [...record.split('</Record>')[0].matchAll(/<Cell Name="([^"]*)">([^<]*)<\/Cell>/g)];
        if (!cells.some(([, name, value]) => name === 'CODE' && value.trim().toUpperCase() === code.toUpperCase())) {
            continue;
        }
        const labels = [];
        for (const [, name, value] of cells) {
            if (name === 'PARAMETERS') {
                labels.push(...value.trim().split('|').map((part) => part.split('=')[0]));
            } else if (!exclude.has(name)) {
                labels.push(name);
            }
        }
        return labels;
    }
    throw new Error(`no ${code} record`);
}

async function main() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'vertex-adt-object-param-goldens-'));
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
            console.log('lsp golden: 41-object-parameter-completion passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
