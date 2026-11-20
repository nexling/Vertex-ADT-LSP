'use strict';

/**
 * Parity goldens for remaining-param completion.
 * Assertions from test-fixtures/remaining-param-completion/TESTING.md.
 *
 * Oracle: the Node host's callWithPathRegex + item mapping
 * (client/out/extension.js) over server.js extractFunctionParameters /
 * targetSectionSupport.js extractCallParametersFromText, fed the buffer-wins
 * callee text (Node itself reads disk only; the spec is buffer-wins).
 *
 * The Node helper results are replayed from
 * scripts/goldens/oracle-tapes/36-*.json (scripts/oracle-tape.js); the Node
 * server was removed on 2026-11-20 (ADR 0018).
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const ranking = require('../completion-ranking');
const { replayModule } = require('../oracle-tape');
const targetSectionSupport = replayModule('targetSectionSupport', '36');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'remaining-param-completion');
const CUSTOM = path.join(FIXTURE, 'custom');
const TEXT_KIND = 1;
const VARIABLE_KIND = 6;
const TYPED = 2;
const INVOKED = 1;
const DOC_PREFIX = 'Input Parameter ';

// Copied verbatim from client/out/extension.js.
const HOST_RE = /(?:[A-Za-z][A-Za-z0-9_]*\$?\s*=\s*)?call\s*\(\s*((?:"[^"]+"|'[^']+'|[^,)]+))(?:,\s*[^,]*)*,\s*$/i;

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

// server.js isInsideQuotes / findValidationCommentIndex / extractFunctionParameters.
function isInsideQuotes(line, position) {
    let activeQuote = null;
    for (let i = 0; i < position; i++) {
        if (activeQuote) {
            if (line[i] === activeQuote) {
                activeQuote = null;
            }
            continue;
        }
        if (line[i] === '"' || line[i] === "'") {
            activeQuote = line[i];
        }
    }
    return activeQuote !== null;
}

function findValidationCommentIndex(line) {
    if (!line) {
        return -1;
    }
    for (let i = 0; i < line.length - 1; i++) {
        if (line[i] === ';' && line[i + 1] === '!' && !isInsideQuotes(line, i)) {
            return i;
        }
    }
    return -1;
}

function extractFunctionParameters(fileContent, targetSection) {
    const extracted = targetSectionSupport.extractCallParametersFromText(fileContent, targetSection);
    const lines = String(fileContent || '').split('\n');
    return extracted.map((item) => {
        const line = lines[item.lineIndex] || '';
        const parameter = { variableName: item.variableName, parameterName: item.parameterName };
        const commentIndex = findValidationCommentIndex(line);
        if (commentIndex !== -1) {
            const comment = line.substring(commentIndex + 2).split('"')[1];
            if (comment) {
                parameter.comment = comment;
            }
        }
        return parameter;
    });
}

// Host provider, with buffer-wins callee text in place of custom/getCallParameters' disk read.
function oracleItems(linePrefix, buffers) {
    const match = linePrefix.match(HOST_RE);
    if (!match) {
        return [];
    }
    const parsedPath = targetSectionSupport.parseIncludeCallPathAndSection(
        match[1].trim().replace(/^["']|["']$/g, ''),
        ''
    );
    if (!parsedPath.filePath) {
        return [];
    }
    const diskPath = path.join(CUSTOM, parsedPath.filePath);
    let text = buffers.get(path.basename(parsedPath.filePath).toLowerCase());
    if (text === undefined) {
        text = fs.existsSync(diskPath) && fs.statSync(diskPath).isFile()
            ? fs.readFileSync(diskPath, 'utf8')
            : null;
    }
    if (!text) {
        return [];
    }
    const parameters = extractFunctionParameters(text, parsedPath.targetSection);
    const commaCount = (linePrefix.match(/,/g) || []).length;
    const paramPosition = commaCount - 1;
    if (paramPosition >= parameters.length) {
        return [];
    }
    return parameters.slice(paramPosition).map((param, index) => ({
        label: param.variableName,
        kind: param.variableName.includes('$') ? TEXT_KIND : VARIABLE_KIND,
        sortText: index.toString().padStart(2, '0'),
        documentation: `Input Parameter ${paramPosition + index + 1}: ${param.comment || 'No comment given. See file!'}`,
    }));
}

function familyItems(result, name) {
    assert(result && Array.isArray(result.items), `${name}: vertexadt completion is a CompletionList`);
    // A typed trigger character with no family items opens no popup; callers then
    // compare [] with the oracle (docs/adr/0007-native-trigger-characters-open-family-sites-only.md).
    if (result.items.length === 0) {
        assertEqual(result.isIncomplete, false, `${name}: no popup is complete`);
        return [];
    }
    // isIncomplete: checked by completion-ranking familyItems (true with family items).
    assert(
        result.items.some((item) => item.label === 'askdim'),
        `${name}: keeps the cheap keyword-variable list`
    );
    const items = ranking.familyItems(result, 'remainingParam', name).filter(
        (item) => typeof item.documentation === 'string' && item.documentation.startsWith(DOC_PREFIX)
    );
    for (const item of items) {
        // filterText: the typed text when one is typed (slot substring match).
        for (const field of ['insertText', 'insertTextFormat', 'textEdit', 'data', 'detail']) {
            assert(item[field] === undefined, `${name}: ${item.label} has no ${field}`);
        }
    }
    return items.map((item) => ({
        label: item.label,
        kind: item.kind,
        sortText: item.sortText,
        documentation: item.documentation,
    }));
}

class Probe {
    constructor(session, fileName) {
        this.session = session;
        this.uri = pathToFileURL(path.join(CUSTOM, fileName)).href;
        this.version = 0;
    }

    async complete(text, line, character, context) {
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({ uri: this.uri, languageId: 'vertexadt', version: 1, text });
        } else {
            this.session.didChange({ uri: this.uri, version: this.version, contentChanges: [{ text }] });
        }
        const params = { textDocument: { uri: this.uri }, position: { line, character } };
        if (context) {
            params.context = context;
        }
        return this.session.request('textDocument/completion', params);
    }

    // `|` marks the cursor; without one the cursor is at the end of the last line.
    async at(textWithCursor, context = { triggerKind: TYPED, triggerCharacter: ',' }) {
        const lines = textWithCursor.split('\n');
        let line = lines.findIndex((row) => row.includes('|'));
        let character;
        if (line >= 0) {
            character = lines[line].indexOf('|');
            lines[line] = lines[line].replace('|', '');
        } else {
            line = lines.length - 1;
            character = lines[line].replace(/\r$/, '').length;
        }
        const result = await this.complete(lines.join('\n'), line, character, context);
        return { result, prefix: lines[line].replace(/\r$/, '').substring(0, character) };
    }
}

// Native items equal the oracle; optional `expected` pins labels.
async function check(probe, buffers, text, name, expected, context) {
    const { result, prefix } = await probe.at(text, context);
    const items = familyItems(result, name);
    assertEqual(items, oracleItems(prefix, buffers), `${name}: matches the Node host oracle`);
    if (expected !== undefined) {
        assertEqual(items.map((item) => item.label), expected, `${name}: labels`);
    }
    return { items, result };
}

const THREE = 'callee_three_params.prg';

const SITE_ROWS = [
    // [prefix, expected labels]
    [`call("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`call("${THREE}",  `, ['obj', 'Panel', 'iswall']],
    [`call("${THREE}" ,`, ['obj', 'Panel', 'iswall']],
    [`call("${THREE}", Area,`, ['Panel', 'iswall']],
    [`call("${THREE}",,`, ['Panel', 'iswall']],
    [`call("${THREE}", Area, Sel,`, ['iswall']],
    [`call("${THREE}", Area, Sel, "1",`, []],
    [`call("${THREE}", Area`, []],
    [`call("${THREE}"`, []],
    [`call("${THREE}", Area)`, []],
    [`call("${THREE}", Area) extra,`, ['Panel', 'iswall']],
    [`call(${THREE},`, ['obj', 'Panel', 'iswall']],
    [`call('${THREE}',`, ['obj', 'Panel', 'iswall']],
    [`call("${THREE}", MACROS,`, ['Panel', 'iswall']],
    [`call("${THREE} SECTION",`, ['obj', 'Panel', 'iswall']],
    [`call("${THREE} SECTIONOFFILE",`, ['obj', 'Panel', 'iswall']],
    [`Ret$=call("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`Ret$ = call("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`Ret=call("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`CALL("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`Call("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`recall("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`my_call("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`call_virtual("${THREE}",`, []],
    [`call ${THREE},`, []],
    [`# include ${THREE},`, []],
    [`x = 1; call("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`    call ("${THREE}",`, ['obj', 'Panel', 'iswall']],
    ['call(,', []],
    ['call( ,', []],
    ['call("",', []],
    ['call("missing.prg",', []],
    ['call("empty_callee.prg",', []],
    [`!call("${THREE}",`, ['obj', 'Panel', 'iswall']],
    [`call("${THREE}", "a,b",`, ['iswall']],
    [`call("file,${THREE}",`, []],
    [`call("${THREE}", call("callee_commented.prg",`, ['Panel', 'iswall']],
    [`call("${THREE}", Area) call("callee_commented.prg",`, ['Panel', 'iswall']],
    [`call("callee_commented.prg",`, ['w', 'Name$', 'tag', 'Other', 'a', 'b', 'c', 'd']],
    ['call("callee_commented.prg", 1, 2, 3, 4, 5, 6,', ['c', 'd']],
    ['call("sectional_callee.prg MACROS",', ['m1', 'm2$']],
    ['call("sectional_callee.prg EDIT_FORM",', ['form']],
    ['call("sectional_callee.prg SECTION1",', []],
    ['call("sectional_callee.prg NOPE",', []],
    ['call("sectional_callee.prg",', ['m1', 'm2$', 'form', 'top']],
    ['call("sectional_callee.prg", MACROS,', ['m2$', 'form', 'top']],
    ['call("crlf_callee.prg",', ['crlf1', 'crlf2']],
];

async function run(session) {
    const init = await session.initialize();
    assertEqual(
        init.capabilities.completionProvider.triggerCharacters,
        [' ', ',', '(', '/', '"', ')'],
        'completion trigger characters stay ticket 09 union'
    );
    await session.waitQuiet();

    const buffers = new Map();
    const probe = new Probe(session, 'caller.prg');

    for (const [prefix, expected] of SITE_ROWS) {
        await check(probe, buffers, prefix, prefix, expected);
    }

    // Item fields on the golden callee.
    let { items } = await check(probe, buffers, `x = 1\ncall("${THREE}",`, 'three params first comma');
    assertEqual(items, [
        { label: 'obj', kind: VARIABLE_KIND, sortText: '00', documentation: 'Input Parameter 1: No comment given. See file!' },
        { label: 'Panel', kind: VARIABLE_KIND, sortText: '01', documentation: 'Input Parameter 2: No comment given. See file!' },
        { label: 'iswall', kind: VARIABLE_KIND, sortText: '02', documentation: 'Input Parameter 3: No comment given. See file!' },
    ], 'three params first comma items');
    ({ items } = await check(probe, buffers, `call("${THREE}", Area,`, 'three params second comma'));
    assertEqual(items.map((item) => [item.sortText, item.documentation]), [
        ['00', 'Input Parameter 2: No comment given. See file!'],
        ['01', 'Input Parameter 3: No comment given. See file!'],
    ], 'remaining-relative sortText, 1-based extract index');

    // Comments: quoted, spaced, [type] kept, unquoted/empty fall back; `$` is Text.
    ({ items } = await check(probe, buffers, 'call("callee_commented.prg",', 'commented callee'));
    assertEqual(items.map((item) => [item.label, item.kind, item.documentation]), [
        ['w', VARIABLE_KIND, 'Input Parameter 1: plate width'],
        ['Name$', TEXT_KIND, 'Input Parameter 2: has spaces'],
        ['tag', VARIABLE_KIND, 'Input Parameter 3: [pvector] keep tag'],
        ['Other', VARIABLE_KIND, 'Input Parameter 4: No comment given. See file!'],
        ['a', VARIABLE_KIND, 'Input Parameter 5: No comment given. See file!'],
        ['b', VARIABLE_KIND, 'Input Parameter 6: after semicolon'],
        ['c', VARIABLE_KIND, 'Input Parameter 7: No comment given. See file!'],
        ['d', VARIABLE_KIND, 'Input Parameter 8: No comment given. See file!'],
    ], 'commented callee items');
    ({ items } = await check(probe, buffers, 'call("crlf_callee.prg",', 'crlf callee'));
    assertEqual(items[0].documentation, 'Input Parameter 1: crlf comment', 'CRLF callee comment');

    // Mid-line cursor, CRLF caller, Invoked trigger.
    await check(probe, buffers, `call("${THREE}", |Area)`, 'mid-line after comma', ['obj', 'Panel', 'iswall']);
    // Native-only (Node is frozen): typing in a slot is the slot at the typed
    // word's start, substring-filtered on the typed text.
    {
        const { result } = await probe.at(`call("${THREE}", A|rea)`);
        const items = familyItems(result, 'mid-line in argument');
        assertEqual(items.map((item) => item.label), ['Panel', 'iswall'], 'mid-line in argument: labels contain "a"');
        const raw = result.items.filter((item) => item.label === 'Panel' || item.label === 'iswall');
        assert(raw.every((item) => item.filterText === 'A'), 'mid-line in argument: filterText is the typed text');
    }
    await check(probe, buffers, `x = 1\r\ncall("${THREE}",\r\ny = 2`.replace('",\r', '",|\r'), 'CRLF caller', ['obj', 'Panel', 'iswall']);
    await check(probe, buffers, `call("${THREE}",`, 'Invoked', ['obj', 'Panel', 'iswall'], { triggerKind: INVOKED });
    await check(probe, buffers, `call("${THREE}",`, 'no context', ['obj', 'Panel', 'iswall'], null);

    // No path/section picker items at a remaining-param slot.
    const picker = await probe.at(`Ret$=call("${THREE}",`);
    assert(
        !picker.result.items.some((item) => item.kind === 17 || item.kind === 19 || item.detail === 'section'),
        'path/section picker is none at a remaining-param comma'
    );

    // Buffer wins over disk for the callee, and disk returns after close.
    const calleeUri = pathToFileURL(path.join(CUSTOM, 'buffer_wins_callee.prg')).href;
    await check(probe, buffers, 'call("buffer_wins_callee.prg",', 'disk callee', ['disk1', 'disk2']);
    const bufferText = 'buf1 = get($CALLPARAM1) ;!"from buffer"\nbuf2$ = get($CALLPARAM2)\nbuf3 = get($CALLPARAM3)\n';
    session.didOpen({ uri: calleeUri, languageId: 'vertexadt', version: 1, text: bufferText });
    buffers.set('buffer_wins_callee.prg', bufferText);
    ({ items } = await check(probe, buffers, 'call("buffer_wins_callee.prg",', 'buffer callee', ['buf1', 'buf2$', 'buf3']));
    assertEqual(items[0].documentation, 'Input Parameter 1: from buffer', 'buffer callee comment');
    const edited = 'only = get($CALLPARAM1)\n';
    session.didChange({ uri: calleeUri, version: 2, contentChanges: [{ text: edited }] });
    buffers.set('buffer_wins_callee.prg', edited);
    await check(probe, buffers, 'call("buffer_wins_callee.prg",', 'edited buffer callee', ['only']);
    session.didClose(calleeUri);
    buffers.delete('buffer_wins_callee.prg');
    await check(probe, buffers, 'call("buffer_wins_callee.prg",', 'closed callee reads disk', ['disk1', 'disk2']);

    // Non-vertexadt: no family.
    const plainUri = pathToFileURL(path.join(CUSTOM, 'other.txt')).href;
    const plainLine = `call("${THREE}",`;
    session.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: plainLine });
    const plain = await session.request('textDocument/completion', {
        textDocument: { uri: plainUri },
        position: { line: 0, character: plainLine.length },
        context: { triggerKind: TYPED, triggerCharacter: ',' },
    });
    const plainItems = plain && Array.isArray(plain.items) ? plain.items : Array.isArray(plain) ? plain : [];
    assertEqual(
        plainItems.filter((item) => typeof item.documentation === 'string' && item.documentation.startsWith(DOC_PREFIX)),
        [],
        'plaintext adds no remaining-param items'
    );

    // completionItem/resolve leaves these items alone.
    ({ items } = await check(probe, buffers, `call("${THREE}",`, 'resolve probe'));
    const resolved = await session.completionResolve({ label: 'obj', kind: VARIABLE_KIND, sortText: '00', documentation: items[0].documentation });
    assertEqual(resolved.documentation, items[0].documentation, 'resolve keeps documentation');

    await session.shutdownAndExit();
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'remaining-param-completion' }],
    });
    try {
        await run(session);
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    main()
        .then(() => {
            console.log('lsp golden: 36-remaining-param-completion passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
