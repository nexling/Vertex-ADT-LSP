'use strict';

/**
 * Parity goldens for call return-value completion.
 * Assertions from test-fixtures/call-return-value-completion/TESTING.md.
 *
 * Oracle: the Node host's previous-line callRegex + item mapping
 * (client/out/extension.js) over server.js extractReturnValues, fed the
 * buffer-wins callee text (Node itself reads disk only; the spec is
 * buffer-wins). Field comments come from the callee's own variable extract;
 * the oracle takes them from COMMENTS, which were checked once against the
 * live Node server's custom/getCallReturnValues.
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const ranking = require('../completion-ranking');
const targetSectionSupport = require('../../server/out/targetSectionSupport');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'call-return-value-completion');
const CUSTOM = path.join(FIXTURE, 'custom');
const SNIPPET_KIND = 15;
const TYPED = 2;
const INVOKED = 1;
const LABEL_PREFIX = 'return:';

// Copied verbatim from client/out/extension.js.
const CALL_RE = /(\w+\$)\s*=\s*call\(\s*("[^"]+"|'[^']+'|[^,)]+)(?:,\s*[^)]*)*\)/;

// First non-string `;!"…"` comment per field name, per callee (exact case).
const COMMENTS = {
    'return_fields.prg': { W: 'plate width', H: '[number] height', DEPTH: 'second depth comment' },
    'crlf.prg': { CR: 'crlf comment' },
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

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    assert(actualJson === expectedJson, `${message}: expected ${expectedJson}, got ${actualJson}`);
}

// server.js extractReturnValues, with the hover lookup reading `comments`
// instead of the process-global hovervariables arrays.
function extractReturnValues(fileContent, comments) {
    const returnValues = [];
    const lines = fileContent.split('\n');
    let returnVar = '';
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();
        if (line.trim().startsWith('exit')) {
            const exitMatch = line.match(/exit\s+(\w+\$)/);
            if (exitMatch) {
                returnVar = exitMatch[1];
            }
        }
    }
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();
        if (line.includes('=') && line.includes('"') && line.includes('&') && line.includes(returnVar)) {
            const varMatch = line.match(/^\s*([a-zA-Z][a-zA-Z0-9_]*\$)\s*=/);
            if (varMatch) {
                returnVar = varMatch[1];
                const formatParts = line.match(/"([^"]*?)"/g) || [];
                const fields = [];
                for (const part of formatParts) {
                    const fieldMatch = part.match(/"([A-Z0-9_]+)=|"\|([A-Z0-9_]+)=/);
                    if (fieldMatch) {
                        fields.push(fieldMatch[1] || fieldMatch[2]);
                    }
                }
                fields.forEach((field) => {
                    let hovertext = comments[field] || '';
                    const extrainfo = '';
                    if (hovertext.length > 0) {
                        hovertext = `\`\`\`vertexadt
"Extract" ${field} "from return string"
!Variableinfo: ${hovertext}
\`\`\``;
                    } else {
                        hovertext = `\`\`\`vertexadt
"Extract" ${field} "from return string"
\`\`\``;
                    }
                    const lowerField = field.toLowerCase();
                    returnValues.push({
                        label: `ret${lowerField}$ = strfield(Ret$,"${field}") ${extrainfo}\nret${lowerField} = ret${lowerField}$ ${extrainfo}`,
                        kind: 'Snippet',
                        detail: hovertext,
                    });
                });
            }
        }
    }
    return returnValues;
}

// Host provider, with buffer-wins callee text in place of the RPC's disk read.
function oracleItems(lines, line, buffers) {
    if (line === 0) {
        return [];
    }
    const prevLine = (lines[line - 1] || '').replace(/\r$/, '').trim();
    const match = prevLine.match(CALL_RE);
    if (!match) {
        return [];
    }
    const returnVar = match[1];
    const filePath = targetSectionSupport.lookupPathFromCallArgument(match[2].replace(/^["']|["']$/g, ''));
    if (!filePath) {
        return [];
    }
    const baseName = path.basename(filePath).toLowerCase();
    let text = buffers.get(baseName);
    if (text === undefined) {
        const diskPath = path.join(CUSTOM, filePath);
        text = !/["]/.test(filePath) && fs.existsSync(diskPath) && fs.statSync(diskPath).isFile()
            ? fs.readFileSync(diskPath, 'utf8')
            : null;
    }
    if (!text) {
        return [];
    }
    return extractReturnValues(text, COMMENTS[baseName] || {}).map((value) => {
        let label = LABEL_PREFIX + value.label.replace('Ret$', returnVar);
        if (label.includes('=')) {
            label = label.split('=')[0];
        }
        return {
            label,
            kind: SNIPPET_KIND,
            insertText: '\n' + value.label.replace('Ret$', returnVar),
            documentation: { kind: 'markdown', value: value.detail },
        };
    });
}

function familyItems(result, name) {
    assert(result && Array.isArray(result.items), `${name}: vertexadt completion is a CompletionList`);
    // isIncomplete: checked by completion-ranking familyItems (true with family items).
    assert(
        result.items.some((item) => item.label === 'askdim'),
        `${name}: keeps the cheap keyword-variable list`
    );
    const items = ranking.familyItems(result, 'callReturnValue', name)
        .filter((item) => item.label.startsWith(LABEL_PREFIX));
    for (const item of items) {
        assertEqual(item.sortText, item.label, `${name}: ${item.label} family key is the label`);
        // filterText: the typed text when one is typed (slot substring match).
        for (const field of ['insertTextFormat', 'textEdit', 'data', 'detail', 'commitCharacters']) {
            assert(item[field] === undefined, `${name}: ${item.label} has no ${field}`);
        }
    }
    return items.map((item) => ({
        label: item.label,
        kind: item.kind,
        insertText: item.insertText,
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
    async at(textWithCursor, context = { triggerKind: INVOKED }) {
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
        return { result, lines, line };
    }
}

// Native items equal the oracle; optional `expected` pins labels.
async function check(probe, buffers, text, name, expected, context) {
    const { result, lines, line } = await probe.at(text, context);
    const items = familyItems(result, name);
    assertEqual(items, oracleItems(lines, line, buffers), `${name}: matches the Node host oracle`);
    if (expected !== undefined) {
        assertEqual(items.map((item) => item.label), expected, `${name}: labels`);
    }
    return { items, result };
}

const FIELDS = ['return:retw$ ', 'return:reth$ ', 'return:retdepth$ ', 'return:retwidth$ '];
const RF = 'return_fields.prg';

// [previous line, expected labels]; the cursor sits on an empty next line.
const SITE_ROWS = [
    [`Ret$=call("${RF}")`, FIELDS],
    [`Ret$ = call("${RF}")`, FIELDS],
    [`Result$=call('${RF}')`, FIELDS],
    [`Ret$=call( "${RF}")`, FIELDS],
    [`Ret$=call("${RF}" )`, []],
    [`Ret$=call("${RF}", a, b)`, FIELDS],
    [`Ret$=call("${RF}", a, (b)`, FIELDS],
    [`Ret$=call("${RF}",`, []],
    [`Ret$=call(${RF})`, FIELDS],
    [`Ret$=call(${RF} MACROS)`, FIELDS],
    [`Ret$=call("${RF} MACROS")`, FIELDS],
    [`Ret$=call("${RF} SECTION")`, FIELDS],
    [`\tRet$=call("${RF}")`, FIELDS],
    [`x = 1; Ret$=call("${RF}")`, FIELDS],
    [`!Ret$=call("${RF}")`, FIELDS],
    [`Ret$=call("${RF}") ! bang`, FIELDS],
    [`_ret$=call("${RF}")`, FIELDS],
    [`1x$=call("${RF}")`, FIELDS],
    [`foo(Ret$=call("${RF}"))`, FIELDS],
    [`a$b$=call("${RF}")`, FIELDS],
    [`Ret$ = call("${RF}")`, FIELDS],
    [`Ret=call("${RF}")`, []],
    [`found=call("${RF}")`, []],
    [`call("${RF}")`, []],
    [`$=call("${RF}")`, []],
    [`Ret$=CALL("${RF}")`, []],
    [`Ret$=Call("${RF}")`, []],
    [`Ret$=call ("${RF}")`, []],
    [`Ret$=call ${RF}`, []],
    [`call ${RF}`, []],
    [`Ret$=recall("${RF}")`, []],
    [`Ret$=call_virtual("${RF}")`, []],
    [`Ret$=call("${RF}"`, []],
    ['Ret$=call( )', []],
    ['Ret$=call("")', []],
    ['Ret$=call("missing.prg")', []],
    ['Ret$=call("empty.prg")', []],
    ['Ret$=call("no_fields.prg")', []],
    ['Ret$=call("packed.prg")', ['return:reta$ ']],
    ['Ret$=call("no_exit.prg")', ['return:retone$ ', 'return:rettwo$ ']],
    ['Ret$=call("last_exit.prg")', ['return:retsecond$ ']],
    ['Ret$=call("quirks.prg")', [
        'return:retplain$ ', 'return:retplain$ ', 'return:retx$ ', 'return:retz$ ',
        'return:retbang$ ', 'return:retform$ ',
    ]],
    ['Ret$=call("sectional.prg MACROS")', ['return:retmac$ ', 'return:retoth$ ']],
    ['Ret$=call("crlf.prg")', ['return:retcr$ ', 'return:retlf$ ']],
    [`Ret$=call("missing.prg") Other$=call("${RF}")`, []],
    [`x$ = call("${RF}") y$=call("packed.prg")`, FIELDS],
    ['', []],
    ['foo', []],
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

    for (const [prevLine, expected] of SITE_ROWS) {
        await check(probe, buffers, `${prevLine}\n`, JSON.stringify(prevLine), expected);
    }

    // Item fields: label, Snippet kind, insertText pair with the caller's name, docs.
    let { items } = await check(probe, buffers, `Plate$ = call("${RF}")\n`, 'item fields', FIELDS);
    assertEqual(items[0], {
        label: 'return:retw$ ',
        kind: SNIPPET_KIND,
        insertText: '\nretw$ = strfield(Plate$,"W") \nretw = retw$ ',
        documentation: {
            kind: 'markdown',
            value: '```vertexadt\n"Extract" W "from return string"\n!Variableinfo: plate width\n```',
        },
    }, 'first item');
    assertEqual(items.map((item) => item.documentation.value.split('\n')[2]), [
        '!Variableinfo: plate width',
        '!Variableinfo: [number] height',
        '!Variableinfo: second depth comment',
        '```',
    ], 'comments: exact-case non-string tuple, first non-empty, [type] kept, string vars not read');
    ({ items } = await check(probe, buffers, 'Ret$=call("crlf.prg")\n', 'crlf comment'));
    assertEqual(items[0].documentation.value.split('\n')[2], '!Variableinfo: crlf comment', 'CRLF callee comment');

    // Only the immediately previous line; the current line is ignored.
    await check(probe, buffers, `Ret$=call("${RF}")`, 'on the call line itself', []);
    await check(probe, buffers, `Ret$=call("${RF}")\n\n`, 'two lines below', []);
    // Native-only (Node is frozen): text typed on the line filters the fields
    // by substring, so an unrelated word leaves none.
    {
        const { result } = await probe.at(`Ret$=call("${RF}")\nif x then`);
        assertEqual(familyItems(result, 'current line has text'), [], 'current line has text: then matches no field');
        assertEqual(result.isIncomplete, true, 'current line has text: slot stays incomplete');
        const { result: typed } = await probe.at(`Ret$=call("${RF}")\nwid`);
        assertEqual(familyItems(typed, 'typed wid').map((item) => item.label), ['return:retwidth$ '], 'typed wid: substring');
        assert(
            typed.items.filter((item) => item.label.startsWith(LABEL_PREFIX)).every((item) => item.filterText === 'wid'),
            'typed wid: filterText is the typed text'
        );
    }
    await check(probe, buffers, `Ret$=call("${RF}")\n  ret|w`, 'mid-line cursor', FIELDS);
    await check(probe, buffers, `x = 1\r\nRet$=call("${RF}")\r\n|\r\ny = 2`, 'CRLF caller', FIELDS);
    await check(probe, buffers, `Ret$=call("${RF}")\nfoo(`, 'typed (', FIELDS, { triggerKind: TYPED, triggerCharacter: '(' });
    await check(probe, buffers, `Ret$=call("${RF}")\n`, 'no context', FIELDS, null);
    await check(probe, buffers, `Ret$=call("${RF}")\nOut$=call("packed.prg")\n`, 'nearest call wins', ['return:reta$ ']);
    await check(probe, buffers, `Ret$=call("${RF}")\nOut$=call("packed.prg")\n`.replace('\nOut', '\n|Out'), 'completion on a call line reads its previous line', FIELDS);

    // Coexists with remaining-param and path/section families on the same request.
    await check(probe, buffers, `Ret$=call("${RF}")\ncall("no_exit.prg",`, 'with remaining-param slot', FIELDS, { triggerKind: TYPED, triggerCharacter: ',' });

    // Buffer wins over disk for the callee, and disk returns after close.
    const calleeUri = pathToFileURL(path.join(CUSTOM, 'buffer_wins.prg')).href;
    await check(probe, buffers, 'Ret$=call("buffer_wins.prg")\n', 'disk callee', ['return:retdisk$ ']);
    const bufferText = 'BUF = 1 ;!"from buffer"\nRet$ = "BUF=" & BUF & "|MORE=" & m\nexit Ret$\n';
    session.didOpen({ uri: calleeUri, languageId: 'vertexadt', version: 1, text: bufferText });
    buffers.set('buffer_wins.prg', bufferText);
    COMMENTS['buffer_wins.prg'] = { BUF: 'from buffer' };
    ({ items } = await check(probe, buffers, 'Ret$=call("buffer_wins.prg")\n', 'buffer callee', ['return:retbuf$ ', 'return:retmore$ ']));
    assertEqual(items[0].documentation.value.split('\n')[2], '!Variableinfo: from buffer', 'buffer callee comment');
    const edited = 'Ret$ = "EDITED=" & e\nexit Ret$\n';
    session.didChange({ uri: calleeUri, version: 2, contentChanges: [{ text: edited }] });
    buffers.set('buffer_wins.prg', edited);
    delete COMMENTS['buffer_wins.prg'];
    await check(probe, buffers, 'Ret$=call("buffer_wins.prg")\n', 'edited buffer callee', ['return:retedited$ ']);
    session.didChange({ uri: calleeUri, version: 3, contentChanges: [{ text: '' }] });
    buffers.set('buffer_wins.prg', '');
    await check(probe, buffers, 'Ret$=call("buffer_wins.prg")\n', 'empty buffer callee', []);
    session.didClose(calleeUri);
    buffers.delete('buffer_wins.prg');
    await check(probe, buffers, 'Ret$=call("buffer_wins.prg")\n', 'closed callee reads disk', ['return:retdisk$ ']);

    // Non-vertexadt: no family.
    const plainUri = pathToFileURL(path.join(CUSTOM, 'other.txt')).href;
    const plainText = `Ret$=call("${RF}")\n`;
    session.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: plainText });
    const plain = await session.request('textDocument/completion', {
        textDocument: { uri: plainUri },
        position: { line: 1, character: 0 },
        context: { triggerKind: INVOKED },
    });
    const plainItems = plain && Array.isArray(plain.items) ? plain.items : Array.isArray(plain) ? plain : [];
    assertEqual(
        plainItems.filter((item) => item.label.startsWith(LABEL_PREFIX)),
        [],
        'plaintext adds no return-value items'
    );

    // completionItem/resolve leaves these items alone.
    ({ items } = await check(probe, buffers, `Ret$=call("${RF}")\n`, 'resolve probe'));
    const resolved = await session.completionResolve({ ...items[0] });
    assertEqual(
        [resolved.label, resolved.insertText, resolved.documentation],
        [items[0].label, items[0].insertText, items[0].documentation],
        'resolve keeps the item'
    );

    await session.shutdownAndExit();
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'call-return-value-completion' }],
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
            console.log('lsp golden: 38-call-return-value-completion passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
